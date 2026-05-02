#include "floorplanner/ThreeDAnnealer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace fp {
namespace {

constexpr double kHugeObjective = 1e90;

double objective(const FloorplanSolution& solution) {
    if (!std::isfinite(solution.objectiveValue)) return 1e100;
    return solution.objectiveValue;
}

bool betterBest(const FloorplanSolution& candidate, const FloorplanSolution& best) {
    if (candidate.feasible != best.feasible) return candidate.feasible;
    return objective(candidate) < objective(best);
}

bool acceptCandidate(const FloorplanSolution& current,
                     const FloorplanSolution& candidate,
                     double temperature,
                     std::mt19937& rng) {
    if (current.feasible && !candidate.feasible) return false;
    if (!current.feasible && candidate.feasible) return true;
    const double candObj = objective(candidate);
    const double currentObj = objective(current);
    if (candObj < currentObj) return true;
    if (candObj >= kHugeObjective || currentObj >= kHugeObjective) return false;
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    return unit(rng) < std::exp(-(candObj - currentObj) / std::max(1e-12, temperature));
}

FloorplanSolution evaluate(const FloorplanProblem& problem,
                           const SequenceTriple& st,
                           const ThreeDOptions& threeDOptions,
                           ThreeDEvaluationMode evaluationMode,
                           LPSolver* solver,
                           const LPOptions& lpOptions) {
    if (evaluationMode == ThreeDEvaluationMode::LP) {
        if (!solver) {
            FloorplanSolution s;
            s.status = "3D LP evaluation requires an LP solver";
            return s;
        }
        return optimizeSequenceTripleByLP(problem, st, *solver, lpOptions, threeDOptions);
    }
    return constructBySequenceTriple3D(problem, st, threeDOptions);
}

double calibrateTemperature(const FloorplanProblem& problem,
                            const SequenceTriple& initial,
                            const AnnealerOptions& options,
                            const ThreeDOptions& threeDOptions,
                            ThreeDEvaluationMode evaluationMode,
                            LPSolver* solver,
                            const LPOptions& lpOptions) {
    std::mt19937 rng(options.seed ^ 0x85ebca6bU);
    SequenceTriple current = initial;
    FloorplanSolution currentSol = evaluate(problem, current, threeDOptions, evaluationMode, solver, lpOptions);
    double currentObj = objective(currentSol);
    double sumDelta = 0.0;
    int useful = 0;
    const int samples = std::max(1, options.temperatureCalibrationSamples);
    for (int i = 0; i < samples; ++i) {
        SequenceTriple candidate = current;
        candidate.mutate(rng);
        FloorplanSolution candSol = evaluate(problem, candidate, threeDOptions, evaluationMode, solver, lpOptions);
        const double candObj = objective(candSol);
        if (candObj < kHugeObjective && currentObj < kHugeObjective) {
            sumDelta += std::abs(candObj - currentObj);
            ++useful;
        }
        current = candidate;
        currentObj = candObj;
    }
    if (useful == 0) return options.initialTemperature > 0.0 ? options.initialTemperature : 100.0;
    const double avgDelta = sumDelta / useful;
    if (avgDelta < 1.0) return 100.0;
    const double target = std::clamp(options.targetAcceptanceProbability, 0.01, 0.99);
    return -avgDelta / std::log(target);
}

} // namespace

ThreeDAnnealerResult runThreeDAnnealing(const FloorplanProblem& problem,
                                        const AnnealerOptions& options,
                                        const ThreeDOptions& threeDOptions,
                                        ThreeDEvaluationMode evaluationMode,
                                        LPSolver* solver,
                                        const LPOptions& lpOptions) {
    const int n = static_cast<int>(problem.blocks.size());
    std::mt19937 rng(options.seed);
    SequenceTriple current = SequenceTriple::random(n, rng);
    FloorplanSolution currentSol = evaluate(problem, current, threeDOptions, evaluationMode, solver, lpOptions);
    SequenceTriple best = current;
    FloorplanSolution bestSol = currentSol;

    double T = options.autoInitialTemperature || options.initialTemperature <= 0.0
                   ? calibrateTemperature(problem, current, options, threeDOptions, evaluationMode, solver, lpOptions)
                   : options.initialTemperature;
    const double startTemperature = T;
    const int epochLength = options.epochLength > 0 ? options.epochLength : std::max(200, n * n);
    if (options.verbose) {
        std::cout << "  [3D-SA] start T=" << T
                  << "  epoch_length=" << epochLength
                  << "  initial_feasible=" << (currentSol.feasible ? "yes" : "no")
                  << "  initial_obj=" << objective(currentSol) << "\n";
    }

    int noImproveEpochs = 0;
    int sinceEpoch = 0;
    int epoch = 0;
    long long totalMoves = 0;
    long long acceptedMoves = 0;
    for (int iter = 0; iter < options.iterations && noImproveEpochs < options.maxEpochsWithoutImprovement; ++iter) {
        SequenceTriple candidate = current;
        candidate.mutate(rng);
        FloorplanSolution candSol = evaluate(problem, candidate, threeDOptions, evaluationMode, solver, lpOptions);
        ++totalMoves;
        if (acceptCandidate(currentSol, candSol, T, rng)) {
            current = candidate;
            currentSol = candSol;
            ++acceptedMoves;
        }
        if (betterBest(candSol, bestSol)) {
            best = candidate;
            bestSol = candSol;
            noImproveEpochs = 0;
        }
        if (++sinceEpoch >= epochLength) {
            sinceEpoch = 0;
            T *= options.coolingRatio;
            ++epoch;
            ++noImproveEpochs;
            if (options.verbose && options.progressIntervalEpochs > 0 && epoch % options.progressIntervalEpochs == 0) {
                const double rate = totalMoves > 0 ? 100.0 * static_cast<double>(acceptedMoves) / totalMoves : 0.0;
                std::cout << "  [3D-SA] epoch=" << epoch
                          << "  iter=" << iter + 1
                          << "  T=" << T
                          << "  best=" << objective(bestSol)
                          << "  best_feasible=" << (bestSol.feasible ? "yes" : "no")
                          << "  accept=" << rate << "%\n";
            }
        }
    }

    ThreeDAnnealerResult result;
    result.solution = bestSol;
    result.sequenceTriple = best;
    result.initialTemperatureUsed = startTemperature;
    result.epochLengthUsed = epochLength;
    result.totalMoves = totalMoves;
    result.acceptedMoves = acceptedMoves;
    return result;
}

} // namespace fp
