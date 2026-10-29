// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../core/OptiLabCore.h"
#include <atomic>

class OptiLabCoreAudioProcessor final : public juce::AudioProcessor,
                                        private juce::AudioProcessorValueTreeState::Listener,
                                        private juce::Timer {
public:
    OptiLabCoreAudioProcessor();
    ~OptiLabCoreAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "OptiLab Core"; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return state_; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float modeStartInputDb(int mode) noexcept;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    OptiLabCore::Parameters currentCoreParameters() const noexcept;
    void updateCoreParametersIfNeeded();

    juce::AudioProcessorValueTreeState state_;
    std::atomic<float>* modeValue_ = nullptr;
    std::atomic<float>* inputValue_ = nullptr;
    std::atomic<float>* adaptValue_ = nullptr;
    OptiLabCore core_;
    bool coreReady_ = false; // Owned by the prepare/process lifecycle.
    // Only the audio thread owns these caches; other threads request invalidation.
    std::atomic<bool> invalidateCache_{false};
    int cachedMode_ = -1;
    float cachedInput_ = 1000.0f;
    float cachedAdapt_ = -1.0f;
    std::atomic<bool> restoringState_{false};
    std::atomic<bool> modeDefaultPending_{false};
    std::atomic<float> pendingModeDefaultDb_{0.0f};
};
