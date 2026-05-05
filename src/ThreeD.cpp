#include "floorplanner/ThreeD.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace fp {
namespace {

constexpr double INF = 1e30;

struct CandidateDim {
    double width = 0.0;
    double height = 0.0;
    Orientation orientation = Orientation::HORIZONTAL;
};

void addEdge(std::vector<std::vector<std::pair<int, double>>>& graph, int from, int to, double weight) {
    graph[from].push_back({to, weight});
}

double blockEffect(const Block& b) {
    const double area = b.type == BlockType::HARD ? b.fixedWidth * b.fixedHeight : b.area;
    double extreme = 1.0;
    if (b.type == BlockType::HARD) {
        const double r = b.fixedHeight / std::max(1e-12, b.fixedWidth);
        extreme = std::max(r, 1.0 / std::max(1e-12, r));
    } else {
        extreme = std::max(b.maxAspectRatio, 1.0 / std::max(1e-12, b.minAspectRatio));
    }
    return area * (1.0 + 0.1 * std::log1p(extreme));
}

std::vector<CandidateDim> dimensionCandidates(const Block& b, int softAspectCandidates) {
    std::vector<CandidateDim> out;
    if (b.type == BlockType::HARD) {
        out.push_back({b.fixedWidth, b.fixedHeight, Orientation::HORIZONTAL});
        if (std::abs(b.fixedWidth - b.fixedHeight) > 1e-12) {
            out.push_back({b.fixedHeight, b.fixedWidth, Orientation::VERTICAL});
        }
        return out;
    }

    const int k = std::max(1, softAspectCandidates);
    if (k == 1 || std::abs(b.maxAspectRatio - b.minAspectRatio) <= 1e-12) {
        const double r = std::sqrt(b.minAspectRatio * b.maxAspectRatio);
        out.push_back({std::sqrt(b.area / std::max(1e-12, r)), std::sqrt(b.area * r), Orientation::HORIZONTAL});
        return out;
    }

    // Same structural role as Kim and Kim's five soft-module candidates, with
    // log spacing so reciprocal aspect-ratio ranges are sampled symmetrically.
    for (int i = 0; i < k; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(k - 1);
        const double logR = std::log(b.minAspectRatio) * (1.0 - t) + std::log(b.maxAspectRatio) * t;
        const double r = std::exp(logR);
        out.push_back({std::sqrt(b.area / std::max(1e-12, r)), std::sqrt(b.area * r), Orientation::HORIZONTAL});
    }
    return out;
}

double softAreaLowerBound(const Block& b, double alpha) {
    return 2.0 * std::sqrt(std::max(0.0, b.area) * std::max(1e-12, alpha));
}

double clampAlpha(double alpha) {
    return std::clamp(alpha, 1e-6, 1e6);
}

FloorplanProblem prepareProblemForSequenceTripleLP(const FloorplanProblem& problem,
                                                   const SequenceTriple& st,
                                                   const ThreeDOptions& options,
                                                   const LPOptions& lpOptions) {
    FloorplanProblem lpProblem = problem;
    lpProblem.numLayers = std::max(1, options.numLayers > 0 ? options.numLayers : problem.numLayers);
    const auto layerOf = st.decodeLayers(lpProblem.numLayers);
    for (int i = 0; i < static_cast<int>(lpProblem.blocks.size()); ++i) {
        auto& b = lpProblem.blocks[i];
        b.layer = layerOf[i];
        const bool rotate = i < static_cast<int>(st.rotated.size()) && st.rotated[i];
        if (b.type == BlockType::HARD) {
            b.width = rotate ? b.fixedHeight : b.fixedWidth;
            b.height = rotate ? b.fixedWidth : b.fixedHeight;
            b.orientation = rotate ? Orientation::VERTICAL : Orientation::HORIZONTAL;
        }
    }
    if (lpOptions.fixHardOrientationsUsingConstruction) {
        const FloorplanSolution constructed = constructBySequenceTriple3D(problem, st, options);
        for (auto& block : lpProblem.blocks) {
            if (block.type != BlockType::HARD) continue;
            for (const auto& placed : constructed.placements) {
                if (placed.name != block.name) continue;
                block.width = placed.width;
                block.height = placed.height;
                block.orientation = std::abs(block.width - block.fixedWidth) <= 1e-9 &&
                                    std::abs(block.height - block.fixedHeight) <= 1e-9
                                        ? Orientation::HORIZONTAL
                                        : Orientation::VERTICAL;
                break;
            }
        }
    }
    return lpProblem;
}

FloorplanProblem assignSequenceTripleLayersPreservingDimensions(const FloorplanProblem& problem,
                                                                const SequenceTriple& st,
                                                                const ThreeDOptions& options) {
    FloorplanProblem layered = problem;
    layered.numLayers = std::max(1, options.numLayers > 0 ? options.numLayers : problem.numLayers);
    const auto layerOf = st.decodeLayers(layered.numLayers);
    for (int i = 0; i < static_cast<int>(layered.blocks.size()); ++i) {
        layered.blocks[i].layer = layerOf[i];
    }
    return layered;
}

void addLe(LPModel& m, const std::string& name, std::initializer_list<std::pair<int, double>> terms, double rhs) {
    std::vector<int> ids;
    std::vector<double> vals;
    for (auto [id, v] : terms) {
        ids.push_back(id);
        vals.push_back(v);
    }
    m.addConstraint(name, std::move(ids), std::move(vals), ConstraintSense::LessEqual, rhs);
}

void addGe(LPModel& m, const std::string& name, std::initializer_list<std::pair<int, double>> terms, double rhs) {
    std::vector<int> ids;
    std::vector<double> vals;
    for (auto [id, v] : terms) {
        ids.push_back(id);
        vals.push_back(v);
    }
    m.addConstraint(name, std::move(ids), std::move(vals), ConstraintSense::GreaterEqual, rhs);
}

void addEq(LPModel& m, const std::string& name, std::initializer_list<std::pair<int, double>> terms, double rhs) {
    std::vector<int> ids;
    std::vector<double> vals;
    for (auto [id, v] : terms) {
        ids.push_back(id);
        vals.push_back(v);
    }
    m.addConstraint(name, std::move(ids), std::move(vals), ConstraintSense::Equal, rhs);
}

std::vector<double> longestPathsDag(const std::vector<std::vector<std::pair<int, double>>>& graph) {
    const int n = static_cast<int>(graph.size());
    std::vector<int> indeg(n, 0);
    for (int u = 0; u < n; ++u) {
        for (const auto& [v, w] : graph[u]) ++indeg[v];
    }
    std::queue<int> q;
    for (int i = 0; i < n; ++i) {
        if (indeg[i] == 0) q.push(i);
    }
    std::vector<double> dist(n, 0.0);
    int seen = 0;
    while (!q.empty()) {
        const int u = q.front();
        q.pop();
        ++seen;
        for (const auto& [v, w] : graph[u]) {
            dist[v] = std::max(dist[v], dist[u] + w);
            if (--indeg[v] == 0) q.push(v);
        }
    }
    if (seen != n) throw std::runtime_error("3D sequence-triple graph has a cycle");
    return dist;
}

double overlapArea(const Block& a, const Block& b) {
    const double ox = std::max(0.0, std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x));
    const double oy = std::max(0.0, std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y));
    return ox * oy;
}

