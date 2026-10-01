"""Synthetic fixtures only: catch false acceptance and broken byte swapping."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

from tools.rom.validate import validate_bytes, inspect_path


class RomTests(unittest.TestCase):
    def setUp(self):
        # Own artificial data; no game bytes or code.
        self.data = bytearray(i % 251 for i in range(4096))
        self.data[:4] = bytes.fromhex("80371240")
        self.data[32:52] = b"SYNTHETIC TEST".ljust(20, b" ")
        self.data[59:63] = b"NTPE"
        self.data[63] = 0
        self.data[64:68] = bytes.fromhex("12345678")
        self.data = bytes(self.data)
        self.manifest = {"id": "synthetic", "size_bytes": len(self.data),
                         "sha256": hashlib.sha256(self.data).hexdigest()}
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def check_reason(self, data, reason):
        r = validate_bytes(data, self.manifest)
        self.assertFalse(r["recognized"])
        self.assertEqual(r["reason"], reason)

    def test_big_endian_identification(self):
        r = validate_bytes(self.data, self.manifest)
        self.assertTrue(r["recognized"])
        self.assertEqual(r["revision_id"], "synthetic")
        self.assertEqual(r["header"]["game_code"], "NTPE")

    def test_v64_normalizes_all_bytes(self):
        swapped = bytes(v for i in range(0, len(self.data), 2) for v in self.data[i:i+2][::-1])
        r = validate_bytes(swapped, self.manifest)
        self.assertTrue(r["recognized"])
        self.assertEqual(r["format"], "v64")
        self.assertEqual(r["sha256"], self.manifest["sha256"])

    def test_n64_normalizes_all_bytes(self):
        swapped = bytes(v for i in range(0, len(self.data), 4) for v in self.data[i:i+4][::-1])
        r = validate_bytes(swapped, self.manifest)
        self.assertTrue(r["recognized"])
        self.assertEqual(r["format"], "n64")

    def test_invalid_magic(self):
        self.check_reason(b"FAKE" + self.data[4:], "invalid_header")

    def test_short_header(self):
        for n in (0, 3, 63):
            with self.subTest(n=n):
                self.check_reason(self.data[:n], "truncated_header")

    def test_truncation(self):
        self.check_reason(self.data[:-4], "truncated_rom")

    def test_unaligned_dump(self):
        self.check_reason(self.data[:-1], "unaligned_size")

    def test_extra_bytes(self):
        self.check_reason(self.data + b"\0" * 4, "unexpected_size")

    def test_other_revision(self):
        d = bytearray(self.data)
        d[63] = 1
        self.check_reason(d, "unknown_revision_or_modified")

    def test_pal_is_not_accepted_by_title(self):
        d = bytearray(self.data)
        d[62] = ord("P")
        self.check_reason(d, "unknown_revision_or_modified")

    def test_modified_payload_with_same_header(self):
        d = bytearray(self.data)
        d[-1] ^= 1
        self.check_reason(d, "unknown_revision_or_modified")

    def test_raw_format_ignores_extension(self):
        p = self.root / "misleading.n64"
        p.write_bytes(self.data)
        self.assertEqual(inspect_path(p, self.manifest)["format"], "z64")

    def test_zip_reads_one_rom_without_extracting(self):
        p = self.root / "rom.zip"
        with zipfile.ZipFile(p, "w", zipfile.ZIP_DEFLATED) as z:
            z.writestr("../../example.Z64", self.data)
            z.writestr("readme.txt", "synthetic")
        self.assertTrue(inspect_path(p, self.manifest)["recognized"])
        self.assertEqual(list(self.root.iterdir()), [p])

    def test_zip_multiple_roms_rejected(self):
        p = self.root / "rom.zip"
        with zipfile.ZipFile(p, "w") as z:
            z.writestr("one.z64", self.data)
            z.writestr("two.v64", self.data)
        self.assertEqual(inspect_path(p, self.manifest)["reason"], "ambiguous_archive")

    def test_zip_no_rom(self):
        p = self.root / "empty.zip"
        with zipfile.ZipFile(p, "w") as z:
            z.writestr("notes.txt", "empty")
        self.assertEqual(inspect_path(p, self.manifest)["reason"], "no_rom_in_archive")

    def test_zip_oversized_payload_rejected(self):
        p = self.root / "large.zip"
        with zipfile.ZipFile(p, "w", zipfile.ZIP_DEFLATED) as z:
            z.writestr("rom.z64", self.data + b"\0" * 100000)
        self.assertEqual(inspect_path(p, self.manifest)["reason"], "unexpected_size")

    def test_broken_zip(self):
        p = self.root / "broken.zip"
        p.write_bytes(b"PK\x03\x04bad")
        self.assertEqual(inspect_path(p, self.manifest)["reason"], "invalid_archive")

    def test_missing_file(self):
        self.assertEqual(inspect_path(self.root / "absent", self.manifest)["reason"], "io_error")

    def test_cli_rejection_is_json_and_nonzero(self):
        script = Path(__file__).resolve().parents[1] / "tools/rom/validate.py"
        p = subprocess.run([sys.executable, str(script), str(self.root / "absent")],
                           capture_output=True, text=True, cwd=self.root)
        self.assertEqual(p.returncode, 1)
        self.assertEqual(json.loads(p.stdout)["reason"], "io_error")
        self.assertEqual(p.stderr, "")


if __name__ == "__main__":
    unittest.main()
