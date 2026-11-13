"""Check the pinned RT64 copy policy for the M6 inspector."""

import unittest
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from tools.build.rt64_m6_inspector import patch_application, patch_state


ROOT = Path(__file__).resolve().parents[1]


class InspectorPatchTests(unittest.TestCase):
    def test_linear_respects_point_and_copy_sampling(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        raw = (ROOT / ".local/deps/rt64/src/hle/rt64_state.cpp").read_bytes()
        patched = patch_state(raw).decode()
        expression = re.search(r"flags\.linearFiltering = ([^;]+);", patched).group(1)
        source = r'''#include <cstdio>
constexpr unsigned G_TF_POINT=0, G_TF_BILERP=2, G_CYC_COPY=2;
struct Mode { unsigned filter,cycle; unsigned textFilt() const {return filter;}
 unsigned cycleType() const {return cycle;} };
struct Call { Mode otherMode; };
int main() {
 for(bool linearFiltering:{false,true})
  for(bool forceLinearFiltering:{false,true})
   for(unsigned filter:{G_TF_POINT,G_TF_BILERP})
    for(unsigned cycle:{0u,G_CYC_COPY}) {
     Call callDesc{{filter,cycle}};
     std::printf("%d", EXPRESSION);
    }
}
'''.replace("#include <cstdio>", "#include <cstdio>\n#include <initializer_list>")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sampling.cpp"
            exe = Path(directory) / "sampling"
            path.write_text(source.replace("EXPRESSION", expression))
            subprocess.run([compiler, "-std=c++17", str(path), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            actual = subprocess.check_output([str(exe)], text=True)
        self.assertEqual(actual, "0000001000100010")

    def test_shortcut_and_capture_policy(self):
        source = (ROOT / ".local/deps/rt64/src/hle/rt64_application.cpp").read_bytes()
        patched = patch_application(source).decode()
        self.assertIn("event->key.repeat == 0", patched)
        self.assertIn("tetrisphere_rt64_clear_keyboard_input();", patched)
        self.assertIn("SDL_SCANCODE_F11", patched)
        self.assertIn("SDL_SCANCODE_F1", patched)
        self.assertLess(patched.index("event->key.keysym.scancode == SDL_SCANCODE_F1"),
                        patched.index("presentQueue->inspector->handleSdlEvent(event)"))

    def test_only_safe_graphics_controls_remain(self):
        source = (ROOT / ".local/deps/rt64/src/hle/rt64_state.cpp").read_bytes()
        patched = patch_state(source).decode()
        configuration = patched.split('if (ImGui::BeginTabItem("Configuration")) {', 1)[1]
        configuration = configuration.split('if (ImGui::BeginTabItem("Textures")) {', 1)[0]
        self.assertIn('"Presentation Filtering"', configuration)
        self.assertIn('"Three-Point Texture Filtering"', configuration)
        for blocked in ('"Resolution Mode"', '"Aspect Ratio Mode"',
                        '"Antialiasing"', '"Upscale 2D Mode"',
                        '"Render to RAM"', '"Remove Black Borders"'):
            self.assertNotIn(blocked, configuration)
        self.assertIn('session only', configuration)
        self.assertTrue('ImGuiCond_Always' in patched)
        self.assertTrue('rt64_inspector_graphics_changed' in patched)


if __name__ == "__main__":
    unittest.main()
