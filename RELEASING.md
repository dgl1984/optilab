# Releasing OptiLab Core

Automated checks prepare a candidate; they do not approve publication.

## Prepare the source

Review README.md, CHANGELOG.md, RELEASE_NOTES.md and docs/USER_GUIDE.md together.
Confirm supported formats, install/upgrade guidance and version strings agree.
The version comes from native/CMakeLists.txt; generated headers and packages must
match it. Use a semantic tag such as `v1.5.0` and title `OptiLab Core 1.5.0` only
for the approved source.

Review the source diff and preserve the three host controls/state IDs, fixed mode
latency, stable numbered internal IDs and Winamp-only delivery guard. Changes to
these need explicit compatibility review. Do not renumber existing parameters.

Maintainer-only review fixtures, recordings, archives, logs,
cache and build products must stay outside public Git history. Inspect the exact
staged diff and tracked file list as well as `git status --short`: ignore rules
do not remove files already tracked.

## Build and test

Run `BUILD_WINDOWS.bat`. For private release review, require the fixture overlay:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_WINDOWS.ps1 -RequireMaintainerTests
```

The default public build runs generated-signal regressions without that overlay.
Use `-Incremental` to reuse compiled objects. Let the script finish independently
and retain BUILD_WINDOWS_LOG.txt and validation logs. Nothing is installed.

Require successful x64 and Win32 builds and every registered test to pass. Review
warnings and failures. Also build a fresh public-source copy without maintainer
assets. See [build instructions](docs/BUILDING.md) for coverage and its limits.
Do not describe processing-chain contract tests as live-host integration tests.

## Inspect the delivery files

- `dist/OptiLab-Core-<version>-Windows.zip`: x64 VST3/CLAP, x86 Winamp DSP,
  dual-architecture foobar2000 component, documentation and licences.
- `dist/foo_optilab_core-<version>.fb2k-component`: the same component separately
  available for installation through foobar2000 Preferences > Components.
- `dist-source/OptiLab-Core-<version>-Source.zip`: scrubbed public source, rooted at `OptiLab-Core/`.

Do not label the combined Windows package x64-only. Check the VST3 bundle is
complete. Inspect the component: root foo_optilab_core.dll must be x86 and
x64/foo_optilab_core.dll must be x64. Confirm it matches the copy in the ZIP.
Verify archive CRCs, `SOURCE_MANIFEST.sha256` and external SHA-256 files. Check local
documentation links. Exclude private maps/fixtures, generated/debug files and
internal reports. Preserve all dependency and licence notices.

Keep the product-first README, prominent download/manual links and useful release
notes consistent with ToneTrace and SpaceTrace. Keep checkpoint history in private
review evidence. Product-specific source organization and architectures can differ.

## Validate the JSFX companion

The companion is `Effects/optilab_core.jsfx`. The Windows batch runs its pinned
headless regression before staging it and checks that the packaged JSFX hash
matches the passing report. The native Core remains the reference.

Before publication, run `VALIDATE_JSFX.bat -Thorough`: it checks 324 coordinator
settings, 486 supported-rate/drive cases, 120 longer generated-signal cases and
63 live-update/state cases (42 state-transition cases plus 21 direct UI-write cases), 606 bidirectional slider-sweep cases, plus 27 legacy-session recall cases. Keep the
complete reports and runtime revision.
Check actual REAPER loading, rendering, automation, state recall, keyboard access
and listening as well. Headless success does not establish those host checks.
Keep maintainer-only review assets out of public source. See
[JSFX validation details](docs/JSFX.md).

## Complete host and listening acceptance

Record actual results with OS, host, format, architecture and screen-reader versions:

- Load the packaged VST3 and CLAP in supported hosts. Check defaults, automation,
  mode-switch latency, save/reopen, multiple instances, offline render, repeated
  creation/destruction and removal during playback.
- Run available VST3 and CLAP validators on those exact files. Record tool versions,
  failures and unavailable checks. Skipped checks are not passes.
- Check keyboard traversal, visible focus, exact entry, Home/End, screen-reader
  names/values and focus recovery in real hosts.
- Load the x86 Winamp DSP in compatible hosts, including RadioBOSS where supported.
  Check the native dialog, PCM formats, input calibration, delivery ceiling and
  settings recall. Do not alter host input scaling to obtain a pass.
- Install the component in both supported foobar2000 architectures. Check its
  dialog, presets, live changes, latency and end-of-stream draining.
- Listen to speech, music and transients across all modes and adaptation ranges,
  level-matched where practical. Obtain the maintainer's sound approval.

See [keyboard guidance](docs/CLAP_ACCESSIBILITY.md) and the [user guide](docs/USER_GUIDE.md).
Automated tests cannot replace these gates.

## Publish after approval

Review final documentation and source, commit only the approved public tree and
tag the source used to build the tested files. Do not commit the maintainer overlay.

Publish one combined Windows ZIP containing all formats, documentation, licenses and SHA256SUMS.txt. Commit the complete public source at the repository root and tag that commit so cloning and GitHub source archives include it. Do not attach a separate source ZIP or individual plug-ins. Release uploads are manual; the workflow is for manually requested build validation only.

After authorized publication, download each uploaded file and compare hashes with
the approved local copies. Confirm download/manual links and tag/source identity.
