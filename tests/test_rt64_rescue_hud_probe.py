"""Checks for the optional Rescue draw-call tracer."""

import unittest
from pathlib import Path

from tools.build.rt64_rescue_hud_probe import instrument_renderer
from tools.build.patch_rt64_probe import SOURCES, patch_source


class RescueHudProbeTests(unittest.TestCase):
    def test_only_diagnostic_renderer_copy_has_rescue_draw_tracer(self):
        root = Path(__file__).resolve().parents[1]
        path = root / ".local/deps/rt64/src/render/rt64_framebuffer_renderer.cpp"
        raw = path.read_bytes()
        regular = patch_source(path.name, raw, SOURCES[path.name], probe=False).decode()
        diagnostic = patch_source(path.name, raw, SOURCES[path.name], probe=True).decode()
        self.assertNotIn("rt64_probe_rescue_draw", regular)
        self.assertIn("rt64_probe_rescue_draw", diagnostic)

    def test_instruments_pinned_renderer_once_with_bounded_opt_in_records(self):
        root = Path(__file__).resolve().parents[1]
        source = (root / ".local/deps/rt64/src/render/rt64_framebuffer_renderer.cpp").read_text()
        patched = instrument_renderer(source)
        self.assertEqual(patched.count("tetrisphere_rescue_hud_probe_emit("), 2)
        self.assertIn("TETRISPHERE_RT64_RESCUE_PROBE", patched)
        self.assertIn("(p.curWorkload->workloadId % 60)", patched)
        self.assertIn("rt64_probe_rescue_draw_truncated", patched)
        self.assertIn("desc.uid", patched)
        self.assertIn("desc.callIndex", patched)
        self.assertIn("draw.triangles.screenOffset", patched)
        self.assertIn("data.rspViewports[proj.transformsIndex]", patched)
        self.assertIn("p.curWorkload->workloadId", patched)
        self.assertIn("p.fbStorage->colorTarget != nullptr", patched)
        self.assertNotIn("rt64_probe_rescue_draw", source)

    def test_fails_closed_if_renderer_anchor_is_ambiguous(self):
        root = Path(__file__).resolve().parents[1]
        source = (root / ".local/deps/rt64/src/render/rt64_framebuffer_renderer.cpp").read_text()
        anchor = "                // Determine to use the draw call either in the RT scene or the raster scene."
        with self.assertRaisesRegex(ValueError, "anchor"):
            instrument_renderer(source.replace(anchor, anchor + "\n" + anchor))


if __name__ == "__main__":
    unittest.main()
