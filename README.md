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

## What is new in v1.4.0

Version 1.4.0 strengthens the upper end of **Stream polish Auto-Adapt**. At
100%, it can produce more sustained loudness and firmer high-frequency control
on real program material without adding controls or changing the final delivery
target.

High Auto-Adapt settings can bring sustained material forward more noticeably,
while bright events receive more independent control. If final limiting remains
heavy, Core automatically eases the lift and withdraws positive bass assistance
instead of allowing the stages to fight or become unnecessarily crushed.

Silence, low-level noise, and rumble do not cause the slow loudness lift to
build. The response is blended continuously across the Auto-Adapt range rather
than switching abruptly. The final output target remains **-0.1 dBFS**.

For StationPlaylist compatibility, the Settings window now opens independently
of the host's Preferences page to avoid focus and speech conflicts with NVDA.
Its optional meters also avoid redundant accessible label updates.

## Output ceiling and sample rates

OptiLab Core now uses a reconstruction-aware final delivery limiter. In addition
to controlling the stored output samples, it detects intersample peaks that can
become actual overs when audio is converted between common sample rates such as
44.1 and 48 kHz.

The final output target remains **-0.1 dBFS**. The delivery limiter is designed
to act only when reconstruction-aware peak safety requires it rather than
lowering the entire program for extra headroom.

Downstream processing, unusual resamplers, and lossy codecs can still alter peaks
after Core, so measuring the final delivered file remains good practice.

## Downloads

Release downloads are available from the repository's GitHub Releases page.

**Updating from a previous version?** Download the bare plug-in files and replace
what you have — no extraction needed:

- `OptiLab_Core.clap` — drop-in replacement for the 64-bit Windows CLAP plug-in.
- `dsp_optilab_core.dll` — drop-in replacement for the Winamp-compatible DSP.
- `optilab_core.jsfx` — drop-in replacement for REAPER users.

**Complete Package:** Download `OptiLab-Core-1.4.0.zip` to get all three plug-in
formats (`OptiLab_Core.clap`, `dsp_optilab_core.dll`, `optilab_core.jsfx`),
documentation, and SHA-256 checksums in a single archive.

## REAPER installation

1. Download `optilab_core.jsfx` from the latest release.
2. In REAPER, choose **Options > Show REAPER resource path in explorer/finder**.
3. Open the `Effects` folder and copy the JSFX file into it.
4. Restart REAPER if it is already running.
5. Add OptiLab Core from REAPER's FX browser.

## CLAP installation

1. Download `OptiLab-Core-1.4.0.zip` (and extract `OptiLab_Core.clap`), or
   download `OptiLab_Core.clap` directly.
2. Close the CLAP host.
3. Copy `OptiLab_Core.clap` to
   `%LOCALAPPDATA%\Programs\Common\CLAP`.
4. Restart the host or perform a full plug-in rescan.

See [`native/CLAP.md`](native/CLAP.md) for controls, accessibility, build
instructions, and host behavior.

## StationPlaylist and Winamp-compatible DSP installation and use

The Windows DLL uses the classic 32-bit Winamp DSP/Effect interface. It can be
used by Winamp and by compatible Windows broadcast applications that support
Winamp DSP plug-ins.

1. Download `OptiLab-Core-1.4.0.zip` (and extract `dsp_optilab_core.dll`), or
   download `dsp_optilab_core.dll` directly.
2. Close the host application.
3. Copy `dsp_optilab_core.dll` into the host's Winamp DSP plug-in folder.
4. Restart the host and select **OptiLab Core 1.4.0** in its DSP configuration.

Winamp normally uses `C:\Program Files (x86)\Winamp\Plugins`. Writing there may
require administrator approval. Other hosts choose their own plug-in folders.

The configuration window opens independently of StationPlaylist's Preferences
page and uses standard Windows controls with keyboard access, normal focus
indication, and screen-reader-friendly labels. See
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

OptiLab Core v1.3.0 and later are **source-available** under the Apache License
2.0 with the Commons Clause License Condition v1.0. See [LICENSE](LICENSE).

You are explicitly welcome to use OptiLab Core for personal or commercial
**audio work**. This includes paid production and mastering, commercial radio
and broadcasting, monetized streams and podcasts, released music, and similar
work. Lanes Audio claims no royalty or ownership interest in audio merely
because OptiLab Core processed it.

The source may be inspected, studied, modified, and redistributed subject to the
license. However, the license does **not** permit selling OptiLab Core itself, a
rebranded or lightly modified version of it, or another product or service whose
value derives entirely or substantially from OptiLab Core's software
functionality. Contact `info@lanesaudio.com` to discuss a separate commercial
software license.

OptiLab Core releases published before v1.3.0 remain under the license terms
under which those versions were originally released.
