import json
from pathlib import Path
import struct
import unittest
import hashlib
import tempfile

from tools.analysis.microcode import (
    MicrocodeError,
    audio_output_ranges,
    classify_task,
    enumerate_commands,
    parse_ostask,
    validate_manifest,
)
from tools.build.verify_microcode_inputs import verify_inputs


ROOT = Path(__file__).resolve().parents[1]


def descriptor(task_type=1, data_ptr=0x80200000, data_size=16,
               ucode=0x80002000, ucode_data=0x80003000):
    return struct.pack(
        ">16I", task_type, 0, 0x80001000, 0xD0, ucode, 0,
        ucode_data, 0x800, 0, 0, 0, 0, data_ptr, data_size, 0, 0,
    )


class OSTaskTests(unittest.TestCase):
    def test_classifies_real_descriptor_kinds_without_using_mmio_source(self):
        self.assertEqual(classify_task(parse_ostask(descriptor(1))), "graphics")
        self.assertEqual(classify_task(parse_ostask(descriptor(2))), "audio")
        with self.assertRaisesRegex(MicrocodeError, "unsupported OSTask type"):
            classify_task(parse_ostask(descriptor(7)))

    def test_rejects_descriptor_region_that_crosses_rdram(self):
        with self.assertRaisesRegex(MicrocodeError, "data_ptr.*outside RDRAM"):
            parse_ostask(descriptor(data_ptr=0x807FFFF8, data_size=16))

    def test_rejects_truncated_descriptor(self):
        with self.assertRaisesRegex(MicrocodeError, "exactly 64"):
            parse_ostask(b"\0" * 63)


class CommandTests(unittest.TestCase):
    def test_gbi_inventory_preserves_full_opcode_and_fixed_width(self):
        commands = enumerate_commands(
            bytes.fromhex("bc00000000000000ff00000012345678"),
            kind="gbi", supported_opcodes={0xBC},
        )
        self.assertEqual(
            [(item.offset, item.opcode, item.size, item.supported)
             for item in commands],
            [(0, 0xBC, 8, True), (8, 0xFF, 8, False)],
        )

    def test_retains_unsupported_audio_command_with_offset(self):
        commands = enumerate_commands(
            bytes.fromhex("0700000000000000fe00000012345678"),
            kind="audio", supported_opcodes={0x07},
        )
        self.assertEqual(
            [(item.offset, item.opcode, item.supported) for item in commands],
            [(0, 0x07, True), (8, 0xFE, False)],
        )

    def test_uses_rt64_rdp_command_lengths(self):
        triangle_shaded_textured_depth = bytes.fromhex("0f00000000000000") * 22
        texrect = bytes.fromhex("e400000000000000") * 2
        commands = enumerate_commands(
            triangle_shaded_textured_depth + texrect,
            kind="rdp", supported_opcodes={0x0F, 0x24},
        )
        self.assertEqual([(item.offset, item.size) for item in commands],
                         [(0, 176), (176, 16)])

    def test_rejects_partial_command_instead_of_dropping_it(self):
        with self.assertRaisesRegex(MicrocodeError, "truncated.*offset 0"):
            enumerate_commands(bytes.fromhex("e400000000000000"), kind="rdp",
                               supported_opcodes={0x24})

    def test_derives_pcm_save_ranges_from_audio_set_and_save_commands(self):
        stream = bytes.fromhex(
            "0800000000000280"  # set buffer: 0x280 output bytes
            "06000000002c1760"  # save output buffer
            "0800000000000040"
            "06000000002c2160"
        )
        self.assertEqual(audio_output_ranges(stream),
                         [(0x002C1760, 0x280), (0x002C2160, 0x40)])

    def test_rejects_audio_save_without_a_bounded_buffer_size(self):
        with self.assertRaisesRegex(MicrocodeError, "save.*set buffer"):
            audio_output_ranges(bytes.fromhex("06000000002c1760"))


class ManifestTests(unittest.TestCase):
    def test_committed_manifest_has_no_private_paths_and_retains_backlog(self):
        manifest = json.loads(
            (ROOT / "config/recomp/microcodes.json").read_text(encoding="utf-8"))
        validate_manifest(manifest)
        serialized = json.dumps(manifest)
        self.assertNotIn(".local", serialized)
        self.assertNotIn("rom_sha256", serialized)
        self.assertEqual(manifest["graphics"]["rt64_identity"], "F3D_SM64_FINAL")
        self.assertTrue(manifest["audio"]["rsp_recomp"]["required"])

    def test_manifest_rejects_success_without_evidence_hash(self):
        value = {
            "schema_version": 1,
            "graphics": {"status": "accepted", "capture_sha256": ""},
            "audio": {"status": "pcm_proven", "capture_sha256": ""},
        }
        with self.assertRaisesRegex(MicrocodeError, "capture_sha256"):
            validate_manifest(value)


