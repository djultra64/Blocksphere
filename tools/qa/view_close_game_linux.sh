#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
binary="${repo_root}/.local/close-game/build-linux/tetrisphere-m1"
rom="${repo_root}/.local/m0/tetrisphere-us.z64"

if [[ ! -x "${binary}" || ! -f "${rom}" ]]; then
    echo "Falta la build de prueba o la ROM privada." >&2
    exit 2
fi

export TETRISPHERE_DATA_DIR="${repo_root}/.local/close-game/manual-data"
mkdir -p -- "${TETRISPHERE_DATA_DIR}"
echo "Prueba Close game: usa flechas para seleccionar y Z para confirmar."
cd -- "${repo_root}"
exec "${binary}" --windowed "${rom}"
