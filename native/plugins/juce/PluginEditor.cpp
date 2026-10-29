// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#include "PluginEditor.h"
#include <cmath>
#include <cerrno>
#include <cstdlib>

namespace {
bool parseStrictDouble(const juce::String& text, double& value) {
    auto normalized = text.trim();
    if (normalized.isEmpty()) return false;
    if (normalized.containsChar(',') && !normalized.containsChar('.'))
        normalized = normalized.replaceCharacter(',', '.');
    const auto utf8 = normalized.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(utf8, &end);
    if (end == utf8 || end == nullptr || errno == ERANGE || !std::isfinite(parsed))
        return false;
    const auto suffix = juce::String::fromUTF8(end).trim();
    if (suffix.isNotEmpty() && suffix.compareIgnoreCase("dB") != 0
        && suffix != "%" && suffix.compareIgnoreCase("percent") != 0) return false;
    value = parsed;
    return true;
}
}

OptiLabCoreAudioProcessorEditor::CoreLookAndFeel::CoreLookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(29, 33, 38));
    setColour(juce::ComboBox::textColourId, juce::Colour(239, 242, 245));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(135, 205, 255));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(61, 68, 76));
    setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(135, 205, 255));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(24, 28, 32));
    setColour(juce::PopupMenu::textColourId, juce::Colour(239, 242, 245));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(43, 91, 132));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(31, 35, 40));
    setColour(juce::TextEditor::textColourId, juce::Colour(239, 242, 245));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(83, 92, 102));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(135, 205, 255));
    setColour(juce::TextEditor::highlightColourId, juce::Colour(55, 112, 170));
    setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
}

