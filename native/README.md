# OptiLab Core Native

This directory contains the framework-independent C++17 OptiLab Core engine,
native plug-in wrappers, command-line validation tool, and native tests.

## Structure

- `core/`: reusable real-time DSP engine and public C++ header
- `plugins/clap/`: 64-bit Windows CLAP wrapper and responsive editor
- `plugins/winamp/`: classic Winamp DSP ABI, PCM conversion, and settings UI
- `tools/cli/`: WAV command-line validation tool
- `tests/`: Core API, adapter, plug-in loading, and accessibility smoke tests
- `third_party/`: pinned external SDK headers and their licenses

See [`API.md`](API.md) for the supported native C++ API, call order,
parameters, latency behavior, and integration notes.

The WAV utility under `tools/cli` is retained for developer validation. It is
not packaged or supported as a public application.

The real-time processing methods allocate no memory and acquire no locks.

## Windows build

Visual Studio 2022 with the Desktop C++ workload and CMake 3.20 or newer are
recommended.

Build the 32-bit Winamp DSP:

```powershell
cmake --preset vs2022-win32 -S native
cmake --build native/build-winamp --config Release
ctest --test-dir native/build-winamp -C Release --output-on-failure
```

The DLL is written to:

```text
native/build-winamp/Release/dsp_optilab_core.dll
```

Build the 64-bit Core, CLAP, command-line tool, and tests:

```powershell
cmake --preset vs2022-x64 -S native
cmake --build native/build --config Release
ctest --test-dir native/build -C Release --output-on-failure
```

See [`WINAMP.md`](WINAMP.md) for plug-in installation and compatibility details.
See [`CLAP.md`](CLAP.md) for CLAP build, installation, accessibility, and
testing details.
See [`../docs/CLAP_ACCESSIBILITY.md`](../docs/CLAP_ACCESSIBILITY.md) before
changing the CLAP parameter interface, native controls, or host window
parenting.
