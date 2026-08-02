#!/usr/bin/env bash
set -euo pipefail

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I include src/tamaink_emulator_state.cpp test/host/test_emulator_state.cpp \
  -o "$tmpdir/test"
"$tmpdir/test" > "$tmpdir/out1"
"$tmpdir/test" > "$tmpdir/out2"
cmp "$tmpdir/out1" "$tmpdir/out2"
cat "$tmpdir/out1"
