import json
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.analysis.trace_runtime import (
    GDBPacketParser,
    GDBRemote,
    TraceEvent,
    _atomic_write_text,
    _ensure_distinct_output,
    collect_trace,
    load_capture,
    merge_trace,
    is_stop_reply,
    write_capture,
)


class GDBPacketParserTests(unittest.TestCase):
    @staticmethod
    def packet(payload: bytes) -> bytes:
        return b"$" + payload + b"#" + f"{sum(payload) & 0xff:02x}".encode()

    def test_fragmented_checksummed_packets_and_acknowledgements(self):
        parser = GDBPacketParser()
        packet = self.packet(b"T05thread:1;")
        self.assertEqual(parser.feed(b"+" + packet[:5]), [])
        self.assertEqual(parser.feed(packet[5:] + b"-+"), [b"T05thread:1;"])
        self.assertEqual(parser.acknowledgements, [b"+", b"-", b"+"])

    def test_parser_rejects_bad_checksum_and_oversize(self):
        parser = GDBPacketParser(max_packet_size=16)
        with self.assertRaisesRegex(ValueError, "checksum"):
            parser.feed(b"$OK#00")
        parser = GDBPacketParser(max_packet_size=16)
        with self.assertRaisesRegex(ValueError, "oversized"):
            parser.feed(b"$" + b"x" * 17)

    def test_ares_reset_signal_is_a_valid_stop_reply(self):
        self.assertTrue(is_stop_reply("S10"))
        self.assertTrue(is_stop_reply("T05thread:1;"))
        self.assertFalse(is_stop_reply("OK"))


