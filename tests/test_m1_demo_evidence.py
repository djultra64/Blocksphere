"""Contracts for a single-run native Linux M1 demonstration."""
from __future__ import annotations

import array
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import wave
import shlex

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class EvidenceFixture:
    def __init__(self, root: Path, source_commit: str = "1" * 40):
        self.root = root
        self.binary = root / "tetrisphere-m1"
        self.binary.write_bytes(b"native diagnostic binary")
        self.before = root / "before.png"
        self.before.write_bytes(b"\x89PNG\r\n\x1a\nbefore-game-frame")
        self.after = root / "after.png"
        self.after.write_bytes(b"\x89PNG\r\n\x1a\nafter-visible-change")
        self.audio = root / "audio.wav"
        rate = 8_000
        samples = array.array("h", [600] * (rate * 2 * 2) + [6000] * (rate * 2))
        with wave.open(str(self.audio), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(samples.tobytes())
        self.log = root / "native.jsonl"
        build_id = "a" * 64
        run_id = "fixture-run"
        records = [
            {"event": "rom_validated", "rom_id": "tetrisphere-us-rev0", "run_id": run_id},
            {"event": "diagnostic_start", "build_id": build_id, "run_id": run_id},
            {"event": "native_linux_started", "backend": "rt64-vulkan",
             "controller_family": "xbox", "prompt_confirm": "A", "run_id": run_id},
            {"event": "rt64_device_ready", "api": "vulkan", "run_id": run_id},
            {"event": "rsp_task_submit", "type": 2, "ucode": "0x800DE7D0", "run_id": run_id},
            {"event": "rt64_present", "sequence": 1, "run_id": run_id},
            {"event": "logical_input_sample", "family": "keyboard",
             "buttons": "0x8000", "prompt_confirm": "Z", "run_id": run_id},
        ]
        self.log.write_text("Device Name: test Vulkan adapter\n" +
                            "".join(json.dumps(record) + "\n" for record in records),
                            encoding="utf-8")
        self.receipt = root / "rt64-verification.json"
        self.receipt.write_text(json.dumps({"commit": "3" * 40}) + "\n",
                                encoding="utf-8")
        markers = ["rom_validated", "native_linux_started", "game_screen_presented",
                   "playable_scene_entered", "logical_input_consumed",
                   "rt64_gpu_presented", "music_output", "effect_output"]
        details = {
            "rom_validated": {"rom_id": "tetrisphere-us-rev0", "exact": True},
            "native_linux_started": {"backend": "native", "executable": "tetrisphere-m1",
                                     "controller_family": "keyboard"},
            "game_screen_presented": {"source": "game", "screen": "title"},
            "playable_scene_entered": {"source": "game", "scene": "rescue"},
            "logical_input_consumed": {"visible_change": True, "action": "confirm",
                                       "input_family": "keyboard", "prompt_label": "Z",
                                       "original_game_prompt": "N64 A"},
            "rt64_gpu_presented": {"backend": "rt64-vulkan", "full_sync": True},
            "music_output": {"backend": "sdl-device", "non_silent": True,
                             "window_seconds": [0.0, 2.0], "rms": 600.0, "peak": 600},
            "effect_output": {"backend": "sdl-device", "distinct_transient": True,
                              "window_seconds": [2.0, 3.0], "rms": 6000.0, "peak": 6000},
        }
        self.manifest = {
            "schema_version": 1,
            "build": {"id": build_id, "source_commit": source_commit,
                      "platform": "linux-x86_64", "binary_sha256": digest(self.binary),
                      "rt64_commit": "3" * 40,
                      "rt64_receipt_sha256": digest(self.receipt)},
            "run": {"id": "fixture-run", "build_id": build_id,
                    "platform": "linux-x86_64", "native": True,
                    "emulator": False, "compatibility_layer": False},
            "events": [{"sequence": index + 1, "marker": marker, "build_id": build_id,
                        "run_id": "fixture-run", "details": details[marker]}
                       for index, marker in enumerate(markers)],
            "artifacts": {
                "screenshot_before": {"path": self.before.name, "sha256": digest(self.before)},
                "screenshot": {"path": self.after.name, "sha256": digest(self.after)},
                "audio": {"path": self.audio.name, "sha256": digest(self.audio)},
                "event_log": {"path": self.log.name, "sha256": digest(self.log)},
            },
        }

    def validate(self, value=None):
        from tools.qa.run_m1_demo import validate_evidence
        return validate_evidence(
            self.manifest if value is None else value, evidence_root=self.root,
            binary=self.binary, source_commit=self.manifest["build"]["source_commit"],
            rt64_receipt=self.receipt)


class DemoEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.fixture = EvidenceFixture(Path(self.temp.name))

    def tearDown(self):
        self.temp.cleanup()

    def test_accepts_real_files_and_contrasts_native_log_audio_and_input_family(self):
        result = self.fixture.validate()
        self.assertEqual(result["event_count"], 8)
        self.assertEqual(result["input_family"], "keyboard")

    def test_requires_explicit_evidence_inputs(self):
        from tools.qa.run_m1_demo import validate_evidence
        with self.assertRaisesRegex(ValueError, "explicit evidence root"):
            validate_evidence(self.fixture.manifest)

    def test_rejects_mutated_artifact_and_declared_hash(self):
        self.fixture.after.write_bytes(self.fixture.after.read_bytes() + b"mutation")
        with self.assertRaisesRegex(ValueError, "artifact hash mismatch"):
            self.fixture.validate()
        self.fixture.manifest["artifacts"]["screenshot"]["sha256"] = digest(self.fixture.after)
        self.fixture.manifest["artifacts"]["audio"]["sha256"] = "f" * 64
        with self.assertRaisesRegex(ValueError, "artifact hash mismatch"):
            self.fixture.validate()

    def test_rejects_log_build_or_required_runtime_marker_mutation(self):
        for old, new in (("a" * 64, "b" * 64), ("rt64_present", "fake_present")):
            with self.subTest(new=new):
                original = self.fixture.log.read_text(encoding="utf-8")
                self.fixture.log.write_text(original.replace(old, new), encoding="utf-8")
                self.fixture.manifest["artifacts"]["event_log"]["sha256"] = digest(self.fixture.log)
                with self.assertRaisesRegex(ValueError, "native event log"):
                    self.fixture.validate()
                self.fixture.log.write_text(original, encoding="utf-8")
                self.fixture.manifest["artifacts"]["event_log"]["sha256"] = digest(self.fixture.log)

    def test_rejects_native_log_from_a_different_run(self):
        original = self.fixture.log.read_text(encoding="utf-8")
        self.fixture.log.write_text(original.replace("fixture-run", "other-run"), encoding="utf-8")
        self.fixture.manifest["artifacts"]["event_log"]["sha256"] = digest(self.fixture.log)
        with self.assertRaisesRegex(ValueError, "run identity"):
            self.fixture.validate()

    def test_rejects_source_binary_and_rt64_receipt_mismatch(self):
        for field, value, message in (("source_commit", "9" * 40, "source commit"),
                                      ("binary_sha256", "9" * 64, "binary hash"),
                                      ("rt64_commit", "9" * 40, "RT64 receipt")):
            manifest = copy.deepcopy(self.fixture.manifest)
            manifest["build"][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
                self.fixture.validate(manifest)

    def test_rejects_keyboard_claim_as_xbox_and_wrong_prompt_label(self):
        for marker, field, value in (("native_linux_started", "controller_family", "xbox"),
                                     ("logical_input_consumed", "input_family", "xbox"),
                                     ("logical_input_consumed", "prompt_label", "A")):
            manifest = copy.deepcopy(self.fixture.manifest)
            event = next(item for item in manifest["events"] if item["marker"] == marker)
            event["details"][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "input family"):
                self.fixture.validate(manifest)

    def test_rejects_audio_claim_not_matching_wav(self):
        manifest = copy.deepcopy(self.fixture.manifest)
        event = next(item for item in manifest["events"] if item["marker"] == "effect_output")
        event["details"]["rms"] = 9000.0
        with self.assertRaisesRegex(ValueError, "audio metric"):
            self.fixture.validate(manifest)

    def test_rejects_reordered_or_missing_required_markers(self):
        manifest = copy.deepcopy(self.fixture.manifest)
        manifest["events"][2], manifest["events"][3] = manifest["events"][3], manifest["events"][2]
        with self.assertRaisesRegex(ValueError, "sequence|ordered marker"):
            self.fixture.validate(manifest)
        manifest = copy.deepcopy(self.fixture.manifest)
        manifest["events"] = manifest["events"][:-1]
        with self.assertRaisesRegex(ValueError, "missing required marker"):
            self.fixture.validate(manifest)

    def test_cli_requires_real_explicit_paths_and_does_not_rewrite_manifest(self):
        source_commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        fixture = EvidenceFixture(self.fixture.root, source_commit)
        path = self.fixture.root / "manifest.json"
        payload = json.dumps(fixture.manifest, sort_keys=True) + "\n"
        path.write_text(payload, encoding="utf-8")
        command = [sys.executable, "tools/qa/run_m1_demo.py", "validate", str(path),
                   "--evidence-root", str(self.fixture.root), "--binary", str(fixture.binary),
                   "--source-root", str(ROOT), "--rt64-receipt", str(fixture.receipt)]
        proc = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn('"valid": true', proc.stdout)
        self.assertEqual(path.read_text(encoding="utf-8"), payload)


class LinuxProtocolTests(unittest.TestCase):
    def test_viewer_launcher_is_a_valid_read_only_shortcut(self):
        launcher = ROOT / "tools/qa/view_m1_linux.sh"
        self.assertTrue(launcher.is_file())
        text = launcher.read_text(encoding="utf-8")
        self.assertIn(".local/m1/build-linux/tetrisphere-m1", text)
        self.assertIn(".local/m0/tetrisphere-us.z64", text)
        self.assertNotIn("cmake", text)
        proc = subprocess.run(["bash", "-n", str(launcher)], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_clean_checkout_protocol_names_exact_generation_and_validation_commands(self):
        protocol = (ROOT / "docs/qa/m1-linux-protocol.md").read_text(encoding="utf-8")
        required = (
            "python3 tools/build/fetch_dependencies.py --root .local/deps",
            "cmake -S .local/deps/N64Recomp -B .local/m1/toolchain/n64recomp-build -G Ninja",
            "python3 -m tools.recomp.generate generate",
            "--output .local/m1/linux-generated",
            "--manifest .local/m1/linux-generated-manifest.json",
            "--recompiler .local/m1/toolchain/n64recomp-build/N64Recomp",
            "-DTETRISPHERE_GENERATED_DIR=.local/m1/linux-generated/functions",
            "-DTETRISPHERE_GENERATION_MANIFEST=.local/m1/linux-generated-manifest.json",
            "git worktree add /tmp/tetrisphere-m1-build-source a7c369f8",
            ".local/m1/evidence/linux-final/manifest.json",
            "--evidence-root .local/m1/evidence/linux-final",
            "--binary .local/m1/build-linux/tetrisphere-m1",
            "--source-root /tmp/tetrisphere-m1-build-source",
            "--rt64-receipt .local/m1/build-linux/rt64-verification.json",
        )
        for text in required:
            with self.subTest(text=text):
                self.assertIn(text, protocol)

    def test_protocol_python_entrypoints_and_documented_private_inputs_exist(self):
        for command in (
            [sys.executable, "tools/build/fetch_dependencies.py", "--help"],
            [sys.executable, "-m", "tools.recomp.generate", "generate", "--help"],
            [sys.executable, "tools/qa/run_m1_demo.py", "validate", "--help"],
        ):
            with self.subTest(command=shlex.join(command)):
                proc = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(proc.returncode, 0, proc.stderr)
        for path in ("config/recomp/audio-rsp.toml", "config/dependencies-m1.json",
                     "tools/build/fetch_dependencies.py", "tools/recomp/generate.py"):
            self.assertTrue((ROOT / path).is_file(), path)


if __name__ == "__main__":
    unittest.main()
