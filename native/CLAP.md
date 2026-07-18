# OptiLab Core CLAP

The CLAP target is a 64-bit Windows plug-in built around the shared
`optilab-core` DSP library. Its descriptor uses vendor `LanesAudio` and stable
plug-in ID `com.lanesaudio.optilab-core`.

## Build And Test

From the repository root:

```powershell
cmake --preset vs2022-x64 -S native
cmake --build native/build --config Release
ctest --test-dir native/build -C Release --output-on-failure
```

The plug-in is written to:

```text
native/build/Release/OptiLab_Core.clap
```

Create a version-checked candidate ZIP and SHA-256 file with:

```powershell
./scripts/package-clap.ps1
```

CI verifies that the module is x64, exports `clap_entry`, and has no dynamic
Microsoft C/C++ runtime dependency before uploading the candidate artifact.
For version tags, the release job downloads that exact tested artifact and
publishes its ZIP with the installable JSFX and Winamp assets. Checksums and
developer documentation remain in CI/package staging rather than cluttering the
public release page.

The CLAP smoke test loads the built module through `clap_entry` and verifies:

- descriptor and extension discovery
- stereo audio processing and reported latency
- parameter names, automation flags, and readable values
- state save and restore
- responsive Win32 editor creation
- standard controls and explicit keyboard focus traversal

## Local Installation

After a successful Release build:

```powershell
./scripts/install-clap.ps1
```

The script installs the exact built file into the current user's standard CLAP
directory and verifies that the source and installed SHA-256 hashes match.
Restart or rescan the host after replacing a loaded plug-in.

## Accessibility

Mode, Input Drive, and Auto-Adapt are standard automatable CLAP parameters.
Hosts and accessibility extensions can expose them without opening the custom
editor.

The editor uses a native combo box, a standard read-only dB field backed by the
visual Input Drive trackbar, and a native percentage trackbar for Auto-Adapt.
Once focus is inside the editor, Tab and Shift+Tab move among all three
parameters. Arrow keys change values; Page Up and Page Down change Input Drive
by 1 dB, Home and End select its limits, and F6 returns focus to the host.
Activity meters are visual-only and do not replace the parameter interface.

In REAPER, the editor recognizes the host's CLAP wrapper and uses a sibling
window so keyboard focus, Tab navigation, and mouse hit testing reach the native
controls immediately. The host-level OSARA parameter path remains independent
of this custom-editor behavior.

The implementation rules, App2Clap references, and required regression tests
are documented in the
[CLAP accessibility contract](https://github.com/dgl1984/optilab/blob/main/docs/CLAP_ACCESSIBILITY.md).

## Rendering

The wrapper reports one constant latency for the active sample rate and pads
lower-latency modes internally. It implements CLAP offline-render notification,
disables visual-meter bookkeeping during offline rendering, and enables
flush-to-zero handling for inaudible denormal values.

Release builds use Visual C++ whole-program optimization. Changes intended only
to improve performance must retain matching rendered-output regression hashes
before they are accepted.