class TraceMergeTests(unittest.TestCase):
    def setUp(self):
        self.static_map = {
            "entry_point": 0x80025C50,
            "detections": {
                "indirect_calls": [
                    {"address": 0x80026498, "target": None, "status": "detected"}
                ],
                "indirect_jumps": [
                    {"address": 0x80025C80, "target": None, "status": "detected"}
                ],
                "overlays": [
                    {"rom_start": 0x120000, "ram_start": 0x80100000,
                     "size": 0x1000, "status": "detected"}
                ],
                "boot_transfers": [
                    {"kind": "cpu_zero_loop", "ram_start": 0x800F2040,
                     "size": 0x72E20, "status": "detected"}
                ],
            },
        }

    def test_hit_counts_indirect_targets_and_load_ranges(self):
        events = [
            TraceEvent("breakpoint", 1.0, address=0x80025C50),
            TraceEvent("breakpoint", 2.0, address=0x80025C50),
            TraceEvent("indirect_target", 3.0, address=0x80026498,
                       target=0x80028964),
            TraceEvent("load_range", 4.0, address=0x80100000, size=0x1000,
                       rom_start=0x120000),
        ]
        result = merge_trace(self.static_map, events)
        self.assertEqual(result["hit_counts"]["0x80025c50"], 2)
        self.assertEqual(result["indirect_targets"][0]["target"], "0x80028964")
        self.assertEqual(result["load_ranges"][0]["size"], 0x1000)

    def test_static_dynamic_disagreement_is_explicit(self):
        result = merge_trace(
            self.static_map,
            [TraceEvent("load_range", 1.0, address=0x80200000, size=0x200,
                        rom_start=0x130000)],
        )
        self.assertEqual(result["rulings"]["overlays"][0]["status"], "open")
        self.assertIn("not present in the static candidates",
                      result["rulings"]["overlays"][0]["evidence"])
        self.assertEqual(result["rulings"]["overlays"][1]["status"], "open")
        self.assertIn("not observed", result["rulings"]["overlays"][1]["evidence"])

    def test_no_overlay_observation_stays_open_not_absent(self):
        static_map = {"entry_point": 0x80025C50, "detections": {
            "overlays": [], "indirect_calls": [], "boot_transfers": []}}
        result = merge_trace(static_map, [])
        self.assertEqual(result["rulings"]["overlay_summary"]["status"], "open")
        self.assertIn("does not prove absence",
                      result["rulings"]["overlay_summary"]["evidence"])

    def test_indirect_jumps_are_not_mislabeled_as_calls(self):
        result = merge_trace(self.static_map, [
            TraceEvent("indirect_target", 1.0, address=0x80025C80,
                       target=0x80029510),
            TraceEvent("indirect_target", 2.0, address=0x80026498,
                       target=0x8007EF9C),
        ])
        self.assertEqual(result["rulings"]["indirect_jumps"][0]["status"],
                         "confirmed")
        self.assertEqual(result["rulings"]["indirect_jumps"][0]["site"],
                         "0x80025c80")
        self.assertEqual(result["rulings"]["indirect_calls"][0]["site"],
                         "0x80026498")

    def test_invalid_event_is_rejected(self):
        for event in (
            TraceEvent("unknown", 1.0, address=1),
            TraceEvent("breakpoint", -1.0, address=1),
            TraceEvent("indirect_target", 1.0, address=1),
            TraceEvent("load_range", 1.0, address=1, size=0),
            TraceEvent("task", 1.0, address=1, task_kind="secret"),
        ):
            with self.subTest(event=event):
                with self.assertRaises(ValueError):
                    merge_trace(self.static_map, [event])

    def test_capture_is_redacted_and_round_trips(self):
        event = TraceEvent("task", 1.25, address=0x800DF714,
                           task_kind="graphics", descriptor_address=0x80001000,
                           metadata={"data_ptr": "0x80100000", "count": 3})
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "capture.jsonl"
            write_capture(path, [event])
            raw = path.read_text()
            self.assertNotIn("payload", raw)
            self.assertNotIn("bytes", raw)
            self.assertNotIn("rom_sha256", raw)
            self.assertEqual(load_capture(path), [event])

    def test_dma_source_is_not_mislabeled_as_task_descriptor(self):
        event = TraceEvent("task", 1.0, address=0x800D1B9C,
                           task_kind="unknown", source_address=0x80162820,
                           metadata={"register": "SP_DRAM_ADDR"})
        result = merge_trace(self.static_map, [event])
        self.assertEqual(result["tasks"][0]["source_address"], "0x80162820")
        self.assertNotIn("descriptor_address", result["tasks"][0])

    def test_capture_rejects_sensitive_or_malformed_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "capture.jsonl"
            path.write_text(json.dumps({"kind": "breakpoint", "timestamp": 0,
                                        "address": 1, "raw_bytes": "abcd"}) + "\n")
            with self.assertRaisesRegex(ValueError, "forbidden"):
                load_capture(path)
            path.write_text("[]\n")
            with self.assertRaisesRegex(ValueError, "object"):
                load_capture(path)

    def test_capture_atomic_failure_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            path = root / "capture.jsonl"
            path.write_text("preserve me\n")
            events = [TraceEvent("breakpoint", 0, address=1),
                      TraceEvent("unknown", 1, address=2)]
            with self.assertRaisesRegex(ValueError, "unknown"):
                write_capture(path, events)
            self.assertEqual(path.read_text(), "preserve me\n")
            self.assertEqual(list(root.glob(".trace-runtime-*")), [])

    def test_output_aliases_are_rejected_without_changing_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "input.json"
            source.write_text("preserve me")
            symlink = root / "alias-symlink.json"
            symlink.symlink_to(source)
            hardlink = root / "alias-hardlink.json"
            os.link(source, hardlink)
            for output in (source, symlink, hardlink, root / "." / "input.json"):
                with self.subTest(output=output):
                    with self.assertRaisesRegex(ValueError, "input"):
                        _ensure_distinct_output(output, [source])
                    self.assertEqual(source.read_text(), "preserve me")

    def test_atomic_replace_failure_preserves_output_and_cleans_temp(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = root / "merged.json"
            output.write_text("preserve me")
            with patch("tools.analysis.trace_runtime.os.replace",
                       side_effect=OSError("injected replace failure")):
                with self.assertRaisesRegex(OSError, "injected"):
                    _atomic_write_text(output, "replacement")
            self.assertEqual(output.read_text(), "preserve me")
            self.assertEqual(list(root.glob(".trace-runtime-*")), [])


class GDBRemoteTests(unittest.TestCase):
    @staticmethod
    def packet(payload: bytes) -> bytes:
        return b"$" + payload + b"#" + f"{sum(payload) & 0xff:02x}".encode()

    def test_fragmented_nak_retransmits_last_frame(self):
        class FakeSocket:
            def __init__(self, chunks):
                self.chunks = list(chunks)
                self.sent = []

            def sendall(self, data):
                self.sent.append(data)

            def recv(self, size):
                return self.chunks.pop(0)

            def settimeout(self, timeout):
                pass

            def close(self):
                pass

        packet = self.packet(b"OK")
        fake = FakeSocket([b"-", packet[:3], packet[3:]])
        with patch("tools.analysis.trace_runtime.socket.create_connection",
                   return_value=fake):
            remote = GDBRemote("::1", 19123)
        remote.send("qSupported")
        frame = remote._frame("qSupported")
        self.assertEqual(remote.receive(), b"OK")
        self.assertEqual(fake.sent.count(frame), 2)

    def test_repeated_naks_fail_boundedly(self):
        class FakeSocket:
            def __init__(self):
                self.sent = []

            def sendall(self, data):
                self.sent.append(data)

            def recv(self, size):
                return b"-"

            def settimeout(self, timeout):
                pass

            def close(self):
                pass

        fake = FakeSocket()
        with patch("tools.analysis.trace_runtime.socket.create_connection",
                   return_value=fake):
            remote = GDBRemote("::1", 19123)
        remote.send("qSupported")
        with self.assertRaisesRegex(ConnectionError, "NAK"):
            remote.receive()


class CollectorTests(unittest.TestCase):
    def test_collector_records_hit_and_never_issues_guest_write(self):
        class FakeRemote:
            instance = None

            def __init__(self, host, port):
                self.commands = []
                FakeRemote.instance = self

            def query(self, command):
                self.commands.append(command)
                if command == "?":
                    return "T05"
                return "OK"

            def send(self, command):
                self.commands.append(command)

            def receive(self, timeout=None):
                return b"T05thread:1;"

            def register(self, index):
                return {37: 0x80026498, 25: 0x8007EF9C}[index]

            def memory(self, address, size):
                return b"\x12\x34"[:size]

            def close(self):
                self.commands.append("D")

        points = [{"name": "callback", "kind": "indirect_target",
                   "address": 0x80026498, "register": 25, "max_hits": 1}]
        with patch("tools.analysis.trace_runtime.GDBRemote", FakeRemote):
            events = collect_trace("::1", 19123, points, max_events=2, seconds=1)
        self.assertEqual(events[0].target, 0x8007EF9C)
        self.assertFalse(any(command.startswith(("M", "P", "G"))
                             for command in FakeRemote.instance.commands))

    def test_collector_rejects_wrong_live_code_before_breakpoints(self):
        class FakeRemote:
            instance = None

            def __init__(self, host, port):
                self.commands = []
                FakeRemote.instance = self

            def query(self, command):
                self.commands.append(command)
                return "T05"

            def memory(self, address, size):
                return b"\x00" * size

            def close(self):
                self.commands.append("D")

        fingerprints = [{"address": "0x80000000", "size_bytes": 4,
                         "sha256": hashlib.sha256(b"good").hexdigest()}]
        with patch("tools.analysis.trace_runtime.GDBRemote", FakeRemote):
            with self.assertRaisesRegex(ValueError, "NTPE rev0"):
                collect_trace("::1", 19123, [], max_events=1, seconds=1,
                              code_fingerprints=fingerprints)
        self.assertEqual(FakeRemote.instance.commands, ["?", "D"])


if __name__ == "__main__":
    unittest.main()
