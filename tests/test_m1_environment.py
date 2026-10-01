"""M1 toolchain contract tests. Git repositories and ROMs here are synthetic."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.build.check_environment import check_tools, check_rom
from tools.build.fetch_dependencies import (fetch_dependency, load_lock,
                                            paths_refer_to_same_root,
                                            validate_private_root, verify_checkout)


class M1EnvironmentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.workspace = Path(self.temp.name)
        self.lock_path = self.workspace / "lock.json"
        self.entry = {
            "name": "example",
            "repository": "https://github.com/example/example.git",
            "commit": "a" * 40,
        }

    def write_lock(self, entries):
        self.lock_path.write_text(json.dumps({"schema_version": 1, "dependencies": entries}))

    def test_load_lock_accepts_pinned_repository(self):
        self.write_lock([self.entry])
        self.assertEqual(load_lock(self.lock_path)["dependencies"][0]["commit"], "a" * 40)

    def test_project_lock_covers_recompiler_runtime_and_renderer(self):
        repo = Path(__file__).resolve().parents[1]
        lock = load_lock(repo / "config/dependencies-m1.json")
        names = {entry["name"] for entry in lock["dependencies"]}
        self.assertTrue({"N64Recomp", "N64ModernRuntime", "rt64"} <= names)

    def test_load_lock_rejects_unpinned_or_duplicate_entries(self):
        for entries in ([{**self.entry, "commit": "main"}],
                        [{**self.entry, "name": "../escape"}],
                        [self.entry, self.entry]):
            with self.subTest(entries=entries):
                self.write_lock(entries)
                with self.assertRaises(ValueError):
                    load_lock(self.lock_path)

    def test_load_lock_rejects_non_object_json_without_traceback(self):
        script = Path(__file__).resolve().parents[1] / "tools/build/fetch_dependencies.py"
        for data in ([], None):
            with self.subTest(data=data):
                self.lock_path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    load_lock(self.lock_path)
                result = subprocess.run([sys.executable, str(script), "--lock", str(self.lock_path),
                                         "--json"], capture_output=True, text=True)
                self.assertEqual(result.returncode, 1)
                self.assertIn("error", json.loads(result.stdout))
                self.assertNotIn("Traceback", result.stderr)

    def test_verify_checkout_rejects_wrong_commit_and_accepts_exact_head(self):
        repo = self.workspace / "checkout"
        repo.mkdir()
        subprocess.run(["git", "init", "-q", str(repo)], check=True)
        subprocess.run(["git", "-C", str(repo), "-c", "user.name=Test", "-c",
                        "user.email=test@example.invalid", "commit", "-q", "--allow-empty",
                        "-m", "synthetic"], check=True)
        head = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
        self.assertTrue(verify_checkout(repo, self.entry))
        self.assertEqual(verify_checkout(repo, {**self.entry, "commit": head}), [])

    def test_windows_checkout_root_comparison_is_case_insensitive_but_exact(self):
        self.assertTrue(paths_refer_to_same_root(
            r"C:\Users\RyArcade\Documents\deps\rt64",
            "c:/users/ryarcade/Documents/deps/rt64",
        ))
        self.assertTrue(paths_refer_to_same_root(
            "/c/Users/RyArcade/Documents/deps/rt64",
            r"C:\Users\ryarcade\Documents\deps\rt64",
        ))
        self.assertFalse(paths_refer_to_same_root(
            r"C:\Users\RyArcade\Documents\deps\rt64",
            r"C:\Users\RyArcade\Documents\deps\N64ModernRuntime",
        ))

    def test_verify_checkout_reports_missing_repository(self):
        self.assertTrue(verify_checkout(self.workspace / "absent", self.entry))

    def test_verify_checkout_rejects_nested_directory_in_parent_git_repo(self):
        repo = self.workspace / "repo"
        repo.mkdir()
        subprocess.run(["git", "init", "-q", str(repo)], check=True)
        subprocess.run(["git", "-C", str(repo), "-c", "user.name=Test", "-c",
                        "user.email=test@example.invalid", "commit", "-q", "--allow-empty",
                        "-m", "synthetic"], check=True)
        nested = repo / "plain-directory"
        nested.mkdir()
        head = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
        self.assertIn("not_git_checkout", verify_checkout(nested, {**self.entry, "commit": head}))

    def test_existing_checkout_reports_missing_and_mismatched_submodules(self):
        sub = self.workspace / "sub"
        parent = self.workspace / "parent"
        sub.mkdir()
        parent.mkdir()
        for repo in (sub, parent):
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
        subprocess.run(["git", "-C", str(sub), "-c", "user.name=Test", "-c",
                        "user.email=test@example.invalid", "commit", "-q", "--allow-empty",
                        "-m", "sub-one"], check=True)
        subprocess.run(["git", "-C", str(parent), "-c", "protocol.file.allow=always",
                        "submodule", "add", "-q", str(sub), "vendor/sub"], check=True)
        subprocess.run(["git", "-C", str(parent), "-c", "user.name=Test", "-c",
                        "user.email=test@example.invalid", "commit", "-q", "-am", "add submodule"], check=True)
        head = subprocess.check_output(["git", "-C", str(parent), "rev-parse", "HEAD"], text=True).strip()
        entry = {**self.entry, "name": "parent", "commit": head}
        self.assertEqual(verify_checkout(parent, entry), [])
        subprocess.run(["git", "-C", str(parent), "submodule", "deinit", "-f", "--all"],
                       check=True, capture_output=True)
        self.assertIn("missing_submodule", verify_checkout(parent, entry))
        self.assertIn("missing_submodule", fetch_dependency(self.workspace, entry))
        subprocess.run(["git", "-C", str(parent), "-c", "protocol.file.allow=always",
                        "submodule", "update", "--init", "--recursive"], check=True, capture_output=True)
        subprocess.run(["git", "-C", str(parent / "vendor/sub"), "-c", "user.name=Test", "-c",
                        "user.email=test@example.invalid", "commit", "-q", "--allow-empty",
                        "-m", "sub-two"], check=True)
        self.assertIn("wrong_submodule_revision", verify_checkout(parent, entry))

    def test_private_root_cannot_escape_local_deps(self):
        private = self.workspace / ".local" / "deps"
        self.assertEqual(validate_private_root(self.workspace, private), private.resolve())
        for root in (self.workspace / "deps", private / ".." / ".." / "public"):
            with self.subTest(root=root), self.assertRaises(ValueError):
                validate_private_root(self.workspace, root)

    def test_private_root_rejects_symlink_to_unrelated_directory(self):
        outside = self.workspace / "outside"
        outside.mkdir()
        (self.workspace / ".local").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(ValueError):
            validate_private_root(self.workspace, self.workspace / ".local/deps")

    def test_private_root_accepts_this_worktrees_authorized_private_symlink(self):
        repo = Path(__file__).resolve().parents[1]
        self.assertEqual(validate_private_root(repo, repo / ".local/deps"),
                         (repo / ".local/deps").resolve())

    def test_missing_tools_are_reported_without_crashing(self):
        status = check_tools(("git", "missing-tool"), which=lambda name: "/usr/bin/git" if name == "git" else None)
        self.assertEqual(status["git"]["status"], "present")
        self.assertEqual(status["missing-tool"]["status"], "missing")

    def test_rom_check_normalizes_byte_order_without_revealing_content(self):
        canonical = bytearray((i % 251 for i in range(4096)))
        canonical[:4] = bytes.fromhex("80371240")
        canonical = bytes(canonical)
        manifest = {"id": "synthetic", "size_bytes": len(canonical),
                    "sha256": hashlib.sha256(canonical).hexdigest()}
        raw = self.workspace / "sample.v64"
        raw.write_bytes(bytes(v for i in range(0, len(canonical), 2)
                              for v in canonical[i:i + 2][::-1]))
        result = check_rom(raw, manifest)
        self.assertEqual(result["status"], "recognized")
        self.assertEqual(result["format"], "v64")
        self.assertNotIn("sha256", result)
        self.assertNotIn(str(raw), json.dumps(result))
        raw.write_bytes(canonical[:-4])
        self.assertEqual(check_rom(raw, manifest)["status"], "rejected")

    def test_build_helpers_are_trackable_but_root_build_output_is_ignored(self):
        repo = Path(__file__).resolve().parents[1]
        helper = subprocess.run(["git", "check-ignore", "-q", "tools/build/check_environment.py"],
                                cwd=repo)
        output = subprocess.run(["git", "check-ignore", "-q", "build/private.bin"], cwd=repo)
        self.assertEqual(helper.returncode, 1)
        self.assertEqual(output.returncode, 0)


if __name__ == "__main__":
    unittest.main()