bool pointHitsBlockKeepout(double x, double y, const Block& b, double radius) {
    return x >= b.x - radius && x <= b.x + b.width + radius &&
           y >= b.y - radius && y <= b.y + b.height + radius;
}

double blockCenterX(const Block& b) {
    return b.x + 0.5 * b.width;
}

double blockCenterY(const Block& b) {
    return b.y + 0.5 * b.height;
}

FloorplanSolution compactSequenceTriple3D(const FloorplanProblem& problem,
                                          const SequenceTriple& st,
                                          const std::vector<Block>& blocksWithDims,
                                          const ThreeDOptions& options) {
    const int n = static_cast<int>(blocksWithDims.size());
    const int layers = std::max(1, options.numLayers > 0 ? options.numLayers : problem.numLayers);
    if (!st.validate(n)) throw std::runtime_error("invalid sequence-triple");

    std::vector<Block> placed = blocksWithDims;
    const auto layerOf = st.decodeLayers(layers);
    for (int i = 0; i < n; ++i) placed[i].layer = layerOf[i];

    std::vector<std::vector<std::pair<int, double>>> horizontal(n), vertical(n);
    for (const auto& rel : st.orderedRelations()) {
        const int a = rel.i;
        const int b = rel.j;
        if (placed[a].layer != placed[b].layer) continue;
        if (rel.relation == TripleRelation::LEFT_OF) {
            addEdge(horizontal, a, b, placed[a].width);
        } else {
            // LOWER_LAYER pairs clamped to the same layer fall back to BELOW,
            // exactly as in the reference 3D-Floorplanner decode.
            addEdge(vertical, a, b, placed[a].height);
        }
    }

    const auto xs = longestPathsDag(horizontal);
    const auto ys = longestPathsDag(vertical);
    double W = 0.0, H = 0.0;
    for (int i = 0; i < n; ++i) {
        placed[i].x = xs[i];
        placed[i].y = ys[i];
        W = std::max(W, placed[i].x + placed[i].width);
        H = std::max(H, placed[i].y + placed[i].height);
    }

    const double outlineWViolation = problem.hasFixedOutline ? std::max(0.0, W - problem.fixedOutlineWidth) : 0.0;
    const double outlineHViolation = problem.hasFixedOutline ? std::max(0.0, H - problem.fixedOutlineHeight) : 0.0;
    const bool outlineOk = outlineWViolation <= 1e-9 && outlineHViolation <= 1e-9;
    auto solution = makeThreeDSolution(problem, placed, W, H, outlineOk, outlineOk ? "3d_compact_feasible" : "3d_compact_outline_violation");
    if (!solution.feasible) {
        solution.objectiveValue += 1e6 * (outlineWViolation + outlineHViolation);
    }
    return solution;
}

} // namespace

