# OptiLab Core

OptiLab Core is a free, accessible broadcast and mastering audio processor from
LanesAudio. It combines leveling, density, tone control, saturation, limiting,
and general program polish. It is available as a JSFX for REAPER, a 64-bit
Windows CLAP plug-in, and a classic Winamp-compatible DSP plug-in for Windows.

The processor has three main controls: **Mode**, **Input**, and **Auto-Adapt**.
That simplicity is intentional. OptiLab Core is designed to reach a useful,
finished sound without building a large processor chain.

It does not provide audio restoration and cannot repair damaged recordings. It
is intended for podcasts, streams, voice work, music playback, internet radio,
and quick mastering-style polish.

## Modes

### Podcast Leveler

Designed for speech, podcasts, voice tracks, and mixed voice content. It focuses
on leveling, control, and a smoother mid-focused sound.

### Stream polish

Designed for music, internet radio, and higher-energy material. It provides a
more finished broadcast-style sound without requiring extensive setup.

### Smooth Limiter

Designed for clean limiting and mastering-style use where peak control is more
important than an aggressive broadcast sound.

## Controls

- **Mode** chooses the general processing behavior.
- **Input** controls how hard the processor is driven.
- **Auto-Adapt** adjusts several leveling, tone, density, and protection stages
  together.

Start gently. If the result becomes too flat, dense, or pushed, lower Input
before changing everything else.

## What is new in v1.3.0

Version 1.3.0 keeps OptiLab Core's simple **Mode**, **Input**, and **Auto-Adapt** workflow while making Auto-Adapt considerably more capable.

In **Stream polish**, lower Auto-Adapt settings remain relatively open. Raising Auto-Adapt progressively adds more width, bass control, density, Shape, and peak protection, moving toward firmer broadcast-style processing without simply driving every stage harder. The transition has also been refined so stronger low-frequency cleanup does not unnecessarily thin useful bass weight or punch.

**Smooth Limiter** also receives improved peak handling while retaining its focused mastering-style role. The visible interface remains unchanged.

## Output ceiling and sample rates

OptiLab Core clamps its own output samples to the ceiling selected by each
mode. If a host processes Core at one sample rate and then converts the result
to another rate, that later resampling can create new peaks above Core's sample
ceiling. This is normal resampling behavior and does not mean Core skipped its
final limiter.

For a controlled ceiling test, use the same project, processing, and output
sample rate. If the final delivery workflow must resample after Core, measure
the converted file and apply any required true-peak or delivery limiting after
that conversion.

## Downloads

Release downloads are available from the repository's GitHub Releases page.

**Updating from a previous version?** Download the bare plug-in files and replace
what you have — no extraction needed:

- `OptiLab_Core.clap` — drop-in replacement for the 64-bit Windows CLAP plug-in.
- `dsp_optilab_core.dll` — drop-in replacement for the Winamp-compatible DSP.
- `optilab_core.jsfx` — drop-in replacement for REAPER users.

**Complete Package:** Download `OptiLab-Core-1.3.0.zip` to get all three plug-in formats (`OptiLab_Core.clap`, `dsp_optilab_core.dll`, `optilab_core.jsfx`), documentation, and SHA-256 checksums in a single archive.

## REAPER installation

1. Download `optilab_core.jsfx` from the latest release.
2. In REAPER, choose **Options > Show REAPER resource path in explorer/finder**.
3. Open the `Effects` folder and copy the JSFX file into it.
4. Restart REAPER if it is already running.
5. Add OptiLab Core from REAPER's FX browser.

## CLAP installation

1. Download `OptiLab-Core-1.3.0.zip` (and extract `OptiLab_Core.clap`), or download `OptiLab_Core.clap` directly.
2. Close the CLAP host.
3. Copy `OptiLab_Core.clap` to
   `%LOCALAPPDATA%\Programs\Common\CLAP`.
4. Restart the host or perform a full plug-in rescan.

See [`native/CLAP.md`](native/CLAP.md) for controls, accessibility, build
instructions, and host behavior.

## Winamp-compatible DSP installation and use

The Windows DLL uses the classic 32-bit Winamp DSP/Effect interface. It can be
used by Winamp and by compatible Windows broadcast applications that support
Winamp DSP plug-ins.

1. Download `OptiLab-Core-1.3.0.zip` (and extract `dsp_optilab_core.dll`), or download `dsp_optilab_core.dll` directly.
2. Close the host application.
3. Copy `dsp_optilab_core.dll` into the host's Winamp DSP plug-in folder.
4. Restart the host and select **OptiLab Core 1.3.0** in its DSP configuration.

Winamp normally uses `C:\Program Files (x86)\Winamp\Plugins`. Writing there may
require administrator approval. Other hosts choose their own plug-in folders.

The configuration window uses standard Windows controls with keyboard access,
normal focus indication, and screen-reader-friendly labels. See
[`native/WINAMP.md`](native/WINAMP.md) for complete details and troubleshooting.

Use the Winamp DSP the same way you use the JSFX: choose a mode, set Input so
the processor is working without sounding overdriven, then raise Auto-Adapt if
you want more automatic leveling, tone balancing, and protection.

## Development Layout

Source ownership and generated-file boundaries are documented in
[`docs/REPOSITORY_LAYOUT.md`](docs/REPOSITORY_LAYOUT.md). CMake build trees,
release staging, installed plug-ins, and local Graphify output are intentionally
kept out of version control.

## Donations and contact

OptiLab Core is free. Donations are welcome but never required.

[Donate via PayPal](https://paypal.me/dgl1984)

Questions, bug reports, and accessibility feedback: `info@lanesaudio.com`

## License

OptiLab Core v1.3.0 and later are **source-available** under the Apache License 2.0 with the Commons Clause License Condition v1.0. See [LICENSE](LICENSE).

You are explicitly welcome to use OptiLab Core for personal or commercial **audio work**. This includes paid production and mastering, commercial radio and broadcasting, monetized streams and podcasts, released music, and similar work. Lanes Audio claims no royalty or ownership interest in audio merely because OptiLab Core processed it.

The source may be inspected, studied, modified, and redistributed subject to the license. However, the license does **not** permit selling OptiLab Core itself, a rebranded or lightly modified version of it, or another product or service whose value derives entirely or substantially from OptiLab Core's software functionality. Contact `info@lanesaudio.com` to discuss a separate commercial software license.

OptiLab Core releases published before v1.3.0 remain under the license terms under which those versions were originally released.