// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
constexpr const char* modeId = "mode";
constexpr const char* inputId = "inputDrive";
constexpr const char* adaptId = "autoAdapt";
}

OptiLabCoreAudioProcessor::OptiLabCoreAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state_(*this, nullptr, "OptiLabCoreState", createParameterLayout()) {
    modeValue_ = state_.getRawParameterValue(modeId);
    inputValue_ = state_.getRawParameterValue(inputId);
    adaptValue_ = state_.getRawParameterValue(adaptId);

    state_.addParameterListener(modeId, this);
    state_.addParameterListener(inputId, this);
    startTimerHz(30);
}

OptiLabCoreAudioProcessor::~OptiLabCoreAudioProcessor() {
    stopTimer();
    state_.removeParameterListener(modeId, this);
    state_.removeParameterListener(inputId, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout OptiLabCoreAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(modeId, 1), "Mode",
        juce::StringArray{"Stream polish", "Podcast Leveler", "Smooth Limiter"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(inputId, 1), "Input Drive",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(adaptId, 1), "Auto-Adapt",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    return layout;
}

float OptiLabCoreAudioProcessor::modeStartInputDb(int mode) noexcept {
    return mode == 1 ? 3.5f : 0.0f;
}

bool OptiLabCoreAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void OptiLabCoreAudioProcessor::prepareToPlay(double sampleRate, int) {
    coreReady_ = false;
    if (!OptiLabCoreEngine::isSampleRateSupported(sampleRate)) {
        setLatencySamples(0);
        return;
    }
    cachedMode_ = -1;
    cachedInput_ = 1000.0f;
    cachedAdapt_ = -1.0f;
    core_.setParameters(currentCoreParameters());
    core_.prepare(sampleRate);
    coreReady_ = true;
    updateCoreParametersIfNeeded();
    setLatencySamples(static_cast<int>(core_.latencySamples()));
}

void OptiLabCoreAudioProcessor::releaseResources() { coreReady_ = false; }

OptiLabCore::Parameters OptiLabCoreAudioProcessor::currentCoreParameters() const noexcept {
    const float rawMode = modeValue_->load();
    const int mode = static_cast<int>(OptiLabCore::sanitizeMode(std::isfinite(rawMode) && rawMode >= 0 && rawMode <= 2 ? static_cast<int>(std::lround(rawMode)) : -1));
    float input = inputValue_->load();
    if (modeDefaultPending_.load(std::memory_order_acquire))
        input = pendingModeDefaultDb_.load(std::memory_order_relaxed);
    OptiLabCore::Parameters p;
    p.mode = static_cast<OptiLabCore::Mode>(mode);
    p.inputDriveDb = juce::jlimit(-18.0f, 18.0f, input);
    p.autoAdaptPct = juce::jlimit(0.0f, 100.0f, adaptValue_->load());
    return p;
}

void OptiLabCoreAudioProcessor::updateCoreParametersIfNeeded() {
    if (invalidateCache_.exchange(false, std::memory_order_acq_rel)) cachedMode_ = -1;
    const auto p = currentCoreParameters();
    const int mode = static_cast<int>(p.mode);
    const float input = static_cast<float>(p.inputDriveDb);
    const float adapt = static_cast<float>(p.autoAdaptPct);
    if (mode == cachedMode_ && std::abs(input - cachedInput_) < 0.00001f &&
        std::abs(adapt - cachedAdapt_) < 0.00001f)
        return;
    core_.setParameters(p);
    // Publish the delay of the DSP that this block will actually use. The format
    // adapter performs its host-specific latency notification/restart handoff.
    const int latency = static_cast<int>(core_.latencySamples());
    if (latency != getLatencySamples()) setLatencySamples(latency);
    cachedMode_ = mode;
    cachedInput_ = input;
    cachedAdapt_ = adapt;
}

void OptiLabCoreAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (!coreReady_) {
        buffer.clear();
        return;
    }
    updateCoreParametersIfNeeded();
    const int channels = buffer.getNumChannels();
    const int frames = buffer.getNumSamples();
    if (channels >= 2) {
        core_.processPlanar(buffer.getWritePointer(0), buffer.getWritePointer(1), static_cast<std::size_t>(frames));
        for (int ch = 2; ch < channels; ++ch)
            buffer.clear(ch, 0, frames);
    } else if (channels == 1) {
        auto* mono = buffer.getWritePointer(0);
        for (int i = 0; i < frames; ++i)
            mono[i] = core_.processSample(mono[i], mono[i]).first;
    }
}

void OptiLabCoreAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (!coreReady_) {
        buffer.clear();
        return;
    }
    updateCoreParametersIfNeeded();
    const int channels = buffer.getNumChannels();
    const int frames = buffer.getNumSamples();
    for (int i = 0; i < frames; ++i) {
        const float left = channels > 0 ? static_cast<float>(buffer.getSample(0, i)) : 0.0f;
        const float right = channels > 1 ? static_cast<float>(buffer.getSample(1, i)) : left;
        const auto out = core_.processSample(left, right);
        if (channels > 0) buffer.setSample(0, i, out.first);
        if (channels > 1) buffer.setSample(1, i, out.second);
        for (int ch = 2; ch < channels; ++ch) buffer.setSample(ch, i, 0.0);
    }
}

