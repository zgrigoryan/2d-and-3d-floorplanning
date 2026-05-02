#include "floorplanner/DataModel.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace fp {

void FloorplanProblem::rebuildIndex() {
    blockNameToId.clear();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        blocks[i].id = i;
        blockNameToId[blocks[i].name] = i;
        if (blocks[i].type == BlockType::HARD) {
            blocks[i].area = blocks[i].fixedWidth * blocks[i].fixedHeight;
            if (blocks[i].width <= 0.0) blocks[i].width = blocks[i].fixedWidth;
            if (blocks[i].height <= 0.0) blocks[i].height = blocks[i].fixedHeight;
        }
        if (blocks[i].power <= 0.0) blocks[i].power = std::max(1.0, blocks[i].area);
    }
    for (int i = 0; i < static_cast<int>(nets.size()); ++i) nets[i].id = i;
}

std::string toString(BlockType type) {
    return type == BlockType::HARD ? "HARD" : "SOFT";
}

std::string toString(Orientation orientation) {
    return orientation == Orientation::HORIZONTAL ? "HORIZONTAL" : "VERTICAL";
}

std::string toString(ObjectiveMode mode) {
    return mode == ObjectiveMode::FixedOutline ? "fixed-outline" : "free-outline-linear";
}

BlockType blockTypeFromString(const std::string& text) {
    std::string upper = text;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper == "HARD") return BlockType::HARD;
    if (upper == "SOFT") return BlockType::SOFT;
    throw std::runtime_error("unknown block type: " + text);
}

ObjectiveMode objectiveModeFromString(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "free-outline-linear" || lower == "free" || lower == "w+h" || lower == "linear") {
        return ObjectiveMode::FreeOutlineLinear;
    }
    if (lower == "fixed-outline" || lower == "fixed" || lower == "outline") {
        return ObjectiveMode::FixedOutline;
    }
    throw std::runtime_error("unknown objective mode: " + text);
}

} // namespace fp