ThreeDMetrics computeThreeDMetrics(const FloorplanProblem& problem, const std::vector<Block>& placedBlocks) {
    ThreeDMetrics metrics;
    for (const auto& net : problem.nets) {
        if (net.blockIds.empty() && net.pads.empty()) continue;
        double minX = 1e100, maxX = -1e100, minY = 1e100, maxY = -1e100;
        double avgX = 0.0, avgY = 0.0;
        int movablePins = 0;
        int minLayer = 1000000, maxLayer = -1000000;
        std::unordered_set<int> layers;
        for (int id : net.blockIds) {
            const auto& b = placedBlocks[id];
            const double cx = blockCenterX(b);
            const double cy = blockCenterY(b);
            minX = std::min(minX, cx);
            maxX = std::max(maxX, cx);
            minY = std::min(minY, cy);
            maxY = std::max(maxY, cy);
            avgX += cx;
            avgY += cy;
            ++movablePins;
            layers.insert(b.layer);
            minLayer = std::min(minLayer, b.layer);
            maxLayer = std::max(maxLayer, b.layer);
        }
        for (const auto& p : net.pads) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        metrics.hpwl += (maxX - minX) + (maxY - minY);
        if (layers.size() > 1) {
            const double tsvs = static_cast<double>(layers.size() - 1);
            metrics.tsvCount += tsvs;
            const double tx = movablePins > 0 ? avgX / movablePins : 0.5 * (minX + maxX);
            const double ty = movablePins > 0 ? avgY / movablePins : 0.5 * (minY + maxY);
            const double radius = std::max(0.5 * problem.tsvDiameter, problem.tsvKeepoutRadius);
            for (const auto& b : placedBlocks) {
                if (b.layer < minLayer || b.layer > maxLayer) continue;
                if (pointHitsBlockKeepout(tx, ty, b, radius)) metrics.tsvOverlapCost += 1.0;
            }
        }
    }

    // Simple hotspot proxy, kept intentionally close to the companion 3D
    // floorplanner's evaluator style: vertically aligned high-power modules
    // are penalized because they create stacked heat density.
    for (size_t i = 0; i < placedBlocks.size(); ++i) {
        for (size_t j = i + 1; j < placedBlocks.size(); ++j) {
            const auto& a = placedBlocks[i];
            const auto& b = placedBlocks[j];
            if (a.layer == b.layer) continue;
            const double ov = overlapArea(a, b);
            if (ov <= 0.0) continue;
            const double norm = std::max(1.0, std::sqrt(std::max(1.0, a.area) * std::max(1.0, b.area)));
            metrics.thermalCost += (ov / norm) * std::sqrt(std::max(1.0, a.power) * std::max(1.0, b.power));
        }
    }
    return metrics;
}

