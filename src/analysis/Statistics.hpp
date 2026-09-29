#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace mm {

struct SummaryStats {
    double mean = 0.0;
    double stddev = 0.0;
    double median = 0.0;
    double p05 = 0.0;
    double p25 = 0.0;
    double p75 = 0.0;
    double p95 = 0.0;
};

inline double percentileSorted(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double idx = p * static_cast<double>(sorted.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(idx));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
    if (lo == hi) {
        return sorted[lo];
    }
    const double w = idx - static_cast<double>(lo);
    return sorted[lo] * (1.0 - w) + sorted[hi] * w;
}

inline SummaryStats summarize(std::vector<double> values) {
    SummaryStats s;
    if (values.empty()) {
        return s;
    }
    const double n = static_cast<double>(values.size());
    s.mean = std::accumulate(values.begin(), values.end(), 0.0) / n;
    double var = 0.0;
    for (double v : values) {
        const double d = v - s.mean;
        var += d * d;
    }
    s.stddev = std::sqrt(var / n);
    std::sort(values.begin(), values.end());
    s.median = percentileSorted(values, 0.50);
    s.p05 = percentileSorted(values, 0.05);
    s.p25 = percentileSorted(values, 0.25);
    s.p75 = percentileSorted(values, 0.75);
    s.p95 = percentileSorted(values, 0.95);
    return s;
}

}  // namespace mm
