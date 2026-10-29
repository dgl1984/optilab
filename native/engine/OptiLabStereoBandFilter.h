#pragma once
#include <algorithm>
#include <cmath>

// Filter only the widening contribution and its transient detector, never dry
// Mid/Side. Coefficients are prepared with parameter updates, not per sample.
class OptiLabStereoBandFilter {
public:
    enum class Shape { Highpass, Lowpass };
    void configure(double rate, double frequency, Shape shape) noexcept {
        enabled_ = frequency > 0.0;
        if (!enabled_) return;
        const double k = std::tan(3.14159265358979323846
                                 * std::min(frequency, rate * 0.45) / rate);
        const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
        b0_ = shape == Shape::Highpass ? norm : k * k * norm;
        b1_ = (shape == Shape::Highpass ? -2.0 : 2.0) * b0_;
        b2_ = b0_;
        a1_ = 2.0 * (k * k - 1.0) * norm;
        a2_ = (1.0 - std::sqrt(2.0) * k + k * k) * norm;
    }
    void reset() noexcept { z1_ = z2_ = 0.0; }
    double process(double x) noexcept {
        if (!enabled_) return x;
        const double y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }
private:
    bool enabled_ = false;
    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
    double z1_ = 0.0, z2_ = 0.0;
};