FloorplanSolution makeThreeDSolution(const FloorplanProblem& problem,
                                      const std::vector<Block>& placedBlocks,
                                      double chipWidth,
                                      double chipHeight,
                                      bool feasible,
                                      const std::string& status) {
    FloorplanSolution sol;
    const auto metrics = computeThreeDMetrics(problem, placedBlocks);
    sol.chipWidth = chipWidth;
    sol.chipHeight = chipHeight;
    sol.chipArea = chipWidth * chipHeight;
    sol.totalWirelength = metrics.hpwl;
    sol.totalTsvCount = metrics.tsvCount;
    sol.thermalCost = metrics.thermalCost;
    sol.tsvOverlapCost = metrics.tsvOverlapCost;
    const double outlineTerm = problem.objectiveMode == ObjectiveMode::FixedOutline && problem.hasFixedOutline
                                   ? 0.0
                                   : problem.areaWeight * (chipWidth + chipHeight);
    const double tsvBenefit = problem.thermalTsvBenefitWeight * std::min(metrics.tsvCount, metrics.thermalCost);
    sol.objectiveValue = outlineTerm +
                         problem.wireWeight * metrics.hpwl +
                         problem.tsvWeight * metrics.tsvCount +
                         problem.thermalWeight * metrics.thermalCost +
                         problem.tsvKeepoutWeight * metrics.tsvOverlapCost -
                         tsvBenefit;
    sol.feasible = feasible;
    sol.status = status;
    sol.placements.reserve(placedBlocks.size());
    for (const auto& b : placedBlocks) {
        sol.placements.push_back({b.name, b.type, b.x, b.y, b.layer, b.width, b.height});
    }
    return sol;
}

FloorplanSolution constructBySequenceTriple3D(const FloorplanProblem& problem,
                                              const SequenceTriple& st,
                                              const ThreeDOptions& options) {
    const int n = static_cast<int>(problem.blocks.size());
    if (!st.validate(n)) throw std::runtime_error("invalid sequence-triple");

    std::vector<Block> assigned = problem.blocks;
    for (int i = 0; i < n; ++i) {
        auto& b = assigned[i];
        const bool rotate = i < static_cast<int>(st.rotated.size()) && st.rotated[i];
        if (b.type == BlockType::HARD) {
            b.width = rotate ? b.fixedHeight : b.fixedWidth;
            b.height = rotate ? b.fixedWidth : b.fixedHeight;
            b.orientation = rotate ? Orientation::VERTICAL : Orientation::HORIZONTAL;
        } else {
            const double r = std::sqrt(b.minAspectRatio * b.maxAspectRatio);
            b.width = std::sqrt(b.area / std::max(1e-12, r));
            b.height = std::sqrt(b.area * r);
        }
    }

    // 3D extension of Kim and Kim's construction method: for a fixed sequence
    // triple, choose block dimensions in decreasing block-effect order. Each
    // candidate is compacted with the decoded layer assignment and scored with
    // the 3D objective, including HPWL, TSV, keepout, and thermal penalties.
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return blockEffect(problem.blocks[a]) > blockEffect(problem.blocks[b]);
    });

    FloorplanSolution bestSoFar;
    for (int id : order) {
        FloorplanSolution bestCandidate;
        std::vector<Block> bestBlocks = assigned;
        for (const auto& cand : dimensionCandidates(problem.blocks[id], options.softAspectCandidates)) {
            std::vector<Block> trial = assigned;
            trial[id].width = cand.width;
            trial[id].height = cand.height;
            trial[id].orientation = cand.orientation;
            auto placed = compactSequenceTriple3D(problem, st, trial, options);
            if (placed.objectiveValue < bestCandidate.objectiveValue) {
                bestCandidate = placed;
                bestBlocks = problem.blocks;
                for (size_t k = 0; k < placed.placements.size(); ++k) {
                    bestBlocks[k].x = placed.placements[k].x;
                    bestBlocks[k].y = placed.placements[k].y;
                    bestBlocks[k].layer = placed.placements[k].layer;
                    bestBlocks[k].width = placed.placements[k].width;
                    bestBlocks[k].height = placed.placements[k].height;
                    bestBlocks[k].orientation = trial[k].orientation;
                }
            }
        }
        assigned = bestBlocks;
        bestSoFar = bestCandidate;
    }
    if (order.empty()) return makeThreeDSolution(problem, assigned, 0.0, 0.0, true, "empty");
    bestSoFar.status = bestSoFar.feasible ? "3d_kim_construction_feasible" : "3d_kim_construction_outline_violation";
    return bestSoFar;
}

