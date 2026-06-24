# OptiLab Core v1.0.2

This is a sound and usability update for OptiLab Core. The plugin still keeps the same simple three-control design: **Mode**, **Input Drive**, and **Auto-Adapt**. No extra controls were added.

## What changed

### More complete output ceiling behavior

Podcast Leveler, Stream Polish, and Smooth Limiter now use the full **-0.1 dBFS** output target instead of holding Podcast and Stream modes back around the older conservative ceiling range.

This gives Core more room to use its own staging and final processing. In practical terms, Podcast and Stream modes can now finish closer to the available digital ceiling when the material and processing call for it.

### Same simple control layout

OptiLab Core remains intentionally simple:

* **Mode** chooses the starting processor behavior.
* **Input Drive** controls how hard the processor is fed.
* **Auto-Adapt** adjusts the amount of adaptive tone and energy balancing.

### Input Drive persistence fix retained

This release keeps the v1.0.1 fix that prevents Input Drive from being reset during playback, render, stop/start, or plugin reinitialization.

Switching modes still loads a sensible starting Input Drive for that mode, but once you adjust Input Drive yourself, Core should not silently overwrite it during normal playback or rendering.

### Small CPU hygiene

This version includes small internal cleanup work, including cached multiband clip scalar values and removal of unused output-gain work. These changes are intended to keep the processor lean without changing the basic three-control workflow.

Projects using OptiLab Core may render faster, especially if multiple instances are being used.

## Install

Download **optilab_core.jsfx**, copy it into REAPER's Effects folder, then restart REAPER or rescan JSFX.

For a portable REAPER install, copy the file into that install's Effects folder.

For a standard Windows REAPER install, the user Effects folder is usually under:

`%APPDATA%\REAPER\Effects`

## Notes

OptiLab Core is the free, simple version of the yet-to-be-released OptiLab Producer. It is designed for practical broadcast-style polish, podcast leveling, streaming, and smooth final limiting without exposing the full Producer control set.