void OptiLabCoreAudioProcessorEditor::CoreLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float, float, const juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style != juce::Slider::LinearHorizontal) {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                          static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
    if (slider.hasKeyboardFocus(true)) {
        g.setColour(juce::Colour(135, 205, 255));
        g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
    }

    auto content = bounds.reduced(10.0f, 5.0f);
    auto valueArea = juce::Rectangle<float>(content.getRight() - 104.0f, content.getY(), 104.0f, 22.0f);
    auto track = juce::Rectangle<float>(content.getX(), content.getBottom() - 12.0f,
                                         content.getWidth(), 8.0f);

    g.setColour(juce::Colour(45, 50, 56));
    g.fillRoundedRectangle(track, 4.0f);
    const float clamped = juce::jlimit(track.getX(), track.getRight(), sliderPos);
    if (clamped > track.getX()) {
        auto active = track.withWidth(clamped - track.getX());
        g.setColour(juce::Colour(72, 157, 230));
        g.fillRoundedRectangle(active, 4.0f);
    }

    g.setColour(juce::Colour(228, 233, 238));
    g.fillEllipse(clamped - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour(juce::Colour(82, 91, 101));
    g.drawEllipse(clamped - 7.0f, track.getCentreY() - 7.0f, 14.0f, 14.0f, 1.0f);

    g.setColour(juce::Colour(28, 33, 38));
    g.fillRoundedRectangle(valueArea, 6.0f);
    g.setColour(juce::Colour(55, 63, 71));
    g.drawRoundedRectangle(valueArea.reduced(0.5f), 6.0f, 1.0f);
    g.setColour(juce::Colour(239, 242, 245));
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    const auto text = slider.textFromValueFunction
        ? slider.textFromValueFunction(slider.getValue())
        : juce::String(slider.getValue(), 1);
    g.drawText(text, valueArea.toNearestInt(), juce::Justification::centred, false);
}

void OptiLabCoreAudioProcessorEditor::CoreLookAndFeel::drawComboBox(
    juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds.reduced(1.0f), 7.0f);
    g.setColour(box.hasKeyboardFocus(true) ? box.findColour(juce::ComboBox::focusedOutlineColourId)
                                           : box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 7.0f, box.hasKeyboardFocus(true) ? 2.0f : 1.0f);

    const float arrowX = static_cast<float>(width - 25);
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(arrowX - 5.0f, cy - 2.0f);
    arrow.lineTo(arrowX, cy + 3.0f);
    arrow.lineTo(arrowX + 5.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OptiLabCoreAudioProcessorEditor::CoreLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(14, 1, box.getWidth() - 46, box.getHeight() - 2);
    label.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
}

bool OptiLabCoreAudioProcessorEditor::ModeComboBox::keyPressed(const juce::KeyPress& key) {
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::homeKey) {
        setSelectedItemIndex(0, juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::endKey) {
        setSelectedItemIndex(getNumItems() - 1, juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::pageUpKey) {
        setSelectedItemIndex(juce::jmin(getNumItems() - 1, getSelectedItemIndex() + 1), juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::pageDownKey) {
        setSelectedItemIndex(juce::jmax(0, getSelectedItemIndex() - 1), juce::sendNotificationSync);
        return true;
    }
    return juce::ComboBox::keyPressed(key);
}

bool OptiLabCoreAudioProcessorEditor::ExactEntry::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::returnKey) {
        if (finish) finish(true);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey) {
        if (finish) finish(false);
        return true;
    }
    return juce::TextEditor::keyPressed(key);
}

void OptiLabCoreAudioProcessorEditor::ExactEntry::focusLost(FocusChangeType cause) {
    juce::TextEditor::focusLost(cause);
    if (isVisible() && finish) finish(false);
}

juce::String OptiLabCoreAudioProcessorEditor::inputText(double value) {
    return juce::String(value, 1) + " dB";
}
juce::String OptiLabCoreAudioProcessorEditor::adaptText(double value) {
    return juce::String(static_cast<int>(std::lround(value))) + " percent";
}

OptiLabCoreAudioProcessorEditor::OptiLabCoreAudioProcessorEditor(OptiLabCoreAudioProcessor& processor)
    : AudioProcessorEditor(processor), processor_(processor),
      input_("Input Drive", -18.0, 18.0, 0.1, 1.0, inputText),
      adapt_("Auto-Adapt", 0.0, 100.0, 1.0, 10.0, adaptText) {
    setLookAndFeel(&lookAndFeel_);
    setAccessible(true);
    setTitle("OptiLab Core editor");
    setDescription("OptiLab Core controls");
    setResizable(true, true);
    setResizeLimits(500, 350, 980, 560);
    setSize(620, 350);

    title_.setText("OptiLab Core", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    title_.setColour(juce::Label::textColourId, juce::Colour(242, 244, 247));
    title_.setAccessible(false);
    addAndMakeVisible(title_);

    subtitle_.setText("Three controls. Complete processing.", juce::dontSendNotification);
    subtitle_.setFont(juce::Font(juce::FontOptions(13.0f)));
    subtitle_.setColour(juce::Label::textColourId, juce::Colour(151, 162, 173));
    subtitle_.setAccessible(false);
    addAndMakeVisible(subtitle_);

    modeLabel_.setText("Mode", juce::dontSendNotification);
    inputLabel_.setText("Input Drive", juce::dontSendNotification);
    adaptLabel_.setText("Auto-Adapt", juce::dontSendNotification);
    for (auto* label : {&modeLabel_, &inputLabel_, &adaptLabel_}) {
        label->setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, juce::Colour(222, 227, 232));
        label->setAccessible(false);
        addAndMakeVisible(*label);
    }

    mode_.addItem("Stream polish", 1);
    mode_.addItem("Podcast Leveler", 2);
    mode_.addItem("Smooth Limiter", 3);
    mode_.setName("Mode");
    mode_.setTitle("Mode");
    mode_.setDescription("Selects the complete OptiLab Core processing mode");
    mode_.setHelpText("Arrow keys change Mode. Page Up and Page Down also move through choices. Home selects Stream polish and End selects Smooth Limiter.");
    mode_.setWantsKeyboardFocus(true);
    mode_.setExplicitFocusOrder(1);
    addAndMakeVisible(mode_);

    input_.setExplicitFocusOrder(2);
    adapt_.setExplicitFocusOrder(3);
    addAndMakeVisible(input_);
    addAndMakeVisible(adapt_);

    inputEntry_.setName("Exact Input Drive value");
    inputEntry_.setTitle("Exact Input Drive value");
    inputEntry_.setDescription("Type a value from minus 18 to plus 18 dB and press Enter. Escape cancels.");
    inputEntry_.setVisible(false);
    addChildComponent(inputEntry_);
    adaptEntry_.setName("Exact Auto-Adapt value");
    adaptEntry_.setTitle("Exact Auto-Adapt value");
    adaptEntry_.setDescription("Type an integer from 0 to 100 and press Enter. Escape cancels.");
    adaptEntry_.setVisible(false);
    addChildComponent(adaptEntry_);

    input_.onRequestExactEntry = [this] { beginExactEntry(input_, inputEntry_, false); };
    adapt_.onRequestExactEntry = [this] { beginExactEntry(adapt_, adaptEntry_, true); };
    inputEntry_.finish = [this](bool apply) { finishExactEntry(input_, inputEntry_, false, apply); };
    adaptEntry_.finish = [this](bool apply) { finishExactEntry(adapt_, adaptEntry_, true, apply); };

    auto& state = processor_.state();
    modeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "mode", mode_);
    inputAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "inputDrive", input_);
    adaptAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "autoAdapt", adapt_);
}

