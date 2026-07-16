# Changelog

## OptiLab Core v1.1.0

This release keeps the established three-control JSFX design and adds a
Winamp-compatible DSP plug-in for Windows. The Winamp plug-in is a native port
of the same OptiLab Core processing design used by the REAPER JSFX, translated
from JSFX into a small Windows DSP engine and wrapped in the classic 32-bit
Winamp DSP/Effect interface.

Existing JSFX users should expect the same simple workflow: **Mode**, **Input**,
and **Auto-Adapt**. The sound has been refined, especially in Stream polish,
with safer internal peak handling that still lets the final result play loudly
near the output ceiling when the material supports it.

### Processing updates

- Updated Stream polish so difficult music and mixed program material get safer
  internal peak handling without forcing the result to become quiet.
- Added Producer-style hidden peak protection inside Core while keeping the
  visible interface at three controls.
- Reduced the hidden 6-band recombination clipping behavior so it cannot be
  pushed into the harsh high settings used by some Producer presets.
- Retuned Stream polish to use recovered headroom and finish closer to the
  selected output ceiling instead of leaving unnecessary level unused.
- Kept the output sample ceiling at the end of the chain. As before, later
  sample-rate conversion or encoding can create new peaks outside Core.

### For existing JSFX users

- The plug-in is still named **OptiLab Core v1.1.0 (LanesAudio)** in REAPER.
- No new controls were added.
- Stream polish is the mode most likely to sound different: it should feel more
  stable, more protected, and more confident near the ceiling.
- Podcast Leveler and Smooth Limiter keep the same simple workflow and share
  the improved final safety behavior where appropriate.

### New Winamp-compatible DSP plug-in

- Added a 32-bit classic Winamp DSP/Effect plug-in for Winamp and compatible
  Windows broadcast hosts.
- Added Podcast Leveler, Stream polish, and Smooth Limiter modes with the same
  Input and Auto-Adapt controls as the JSFX.
- Ported the Core processing from the JSFX design into a native Windows DSP so
  Winamp-compatible hosts can use the same Core sound without REAPER.
- Added an accessible configuration window built entirely from standard Windows
  controls, with keyboard access keys, logical tab order, and native focus.
- Added host-specific settings under the current Windows user so Winamp,
  RadioBOSS, StationPlaylist Studio, and other hosts do not overwrite one
  another's OptiLab settings.
- Added mono and stereo support for 8, 16, 24, and 32-bit integer PCM.
- Added LanesAudio company, product, and version metadata to the DLL.

### JSFX

- Updated the displayed version to v1.1.0 so all OptiLab Core formats share one
  release number.
- Kept the same three-control interface while applying the corrected Stream
  polish and final peak handling.

## OptiLab Core v1.0.3

This is an Auto-Adapt and speech-transition update for OptiLab Core. The plugin keeps the same simple three-control design: **Mode**, **Input**, and **Auto-Adapt**. No new controls were added.

### Refined Auto-Adapt behavior

Auto-Adapt has been refined in **Podcast Leveler** and **Stream polish**.

In Podcast Leveler, higher Auto-Adapt settings now focus more on controlled leveling and protection rather than simply pushing harder into the processing chain. This reduces the chance of clip-like behavior on difficult speech while still allowing high Auto-Adapt settings to become more active when needed.

In Stream polish, Auto-Adapt now keeps the familiar base sound while becoming smoother and more stable as it is raised. The goal is better consistency for mixed program material without making the mode feel slammed, dull, or over-smoothed.

### Better handling of pauses and speech transitions

A few users reported an edge case where spoken-word material with pauses between phrases could trigger a chunky, old-style automatic gain sound. The effect was similar to the level movement you might hear from a basic tape recorder or consumer voice recorder when it turns quieter material up and then grabs the next loud phrase.

Version 1.0.3 refines that behavior so speech transitions feel smoother and less abrupt, especially on uneven voice recordings, breaths, and short pauses between phrases.

### Startup transition cleanup

Older startup peak-catch behavior has been retired. This helps Core rely more naturally on its main leveling and final control stages instead of adding an extra startup reaction that could become audible on certain material.

### Screen-reader wording improvement

The Stream mode is now written as **Stream polish** instead of **Stream Polish**. This avoids a confusing pronunciation issue in some screen readers and text-to-speech voices, where the capitalized word could be read as “Polish” referring to Poland rather than “polish” as in audio polish.

### Smooth Limiter unchanged

Smooth Limiter remains essentially unchanged in this update. It already has a focused job, so this release concentrates on Podcast Leveler, Stream polish, and Auto-Adapt behavior.

## OptiLab Core v1.0.2

This was a sound and usability update for OptiLab Core. The plugin kept the same simple three-control design: **Mode**, **Input Drive**, and **Auto-Adapt**. No extra controls were added.

### More complete output ceiling behavior

Podcast Leveler, Stream Polish, and Smooth Limiter were updated to use the full **-0.1 dBFS** output target instead of holding Podcast and Stream modes back around the older conservative ceiling range.

This gave Core more room to use its own staging and final processing. In practical terms, Podcast and Stream modes could finish closer to the available digital ceiling when the material and processing called for it.

### Same simple control layout

OptiLab Core remained intentionally simple:

- **Mode** chooses the starting processor behavior.
- **Input Drive** controls how hard the processor is fed.
- **Auto-Adapt** adjusts the amount of adaptive tone and energy balancing.

### Input Drive persistence fix retained

This release kept the v1.0.1 fix that prevents Input Drive from being reset during playback, render, stop/start, or plugin reinitialization.

Switching modes still loads a sensible starting Input Drive for that mode, but once you adjust Input Drive yourself, Core should not silently overwrite it during normal playback or rendering.

### Small CPU hygiene

This version included small internal cleanup work, including cached multiband clip scalar values and removal of unused output-gain work. These changes were intended to keep the processor lean without changing the basic three-control workflow.

Projects using OptiLab Core may render faster, especially if multiple instances are being used.

## OptiLab Core v1.0.1

- Fixed Input Drive being reset to the mode default during playback, rendering, stop/start, or plugin reinitialization.

## OptiLab Core v1.0.0

- Initial public release.
