"""Synthetic MIPS/ROM fixtures; no game bytes or private inputs required."""

import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import tomllib
import unittest

from tools.analysis.mips import decode_flow
from tools.analysis.map_rom import analyze_rom, validate_sections


ENTRY = 0x80025C50


def rom(words=(), tail=b""):
    data = bytearray(0x1000)
    data[:4] = bytes.fromhex("80371240")
    struct.pack_into(">I", data, 8, ENTRY)
    data.extend(b"".join(struct.pack(">I", word) for word in words))
    data.extend(tail)
    data.extend(bytes((-len(data)) % 4))
    return bytes(data)


class MipsFlowTests(unittest.TestCase):
    def test_jal_uses_pc_plus_four_high_bits_and_delay_slot(self):
        flow = decode_flow(0x0C000010, 0x8FFFFFFC)
        self.assertEqual((flow.kind, flow.target, flow.delay_slot),
                         ("call", 0x90000040, 0x90000000))

    def test_backward_branch_and_likely_delay_semantics(self):
        normal = decode_flow(0x1500FFFD, ENTRY)
        likely = decode_flow(0x5500FFFD, ENTRY)
        self.assertEqual(normal.target, 0x80025C48)
        self.assertFalse(normal.annul_not_taken)
        self.assertTrue(likely.annul_not_taken)

    def test_indirect_call_return_and_plain_instruction(self):
        self.assertEqual(decode_flow(0x0320F809, ENTRY).kind, "indirect_call")
        self.assertEqual(decode_flow(0x03E00008, ENTRY).kind, "return")
        self.assertEqual(decode_flow(0x03200008, ENTRY).kind, "indirect_jump")
        self.assertIsNone(decode_flow(0, ENTRY))


