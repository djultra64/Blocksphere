"""Contracts for the N64ModernRuntime checkout used by the native binary."""

import json
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.build.verify_runtime import (
    source_identity,
    verify_dependency,
    verify_runtime,
)


class RuntimeCheckoutTests(unittest.TestCase):
    def make_checkout(self, root: Path) -> tuple[Path, Path]:
        checkout = root / "N64ModernRuntime"
        checkout.mkdir()
        subprocess.run(["git", "init", "-q", str(checkout)], check=True)
        subprocess.run(["git", "-C", str(checkout), "config", "user.email", "test@example.invalid"], check=True)
        subprocess.run(["git", "-C", str(checkout), "config", "user.name", "Runtime Test"], check=True)
        (checkout / "runtime.cpp").write_text("int runtime = 1;\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(checkout), "add", "runtime.cpp"], check=True)
        subprocess.run(["git", "-C", str(checkout), "commit", "-qm", "fixture"], check=True)
        commit = subprocess.check_output(
            ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
        lock = root / "dependencies.json"
        lock.write_text(json.dumps({
            "schema_version": 1,
            "dependencies": [{
                "name": "N64ModernRuntime",
                "repository": "https://github.com/N64Recomp/N64ModernRuntime",
                "commit": commit,
            }],
        }), encoding="utf-8")
        return checkout, lock

    def switch_to_clean_revision(self, checkout: Path, lock: Path) -> None:
        (checkout / "runtime.cpp").write_text("int runtime = 2;\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(checkout), "add", "runtime.cpp"], check=True)
        subprocess.run(["git", "-C", str(checkout), "commit", "-qm", "second fixture"], check=True)
        commit = subprocess.check_output(
            ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
        contents = json.loads(lock.read_text(encoding="utf-8"))
        contents["dependencies"][0]["commit"] = commit
        lock.write_text(json.dumps(contents), encoding="utf-8")

    def test_exact_clean_checkout_is_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            checkout, lock = self.make_checkout(Path(temp))
            receipt = verify_runtime(checkout, lock)
            self.assertEqual(receipt["commit"], subprocess.check_output(
                ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip())
            self.assertRegex(receipt["source_identity"], r"^[0-9a-f]{64}$")
            self.assertEqual(receipt["lock_digest"],
                             hashlib.sha256(lock.read_bytes()).hexdigest())

    def test_named_rt64_checkout_uses_same_locked_receipt_contract(self):
        with tempfile.TemporaryDirectory() as temp:
            checkout, lock = self.make_checkout(Path(temp))
            contents = json.loads(lock.read_text(encoding="utf-8"))
            contents["dependencies"][0]["name"] = "rt64"
            lock.write_text(json.dumps(contents), encoding="utf-8")
            receipt = verify_dependency(checkout, lock, "rt64")
            self.assertEqual(receipt["name"], "rt64")
            self.assertRegex(receipt["source_identity"], r"^[0-9a-f]{64}$")

            configured = receipt.copy()
            (checkout / "runtime.cpp").write_text("dirty rt64\n", encoding="utf-8")
            process = subprocess.run([
                sys.executable, str(Path(__file__).parents[1] / "tools/build/verify_runtime.py"),
                "--name", "rt64", "--root", str(checkout), "--lock", str(lock),
                "--receipt", str(Path(temp) / "rt64-receipt.json"),
                "--expected-commit", configured["commit"],
                "--expected-source-identity", configured["source_identity"],
                "--expected-lock-digest", configured["lock_digest"],
            ], text=True, capture_output=True, check=False)
            self.assertNotEqual(process.returncode, 0)
            self.assertIn("modified_checkout", process.stdout)

    def test_rt64_clean_revision_change_rejects_configured_guard(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            checkout, lock = self.make_checkout(root)
            contents = json.loads(lock.read_text(encoding="utf-8"))
            contents["dependencies"][0]["name"] = "rt64"
            lock.write_text(json.dumps(contents), encoding="utf-8")
            configured = verify_dependency(checkout, lock, "rt64")
            self.switch_to_clean_revision(checkout, lock)
            receipt_path = root / "rt64-stale.json"
            process = subprocess.run([
                sys.executable, str(Path(__file__).parents[1] / "tools/build/verify_runtime.py"),
                "--name", "rt64", "--root", str(checkout), "--lock", str(lock),
                "--receipt", str(receipt_path),
                "--expected-commit", configured["commit"],
                "--expected-source-identity", configured["source_identity"],
                "--expected-lock-digest", configured["lock_digest"],
            ], text=True, capture_output=True, check=False)
            self.assertNotEqual(process.returncode, 0)
            self.assertIn("configured_identity_mismatch", process.stdout)
            self.assertFalse(receipt_path.exists())

    def test_clean_lock_switch_rejects_stale_configured_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            checkout, lock = self.make_checkout(root)
            configured = verify_runtime(checkout, lock)
            self.switch_to_clean_revision(checkout, lock)

            reconfigured = verify_runtime(checkout, lock)
            self.assertNotEqual(reconfigured["commit"], configured["commit"])
            self.assertNotEqual(reconfigured["source_identity"],
                                configured["source_identity"])
            self.assertNotEqual(reconfigured["lock_digest"], configured["lock_digest"])

            receipt = root / "stale-build-receipt.json"
            process = subprocess.run([
                sys.executable, str(Path(__file__).parents[1] / "tools/build/verify_runtime.py"),
                "--root", str(checkout), "--lock", str(lock), "--receipt", str(receipt),
                "--expected-commit", configured["commit"],
                "--expected-source-identity", configured["source_identity"],
                "--expected-lock-digest", configured["lock_digest"],
            ], text=True, capture_output=True, check=False)
            self.assertNotEqual(process.returncode, 0)
            self.assertIn("configured_identity_mismatch", process.stdout)
            self.assertFalse(receipt.exists())

    def test_modified_checkout_is_rejected_and_changes_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            checkout, lock = self.make_checkout(Path(temp))
            clean_identity = source_identity(checkout)
            (checkout / "runtime.cpp").write_text("int runtime = 2;\n", encoding="utf-8")
            self.assertNotEqual(source_identity(checkout), clean_identity)
            with self.assertRaisesRegex(ValueError, "modified_checkout"):
                verify_runtime(checkout, lock)

    def test_fake_checkout_at_wrong_revision_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            checkout, lock = self.make_checkout(Path(temp))
            (checkout / "second.cpp").write_text("int second = 2;\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(checkout), "add", "second.cpp"], check=True)
            subprocess.run(["git", "-C", str(checkout), "commit", "-qm", "wrong revision"], check=True)
            with self.assertRaisesRegex(ValueError, "wrong_revision"):
                verify_runtime(checkout, lock)


if __name__ == "__main__":
    unittest.main()
