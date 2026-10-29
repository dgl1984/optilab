// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
#pragma once

#include "OptiLabCorePipeline.h"
#include <cstddef>
#include <utility>
#include <vector>
#include <array>

class OptiLabCore {
public:
    enum class Mode {
        StreamPolish = 0,
        PodcastLeveler = 1,
        SmoothLimiter = 2,
    };
    struct Parameters {
        Mode mode = Mode::StreamPolish;
        double inputDriveDb = 0.0;
        double autoAdaptPct = 0.0;
    };
    struct Activity {
        double agcGain = 1.0;
        double densityGain = 1.0;
        double finalGain = 1.0;
    };

    static Mode sanitizeMode(int value) noexcept;
    static Parameters defaultParameters(Mode mode) noexcept;
    void prepare(double sampleRate);
    void setParameters(const Parameters& p);
    Parameters parameters() const noexcept { return params_; }
    Activity activity() const noexcept;
    void setActivityTracking(bool enabled) noexcept;
    std::size_t latencySamples() const noexcept;
    void reset();
    std::pair<float,float> processSample(float left,float right);
    void processPlanar(float* left,float* right,std::size_t frames);
    void processInterleaved(float* samples,std::size_t frames,std::size_t channels);

private:
    using Values = OptiLabCoreEngine::ParameterValues;
    struct Endpoints { Values low, high; };
    void rebuildEndpoints();
    void applyCurrentParameters();
    Values makePresetValues(OptiLabCorePipeline::Preset preset, const Values& overrides);
    static double clamp(double x,double lo,double hi) noexcept;
    static double smoothstep(double x) noexcept;
    void interpolateInto(Values& dst,const Values& lo,const Values& hi,double t,double adapt,Mode mode);

    OptiLabCorePipeline producer_;
    OptiLabCorePipeline scratch_;
    Parameters params_{};
    double sampleRate_ = 48000.0;
    bool prepared_ = false;
    bool hasProcessed_ = false;
    Values workingValues_;
    Endpoints stream_, podcast_, smooth_;
    Values streamMature_;
    Values podcastMiddle_;
    Values smoothMiddle_;
    std::array<std::size_t,3> modeLatencyTarget_{0,0,0};
    std::vector<float> latencyDelayL_, latencyDelayR_;
    std::size_t latencyWrite_ = 0;
    std::size_t compensationDelay_ = 0;
    std::size_t previousCompensationDelay_ = 0;
    std::size_t compensationFadeRemaining_ = 0;
    static constexpr std::size_t compensationFadeLength_ = 128;
    void updateLatencyCompensation() noexcept;
    std::pair<float,float> applyLatencyCompensation(float left,float right) noexcept;
};
