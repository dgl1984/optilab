#include "OptiLabCore.h"
#include "OptiLabCoreEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> render(OptiLabCore& core, const std::vector<float>& input) {
    std::vector<float> output(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto frame = core.processSample(input[i], input[i]);
        output[i] = frame.first;
        require(std::isfinite(frame.first) && std::isfinite(frame.second), "Non-finite Core output");
    }
    return output;
}

void testDefaultsAndSanitizing() {
    require(OptiLabCore::sanitizeMode(-1) == OptiLabCore::Mode::StreamPolish, "Invalid mode must sanitize to Stream");
    require(OptiLabCore::sanitizeMode(99) == OptiLabCore::Mode::StreamPolish, "High invalid mode must sanitize to Stream");
    require(OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish).inputDriveDb == 0.0, "Stream default input changed");
    require(OptiLabCore::defaultParameters(OptiLabCore::Mode::PodcastLeveler).inputDriveDb == 3.5, "Podcast default input changed");
    require(OptiLabCore::defaultParameters(OptiLabCore::Mode::SmoothLimiter).inputDriveDb == 0.0, "Smooth default input changed");
}


void testCoreEngineInputDriveContract() {
    using Mode = OptiLabCoreEngine::Mode;
    require(OptiLabCoreEngine::defaultParameters(Mode::StreamPolish).inputDriveDb == 0.0,
            "Core engine Stream default input disagrees with Core");
    require(OptiLabCoreEngine::defaultParameters(Mode::PodcastLeveler).inputDriveDb == 3.5,
            "Core engine Podcast default input disagrees with Core");
    require(OptiLabCoreEngine::defaultParameters(Mode::SmoothLimiter).inputDriveDb == 0.0,
            "Core engine Smooth default input disagrees with Core");

    OptiLabCoreEngine core;
    core.prepare(48000.0);
    core.setParameters(OptiLabCoreEngine::defaultParameters(Mode::StreamPolish));
    core.setParameterValue("p002", 6.0);
    require(std::abs(core.parameters().inputDriveDb - 6.0) < 1.0e-12,
            "p002 write did not update Input Drive");
    require(core.setParameterValueRealtime("p002", -20.0),
            "Realtime p002 write was rejected");
    require(std::abs(core.parameters().inputDriveDb + 18.0) < 1.0e-12,
            "p002 range did not clamp to -18 dB");

    const auto& info = OptiLabCoreEngine::parameterInfo();
    const auto it = std::find_if(info.begin(), info.end(), [](const auto& item) {
        return std::string_view(item.key) == "p002";
    });
    require(it != info.end(), "p002 missing from parameter catalog");
    require(it->minimum == -18.0 && it->maximum == 18.0,
            "p002 catalog range disagrees with Input Drive contract");
}

void testUnsupportedSampleRateRejects() {
    OptiLabCore core;
    bool threw = false;
    try { core.prepare(1000.0); }
    catch (const std::invalid_argument&) { threw = true; }
    require(threw, "Unsupported sample rate must be rejected instead of silently clamped");
}

void testFiniteProcessingAndFixedLatency() {
    constexpr double sampleRate = 48000.0;
    std::array<OptiLabCore::Mode, 3> modes{
        OptiLabCore::Mode::StreamPolish,
        OptiLabCore::Mode::PodcastLeveler,
        OptiLabCore::Mode::SmoothLimiter};

    for (const auto mode : modes) {
        OptiLabCore core;
        auto p = OptiLabCore::defaultParameters(mode);
        core.setParameters(p);
        core.prepare(sampleRate);
        const auto expectedLatency = core.latencySamples();

        for (double adapt : {0.0, 50.0, 100.0}) {
            p.autoAdaptPct = adapt;
            core.setParameters(p);
            require(core.latencySamples() == expectedLatency, "Per-mode latency changed with Auto-Adapt");
            for (int i = 0; i < 4096; ++i) {
                const float x = static_cast<float>(0.35 * std::sin(2.0 * 3.141592653589793 * 997.0 * i / sampleRate));
                const auto y = core.processSample(x, -x * 0.75f);
                require(std::isfinite(y.first) && std::isfinite(y.second), "Non-finite Core sample");
            }
        }
    }
}

void testResetDeterminism() {
    OptiLabCore core;
    OptiLabCore::Parameters p;
    p.mode = OptiLabCore::Mode::StreamPolish;
    p.inputDriveDb = 2.0;
    p.autoAdaptPct = 65.0;
    core.setParameters(p);
    core.prepare(48000.0);

    std::vector<float> input(2048);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<float>(0.2 * std::sin(2.0 * 3.141592653589793 * 431.0 * static_cast<double>(i) / 48000.0));

    const auto first = render(core, input);
    core.reset();
    const auto second = render(core, input);
    require(first.size() == second.size(), "Reset render size mismatch");
    for (std::size_t i = 0; i < first.size(); ++i)
        require(std::abs(first[i] - second[i]) < 1.0e-7f, "Reset did not restore deterministic state");
}

void testParameterSanitizing() {
    OptiLabCore core;
    core.prepare(48000.0);
    OptiLabCore::Parameters p;
    p.mode = static_cast<OptiLabCore::Mode>(99);
    p.inputDriveDb = std::numeric_limits<double>::quiet_NaN();
    p.autoAdaptPct = 1000.0;
    core.setParameters(p);
    const auto clean = core.parameters();
    require(clean.mode == OptiLabCore::Mode::StreamPolish, "Mode sanitizing failed");
    require(clean.inputDriveDb == 0.0, "Non-finite input sanitizing failed");
    require(clean.autoAdaptPct == 100.0, "Auto-Adapt range sanitizing failed");
}

} // namespace

int main() {
    try {
        testDefaultsAndSanitizing();
        testCoreEngineInputDriveContract();
        testUnsupportedSampleRateRejects();
        testFiniteProcessingAndFixedLatency();
        testResetDeterminism();
        testParameterSanitizing();
        std::cout << "OptiLab Core public regression: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "OptiLab Core public regression: FAIL: " << e.what() << '\n';
        return 1;
    }
}
