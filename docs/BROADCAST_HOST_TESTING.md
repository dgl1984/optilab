# Broadcast Host Test Checklist

Use the Release ZIP from `dist`, not a loose development DLL. Record the host
version, Windows version, audio format, and result for every test run.

## Hosts

- [ ] Winamp
- [ ] RadioBOSS
- [ ] StationPlaylist Studio

## Installation and accessibility

- [ ] The host discovers `dsp_optilab_core.dll` after a full restart.
- [ ] The plug-in is identified as **OptiLab Core 1.1.2** in the host's DSP
      list.
- [ ] The settings dialog opens from the host's DSP configuration.
- [ ] Every control is announced correctly by the screen reader (NVDA / JAWS).
- [ ] Tab, Shift+Tab, Alt access keys, Enter, and Escape work as expected.
- [ ] Invalid numeric values produce a readable warning and return focus to the
      relevant field.
- [ ] **Show visual meters** checkbox is present and is **unchecked by
      default**.
- [ ] Enabling **Show visual meters** shows live progress bars and text
      readouts for Input peak, Output peak, and Full-scale activity.
- [ ] With meters hidden, the screen reader does not announce continuously
      changing percentages during playback.
- [ ] With meters visible, text readouts update and are readable on demand.
- [ ] Settings persist after closing and reopening the host.
- [ ] Settings changed in one host do not change another host's settings
      (per-host registry isolation).

## Audio

- [ ] 44.1 kHz stereo playback processes continuously.
- [ ] 48 kHz stereo playback processes continuously.
- [ ] Mono playback processes correctly when supported by the host.
- [ ] Starting, stopping, pausing, seeking, and changing tracks are clean.
- [ ] Transitions between files with different sample rates are clean.
- [ ] A same-rate render respects the mode's final sample ceiling.
- [ ] Any ceiling overshoot after host sample-rate conversion is recorded
      separately from the same-rate result.

### Mode defaults (Winamp DSP, v1.1.2)

- [ ] Podcast Leveler defaults to **+3.5 dB** Input and **0 %** Auto-Adapt.
- [ ] Stream polish defaults to **+1.0 dB** Input and **0 %** Auto-Adapt.
      *(Note: gentler than the REAPER JSFX default of +4.5 dB; this is
      intentional for integer-PCM Winamp-compatible hosts.)*
- [ ] Smooth Limiter defaults to **0 dB** Input and **0 %** Auto-Adapt.
- [ ] **Restore Defaults** button restores Podcast Leveler, +3.5 dB, 0 %.
- [ ] Switching modes during playback applies the new mode default Input.
- [ ] A user-adjusted Input value is preserved when re-opening the settings
      window.

## Audio formats

- [ ] 16-bit 44.1 kHz stereo (primary broadcast path).
- [ ] 16-bit 48 kHz stereo.
- [ ] 24-bit stereo (where host supports it).
- [ ] 8-bit stereo (pass-through or processed where host supports it).
- [ ] Mono at any of the above formats.
- [ ] An unsupported format or channel count passes through unchanged without
      a crash or silence.

## Continuous operation

- [ ] Changing controls during playback does not stop or destabilize audio.
- [ ] At least four hours of continuous processing completes without a crash,
      dropout, or increasing memory use.

## Host behavior

- [ ] Disabling or removing the DSP plug-in restores unprocessed audio.
- [ ] Restarting after an abnormal host shutdown restores valid settings.
- [ ] A second attempted OptiLab instance is either handled correctly or
      refused clearly by the host.
- [ ] Other encoders, metadata tools, schedulers, and DSP plug-ins remain
      stable alongside OptiLab Core.

## Reporting failures

Include the following in any bug report:

- Host application name and version.
- Windows version and architecture (32-bit / 64-bit OS).
- Audio format: sample rate, bit depth, channel count.
- Selected mode, Input drive, and Auto-Adapt value.
- Whether **Show visual meters** was enabled.
- The shortest repeatable sequence that demonstrates the problem.

Send reports to `info@lanesaudio.com` or open a GitHub issue.
