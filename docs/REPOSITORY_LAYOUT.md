# Repository layout

- native/core/: shared Core coordinator.
- native/engine/: internal processing engine used by Core.
- native/plugins/juce/: VST3/CLAP processor and editor.
- native/plugins/winamp/: Winamp-compatible adapter.
- native/plugins/foobar2000/: foobar2000 adapter.
- native/tools/cli/: WAV processing command-line tool.
- native/cmake/: generated-version template.
- scripts/: dependency, validation, and packaging helpers.
- tests/: public, reproducible regression tests.
- docs/: user, build, and accessibility documentation.
- Licenses/: dependency notices.
- maintainer/: optional fixture-dependent review regressions and scripts, excluded from the public-source archive.

Dependencies, build trees, validation logs, and staged artifacts are generated locally and excluded from version control. `dist` holds binary release staging; `dist-source` holds public-source staging.
