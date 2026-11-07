# OptiLab Core on macOS

Download **OptiLab-Core-1.6.0-macOS-Universal.dmg** from the
[Releases page](https://github.com/dgl1984/optilab/releases). The installer contains
Audio Unit, VST3 and CLAP versions for Apple Silicon and Intel Macs running macOS
12 or later. Choose the formats your host uses.

Open the disk image and run **Install OptiLab Core.pkg**. The installer places
Audio Units in `/Library/Audio/Plug-Ins/Components`, VST3 in
`/Library/Audio/Plug-Ins/VST3`, and CLAP in `/Library/Audio/Plug-Ins/CLAP`.
Restart or rescan your host if Core does not appear. The package is signed ad hoc,
not notarized by Apple. If macOS blocks it, review the blocked installer in
System Settings > Privacy & Security and allow it only if you trust its source.

Use Audio Unit in Logic or GarageBand. In REAPER or another compatible host,
choose VST3 or CLAP. Keep using the same format when reopening a saved project;
formats are separate host identities. To uninstall, remove the installed bundle
for each format you selected.

Core has the same Mode, Input Drive and Auto-Adapt controls on Mac and Windows.
Use Tab to move through the editor, arrow keys to adjust a focused slider, and
Enter or F2 to enter an exact value. The existing accessible control names and
values are shared with the Windows editor. See the [user guide](USER_GUIDE.md)
for how the controls shape the sound. The Effects folder contains the REAPER
JSFX companion; install it through REAPER separately if you want that format.
Winamp and foobar2000 components are Windows-only.

## Building and testing

Run `bash BUILD_MACOS.sh` with Xcode command-line tools, CMake, Git and Python 3
installed. Dependencies are pinned and verified by SHA-256. The build produces
universal arm64/x86_64 AU, VST3 and CLAP bundles, runs the shared public engine and
JUCE wrapper tests, and packages the results. Use a fresh build directory when
staging again; staging refuses to overwrite an existing portable folder.

CI also installs the packaged plug-ins, validates the AU with auval and the VST3
with pluginval, and retains the logs. Both architectures are checked with lipo;
execution tests run on the runner's native architecture. These checks do not
replace listening, real-host use, or VoiceOver testing on an actual Mac.

Before reporting VoiceOver compatibility as tested, check control names and
values, Tab order, exact entry and focus recovery in a real host, plus automation,
project save/reopen and playback across all three modes.

The Mac installer and build layout were adapted from Victor Tsaran's Tone Trace
macOS port, keeping the same installer approach across the products.
