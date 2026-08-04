// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.
#include <algorithm>
#include <array>
#include "OptiLabCore.h"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool testModeDefaults() {
    const auto podcast = OptiLabCore::defaultParameters(OptiLabCore::Mode::PodcastLeveler);
    const auto stream = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
    const auto limiter = OptiLabCore::defaultParameters(OptiLabCore::Mode::SmoothLimiter);

    return expect(podcast.inputDriveDb == 3.5, "podcast input default") &&
           expect(stream.inputDriveDb == 0.0, "stream input default") &&
           expect(limiter.inputDriveDb == 0.0, "limiter input default") &&
           expect(podcast.autoAdaptPct == 0.0 && stream.autoAdaptPct == 0.0 &&
                      limiter.autoAdaptPct == 0.0,
                  "adapt defaults");
}

bool testPlanarMatchesInterleaved() {
    constexpr std::size_t frames = 2048;
    std::vector<float> left(frames);
    std::vector<float> right(frames);
    std::vector<float> interleaved(frames * 2);

    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        left[i] = static_cast<float>(0.45 * std::sin(2.0 * 3.14159265358979323846 * 173.0 * t));
        right[i] = static_cast<float>(0.37 * std::cos(2.0 * 3.14159265358979323846 * 311.0 * t));
        interleaved[i * 2] = left[i];
        interleaved[i * 2 + 1] = right[i];
    }

    OptiLabCore planarCore;
    OptiLabCore interleavedCore;
    const auto parameters = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
    planarCore.prepare(48000.0);
    interleavedCore.prepare(48000.0);
    planarCore.setParameters(parameters);
    interleavedCore.setParameters(parameters);

    planarCore.processPlanar(left.data(), right.data(), frames);
    interleavedCore.processInterleaved(interleaved.data(), frames, 2);

    for (std::size_t i = 0; i < frames; ++i) {
        if (left[i] != interleaved[i * 2] || right[i] != interleaved[i * 2 + 1]) {
            return expect(false, "planar output must exactly match interleaved output");
        }
    }
    return true;
}

std::pair<double, double> processStreamTone(double adaptPct) {
    constexpr std::size_t frames = 96000;
    constexpr std::size_t skip = 12000;
    std::vector<float> samples(frames * 2);

    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        const double left =
            0.25 * std::sin(2.0 * 3.14159265358979323846 * 93.0 * t) +
            0.18 * std::sin(2.0 * 3.14159265358979323846 * 271.0 * t) +
            0.12 * std::sin(2.0 * 3.14159265358979323846 * 1420.0 * t);
        const double right =
            0.23 * std::sin(2.0 * 3.14159265358979323846 * 111.0 * t) +
            0.17 * std::sin(2.0 * 3.14159265358979323846 * 329.0 * t) +
            0.10 * std::sin(2.0 * 3.14159265358979323846 * 1880.0 * t);
        samples[i * 2] = static_cast<float>(left);
        samples[i * 2 + 1] = static_cast<float>(right);
    }

    OptiLabCore core;
    auto parameters = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
    parameters.autoAdaptPct = adaptPct;
    core.prepare(48000.0);
    core.setParameters(parameters);
    core.processInterleaved(samples.data(), frames, 2);

    double peak = 0.0;
    double sumSquares = 0.0;
    std::size_t count = 0;
    for (std::size_t i = skip * 2; i < samples.size(); ++i) {
        const double value = samples[i];
        peak = std::max(peak, std::abs(value));
        sumSquares += value * value;
        ++count;
    }
    return {peak, std::sqrt(sumSquares / static_cast<double>(count))};
}

bool testStreamAdaptKeepsLoudnessHeadroom() {
    const auto base = processStreamTone(0.0);
    const auto adapted = processStreamTone(100.0);
    const double basePeak = base.first;
    const double baseRms = base.second;
    const double adaptedPeak = adapted.first;
    const double adaptedRms = adapted.second;
    const double rmsDropDb = 20.0 * std::log10(std::max(adaptedRms, 0.0000001) /
                                               std::max(baseRms, 0.0000001));
    const double baseCrest = 20.0 * std::log10(std::max(basePeak, 0.0000001) /
                                               std::max(baseRms, 0.0000001));
    const double adaptedCrest = 20.0 * std::log10(std::max(adaptedPeak, 0.0000001) /
                                                  std::max(adaptedRms, 0.0000001));

    return expect(rmsDropDb >= -0.75, "stream adapt must not get materially quieter") &&
           expect(adaptedCrest <= baseCrest + 1.25,
                  "stream adapt must not trade loudness for extra crest against the ceiling");
}

bool testDeliveryCeilingAcrossSampleRates() {
    constexpr std::array<double, 4> sampleRates{44100.0, 48000.0, 88200.0, 96000.0};
    const float ceiling = std::nextafter(
        static_cast<float>(std::pow(10.0, -0.1 / 20.0)), 0.0f);

    for (const double sampleRate : sampleRates) {
        OptiLabCore core;
        auto parameters = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
        parameters.inputDriveDb = 18.0;
        parameters.autoAdaptPct = 100.0;
        core.setParameters(parameters);
        core.prepare(sampleRate);

        const std::size_t frames = static_cast<std::size_t>(sampleRate * 6.0);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const double t = static_cast<double>(frame) / sampleRate;
            const double pulse = frame % 997 < 8 ? 0.95 : 0.0;
            const float left = static_cast<float>(
                0.82 * std::sin(2.0 * 3.14159265358979323846 * 47.0 * t) +
                0.61 * std::sin(2.0 * 3.14159265358979323846 * 173.0 * t) + pulse);
            const float right = static_cast<float>(
                0.79 * std::cos(2.0 * 3.14159265358979323846 * 53.0 * t) +
                0.58 * std::sin(2.0 * 3.14159265358979323846 * 281.0 * t) - pulse);
            const auto output = core.processSample(left, right);
            if (!std::isfinite(output.first) || !std::isfinite(output.second) ||
                std::abs(output.first) > ceiling || std::abs(output.second) > ceiling) {
                return expect(false,
                              "native delivery must remain finite and within -0.1 dBFS");
            }
        }
    }
    return true;
}

bool testInvalidSampleRateFallsBackTo48k() {
    OptiLabCore invalidRateCore;
    OptiLabCore referenceCore;
    auto parameters = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
    parameters.autoAdaptPct = 100.0;
    invalidRateCore.setParameters(parameters);
    referenceCore.setParameters(parameters);
    invalidRateCore.prepare(std::numeric_limits<double>::quiet_NaN());
    referenceCore.prepare(48000.0);

    for (std::size_t frame = 0; frame < 4096; ++frame) {
        const double t = static_cast<double>(frame) / 48000.0;
        const float left = static_cast<float>(0.55 * std::sin(
            2.0 * 3.14159265358979323846 * 233.0 * t));
        const float right = static_cast<float>(0.47 * std::cos(
            2.0 * 3.14159265358979323846 * 389.0 * t));
        if (invalidRateCore.processSample(left, right) !=
            referenceCore.processSample(left, right)) {
            return expect(false, "invalid sample rate must use the 48 kHz fallback");
        }
    }
    return true;
}

} // namespace

int main() {
    const bool passed = testModeDefaults() && testPlanarMatchesInterleaved() &&
                        testStreamAdaptKeepsLoudnessHeadroom() &&
                        testDeliveryCeilingAcrossSampleRates() &&
                        testInvalidSampleRateFallsBackTo48k();
    if (passed) {
        std::cout << "OptiLab Core API tests passed.\n";
        return 0;
    }
    return 1;
}
