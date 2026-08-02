#!/usr/bin/env bash
set -euo pipefail
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/tamalib"
cp tamalib/hal_types.h.template "$tmp/hal_types.h"
for f in cpu.c cpu.h hw.c hw.h tamalib.c tamalib.h hal.h; do cp "tamalib/$f" "$tmp/tamalib/$f"; done
if [ "$(grep -Ec '^[[:space:]]*#define[[:space:]]+E0C6S48_SUPPORT[[:space:]]*$' "$tmp/tamalib/cpu.h")" -ne 1 ]; then exit 1; fi
sed -i '/^[[:space:]]*#define[[:space:]]\+E0C6S48_SUPPORT[[:space:]]*$/d' "$tmp/tamalib/cpu.h"
if grep -Eq '^[[:space:]]*#define[[:space:]]+E0C6S48_SUPPORT[[:space:]]*$' "$tmp/tamalib/cpu.h"; then exit 1; fi
if [ "$(grep -Ec '^[[:space:]]*#define[[:space:]]+E0C6S46_SUPPORT[[:space:]]*$' "$tmp/tamalib/cpu.h")" -ne 1 ]; then exit 1; fi
flags=(-std=c11 -Wall -Wextra -Werror -Wno-error=unused-parameter -fsanitize=address,undefined -I "$tmp/tamalib")
objs=(); for source in cpu hw tamalib; do cc "${flags[@]}" -c "$tmp/tamalib/$source.c" -o "$tmp/$source.o"; objs+=("$tmp/$source.o"); done
g++ -std=c++17 -Wall -Wextra -Werror -DTAMAINK_HOST_TAMALIB=1 -fsanitize=address,undefined -I include -I "$tmp/tamalib" -c src/tamaink_tamalib.cpp -o "$tmp/adapter.o"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I include -c src/tamaink_clock.cpp -o "$tmp/clock.o"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I include -c src/tamaink_emulator_state.cpp -o "$tmp/state.o"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I include -I "$tmp/tamalib" -c test/host/test_tamalib_adapter.cpp -o "$tmp/test.o"
g++ -fsanitize=address,undefined "${objs[@]}" "$tmp/adapter.o" "$tmp/clock.o" "$tmp/state.o" "$tmp/test.o" -o "$tmp/adapter-test"
first="$("$tmp/adapter-test")"; second="$("$tmp/adapter-test")"; test "$first" = "$second"; printf '%s\n' "$first"
