# TamaInk

TamaInk turns an unlocked regular Xteink X3 or X4 into a dedicated, persistent
Tamagotchi P1 device. It boots directly into the emulator, maps each board's
three primary buttons to the original controls, and presents a purpose-built
monochrome interface.

> [!CAUTION]
> TamaInk is firmware for unlocked regular X3/X4 devices.
> Supported controllers are X3 UC8253/UC8279d and X4 SSD1677/UC8179/UC8279.
> X4 Pro and other devices are unsupported.

## What works

- Tamagotchi P1 emulation through TamaLib
- portrait 1-bit e-ink rendering with eight menu icons
- responsive physical A, B, and C controls
- on-device settings opened with the upper side button
- configurable battery display, display profile, and autosave interval
- confirmed reset of Tamagotchi save data
- user-supplied `rom.bin` loaded read-only from microSD
- two-generation, CRC-checked save recovery
- automatic saves every 5, 15, or 30 minutes
- RTC-based bounded catch-up after deep sleep on X3
- battery percentage display and guarded low-battery shutdown on X3
- power-button sleep and wake with a dedicated sleep screen
- configurable full-refresh cadence to limit e-ink ghosting
- reproducible application-only firmware builds
- repeatable return to working CrossPoint firmware

On X4, saved state resumes exactly but emulated time remains paused during deep
sleep because the board has no RTC. Its ADC battery estimate is displayed, but
automatic low-battery shutdown remains disabled pending threshold validation.

## Preparing the microSD card

You must supply your own compatible Tamagotchi P1 ROM.

1. Format a microSD card using a filesystem supported by the X3 or X4.
2. Place the ROM at the card root as `/rom.bin`.
3. Insert the card before starting TamaInk.

The ROM is treated as read-only. TamaInk stores its own recoverable state under
`/.tamaink/` and does not modify the ROM file directly.

## Controls

| Board control | Action |
| --- | --- |
| Back | Tamagotchi A |
| Confirm | Tamagotchi B |
| Power | Tamagotchi C |
| Upper side button | Open Settings |

Hold **Power** for at least two seconds, then release it, to save and enter deep
sleep. Press **Power** once to wake the device.

In Settings, **Back** moves to the next item, **Confirm** changes or selects the
current item, and **Power** closes the menu. Available options include:

- battery percentage: show or hide
- display profile: Smooth, Balanced, or Eco
- autosave interval: 5, 15, or 30 minutes
- reset Tamagotchi save data, protected by a confirmation screen

Resetting removes TamaInk's saved Tamagotchi state and starts again from the
beginning. It does not remove `/rom.bin` or reset the interface settings.

The eight bottom icons follow the original P1 order: Food, Light, Game,
Medicine, Toilet, Health, Discipline, and Attention.

## Firmware and flashing

TamaInk preserves CrossPoint's 16 MiB A/B partition contract, including the
ability to flash back to CrossPoint.

Normal releases provide an application-only image named `firmware.bin`. They do
not provide a merged full-flash image or replacement partition table.

The complete installation safety contract is documented in
[SCOPE.md](SCOPE.md#flash-compatibility-and-safety).

## Current release status (v0.2.0)

The following regular Xteink configurations are physically validated:

- X3 with the UC8253 display controller
- X4 with the SSD1677 display controller

Validation covers board and controller detection, display rendering, physical
controls, microSD access, emulation, settings, persistence, and sleep/wake
behavior. X3 validation additionally covers RTC catch-up, BQ27220 battery
telemetry, guarded low-battery shutdown, and repeated CrossPoint recovery.

Firmware supports X3 UC8279d and X4 UC8179/UC8279, but physical panel
validation for those variants remains pending; they are firmware-supported and
not policy-gated. X4 ADC battery telemetry is available, but automatic low-battery
shutdown remains disabled until its thresholds have been validated.

Release checks cover clean installation, missing or corrupt files,
documentation, licensing, reproducible builds, and packaged firmware artifacts.

## Development

TamaInk is a native ESP32 application:

- [TamaLib](https://github.com/jcrona/tamalib) provides P1 emulation.
- [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) provides the regular
  X3/X4 hardware layer.
- TamaInk owns ROM validation, rendering, persistence, settings, and power
  behavior.

Development follows narrow, hardware-safe gates. Read [SCOPE.md](SCOPE.md)
before contributing. Dependencies and adapted materials are recorded in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

### Containerized tests and builds

The complete host-test suite and reproducible X3 firmware build run in the
repository's Docker image (with a pinned Python base image). The source
checkout is mounted read-only and copied into an ephemeral container workspace;
Compose scopes the image and PlatformIO volume to its project name. From the
repository root (set `COMPOSE_PROJECT_NAME` to isolate or select a project):

```sh
docker compose build
docker compose run --rm test
```

The regular X3/X4 display-driver compile matrix is also container-only:

```sh
docker compose run --rm test drivers
```

The test container defaults to a four-CPU limit and four PlatformIO compiler
jobs. Set `TAMAINK_CPUS` and `TAMAINK_JOBS` to adjust them for a local machine;
`TAMAINK_JOBS` accepts values from 1 through 8. The memory limit defaults to
12 GiB and can be adjusted with `TAMAINK_MEMORY_LIMIT`.

To produce an application image, use the separate artifact command and an
explicit output mount:

```sh
mkdir -p dist
docker compose run --rm -e TAMAINK_OUTPUT=/output \
  -v "$(pwd)/dist:/output" test artifact
```

No host Docker socket is required. The canonical runner is
`scripts/container-test.sh`, used by both local Compose runs and CI.

## License

TamaInk is licensed under the
[GNU Affero General Public License v3.0](LICENSE).