LPBuildResult buildThreeDLPModel(const FloorplanProblem& problem,
                                 const SequenceTriple& st,
                                 const std::vector<std::vector<double>>& alphaCuts,
                                 const ThreeDOptions& options) {
    // The caller may pass a problem whose hard-block orientations have already
    // been fixed by the 3D Kim/Kim construction method. Preserve those
    // dimensions here; only refresh sequence-triple layer assignments.
    const FloorplanProblem lpProblem = assignSequenceTripleLayersPreservingDimensions(problem, st, options);
    LPBuildResult out;
    const int n = static_cast<int>(lpProblem.blocks.size());
    const int nets = static_cast<int>(lpProblem.nets.size());
    out.vars.x.resize(n);
    out.vars.y.resize(n);
    out.vars.w.resize(n, -1);
    out.vars.h.resize(n, -1);
    out.vars.netLeft.resize(nets);
    out.vars.netRight.resize(nets);
    out.vars.netBottom.resize(nets);
    out.vars.netTop.resize(nets);
    out.vars.netWidth.resize(nets);
    out.vars.netHeight.resize(nets);

    for (int i = 0; i < n; ++i) {
        out.vars.x[i] = out.model.addVariable("x_" + lpProblem.blocks[i].name, 0.0, INF, 0.0);
        out.vars.y[i] = out.model.addVariable("y_" + lpProblem.blocks[i].name, 0.0, INF, 0.0);
        if (lpProblem.blocks[i].type == BlockType::SOFT) {
            out.vars.w[i] = out.model.addVariable("w_" + lpProblem.blocks[i].name, 1e-9, INF, 0.0);
            out.vars.h[i] = out.model.addVariable("h_" + lpProblem.blocks[i].name, 1e-9, INF, 0.0);
        }
    }

    // Fixed-outline experiments keep W and H constant. The LP objective then
    // focuses on wirelength; TSV/thermal terms are evaluated after solve
    // because they are fixed or nonlinear for a fixed sequence triple.
    const bool fixedOutlineObjective = lpProblem.objectiveMode == ObjectiveMode::FixedOutline && lpProblem.hasFixedOutline;
    const double chipObjective = fixedOutlineObjective ? 0.0 : lpProblem.areaWeight;
    out.vars.W = out.model.addVariable("W",
                                       lpProblem.hasFixedOutline ? lpProblem.fixedOutlineWidth : 0.0,
                                       lpProblem.hasFixedOutline ? lpProblem.fixedOutlineWidth : INF,
                                       chipObjective);
    out.vars.H = out.model.addVariable("H",
                                       lpProblem.hasFixedOutline ? lpProblem.fixedOutlineHeight : 0.0,
                                       lpProblem.hasFixedOutline ? lpProblem.fixedOutlineHeight : INF,
                                       chipObjective);

    for (int ni = 0; ni < nets; ++ni) {
        const auto suffix = "_" + lpProblem.nets[ni].name;
        out.vars.netLeft[ni] = out.model.addVariable("netLeft" + suffix, -INF, INF, 0.0);
        out.vars.netRight[ni] = out.model.addVariable("netRight" + suffix, -INF, INF, 0.0);
        out.vars.netBottom[ni] = out.model.addVariable("netBottom" + suffix, -INF, INF, 0.0);
        out.vars.netTop[ni] = out.model.addVariable("netTop" + suffix, -INF, INF, 0.0);
        out.vars.netWidth[ni] = out.model.addVariable("netWidth" + suffix, 0.0, INF, lpProblem.wireWeight);
        out.vars.netHeight[ni] = out.model.addVariable("netHeight" + suffix, 0.0, INF, lpProblem.wireWeight);
    }

    auto widthTerm = [&](int block, double coef) -> std::pair<int, double> {
        const auto& b = lpProblem.blocks[block];
        if (b.type == BlockType::SOFT) return {out.vars.w[block], coef};
        return {-1, coef * b.width};
    };
    auto heightTerm = [&](int block, double coef) -> std::pair<int, double> {
        const auto& b = lpProblem.blocks[block];
        if (b.type == BlockType::SOFT) return {out.vars.h[block], coef};
        return {-1, coef * b.height};
    };
    auto addMixedLe = [&](const std::string& name, std::vector<std::pair<int, double>> terms, double rhs) {
        std::vector<int> ids;
        std::vector<double> vals;
        for (auto [id, v] : terms) {
            if (id >= 0) {
                ids.push_back(id);
                vals.push_back(v);
            } else {
                rhs -= v;
            }
        }
        out.model.addConstraint(name, std::move(ids), std::move(vals), ConstraintSense::LessEqual, rhs);
    };
    auto addMixedGe = [&](const std::string& name, std::vector<std::pair<int, double>> terms, double rhs) {
        std::vector<int> ids;
        std::vector<double> vals;
        for (auto [id, v] : terms) {
            if (id >= 0) {
                ids.push_back(id);
                vals.push_back(v);
            } else {
                rhs -= v;
            }
        }
        out.model.addConstraint(name, std::move(ids), std::move(vals), ConstraintSense::GreaterEqual, rhs);
    };

    for (int i = 0; i < n; ++i) {
        const auto& b = lpProblem.blocks[i];
        if (b.type != BlockType::SOFT) continue;
        addGe(out.model, "aspect_min_" + b.name, {{out.vars.h[i], 1.0}, {out.vars.w[i], -b.minAspectRatio}}, 0.0);
        addLe(out.model, "aspect_max_" + b.name, {{out.vars.h[i], 1.0}, {out.vars.w[i], -b.maxAspectRatio}}, 0.0);
        const auto& cuts = alphaCuts.size() > static_cast<size_t>(i) ? alphaCuts[i] : std::vector<double>{};
        if (cuts.empty()) {
            addGe(out.model, "area_surrogate_" + b.name,
                  {{out.vars.w[i], 1.0}, {out.vars.h[i], 1.0}},
                  softAreaLowerBound(b, 1.0));
        } else {
            for (size_t k = 0; k < cuts.size(); ++k) {
                const double a = clampAlpha(cuts[k]);
                addGe(out.model, "area_surrogate_" + b.name + "_" + std::to_string(k),
                      {{out.vars.w[i], 1.0}, {out.vars.h[i], a}},
                      softAreaLowerBound(b, a));
            }
        }
    }

    // Sequence triple extension of the paper's sequence-pair constraints:
    // z-relations assign layers first; only blocks on the same layer generate
    // x/y non-overlap constraints. Cross-layer blocks may overlap in footprint.
    for (const auto& rel : st.orderedRelations()) {
        const int i = rel.i;
        const int j = rel.j;
        if (lpProblem.blocks[i].layer != lpProblem.blocks[j].layer) continue;
        if (rel.relation == TripleRelation::LEFT_OF) {
            addMixedLe("left3d_" + std::to_string(i) + "_" + std::to_string(j),
                       {{out.vars.x[i], 1.0}, widthTerm(i, 1.0), {out.vars.x[j], -1.0}},
                       0.0);
        } else {
            addMixedLe("below3d_" + std::to_string(i) + "_" + std::to_string(j),
                       {{out.vars.y[i], 1.0}, heightTerm(i, 1.0), {out.vars.y[j], -1.0}},
                       0.0);
        }
    }

    for (int i = 0; i < n; ++i) {
        addMixedLe("bound_x_" + lpProblem.blocks[i].name,
                   {{out.vars.x[i], 1.0}, widthTerm(i, 1.0), {out.vars.W, -1.0}},
                   0.0);
        addMixedLe("bound_y_" + lpProblem.blocks[i].name,
                   {{out.vars.y[i], 1.0}, heightTerm(i, 1.0), {out.vars.H, -1.0}},
                   0.0);
    }
    if (lpProblem.chipAspectLower > 0.0) {
        addGe(out.model, "chip_aspect_lower", {{out.vars.H, 1.0}, {out.vars.W, -lpProblem.chipAspectLower}}, 0.0);
    }
    if (lpProblem.chipAspectUpper < INF / 2.0) {
        addLe(out.model, "chip_aspect_upper", {{out.vars.H, 1.0}, {out.vars.W, -lpProblem.chipAspectUpper}}, 0.0);
    }

    for (int ni = 0; ni < nets; ++ni) {
        const auto& net = lpProblem.nets[ni];
        for (int id : net.blockIds) {
            addMixedLe("net_left_" + std::to_string(ni) + "_" + std::to_string(id),
                       {{out.vars.netLeft[ni], 1.0}, {out.vars.x[id], -1.0}, widthTerm(id, -0.5)},
                       0.0);
            addMixedGe("net_right_" + std::to_string(ni) + "_" + std::to_string(id),
                       {{out.vars.netRight[ni], 1.0}, {out.vars.x[id], -1.0}, widthTerm(id, -0.5)},
                       0.0);
            addMixedLe("net_bottom_" + std::to_string(ni) + "_" + std::to_string(id),
                       {{out.vars.netBottom[ni], 1.0}, {out.vars.y[id], -1.0}, heightTerm(id, -0.5)},
                       0.0);
            addMixedGe("net_top_" + std::to_string(ni) + "_" + std::to_string(id),
                       {{out.vars.netTop[ni], 1.0}, {out.vars.y[id], -1.0}, heightTerm(id, -0.5)},
                       0.0);
        }
        for (const auto& pad : net.pads) {
            addLe(out.model, "pad_left_" + std::to_string(ni), {{out.vars.netLeft[ni], 1.0}}, pad.x);
            addGe(out.model, "pad_right_" + std::to_string(ni), {{out.vars.netRight[ni], 1.0}}, pad.x);
            addLe(out.model, "pad_bottom_" + std::to_string(ni), {{out.vars.netBottom[ni], 1.0}}, pad.y);
            addGe(out.model, "pad_top_" + std::to_string(ni), {{out.vars.netTop[ni], 1.0}}, pad.y);
        }
        addEq(out.model, "net_width_def_" + std::to_string(ni),
              {{out.vars.netWidth[ni], 1.0}, {out.vars.netRight[ni], -1.0}, {out.vars.netLeft[ni], 1.0}},
              0.0);
        addEq(out.model, "net_height_def_" + std::to_string(ni),
              {{out.vars.netHeight[ni], 1.0}, {out.vars.netTop[ni], -1.0}, {out.vars.netBottom[ni], 1.0}},
              0.0);
    }
    return out;
}