OptiLabCoreAudioProcessorEditor::~OptiLabCoreAudioProcessorEditor() {
    setLookAndFeel(nullptr);
}

void OptiLabCoreAudioProcessorEditor::beginExactEntry(AccessibleParameterSlider& slider, ExactEntry& editor, bool integer) {
    editor.setText(integer ? juce::String(static_cast<int>(std::lround(slider.getValue())))
                           : juce::String(slider.getValue(), 1), false);
    editor.setBounds(slider.getBounds());
    slider.setVisible(false);
    editor.setVisible(true);
    editor.toFront(false);
    editor.selectAll();
    editor.grabKeyboardFocus();
}

void OptiLabCoreAudioProcessorEditor::finishExactEntry(AccessibleParameterSlider& slider, ExactEntry& editor, bool integer, bool apply) {
    bool valid = false;
    double value = slider.getValue();
    if (apply) {
        const auto text = editor.getText().trim();
        if (parseStrictDouble(text, value)) {
            valid = value >= slider.getMinimum() && value <= slider.getMaximum();
            if (integer) valid = valid && std::abs(value - std::round(value)) < 0.000001;
        }
    }
    if (valid)
        slider.setValue(value, juce::sendNotificationSync);
    editor.setVisible(false);
    slider.setVisible(true);
    slider.grabKeyboardFocus();
    if (apply && !valid)
        juce::AccessibilityHandler::postAnnouncement("Invalid value. Previous value retained.", juce::AccessibilityHandler::AnnouncementPriority::high);
}

void OptiLabCoreAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(17, 20, 23));
    auto panel = getLocalBounds().toFloat().reduced(18.0f);
    panel.removeFromTop(76.0f);
    g.setColour(juce::Colour(24, 28, 32));
    g.fillRoundedRectangle(panel, 10.0f);
    g.setColour(juce::Colour(61, 68, 76));
    g.drawRoundedRectangle(panel.reduced(0.5f), 10.0f, 1.0f);
}

void OptiLabCoreAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(22);
    title_.setBounds(area.removeFromTop(34));
    subtitle_.setBounds(area.removeFromTop(24));
    area.removeFromTop(18);
    area.reduce(14, 8);
    auto row = [&area](juce::Label& label, juce::Component& control) {
        auto r = area.removeFromTop(62);
        label.setBounds(r.removeFromLeft(132));
        control.setBounds(r.reduced(4, 7));
        area.removeFromTop(7);
    };
    row(modeLabel_, mode_);
    row(inputLabel_, input_);
    inputEntry_.setBounds(input_.getBounds());
    row(adaptLabel_, adapt_);
    adaptEntry_.setBounds(adapt_.getBounds());
}
