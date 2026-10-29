#pragma once

#include <algorithm>
#include <cmath>
#include "OptiLabStereoType.h"
#include "OptiLabStereoBandFilter.h"

// One widening processor with selectable envelope laws and a shared band filter.
// Original Side is never filtered; optional band filters affect the added Side
// and the transient detector only. Mid therefore remains exactly unchanged.
class OptiLabStereoEnhancer {
public:
    void configure(double sampleRate, double highpassHz, int speed,
                   double ratioLimitPct, double sensitivityPct,
                   OptiLabStereoType type = OptiLabStereoType::Adaptive,
                   double lowpassHz = 0.0) noexcept {
        if (optilabStereoTypeName(static_cast<int>(type)) == nullptr)
            type = OptiLabStereoType::Adaptive;
        const bool typeChanged = type != type_;
        type_ = type;
        const auto& law = profiles_[static_cast<std::size_t>(type_)];
        const double timeScale = speed == 0 ? 1.5 : speed == 2 ? 2.0 / 3.0 : 1.0;
        const auto coefficient = [sampleRate, timeScale](double seconds) {
            return std::exp(-1.0 / (sampleRate * seconds * timeScale));
        };
        fastCoefficient_ = coefficient(0.005);
        slowCoefficient_ = coefficient(0.050);
        levelAttack_ = coefficient(0.008);
        levelRelease_ = coefficient(0.180);
        coherenceCoefficient_ = coefficient(0.040);
        gainAttack_ = coefficient(law.attackSeconds);
        gainRelease_ = coefficient(law.releaseSeconds);
        legacyFastAttack_ = coefficient(0.0015);
        legacyFastRelease_ = coefficient(0.050);
        legacySlowAttack_ = coefficient(0.030);
        legacySlowRelease_ = coefficient(0.250);
        ratioLimit_ = std::clamp(ratioLimitPct * 0.01, 0.70, 1.0);
        sensitivity_ = 1.5 + 3.0 * std::clamp(sensitivityPct * 0.01, 0.0, 1.0);
        if (sampleRate != filterRate_ || highpassHz != filterHz_ || lowpassHz != lowpassHz_ || typeChanged) {
            filterRate_ = sampleRate;
            filterHz_ = highpassHz;
            lowpassHz_ = lowpassHz;
            midHighpass_.configure(sampleRate, highpassHz, OptiLabStereoBandFilter::Shape::Highpass);
            sideHighpass_.configure(sampleRate, highpassHz, OptiLabStereoBandFilter::Shape::Highpass);
            midLowpass_.configure(sampleRate, lowpassHz, OptiLabStereoBandFilter::Shape::Lowpass);
            sideLowpass_.configure(sampleRate, lowpassHz, OptiLabStereoBandFilter::Shape::Lowpass);
            reset();
        }
    }

    void reset() noexcept {
        midHighpass_.reset();
        sideHighpass_.reset();
        midLowpass_.reset(); sideLowpass_.reset();
        legacyFast_ = legacySlow_ = 0.0;
        fastEnergy_ = slowEnergy_ = 0.0;
        midLevel_ = sideLevel_ = bandSideLevel_ = 0.0;
        midEnergy_ = sideEnergy_ = crossEnergy_ = 0.0;
        addedGain_ = 0.0;
    }

