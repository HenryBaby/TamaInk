# TamaInk

TamaInk turns an Xteink X3 into a dedicated, persistent Tamagotchi P1 device.
It boots directly into the emulator, maps the X3's three physical buttons to
the original controls, and presents a purpose-built monochrome interface on
the e-ink display.

> [!CAUTION]
> Current builds are pre-release firmware for unlocked, directly
> USB-flashable Xteink X3 devices. Do not use this on any other device. It has
> currently been tested only with the UC8253 display controller.

## What works

- Tamagotchi P1 emulation through TamaLib
- portrait 1-bit e-ink rendering with eight menu icons
- responsive physical A, B, and C controls
- user-supplied `rom.bin` loaded read-only from microSD
- two-generation, CRC-checked save recovery
- automatic saves every 15 minutes and before controlled sleep
- RTC-based bounded catch-up after deep sleep
- battery percentage display and guarded low-battery shutdown
- power-button wake and a dedicated sleep screen
- periodic full refreshes to limit e-ink ghosting
- reproducible application-only firmware builds
- repeatable return to working CrossPoint firmware

## Preparing the microSD card

TamaInk does not include, distribute, download, or generate Tamagotchi ROMs.
You must supply your own compatible packed P1 ROM.

1. Format a microSD card using a filesystem supported by the X3.
2. Place the ROM at the card root as `/rom.bin`.
3. Insert the card before starting TamaInk.

The ROM is treated as read-only. TamaInk stores its own recoverable state under
`/.tamaink/` and does not modify the ROM.

## Controls

| X3 control | Tamagotchi control |
| --- | --- |
| Back | A |
| Confirm | B |
| Power | C |

Hold **Back + Power** together for at least two seconds, then release both, to
save and enter deep sleep. Press **Power** once to wake the device.

The eight bottom icons follow the original P1 order: Food, Light, Game,
Medicine, Toilet, Health, Discipline, and Attention.

## Firmware and flashing

TamaInk preserves CrossPoint's 16 MiB A/B partition contract. This preserves
the ability to flash back to CrossPoint.

The complete installation safety contract is documented in
[SCOPE.md](SCOPE.md#flash-compatibility-and-safety).

## Current release status

Hardware validation on the UC8253 X3 covers display refresh behavior, controls,
microSD access, RTC and battery readings, emulation, save recovery, autosave,
sleep/wake, bounded catch-up, low-battery protection, and repeated
CrossPoint ↔ TamaInk recovery.

Release-candidate preparation includes clean-install,
missing/corrupt-file, documentation, licensing, and packaged-artifact checks.

## Development

TamaInk is a native ESP32 application:

- [TamaLib](https://github.com/jcrona/tamalib) provides P1 emulation.
- [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) provides the X3
  hardware layer.
- TamaInk owns ROM validation, rendering, persistence, and power behavior.

Development follows narrow, hardware-safe gates. Read [SCOPE.md](SCOPE.md)
before contributing. Dependencies and adapted materials are recorded in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

TamaInk is licensed under the
[GNU Affero General Public License v3.0](LICENSE).
