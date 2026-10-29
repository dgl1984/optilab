// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "AccessibleParameterSlider.h"

class OptiLabCoreAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit OptiLabCoreAudioProcessorEditor(OptiLabCoreAudioProcessor&);
    ~OptiLabCoreAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class CoreLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        CoreLookAndFeel();
        void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle, juce::Slider&) override;
        void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox&) override;
        void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    };
    class ModeComboBox final : public juce::ComboBox {
    public:
        bool keyPressed(const juce::KeyPress& key) override;
    };
    class ExactEntry final : public juce::TextEditor {
    public:
        std::function<void(bool)> finish;
        bool keyPressed(const juce::KeyPress& key) override;
        void focusLost(FocusChangeType cause) override;
    };
    void beginExactEntry(AccessibleParameterSlider& slider, ExactEntry& editor, bool integer);
    void finishExactEntry(AccessibleParameterSlider& slider, ExactEntry& editor, bool integer, bool apply);
    static juce::String inputText(double value);
    static juce::String adaptText(double value);

    OptiLabCoreAudioProcessor& processor_;
    CoreLookAndFeel lookAndFeel_;
    juce::Label title_, subtitle_;
    juce::Label modeLabel_, inputLabel_, adaptLabel_;
    ModeComboBox mode_;
    AccessibleParameterSlider input_;
    AccessibleParameterSlider adapt_;
    ExactEntry inputEntry_, adaptEntry_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> adaptAttachment_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptiLabCoreAudioProcessorEditor)
};
