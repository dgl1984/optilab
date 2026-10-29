#include "OptiLabAnalysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr double kSilenceDb = -120.0;
constexpr double kTiny = 1.0e-24;
constexpr double kPi = 3.1415926535897932384626433832795;

double amplitudeDb(double value) noexcept {
    return value > 1.0e-6 ? 20.0 * std::log10(value) : kSilenceDb;
}

double powerDb(double value) noexcept {
    return value > 1.0e-12 ? 10.0 * std::log10(value) : kSilenceDb;
}

double loudnessFromPower(double value) noexcept {
    return value > 1.0e-12 ? -0.691 + 10.0 * std::log10(value) : kSilenceDb;
}

double gainReductionDb(double gain) noexcept {
    return gain < 1.0 ? std::max(0.0, -20.0 * std::log10(std::max(gain, 1.0e-12))) : 0.0;
}

struct Biquad {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double z1 = 0.0;
    double z2 = 0.0;

    double run(double x) noexcept {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept {
        z1 = 0.0;
        z2 = 0.0;
    }
};

Biquad makeHighShelf(
    double sampleRate, double frequency, double gainDb, double q) noexcept {
    const double k = std::tan(kPi * frequency / sampleRate);
    const double vh = std::pow(10.0, gainDb / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + k / q + k * k;

    Biquad result;
    result.b0 = (vh + vb * k / q + k * k) / a0;
    result.b1 = 2.0 * (k * k - vh) / a0;
    result.b2 = (vh - vb * k / q + k * k) / a0;
    result.a1 = 2.0 * (k * k - 1.0) / a0;
    result.a2 = (1.0 - k / q + k * k) / a0;
    return result;
}

Biquad makeHighPass(double sampleRate, double frequency, double q) noexcept {
    const double k = std::tan(kPi * frequency / sampleRate);
    const double a0 = 1.0 + k / q + k * k;

    Biquad result;
    result.b0 = 1.0;
    result.b1 = -2.0;
    result.b2 = 1.0;
    result.a1 = 2.0 * (k * k - 1.0) / a0;
    result.a2 = (1.0 - k / q + k * k) / a0;
    return result;
}

struct KWeighting {
    Biquad shelf;
    Biquad highPass;

    void prepare(double sampleRate) noexcept {
        // BS.1770 K-weighting response, expressed as sample-rate-independent
        // De Man biquads. At 48 kHz these reproduce the published coefficients.
        shelf = makeHighShelf(
            sampleRate, 1681.974450955533, 3.999843853973347,
            0.7071752369554196);
        highPass = makeHighPass(
            sampleRate, 38.13547087602444, 0.5003270373238773);
    }

    void reset() noexcept {
        shelf.reset();
        highPass.reset();
    }

    double run(double value) noexcept {
        return highPass.run(shelf.run(value));
    }
};

struct TruePeakEstimator {
    static constexpr std::array<std::array<double, 4>, 12> coefficients{{
        {{ 0.0017089843750, -0.0291748046875, -0.0189208984375, -0.0083007812500}},
        {{ 0.0109863281250,  0.0292968750000,  0.0330810546875,  0.0148925781250}},
        {{-0.0196533203125, -0.0517578125000, -0.0582275390625, -0.0266113281250}},
        {{ 0.0332031250000,  0.0891113281250,  0.1015625000000,  0.0476074218750}},
        {{-0.0594482421875, -0.1665039062500, -0.2003173828125, -0.1022949218750}},
        {{ 0.1373291015625,  0.4650878906250,  0.7797851562500,  0.9721679687500}},
        {{ 0.9721679687500,  0.7797851562500,  0.4650878906250,  0.1373291015625}},
        {{-0.1022949218750, -0.2003173828125, -0.1665039062500, -0.0594482421875}},
        {{ 0.0476074218750,  0.1015625000000,  0.0891113281250,  0.0332031250000}},
        {{-0.0266113281250, -0.0582275390625, -0.0517578125000, -0.0196533203125}},
        {{ 0.0148925781250,  0.0330810546875,  0.0292968750000,  0.0109863281250}},
        {{-0.0083007812500, -0.0189208984375, -0.0291748046875,  0.0017089843750}},
    }};

    std::array<double, 12> history{};
    std::size_t available = 0;
    bool oversample = true;
    double peak = 0.0;

    void prepare(double sampleRate) noexcept {
        // BS.1770 recommends measuring at 192 kHz or above. Four phases cover
        // common 44.1/48 kHz hosts and remain conservative at 88.2/96 kHz.
        oversample = sampleRate < 192000.0;
        reset();
    }

    void reset() noexcept {
        history.fill(0.0);
        available = 0;
        peak = 0.0;
    }

    void process(double value) noexcept {
        peak = std::max(peak, std::abs(value));
        history[0] = history[1];
        for (std::size_t index = 1; index + 1 < history.size(); ++index) {
            history[index] = history[index + 1];
        }
        history.back() = value;
        available = std::min<std::size_t>(available + 1, history.size());
        if (available < history.size() || !oversample) return;

        for (std::size_t phase = 0; phase < 4; ++phase) {
            double interpolated = 0.0;
            for (std::size_t tap = 0; tap < history.size(); ++tap) {
                interpolated +=
                    coefficients[tap][phase] * history[history.size() - 1 - tap];
            }
            peak = std::max(peak, std::abs(interpolated));
        }
    }
};

double gatedIntegratedLoudness(const double* blocks, std::size_t count) noexcept {
    double absolutePower = 0.0;
    std::size_t absoluteCount = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (loudnessFromPower(blocks[index]) >= -70.0) {
            absolutePower += blocks[index];
            ++absoluteCount;
        }
    }
    if (absoluteCount == 0) return kSilenceDb;

    const double relativeThreshold =
        loudnessFromPower(absolutePower / static_cast<double>(absoluteCount)) - 10.0;
    double gatedPower = 0.0;
    std::size_t gatedCount = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const double loudness = loudnessFromPower(blocks[index]);
        if (loudness >= -70.0 && loudness >= relativeThreshold) {
            gatedPower += blocks[index];
            ++gatedCount;
        }
    }
    return gatedCount
        ? loudnessFromPower(gatedPower / static_cast<double>(gatedCount))
        : kSilenceDb;
}

