#!/bin/bash
# Universal AU/VST3/CLAP build; packaging follows Victor Tsaran's Tone Trace port.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
BUILD="$ROOT/build-macos"
exec > >(tee "$ROOT/BUILD_MACOS_LOG.txt") 2>&1
python3 scripts/fetch-macos-deps.py
cmake -S native -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DOPTILAB_BUILD_JUCE_PLUGINS=ON -DOPTILAB_BUILD_WINAMP=OFF -DOPTILAB_BUILD_FOOBAR2000=OFF
cmake --build "$BUILD" --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir "$BUILD" --output-on-failure
for format in AU VST3 CLAP; do
  case "$format" in AU) suffix=component;; VST3) suffix=vst3;; CLAP) suffix=clap;; esac
  lipo "$BUILD/OptiLabCoreJuce_artefacts/Release/$format/OptiLab Core.$suffix/Contents/MacOS/OptiLab Core" -verify_arch arm64 x86_64
done
VERSION=$(sed -nE 's/^project\(optilab_core VERSION ([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' native/CMakeLists.txt)
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
python3 scripts/stage_macos.py --build "$BUILD"
python3 scripts/package_macos.py --portable "$BUILD/portable" --output-dir "$BUILD/release" --version "$VERSION"
echo "Mac packages ready in $BUILD/release. Nothing installed."
