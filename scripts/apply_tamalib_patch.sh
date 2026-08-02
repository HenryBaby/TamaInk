#!/usr/bin/env bash
set -euo pipefail

expected='ce304d55f9a73c60232ce3f552e7983db3fa399c'
actual="$(git -C tamalib rev-parse HEAD)"
test "$actual" = "$expected" || { echo "unsupported tamalib revision: $actual" >&2; exit 1; }
test -z "$(git -C tamalib status --porcelain)" || { echo "tamalib worktree must be pristine" >&2; exit 1; }
git -C tamalib apply --check ../patches/tamalib-live-state.patch
git -C tamalib apply ../patches/tamalib-live-state.patch
