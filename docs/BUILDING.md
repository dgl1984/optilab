# Building on Windows

Requirements:

- Windows 10 or later.
- Visual Studio 2022 with Desktop development with C++ and a Windows SDK.
- CMake 3.20 or later, Git, and 7-Zip (`7z`) on PATH.
- Windows PowerShell 5.1 and internet access for the pinned dependencies.

Run `BUILD_WINDOWS.bat` from the repository root. Python is not required. The script verifies the JUCE archive and foobar SDK and checks out the pinned CLAP wrapper/submodules. It configures CTest, builds x64 VST3/CLAP/foobar/CLI plus Win32 Winamp/foobar/CLI, runs the public regression set in both architectures, creates the binary release payload, and stages a scrubbed public-source archive with SHA-256 checksums.

The full transcript is written to `BUILD_WINDOWS_LOG.txt`; prior transcripts are retained under `validation-logs`. These logs are generated locally and are intentionally excluded from the public-source archive. Nothing is installed automatically. Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_WINDOWS.ps1 -Incremental` to reuse build objects; default builds start clean.

Current product sources, build files, and the public regression tests are included. Private fixtures and release-review-only audits are not distributed. No CI workflow is required to reproduce the public test layer. The maintainer/review tree can add deeper tests on top of the same CMake configuration; its CP13 automated results passed on 2026-10-25, with REAPER host, accessibility and listening acceptance subsequently confirmed by the maintainer on October 29, 2026.

The WAV command-line tool is built in both architectures by the batch entry point. It can also be built separately with `cmake --build build-windows-x64 --config Release --target optilab-core-cli` after configuration. It exposes the same three controls; run it without arguments for usage.

CLI exports retain the input frame count: processing latency is flushed, then removed from the front. This includes the delayed end of the input; it does not append an indefinite filter-decay tail.

The public CTest layer includes Core lifecycle/fixed-latency checks, behavior regression, WAV parser hardening, Winamp PCM/delivery checks, and wrapper checks when those targets are available. See `../TESTING.md` for the public verification contract. Configure with `-DBUILD_TESTING=ON` to enable them. A maintainer checkout may add additional release-review tests. Configure with `-DOPTILAB_REQUIRE_MAINTAINER_TESTS=ON`, or run `BUILD_WINDOWS.ps1 -RequireMaintainerTests`, when that release-review layer is required.

The review package may also provide `maintainer/VALIDATE_CP13.ps1` and `maintainer/fixtures`. Those assets extend the same build rather than replacing the public tests. Do not treat the public test layer alone as complete release certification.

The complete gate builds and stages all supported Windows formats and runs the
public regressions plus the maintainer release-review layer when supplied. Winamp tests run
in both architectures; the delivered Winamp plug-in is Win32. JUCE parameter,
processing and state checks run in x64.

Host-specific state/session recall, screen-reader announcements and focus, keyboard
behavior, native Winamp/foobar dialogs, live automation and listening acceptance
require manual verification. The allocation counters are targeted C++ new/new[]
checks, not proof of every allocation API or a thread-sanitizer run.

## Public regression coverage

The default build includes generated-signal Core behavior tests: the Stream tone
cap and timing, prepared live-update C++ allocation checks, sample-exact latency
compensation, supported rates, mode transitions, reset, planar/interleaved parity
and sanitization. It also includes WAV parsing, Winamp PCM/delivery and processing
chain contract checks, plus JUCE parameter, processing and state tests when that
target is enabled. These tests need no maintainer-only recording or fixture directory.
Numbered public fixtures are included where required by Core regressions; a maintainer checkout may extend that coverage.
The Winamp contract test does not load a host DLL; the JUCE test does not establish
keyboard or screen-reader acceptance. Manual host and listening gates still apply.

The public parameter catalogue contains only the runtime values needed by Core.
Core-only tuning state is kept outside that catalogue and is not host-automatable.
The three host controls and their saved-state identifiers are unchanged.

VST3 and CLAP silence output and report zero latency outside Core's supported
8 kHz to 768 kHz rate range. Returning to a supported rate prepares Core normally.
State restoration notifies the host to refresh parameter values.


## JSFX companion regression

`BUILD_WINDOWS.bat` also validates and packages `Effects/optilab_core.jsfx`.
`VALIDATE_JSFX.bat` runs the companion gate separately. It fetches the pinned
ysfx runtime, builds a headless Win32 test executable with Visual Studio 2022,
and compares generated-signal output against the unchanged native Core.
No maintainer-only fixture or recording is required.

The standard gate checks all 27,216 numbered values across 324 settings and
705 audio/state cases plus 27 legacy-session recall cases. Run
`VALIDATE_JSFX.bat -Thorough` for the 1,275-case extended matrix plus those 27
legacy cases (1,302 audio comparisons in total).
Reports, source hashes, the runtime revision and the complete transcript are
saved under `build-jsfx-validation`. That directory is excluded from source
packages. `scripts/validate-jsfx.ps1 -YsfxSourceDirectory <path>` can reuse an
unmodified checkout at the pinned revision instead of fetching it again.

Actual REAPER host, accessibility and listening acceptance remain separate
release gates. See [JSFX notes](JSFX.md).