FloorplanSolution optimizeSequenceTripleByLP(const FloorplanProblem& problem,
                                             const SequenceTriple& st,
                                             LPSolver& solver,
                                             const LPOptions& lpOptions,
                                             const ThreeDOptions& threeDOptions) {
    if (!solver.available()) {
        FloorplanSolution s;
        s.status = "LP mode requires an available LP solver";
        return s;
    }
    const FloorplanProblem lpProblem = prepareProblemForSequenceTripleLP(problem, st, threeDOptions, lpOptions);
    const int n = static_cast<int>(lpProblem.blocks.size());
    std::vector<std::vector<double>> alphaCuts(n);
    for (int i = 0; i < n; ++i) {
        if (lpProblem.blocks[i].type == BlockType::SOFT) {
            const double r = std::sqrt(lpProblem.blocks[i].minAspectRatio * lpProblem.blocks[i].maxAspectRatio);
            alphaCuts[i].push_back(clampAlpha(1.0 / std::max(1e-12, r)));
        }
    }

    FloorplanSolution last;
    for (int iter = 0; iter < std::max(1, lpOptions.maxAreaCorrectionIterations); ++iter) {
        const auto build = buildThreeDLPModel(lpProblem, st, alphaCuts, threeDOptions);
        const auto lp = solver.solve(build.model);
        if (!lp.feasible) {
            last.status = lp.status;
            last.objectiveValue = std::numeric_limits<double>::infinity();
            return last;
        }
        std::vector<Block> placed = lpProblem.blocks;
        bool areaOk = true;
        for (int i = 0; i < n; ++i) {
            placed[i].x = lp.values[build.vars.x[i]];
            placed[i].y = lp.values[build.vars.y[i]];
            if (placed[i].type == BlockType::SOFT) {
                placed[i].width = lp.values[build.vars.w[i]];
                placed[i].height = lp.values[build.vars.h[i]];
                const double actualArea = placed[i].width * placed[i].height;
                if (lpOptions.verboseAreaCorrection) {
                    std::cerr << "3d_area_correction iter=" << iter
                              << " block=" << placed[i].name
                              << " width=" << placed[i].width
                              << " height=" << placed[i].height
                              << " actual_area=" << actualArea
                              << " required_area=" << placed[i].area
                              << " alpha=" << alphaCuts[i].back() << "\n";
                }
                const double effectiveAreaTolerance = lpOptions.areaTolerance * std::max(1.0, placed[i].area);
                if (actualArea + effectiveAreaTolerance < placed[i].area) {
                    areaOk = false;
                    const double nextAlpha = clampAlpha(placed[i].width / std::max(1e-9, placed[i].height));
                    const auto duplicate = std::any_of(alphaCuts[i].begin(), alphaCuts[i].end(), [&](double oldAlpha) {
                        return std::abs(oldAlpha - nextAlpha) <= 1e-8 * std::max(1.0, std::abs(oldAlpha));
                    });
                    if (!duplicate) alphaCuts[i].push_back(nextAlpha);
                }
            }
        }
        const double W = lp.values[build.vars.W];
        const double H = lp.values[build.vars.H];
        last = makeThreeDSolution(lpProblem, placed, W, H, areaOk, areaOk ? "3d_lp_optimal" : "3d_lp_area_correction_needed");
        if (areaOk) return last;
    }
    last.feasible = false;
    last.status = "3d_lp_area_correction_limit";
    last.objectiveValue = std::numeric_limits<double>::infinity();
    return last;
}