double percentile(std::vector<double>& values, double fraction) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = fraction * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    const double mix = position - static_cast<double>(lower);
    return values[lower] + mix * (values[upper] - values[lower]);
}

double loudnessRange(
    const double* shortTermPowers, std::size_t count) {
    std::vector<double> absolutePowers;
    absolutePowers.reserve(count);
    double sum = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        if (loudnessFromPower(shortTermPowers[index]) >= -70.0) {
            absolutePowers.push_back(shortTermPowers[index]);
            sum += shortTermPowers[index];
        }
    }
    if (absolutePowers.size() < 2) return 0.0;

    const double relativeThreshold =
        loudnessFromPower(sum / static_cast<double>(absolutePowers.size())) - 20.0;
    std::vector<double> gatedLoudness;
    gatedLoudness.reserve(absolutePowers.size());
    for (double power : absolutePowers) {
        const double loudness = loudnessFromPower(power);
        if (loudness >= relativeThreshold) gatedLoudness.push_back(loudness);
    }
    if (gatedLoudness.size() < 2) return 0.0;
    const double low = percentile(gatedLoudness, 0.10);
    const double high = percentile(gatedLoudness, 0.95);
    return std::max(0.0, high - low);
}

struct EndpointState {
    double sampleRate = 48000.0;
    std::uint64_t count = 0;
    double sumLeft = 0.0;
    double sumRight = 0.0;
    double sumLeftSquared = 0.0;
    double sumRightSquared = 0.0;
    double sumLeftRight = 0.0;
    double sumMidSquared = 0.0;
    double sumSideSquared = 0.0;
    double samplePeakLeft = 0.0;
    double samplePeakRight = 0.0;
    std::uint64_t clippedLeft = 0;
    std::uint64_t clippedRight = 0;
    std::uint64_t clippedFrames = 0;
    TruePeakEstimator truePeakLeft;
    TruePeakEstimator truePeakRight;
    KWeighting weightingLeft;
    KWeighting weightingRight;
    std::vector<double> powerRing;
    std::size_t powerPosition = 0;
    std::size_t momentarySamples = 1;
    std::size_t shortTermSamples = 1;
    std::size_t hopSamples = 1;
    std::size_t samplesSinceHop = 0;
    double momentaryPowerSum = 0.0;
    double shortTermPowerSum = 0.0;
    std::vector<double> momentaryBlocks;
    std::vector<double> shortTermBlocks;
    std::size_t momentaryBlockCount = 0;
    std::size_t shortTermBlockCount = 0;
    double maximumMomentary = kSilenceDb;
    double maximumShortTerm = kSilenceDb;

