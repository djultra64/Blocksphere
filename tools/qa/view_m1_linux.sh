#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
binary="${repo_root}/.local/m1/build-linux/tetrisphere-m1"
rom="${repo_root}/.local/m0/tetrisphere-us.z64"

if [[ ! -x "${binary}" ]]; then
    echo "No está la build Linux de M1: ${binary}" >&2
    echo "Consulta docs/qa/m1-linux-protocol.md para reconstruirla." >&2
    exit 2
fi

if [[ ! -f "${rom}" ]]; then
    echo "No está la copia privada de la ROM autorizada: ${rom}" >&2
    exit 2
fi

echo "Abriendo Tetrisphere M1. Cierra la ventana para terminar."
cd -- "${repo_root}"
exec "${binary}" "${rom}"
