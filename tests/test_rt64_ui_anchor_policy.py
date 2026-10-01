"""A tagged draw call receives its own UI offset, independent of its neighbor."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.build.rt64_ui_anchor_policy import UI_ANCHOR_HELPER


class UiAnchorPolicyTests(unittest.TestCase):
    def test_renderer_applies_screen_offset_to_tagged_perspective_call(self):
        source = (Path(__file__).resolve().parents[1] /
                  "tools/build/patch_rt64_probe.py").read_text()
        self.assertIn("proj.type == Projection::Type::Orthographic ||\n"
                      "                    proj.type == Projection::Type::Perspective", source)
        self.assertIn("proj.type == Projection::Type::Perspective ||\n"
                      "                            proj.type == Projection::Type::Orthographic", source)

    def test_per_call_ortho_rectangle_and_scissor_reset(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        source = r'''#include <cassert>
#include <cmath>
struct Triangles { struct { float x = 0; } screenOffset; };
struct Rect { float x = 0; };
struct Scissor { float left = 0, top = 7, right = 0, bottom = 80; };
namespace RT64 {
''' + UI_ANCHOR_HELPER + r'''
int run() {
    constexpr float wide = 1920.0f, original = 1440.0f, aspect = 4.0f / 3.0f;
    assert(tetrisphere_ui_anchor_shift_pixels(1, wide, original, aspect) == -192.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(2, wide, original, aspect) == 192.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(3, wide, original, aspect) == 0.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(99, wide, original, aspect) == 0.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(1, wide, original, 1.0f) == 0.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(2, original, wide, aspect) == 0.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(7, wide, original, aspect) == -48.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(7, wide, original, 1.0f) == 0.0f);
    assert(tetrisphere_ui_anchor_shift_pixels(7, 2560, 1920, aspect) == -64.0f);
    const auto tag = tetrisphere_ui_anchor_tag(1);
    assert(tag.anchor == TetrisphereUiAnchor::Left);
    assert(tag.player == TetrisphereUiPlayer::Shared);

    // All four calls share one projection and one mutable Triangles object.
    // Center must restore the unshifted base even after a Left call.
    Triangles triangles;
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 1, wide, original, aspect, 960);
    assert(std::fabs(triangles.screenOffset.x - .05f) < .00001f);
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 3, wide, original, aspect, 960);
    assert(std::fabs(triangles.screenOffset.x - .25f) < .00001f);
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 2, wide, original, aspect, 960);
    assert(std::fabs(triangles.screenOffset.x - .45f) < .00001f);
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 0, wide, original, aspect, 960);
    assert(std::fabs(triangles.screenOffset.x - .25f) < .00001f);
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 1, wide, original, 1.0f, 960);
    assert(std::fabs(triangles.screenOffset.x - .25f) < .00001f);
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 1, wide, original, aspect, 0);
    assert(std::fabs(triangles.screenOffset.x - .25f) < .00001f);
    // A tagged Perspective animation uses the same screen-space offset;
    // the following untagged world call must recover its base immediately.
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 1, wide, original, aspect, 960);
    assert(std::fabs(triangles.screenOffset.x - .05f) < .00001f);
    tetrisphere_ui_anchor_ortho_for_call(triangles, .25f, 0, wide, original, aspect, 960);
    assert(std::fabs(triangles.screenOffset.x - .25f) < .00001f);

    Rect rect;
    Scissor scissor;
    const Scissor base{240, 7, 1680, 80};
    tetrisphere_ui_anchor_rect_for_call(rect, 500, 1, wide, original, aspect);
    tetrisphere_ui_anchor_scissor_for_call(scissor, base, 1, wide, original, aspect, 1920);
    assert(rect.x == 308 && scissor.left == 0 && scissor.right == 1920);
    tetrisphere_ui_anchor_rect_for_call(rect, 500, 3, wide, original, aspect);
    tetrisphere_ui_anchor_scissor_for_call(scissor, base, 3, wide, original, aspect, 1920);
    assert(rect.x == 500 && scissor.left == 240 && scissor.right == 1680);
    tetrisphere_ui_anchor_rect_for_call(rect, 500, 2, wide, original, aspect);
    tetrisphere_ui_anchor_scissor_for_call(scissor, base, 2, wide, original, aspect, 1920);
    assert(rect.x == 692 && scissor.left == 0 && scissor.right == 1920);
    assert(scissor.top == 7 && scissor.bottom == 80);
    tetrisphere_ui_anchor_rect_for_call(rect, 500, 1, wide, original, 1.0f);
    tetrisphere_ui_anchor_scissor_for_call(scissor, base, 1, wide, original, 1.0f, 1920);
    assert(rect.x == 500 && scissor.left == 240 && scissor.right == 1680);
    return 0;
}}
int main() { return RT64::run(); }
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "policy.cpp"
            executable = Path(directory) / "policy"
            path.write_text(source)
            compiled = subprocess.run([compiler, "-std=c++17", str(path), "-o", str(executable)],
                                      capture_output=True, text=True)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
