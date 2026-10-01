#!/usr/bin/env python3
"""Close a real Linux RT64 window and require an orderly process exit."""

import argparse
import os
import subprocess
import time


def tetrisphere_windows():
    result = subprocess.run(["wmctrl", "-lp"], capture_output=True, text=True,
                            check=True)
    return [line.split()[0] for line in result.stdout.splitlines()
            if "Tetrisphere M1" in line]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary")
    parser.add_argument("rom")
    args = parser.parse_args()
    old_windows = set(tetrisphere_windows())
    environment = dict(os.environ, SDL_VIDEODRIVER="x11")
    process = subprocess.Popen([args.binary, args.rom], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, env=environment)
    try:
        deadline = time.monotonic() + 15
        window = None
        while time.monotonic() < deadline and process.poll() is None:
            found = list(set(tetrisphere_windows()) - old_windows)
            if found:
                window = found[0]
                break
            time.sleep(0.2)
        if window is None:
            stdout, stderr = process.communicate(timeout=5)
            raise RuntimeError("RT64 window did not appear; "
                               f"exit={process.returncode}; "
                               f"stderr tail: {stderr[-300:].decode(errors='replace')}")
        time.sleep(3)
        subprocess.run(["wmctrl", "-ic", window], check=True)
        stdout, stderr = process.communicate(timeout=15)
        if process.returncode != 0:
            raise RuntimeError(f"close returned {process.returncode}; "
                               f"stderr tail: {stderr[-300:].decode(errors='replace')}")
        if b'"event":"rt64_device_ready"' not in stdout:
            raise RuntimeError("RT64 did not initialize before close")
        print("clean_shutdown exit=0 rt64_ready=true")
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()


if __name__ == "__main__":
    main()
