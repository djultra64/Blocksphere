#!/usr/bin/env python3
"""Exercise the native Close game menu with isolated data and a private X display.

Screenshots and logs contain ROM-derived content: keep --output under .local.
Virtual-controller coverage does not establish physical-controller validation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--boot-seconds", type=float, default=60)
    parser.add_argument("--input", choices=("both", "keyboard", "virtual-controller"),
                        default="both")
    parser.add_argument("--aspect", choices=("4:3", "16:9"), default="4:3")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.relative_to((root / ".local").resolve())
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    env = dict(os.environ, SDL_VIDEODRIVER="x11", SDL_AUDIODRIVER="dummy",
               VK_DRIVER_FILES="/usr/share/vulkan/icd.d/lvp_icd.x86_64.json",
               VK_ICD_FILENAMES="/usr/share/vulkan/icd.d/lvp_icd.x86_64.json")
    env.pop("TETRISPHERE_CONTROLLER_FAMILY", None)
    env.pop("TETRISPHERE_QA_CONTROLLERS", None)
    binary = args.binary.resolve()
    summary = {"binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
               "aspect": args.aspect,
               "cases": [], "physical_controllers_tested": False}
    game = None
    read_fd, write_fd = os.pipe()
    with (output / "xvfb.log").open("wb") as xlog:
        xvfb = subprocess.Popen(
            ["Xvfb", "-displayfd", str(write_fd), "-screen", "0",
             "1280x720x24", "-nolisten", "tcp"], pass_fds=(write_fd,),
            stdout=xlog, stderr=subprocess.STDOUT)
    os.close(write_fd)
    try:
        if not select.select([read_fd], [], [], 15)[0]:
            raise RuntimeError("private X display failed to start")
        display = os.read(read_fd, 64).decode().strip()
        if not display.isdigit():
            raise RuntimeError("private X display unavailable")
        env["DISPLAY"] = ":" + display
        modes = ("keyboard", "virtual-controller") if args.input == "both" else (args.input,)
        for mode in modes:
            case = output / mode
            case.mkdir(mode=0o700)
            data = case / "data"
            data.mkdir(mode=0o700)
            subprocess.run([str(binary.with_name("tetrisphere-config")),
                            "--data-dir", str(data), "graphics", "aspect", args.aspect],
                           check=True, capture_output=True, timeout=10)
            env["TETRISPHERE_DATA_DIR"] = str(data)
            actions = data / "actions.jsonl"
            seq = 0
            if mode == "virtual-controller":
                actions.write_text("")
                env["TETRISPHERE_QA_CONTROLLERS"] = str(actions)
            else:
                env.pop("TETRISPHERE_QA_CONTROLLERS", None)
            started = time.monotonic()
            with (case / "stdout.log").open("wb") as out, \
                    (case / "stderr.log").open("wb") as err:
                game = subprocess.Popen([str(binary), "--windowed", str(args.rom.resolve())],
                                        cwd=root, env=env, stdout=out, stderr=err)

            def alive_wait(seconds):
                deadline = time.monotonic() + seconds
                while time.monotonic() < deadline:
                    if game.poll() is not None:
                        raise RuntimeError(f"unexpected game exit {game.returncode}")
                    time.sleep(min(.2, max(0, deadline - time.monotonic())))

            window = None
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                found = subprocess.run(
                    ["xdotool", "search", "--onlyvisible", "--pid", str(game.pid)],
                    env=env, text=True, capture_output=True, timeout=5)
                if found.stdout.strip():
                    window = found.stdout.splitlines()[-1]
                    break
                alive_wait(.25)
            if window is None:
                raise RuntimeError("native game window unavailable")
            alive_wait(max(0, args.boot_seconds - (time.monotonic() - started)))

            def capture(label):
                subprocess.run(["import", "-window", window, str(case / (label + ".png"))],
                               env=env, check=True, capture_output=True, timeout=15)
                print(json.dumps({"case": mode, "capture": label}), flush=True)

            def key(name):
                subprocess.run(["xdotool", "key", "--delay", "180", "--window",
                                window, name], env=env, check=True, capture_output=True,
                               timeout=8)
                alive_wait(1.2)

            def virtual(button, pressed):
                nonlocal seq
                seq += 1
                with actions.open("a") as stream:
                    stream.write(json.dumps({"seq": seq, "port": 0,
                                             "button": button, "pressed": pressed}) + "\n")
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    if f'"seq":{seq},' in (case / "stdout.log").read_text():
                        return
                    alive_wait(.01)
                raise RuntimeError("virtual controller action unacknowledged")

            capture("00-menu")
            if mode == "keyboard":
                key("Up")
                capture("00-options-model")
                key("Down")
                key("Down")
                capture("01-close-game-selected")
                key("Down")
                capture("02-down-wrap-single")
                key("z")
                alive_wait(4)
                capture("03-original-single-menu")
                key("x")
                alive_wait(4)
                capture("04-back-main")
                key("Up")
                capture("05-up-wrap-close-game")
                subprocess.run(["xdotool", "key", "--delay", "180", "--window",
                                window, "z"], env=env, check=True,
                               capture_output=True, timeout=8)
            else:
                virtual("down", True)
                alive_wait(.03)
                virtual("down", False)
                alive_wait(1.2)
                capture("01-close-game-selected")
                virtual("south", True)
            code = game.wait(timeout=15)
            log = (case / "stdout.log").read_text(errors="replace")
            if code != 0 or '"event":"close_game_requested"' not in log:
                raise RuntimeError(f"{mode}: missing Close game shutdown, exit={code}")
            summary["cases"].append({"input": mode, "exit_code": code,
                                     "close_game_requested": True})
            print(json.dumps(summary["cases"][-1]), flush=True)
            game = None
        summary["passed"] = True
    finally:
        os.close(read_fd)
        if game is not None and game.poll() is None:
            game.terminate()
            try:
                game.wait(timeout=5)
            except subprocess.TimeoutExpired:
                game.kill()
                game.wait(timeout=5)
        xvfb.terminate()
        xvfb.wait(timeout=5)
        (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
