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

## What is new in v1.2.0

Version 1.2.0 adds the first public 64-bit Windows CLAP build while keeping the
same Mode, Input, and Auto-Adapt workflow across formats.

It fixes a user-reported short fade-in at the beginning of playback. The CLAP
build reports its latency correctly, preserves matching rendered output, and
uses standard parameters that hosts and OSARA can expose without opening the
custom editor.

The resizable CLAP editor uses native controls, exact dB text, keyboard
operation, and REAPER-specific window parenting informed by App2Clap so focus,
Tab navigation, and mouse hit testing reach the controls immediately.

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

- REAPER users need `optilab_core.jsfx`.
- 64-bit Windows CLAP hosts need `OptiLab-Core-1.2.0-CLAP-x64.zip`.
- Winamp-compatible hosts need `OptiLab-Core-1.2.0-Winamp-DSP-x86.zip`.

## REAPER installation

1. Download `optilab_core.jsfx` from the latest release.
2. In REAPER, choose **Options > Show REAPER resource path in explorer/finder**.
3. Open the `Effects` folder and copy the JSFX file into it.
4. Restart REAPER if it is already running.
5. Add OptiLab Core from REAPER's FX browser.

## CLAP installation

1. Download and extract `OptiLab-Core-1.2.0-CLAP-x64.zip`.
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

1. Download and extract the Winamp DSP ZIP from the latest release.
2. Close the host application.
3. Copy `dsp_optilab_core.dll` into the host's Winamp DSP plug-in folder.
4. Restart the host and select **OptiLab Core 1.2.0** in its DSP configuration.

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

OptiLab Core is released under the Apache License 2.0. You may use, modify, and
redistribute it under the terms of the included [`LICENSE`](LICENSE) file.

If you distribute a modified version, clearly identify it as modified from the
original LanesAudio OptiLab Core release.
