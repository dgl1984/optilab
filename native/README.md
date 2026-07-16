# OptiLab Core Native

This directory contains the framework-independent C++17 OptiLab Core engine and
the classic Winamp DSP wrapper.

## Structure

- `src/OptiLabCore.*`: reusable real-time DSP engine
- `winamp/`: classic Winamp DSP ABI, PCM conversion, and accessible settings UI
- `tests/`: Core API, exhaustive PCM, and DLL loading tests

See [`API.md`](API.md) for the supported native C++ API, call order,
parameters, latency behavior, and integration notes.

The WAV utility source in `src` is retained for developer validation. It is not
packaged or supported as a public application.

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

Build the 64-bit native tests:

```powershell
cmake --preset vs2022-x64 -S native
cmake --build native/build --config Release
ctest --test-dir native/build -C Release --output-on-failure
```

See [`WINAMP.md`](WINAMP.md) for plug-in installation and compatibility details.
