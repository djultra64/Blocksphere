"""Transition capture allows menus while validating guest buffer boundaries."""

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PauseSceneGateTests(unittest.TestCase):
    def test_menu_and_gameplay_copies_can_request_wide_capture(self):
        source = r'''
#include "tetrisphere/pause_capture_scene.h"
#include <cassert>
int main() {
    using tetrisphere::pause_capture_scene_eligible;
    using tetrisphere::transition_capture_buffers_eligible;
    assert(transition_capture_buffers_eligible(0x100000,0x200000));
    assert(transition_capture_buffers_eligible(0x200000,0x100000));
    assert(!transition_capture_buffers_eligible(0,0x200000));
    assert(!transition_capture_buffers_eligible(0x100001,0x200000));
    assert(!transition_capture_buffers_eligible(0x100000,0x100002));
    assert(!transition_capture_buffers_eligible(0x100000,0x800000));
    return pause_capture_scene_eligible(-1) &&
           pause_capture_scene_eligible(0) &&
           pause_capture_scene_eligible(2) &&
           pause_capture_scene_eligible(4) ? 0 : 1;
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
