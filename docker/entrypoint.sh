#!/usr/bin/env bash
set -euo pipefail

test -d /src || { echo 'TamaInk source mount /src is required' >&2; exit 2; }
cd /
rm -rf /workspace
mkdir -p /workspace
cp -a /src/. /workspace/
# Keep build state ephemeral even when the checkout already has local output.
rm -rf /workspace/.pio /workspace/.venv
chmod -R u+w /workspace
git config --global --add safe.directory /workspace
git config --global --add safe.directory /workspace/tamalib
git config --global --add safe.directory /workspace/freeink-sdk
cd /workspace
case "${1:-}" in
  test|artifact|drivers)
    exec /workspace/scripts/container-test.sh "$@"
    ;;
  *)
    exec "$@"
    ;;
esac
