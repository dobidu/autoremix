#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_DIR="$SCRIPT_DIR/.."
PORT="${AUTOREMIX_PORT:-17432}"
TEMP_DIR="${AUTOREMIX_TEMP_DIR:-/tmp/autoremix}"

mkdir -p "$TEMP_DIR"

if [ ! -d "$PYTHON_DIR/.venv" ]; then
  echo "[autoremix] Creating Python venv..."
  uv venv "$PYTHON_DIR/.venv"
  uv pip install -r "$PYTHON_DIR/requirements.txt" --python "$PYTHON_DIR/.venv/bin/python"
fi

source "$PYTHON_DIR/.venv/bin/activate"

echo "[autoremix] Starting sidecar on port $PORT..."
cd "$PYTHON_DIR"
exec python -m server.main