class StaticMapTests(unittest.TestCase):
    def test_direct_call_creates_candidate_not_confirmed_function(self):
        result = analyze_rom(rom([0x0C009718, 0, 0, 0, 0x03E00008, 0]))
        call = result.detections["direct_calls"][0]
        self.assertEqual((call["address"], call["target"], call["delay_slot"]),
                         (ENTRY, 0x80025C60, ENTRY + 4))
        functions = result.detections["functions"]
        self.assertIn(0x80025C60, [item["address"] for item in functions])
        self.assertTrue(all(item["status"] == "detected" for item in functions))
        self.assertTrue(all(item["size"] is None for item in functions))
        self.assertEqual(result.human_rulings, [])

    def test_indirect_call_retains_register_and_unknown_target(self):
        result = analyze_rom(rom([0x0320F809, 0, 0x03E00008, 0]))
        call = result.detections["indirect_calls"][0]
        self.assertEqual(call["register"], 25)
        self.assertIsNone(call["target"])
        self.assertEqual(call["delay_slot"], ENTRY + 4)

    def test_pointer_run_is_only_jump_table_candidate(self):
        result = analyze_rom(rom([0x03E00008, 0, 0, 0,
                                  0x80025C50, 0x80025C58, 0x80025C5C, 0]))
        tables = result.detections["jump_tables"]
        self.assertEqual(len(tables), 1)
        self.assertEqual((tables[0]["rom_start"], tables[0]["entry_count"]), (0x1010, 3))
        self.assertEqual(tables[0]["status"], "detected")
        self.assertEqual(tables[0]["confidence"], "low")

    def test_pi_dma_register_sequence_recovers_rom_ram_and_length(self):
        # PI base, DRAM=0x30000, CART=0x10002000, WR_LEN=0x1f (32 bytes).
        result = analyze_rom(rom([
            0x3C08A460, 0x3C090003, 0xAD090000,
            0x3C091000, 0x35292000, 0xAD090004,
            0x2409001F, 0xAD09000C, 0x03E00008, 0,
        ]))
        copy = result.detections["boot_transfers"][0]
        self.assertEqual((copy["kind"], copy["rom_start"], copy["ram_start"], copy["size"]),
                         ("pi_dma_read", 0x2000, 0x80030000, 32))

    def test_cpu_copy_loop_recovers_bounded_range(self):
        result = analyze_rom(rom([
            0x3C088002, 0x35086000, 0x3C098003,
            0x3C0B8002, 0x356B6020,
            0x8D0A0000, 0xAD2A0000, 0x25080004, 0x25290004,
            0x150BFFFB, 0, 0x03E00008, 0,
        ]))
        copies = [x for x in result.detections["boot_transfers"] if x["kind"] == "cpu_copy_loop"]
        self.assertEqual(len(copies), 1)
        self.assertEqual((copies[0]["source_start"], copies[0]["ram_start"], copies[0]["size"]),
                         (0x80026000, 0x80030000, 32))

    def test_constant_tracking_does_not_leak_across_control_flow(self):
        result = analyze_rom(rom([
            0x3C08A460, 0x3C090003, 0xAD090000,
            0x3C091000, 0x35292000, 0xAD090004,
            0x0320F809, 0, 0x2409001F, 0xAD09000C,
        ]))
        self.assertEqual(result.detections["boot_transfers"], [])

    def test_boot_zero_loop_recovers_bss_range_including_delay_slot(self):
        result = analyze_rom(rom([
            0x3C088004, 0x24090020,
            0x2129FFF8, 0xAD000000, 0xAD000004, 0x1520FFFC, 0x21080008,
            0x03E00008, 0,
        ]))
        zero = result.detections["boot_transfers"][0]
        self.assertEqual((zero["kind"], zero["ram_start"], zero["size"]),
                         ("cpu_zero_loop", 0x80040000, 32))

    def test_copy_loop_cannot_reuse_constants_from_before_call(self):
        result = analyze_rom(rom([
            0x3C088002, 0x35086000, 0x3C098003,
            0x3C0B8002, 0x356B6020, 0x0320F809, 0,
            0x8D0A0000, 0xAD2A0000, 0x25080004, 0x25290004,
            0x150BFFFB, 0, 0x03E00008, 0,
        ]))
        self.assertEqual(result.detections["boot_transfers"], [])

    def test_compression_signatures_do_not_claim_a_valid_stream(self):
        result = analyze_rom(rom([0x03E00008, 0], b"Yaz0" + bytes(12) + b"RNC\x01"))
        hits = result.detections["compression"]
        self.assertEqual([item["format"] for item in hits], ["Yaz0", "RNC1"])
        self.assertTrue(all(item["validated_stream"] is False for item in hits))
        self.assertEqual(result.detections["boot_decompression"], [])
        self.assertEqual(result.detections["overlays"], [])

    def test_rejects_malformed_or_unnormalized_input(self):
        for data in (b"", rom([0])[:0xFFF], b"\x37\x80\x40\x12" + rom([0])[4:]):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                analyze_rom(data)

    def test_sections_reject_rom_or_ram_overlap_but_allow_adjacent(self):
        first = {"name": "a", "rom_start": 0x1000, "ram_start": ENTRY, "size": 0x20}
        second = {"name": "b", "rom_start": 0x1020, "ram_start": ENTRY + 0x20, "size": 0x20}
        validate_sections([first, second])
        for field in ("rom_start", "ram_start"):
            with self.subTest(field=field), self.assertRaises(ValueError):
                validate_sections([first, {**second, field: second[field] - 4}])

    def test_serialization_is_deterministic_and_contains_provenance(self):
        data = rom([0x0320F809, 0, 0x03E00008, 0])
        first = analyze_rom(data).to_json()
        self.assertEqual(first, analyze_rom(data).to_json())
        parsed = json.loads(first)
        self.assertEqual(parsed["entry_point"], ENTRY)
        self.assertIn("provenance", parsed["detections"]["indirect_calls"][0])
        self.assertNotIn("timestamp", parsed)

    def test_cli_rejects_public_output_before_writing(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "synthetic.z64"
            output = Path(temp) / "public.json"
            source.write_bytes(rom([0x03E00008, 0]))
            run = subprocess.run([sys.executable, "-m", "tools.analysis.map_rom", str(source),
                                  "--output", str(output)], capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertIn(".local", run.stderr)
            self.assertFalse(output.exists())

    def test_cli_preserves_input_for_identical_symlink_and_hardlink_output(self):
        root = Path(__file__).resolve().parents[1]
        private = root / ".local"
        private.mkdir(exist_ok=True)
        original = rom([0x03E00008, 0])
        for alias in ("identical", "symlink", "hardlink"):
            with self.subTest(alias=alias), tempfile.TemporaryDirectory(dir=private) as temp:
                source = Path(temp) / "synthetic.z64"
                source.write_bytes(original)
                output = source if alias == "identical" else Path(temp) / "alias.json"
                if alias == "symlink":
                    output.symlink_to(source)
                elif alias == "hardlink":
                    output.hardlink_to(source)
                run = subprocess.run([sys.executable, "-m", "tools.analysis.map_rom", str(source),
                                      "--output", str(output)], cwd=root,
                                     capture_output=True, text=True)
                self.assertEqual(source.read_bytes(), original, "CLI must preserve its input bytes")
                self.assertNotEqual(run.returncode, 0)
                self.assertIn("Output must not refer to the input ROM", run.stderr)

    def test_cli_rejects_short_and_modified_normalized_rom_without_output(self):
        root = Path(__file__).resolve().parents[1]
        private = root / ".local"
        private.mkdir(exist_ok=True)
        short = rom([0x03E00008, 0])
        full_size = bytearray(short + bytes(0x800000 - len(short)))
        full_size[59:63] = b"NTPE"
        for data, reason in ((short, "truncated_rom"),
                             (full_size, "unknown_revision_or_modified")):
            with self.subTest(reason=reason), tempfile.TemporaryDirectory(dir=private) as temp:
                source, output = Path(temp) / "synthetic.z64", Path(temp) / "map.json"
                source.write_bytes(data)
                run = subprocess.run([sys.executable, "-m", "tools.analysis.map_rom", str(source),
                                      "--output", str(output)], cwd=root,
                                     capture_output=True, text=True)
                self.assertNotEqual(run.returncode, 0)
                self.assertIn(f"Unsupported normalized NTPE rev0 ROM: {reason}", run.stderr)
                self.assertNotIn("Traceback", run.stderr)
                self.assertFalse(output.exists())
                self.assertEqual(source.read_bytes(), data)

    def test_checked_in_symbols_have_nonoverlapping_bounds_inside_reviewed_mapping(self):
        root = Path(__file__).resolve().parents[1]
        metadata = json.loads((root / "config/recomp/sections.json").read_text())
        symbols = tomllib.loads((root / "config/recomp/symbols.toml").read_text())
        reviewed = {item["name"]: item for item in metadata["sections"]}
        ranges = []
        for section in symbols["section"]:
            source = reviewed[section["name"]]
            self.assertEqual((section["rom"], section["vram"], section["size"]),
                             (source["rom_start"], source["ram_start"], source["size"]))
            self.assertEqual(source["status"], "confirmed")
            for function in section["functions"]:
                self.assertEqual(function["vram"] % 4, 0)
                self.assertEqual(function["size"] % 4, 0)
                self.assertGreater(function["size"], 0)
                self.assertGreaterEqual(function["vram"], section["vram"])
                self.assertLessEqual(function["vram"] + function["size"], section["vram"] + section["size"])
                ranges.append({"name": function["name"], "ram_start": function["vram"],
                               "rom_start": section["rom"] + function["vram"] - section["vram"],
                               "size": function["size"]})
        self.assertTrue(ranges)
        validate_sections(ranges)


if __name__ == "__main__":
    unittest.main()
