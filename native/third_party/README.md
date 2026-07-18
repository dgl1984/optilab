# Third-Party Dependencies

Third-party source is kept here only when a native target must build without a
network download.

## CLAP

- Upstream: `https://github.com/free-audio/clap`
- Version: `1.2.10`
- Commit: `195b42a004144fab0b3cf95e9c067187d15365b7d`
- Local content: public headers under `clap/include/clap`
- License: `clap/LICENSE` (MIT)

Do not edit vendored CLAP headers as part of OptiLab feature work. Update the
entire pinned header set, version information, and license together.
