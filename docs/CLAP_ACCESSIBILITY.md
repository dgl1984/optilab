# Keyboard and accessibility

The JUCE VST3 and CLAP editor exposes Mode, Input Drive, and Auto-Adapt with explicit accessible names and visible keyboard focus. Tab and Shift+Tab move through the controls.

- Mode: arrows and Page Up/Down change choices; Home selects Stream (the first choice) and End selects Smooth (the last choice).
- Input Drive: arrows change 0.1 dB; Page Up/Down change 1 dB; Home selects maximum and End selects minimum.
- Auto-Adapt: arrows change 1%; Page Up/Down change 10%; Home selects maximum and End selects minimum.
- Enter or F2 on a continuous control opens exact numeric entry. Plain numbers and the displayed units are accepted; comma decimals are also accepted. Auto-Adapt exact entry requires a whole percentage. Enter accepts the entry; Escape cancels it. Invalid and out-of-range input leaves the previous value unchanged. Moving focus away cancels exact entry and restores the slider.

Host generic parameter interfaces, including REAPER/OSARA, remain available. Winamp-compatible hosts and foobar use their native settings dialogs. Saved values may differ from fresh-instance defaults.

When reporting an accessibility issue, include the host, plugin format, screen reader/version, and the keyboard sequence that reproduces it. Host-specific testing remains necessary.

For the native dialog options and REAPER JSFX controls, see [Finding your way around](USER_GUIDE.md#finding-your-way-around). The exact-entry shortcuts above apply to the JUCE VST3 and CLAP editor.

In foobar2000 and Winamp, Mode is a native combo box; Input Drive and Auto-Adapt are native sliders with value readouts. Arrow steps are 0.1 dB or 1%; page steps are 1 dB or 10%. Native slider Home selects the minimum and End the maximum.
