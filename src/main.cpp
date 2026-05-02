#include "floorplanner/Annealer.h"
#include "floorplanner/IO.h"
#include "floorplanner/LPFloorplanner.h"
#include "floorplanner/LPSolver.h"
#include "floorplanner/ThreeD.h"
#include "floorplanner/ThreeDAnnealer.h"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {

struct Args {
    std::string input;
    std::string mcncName;
    std::string mcncDir = "mcnc_hard";
    std::string blocksPath;
    std::string netsPath;
    std::string output = "out";
    std::string mode = "SA-LP";
    std::string solver = "highs";
    int iterations = 10000;
    int epochLength = 100;
    int maxNoImproveEpochs = 50;
    double initialTemperature = 100.0;
    double coolingRatio = 0.95;
    unsigned seed = 1;
    bool autoTemperature = false;
    bool autoEpochLength = false;
    bool verboseSa = false;
    int layers = 0;
    std::string objectiveMode;
    double tsvWeight = -1.0;
    double thermalWeight = -1.0;
    double tsvKeepoutWeight = -1.0;
    double thermalTsvBenefitWeight = -1.0;
    double tsvDiameter = -1.0;
    double tsvKeepoutRadius = -1.0;
    std::string exportLp;
    std::string exportMps;
};

void usage() {
    std::cerr << "Usage:\n";
    std::cerr << "  floorplanner --mcnc apte --mcnc-dir mcnc_hard --mode SA-CT-LP --solver highs --iterations 10000 --output out/apte\n";
    std::cerr << "  floorplanner --mcnc apte --mcnc-dir mcnc_hard --mode SA-CT-LP --solver mosek --iterations 10000 --output out/apte_mosek\n";
    std::cerr << "  floorplanner --mcnc ami33 --mcnc-dir mcnc_hard --mode SA-3D-CT --layers 4 --iterations 10000 --output out/ami33_3d\n";
    std::cerr << "  floorplanner --blocks mcnc_hard/apte.block --nets mcnc_hard/apte.nets --mode SA-CT-LP --solver highs --output out/apte\n";
    std::cerr << "  floorplanner --input custom.json --mode LP --solver highs --output out/custom\n";
    std::cerr << "Options:\n";
    std::cerr << "  --auto-temperature       calibrate the SA start temperature from sampled moves\n";
    std::cerr << "  --auto-epoch-length      use max(200, num_blocks^2) moves per cooling step\n";
    std::cerr << "  --verbose-sa             print annealing progress and acceptance statistics\n";
    std::cerr << "  --objective-mode MODE    free-outline-linear or fixed-outline\n";
    std::cerr << "  --layers N               number of layers for 3D sequence-triple modes\n";
    std::cerr << "3D modes:\n";
    std::cerr << "  3D-CT, 3D-LP, SA-3D-CT, SA-3D-LP, SA-3D-CT-LP\n";
}

bool is3DMode(const std::string& mode) {
    return mode == "3D-CT" || mode == "CT-3D" ||
           mode == "3D-LP" || mode == "LP-3D" ||
           mode == "SA-3D-CT" || mode == "3D-SA-CT" ||
           mode == "SA-3D-LP" || mode == "3D-SA-LP" ||
           mode == "SA-3D-CT-LP" || mode == "3D-SA-CT-LP" ||
           mode == "SA-ST" || mode == "ST-SA";
}

bool is3DAnnealingMode(const std::string& mode) {
    return mode == "SA-3D-CT" || mode == "3D-SA-CT" ||
           mode == "SA-3D-LP" || mode == "3D-SA-LP" ||
           mode == "SA-3D-CT-LP" || mode == "3D-SA-CT-LP" ||
           mode == "SA-ST" || mode == "ST-SA";
}

bool is3DLPMode(const std::string& mode) {
    return mode == "3D-LP" || mode == "LP-3D" ||
           mode == "SA-3D-LP" || mode == "3D-SA-LP" ||
           mode == "SA-3D-CT-LP" || mode == "3D-SA-CT-LP";
}

bool is3DAnnealingLPMode(const std::string& mode) {
    return mode == "SA-3D-LP" || mode == "3D-SA-LP";
}

