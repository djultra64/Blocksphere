"""Pause snapshots belong to gameplay, regardless of the single player mode."""

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PauseSceneGateTests(unittest.TestCase):
    def test_only_gameplay_scene_can_request_wide_pause_capture(self):
        source = r'''
#include "tetrisphere/pause_capture_scene.h"
int main() {
    using tetrisphere::pause_capture_scene_eligible;
    return pause_capture_scene_eligible(-1) &&
           !pause_capture_scene_eligible(0) &&
           !pause_capture_scene_eligible(2) &&
           !pause_capture_scene_eligible(4) ? 0 : 1;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gate.cpp"
            executable = Path(directory) / "gate"
            path.write_text(source)
            subprocess.run(["c++", "-std=c++17", "-I", str(ROOT / "include"),
                            str(path), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

if __name__ == "__main__":
    unittest.main()
