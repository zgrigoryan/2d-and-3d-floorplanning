#include "floorplanner/SequenceTriple.h"

#include <algorithm>
#include <numeric>
#include <sstream>

namespace fp {

SequenceTriple::SequenceTriple(int n)
    : gamma1(n), gamma2(n), gamma3(n), rotated(n, false) {
    std::iota(gamma1.begin(), gamma1.end(), 0);
    gamma2 = gamma1;
    gamma3 = gamma1;
}

SequenceTriple::SequenceTriple(std::vector<int> g1, std::vector<int> g2, std::vector<int> g3)
    : gamma1(std::move(g1)), gamma2(std::move(g2)), gamma3(std::move(g3)), rotated(gamma1.size(), false) {}

SequenceTriple SequenceTriple::identity(int n) {
    return SequenceTriple(n);
}

SequenceTriple SequenceTriple::random(int n, std::mt19937& rng) {
    SequenceTriple st(n);
    std::shuffle(st.gamma1.begin(), st.gamma1.end(), rng);
    std::shuffle(st.gamma2.begin(), st.gamma2.end(), rng);
    std::shuffle(st.gamma3.begin(), st.gamma3.end(), rng);
    std::bernoulli_distribution flip(0.5);
    for (int i = 0; i < n; ++i) st.rotated[i] = flip(rng);
    return st;
}

bool SequenceTriple::checkPermutation(const std::vector<int>& seq, int n) {
    if (static_cast<int>(seq.size()) != n) return false;
    std::vector<int> seen(n, 0);
    for (int value : seq) {
        if (value < 0 || value >= n || seen[value]) return false;
        seen[value] = 1;
    }
    return true;
}

bool SequenceTriple::validate(int n) const {
    return checkPermutation(gamma1, n) &&
           checkPermutation(gamma2, n) &&
           checkPermutation(gamma3, n) &&
           (rotated.empty() || static_cast<int>(rotated.size()) == n);
}

std::vector<int> SequenceTriple::inverse1() const {
    std::vector<int> inv(gamma1.size());
    for (int i = 0; i < static_cast<int>(gamma1.size()); ++i) inv[gamma1[i]] = i;
    return inv;
}

std::vector<int> SequenceTriple::inverse2() const {
    std::vector<int> inv(gamma2.size());
    for (int i = 0; i < static_cast<int>(gamma2.size()); ++i) inv[gamma2[i]] = i;
    return inv;
}

std::vector<int> SequenceTriple::inverse3() const {
    std::vector<int> inv(gamma3.size());
    for (int i = 0; i < static_cast<int>(gamma3.size()); ++i) inv[gamma3[i]] = i;
    return inv;
}

std::vector<TriplePairRelation> SequenceTriple::orderedRelations() const {
    // Matches the companion 3D-Floorplanner sequence-triple interpretation:
    // for each A before B in gamma1, gamma2 decides LEFT vs crossing, and
    // gamma3 splits crossing pairs into BELOW vs LOWER_LAYER.
    const int n = static_cast<int>(gamma1.size());
    const auto p2 = inverse2();
    const auto p3 = inverse3();
    std::vector<TriplePairRelation> out;
    out.reserve(n * (n - 1) / 2);
    for (int idxB = 0; idxB < n; ++idxB) {
        const int b = gamma1[idxB];
        for (int idxA = 0; idxA < idxB; ++idxA) {
            const int a = gamma1[idxA];
            TripleRelation relation = TripleRelation::LOWER_LAYER;
            if (p2[a] < p2[b]) relation = TripleRelation::LEFT_OF;
            else if (p3[a] < p3[b]) relation = TripleRelation::BELOW;
            out.push_back({a, b, relation});
        }
    }
    return out;
}

std::vector<int> SequenceTriple::decodeLayers(int numLayers) const {
    const int n = static_cast<int>(gamma1.size());
    const int layers = std::max(1, numLayers);
    const auto p2 = inverse2();
    const auto p3 = inverse3();
    std::vector<int> layer(n, 0);
    for (int idxB = 0; idxB < n; ++idxB) {
        const int b = gamma1[idxB];
        for (int idxA = 0; idxA < idxB; ++idxA) {
            const int a = gamma1[idxA];
            if (p2[a] > p2[b] && p3[a] > p3[b]) {
                layer[b] = std::max(layer[b], layer[a] + 1);
            }
        }
        layer[b] = std::min(layer[b], layers - 1);
    }
    return layer;
}

void SequenceTriple::mutate(std::mt19937& rng) {
    const int n = static_cast<int>(gamma1.size());
    if (n < 2) return;
    if (rotated.size() != gamma1.size()) rotated.assign(gamma1.size(), false);
    std::uniform_int_distribution<int> moveDist(0, 4);
    std::uniform_int_distribution<int> idxDist(0, n - 1);
    const int move = moveDist(rng);
    if (move <= 3) {
        int a = idxDist(rng);
        int b = idxDist(rng);
        while (b == a) b = idxDist(rng);
        if (move == 0) std::swap(gamma1[a], gamma1[b]);
        else if (move == 1) std::swap(gamma2[a], gamma2[b]);
        else if (move == 2) std::swap(gamma3[a], gamma3[b]);
        else {
            std::swap(gamma1[a], gamma1[b]);
            std::swap(gamma2[a], gamma2[b]);
        }
    } else {
        const int idx = idxDist(rng);
        rotated[idx] = !rotated[idx];
    }
}

std::string SequenceTriple::toString() const {
    auto seqToString = [](const std::vector<int>& seq) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < seq.size(); ++i) {
            if (i) oss << ",";
            oss << seq[i];
        }
        oss << "]";
        return oss.str();
    };
    return "g1=" + seqToString(gamma1) + " g2=" + seqToString(gamma2) + " g3=" + seqToString(gamma3);
}

std::string toString(TripleRelation relation) {
    switch (relation) {
        case TripleRelation::LEFT_OF: return "LEFT_OF";
        case TripleRelation::BELOW: return "BELOW";
        case TripleRelation::LOWER_LAYER: return "LOWER_LAYER";
    }
    return "UNKNOWN";
}

} // namespace fp
