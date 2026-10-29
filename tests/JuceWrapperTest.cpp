#include "PluginProcessor.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <algorithm>
#include <stdexcept>

namespace {
struct RestoreListener final : juce::AudioProcessorListener {
    bool valuesChanged = false;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override {
        valuesChanged = valuesChanged || details.programChanged;
    }
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Exercise the editor attachment, host parameter, processor cache, and live DSP
// together. Direct Core parameter tests cannot detect a disconnected UI slider.
void testLiveModeLatency() {
    struct LatencyListener final : juce::AudioProcessorListener {
        int changes = 0;
        void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override {
            if (details.latencyChanged) ++changes;
        }
    } listener;
    OptiLabCoreAudioProcessor processor;
    processor.addListener(&listener);
    processor.setRateAndBufferSizeDetails(48000.0, 64);
    processor.prepareToPlay(48000.0, 64);
    auto& state = processor.state();
    auto* mode = state.getParameter("mode");
    auto* adapt = state.getParameter("autoAdapt");
    juce::AudioBuffer<float> block(2, 64);
    juce::MidiBuffer midi;
    // Deliberately do not pump the message loop: offline and busy-host audio
    // processing must report its active delay without an asynchronous probe.
    for (int selected : {1, 2, 0, 2, 1, 0}) {
        mode->setValueNotifyingHost(mode->convertTo0to1(float(selected)));
        for (float amount : {0.0f, 50.0f, 100.0f}) {
            const int changesBefore = listener.changes;
            adapt->setValueNotifyingHost(adapt->convertTo0to1(amount));
            block.clear();
            processor.processBlock(block, midi);
            const int expected = selected == 0 ? 648 : selected == 1 ? 301 : 1033;
            require(processor.getLatencySamples() == expected,
                    "Host latency does not match active mode before message dispatch");
            if (amount != 0.0f)
                require(listener.changes == changesBefore, "Auto-Adapt unnecessarily notified host latency");
        }
    }
    processor.removeListener(&listener);
}

void testLiveAdaptSlider() {
    constexpr double rate = 48000.0;
    constexpr int blockSize = 512;
    for (int modeIndex : {0, 1, 2}) {
        OptiLabCoreAudioProcessor processor;
        auto& state = processor.state();
        auto* mode = state.getParameter("mode");
        auto* input = state.getParameter("inputDrive");
        mode->setValueNotifyingHost(mode->convertTo0to1(float(modeIndex)));
        const float drive = OptiLabCoreAudioProcessor::modeStartInputDb(modeIndex);
        input->setValueNotifyingHost(input->convertTo0to1(drive));
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        juce::Slider* slider = nullptr;
        for (auto* child : editor->getChildren())
            if (auto* candidate = dynamic_cast<juce::Slider*>(child))
                if (candidate->getName() == "Auto-Adapt") slider = candidate;
        require(slider != nullptr, "Auto-Adapt editor slider was not found");
        processor.prepareToPlay(rate, blockSize);
        OptiLabCore expected, fixedZero;
        auto parameters = OptiLabCore::defaultParameters(static_cast<OptiLabCore::Mode>(modeIndex));
        expected.setParameters(parameters); expected.prepare(rate);
        fixedZero.setParameters(parameters); fixedZero.prepare(rate);
        juce::AudioBuffer<float> block(2, blockSize);
        juce::MidiBuffer midi;
        int position = 0;
        double maximumError = 0, difference = 0, baselineEnergy = 0;
        for (double adapt : {0.0, 50.0, 100.0, 0.0}) {
            slider->setValue(adapt, juce::sendNotificationSync);
            require(std::abs(state.getRawParameterValue("autoAdapt")->load() - adapt) < 1.0e-6,
                    "Editor Auto-Adapt did not reach the host parameter");
            parameters.autoAdaptPct = adapt;
            expected.setParameters(parameters);
            for (int b = 0; b < 750; ++b) {
                float reference[2][blockSize], baseline[2][blockSize];
                for (int i = 0; i < blockSize; ++i, ++position) {
                    const double t = position / rate;
                    const double envelope = .12 + .10 * (1 + std::sin(t * 2.3));
                    const float l = float(envelope * (std::sin(t * 2 * 3.141592653589793 * 173)
                        + .3 * std::sin(t * 2 * 3.141592653589793 * 3700)));
                    const float r = float(.9 * l + .03 * std::sin(t * 2 * 3.141592653589793 * 83));
                    block.setSample(0, i, l); block.setSample(1, i, r);
                    const auto a = expected.processSample(l, r), z = fixedZero.processSample(l, r);
                    reference[0][i] = a.first; reference[1][i] = a.second;
                    baseline[0][i] = z.first; baseline[1][i] = z.second;
                }
                processor.processBlock(block, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < blockSize; ++i) {
                        const double actual = block.getSample(ch, i);
                        require(std::isfinite(actual), "Live Auto-Adapt emitted nonfinite output");
                        maximumError = std::max(maximumError, std::abs(actual - reference[ch][i]));
                        if (adapt == 100 && b >= 375) {
                            const double d = actual - baseline[ch][i];
                            difference += d * d;
                            baselineEnergy += double(baseline[ch][i]) * baseline[ch][i];
                        }
                    }
            }
        }
        const double relativeDifference = std::sqrt(difference / std::max(baselineEnergy, 1.0e-30));
        std::cout << "Live editor Auto-Adapt mode=" << modeIndex << " max Core error=" << maximumError
                  << " difference from fixed 0%=" << relativeDifference << '\n';
        require(maximumError < 1.0e-6, "Live editor Auto-Adapt audio disagreed with shared Core");
        require(relativeDifference > .01, "Auto-Adapt 100% audio did not differ from fixed 0%");
    }
}
}

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        testLiveAdaptSlider();
        testLiveModeLatency();
        OptiLabCoreAudioProcessor processor;
        auto& state = processor.state();
        auto* mode = state.getParameter("mode");
        auto* input = state.getParameter("inputDrive");
        auto* adapt = state.getParameter("autoAdapt");
        require(mode && input && adapt, "Expected three host parameters were not enrolled");
        require(processor.getParameters().size() == 3, "Unexpected JUCE host parameter count");
        require(std::abs(OptiLabCoreAudioProcessor::modeStartInputDb(0) - 0.0f) < 1.0e-6f,
                "Stream start input changed");
        require(std::abs(OptiLabCoreAudioProcessor::modeStartInputDb(1) - 3.5f) < 1.0e-6f,
                "Podcast start input changed");
        require(std::abs(OptiLabCoreAudioProcessor::modeStartInputDb(2) - 0.0f) < 1.0e-6f,
                "Smooth start input changed");

