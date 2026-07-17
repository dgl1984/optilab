# OptiLab Core Native API

OptiLab Core v1.1.2 includes a framework-independent C++17 static library named
`optilab-core`. It is the same processing engine used by the Winamp DSP wrapper.

This is a C++ API, not a stable C ABI. If you need to call OptiLab Core from C,
Rust, C#, Python, or another language, wrap this C++ class in a small adapter
owned by your project. A stable exported C API may be added in a later release,
but v1.1.2 does not promise one.

## Files

- `native/src/OptiLabCore.h`: public C++ API
- `native/src/OptiLabCore.cpp`: processor implementation
- `native/tests/OptiLabCoreTests.cpp`: small usage and regression tests

## Build target

The CMake target is:

```cmake
optilab-core
```

It builds a static library. Link it into your host, plug-in, command-line tool,
or adapter layer:

```cmake
add_subdirectory(native)
target_link_libraries(your_target PRIVATE optilab-core)
```

The target publishes `native/src` as an include directory, so consumers can use:

```cpp
#include "OptiLabCore.h"
```

## Processing model

Create one `OptiLabCore` instance per independent audio stream. Do not share a
single instance between unrelated streams.

Call order:

1. Construct `OptiLabCore`.
2. Call `prepare(sampleRate)`.
3. Set parameters with `setParameters(...)`.
4. Process audio with one of the process methods.
5. Call `reset()` when transport, seek, stream format, or stream identity
   changes.

Example:

```cpp
#include "OptiLabCore.h"

OptiLabCore core;
core.prepare(48000.0);

auto params = OptiLabCore::defaultParameters(OptiLabCore::Mode::StreamPolish);
params.inputDriveDb = 4.5;
params.autoAdaptPct = 25.0;
core.setParameters(params);

core.processInterleaved(samples, frameCount, 2);
```

## Parameters

`OptiLabCore::Parameters` contains the public controls:

```cpp
struct Parameters {
    Mode mode = Mode::PodcastLeveler;
    double inputDriveDb = 3.5;
    double autoAdaptPct = 0.0;
};
```

Modes:

```cpp
OptiLabCore::Mode::PodcastLeveler
OptiLabCore::Mode::StreamPolish
OptiLabCore::Mode::SmoothLimiter
```

Use `OptiLabCore::defaultParameters(mode)` to get the recommended starting
values for a mode. Current defaults:

- Podcast Leveler: `inputDriveDb = 3.5`, `autoAdaptPct = 0.0`
- Stream polish: `inputDriveDb = 4.5`, `autoAdaptPct = 0.0`
- Smooth Limiter: `inputDriveDb = 0.0`, `autoAdaptPct = 0.0`

Supported ranges:

- `inputDriveDb`: `-12.0` through `18.0`
- `autoAdaptPct`: `0.0` through `100.0`

The processor clamps out-of-range public values internally, but hosts should
clamp UI values before calling `setParameters`.

## Audio formats

The native engine processes 32-bit floating-point samples. Integer PCM
conversion is handled by the Winamp adapter in `native/winamp/WinampPcm.*`.

Available processing methods:

```cpp
std::pair<float, float> processSample(float left, float right);
void processPlanar(float* left, float* right, std::size_t frames);
void processInterleaved(float* samples, std::size_t frames, std::size_t channels);
```

`processPlanar` expects stereo buffers. `processInterleaved` supports one or two
channels. Unsupported channel counts are passed through unchanged.

For mono input, `processInterleaved(..., channels = 1)` processes the mono stream
through both sides of the stereo engine and writes a mono result.

## Latency

Call `latencySamples()` after `prepare()` and `setParameters()` to get the
current processor latency in samples. Hosts that support plug-in delay
compensation should report this value.

Latency can change when mode or Auto-Adapt changes because lookahead-related
stages are mode-dependent. If your host exposes latency to another system, check
the value after parameter changes.

## Real-time behavior

The process methods are intended for real-time audio use. They do not allocate
memory or acquire locks during processing.

Call `prepare`, `setParameters`, and `reset` from a safe host point. If your host
can change parameters from a non-audio thread, synchronize those changes before
calling into `OptiLabCore`; the class does not provide its own thread locking.

## Reset behavior

Use `reset()` when a stream discontinuity occurs:

- playback starts from a new position
- the host seeks
- the sample rate changes
- the input stream changes
- a host flushes or recreates its processing graph

Do not call `reset()` every block during continuous playback, or the leveling
and lookahead stages will not settle normally.

## Versioning note

The v1.1.2 C++ API is small and practical, but it should not yet be treated as a
long-term binary compatibility promise. Prefer rebuilding against the matching
release source rather than mixing headers and libraries from different OptiLab
Core versions.
