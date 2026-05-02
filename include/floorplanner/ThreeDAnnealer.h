#pragma once

#include "floorplanner/Annealer.h"
#include "floorplanner/SequenceTriple.h"
#include "floorplanner/ThreeD.h"

namespace fp {

enum class ThreeDEvaluationMode { Construction, LP };

struct ThreeDAnnealerResult {
    FloorplanSolution solution;
    SequenceTriple sequenceTriple;
    double initialTemperatureUsed = 0.0;
    int epochLengthUsed = 0;
    long long totalMoves = 0;
    long long acceptedMoves = 0;
};

ThreeDAnnealerResult runThreeDAnnealing(const FloorplanProblem& problem,
                                        const AnnealerOptions& options,
                                        const ThreeDOptions& threeDOptions = {},
                                        ThreeDEvaluationMode evaluationMode = ThreeDEvaluationMode::Construction,
                                        LPSolver* solver = nullptr,
                                        const LPOptions& lpOptions = {});

} // namespace fp
