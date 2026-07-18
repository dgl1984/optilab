# Changelog

## OptiLab Core v1.2.0

This release adds the first public 64-bit Windows CLAP build, corrects a
user-reported fade at the beginning of playback, and carries OptiLab Core's
three-control workflow into an accessible, resizable native editor. The JSFX,
CLAP, and native Core retain matching processing behavior.

### Processing

- Fixed an unintended short fade-in at the beginning of playback. OptiLab Core
  now starts at its intended processing state without the previous startup
  ramp.
- Kept constant reported CLAP latency across modes by padding lower-latency
  configurations internally.
- Preserved bit-identical rendered output through the CLAP performance and
  accessibility work.

### New CLAP plug-in

- Added a self-contained 64-bit Windows CLAP plug-in with the stable ID
  `com.lanesaudio.optilab-core` and LanesAudio branding.
- Exposed Mode, Input Drive, and Auto-Adapt through the standard CLAP parameter
  extension so hosts, automation, OSARA, and other accessibility tools can use
  every parameter without opening the custom editor.
- Added a responsive native Windows editor with standard controls, exact dB
  text for Input Drive, percentage output for Auto-Adapt, deterministic Tab and
  Shift+Tab traversal, and keyboard adjustment.
- Added state save/restore, sample-rate-aware fixed latency, offline-render
  notification, denormal protection, and visual-meter work that is disabled
  from the audio path during offline rendering.
- Added version-checked packaging, architecture/export/runtime verification,
  and CLAP ABI, processing, state, editor, focus, and MSAA smoke tests.

### Accessibility implementation note

CLAP accessibility has two layers and both must remain functional. First, every
user parameter must have a complete standard CLAP parameter implementation with
a meaningful name, range, text conversion, and automation behavior. This is
the host-level path used by OSARA and must not depend on the custom editor.
Second, a custom editor must use controls with native accessibility semantics,
logical focus order, exact unit text, and keyboard operation. A Windows
trackbar reports its normalized percentage, so a non-percentage value such as
Input Drive also needs a focusable native field that exposes the actual dB
value.

REAPER adds a `reaperPluginHostWrapProc` window around an embedded CLAP editor.
Parenting the accessible controls directly beneath that wrapper can prevent Tab
and mouse hit testing from reaching them. OptiLab detects that REAPER-specific
wrapper, places its `WS_EX_CONTROLPARENT` editor beside it under the wrapper's
parent, preserves the wrapper bounds, and posts a message after `gui.show()` to
hide the wrapper before moving focus to the first control. Other hosts retain
normal CLAP parenting.

This approach was informed by James Teh's GPLv2
[App2Clap](https://github.com/jcsteh/app2clap): its
[native `DS_CONTROL` dialog definitions](https://github.com/jcsteh/app2clap/blob/d175ef32c4da31dcc711fa8b8a481c1654987411/src/resource.rc#L13-L46),
[REAPER-specific parenting and show handling](https://github.com/jcsteh/app2clap/blob/d175ef32c4da31dcc711fa8b8a481c1654987411/src/common.cpp#L11-L44),
and the original
[tabbing and hit-testing fix](https://github.com/jcsteh/app2clap/commit/f0af98c852f314c4efc7de8767fe51cc69a92fd2).
OptiLab's smoke test emulates the REAPER wrapper and fails if the editor is
again placed beneath it, if the wrapper remains over the controls, if initial
focus fails, or if Tab order and accessible dB text regress. Maintainers and
fork authors should read
[`docs/CLAP_ACCESSIBILITY.md`](docs/CLAP_ACCESSIBILITY.md) before replacing the
editor or changing its window hierarchy.

## OptiLab Core v1.1.2

This release keeps the Winamp DSP sound and host calibration from v1.1.1, while
making the settings-window meters opt-in for better screen-reader behavior.

### Winamp-compatible DSP

- Added a **Show visual meters** checkbox to the settings window.
- Left visual meters off by default so screen readers do not announce changing
  progress percentages unless the user explicitly enables them.
- Kept simple text status fields for input peak, output peak, and recent
  full-scale sample activity.
- Kept the v1.1.1 Winamp Stream polish default of `+1.0 dB`.

## OptiLab Core v1.1.1

This is a small Winamp DSP polish release. The REAPER JSFX is still the same
three-control Core processor, with the displayed release number updated so the
downloadable JSFX and Winamp build share one version.

### Winamp-compatible DSP

- Added live standard Windows meters to the settings window: input peak, output
  peak, and a recent full-scale sample indicator.
- Kept the metering accessible by pairing the meter bars with plain text
  readouts instead of relying on custom graphics.
- Lowered the Winamp Stream polish starting Input drive to `+1.0 dB`. This is
  better matched to integer-PCM Winamp-compatible hosts such as Winamp and
  RadioBOSS, where the earlier `+4.5 dB` starting point could feel pinned or
  clipped.
- Existing saved per-host settings are still respected.

### Build and release

- Pinned the GitHub Actions Windows workflow to the VS2022 runner image.
- Skipped native CI for Markdown-only pushes.

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