    void prepare(double newSampleRate, double maximumSeconds) {
        sampleRate = std::max(1.0, newSampleRate);
        momentarySamples =
            std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate * 0.400)));
        shortTermSamples =
            std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate * 3.000)));
        hopSamples =
            std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate * 0.100)));
        powerRing.assign(shortTermSamples, 0.0);
        const std::size_t maximumHops =
            static_cast<std::size_t>(std::ceil(maximumSeconds * sampleRate
                                              / static_cast<double>(hopSamples))) + 4;
        momentaryBlocks.assign(maximumHops, 0.0);
        shortTermBlocks.assign(maximumHops, 0.0);
        truePeakLeft.prepare(sampleRate);
        truePeakRight.prepare(sampleRate);
        weightingLeft.prepare(sampleRate);
        weightingRight.prepare(sampleRate);
        reset();
    }

    void reset() noexcept {
        count = 0;
        sumLeft = sumRight = 0.0;
        sumLeftSquared = sumRightSquared = sumLeftRight = 0.0;
        sumMidSquared = sumSideSquared = 0.0;
        samplePeakLeft = samplePeakRight = 0.0;
        clippedLeft = clippedRight = clippedFrames = 0;
        truePeakLeft.reset();
        truePeakRight.reset();
        weightingLeft.reset();
        weightingRight.reset();
        std::fill(powerRing.begin(), powerRing.end(), 0.0);
        powerPosition = 0;
        samplesSinceHop = 0;
        momentaryPowerSum = shortTermPowerSum = 0.0;
        momentaryBlockCount = shortTermBlockCount = 0;
        maximumMomentary = maximumShortTerm = kSilenceDb;
    }

    void process(double left, double right) noexcept {
        ++count;
        sumLeft += left;
        sumRight += right;
        sumLeftSquared += left * left;
        sumRightSquared += right * right;
        sumLeftRight += left * right;
        const double mid = (left + right) * 0.5;
        const double side = (left - right) * 0.5;
        sumMidSquared += mid * mid;
        sumSideSquared += side * side;
        samplePeakLeft = std::max(samplePeakLeft, std::abs(left));
        samplePeakRight = std::max(samplePeakRight, std::abs(right));
        const bool leftClipped = std::abs(left) >= 1.0;
        const bool rightClipped = std::abs(right) >= 1.0;
        clippedLeft += leftClipped ? 1u : 0u;
        clippedRight += rightClipped ? 1u : 0u;
        clippedFrames += leftClipped || rightClipped ? 1u : 0u;
        truePeakLeft.process(left);
        truePeakRight.process(right);

        const double weightedLeft = weightingLeft.run(left);
        const double weightedRight = weightingRight.run(right);
        const double power = weightedLeft * weightedLeft + weightedRight * weightedRight;
        if (count > shortTermSamples) {
            shortTermPowerSum -= powerRing[powerPosition];
        }
        if (count > momentarySamples) {
            const std::size_t oldMomentary =
                (powerPosition + powerRing.size() - momentarySamples) % powerRing.size();
            momentaryPowerSum -= powerRing[oldMomentary];
        }
        powerRing[powerPosition] = power;
        powerPosition = (powerPosition + 1) % powerRing.size();
        momentaryPowerSum += power;
        shortTermPowerSum += power;

        ++samplesSinceHop;
        if (samplesSinceHop < hopSamples) return;
        samplesSinceHop = 0;
        if (count >= momentarySamples && momentaryBlockCount < momentaryBlocks.size()) {
            const double mean = std::max(
                0.0, momentaryPowerSum / static_cast<double>(momentarySamples));
            momentaryBlocks[momentaryBlockCount++] = mean;
            maximumMomentary = std::max(maximumMomentary, loudnessFromPower(mean));
        }
        if (count >= shortTermSamples && shortTermBlockCount < shortTermBlocks.size()) {
            const double mean =
                std::max(0.0, shortTermPowerSum / static_cast<double>(shortTermSamples));
            shortTermBlocks[shortTermBlockCount++] = mean;
            maximumShortTerm = std::max(maximumShortTerm, loudnessFromPower(mean));
        }
    }

    OptiLabEndpointAnalysis finish() const {
        OptiLabEndpointAnalysis result;
        if (count == 0) return result;
        const double frames = static_cast<double>(count);
        const double rmsLeft = std::sqrt(std::max(0.0, sumLeftSquared / frames));
        const double rmsRight = std::sqrt(std::max(0.0, sumRightSquared / frames));
        const double rms = std::sqrt(
            std::max(0.0, (sumLeftSquared + sumRightSquared) / (2.0 * frames)));
        const double samplePeak = std::max(samplePeakLeft, samplePeakRight);
        const double truePeak = std::max(truePeakLeft.peak, truePeakRight.peak);
        const double denominator =
            std::sqrt(std::max(0.0, sumLeftSquared * sumRightSquared));
        const double stereoEnergy = sumMidSquared + sumSideSquared;
        const double integrated =
            gatedIntegratedLoudness(momentaryBlocks.data(), momentaryBlockCount);

        result.samplePeakLeftDb = amplitudeDb(samplePeakLeft);
        result.samplePeakRightDb = amplitudeDb(samplePeakRight);
        result.samplePeakDb = amplitudeDb(samplePeak);
        result.truePeakLeftDbtp = amplitudeDb(truePeakLeft.peak);
        result.truePeakRightDbtp = amplitudeDb(truePeakRight.peak);
        result.truePeakDbtp = amplitudeDb(truePeak);
        result.rmsLeftDb = amplitudeDb(rmsLeft);
        result.rmsRightDb = amplitudeDb(rmsRight);
        result.rmsDb = amplitudeDb(rms);
        result.dcLeftPercent = 100.0 * sumLeft / frames;
        result.dcRightPercent = 100.0 * sumRight / frames;
        result.crestFactorDb =
            samplePeak > 1.0e-6 && rms > 1.0e-6
                ? amplitudeDb(samplePeak) - amplitudeDb(rms) : 0.0;
        result.correlation =
            denominator > kTiny ? std::clamp(sumLeftRight / denominator, -1.0, 1.0) : 0.0;
        result.sideEnergyPercent =
            stereoEnergy > kTiny ? 100.0 * sumSideSquared / stereoEnergy : 0.0;
        result.integratedLufs = integrated;
        result.maximumMomentaryLufs = maximumMomentary;
        result.maximumShortTermLufs = maximumShortTerm;
        result.loudnessRangeLu =
            loudnessRange(shortTermBlocks.data(), shortTermBlockCount);
        result.peakToLoudnessRatioLu =
            integrated > kSilenceDb + 0.1 ? result.truePeakDbtp - integrated : 0.0;
        result.clippedLeftSamples = clippedLeft;
        result.clippedRightSamples = clippedRight;
        result.clippedFrames = clippedFrames;
        return result;
    }
};

