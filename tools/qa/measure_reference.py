#!/usr/bin/env python3
"""Read NTPE rev0 counters through ares v148 GDB; never write guest memory."""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.rom.validate import MANIFEST, inspect_path

COUNT_HZ = 46_875_000


def decode_packet(packet):
    if not packet.startswith(b'$') or len(packet) < 4 or packet[-3:-2] != b'#':
        raise ValueError('Malformed GDB packet')
    payload = packet[1:-3]
    try:
        checksum = int(packet[-2:], 16)
    except ValueError:
        raise ValueError('Malformed GDB checksum') from None
    if sum(payload) % 256 != checksum:
        raise ValueError('GDB checksum mismatch')
    return payload.decode('ascii')


def summarize(before, after, wall_seconds):
    if wall_seconds <= 0:
        raise ValueError('Duration must be positive')
    deltas = {k: (after[k] - before[k]) & 0xffffffff for k in ('vi', 'common_updates', 'graphics_done')}
    game_keys = ('logic_ticks', 'state_updates')
    has_game = all(k in before and k in after for k in (*game_keys, 'mode_id'))
    valid_game = has_game and before['mode_id'] == after['mode_id'] and all(
        after[k] >= before[k] for k in game_keys)
    if has_game:
        # These game-owned counters reset on scene changes. Unlike the
        # OS counters, a decrease cannot safely be called a 32-bit wrap.
        deltas.update({k: after[k] - before[k] if valid_game else None for k in game_keys})
    emulated = ((after['os_ticks'] - before['os_ticks']) & 0xffffffffffffffff) / COUNT_HZ
    return {'wall_seconds': wall_seconds, 'deltas': deltas,
            'rates_per_wall_second': {k: v / wall_seconds if v is not None else None for k, v in deltas.items()},
            'valid_game_window': valid_game,
            'emulated_seconds': emulated, 'emulated_to_wall_ratio': emulated / wall_seconds}


class Remote:
    def __init__(self, port):
        self.socket = socket.create_connection(('::1', port), timeout=10)
        # ares's TCPText server requires this initial GDB acknowledgement.
        self.socket.sendall(b'+')
        self.buffer = b''

    def receive(self):
        while True:
            start = self.buffer.find(b'$')
            end = self.buffer.find(b'#', max(start, 0))
            if start >= 0 and end >= 0 and len(self.buffer) >= end + 3:
                packet = self.buffer[start:end + 3]
                self.buffer = self.buffer[end + 3:]
                payload = decode_packet(packet)
                self.socket.sendall(b'+')
                return payload
            chunk = self.socket.recv(65536)
            if not chunk:
                raise ConnectionError('ares closed the debug connection')
            self.buffer += chunk
            if len(self.buffer) > 65536:
                raise ValueError('Oversized GDB reply')

    def query(self, command):
        payload = command.encode('ascii')
        self.socket.sendall(b'$' + payload + b'#' + f'{sum(payload) % 256:02x}'.encode())
        return self.receive()

    def read(self, address, size):
        payload = self.query(f'm{address:x},{size:x}')
        if len(payload) != size * 2:
            raise ValueError('Unexpected memory reply length')
        return bytes.fromhex(payload)

    def verify_code(self):
        path = Path(__file__).resolve().parents[2] / "config/roms/tetrisphere-us-rev0-counters.json"
        for item in json.loads(path.read_text())["code_fingerprints"]:
            actual = hashlib.sha256(self.read(int(item["address"], 16), item["size_bytes"])).hexdigest()
            if actual != item["sha256"]:
                raise ValueError("Live code does not match the NTPE rev0 counter map")

    def snapshot(self):
        start = time.monotonic()
        common = self.read(0x800df714, 0x7d8)
        vi = self.read(0x80163b50, 16)
        game = self.read(0x800e4478, 0x32)
        result = {'graphics_done': int.from_bytes(common[:4], 'big'),
                  'common_updates': int.from_bytes(common[0x7d4:0x7d8], 'big'),
                  'vi': int.from_bytes(vi[12:16], 'big'),
                  'os_ticks': int.from_bytes(vi[:8], 'big'),
                  'logic_ticks': int.from_bytes(game[:4], 'big'),
                  'state_updates': int.from_bytes(game[8:12], 'big'),
                  'mode_id': int.from_bytes(game[48:50], 'big')}
        end = time.monotonic()
        return result, (start + end) / 2, end - start

    def close(self):
        try:
            self.query('D')
        finally:
            self.socket.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--scene', required=True)
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--port', type=int, default=19123)
    args = parser.parse_args()
    if not 0 < args.seconds <= 3600 or not 1 <= args.repeats <= 10:
        parser.error('Use seconds in (0, 3600] and repeats in [1, 10]')
    manifest = json.loads(MANIFEST.read_text())
    rom = inspect_path(args.rom, manifest)
    if not rom['recognized']:
        parser.error('Requires the recognized NTPE rev0 ROM')
    remote = Remote(args.port)
    try:
        remote.verify_code()
        for repeat in range(args.repeats):
            before, start, overhead_a = remote.snapshot()
            time.sleep(args.seconds)
            after, end, overhead_b = remote.snapshot()
            print(json.dumps({'schema_version': 2, 'scene': args.scene, 'repeat': repeat + 1,
                              'rom_sha256': rom['sha256'], 'before': before, 'after': after,
                              'sample_overhead_seconds': [overhead_a, overhead_b],
                              **summarize(before, after, end - start)}), flush=True)
    finally:
        failed = sys.exc_info()[0] is not None
        try:
            remote.close()
        except (OSError, ValueError) as exc:
            if not failed:
                raise
            print(f"Debug detach failed: {exc}", file=sys.stderr)


if __name__ == '__main__':
    main()
