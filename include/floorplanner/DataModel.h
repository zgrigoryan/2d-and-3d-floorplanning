#pragma once

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace fp {

enum class BlockType { HARD, SOFT };
enum class Orientation { HORIZONTAL, VERTICAL };
enum class ObjectiveMode { FreeOutlineLinear, FixedOutline };

struct PadLocation {
    double x = 0.0;
    double y = 0.0;
};

struct Block {
    int id = -1;
    std::string name;
    BlockType type = BlockType::SOFT;
    double area = 0.0;
    double fixedWidth = 0.0;
    double fixedHeight = 0.0;
    double minAspectRatio = 1.0;
    double maxAspectRatio = 1.0;
    double width = 0.0;
    double height = 0.0;
    double x = 0.0;
    double y = 0.0;
    int layer = 0;
    double power = 0.0;
    Orientation orientation = Orientation::HORIZONTAL;
};

struct Net {
    int id = -1;
    std::string name;
    std::vector<int> blockIds;
    std::vector<PadLocation> pads;
};

struct FloorplanProblem {
    std::vector<Block> blocks;
    std::vector<Net> nets;
    double chipAspectLower = 0.0;
    double chipAspectUpper = 1e30;
    double areaWeight = 1.0;
    double wireWeight = 1.0;
    double tsvWeight = 1.0;
    double thermalWeight = 0.0;
    double tsvKeepoutWeight = 0.0;
    double thermalTsvBenefitWeight = 0.0;
    ObjectiveMode objectiveMode = ObjectiveMode::FreeOutlineLinear;
    int numLayers = 1;
    double tsvDiameter = 1.0;
    double tsvKeepoutRadius = 0.0;
    bool hasFixedOutline = false;
    double fixedOutlineWidth = 0.0;
    double fixedOutlineHeight = 0.0;
    std::unordered_map<std::string, int> blockNameToId;

    void rebuildIndex();
};

struct BlockPlacement {
    std::string name;
    BlockType type = BlockType::SOFT;
    double x = 0.0;
    double y = 0.0;
    int layer = 0;
    double width = 0.0;
    double height = 0.0;
};

struct FloorplanSolution {
    std::vector<BlockPlacement> placements;
    double chipWidth = 0.0;
    double chipHeight = 0.0;
    double chipArea = 0.0;
    double totalWirelength = 0.0;
    double totalTsvCount = 0.0;
    double thermalCost = 0.0;
    double tsvOverlapCost = 0.0;
    double objectiveValue = std::numeric_limits<double>::infinity();
    bool feasible = false;
    std::string status = "uninitialized";
};

std::string toString(BlockType type);
std::string toString(Orientation orientation);
std::string toString(ObjectiveMode mode);
BlockType blockTypeFromString(const std::string& text);
ObjectiveMode objectiveModeFromString(const std::string& text);

} // namespace fp
