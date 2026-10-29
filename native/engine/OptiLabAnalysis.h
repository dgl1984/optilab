#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

constexpr std::size_t kOptiLabAnalysisSectionCount = 6;

struct OptiLabStereoSample {
    float left = 0.0f;
    float right = 0.0f;
};

struct OptiLabAnalysisSectionFrame {
    OptiLabStereoSample input;
    OptiLabStereoSample output;
    double detectorGain = 1.0;
};

struct OptiLabAnalysisFrame {
    OptiLabStereoSample input;
    OptiLabStereoSample output;
    std::array<OptiLabAnalysisSectionFrame, kOptiLabAnalysisSectionCount> sections{};
};

struct OptiLabEndpointAnalysis {
    double samplePeakLeftDb = -120.0;
    double samplePeakRightDb = -120.0;
    double samplePeakDb = -120.0;
    double truePeakLeftDbtp = -120.0;
    double truePeakRightDbtp = -120.0;
    double truePeakDbtp = -120.0;
    double rmsLeftDb = -120.0;
    double rmsRightDb = -120.0;
    double rmsDb = -120.0;
    double dcLeftPercent = 0.0;
    double dcRightPercent = 0.0;
    double crestFactorDb = 0.0;
    double correlation = 0.0;
    double sideEnergyPercent = 0.0;
    double integratedLufs = -120.0;
    double maximumMomentaryLufs = -120.0;
    double maximumShortTermLufs = -120.0;
    double loudnessRangeLu = 0.0;
    double peakToLoudnessRatioLu = 0.0;
    std::uint64_t clippedLeftSamples = 0;
    std::uint64_t clippedRightSamples = 0;
    std::uint64_t clippedFrames = 0;
};

struct OptiLabSectionAnalysis {
    double inputRmsDb = -120.0;
    double outputRmsDb = -120.0;
    double averageLevelChangeDb = 0.0;
    double maximum400msBoostDb = 0.0;
    double maximum400msReductionDb = 0.0;
    double inputSamplePeakDb = -120.0;
    double outputSamplePeakDb = -120.0;
    double averageDetectorReductionDb = 0.0;
    double maximumDetectorReductionDb = 0.0;
    std::uint64_t inputClippedFrames = 0;
    std::uint64_t outputClippedFrames = 0;
    bool bypassed = false;
    bool hasDetectorReduction = false;
};

struct OptiLabAnalysisReport {
    double requestedSeconds = 30.0;
    double measuredSeconds = 0.0;
    double sampleRate = 48000.0;
    std::uint64_t measuredFrames = 0;
    OptiLabEndpointAnalysis input;
    OptiLabEndpointAnalysis output;
    std::array<OptiLabSectionAnalysis, kOptiLabAnalysisSectionCount> sections{};
};

class OptiLabAnalysisMeter {
public:
    OptiLabAnalysisMeter();
    ~OptiLabAnalysisMeter();

    OptiLabAnalysisMeter(const OptiLabAnalysisMeter&) = delete;
    OptiLabAnalysisMeter& operator=(const OptiLabAnalysisMeter&) = delete;

    // prepare() may allocate and must not be called from the realtime process loop.
    void prepare(double sampleRate, double maximumSeconds = 60.0);
    void start(
        double durationSeconds,
        const std::array<bool, kOptiLabAnalysisSectionCount>& sectionBypass) noexcept;
    void cancel() noexcept;
    bool process(const OptiLabAnalysisFrame& frame) noexcept;

    bool running() const noexcept;
    double progress() const noexcept;
    std::uint64_t framesProcessed() const noexcept;
    std::uint64_t targetFrames() const noexcept;
    OptiLabAnalysisReport report() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