    double process(double mid, double side, double maximumExtraGain) noexcept {
        const double bandMid = midLowpass_.process(midHighpass_.process(mid));
        const double bandSide = sideLowpass_.process(sideHighpass_.process(side));
        const auto& law = profiles_[static_cast<std::size_t>(type_)];
        // Energy estimates separate the transient contribution from the
        // intentional sustained widening below.
        if (law.legacyEnvelope) {
            const double magnitude = std::abs(bandMid);
            legacyFast_ = smooth(legacyFast_, magnitude,
                magnitude > legacyFast_ ? legacyFastAttack_ : legacyFastRelease_);
            legacySlow_ = smooth(legacySlow_, magnitude,
                magnitude > legacySlow_ ? legacySlowAttack_ : legacySlowRelease_);
        } else {
            const double power = bandMid * bandMid;
            fastEnergy_ = smooth(fastEnergy_, power, fastCoefficient_);
            slowEnergy_ = smooth(slowEnergy_, power, slowCoefficient_);
        }
        midLevel_ = envelope(midLevel_, std::abs(mid));
        sideLevel_ = envelope(sideLevel_, std::abs(side));
        bandSideLevel_ = envelope(bandSideLevel_, std::abs(bandSide));
        if (!law.legacyEnvelope) {
            midEnergy_ = smooth(midEnergy_, mid * mid, coherenceCoefficient_);
            sideEnergy_ = smooth(sideEnergy_, side * side, coherenceCoefficient_);
            crossEnergy_ = smooth(crossEnergy_, mid * side, coherenceCoefficient_);
        }

        // Whole Stereo/FM bypass skips this processor and freezes its history.
        // At unity, track the input but discard the old widening envelope.
        if (maximumExtraGain <= 0.0) {
            addedGain_ = 0.0;
            return side;
        }
        const double ratio = sideLevel_ / std::max(midLevel_, tiny);
        const double transient = law.legacyEnvelope
            ? unit((legacyFast_ / std::max(legacySlow_, tiny) - 1.0) * sensitivity_ * 0.5)
            : unit((std::sqrt(fastEnergy_ / std::max(slowEnergy_, tiny)) - 1.15) * sensitivity_);
        const double sideGate = unit((ratio - law.sideGateStart) / law.sideGateSpan);
        const double levelGate = unit((midLevel_ - 0.001) / 0.009);
        // M and S are proportional for mono with a channel-level imbalance.
        // Reject that case even if its Side level passes the width gate.
        double monoGate = 1.0;
        if (!law.legacyEnvelope) {
            const double denominator = std::sqrt(std::max(0.0, midEnergy_ * sideEnergy_));
            const double coherence = denominator > tiny
                ? unit(std::abs(crossEnergy_) / denominator) : 1.0;
            monoGate = unit((1.0 - coherence) / 0.05);
        }
        // Conservative envelope headroom for original Side + filtered addition.
        // Never narrow an already wide source merely to satisfy this limit.
        const double headroom = std::max(0.0, ratioLimit_ * midLevel_ - sideLevel_);
        const double cap = headroom / std::max(bandSideLevel_, tiny);
        // Each profile chooses its steady and transient contributions explicitly.
        const double narrowness = unit((0.45 - ratio) / 0.35);
        const double character = unit(law.foundation + law.narrowness * narrowness + law.transient * transient);
        if (law.legacyEnvelope) {
            const double extra = std::clamp(maximumExtraGain, 0.0, 0.5);
            const double normalizedCap = unit(cap / extra);
            const double target = character * sideGate * levelGate * normalizedCap;
            addedGain_ = smooth(addedGain_, target,
                               target > addedGain_ ? gainAttack_ : gainRelease_);
            addedGain_ = std::min(addedGain_, normalizedCap);
            return side + extra * addedGain_ * bandSide;
        }
        const double target = std::min(std::clamp(maximumExtraGain, 0.0, 0.5), cap)
                            * character * sideGate * levelGate * monoGate;
        addedGain_ = smooth(addedGain_, target,
                           target > addedGain_ ? gainAttack_ : gainRelease_);
        addedGain_ = std::min({addedGain_, cap, maximumExtraGain});
        return side + addedGain_ * bandSide;
    }

private:
    struct Profile {
        bool legacyEnvelope;
        double foundation, narrowness, transient, attackSeconds, releaseSeconds;
        double sideGateStart, sideGateSpan;
    };
    // Average recovers the original law; Sustain is the auditioned variant.
    // Transients and Adaptive share the energy detector and mono protection.
    inline static constexpr std::array<Profile, 4> profiles_ {{
        {true,  0.35, 0.25, 0.40, 0.035, 0.0005, 0.012, 0.048},
        {false, 0.00, 0.00, 1.00, 0.002, 0.080,  0.060, 0.060},
        {true,  0.70, 0.00, 0.30, 0.035, 0.350,  0.012, 0.048},
        {false, 0.35, 0.25, 0.40, 0.020, 0.350,  0.012, 0.048}
    }};
    static_assert(profiles_.size() == optilabStereoTypeNames.size());
    static constexpr double tiny = 1.0e-30;
    static double unit(double x) noexcept { return std::clamp(x, 0.0, 1.0); }
    static double smooth(double previous, double input, double coefficient) noexcept {
        return input + coefficient * (previous - input);
    }
    double envelope(double previous, double input) const noexcept {
        return smooth(previous, input, input > previous ? levelAttack_ : levelRelease_);
    }
    OptiLabStereoBandFilter midHighpass_, sideHighpass_, midLowpass_, sideLowpass_;
    OptiLabStereoType type_ = OptiLabStereoType::Adaptive;
    double lowpassHz_ = -1.0;
    double legacyFastAttack_ = 0.0, legacyFastRelease_ = 0.0;
    double legacySlowAttack_ = 0.0, legacySlowRelease_ = 0.0;
    double legacyFast_ = 0.0, legacySlow_ = 0.0;
    double filterRate_ = 0.0, filterHz_ = -1.0;
    double fastCoefficient_ = 0.0, slowCoefficient_ = 0.0;
    double levelAttack_ = 0.0, levelRelease_ = 0.0, coherenceCoefficient_ = 0.0;
    double gainAttack_ = 0.0, gainRelease_ = 0.0;
    double ratioLimit_ = 0.92, sensitivity_ = 3.0;
    double fastEnergy_ = 0.0, slowEnergy_ = 0.0;
    double midLevel_ = 0.0, sideLevel_ = 0.0, bandSideLevel_ = 0.0;
    double midEnergy_ = 0.0, sideEnergy_ = 0.0, crossEnergy_ = 0.0;
    double addedGain_ = 0.0;
};
