// Copyright 2026 Lanes Audio
// SPDX-License-Identifier: LicenseRef-Apache-2.0-with-Commons-Clause-1.0
// Licensed under the Apache License, Version 2.0 with the Commons Clause
// License Condition v1.0. See LICENSE and NOTICE in the repository root.

#include "AccessibleParameterSlider.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cmath>

AccessibleParameterSlider::AccessibleParameterSlider(juce::String name,
                                                     double minimum,
                                                     double maximum,
                                                     double step,
                                                     double pageStep,
                                                     Formatter formatter)
    : pageStep_(pageStep), formatter_(std::move(formatter)) {
    setName(name);
    setTitle(name);
    setDescription("Parameter control");
    setHelpText("Arrow keys change the value. Page Up and Page Down use a larger step. Home selects the highest value and End selects the lowest value. Enter or F2 opens exact value entry.");
    setAccessible(true);
    setWantsKeyboardFocus(true);
    setSliderStyle(juce::Slider::LinearHorizontal);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(minimum, maximum, step);
    textFromValueFunction = [this](double value) { return formatter_(value); };
}

juce::String AccessibleParameterSlider::formattedValue() const {
    return formatter_(getValue());
}

bool AccessibleParameterSlider::keyPressed(const juce::KeyPress& key) {
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey) {
        setValue(juce::jlimit(getMinimum(), getMaximum(), getValue() - getInterval()), juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey) {
        setValue(juce::jlimit(getMinimum(), getMaximum(), getValue() + getInterval()), juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::pageDownKey) {
        setValue(juce::jlimit(getMinimum(), getMaximum(), getValue() - pageStep_), juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::pageUpKey) {
        setValue(juce::jlimit(getMinimum(), getMaximum(), getValue() + pageStep_), juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::homeKey) {
        setValue(getMaximum(), juce::sendNotificationSync);
        return true;
    }
    if (code == juce::KeyPress::endKey) {
        setValue(getMinimum(), juce::sendNotificationSync);
        return true;
    }
    if ((code == juce::KeyPress::returnKey || code == juce::KeyPress::F2Key) && onRequestExactEntry) {
        onRequestExactEntry();
        return true;
    }
    return juce::Slider::keyPressed(key);
}

std::unique_ptr<juce::AccessibilityHandler> AccessibleParameterSlider::createAccessibilityHandler() {
    class ValueInterface final : public juce::AccessibilityValueInterface {
    public:
        explicit ValueInterface(AccessibleParameterSlider& slider) : slider_(slider) {}
        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return slider_.getValue(); }
        juce::String getCurrentValueAsString() const override { return slider_.formattedValue(); }
        void setValue(double value) override {
            slider_.setValue(juce::jlimit(slider_.getMinimum(), slider_.getMaximum(), value), juce::sendNotificationSync);
        }
        void setValueAsString(const juce::String& text) override {
            auto normalized = text.trim();
            if (normalized.isEmpty()) return;
            if (normalized.containsChar(',') && !normalized.containsChar('.'))
                normalized = normalized.replaceCharacter(',', '.');
            const auto utf8 = normalized.toRawUTF8();
            char* end = nullptr;
            errno = 0;
            const double value = std::strtod(utf8, &end);
            if (end == utf8 || end == nullptr || errno == ERANGE || !std::isfinite(value)) return;
            const auto suffix = juce::String::fromUTF8(end).trim();
            if (suffix.isNotEmpty() && suffix.compareIgnoreCase("dB") != 0
                && suffix != "%" && suffix.compareIgnoreCase("percent") != 0) return;
            if (value < slider_.getMinimum() || value > slider_.getMaximum()) return;
            slider_.setValue(value, juce::sendNotificationSync);
        }
        AccessibleValueRange getRange() const override {
            return {{slider_.getMinimum(), slider_.getMaximum()}, slider_.getInterval()};
        }
    private:
        AccessibleParameterSlider& slider_;
    };
    return std::make_unique<juce::AccessibilityHandler>(
        *this, juce::AccessibilityRole::slider, juce::AccessibilityActions(),
        juce::AccessibilityHandler::Interfaces{std::make_unique<ValueInterface>(*this)});
}