void OptiLabCoreAudioProcessor::parameterChanged(const juce::String& id, float value) {
    const bool restoring = restoringState_.load(std::memory_order_acquire);
    if (id == modeId && !restoring) {
        const int mode = std::isfinite(value) && value >= 0 && value <= 2
            ? static_cast<int>(std::lround(value)) : 0;
        pendingModeDefaultDb_.store(modeStartInputDb(mode), std::memory_order_relaxed);
        modeDefaultPending_.store(true, std::memory_order_release);
    } else if (id == inputId && !restoring) {

        modeDefaultPending_.store(false, std::memory_order_release);
    }
}

void OptiLabCoreAudioProcessor::timerCallback() {
    if (modeDefaultPending_.exchange(false, std::memory_order_acq_rel)) {
        const float target = pendingModeDefaultDb_.load(std::memory_order_relaxed);
        if (auto* parameter = state_.getParameter(inputId))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(target));
        invalidateCache_.store(true, std::memory_order_release);
    }
}

void OptiLabCoreAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto snapshot = state_.copyState();
    if (modeDefaultPending_.load(std::memory_order_acquire)) {
        const float pendingInput = pendingModeDefaultDb_.load(std::memory_order_relaxed);
        for (auto child : snapshot) {
            if (child.getProperty("id").toString() == inputId) {
                child.setProperty("value", pendingInput, nullptr);
                break;
            }
        }
    }
    if (auto xml = snapshot.createXml())
        copyXmlToBinary(*xml, destData);
}

void OptiLabCoreAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml || !xml->hasTagName(state_.state.getType()))
        return;
    auto restored = juce::ValueTree::fromXml(*xml);
    for (const auto* id : {modeId, inputId, adaptId}) {
        auto child = restored.getChildWithProperty("id", id);
        if (!child.isValid()) {
            child = juce::ValueTree("PARAM");
            child.setProperty("id", id, nullptr);
            restored.addChild(child, -1, nullptr);
        }
        float value = static_cast<float>(child.getProperty("value", 0.0f));
        if (!std::isfinite(value)) value = 0.0f;
        if (juce::String(id) == modeId)
            value = value >= 0 && value <= 2 && value == std::floor(value) ? value : 0.0f;
        else if (juce::String(id) == inputId) value = juce::jlimit(-18.0f, 18.0f, value);
        else value = juce::jlimit(0.0f, 100.0f, value);
        child.setProperty("value", value, nullptr);
    }
    restoringState_.store(true, std::memory_order_release);
    state_.replaceState(restored);
    restoringState_.store(false, std::memory_order_release);
    modeDefaultPending_.store(false, std::memory_order_release);
    invalidateCache_.store(true, std::memory_order_release);
    // The CLAP adapter translates this into a parameter-values rescan for the host.
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true));
}

juce::AudioProcessorEditor* OptiLabCoreAudioProcessor::createEditor() {
    return new OptiLabCoreAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OptiLabCoreAudioProcessor();
}
