#pragma once

#include "floorplanner/DataModel.h"
#include "floorplanner/LPFloorplanner.h"
#include "floorplanner/LPSolver.h"
#include "floorplanner/SequenceTriple.h"

namespace fp {

struct ThreeDOptions {
    int numLayers = 2;
    int softAspectCandidates = 5;
};

struct ThreeDMetrics {
    double hpwl = 0.0;
    double tsvCount = 0.0;
    double thermalCost = 0.0;
    double tsvOverlapCost = 0.0;
};

ThreeDMetrics computeThreeDMetrics(const FloorplanProblem& problem, const std::vector<Block>& placedBlocks);
FloorplanSolution makeThreeDSolution(const FloorplanProblem& problem,
                                      const std::vector<Block>& placedBlocks,
                                      double chipWidth,
                                      double chipHeight,
                                      bool feasible,
                                      const std::string& status);
FloorplanSolution constructBySequenceTriple3D(const FloorplanProblem& problem,
                                              const SequenceTriple& st,
                                              const ThreeDOptions& options = {});
LPBuildResult buildThreeDLPModel(const FloorplanProblem& problem,
                                 const SequenceTriple& st,
                                 const std::vector<std::vector<double>>& alphaCuts,
                                 const ThreeDOptions& options = {});
FloorplanSolution optimizeSequenceTripleByLP(const FloorplanProblem& problem,
                                             const SequenceTriple& st,
                                             LPSolver& solver,
                                             const LPOptions& lpOptions = {},
                                             const ThreeDOptions& threeDOptions = {});
LPBuildResult buildInitialThreeDLPModelForExport(const FloorplanProblem& problem,
                                                 const SequenceTriple& st,
                                                 const ThreeDOptions& threeDOptions = {});
LPBuildResult buildCorrectedThreeDLPModelForExport(const FloorplanProblem& problem,
                                                   const SequenceTriple& st,
                                                   LPSolver& solver,
                                                   const LPOptions& lpOptions = {},
                                                   const ThreeDOptions& threeDOptions = {});

} // namespace fp
