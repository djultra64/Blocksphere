"""ROM-free deterministic M1 diagnostic package contracts."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.binary = self.root / "tetrisphere-m1"
        self.binary.write_bytes(b"native-binary")
        self.runtime = self.root / "runtime.so"
        self.runtime.write_bytes(b"runtime")
        self.notice = self.root / "THIRD_PARTY_NOTICES.md"
        self.notice.write_text("M1_LICENSE_BUNDLE_COMPLETE=1\nPinned licenses.\n",
                               encoding="utf-8")
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps({
            "schema_version": 1,
            "build": {"id": "a" * 64, "source_commit": "b" * 40,
                      "platform": "linux-x86_64",
                      "binary_sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest(),
                      "runtime_sha256": {
                          self.runtime.name: hashlib.sha256(self.runtime.read_bytes()).hexdigest()
                      }},
            "run": {"build_id": "a" * 64, "platform": "linux-x86_64",
                    "native": True, "emulator": False,
                    "compatibility_layer": False},
        }), encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def test_create_is_deterministic_and_auditable(self):
        from tools.qa.package_m1 import audit_package, create_package
        outputs = [self.root / f"package-{index}.tar.gz" for index in range(2)]
        for output in outputs:
            create_package("linux-x86_64", self.binary, [self.runtime],
                           self.manifest, self.notice, output)
            result = audit_package(output)
            self.assertEqual(result["platform"], "linux-x86_64")
            self.assertEqual(result["build_id"], "a" * 64)
        digests = [hashlib.sha256(path.read_bytes()).hexdigest() for path in outputs]
        self.assertEqual(digests[0], digests[1])

    def test_audit_rejects_rom_magic_suffix_private_paths_and_credentials(self):
        from tools.qa.package_m1 import audit_package
        bad_values = (
            ("game.z64", bytes.fromhex("80371240") + b"x" * 16),
            ("notes.txt", b"/var/home/person/private/.local/m0/game.z64"),
            ("config.txt", b"password=hunter2"),
            ("hidden.env", b"PASSWORD=hunter2"),
            ("deploy.key", b"BEGIN OPENSSH PRIVATE KEY"),
            ("opaque.bin", b"prefix" + bytes.fromhex("80371240") + b"payload"),
        )
        for name, payload in bad_values:
            archive = self.root / (name.replace(".", "-") + ".tar.gz")
            with tarfile.open(archive, "w:gz") as output:
                fixture = self.root / "fixture"
                fixture.write_bytes(payload)
                output.add(fixture, arcname=name)
            with self.subTest(name=name), self.assertRaises(ValueError):
                audit_package(archive)

    def test_create_rejects_binary_or_runtime_not_bound_to_evidence(self):
        from tools.qa.package_m1 import create_package
        value = json.loads(self.manifest.read_text(encoding="utf-8"))
        for mutation in ("binary", "runtime"):
            candidate = json.loads(json.dumps(value))
            if mutation == "binary":
                candidate["build"]["binary_sha256"] = "f" * 64
            else:
                candidate["build"]["runtime_sha256"][self.runtime.name] = "f" * 64
            self.manifest.write_text(json.dumps(candidate), encoding="utf-8")
            with self.subTest(mutation=mutation), self.assertRaisesRegex(
                    ValueError, "hash"):
                create_package("linux-x86_64", self.binary, [self.runtime],
                               self.manifest, self.notice, self.root / "bad.tar.gz")
        self.manifest.write_text(json.dumps(value), encoding="utf-8")

    def test_audit_pair_requires_exactly_two_packages(self):
        from tools.qa.package_m1 import audit_pair
        with self.assertRaisesRegex(ValueError, "exactly two"):
            audit_pair([{"platform": "linux-x86_64"}])

    def test_pair_rejects_mismatched_build_or_source_identity(self):
        from tools.qa.package_m1 import audit_pair
        left = {"platform": "linux-x86_64", "build_id": "a" * 64,
                "source_commit": "b" * 40}
        for field, value in (("build_id", "c" * 64), ("source_commit", "d" * 40)):
            right = {"platform": "windows-x86_64", "build_id": "a" * 64,
                     "source_commit": "b" * 40, field: value}
            with self.subTest(field=field), self.assertRaises(ValueError):
                audit_pair([left, right])

    def test_required_m1_delivery_documents_exist(self):
        for relative in (
            "docs/research/feasibility.md",
            "docs/research/m1-symbol-map.md",
            "docs/research/m1-compatibility-backlog.md",
            "docs/research/m1-third-party-notices.md",
            "docs/milestones/M1.md",
        ):
            self.assertTrue((ROOT / relative).is_file(), relative)

    def test_license_bundle_is_deterministic_and_contains_full_texts(self):
        from tools.qa.build_m1_notices import build_notice_bundle
        dependency = self.root / "dependency"
        dependency.mkdir()
        (dependency / "LICENSE").write_text("Complete license A\n", encoding="utf-8")
        nested = dependency / "contrib"
        nested.mkdir()
        (nested / "COPYING.txt").write_text("Complete license B\n", encoding="utf-8")
        first = build_notice_bundle({"fixture": dependency})
        second = build_notice_bundle({"fixture": dependency})
        self.assertEqual(first, second)
        self.assertIn("M1_LICENSE_BUNDLE_COMPLETE=1", first)
        self.assertIn("Complete license A", first)
        self.assertIn("Complete license B", first)
        self.assertNotIn(str(self.root), first)


if __name__ == "__main__":
    unittest.main()
