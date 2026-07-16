# Broadcast Host Test Checklist

Use the Release ZIP from `dist`, not a loose development DLL. Record the host
version, Windows version, audio format, and result for every test.

## Hosts

- [ ] Winamp
- [ ] RadioBOSS
- [ ] StationPlaylist Studio

## Installation and accessibility

- [ ] The host discovers `dsp_optilab_core.dll` after a restart.
- [ ] The plug-in is identified as OptiLab Core 1.1.0.
- [ ] The settings dialog opens from the host.
- [ ] Every control is announced correctly by the screen reader.
- [ ] Tab, Shift+Tab, Alt access keys, Enter, and Escape work as expected.
- [ ] Invalid numeric values produce a readable warning and return focus to the
      relevant field.
- [ ] Settings persist after closing and reopening the host.
- [ ] Settings changed in one host do not change another host's settings.

## Audio

- [ ] 44.1 kHz stereo playback processes continuously.
- [ ] 48 kHz stereo playback processes continuously.
- [ ] Mono playback processes correctly when supported by the host.
- [ ] Starting, stopping, pausing, seeking, and changing tracks are clean.
- [ ] Transitions between files with different sample rates are clean.
- [ ] A same-rate render respects the mode's final sample ceiling.
- [ ] Any ceiling overshoot after host sample-rate conversion is recorded
      separately from the same-rate result.
- [ ] Podcast Leveler defaults to +3.5 dB Input and 0 percent Auto-Adapt.
- [ ] Stream polish defaults to +4.5 dB Input and 0 percent Auto-Adapt.
- [ ] Smooth Limiter defaults to 0 dB Input and 0 percent Auto-Adapt.
- [ ] Changing controls during playback does not stop or destabilize audio.
- [ ] At least four hours of continuous processing completes without a crash,
      dropout, or increasing memory use.

## Host behavior

- [ ] Disabling or removing the DSP restores unprocessed audio.
- [ ] Restarting after an abnormal host shutdown restores valid settings.
- [ ] A second attempted OptiLab instance is either handled correctly or refused
      clearly by the host.
- [ ] Other encoders, metadata tools, schedulers, and DSP plug-ins remain stable.

Report failures with the host version, sample rate, channel count, bit depth,
selected mode, Input, Auto-Adapt, and the shortest repeatable sequence.