struct SectionState {
    std::uint64_t count = 0;
    double totalInputPower = 0.0;
    double totalOutputPower = 0.0;
    double inputPeak = 0.0;
    double outputPeak = 0.0;
    std::uint64_t inputClipped = 0;
    std::uint64_t outputClipped = 0;
    double detectorReductionSum = 0.0;
    double detectorReductionMaximum = 0.0;
    std::uint64_t detectorCount = 0;
    std::size_t hopSamples = 4800;
    std::size_t bucketSamples = 0;
    double bucketInputPower = 0.0;
    double bucketOutputPower = 0.0;
    std::array<double, 4> inputBuckets{};
    std::array<double, 4> outputBuckets{};
    std::size_t bucketPosition = 0;
    std::size_t completedBuckets = 0;
    double rollingInputPower = 0.0;
    double rollingOutputPower = 0.0;
    double maximumBoost = 0.0;
    double maximumReduction = 0.0;
    bool bypassed = false;
    bool hasDetector = false;

    void prepare(double sampleRate) noexcept {
        hopSamples =
            std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate * 0.100)));
    }

    void reset(bool isBypassed, bool detector) noexcept {
        count = 0;
        totalInputPower = totalOutputPower = 0.0;
        inputPeak = outputPeak = 0.0;
        inputClipped = outputClipped = 0;
        detectorReductionSum = detectorReductionMaximum = 0.0;
        detectorCount = 0;
        bucketSamples = 0;
        bucketInputPower = bucketOutputPower = 0.0;
        inputBuckets.fill(0.0);
        outputBuckets.fill(0.0);
        bucketPosition = 0;
        completedBuckets = 0;
        rollingInputPower = rollingOutputPower = 0.0;
        maximumBoost = maximumReduction = 0.0;
        bypassed = isBypassed;
        hasDetector = detector;
    }

    void process(const OptiLabAnalysisSectionFrame& frame) noexcept {
        const double inputLeft = frame.input.left;
        const double inputRight = frame.input.right;
        const double outputLeft = frame.output.left;
        const double outputRight = frame.output.right;
        const double inputPower = (inputLeft * inputLeft + inputRight * inputRight) * 0.5;
        const double outputPower = (outputLeft * outputLeft + outputRight * outputRight) * 0.5;
        ++count;
        totalInputPower += inputPower;
        totalOutputPower += outputPower;
        inputPeak = std::max(inputPeak, std::max(std::abs(inputLeft), std::abs(inputRight)));
        outputPeak = std::max(outputPeak, std::max(std::abs(outputLeft), std::abs(outputRight)));
        inputClipped +=
            std::abs(inputLeft) >= 1.0 || std::abs(inputRight) >= 1.0 ? 1u : 0u;
        outputClipped +=
            std::abs(outputLeft) >= 1.0 || std::abs(outputRight) >= 1.0 ? 1u : 0u;
        if (hasDetector) {
            const double reduction = gainReductionDb(frame.detectorGain);
            detectorReductionSum += reduction;
            detectorReductionMaximum = std::max(detectorReductionMaximum, reduction);
            ++detectorCount;
        }

        bucketInputPower += inputPower;
        bucketOutputPower += outputPower;
        ++bucketSamples;
        if (bucketSamples < hopSamples) return;
        const double inputMean = bucketInputPower / static_cast<double>(bucketSamples);
        const double outputMean = bucketOutputPower / static_cast<double>(bucketSamples);
        rollingInputPower -= inputBuckets[bucketPosition];
        rollingOutputPower -= outputBuckets[bucketPosition];
        inputBuckets[bucketPosition] = inputMean;
        outputBuckets[bucketPosition] = outputMean;
        rollingInputPower += inputMean;
        rollingOutputPower += outputMean;
        bucketPosition = (bucketPosition + 1) % inputBuckets.size();
        ++completedBuckets;
        bucketSamples = 0;
        bucketInputPower = bucketOutputPower = 0.0;
        if (completedBuckets < inputBuckets.size()) return;
        if (rollingInputPower <= kTiny && rollingOutputPower <= kTiny) return;
        const double change =
            powerDb(rollingOutputPower / inputBuckets.size())
            - powerDb(rollingInputPower / inputBuckets.size());
        maximumBoost = std::max(maximumBoost, change);
        maximumReduction = std::max(maximumReduction, -change);
    }

    OptiLabSectionAnalysis finish() const noexcept {
        OptiLabSectionAnalysis result;
        if (count == 0) return result;
        const double frames = static_cast<double>(count);
        const double inputMean = totalInputPower / frames;
        const double outputMean = totalOutputPower / frames;
        result.inputRmsDb = powerDb(inputMean);
        result.outputRmsDb = powerDb(outputMean);
        if (inputMean > kTiny || outputMean > kTiny) {
            result.averageLevelChangeDb = powerDb(outputMean) - powerDb(inputMean);
        }
        result.maximum400msBoostDb = maximumBoost;
        result.maximum400msReductionDb = maximumReduction;
        result.inputSamplePeakDb = amplitudeDb(inputPeak);
        result.outputSamplePeakDb = amplitudeDb(outputPeak);
        result.averageDetectorReductionDb =
            detectorCount
                ? detectorReductionSum / static_cast<double>(detectorCount) : 0.0;
        result.maximumDetectorReductionDb = detectorReductionMaximum;
        result.inputClippedFrames = inputClipped;
        result.outputClippedFrames = outputClipped;
        result.bypassed = bypassed;
        result.hasDetectorReduction = hasDetector;
        return result;
    }
};

} // namespace

