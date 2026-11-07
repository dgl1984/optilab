import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from package_macos import distribution_xml, PLUGIN_BUNDLES
from stage_macos import validate_public_tree, NAMES

class PackagingTests(unittest.TestCase):
    def test_installer_choices_and_platforms(self):
        packages = [("com.example." + str(i), str(i) + ".pkg", title) for i, (_, _, title) in enumerate(PLUGIN_BUNDLES)]
        root = ET.fromstring(distribution_xml("1.6.0", packages))
        self.assertEqual(root.find("options").get("hostArchitectures"), "arm64,x86_64")
        self.assertEqual(len(root.findall("choice")), 3)
        self.assertEqual(root.find("os-version").get("min"), "12.0")
    def test_stage_requires_bundles_and_rejects_windows_binary(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for name in NAMES:
                binary = root / name / "Contents/MacOS/OptiLab Core"
                binary.parent.mkdir(parents=True)
                binary.write_bytes(b"test")
            for name in ("docs", "licenses", "Effects"):
                (root / name).mkdir()
            for name in ("LICENSE", "NOTICE", "CHANGELOG.md"):
                (root / name).write_text("test")
            validate_public_tree(root)
            (root / "docs/accidental.dll").write_bytes(b"test")
            with self.assertRaises(ValueError):
                validate_public_tree(root)
            (root / "docs/accidental.dll").unlink()
            (root / NAMES[0] / "Contents/MacOS/OptiLab Core").unlink()
            with self.assertRaises(ValueError):
                validate_public_tree(root)
