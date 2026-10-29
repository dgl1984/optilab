#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// Exact acceleration of a shaped lookahead minimum. Quiet or weaker regions
// are rejected using a conservative lower bound; leaves use the original
// required + (1-required)*shape arithmetic. No allocation in update/query.
class OptiLabLookaheadEnvelope {
public:
    void prepare(int length) {
        if (length == length_) return;
        length_ = length;
        leaves_ = 1;
        while (leaves_ < length_) leaves_ *= 2;
        tree_.assign(static_cast<std::size_t>(leaves_ * 2), 1.0);
    }
    void reset() noexcept { std::fill(tree_.begin(), tree_.end(), 1.0); }
    void update(int index, double required) noexcept {
        int node = leaves_ + index;
        if (tree_[node] == required) return;
        tree_[node] = required;
        while ((node /= 2) != 0) {
            const double value = std::min(tree_[node * 2], tree_[node * 2 + 1]);
            if (tree_[node] == value) break;
            tree_[node] = value;
        }
    }
    template<class Shape>
    double minimum(int start, int count, const Shape& shape) const noexcept {
        double best = 1.0;
        const int firstCount = std::min(count, length_ - start);
        visit(1, 0, leaves_, start, start + firstCount, -start, shape, best);
        if (firstCount < count)
            visit(1, 0, leaves_, 0, count - firstCount, firstCount, shape, best);
        return best;
    }
private:
    template<class Shape>
    void visit(int node, int begin, int end, int first, int last, int offset,
               const Shape& shape, double& best) const noexcept {
        if (end <= first || begin >= last || tree_[node] == 1.0) return;
        const int distance = std::max(begin, first) + offset;
        const double required = tree_[node];
        const double bound = required + (1.0 - required) * shape(distance);
        // Cover floating-point rounding of both the bound and leaf formula.
        if (bound - 4.0 * std::numeric_limits<double>::epsilon() >= best) return;
        if (end - begin == 1) { best = std::min(best, bound); return; }
        const int middle = begin + (end - begin) / 2;
        visit(node * 2, begin, middle, first, last, offset, shape, best);
        visit(node * 2 + 1, middle, end, first, last, offset, shape, best);
    }
    int length_ = 0, leaves_ = 0;
    std::vector<double> tree_;
};
