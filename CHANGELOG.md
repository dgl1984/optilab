# Changelog

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
