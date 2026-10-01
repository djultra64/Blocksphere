"""Contracts for the native Windows M1 demonstration."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tests.test_m1_demo_evidence import EvidenceFixture, digest

ROOT = Path(__file__).resolve().parents[1]


class WindowsEvidenceFixture(EvidenceFixture):
    def __init__(self, root: Path):
        super().__init__(root)
        windows_binary = root / "tetrisphere-m1.exe"
        self.binary.rename(windows_binary)
        self.binary = windows_binary
        self.manifest["build"]["binary_sha256"] = digest(self.binary)
        self.manifest["build"]["platform"] = "windows-x86_64"
        self.manifest["run"].update({
            "platform": "windows-x86_64",
            "wine": False,
            "proton": False,
        })
        for event in self.manifest["events"]:
            if event["marker"] == "native_linux_started":
                event["marker"] = "native_windows_started"
                event["details"]["executable"] = "tetrisphere-m1.exe"
        records = []
        for line in self.log.read_text(encoding="utf-8").splitlines():
            if line.startswith("{"):
                record = json.loads(line)
                if record.get("event") == "native_linux_started":
                    record["event"] = "native_windows_started"
                records.append(record)
        self.log.write_text(
            "Device Name: test Vulkan adapter\n" +
            "".join(json.dumps(record) + "\n" for record in records),
            encoding="utf-8",
        )
        self.manifest["artifacts"]["event_log"]["sha256"] = digest(self.log)

    def validate_windows(self, value=None):
        from tools.qa.run_m1_demo import validate_evidence
        return validate_evidence(
            self.manifest if value is None else value,
            evidence_root=self.root,
            binary=self.binary,
            source_commit=self.manifest["build"]["source_commit"],
            rt64_receipt=self.receipt,
            expected_platform="windows-x86_64",
        )


class WindowsEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.fixture = WindowsEvidenceFixture(Path(self.temp.name))

    def tearDown(self):
        self.temp.cleanup()

    def test_accepts_native_windows_artifacts_from_one_build(self):
        result = self.fixture.validate_windows()
        self.assertTrue(result["valid"])
        self.assertEqual(result["platform"], "windows-x86_64")
        self.assertEqual(result["build_id"], "a" * 64)

    def test_rejects_platform_marker_or_build_identity_mismatch(self):
        for mutation, message in (
            (("build", "platform", "linux-x86_64"), "Windows"),
            (("run", "platform", "linux-x86_64"), "Windows"),
            (("run", "build_id", "b" * 64), "one native Windows run"),
        ):
            manifest = copy.deepcopy(self.fixture.manifest)
            section, field, value = mutation
            manifest[section][field] = value
            with self.subTest(section=section, field=field), self.assertRaisesRegex(
                    ValueError, message):
                self.fixture.validate_windows(manifest)

        manifest = copy.deepcopy(self.fixture.manifest)
        marker = next(event for event in manifest["events"]
                      if event["marker"] == "native_windows_started")
        marker["marker"] = "native_linux_started"
        with self.assertRaisesRegex(ValueError, "native_windows_started"):
            self.fixture.validate_windows(manifest)

    def test_rejects_wine_proton_emulator_or_compatibility_layer(self):
        for field in ("wine", "proton", "emulator", "compatibility_layer"):
            manifest = copy.deepcopy(self.fixture.manifest)
            manifest["run"][field] = True
            with self.subTest(field=field), self.assertRaisesRegex(
                    ValueError, "native Windows run"):
                self.fixture.validate_windows(manifest)


class WindowsProtocolTests(unittest.TestCase):
    def test_native_builds_strip_private_source_and_runtime_paths(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("CMAKE_SKIP_RPATH TRUE", cmake)
        self.assertIn("/experimental:deterministic", cmake)
        self.assertIn("/pathmap:${N64MODERN_RUNTIME_ROOT}", cmake)
        self.assertIn("/pathmap:${RT64_ROOT}", cmake)

    def test_windows_package_copies_rt64_shader_runtime(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        builder = (ROOT / "tools/build/build_windows.ps1").read_text(encoding="utf-8")
        for name in ("dxcompiler.dll", "dxil.dll"):
            with self.subTest(name=name):
                self.assertIn(name, cmake)
                self.assertIn(name, builder)

    def test_windows_builder_derives_commit_and_requires_clean_source(self):
        builder = (ROOT / "tools/build/build_windows.ps1").read_text(encoding="utf-8")
        self.assertIn("rev-parse HEAD", builder)
        self.assertIn("status --porcelain", builder)
        self.assertNotIn("[string]$SourceCommit", builder)

    def test_interactive_runner_binds_real_build_capture_and_input(self):
        runner = ROOT / "tools/qa/run_m1_windows.ps1"
        self.assertTrue(runner.is_file())
        text = runner.read_text(encoding="utf-8")
        for required in (
            "windows-build-receipt.json",
            "TETRISPHERE_AUDIO_CAPTURE_WAV",
            "SetForegroundWindow",
            "keybd_event",
            "CopyFromScreen",
            "native-windows-run.json",
            "Get-FileHash",
        ):
            with self.subTest(required=required):
                self.assertIn(required, text)
        self.assertNotIn("wine", text.lower())
        self.assertNotIn("proton", text.lower())

    def test_protocol_names_exact_build_run_and_validation_commands(self):
        protocol = (ROOT / "docs/qa/m1-windows-protocol.md").read_text(encoding="utf-8")
        for required in (
            "tools\\build\\build_windows.ps1",
            "tools\\qa\\run_m1_windows.ps1",
            "--platform windows-x86_64",
            "windows-build-receipt.json",
            "native-windows-run.json",
            "InteractiveToken",
        ):
            with self.subTest(required=required):
                self.assertIn(required, protocol)

if __name__ == "__main__":
    unittest.main()
