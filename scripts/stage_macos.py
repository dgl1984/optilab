#!/usr/bin/env python3
"""Stage only the universal plug-in bundles, public guide and required licences."""
import argparse
import shutil
from pathlib import Path
MACOS_AU_NAME = "OptiLab Core.component"
MACOS_VST3_NAME = "OptiLab Core.vst3"
MACOS_CLAP_NAME = "OptiLab Core.clap"
NAMES = (MACOS_AU_NAME, MACOS_VST3_NAME, MACOS_CLAP_NAME)

def validate_public_tree(root):
    expected = set(NAMES) | {"docs", "licenses", "LICENSE", "NOTICE", "Effects"}
    if {p.name for p in root.iterdir()} != expected:
        raise ValueError("Unexpected staged package contents")
    for name in NAMES:
        if not (root / name / "Contents/MacOS/OptiLab Core").is_file():
            raise ValueError("Missing executable: " + name)
    for path in root.rglob("*"):
        if path.suffix.lower() in {".pdb", ".obj", ".lib", ".exe", ".dll"}:
            raise ValueError("Development or Windows binary in Mac package: " + str(path))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", type=Path, required=True)
    args = ap.parse_args()
    source = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    stage = build / "portable"
    # Refuse stale output instead of deleting anything supplied by the caller.
    stage.mkdir(parents=True, exist_ok=False)
    artifacts = build / "OptiLabCoreJuce_artefacts/Release"
    for fmt, name in zip(("AU", "VST3", "CLAP"), NAMES):
        shutil.copytree(artifacts / fmt / name, stage / name, symlinks=True)
    shutil.copytree(source / "Licenses", stage / "licenses")
    juce = source / "native/third_party/JUCE"
    shutil.copy2(juce / "LICENSE.md", stage / "licenses/JUCE_LICENSE.md")
    shutil.copy2(juce / "modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt",
                 stage / "licenses/VST3_SDK_LICENSE.txt")
    (stage / "docs").mkdir()
    for name in ("USER_GUIDE.md", "MACOS.md"):
        shutil.copy2(source / "docs" / name, stage / "docs" / name)
    shutil.copytree(source / "Effects", stage / "Effects")
    for name in ("LICENSE", "NOTICE"):
        shutil.copy2(source / name, stage / name)
    for name in NAMES:
        resources = stage / name / "Contents/Resources"
        resources.mkdir(parents=True, exist_ok=True)
        shutil.copytree(stage / "licenses", resources / "licenses")
        for item in ("LICENSE", "NOTICE"):
            shutil.copy2(stage / item, resources / item)
    validate_public_tree(stage)
    print(stage)

if __name__ == "__main__":
    main()
