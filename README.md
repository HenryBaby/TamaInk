# TamaInk

TamaInk turns an unlocked regular Xteink X3 or X4 into a dedicated, persistent Tamagotchi P1 device.
It boots directly into the emulator, maps each board's three primary buttons to the
original controls, and presents a purpose-built monochrome interface.

> [!CAUTION]
> Current builds are pre-release firmware for unlocked regular X3/X4 devices.
> Supported controllers are X3 UC8253 and X4 SSD1677. UC8279d and UC8179 are
> detected but disabled pending validation. X4 Pro and other devices are unsupported.

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
- RTC-based bounded catch-up after deep sleep (X3)
- battery percentage display; guarded low-battery shutdown (X3 only)
- power-button sleep and wake with a dedicated sleep screen
- configurable full-refresh cadence to limit e-ink ghosting
- reproducible application-only firmware builds
- repeatable return to working CrossPoint firmware

On X4, saved state resumes exactly but emulated time remains paused during deep
sleep because the board has no RTC. Its ADC battery estimate is displayed, but
automatic low-battery shutdown remains disabled pending hardware validation.

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

## Current release status

Hardware validation on the UC8253 X3 covers display refresh behavior, controls,
microSD access, RTC and battery readings, emulation, save recovery, autosave,
settings, sleep/wake, bounded catch-up, low-battery protection, and repeated
CrossPoint ↔ TamaInk recovery.

The X4 SSD1677 build path is implemented and host-tested but has not yet been
validated on physical X4 hardware.

Release-candidate checks cover clean installation, missing or corrupt files,
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

## License

TamaInk is licensed under the
[GNU Affero General Public License v3.0](LICENSE).