struct OptiLabAnalysisMeter::Impl {
    double sampleRate = 48000.0;
    double maximumSeconds = 60.0;
    double requestedSeconds = 30.0;
    std::uint64_t frames = 0;
    std::uint64_t target = 0;
    bool active = false;
    EndpointState input;
    EndpointState output;
    std::array<SectionState, kOptiLabAnalysisSectionCount> sections{};
    std::array<bool, kOptiLabAnalysisSectionCount> bypass{};

    void buildReport(OptiLabAnalysisReport& result) const {
        result.requestedSeconds = requestedSeconds;
        result.measuredSeconds =
            sampleRate > 0.0 ? static_cast<double>(frames) / sampleRate : 0.0;
        result.sampleRate = sampleRate;
        result.measuredFrames = frames;
        result.input = input.finish();
        result.output = output.finish();
        for (std::size_t index = 0; index < sections.size(); ++index) {
            result.sections[index] = sections[index].finish();
        }
    }
};

OptiLabAnalysisMeter::OptiLabAnalysisMeter()
    : impl_(std::make_unique<Impl>()) {}

OptiLabAnalysisMeter::~OptiLabAnalysisMeter() = default;

void OptiLabAnalysisMeter::prepare(double sampleRate, double maximumSeconds) {
    impl_->sampleRate = std::max(1.0, sampleRate);
    impl_->maximumSeconds = std::clamp(maximumSeconds, 1.0, 120.0);
    impl_->input.prepare(impl_->sampleRate, impl_->maximumSeconds);
    impl_->output.prepare(impl_->sampleRate, impl_->maximumSeconds);
    for (auto& section : impl_->sections) section.prepare(impl_->sampleRate);
    impl_->active = false;
    impl_->frames = 0;
    impl_->target = 0;
}

