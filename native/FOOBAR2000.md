# OptiLab Core for foobar2000

The foobar2000 component is a thin Windows adapter around the shared,
framework-independent `OptiLabCore` class. The adapter does not alter the DSP
implementation. It supplies foobar2000 preset storage, a native configuration
dialog, host sample conversion, latency reporting, and end-of-stream draining.

The component supports mono and stereo streams. Streams with more than two
channels pass through unchanged, matching the public OptiLab Core API contract.

## Prerequisites

- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.20 or newer
- 7-Zip on `PATH`

Install the pinned official foobar2000 SDK once:

```powershell
./scripts/setup-foobar2000-sdk.ps1
```

The script downloads SDK 2025-03-07 from foobar2000.org, verifies its SHA-256
hash, and extracts it to the git-ignored
`native/third_party/foobar2000-sdk` directory. A different extracted SDK path
can be supplied with CMake's `FOOBAR2000_SDK_ROOT` cache variable.

## Build and test both bitnesses

```powershell
cmake --preset vs2022-foobar2000-x64 -S native
cmake --build native/build-foobar2000-x64 --config Release
ctest --test-dir native/build-foobar2000-x64 -C Release --output-on-failure

cmake --preset vs2022-foobar2000-win32 -S native
cmake --build native/build-foobar2000-win32 --config Release
ctest --test-dir native/build-foobar2000-win32 -C Release --output-on-failure
```

The resulting DLLs are:

```text
native/build-foobar2000-x64/Release/foo_optilab_core.dll
native/build-foobar2000-win32/Release/foo_optilab_core.dll
```

Create one installable dual-bitness package:

```powershell
./scripts/package-foobar2000.ps1
```

The `.fb2k-component` archive contains the Win32 DLL at its root and the x64
DLL under `x64/`, matching the official foobar2000 dual-architecture package
layout. It also carries the OptiLab Core license, notice, and the required
foobar2000 SDK redistribution license.

## Configuration and processing

Add **OptiLab Core** to foobar2000's active DSP chain, then open its settings
to choose Mode, Input drive, and Auto-adapt. Input Drive is a non-editable
native dropdown with 0.1 dB choices and natural Up/Down navigation. Auto-Adapt
is a native trackbar slider with 1% arrow-key steps and 10% Page Up/Page Down
steps. Settings are stored in the DSP preset, so separate converter or playback
chains retain their own values.

The settings window is a resource-defined native Windows dialog. It preserves
standard dialog keyboard and accessibility behavior, including labeled native
controls, Tab order, Enter for OK, Escape for Cancel, and focus restoration to
foobar2000.
