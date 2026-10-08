import hashlib
import tempfile
import unittest
from pathlib import Path
import tarfile
import zipfile

from tools.qa.package_m6 import _validate_name, _validate_payload, audit_package, audit_pair, create_package


class PackageM6Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.files = {}
        for name in ("game", "config", "SDL2.dll", "dxcompiler.dll", "dxil.dll",
                     "libSDL2-2.0.so.0", "readme", "controls",
                     "troubleshooting", "release_notes", "notices"):
            path = self.root / name
            path.write_bytes((name + "\n").encode())
            self.files[name] = path
        self.commit = "a" * 40
        self.build_id = "b" * 64

    def make(self, platform, output, **kwargs):
        return create_package(
            platform=platform, binary=self.files["game"],
            configurator=self.files["config"],
            runtime_files=[self.files[name] for name in
                           (("SDL2.dll", "dxcompiler.dll", "dxil.dll")
                            if platform.startswith("windows") else ("libSDL2-2.0.so.0",))],
            documents={name: self.files[name] for name in
                       ("readme", "controls", "troubleshooting", "release_notes")},
            notices=self.files["notices"], source_commit=self.commit,
            build_id=self.build_id, output=output, **kwargs,
        )

    def test_linux_tar_is_deterministic_and_auditable(self):
        first = self.root / "first.tar.gz"
        second = self.root / "second.tar.gz"
        self.make("linux-x86_64", first)
        self.make("linux-x86_64", second)
        self.assertEqual(hashlib.sha256(first.read_bytes()).hexdigest(),
                         hashlib.sha256(second.read_bytes()).hexdigest())
        result = audit_package(first)
        self.assertEqual(result["source_commit"], self.commit)
        self.assertEqual(result["build_id"], self.build_id)
        with tarfile.open(first, "r:gz") as archive:
            names = {member.name.split("/", 1)[1] for member in archive.getmembers()}
        self.assertIn("tetrisphere", names)
        self.assertIn("README.md", names)
        self.assertIn("lib/libSDL2-2.0.so.0", names)
        self.assertIn("tetrisphere-config", names)
        self.assertNotIn("tools/tetrisphere-config", names)
        self.assertIn("docs/CONTROLS.md", names)
        self.assertIn("licenses/THIRD_PARTY_NOTICES.md", names)
        self.assertIn("assets/README.md", names)
        self.assertIn("gamecontrollerdb.txt", names)
        self.assertIn("licenses/controller-db/LICENSE", names)
        self.assertNotIn("run.sh", names)
        self.assertNotIn("config.sh", names)

    def test_windows_zip_and_pair_share_revision(self):
        linux = self.root / "linux.tar.gz"
        windows = self.root / "windows.zip"
        self.make("linux-x86_64", linux)
        self.make("windows-x86_64", windows)
        self.assertTrue(audit_pair([audit_package(linux), audit_package(windows)])["valid"])
        with zipfile.ZipFile(windows) as archive:
            names = {name.split("/", 1)[1] for name in archive.namelist()}
        self.assertIn("tetrisphere.exe", names)
        with zipfile.ZipFile(windows) as archive:
            self.assertTrue(all(name.startswith("tetrisphere-1.0-") for name in archive.namelist()))
        self.assertIn("README.md", names)
        self.assertIn("tetrisphere-config.exe", names)
        self.assertNotIn("tools/tetrisphere-config.exe", names)
        self.assertIn("lib/SDL2.dll", names)
        self.assertIn("lib/dxcompiler.dll", names)
        self.assertIn("lib/dxil.dll", names)
        self.assertFalse(any(name.endswith(".dll") and "/" not in name for name in names))
        self.assertNotIn("tools/SDL2.dll", names)
        self.assertIn("docs/CONTROLS.md", names)
        self.assertIn("licenses/THIRD_PARTY_NOTICES.md", names)
        self.assertIn("assets/README.md", names)
        self.assertIn("gamecontrollerdb.txt", names)
        self.assertIn("licenses/controller-db/LICENSE", names)

    def test_patch_release_version_matches_root_and_manifest(self):
        import json
        archive = self.root / "patch.tar.gz"
        self.make("linux-x86_64", archive, version="1.0.1")
        with tarfile.open(archive) as package:
            names = package.getnames()
            self.assertTrue(all(n.startswith("tetrisphere-1.0.1-linux-x86_64/") for n in names))
            metadata = json.load(package.extractfile("tetrisphere-1.0.1-linux-x86_64/docs/BUILD.json"))
        self.assertEqual(metadata["version"], "1.0.1")
        self.assertEqual(audit_package(archive)["version"], "1.0.1")

    def test_malformed_release_version_cannot_create_an_archive(self):
        for version in ("../1.0.1", "1.0", "v1.0.1", "1.0.1/evil"):
            with self.subTest(version=version), self.assertRaisesRegex(ValueError, "version"):
                self.make("linux-x86_64", self.root / "bad.tar.gz", version=version)

    def test_private_payload_is_rejected(self):
        self.files["readme"].write_text("/home/someone/private/rom.z64\n")
        with self.assertRaisesRegex(ValueError, "private"):
            self.make("linux-x86_64", self.root / "bad.tar.gz")

    def test_opposite_platform_instructions_are_rejected(self):
        self.files["readme"].write_text("On Windows, open tetrisphere.exe.\n")
        with self.assertRaisesRegex(ValueError, "other platform"):
            self.make("linux-x86_64", self.root / "wrong-linux.tar.gz")
        self.files["readme"].write_text("On Linux, run ./tetrisphere.\n")
        with self.assertRaisesRegex(ValueError, "other platform"):
            self.make("windows-x86_64", self.root / "wrong-windows.zip")

    def test_relative_build_diagnostic_is_not_private_content(self):
        _validate_payload("tetrisphere", b"./.local/m1/microcode/audio-rsp-recompiled.c")
        with self.assertRaisesRegex(ValueError, "unsafe"):
            _validate_name("tetrisphere/.local/rom.z64")


if __name__ == "__main__":
    unittest.main()
