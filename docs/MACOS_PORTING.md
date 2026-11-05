# Porting OptiLab Core to macOS

The intended Mac build uses the existing Core engine and JUCE editor to produce
Audio Unit, VST3, and CLAP plug-ins for Apple Silicon and Intel. This guide maps
the work needed in the current Windows build configuration. The Mac commands
below are starting points for the porter and still need execution on macOS.

## Start with the engine

Use a Mac with Xcode command-line tools and CMake 3.20 or newer. From the repository
root, this builds the portable engine, WAV command-line tool, and public tests
without fetching plug-in dependencies:

```sh
cmake -S native -B build-macos-core -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DOPTILAB_BUILD_JUCE_PLUGINS=OFF \
  -DOPTILAB_BUILD_WINAMP=OFF -DOPTILAB_BUILD_FOOBAR2000=OFF
cmake --build build-macos-core --parallel
ctest --test-dir build-macos-core --output-on-failure
```

Keep the first compiler or test failure and its full log. The public tests run
without private maintainer fixtures. See [testing](../TESTING.md) for coverage.

## Reuse Victor Tsaran's Mac build work

Victor (`vick08`) contributed universal Mac ports with installer packaging to
both projects below. They use JUCE 8.0.12 and the CLAP JUCE adapter revision
`55525c9858d4b25687be7759a5e0f70eccef218e`, matching OptiLab's dependency choices.

- [Tone Trace port, PR #11](https://github.com/dgl1984/ToneTrace/pull/11)
  contains `BUILD_MACOS.sh`, `scripts/fetch_deps.py`, `scripts/stage_release.py`,
  `scripts/package_macos.py`, and a macOS CI job. Its Mac job passed universal
  binary checks and installed-package Audio Unit validation before merge.
- [Space Trace port, PR #1](https://github.com/dgl1984/spacetrace/pull/1)
  also explains bundle resources, signing, installer layout, and Mac keyboard
  equivalents. Its PR records the testing scope and outstanding items.

Adapt the product names, paths, identifiers, test targets, and public-file list.
OptiLab's CMake project is under `native/`. Its dependency directories are under
`native/third_party/`. The existing dependency fetcher is Windows PowerShell and
downloads a Windows JUCE archive, so use the verified source-archive approach in
Victor's script for the Mac build.

## Enable the plug-ins

In [native/CMakeLists.txt](../native/CMakeLists.txt):

1. Extend the `OPTILAB_BUILD_JUCE_PLUGINS AND WIN32` platform condition to include
   Apple builds. A successful engine build currently produces no Mac plug-ins.
2. Set the macOS deployment target and `arm64;x86_64` architectures before
   `project()`. Victor's ports use macOS 12 as their minimum.
3. Add AU to `FORMATS` on Apple and enable the languages needed by JUCE's Apple
   wrappers. Follow the working Tone Trace configuration for the CLAP wrapper's
   Objective-C++ compilation if the pinned adapter needs it.
4. Build the existing `OptiLabCoreJuce` target and `optilab-juce-wrapper-test`.
   Keep Winamp and foobar2000 targets disabled in the Mac configuration.

Preserve the existing VST3/AU bundle identity `com.lanesaudio.optilabcore`,
manufacturer code `LnAu`, plug-in code `OpCr`, and CLAP ID
`com.lanesaudio.optilab-core`. Preserve the three host parameter IDs `mode`,
`inputDrive`, and `autoAdapt`, along with the saved-state format.

The editor and accessible value controls are in `native/plugins/juce/`.
Processing is in `native/core/` and `native/engine/`. One processor-specific
item to measure is `OptiLabDenormalGuard.h`: its floating-point optimization
currently operates on x86, so check Apple Silicon CPU use as audio decays to
silence. Treat measured compiler, correctness, or performance problems as focused
porting fixes.

## Finish the first working build

- Run the public Core and JUCE wrapper tests. Check all three modes, latency,
  rate changes, parameter updates, and project save/reload.
- Verify both executable slices in every bundle with `lipo -verify_arch arm64 x86_64`.
  Record which architecture actually executed the tests.
- Run `auval -v aufx OpCr LnAu`, pluginval for VST3, and clap-validator for CLAP.
- Exercise the three controls, exact value entry, focus, and announcements with
  VoiceOver in a Mac audio host, and listen to the processing in each mode.
- Adapt Victor's staging and DMG/PKG scripts, including OptiLab's `LICENSE`,
  `NOTICE`, dependency licenses, and user documentation. Ad-hoc signing supports
  development builds. Public notarization needs the maintainer's Developer ID
  identities and notarization credentials.
- After the first working build, add macOS CI using the same build entry point,
  public tests, bundle checks, and validator logs. Run Windows CI on shared-file
  changes as well.

Include the Mac model/architecture, macOS and Xcode versions, tested hosts,
validator results, and signing method in the port's pull request.
