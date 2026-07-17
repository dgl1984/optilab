# Contributing to OptiLab Core

Thank you for your interest in contributing. OptiLab Core is a small, focused
project. Contributions that fit the existing scope are welcome; larger design
changes should be discussed in an issue first.

## What is in scope

- Bug fixes for the JSFX, C++ engine, Winamp adapter, or CI.
- Improvements to accessibility, documentation, or test coverage.
- Host compatibility fixes for Winamp, RadioBOSS, StationPlaylist Studio, or
  other Winamp DSP–compatible broadcast applications.

## What to discuss first

- New controls or new modes.
- Changes to the native C++ API surface.
- Changes that affect the sound of the processor in any mode.

Open a GitHub issue and describe the motivation before writing code for any of
the above.

## Build requirements

- Windows 10 or later.
- Visual Studio 2022 with the **Desktop development with C++** workload.
- CMake 3.20 or newer (bundled with Visual Studio 2022).

## Building

Build the 32-bit Winamp DSP (required for plug-in testing):

```powershell
cmake --preset vs2022-win32 -S native
cmake --build native/build-winamp --config Release
ctest --test-dir native/build-winamp -C Release --output-on-failure
```

Build the 64-bit engine and run all tests:

```powershell
cmake --preset vs2022-x64 -S native
cmake --build native/build --config Release
ctest --test-dir native/build -C Release --output-on-failure
```

The packaging script produces the release ZIP and SHA-256 file:

```powershell
./scripts/package-winamp.ps1
```

## Tests

The project includes three test executables:

- `optilab-core-tests` — Core API and processing behavior.
- `optilab-winamp-pcm-tests` — PCM integer-conversion paths.
- `optilab-winamp-smoke-tests` — DLL loading, export presence, and version
  metadata (Win32 build only).

Run `ctest --output-on-failure` in the relevant build directory. All tests must
pass before a pull request is merged.

## Code style

- C++17 with `/W4 /permissive-` (MSVC) or `-Wall -Wextra -Wpedantic` (GCC/Clang).
- No dynamic CRT dependency in the Release Winamp DLL.
- No memory allocation or lock acquisition in any `process*` method.
- Match the indentation and naming style of the surrounding code.

## Documentation

- Update `CHANGELOG.md` under a new version heading for any user-visible change.
- If your change affects installation, controls, or accessibility, update the
  relevant `.md` file in the repository root or `native/`.
- Keep all user-facing text in plain English. Avoid jargon that a non-audio
  developer would not understand.

## Pull requests

- Target the `main` branch.
- Keep pull requests focused. One logical change per PR makes review faster.
- The CI must pass. The DLL architecture, export, and runtime-dependency checks
  are enforced automatically.

## Questions

Open a GitHub issue or email `info@lanesaudio.com`.