bool is3DAnnealingCTLPMode(const std::string& mode) {
    return mode == "SA-3D-CT-LP" || mode == "3D-SA-CT-LP";
}

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        auto need = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + name);
            return argv[++i];
        };
        if (key == "--input") a.input = need(key);
        else if (key == "--mcnc") a.mcncName = need(key);
        else if (key == "--mcnc-dir") a.mcncDir = need(key);
        else if (key == "--blocks") a.blocksPath = need(key);
        else if (key == "--nets") a.netsPath = need(key);
        else if (key == "--output") a.output = need(key);
        else if (key == "--mode") a.mode = need(key);
        else if (key == "--solver") a.solver = need(key);
        else if (key == "--iterations") a.iterations = std::stoi(need(key));
        else if (key == "--epoch-length") a.epochLength = std::stoi(need(key));
        else if (key == "--max-no-improve-epochs") a.maxNoImproveEpochs = std::stoi(need(key));
        else if (key == "--initial-temperature") a.initialTemperature = std::stod(need(key));
        else if (key == "--cooling-ratio") a.coolingRatio = std::stod(need(key));
        else if (key == "--seed") a.seed = static_cast<unsigned>(std::stoul(need(key)));
        else if (key == "--auto-temperature") a.autoTemperature = true;
        else if (key == "--auto-epoch-length") a.autoEpochLength = true;
        else if (key == "--verbose-sa") a.verboseSa = true;
        else if (key == "--layers") a.layers = std::stoi(need(key));
        else if (key == "--objective-mode" || key == "--objective") a.objectiveMode = need(key);
        else if (key == "--tsv-weight") a.tsvWeight = std::stod(need(key));
        else if (key == "--thermal-weight") a.thermalWeight = std::stod(need(key));
        else if (key == "--tsv-keepout-weight") a.tsvKeepoutWeight = std::stod(need(key));
        else if (key == "--thermal-tsv-benefit-weight") a.thermalTsvBenefitWeight = std::stod(need(key));
        else if (key == "--tsv-diameter") a.tsvDiameter = std::stod(need(key));
        else if (key == "--tsv-keepout-radius") a.tsvKeepoutRadius = std::stod(need(key));
        else if (key == "--export-lp") a.exportLp = need(key);
        else if (key == "--export-mps") a.exportMps = need(key);
        else if (key == "--help" || key == "-h") {
            usage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + key);
        }
    }
    const int inputModes = (a.input.empty() ? 0 : 1) + (a.mcncName.empty() ? 0 : 1) + ((!a.blocksPath.empty() || !a.netsPath.empty()) ? 1 : 0);
    if (inputModes != 1) throw std::runtime_error("provide exactly one input source: --input, --mcnc, or --blocks/--nets");
    if ((!a.blocksPath.empty() || !a.netsPath.empty()) && (a.blocksPath.empty() || a.netsPath.empty())) {
        throw std::runtime_error("--blocks and --nets must be provided together");
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Args args = parseArgs(argc, argv);
        fp::FloorplanProblem problem;
        if (!args.input.empty()) {
            problem = fp::readProblemJson(args.input);
        } else if (!args.mcncName.empty()) {
            const auto base = std::filesystem::path(args.mcncDir) / args.mcncName;
            problem = fp::readMcncBenchmark(base.string() + ".block", base.string() + ".nets");
        } else {
            problem = fp::readMcncBenchmark(args.blocksPath, args.netsPath);
        }
        if (args.layers > 0) problem.numLayers = args.layers;
        if (!args.objectiveMode.empty()) problem.objectiveMode = fp::objectiveModeFromString(args.objectiveMode);
        if (args.tsvWeight >= 0.0) problem.tsvWeight = args.tsvWeight;
        if (args.thermalWeight >= 0.0) problem.thermalWeight = args.thermalWeight;
        if (args.tsvKeepoutWeight >= 0.0) problem.tsvKeepoutWeight = args.tsvKeepoutWeight;
        if (args.thermalTsvBenefitWeight >= 0.0) problem.thermalTsvBenefitWeight = args.thermalTsvBenefitWeight;
        if (args.tsvDiameter >= 0.0) problem.tsvDiameter = args.tsvDiameter;
        if (args.tsvKeepoutRadius >= 0.0) problem.tsvKeepoutRadius = args.tsvKeepoutRadius;
        auto solver = fp::createSolver(args.solver);
        const bool threeDMode = is3DMode(args.mode);
        const fp::EvaluationMode mode = threeDMode ? fp::EvaluationMode::CT : fp::parseMode(args.mode);
        const bool needsLpSolver = mode == fp::EvaluationMode::LP ||
                                   mode == fp::EvaluationMode::SA_LP ||
                                   mode == fp::EvaluationMode::SA_CT_LP ||
                                   (threeDMode && is3DLPMode(args.mode));
        if (needsLpSolver && !solver->available()) {
            if (args.solver != "none" && args.solver != "compact" && !solver->unavailableReason().empty()) {
                throw std::runtime_error(solver->unavailableReason());
            }
            throw std::runtime_error("LP mode requires an available LP solver");
        }
        const auto start = std::chrono::steady_clock::now();

        fp::AnnealerResult result;
        fp::ThreeDAnnealerResult result3d;
        bool has3dResult = false;
        if (threeDMode) {
            fp::ThreeDOptions threeDOptions;
            threeDOptions.numLayers = std::max(1, problem.numLayers);
            if (is3DAnnealingMode(args.mode)) {
                fp::AnnealerOptions options;
                options.iterations = args.iterations;
                options.epochLength = args.autoEpochLength ? 0 : args.epochLength;
                options.maxEpochsWithoutImprovement = args.maxNoImproveEpochs;
                options.initialTemperature = args.initialTemperature;
                options.coolingRatio = args.coolingRatio;
                options.seed = args.seed;
                options.autoInitialTemperature = args.autoTemperature;
                options.verbose = args.verboseSa;
                const fp::ThreeDEvaluationMode evalMode = is3DAnnealingLPMode(args.mode)
                                                              ? fp::ThreeDEvaluationMode::LP
                                                              : fp::ThreeDEvaluationMode::Construction;
                result3d = fp::runThreeDAnnealing(problem, options, threeDOptions, evalMode, solver.get());
                if (is3DAnnealingCTLPMode(args.mode) && solver && solver->available()) {
                    fp::FloorplanSolution refined = fp::optimizeSequenceTripleByLP(problem, result3d.sequenceTriple, *solver, {}, threeDOptions);
                    if (refined.feasible) result3d.solution = refined;
                }
            } else if (args.mode == "3D-LP" || args.mode == "LP-3D") {
                result3d.sequenceTriple = fp::SequenceTriple::identity(static_cast<int>(problem.blocks.size()));
                result3d.solution = fp::optimizeSequenceTripleByLP(problem, result3d.sequenceTriple, *solver, {}, threeDOptions);
            } else {
                result3d.sequenceTriple = fp::SequenceTriple::identity(static_cast<int>(problem.blocks.size()));
                result3d.solution = fp::constructBySequenceTriple3D(problem, result3d.sequenceTriple, threeDOptions);
            }
            has3dResult = true;
        } else if (mode == fp::EvaluationMode::CT || mode == fp::EvaluationMode::LP) {
            result.sequencePair = fp::SequencePair::identity(static_cast<int>(problem.blocks.size()));
            result.solution = fp::evaluateSequencePair(problem, result.sequencePair, mode, solver.get());
        } else {
            fp::AnnealerOptions options;
            options.iterations = args.iterations;
            options.epochLength = args.autoEpochLength ? 0 : args.epochLength;
            options.maxEpochsWithoutImprovement = args.maxNoImproveEpochs;
            options.initialTemperature = args.initialTemperature;
            options.coolingRatio = args.coolingRatio;
            options.seed = args.seed;
            options.autoInitialTemperature = args.autoTemperature;
            options.verbose = args.verboseSa;
            result = fp::runAnnealing(problem, mode, solver.get(), options);
        }

        const auto stop = std::chrono::steady_clock::now();
        const double runtime = std::chrono::duration<double>(stop - start).count();
        std::filesystem::create_directories(args.output);
        auto createParent = [](const std::string& path) {
            const auto parent = std::filesystem::path(path).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
        };
        if (!args.exportLp.empty() || !args.exportMps.empty()) {
            if (has3dResult) {
                const fp::ThreeDOptions threeDOptions{std::max(1, problem.numLayers)};
                const auto build = solver->available() && is3DLPMode(args.mode)
                                       ? fp::buildCorrectedThreeDLPModelForExport(problem, result3d.sequenceTriple, *solver, {}, threeDOptions)
                                       : fp::buildInitialThreeDLPModelForExport(problem, result3d.sequenceTriple, threeDOptions);
                if (!args.exportLp.empty()) {
                    createParent(args.exportLp);
                    fp::writeLPModel(args.exportLp, build.model);
                }
                if (!args.exportMps.empty()) {
                    createParent(args.exportMps);
                    fp::writeMPSModel(args.exportMps, build.model);
                }
            } else {
            // Export the LP corresponding to the final selected sequence-pair.
            // For SA modes this avoids writing thousands of intermediate LPs.
            const auto build = solver->available()
                                   ? fp::buildCorrectedLPModelForExport(problem, result.sequencePair, *solver)
                                   : fp::buildInitialLPModelForExport(problem, result.sequencePair);
            if (!args.exportLp.empty()) {
                createParent(args.exportLp);
                fp::writeLPModel(args.exportLp, build.model);
            }
            if (!args.exportMps.empty()) {
                createParent(args.exportMps);
                fp::writeMPSModel(args.exportMps, build.model);
            }
            }
        }
        fp::RunMetadata metadata;
        metadata.mode = has3dResult ? args.mode : fp::toString(mode);
        metadata.solver = args.solver;
        metadata.iterations = args.iterations;
        metadata.seed = args.seed;
        metadata.epochLength = args.autoEpochLength ? 0 : args.epochLength;
        metadata.initialTemperature = args.initialTemperature;
        metadata.initialTemperatureUsed = has3dResult ? result3d.initialTemperatureUsed : result.initialTemperatureUsed;
        metadata.epochLengthUsed = has3dResult ? result3d.epochLengthUsed : result.epochLengthUsed;
        metadata.autoTemperature = args.autoTemperature;
        metadata.verboseAnnealing = args.verboseSa;
        metadata.totalMoves = has3dResult ? result3d.totalMoves : result.totalMoves;
        metadata.acceptedMoves = has3dResult ? result3d.acceptedMoves : result.acceptedMoves;
        metadata.coolingRatio = args.coolingRatio;
        metadata.numBlocks = static_cast<int>(problem.blocks.size());
        metadata.numNets = static_cast<int>(problem.nets.size());
        metadata.numLayers = problem.numLayers;
        metadata.objectiveMode = fp::toString(problem.objectiveMode);
        metadata.tsvWeight = problem.tsvWeight;
        metadata.thermalWeight = problem.thermalWeight;
        metadata.tsvKeepoutWeight = problem.tsvKeepoutWeight;
        metadata.thermalTsvBenefitWeight = problem.thermalTsvBenefitWeight;
        metadata.tsvDiameter = problem.tsvDiameter;
        metadata.tsvKeepoutRadius = problem.tsvKeepoutRadius;
        metadata.hasFixedOutline = problem.hasFixedOutline;
        metadata.fixedOutlineWidth = problem.fixedOutlineWidth;
        metadata.fixedOutlineHeight = problem.fixedOutlineHeight;
        metadata.chipAspectLower = problem.chipAspectLower;
        metadata.chipAspectUpper = problem.chipAspectUpper;
        if (has3dResult) {
            fp::writePlacementCsv((std::filesystem::path(args.output) / "placements.csv").string(), result3d.solution);
            fp::writeSummaryJson((std::filesystem::path(args.output) / "summary.json").string(), result3d.solution, result3d.sequenceTriple, runtime, metadata);
            fp::printSolution(result3d.solution, result3d.sequenceTriple, runtime);
            if (!result3d.solution.feasible) return 2;
        } else {
            fp::writePlacementCsv((std::filesystem::path(args.output) / "placements.csv").string(), result.solution);
            fp::writeSummaryJson((std::filesystem::path(args.output) / "summary.json").string(), result.solution, result.sequencePair, runtime, metadata);
            fp::printSolution(result.solution, result.sequencePair, runtime);
            if (!result.solution.feasible) return 2;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        usage();
        return 1;
    }
}
