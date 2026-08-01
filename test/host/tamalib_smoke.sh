#!/usr/bin/env bash
set -euo pipefail

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/tamalib"

# hal.h uses the upstream relative ../hal_types.h include. Supply the
# template in the temporary harness; no file in the submodule is changed.
cp tamalib/hal_types.h.template "$tmp/hal_types.h"
for f in cpu.c cpu.h hw.c hw.h tamalib.c tamalib.h hal.h; do
  cp "tamalib/$f" "$tmp/tamalib/$f"
done

# P1 is E0C6S46. The pinned cpu.h defines both models and gives S48 priority;
# remove only that default line in the temporary copy, then assert selection
# in test_tamalib_smoke.c. The checked-in upstream submodule remains pristine.
if [ "$(grep -Ec '^[[:space:]]*#define[[:space:]]+E0C6S48_SUPPORT[[:space:]]*$' "$tmp/tamalib/cpu.h")" -ne 1 ]; then
  echo "unexpected pinned cpu.h E0C6S48_SUPPORT define count" >&2
  exit 1
fi
sed -i '/^[[:space:]]*#define[[:space:]]\+E0C6S48_SUPPORT[[:space:]]*$/d' "$tmp/tamalib/cpu.h"
if grep -Eq '^[[:space:]]*#define[[:space:]]+E0C6S48_SUPPORT[[:space:]]*$' "$tmp/tamalib/cpu.h"; then
  echo "failed to remove pinned cpu.h E0C6S48_SUPPORT define" >&2
  exit 1
fi

cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I "$tmp/tamalib" \
  "$tmp/tamalib/cpu.c" "$tmp/tamalib/hw.c" "$tmp/tamalib/tamalib.c" \
  test/host/test_tamalib_smoke.c -o "$tmp/tamalib-smoke"
"$tmp/tamalib-smoke"
