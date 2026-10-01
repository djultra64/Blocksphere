"""Separate portable folders own their settings, regardless of working directory."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(dir=sys.argv[2]) as tmp:
    root = Path(tmp)
    env = dict(os.environ, XDG_DATA_HOME=str(root / 'unused-user-data'))
    env.pop('TETRISPHERE_DATA_DIR', None)
    for name in ('first', 'second', 'tools'):
        tools = root / name
        tools.mkdir(parents=True)
        shutil.copy2(binary, tools / binary.name)
        if os.name == 'nt':
            (tools / 'lib').mkdir()
            shutil.copy2(binary.parent / 'lib/SDL2.dll', tools / 'lib/SDL2.dll')
    config = root / 'first' / binary.name
    def run(path, *args):
        p = subprocess.run([str(path), *args], env=env, cwd=root,
                           capture_output=True, text=True)
        assert p.returncode == 0, p.stderr
        return p.stdout
    assert json.loads(run(config, 'show'))['graphics']['aspect'] == '16:9', 'Fresh folder must default to widescreen'
    run(config, 'graphics', 'aspect', '4:3')
    run(config, 'graphics', 'msaa', '4x')
    assert (root / 'first/data/settings.dat').is_file(), 'Settings escaped the game folder'
    assert not (root / 'first/tools/data').exists(), 'Configurator used tools/data'
    assert json.loads(run(root / 'second' / binary.name, 'show'))['graphics']['msaa_samples'] == 0, 'New installation reused another folder settings'
    assert json.loads(run(config, 'show'))['graphics']['msaa_samples'] == 4
    assert json.loads(run(config, 'show'))['graphics']['aspect'] == '4:3', 'Saved aspect must be preserved'
    # Moving the whole package preserves its data and does not bind to an old path.
    moved = root / 'moved package'
    shutil.move(root / 'first', moved)
    assert json.loads(run(moved / binary.name, 'show'))['graphics']['msaa_samples'] == 4
    run(root / 'tools' / binary.name, 'graphics', 'msaa', '2x')
    assert (root / 'tools/data/settings.dat').is_file(), 'Folder named tools escaped its own data directory'
    assert not (root / 'data').exists(), 'Configurator used another installation parent'
    assert not (root / 'unused-user-data').exists(), 'Portable app touched shared data'
print('portable settings isolation and relocation: PASS')