void OptiLabAnalysisMeter::start(
    double durationSeconds,
    const std::array<bool, kOptiLabAnalysisSectionCount>& sectionBypass) noexcept {
    impl_->requestedSeconds =
        std::clamp(durationSeconds, 1.0, impl_->maximumSeconds);
    impl_->target = static_cast<std::uint64_t>(
        std::llround(impl_->requestedSeconds * impl_->sampleRate));
    impl_->target = std::max<std::uint64_t>(1, impl_->target);
    impl_->frames = 0;
    impl_->bypass = sectionBypass;
    impl_->input.reset();
    impl_->output.reset();
    for (std::size_t index = 0; index < impl_->sections.size(); ++index) {
        const bool detector = index == 1 || index == 3 || index == 5;
        impl_->sections[index].reset(sectionBypass[index], detector);
    }
    impl_->active = true;
}

void OptiLabAnalysisMeter::cancel() noexcept {
    impl_->active = false;
}

bool OptiLabAnalysisMeter::process(const OptiLabAnalysisFrame& frame) noexcept {
    if (!impl_->active) return false;
    impl_->input.process(frame.input.left, frame.input.right);
    impl_->output.process(frame.output.left, frame.output.right);
    for (std::size_t index = 0; index < impl_->sections.size(); ++index) {
        impl_->sections[index].process(frame.sections[index]);
    }
    ++impl_->frames;
    if (impl_->frames < impl_->target) return false;
    impl_->active = false;
    return true;
}

bool OptiLabAnalysisMeter::running() const noexcept {
    return impl_->active;
}

double OptiLabAnalysisMeter::progress() const noexcept {
    return impl_->target
        ? std::clamp(
              static_cast<double>(impl_->frames) / static_cast<double>(impl_->target),
              0.0, 1.0)
        : 0.0;
}

std::uint64_t OptiLabAnalysisMeter::framesProcessed() const noexcept {
    return impl_->frames;
}

std::uint64_t OptiLabAnalysisMeter::targetFrames() const noexcept {
    return impl_->target;
}

OptiLabAnalysisReport OptiLabAnalysisMeter::report() const {
    OptiLabAnalysisReport result;
    impl_->buildReport(result);
    return result;
}
