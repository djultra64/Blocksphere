"""A nearly full N64 3D frame keeps the visible VI height."""

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class FullFrameTargetHeightTests(unittest.TestCase):
    def test_nearly_full_3d_frame_keeps_240_visible_rows(self):
        program = r'''
#include <cstdint>
#include "tetrisphere/full_frame_target_height.h"

int main() {
    using tetrisphere::full_frame_target_height;
    if (full_frame_target_height(219, 320, 240, 320, true) != 240) return 1;
    if (full_frame_target_height(240, 320, 240, 320, true) != 240) return 2;
    if (full_frame_target_height(219, 320, 240, 320, false) != 219) return 3;
    if (full_frame_target_height(120, 320, 240, 320, true) != 120) return 4;
    if (full_frame_target_height(219, 640, 480, 640, true) != 219) return 5;
    if (full_frame_target_height(219, 320, 240, 256, true) != 219) return 6;
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "target_height.cpp"
            binary = Path(directory) / "target_height"
            source.write_text(program)
            subprocess.run(["c++", "-std=c++17", "-I", str(ROOT / "include"),
                            str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