LPBuildResult buildInitialThreeDLPModelForExport(const FloorplanProblem& problem,
                                                 const SequenceTriple& st,
                                                 const ThreeDOptions& threeDOptions) {
    const FloorplanProblem lpProblem = prepareProblemForSequenceTripleLP(problem, st, threeDOptions, LPOptions{});
    std::vector<std::vector<double>> alphaCuts(lpProblem.blocks.size());
    for (size_t i = 0; i < lpProblem.blocks.size(); ++i) {
        if (lpProblem.blocks[i].type == BlockType::SOFT) {
            const double r = std::sqrt(lpProblem.blocks[i].minAspectRatio * lpProblem.blocks[i].maxAspectRatio);
            alphaCuts[i].push_back(clampAlpha(1.0 / std::max(1e-12, r)));
        }
    }
    return buildThreeDLPModel(lpProblem, st, alphaCuts, threeDOptions);
}

LPBuildResult buildCorrectedThreeDLPModelForExport(const FloorplanProblem& problem,
                                                   const SequenceTriple& st,
                                                   LPSolver& solver,
                                                   const LPOptions& lpOptions,
                                                   const ThreeDOptions& threeDOptions) {
    const FloorplanProblem lpProblem = prepareProblemForSequenceTripleLP(problem, st, threeDOptions, lpOptions);
    std::vector<std::vector<double>> alphaCuts(lpProblem.blocks.size());
    for (size_t i = 0; i < lpProblem.blocks.size(); ++i) {
        if (lpProblem.blocks[i].type == BlockType::SOFT) {
            const double r = std::sqrt(lpProblem.blocks[i].minAspectRatio * lpProblem.blocks[i].maxAspectRatio);
            alphaCuts[i].push_back(clampAlpha(1.0 / std::max(1e-12, r)));
        }
    }

    LPBuildResult lastBuild;
    for (int iter = 0; iter < std::max(1, lpOptions.maxAreaCorrectionIterations); ++iter) {
        lastBuild = buildThreeDLPModel(lpProblem, st, alphaCuts, threeDOptions);
        const auto lp = solver.solve(lastBuild.model);
        if (!lp.feasible) return lastBuild;

        bool areaOk = true;
        for (size_t i = 0; i < lpProblem.blocks.size(); ++i) {
            if (lpProblem.blocks[i].type != BlockType::SOFT) continue;
            const double width = lp.values[lastBuild.vars.w[i]];
            const double height = lp.values[lastBuild.vars.h[i]];
            const double actualArea = width * height;
            const double effectiveAreaTolerance = lpOptions.areaTolerance * std::max(1.0, lpProblem.blocks[i].area);
            if (actualArea + effectiveAreaTolerance < lpProblem.blocks[i].area) {
                areaOk = false;
                const double nextAlpha = clampAlpha(width / std::max(1e-9, height));
                const auto duplicate = std::any_of(alphaCuts[i].begin(), alphaCuts[i].end(), [&](double oldAlpha) {
                    return std::abs(oldAlpha - nextAlpha) <= 1e-8 * std::max(1.0, std::abs(oldAlpha));
                });
                if (!duplicate) alphaCuts[i].push_back(nextAlpha);
            }
        }
        if (areaOk) return lastBuild;
    }
    return lastBuild;
}

} // namespace fp