        require(std::abs(state.getRawParameterValue("mode")->load() - 0.0f) < 1.0e-6f,
                "Fresh JUCE instance is not Stream");
        require(std::abs(state.getRawParameterValue("inputDrive")->load() - 0.0f) < 1.0e-6f,
                "Fresh JUCE instance input is not 0 dB");
        require(std::abs(state.getRawParameterValue("autoAdapt")->load() - 0.0f) < 1.0e-6f,
                "Fresh JUCE instance Auto-Adapt is not 0%");

        processor.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 2048);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const float x = static_cast<float>(0.3 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / 48000.0));
            buffer.setSample(0, i, x);
            buffer.setSample(1, i, -0.7f*x);
        }
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                require(std::isfinite(buffer.getSample(ch, i)), "JUCE processBlock emitted non-finite sample");

        juce::MemoryBlock saved;
        input->setValueNotifyingHost(input->convertTo0to1(6.0f));
        adapt->setValueNotifyingHost(adapt->convertTo0to1(55.0f));
        processor.getStateInformation(saved);

        OptiLabCoreAudioProcessor restored;
        RestoreListener restoreListener;
        restored.addListener(&restoreListener);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        require(restoreListener.valuesChanged, "State restore did not request host parameter refresh");
        restored.removeListener(&restoreListener);
        require(std::abs(restored.state().getRawParameterValue("inputDrive")->load() - 6.0f) < 0.11f,
                "JUCE state round-trip lost Input Drive");
        require(std::abs(restored.state().getRawParameterValue("autoAdapt")->load() - 55.0f) < 0.51f,
                "JUCE state round-trip lost Auto-Adapt");

        juce::MemoryBlock resaved;
        restored.getStateInformation(resaved);
        require(saved == resaved, "State resave changed serialized bytes");

        OptiLabCoreAudioProcessor rateProbe;
        for (double rate : {1000.0, 7999.0, 768001.0,
                            std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()}) {
            rateProbe.prepareToPlay(rate, 64);
            require(rateProbe.getLatencySamples() == 0, "Unsupported rate retained latency");
            juce::AudioBuffer<float> f(2, 64);
            juce::AudioBuffer<double> d(2, 64);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 64; ++i) { f.setSample(ch, i, 0.5f); d.setSample(ch, i, 0.5); }
            rateProbe.processBlock(f, midi);
            rateProbe.processBlock(d, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 64; ++i)
                    require(f.getSample(ch, i) == 0 && d.getSample(ch, i) == 0,
                            "Unsupported rate did not silence output");
        }
        rateProbe.prepareToPlay(48000, 2048);
        require(rateProbe.getLatencySamples() > 0, "Supported rate did not recover latency");
        juce::AudioBuffer<float> recovered(2, 2048);
        for (int i = 0; i < 2048; ++i) {
            const auto x = float(0.2 * std::sin(i * 0.13));
            recovered.setSample(0, i, x); recovered.setSample(1, i, -x);
        }
        rateProbe.processBlock(recovered, midi);
        bool nonzero = false;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 2048; ++i) {
                require(std::isfinite(recovered.getSample(ch, i)), "Rate recovery emitted non-finite audio");
                nonzero = nonzero || recovered.getSample(ch, i) != 0;
            }
        require(nonzero, "Supported rate stayed silent after recovery");

        std::cout << "OptiLab JUCE wrapper regression: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "OptiLab JUCE wrapper regression: FAIL: " << e.what() << '\n';
        return 1;
    }
}
