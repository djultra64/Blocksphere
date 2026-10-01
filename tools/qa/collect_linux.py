#!/usr/bin/env python3
"""Print a hardware snapshot without user names, host names or serial numbers."""
import datetime
import json
import os
from pathlib import Path
import platform
import re
import subprocess


def read(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None


def run(args):
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=15)
        return {"exit_code": p.returncode, "stdout": p.stdout.strip(), "stderr": p.stderr.strip()}
    except (OSError, subprocess.TimeoutExpired) as e:
        return {"unavailable": type(e).__name__}


def main():
    cpu = read('/proc/cpuinfo') or ''
    memory = read('/proc/meminfo') or ''
    inputs = read('/proc/bus/input/devices') or ''
    out = {
        "schema_version": 1,
        "recorded_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "os_release": read('/etc/os-release'), "kernel": platform.release(),
        "machine": platform.machine(), "product": read('/sys/class/dmi/id/product_name'),
        "cpu": next((s.split(':', 1)[1].strip() for s in cpu.splitlines() if s.startswith('model name')), None),
        "memory_total_kib": next((int(s.split()[1]) for s in memory.splitlines() if s.startswith('MemTotal:')), None),
        "memory_note": "OS-visible memory, not installed physical RAM; UMA reservation not inferred",
        "platform_profile": read('/sys/firmware/acpi/platform_profile'),
        "tdp_watts": None,
        "session": {k: os.environ.get(k) for k in ('XDG_SESSION_TYPE', 'XDG_CURRENT_DESKTOP')},
        "controllers": [], "power": [],
        "packages": run(['rpm', '-q', 'mesa-vulkan-drivers', 'mesa-dri-drivers', 'hhd', 'gamescope']),
        "ares": run(['flatpak', 'info', 'dev.ares.ares']),
    }
    for block in inputs.split('\n\n'):
        if re.search(r'Handlers=.*\bjs\d', block):
            out['controllers'].append({
                "name": next((s.removeprefix('N: Name=').strip('"') for s in block.splitlines() if s.startswith('N: Name=')), None),
                "bus_ids": next((s[3:] for s in block.splitlines() if s.startswith('I: ')), None),
                "independence_verified": False,
            })
    for p in Path('/sys/class/power_supply').glob('*'):
        out['power'].append({"device": p.name, **{k: read(p/k) for k in ('type', 'online', 'status', 'capacity')}})
    screens = run(['kscreen-doctor', '-j'])
    try:
        out['displays'] = []
        for o in json.loads(screens['stdout'])['outputs']:
            out['displays'].append({
                "connector": o['name'], "connected": o['connected'], "enabled": o['enabled'],
                "current_mode": next((m for m in o['modes'] if m['id'] == o.get('currentModeId')), None),
                "modes_4k": [m for m in o['modes'] if m['size']['width'] >= 3840],
                "scale": o.get('scale'),
            })
    except (KeyError, ValueError):
        out['displays'] = {"unavailable": True, "exit_code": screens.get('exit_code')}
    vk = run(['vulkaninfo', '--summary'])
    fields = ('apiVersion', 'driverVersion', 'vendorID', 'deviceID', 'deviceType', 'deviceName', 'driverID', 'driverName', 'driverInfo')
    out['vulkan'] = {"exit_code": vk.get('exit_code'), "fields": [s.strip() for s in vk.get('stdout', '').splitlines() if s.strip().startswith(fields)], "warnings": vk.get('stderr')}
    print(json.dumps(out, indent=2))


if __name__ == '__main__':
    main()
