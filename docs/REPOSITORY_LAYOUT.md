# Repository Layout

OptiLab Core keeps source assets separate from generated builds, release
packages, installed plug-ins, and local analysis data.

## Source

```text
Effects/                     REAPER JSFX distributed by ReaPack/releases
native/
  core/                      Shared framework-independent DSP
  plugins/
    clap/                    CLAP ABI wrapper and responsive editor
    winamp/                  Winamp DSP wrapper and PCM adapter
  tools/cli/                 Developer WAV processing tool
  tests/                     Native unit and plug-in smoke tests
  third_party/               Pinned SDK headers and licenses
scripts/                     Build installation and packaging helpers
docs/                        Cross-format testing and repository documentation
```

`Effects/optilab_core.jsfx` remains at its REAPER-compatible path. Native
wrappers must use `native/core` rather than copying the DSP implementation.
Future native formats belong under `native/plugins/<format>/`.

GitHub's automatically generated source archives contain all tracked source,
tests, packaging and installation scripts, CI configuration, vendored SDK
headers and licenses, and project documentation for the tagged version.

## Generated And Local Data

The following directories are intentionally ignored by Git:

```text
native/build/                x64 CMake build tree
native/build-*/              Other architecture or toolchain build trees
dist/                        Staged release packages and checksums
graphify-out/                Local code graph, cache, and reports
.vs/                         Visual Studio local state
```

Installed test plug-ins live outside the repository in their host-standard
locations. They are reproduced from a build using scripts such as
`scripts/install-clap.ps1`; installed binaries are never treated as source.
Release ZIP files and the standalone JSFX download are published separately as
GitHub Release assets. Local Graphify output is analysis data, not project
source, and is never included in a source archive or release package.

## Ownership Rules

- Sound changes begin in the JSFX/native DSP pair and require regression tests.
- Format-specific state, host APIs, and editors stay inside their plug-in folder.
- Shared DSP code must not depend on CLAP, Winamp, Windows UI, or packaging code.
- Tests may depend on public target interfaces, not private build-directory
  paths except when loading the plug-in artifact under test.
- Vendored SDK content is pinned and documented; update it as one intentional
  change with its upstream license.
- Release notes are consolidated in `CHANGELOG.md` when a release is prepared,
  rather than accumulating candidate-build narration in source headers.
