#!/usr/bin/env bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export AUTOREMIX_SERVER_PATH="$DIR/sidecar/server/main.py"
exec "$DIR/AutoRemix"
