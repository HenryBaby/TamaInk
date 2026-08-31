#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git show -s --format=%ct HEAD)}"

TAMAINK_JOBS="${TAMAINK_JOBS:-4}"
if ! [[ "$TAMAINK_JOBS" =~ ^[0-9]+$ ]]; then
  echo 'TAMAINK_JOBS must be an integer from 1 through 8' >&2
  exit 2
fi
TAMAINK_JOBS_NUMBER=$((10#$TAMAINK_JOBS))
if (( TAMAINK_JOBS_NUMBER < 1 || TAMAINK_JOBS_NUMBER > 8 )); then
  echo 'TAMAINK_JOBS must be an integer from 1 through 8' >&2
  exit 2
fi
PIO_JOBS="$TAMAINK_JOBS_NUMBER"

run_cpp() {
  local output="$1"
  shift
  g++ -std=c++17 -Wall -Wextra -Werror "$@" -o "/tmp/$output"
  "/tmp/$output"
}

export_firmware() {
  local output_dir
  output_dir="$(realpath "$TAMAINK_OUTPUT")"
  install -D -m 0644 .pio/build/x3/firmware.bin "$output_dir/firmware.bin"
  test -s "$output_dir/firmware.bin"
  sha256sum "$output_dir/firmware.bin"
  printf 'artifact: %s\n' "$output_dir/firmware.bin"
}

build_firmware() {
  pio run -e x3 -j "$PIO_JOBS"
  cp .pio/build/x3/firmware.bin /tmp/tamaink-firmware-first.bin
  pio run -e x3 --target clean -j "$PIO_JOBS"
  pio run -e x3 -j "$PIO_JOBS"
  cmp /tmp/tamaink-firmware-first.bin .pio/build/x3/firmware.bin
  sha256sum .pio/build/x3/firmware.bin
  if [[ -n "${TAMAINK_OUTPUT:-}" ]]; then
    mkdir -p "$TAMAINK_OUTPUT"
    export_firmware
  fi
}

build_driver_matrix() {
  cd verification/freeink-display-drivers
  pio run -e xteink -e xteink_x4 -j "$PIO_JOBS"
}

if [[ "${1:-test}" == artifact ]]; then
  bash scripts/apply_tamalib_patch.sh
  test -n "${TAMAINK_OUTPUT:-}" || { echo 'TAMAINK_OUTPUT is required for artifact mode' >&2; exit 2; }
  build_firmware
  exit 0
fi

if [[ "${1:-test}" == drivers ]]; then
  build_driver_matrix
  exit 0
fi

test "${1:-test}" = test || { echo "usage: $0 [test|drivers|artifact]" >&2; exit 2; }
bash scripts/apply_tamalib_patch.sh

run_cpp tamaink-persistence-tests -fsanitize=address,undefined -I include src/tamaink_persistence.cpp src/tamaink_emulator_state.cpp test/host/test_persistence.cpp
run_cpp tamaink-autosave-tests -I include test/host/test_autosave.cpp
run_cpp tamaink-battery-telemetry-tests -I include src/tamaink_battery_telemetry.cpp test/host/test_battery_telemetry.cpp
run_cpp tamaink-battery-policy-tests -I include test/host/test_battery_policy.cpp
run_cpp tamaink-low-battery-sleep-tests -I include src/tamaink_low_battery_sleep.cpp test/host/test_low_battery_sleep.cpp
run_cpp tamaink-wake-tests -I include test/host/wake_diagnostic_test.cpp
run_cpp tamaink-rtc-sleep-tests -I include src/tamaink_rtc_sleep_gate.cpp test/host/test_rtc_sleep_gate.cpp
run_cpp tamaink-wake-catchup-plan-tests -I include src/tamaink_wake_catchup_plan.cpp test/host/test_wake_catchup_plan.cpp
run_cpp tamaink-wake-catchup-tests -I include src/tamaink_wake_catchup.cpp test/host/test_wake_catchup.cpp
run_cpp tamaink-clock-tests -I include src/tamaink_clock.cpp test/host/test_clock.cpp
run_cpp tamaink-settings-tests -I include src/tamaink_settings.cpp test/host/test_settings.cpp
run_cpp tamaink-board-policy-tests -I include test/host/test_board_policy.cpp
run_cpp tamaink-display-variant-tests -I include test/host/test_display_variant.cpp
run_cpp tamaink-rom-tests -fsanitize=address,undefined -I include src/tamaink_rom.cpp test/host/test_rom.cpp
bash test/host/tamalib_smoke.sh
bash test/host/tamalib_adapter_determinism.sh
bash test/host/tamalib_adapter_continuation.sh
bash test/host/run_emulator_state_tests.sh
run_cpp tamaink-renderer-tests -fsanitize=address,undefined -I include src/tamaink_renderer.cpp src/tamaink_settings.cpp test/host/test_renderer.cpp
run_cpp tamaink-renderer-refresh-tests -fsanitize=address,undefined -I include src/tamaink_renderer_refresh.cpp test/host/renderer_refresh_test.cpp
run_cpp tamaink-renderer-dispatch-tests -fsanitize=address,undefined -I include src/tamaink_renderer_dispatch.cpp test/host/renderer_dispatch_test.cpp
run_cpp tamaink-renderer-layout-tests -I include test/host/renderer_layout_test.cpp
run_cpp tamaink-sleep-tests -fsanitize=address,undefined -I include -I freeink-sdk/libs/ui/FreeInkUI/include src/tamaink_sleep_screen.cpp test/host/sleep_screen_test.cpp
build_firmware
