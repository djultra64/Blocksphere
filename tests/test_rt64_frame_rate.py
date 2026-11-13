"""Focused checks for bounded, uncapped RT64 presentation patches."""
import importlib.util
import hashlib
import shutil
import subprocess
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "tools/build/rt64_frame_rate.py"
PINNED = ROOT / ".local/deps/rt64/src/hle"

class FrameRatePatchTest(unittest.TestCase):
    def test_patch_module_exists(self):
        self.assertTrue(MODULE.exists(), "Unlimited patch module is missing")

    def test_bounded_stream_behavior(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="tetrisphere-unlimited-") as temporary:
            binary = Path(temporary) / "stream-test"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                            "-I" + str(ROOT / "include"),
                            str(ROOT / "tests/unlimited_frame_stream_test.cpp"),
                            "-o", str(binary)], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, timeout=10, capture_output=True, text=True)

    @unittest.skipUnless(MODULE.exists(), "implementation not present")
    def test_patches_real_sources(self):
        spec = importlib.util.spec_from_file_location("frame_rate", MODULE)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for name in ("rt64_application.cpp", "rt64_workload_queue.cpp", "rt64_present_queue.cpp"):
            with self.subTest(name=name):
                source = (PINNED / name).read_text()
                patched = module.patch_frame_rate_source(name, source)
                self.assertIn("tetrisphere_rt64_unlimited_frame_rate", patched)
                self.assertIn("tetrisphere/unlimited_frame_stream.h", patched)
                self.assertNotEqual(source, patched)
                self.assertEqual(patched.count("{"), patched.count("}"))
                from tools.build.patch_rt64_probe import patch_source
                for probe in (False, True):
                    raw = source.encode()
                    composed = patch_source(name, raw, hashlib.sha256(raw).hexdigest(), probe=probe).decode()
                    self.assertEqual(composed.count("tetrisphere/unlimited_frame_stream.h"), 1)
                    if name == "rt64_present_queue.cpp":
                        self.assertIn("if (i == 0) notifyPresentId(present);", composed)
                        self.assertIn("framesToPresent = unlimitedFrameRate ? 1", composed)
                    if name == "rt64_application.cpp":
                        end = composed.split("void Application::end()", 1)[1]
                        self.assertLess(end.index("presentQueue->presentIdCondition.notify_all()"),
                                        end.index("presentQueue->presentThread->join()"))
                        self.assertLess(end.index("presentQueue->presentThread->join()"),
                                        end.index("workloadQueue.reset()"))
        with self.assertRaises(ValueError):
            module.patch_frame_rate_source("rt64_workload_queue.cpp", "bad pinned source")

if __name__ == "__main__":
    unittest.main()
