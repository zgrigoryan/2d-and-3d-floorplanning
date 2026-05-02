#pragma once

#include "floorplanner/DataModel.h"

#include <random>
#include <string>
#include <vector>

namespace fp {

enum class TripleRelation { LEFT_OF, BELOW, LOWER_LAYER };

struct TriplePairRelation {
    int i = -1;
    int j = -1;
    TripleRelation relation = TripleRelation::LEFT_OF;
};

class SequenceTriple {
public:
    std::vector<int> gamma1;
    std::vector<int> gamma2;
    std::vector<int> gamma3;
    std::vector<bool> rotated;

    SequenceTriple() = default;
    explicit SequenceTriple(int n);
    SequenceTriple(std::vector<int> g1, std::vector<int> g2, std::vector<int> g3);

    static SequenceTriple identity(int n);
    static SequenceTriple random(int n, std::mt19937& rng);

    bool validate(int n) const;
    std::vector<int> inverse1() const;
    std::vector<int> inverse2() const;
    std::vector<int> inverse3() const;
    std::vector<TriplePairRelation> orderedRelations() const;
    std::vector<int> decodeLayers(int numLayers) const;

    void mutate(std::mt19937& rng);
    std::string toString() const;

private:
    static bool checkPermutation(const std::vector<int>& seq, int n);
};

std::string toString(TripleRelation relation);

} // namespace fp
