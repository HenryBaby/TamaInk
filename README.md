# TamaInk

TamaInk is dedicated, open-source firmware that aims to turn the Xteink X3
into a standalone Tamagotchi P1 device.

Rather than running as an application inside an e-reader interface, TamaInk
boots directly into the emulated device. The finished experience is intended
to feel focused and appliance-like: three physical buttons, a persistent
Tamagotchi, and a purpose-built interface on the X3's e-ink display.

> [!WARNING]
> TamaInk is in early hardware bring-up. Current builds are diagnostic firmware,
> not a usable Tamagotchi emulator. Do not flash them unless you are actively
> participating in development and have a directly USB-flashable Xteink X3.

## Project status

Development follows a sequence of hardware-safety gates. Each subsystem is
proven independently before later work is allowed to depend on it.

| Area | Status |
| --- | --- |
| Deterministic application-only builds | Complete |
| Reversible CrossPoint flashing | Validated on development hardware |
| X3 and display-controller detection | Validated on a UC8253 X3 |
| E-ink refresh modes | UC8253 full validated; UC8253 half/fast and UC8279d differential validation in progress |
| Buttons, microSD, RTC, and battery | Not started |
| Tamagotchi P1 emulation | Not started |
| Save data and power management | Not started |

The complete requirements, safety contract, and delivery gates are documented
in [SCOPE.md](SCOPE.md).

## Design goals

- **Dedicated experience:** boot directly into TamaInk without retaining
  e-reader features or a general-purpose application shell.
- **Safe installation:** preserve the X3's existing bootloader, partition
  table, recovery path, and ability to return to CrossPoint.
- **Reliable state:** protect the user's Tamagotchi across ordinary resets,
  interrupted writes, sleep, and storage failures.
- **Minimal firmware:** include only the hardware and emulator components the
  device actually needs.
- **Honest e-ink behavior:** keep emulation timing authoritative while treating
  the slower display as a sampled view of the running Tamagotchi.

## Target hardware

The initial and only supported target is the ESP32-C3-based Xteink X3:

- 792x528 monochrome e-ink display;
- UC8253 or UC8279d display controller;
- physical ADC-ladder buttons;
- microSD storage;
- DS3231 real-time clock; and
- BQ27220 battery gauge.

Xteink X4 models, unrelated e-readers, and USB-locked X3 units are currently
out of scope.

## Architecture

TamaInk is a native ESP32 application. CrossPoint is not bundled with it and
does not run underneath it.

- [TamaLib](https://github.com/jcrona/tamalib) will provide the P1 CPU,
  Tamagotchi timing, LCD state, icons, and emulated button behavior.
- [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) provides the reusable
  X3 hardware layer and display drivers.
- TamaInk owns ROM validation, application lifecycle, rendering, persistence,
  and power policy.

Dependencies are pinned to reviewed revisions, and third-party provenance is
tracked in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## ROM policy

TamaInk does not include, distribute, download, or generate Tamagotchi ROMs.

When ROM support is implemented, users will supply their own compatible P1
`rom.bin` on the microSD card. TamaInk will treat that file as read-only and
will keep it separate from emulator save data.

Do not open issues, pull requests, or discussions containing ROM files or ROM
contents.

## Firmware builds

Firmware is built exclusively by the repository's GitHub Actions workflow, not
on developer workstations. Each workflow run:

1. builds the firmware twice from clean output;
2. requires both application images to be byte-for-byte identical;
3. validates that `firmware.bin` is an ESP application image;
4. verifies that it fits the CrossPoint-compatible OTA slot; and
5. uploads only the application image as a short-lived artifact.

Normal builds never publish a bootloader, partition table, merged image, or
full-flash dump.

## Flash safety

TamaInk preserves CrossPoint's established 16 MiB A/B partition contract. A
normal installation must never erase the complete flash chip, replace the
bootloader or partition table, modify eFuses, or disable USB recovery.

Current development builds are intended only for unlocked X3 units that are
recognized directly by the CrossPoint web flasher. Every hardware milestone
must retain the path back to a working CrossPoint installation.

Detailed rules and recovery expectations are maintained in
[SCOPE.md](SCOPE.md#flash-compatibility-and-safety).

## Roadmap

Work proceeds in deliberately small stages:

1. deterministic builds and reversible flashing;
2. board and display-controller detection;
3. display modes and diagnostic patterns;
4. physical button input;
5. read-only microSD access;
6. recoverable persistent storage;
7. RTC and battery monitoring;
8. deterministic host-side P1 emulation;
9. emulator bring-up on the X3;
10. e-ink rendering integration; and
11. final sleep, wake, autosave, and low-battery behavior.

Stable releases will begin only after installation recovery, display variants,
input, emulation, persistence, timekeeping, and long-duration behavior have all
been validated on physical hardware.

## Contributing

TamaInk is currently organized around narrow, reviewable milestones. Before
starting a change:

- read [SCOPE.md](SCOPE.md);
- keep work within the current delivery gate;
- avoid coupling unproven hardware subsystems together;
- never add ROMs, save files, device dumps, or generated firmware binaries; and
- distinguish host-tested, build-tested, and hardware-tested evidence in your
  pull request.

Hardware reports are most useful when they include the X3 variant, detected
display controller, firmware commit, exact serial output, and observed recovery
result.

## License and acknowledgements

TamaInk is licensed under the [GNU Affero General Public License v3.0](LICENSE).

The project builds on the work of TamaLib, FreeInk SDK, CrossPoint Reader, and
their respective contributors. Licensing, revisions, attribution, and
transitive notices are recorded in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
