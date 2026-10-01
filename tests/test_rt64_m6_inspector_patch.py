"""Check the pinned RT64 copy policy for the M6 inspector."""

import unittest
from pathlib import Path

from tools.build.rt64_m6_inspector import patch_application, patch_state


ROOT = Path(__file__).resolve().parents[1]


class InspectorPatchTests(unittest.TestCase):
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
