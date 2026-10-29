# OptiLab Core

I created Core because I was tired of the sound I kept hearing from a lot of default processing, especially on online streams: loud and controlled, but too often flatter, harsher, smaller, or simply less interesting than the material deserved.

Core is my answer to that. Instead of asking you to adjust each stage separately, it lets you work with the overall behavior of the processor. I also use it as a tool to learn, experiment, and teach—to explore what processing does to sound without requiring every individual decision to be made separately.

The processor coordinates the details. You still decide what sounds good.

If a program does more than say “Hello, world!” and/or looks well-designed visually, every line responsible for such elevated standards was coded by AI—but the design is mine. The user guide includes a fuller answer about the human and AI contributions to OptiLab.

OptiLab Core is a free, accessible broadcast and mastering processor from Lanes Audio. Choose a **Mode**, decide how hard to feed it with **Input Drive**, and turn **Auto-Adapt** until the result is where you want it. There is no magic percentage that is right for every recording.

[**Published downloads**](https://github.com/dgl1984/optilab/releases) · [**Read the user guide**](docs/USER_GUIDE.md)

## Version 1.5.0

Download **1.5.0** as one Windows ZIP containing all supported formats. The complete public source is included when you clone this repository. See the [release notes](RELEASE_NOTES.md), [change history](CHANGELOG.md), and [user guide](docs/USER_GUIDE.md).

Version 1.5 rebuilds Core around the current processing engine while keeping the same three-control workflow. The idea is to make each of those three choices count.

**Stream polish** is now the first Mode in the native formats and the fresh-instance default in every format. JSFX preserves its legacy Mode numbering for existing sessions. **Podcast Leveler** has developed into a distinctly flatter, more controlled talk-station-style voicing. **Smooth Limiter** now has a clearer progression from gentle mastering-style control toward its strongest event-limiting endpoint.

## Formats

| Format | Windows architecture |
|---|---|
| VST3 | 64-bit |
| CLAP | 64-bit |
| Winamp-compatible DSP | 32-bit |
| foobar2000 | 32-bit and 64-bit in one component package |
| JSFX | REAPER effect; source included in `Effects` |

## Modes and controls

**Stream polish** is the default. Use it when you want a bigger, more controlled sound across mixed content such as music, games, speech, streams and radio. At 0% Auto-Adapt it already has a useful processing sound; higher settings explore more intense voicings. It starts Input Drive at 0 dB.

**Podcast Leveler** is for a flatter, more consistent sound that helps very different voices and sources feel as though they belong in the same program. Its overall approach is similar in spirit to modern talk-station processing. It is designed around speech, but try it on other material too. It starts Input Drive at +3.5 dB.

**Smooth Limiter** is for already reasonably balanced material when you mainly want smooth loudness and peak control. It starts Input Drive at 0 dB and expects you to use Input Drive to decide how hard the material hits the limiter.

All three modes now finish through the same history-aware, reconstruction-aware delivery limiter. The modes still sound different because the processing before that limiter is different; the final limiter itself is deliberately shared.

Fresh instances start on **Stream polish**, with Input Drive at 0 dB and Auto-Adapt at 0%.

For practical guidance on the controls, the three voicings, and how Auto-Adapt changes each one, read the [user guide](docs/USER_GUIDE.md).

## Installation

- **VST3:** copy the complete `OptiLab Core.vst3` bundle to your VST3 folder and rescan.
- **CLAP:** copy `OptiLab Core.clap` to your CLAP folder and rescan. Read the [CLAP upgrade note](CHANGELOG.md#clap-upgrade-note) before replacing the 1.4 CLAP plugin.
- **Winamp-compatible hosts:** copy `dsp_optilab_core.dll` to the host's DSP plugin folder.
- **foobar2000:** install `foo_optilab_core-1.5.0.fb2k-component` through **Preferences > Components**. The installer selects the appropriate architecture; do not copy the archive or a loose DLL into `user-components`.

- **JSFX:** copy `Effects/optilab_core.jsfx` to your REAPER resource directory's `Effects` folder, then insert it as a JS effect. See [JSFX notes and validation](docs/JSFX.md).

## First use

1. Insert Core on a stereo track or bus and start with **Stream polish**.
2. Begin at 0 dB **Input Drive** and 0% **Auto-Adapt** while checking your source level.
3. Raise Auto-Adapt gradually and compare at a similar listening level.
4. Try Podcast Leveler for consistent speech, or Smooth Limiter for material
   that mainly needs peak control. Each mode has its own fresh Input Drive default.

The [user guide](docs/USER_GUIDE.md) walks from installation and finding your way around to the controls, each Mode, and what to listen for.
Back up important projects and read the [CLAP upgrade note](CHANGELOG.md#clap-upgrade-note)
before replacing an older installation.

## Accessibility

The JUCE VST3 and CLAP editor provides named controls, keyboard operation, visible focus, and exact value entry. Generic host parameter access remains available. See [keyboard and accessibility guidance](docs/CLAP_ACCESSIBILITY.md).

Winamp-compatible hosts and foobar2000 use native Windows settings dialogs.

## Build from source

See [Windows build instructions](docs/BUILDING.md). Run `BUILD_WINDOWS.bat` to fetch pinned dependencies, build all Windows formats, run the public native and JSFX regression sets, and stage both binary and public-source packages. Outputs appear under `dist` and `dist-source`; nothing is installed automatically. See [TESTING.md](TESTING.md) for the public verification contract and the [release procedure](RELEASING.md) for validation and publication requirements.

## License and contact

See [LICENSE](LICENSE), [NOTICE](NOTICE), and [third-party notices](Licenses/THIRD_PARTY_NOTICES.txt). Dependency licenses remain unchanged.

Donations: https://paypal.me/dgl1984
Contact: info@lanesaudio.com
