"""The UI hook manifest must match NTPE rev0 instructions before generation."""

import struct
import unittest

from tools.recomp.ui_anchor_hooks import (
    UiAnchorManifestError,
    render_ui_anchor_hooks,
    validate_ui_anchor_manifest,
)


class UiAnchorHookTests(unittest.TestCase):
    def setUp(self):
        self.image = bytearray(0x100000)
        self.calls = [
            {"pc": "0x800765D4", "target": "0x8003396C", "anchor": "left"},
            {"pc": "0x80076664", "target": "0x80071DE8", "anchor": "native"},
        ]
        for item in self.calls:
            pc, target = int(item["pc"], 16), int(item["target"], 16)
            index = pc - 0x80025C50
            struct.pack_into(">I", self.image, index, 0x0C000000 | ((target >> 2) & 0x03FFFFFF))

    def manifest(self):
        return {"schema_version": 1, "rom_revision": "NTPE rev0",
                "function": "func_80075CD8", "expected_direct_calls": 2,
                "calls": self.calls}

    def test_verifies_jal_and_generates_only_classified_hooks(self):
        manifest = self.manifest()
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertIn("before_vram = 0x800765D4", hooks)
        self.assertIn("before_vram = 0x800765DC", hooks)
        self.assertIn("tetrisphere_ui_span_begin", hooks)
        self.assertIn("tetrisphere_ui_span_end", hooks)
        self.assertNotIn("0x80076664", hooks)

    def test_wrong_instruction_or_unclassified_call_fails(self):
        manifest = self.manifest()
        manifest["calls"][0]["target"] = "0x80033970"
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))
        manifest = self.manifest()
        manifest["calls"][1]["anchor"] = "unknown"
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))

    def test_optional_single_player_gate_only_wraps_begin(self):
        manifest = self.manifest()
        manifest["calls"][0]["when"] = "player_count_one"
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertIn("MEM_H(0, S32(0x8015D980u)) == 1", hooks)
        self.assertIn("tetrisphere_ui_span_end(rdram, ctx, 0x800765D4u);", hooks)
        manifest["calls"][0]["when"] = "some_other_state"
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))

    def test_secondary_heart_and_skull_calls_are_scoped_to_single_player(self):
        manifest = self.manifest()
        extra = [
            {"pc": "0x80078C70", "target": "0x8003396C", "anchor": "left",
             "when": "single_player_not_mode_three"},
            {"pc": "0x80078CE8", "target": "0x8003396C", "anchor": "left",
             "when": "single_player_not_mode_three"},
            {"pc": "0x80078D48", "target": "0x8003396C", "anchor": "left",
             "when": "single_player_not_mode_three"},
        ]
        for call in extra:
            pc = int(call["pc"], 16)
            struct.pack_into(">I", self.image, pc - 0x80025C50,
                             0x0C000000 | ((0x8003396C >> 2) & 0x03FFFFFF))
        manifest["extra_calls"] = extra
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertIn('func = "func_80078BA8"', hooks)
        self.assertIn("MEM_H(0, S32(0x8015D980u)) == 1", hooks)
        self.assertIn("MEM_H(0, S32(0x800E44A8u)) != 3", hooks)
        self.assertIn("before_vram = 0x80078C70", hooks)
        manifest["extra_calls"].pop()
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))

    def test_heart_loss_model_has_its_own_scoped_producer(self):
        manifest = self.manifest()
        pc, target = 0x800C3DC0, 0x8002D93C
        struct.pack_into(">I", self.image, pc - 0x80025C50,
                         0x0C000000 | ((target >> 2) & 0x03FFFFFF))
        manifest["model_calls"] = [
            {"pc": "0x800C3DC0", "target": "0x8002D93C", "anchor": "left",
             "when": "single_player_not_mode_three"},
        ]
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertIn('func = "func_800C3CEC"', hooks)
        self.assertIn("before_vram = 0x800C3DC0", hooks)
        self.assertIn("before_vram = 0x800C3DC8", hooks)
        manifest["model_calls"][0]["target"] = "0x8002D940"
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))

    def test_duplicates_and_missing_calls_fail(self):
        manifest = self.manifest()
        manifest["calls"][1]["pc"] = manifest["calls"][0]["pc"]
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))

    def test_reviewed_helper_calls_scope_indirect_hud_producers(self):
        manifest = self.manifest()
        helpers = [
            (0x80076ED8, 0x80078DA0, "single_player_not_mode_three"),
            (0x80076F64, 0x80078514, "single_player_not_mode_three"),
            (0x80077470, 0x8007570C, "player_count_one"),
            (0x800774E0, 0x8007570C, "player_count_one"),
        ]
        manifest["helper_calls"] = [
            {"function": "func_80075CD8", "pc": f"0x{pc:08X}",
             "target": f"0x{target:08X}", "anchor": "right", "when": gate}
            for pc, target, gate in helpers
        ]
        for pc, target, _ in helpers:
            struct.pack_into(">I", self.image, pc - 0x80025C50,
                             0x0C000000 | ((target >> 2) & 0x03FFFFFF))
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        for pc, _, _ in helpers:
            self.assertIn(f"before_vram = 0x{pc:08X}", hooks)
            self.assertIn(f"before_vram = 0x{pc + 8:08X}", hooks)
            self.assertIn(f"tetrisphere_ui_span_end(rdram, ctx, 0x{pc:08X}u);", hooks)
        self.assertEqual(hooks.count('func = "func_80075CD8"'), 10)
        self.assertEqual(hooks.count("MEM_H(0, S32(0x800E44A8u)) != 3"), 2)

        for key, bad_value in (("function", "func_80078BA8"),
                               ("target", "0x8003396C"),
                               ("anchor", "left"),
                               ("when", "player_count_one"),
                               ("pc", "0x80076EDC")):
            changed = self.manifest()
            changed["helper_calls"] = [dict(call) for call in manifest["helper_calls"]]
            changed["helper_calls"][0][key] = bad_value
            with self.subTest(key=key), self.assertRaises(UiAnchorManifestError):
                validate_ui_anchor_manifest(changed, bytes(self.image))

        duplicated = self.manifest()
        duplicated["helper_calls"] = [manifest["helper_calls"][0]] * 2
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(duplicated, bytes(self.image))
        broken_rom = bytearray(self.image)
        struct.pack_into(">I", broken_rom, 0x80076ED8 - 0x80025C50, 0)
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(broken_rom))

    def test_combo_number_and_star_share_one_single_player_scope(self):
        manifest = self.manifest()
        combo = {"function": "func_800CD3B8", "pc": "0x800D003C",
                 "target": "0x800699B4", "anchor": "right",
                 "when": "single_player_not_mode_three"}
        manifest["helper_calls"] = [combo]
        pc, target = 0x800D003C, 0x800699B4
        struct.pack_into(">I", self.image, pc - 0x80025C50,
                         0x0C000000 | ((target >> 2) & 0x03FFFFFF))
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertIn('func = "func_800CD3B8"\nbefore_vram = 0x800D003C', hooks)
        self.assertIn('tetrisphere_ui_span_begin(rdram, ctx, 0x800D003Cu, 2u);', hooks)
        self.assertIn('tetrisphere_ui_span_end(rdram, ctx, 0x800D003Cu);', hooks)
        self.assertIn('MEM_H(0, S32(0x800E44A8u)) != 3', hooks)
        self.assertNotIn('0x800D0064', hooks)
        for key, value in (("pc", "0x800D0064"), ("anchor", "left"),
                           ("target", "0x800699B8"), ("when", "player_count_one")):
            changed = self.manifest()
            changed["helper_calls"] = [{**combo, key: value}]
            with self.subTest(key=key), self.assertRaises(UiAnchorManifestError):
                validate_ui_anchor_manifest(changed, bytes(self.image))
        struct.pack_into(">I", self.image, pc - 0x80025C50, 0)
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))

    def test_pause_text_calls_exclude_frame_and_robot(self):
        manifest = self.manifest()
        pcs = (0x800408BC, 0x80040954, 0x800409F0, 0x80040A84)
        manifest["helper_calls"] = [
            {"function": "func_8003FE3C", "pc": f"0x{pc:08X}",
             "target": "0x80038478", "anchor": "pause_text", "when": "always"}
            for pc in pcs]
        for pc in pcs:
            struct.pack_into(">I", self.image, pc - 0x80025C50,
                             0x0C000000 | ((0x80038478 >> 2) & 0x03FFFFFF))
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertEqual(hooks.count('func = "func_8003FE3C"'), 8)
        for pc in pcs:
            self.assertIn(f'tetrisphere_ui_span_begin(rdram, ctx, 0x{pc:08X}u, 7u);', hooks)
            self.assertIn(f'tetrisphere_ui_span_end(rdram, ctx, 0x{pc:08X}u);', hooks)
        self.assertNotIn('0x80040738', hooks)
        self.assertNotIn('0x8004064C', hooks)
        changed = self.manifest()
        changed["helper_calls"] = [{**manifest["helper_calls"][0], "anchor": "left"}]
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(changed, bytes(self.image))

    def test_left_preview_helper_is_scoped_in_its_containing_function(self):
        manifest = self.manifest()
        preview = {"function": "func_800CD3B8", "pc": "0x800CF8A8",
                   "target": "0x800A5BB4", "anchor": "left",
                   "when": "single_player_not_mode_three"}
        manifest["helper_calls"] = [preview]
        pc, target = 0x800CF8A8, 0x800A5BB4
        struct.pack_into(">I", self.image, pc - 0x80025C50,
                         0x0C000000 | ((target >> 2) & 0x03FFFFFF))
        validate_ui_anchor_manifest(manifest, bytes(self.image))
        hooks = render_ui_anchor_hooks(manifest)
        self.assertIn('func = "func_800CD3B8"\nbefore_vram = 0x800CF8A8', hooks)
        self.assertIn('func = "func_800CD3B8"\nbefore_vram = 0x800CF8B0', hooks)
        self.assertIn('tetrisphere_ui_span_begin(rdram, ctx, 0x800CF8A8u, 1u);', hooks)
        self.assertIn('tetrisphere_ui_span_end(rdram, ctx, 0x800CF8A8u);', hooks)
        for key, bad_value in (("function", "func_80075CD8"),
                               ("target", "0x800A5BB8"),
                               ("anchor", "right"),
                               ("when", "player_count_one")):
            changed = self.manifest()
            changed["helper_calls"] = [{**preview, key: bad_value}]
            with self.subTest(key=key), self.assertRaises(UiAnchorManifestError):
                validate_ui_anchor_manifest(changed, bytes(self.image))
        struct.pack_into(">I", self.image, pc - 0x80025C50, 0)
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))
        manifest = self.manifest()
        manifest["calls"].pop()
        with self.assertRaises(UiAnchorManifestError):
            validate_ui_anchor_manifest(manifest, bytes(self.image))


if __name__ == "__main__":
    unittest.main()