class InputReceiptTests(unittest.TestCase):
    def make_inputs(self, root: Path):
        graphics = bytearray(0x4000)
        audio = bytearray(0x4000)
        graphics_commands = bytes.fromhex("bc00000000000000ff00000000000000")
        audio_commands = bytes.fromhex("08000000000000040600000000003000")
        graphics[0x100:0x140] = descriptor(
            1, data_ptr=0x80003000, data_size=len(graphics_commands),
            ucode=0x80001000, ucode_data=0x80002000)
        graphics[0x1000:0x1010] = b"graphics-ucode!!"
        graphics[0x2000:0x2008] = b"gfx-data"
        graphics[0x3000:0x3010] = graphics_commands
        audio[0x200:0x240] = descriptor(
            2, data_ptr=0x80003000, data_size=len(audio_commands),
            ucode=0x80001000, ucode_data=0x80002000)
        audio[0x1000:0x1010] = b"audio-ucode-1234"
        audio[0x3000:0x3010] = audio_commands
        gfx_path = root / "gfx.bin"
        audio_path = root / "audio.bin"
        commands_path = root / "gfx-commands.bin"
        gfx_path.write_bytes(graphics)
        audio_path.write_bytes(audio)
        commands_path.write_bytes(graphics_commands)
        digest = lambda value: hashlib.sha256(value).hexdigest()
        manifest = {
            "schema_version": 1,
            "graphics": {
                "capture_sha256": digest(bytes(graphics[0x100:0x140])),
                "snapshot_sha256": digest(graphics),
                "descriptor_offset": "0x100",
                "text": {"size": 16, "sha256": digest(graphics[0x1000:0x1010])},
                "data": {"size": 8, "sha256": digest(graphics[0x2000:0x2008])},
                "display_list": {"bytes": 16, "sha256": digest(graphics_commands)},
            },
            "audio": {
                "capture_sha256": digest(bytes(audio[0x200:0x240])),
                "snapshot_sha256": digest(audio),
                "descriptor_offset": "0x200",
                "command_bytes": 16,
                "command_sha256": digest(audio_commands),
                "rsp_recomp": {"text_size": 16,
                               "ucode_sha256": digest(audio[0x1000:0x1010])},
            },
        }
        manifest_path = root / "microcodes.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        return manifest_path, gfx_path, commands_path, audio_path

    def test_receipt_binds_snapshots_descriptors_commands_and_ucodes(self):
        with tempfile.TemporaryDirectory() as temp:
            paths = self.make_inputs(Path(temp))
            receipt = verify_inputs(*paths)
            self.assertRegex(receipt["source_identity"], r"^[0-9a-f]{64}$")
            self.assertEqual(receipt["audio_ucode_sha256"],
                             json.loads(paths[0].read_text())["audio"]
                             ["rsp_recomp"]["ucode_sha256"])

    def test_every_declared_private_input_mutation_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            paths = self.make_inputs(Path(temp))
            for index in range(1, 4):
                manifest, gfx, commands, audio = paths
                target = paths[index]
                original = target.read_bytes()
                changed = bytearray(original)
                changed[-1] ^= 1
                target.write_bytes(changed)
                with self.assertRaisesRegex(ValueError, "hash mismatch"):
                    verify_inputs(manifest, gfx, commands, audio)
                target.write_bytes(original)

    def test_audio_ucode_mutation_fails_even_if_descriptor_and_address_match(self):
        with tempfile.TemporaryDirectory() as temp:
            manifest, gfx, commands, audio = self.make_inputs(Path(temp))
            changed = bytearray(audio.read_bytes())
            changed[0x1000] ^= 1
            # Simulate an attacker updating only the broad snapshot digest while
            # retaining the declared captured microcode identity.
            data = json.loads(manifest.read_text())
            data["audio"]["snapshot_sha256"] = hashlib.sha256(changed).hexdigest()
            manifest.write_text(json.dumps(data), encoding="utf-8")
            audio.write_bytes(changed)
            with self.assertRaisesRegex(ValueError, "audio ucode hash mismatch"):
                verify_inputs(manifest, gfx, commands, audio)


if __name__ == "__main__":
    unittest.main()
