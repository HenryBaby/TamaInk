# TamaInk Project Scope

## Purpose

TamaInk is dedicated firmware that turns an Xteink X3 into a Tamagotchi P1
device. It boots directly into the emulator and does not provide e-reader
functionality.

The user supplies their own compatible Tamagotchi P1 `rom.bin` on the microSD
card. TamaInk will never include, distribute, download, or generate copyrighted
ROM data.

## Core principles

1. **Stability before features.** Hardware support, flashing, storage, and state
   recovery must be proven independently before emulator integration proceeds.
2. **Safe and reversible installation.** Installing TamaInk must not prevent the
   device from later returning to CrossPoint with its web flasher.
3. **Single-purpose operation.** The finished device behaves as a Tamagotchi,
   not as an e-reader with a Tamagotchi application.
4. **Minimal dependencies.** Use only the FreeInk components needed to operate
   X3 hardware. CrossPoint is a reference, not the application base.
5. **User data must survive failure.** Save-state handling must tolerate resets,
   interrupted writes, and corrupt files without silently losing the last valid
   state.

## Supported platform

The initial release target is an unlocked ESP32-C3-based Xteink X3 with:

- 792x528 monochrome e-ink display;
- physical ADC-ladder buttons;
- microSD storage;
- DS3231 real-time clock;
- BQ27220 battery gauge; and
- the physically validated UC8253 display controller.

UC8279d units are detected, but their display path remains disabled until it
has been physically validated. Broad X3 support must not be claimed on the
basis of controller detection alone.

Initial releases support only X3 units that can be detected and reflashed
directly over USB. USB-locked devices are unsupported until installation,
updates, and recovery have been independently proven safe on that hardware.

## Architecture

TamaInk is a native, standalone ESP32 application. CrossPoint does not run
underneath it.

- **TamaLib** provides E0C6S46 emulation, P1 execution, LCD state, icons, button
  input, timing, and emulated sound state.
- **FreeInk SDK** provides the reusable X3 hardware layer.
- **TamaInk code** owns the application lifecycle, ROM validation, rendering,
  persistence, power policy, and user experience.

FreeInk dependencies are pinned to reviewed commits. Updates must be
deliberate and must pass the applicable hardware tests before the pin changes.

The current firmware uses these FreeInk components:

- `BoardConfig`;
- `XteinkDetect`;
- `EInkDisplay`;
- `InputManager`;
- `SDCardManager`;
- `BatteryMonitor`;
- `PowerManager`; and
- `FreeInkUI`.

The DS3231 is accessed through the Arduino Wire API so the firmware can read it
without applying FreeInk's mutating RTC initialization. SdFat provides the
microSD filesystem used for the read-only ROM and TamaInk-owned save state.

Reader, book, network, web-server, theme, font, image-decoding, localization,
and general UI components will not be included unless a later requirement
demonstrates a concrete need.

## Flash compatibility and safety

TamaInk will retain CrossPoint's established 16 MiB flash and A/B OTA partition
contract:

| Region | Offset | Size |
| --- | ---: | ---: |
| NVS | `0x9000` | `0x5000` |
| OTA metadata | `0xE000` | `0x2000` |
| Application 0 | `0x10000` | `0x640000` |
| Application 1 | `0x650000` | `0x640000` |
| SPIFFS | `0xC90000` | `0x360000` |
| Coredump | `0xFF0000` | `0x10000` |

Normal releases will contain only an ESP32 application image named
`firmware.bin`. The image must be valid in either application slot and remain
below the slot-size limit.

Normal installation must never:

- overwrite or replace the bootloader;
- overwrite or replace the partition table;
- erase the complete flash chip;
- write directly below `0x10000`;
- burn or modify eFuses;
- enable secure boot or flash encryption;
- disable the ROM serial downloader or USB recovery path;
- blindly assume that `app0` is the inactive OTA slot; or
- distribute a merged, factory, or full-flash image as ordinary firmware.

The CrossPoint-compatible partition definition may be used at build time, but
its partition-table binary will not be included in a normal TamaInk release.
Installation should use an OTA-aware flasher that writes the inactive slot,
verifies the image, and changes the boot selection only after a successful
write.

Release qualification requires repeated CrossPoint -> TamaInk -> CrossPoint
round trips on physical hardware. This recovery path has been repeatedly
validated on the UC8253 development device. Any new controller variant, device
revision, or installation method must repeat the qualification; interrupted
inactive-slot writes must leave the previously active application bootable.

## Functional requirements

### ROM handling

- Load a user-provided `/rom.bin` from microSD.
- Validate its size, encoding, and supported P1 identity before execution.
- Fail safely with clear on-device instructions when it is missing or invalid.
- Never write to or modify the user's ROM.

### Emulation

- Run the P1 ROM through TamaLib with deterministic timing.
- Map three physical X3 buttons to Tamagotchi A, B, and C press/release events.
- Preserve all emulator state required for an exact resume.
- Keep emulation independent of the comparatively slow display refresh path.

### Display

- Render the 32x16 Tamagotchi LCD and eight status icons as a dedicated P1-style
  interface.
- Scale pixels without smoothing.
- Update only when logical LCD state changes.
- Coalesce rapid changes and use the safest appropriate partial or differential
  refresh mode.
- Perform periodic full cleaning refreshes to control ghosting.
- Support both known X3 display-controller variants before declaring broad X3
  support.

E-ink cannot reproduce the original LCD frame rate. Gameplay and emulation
timing remain authoritative; the display presents a sampled view of that state.

### Persistence

- Keep ROM and Tamagotchi save data on microSD rather than repurposing
  CrossPoint's internal SPIFFS region.
- Use a versioned save format with integrity checking.
- Maintain at least two save generations.
- Write a new state completely before replacing the last known-good state.
- Autosave at bounded, meaningful points and before controlled sleep or
  shutdown.
- Recover automatically from an incomplete or corrupt newest save.

### Time and power

- Preserve Tamagotchi time correctly across ordinary resets and sleep.
- Autosave periodically and complete a verified save before controlled sleep
  or low-battery shutdown.
- Use the explicit physical gesture for the normal on-device sleep path. The
  serial development diagnostic may request the same verified-save flow;
  automatic low-battery sleep may proceed only after a verified save. All
  paths wake through the confirmed power-button path.
- Use bounded RTC-based emulator execution after sleep rather than treating a
  wall-clock adjustment as equivalent to skipped CPU execution.
- Keep catch-up limits and failure outcomes explicit so boot cannot run
  indefinitely.
- Do not depend on RTC alarms or non-power-button wake without separate
  hardware validation.

### Sound

TamaLib sound state may be emulated, but the stock Xteink X3 has no speaker or
buzzer. Initial TamaInk releases are silent. Optional visual notification may be
considered, but authentic sound is out of scope without additional hardware.

## Incremental delivery gates

Each gate must be reproducible before work depends on it:

1. **Build:** Use GitHub Actions to produce a deterministic application-only
   `firmware.bin`; report version and reset reason over serial. Firmware is not
   built on developer workstations.
2. **Flash safety:** Prove reversible A/B installation and CrossPoint round-trip
   recovery without changing bootloader or partition-table regions.
3. **Board detection:** Identify the X3 and its display-controller variant.
4. **Display:** Validate deterministic test patterns and supported refresh modes
   on physical hardware.
5. **Input:** Validate raw and decoded press/release behavior, debounce, holds,
   and simultaneous inputs.
6. **Read-only storage:** Mount microSD and validate known files without writes.
7. **Persistent storage:** Prove atomic, recoverable writes under forced resets.
8. **RTC and battery:** Validate normal readings and all expected failure modes.
9. **Host emulation:** Test ROM parsing, deterministic TamaLib stepping, button
   input, LCD output, and save/resume without X3 hardware.
10. **Device emulation:** Run TamaLib on the X3 with serial LCD output and no
    e-ink coupling.
11. **Rendering integration:** Connect changed logical frames to the proven
    e-ink pipeline and tune refresh behavior.
12. **Power behavior:** Validate autosave, sleep, wake, low-battery, and
    timekeeping across reset and sleep.

## Release-candidate status

The UC8253 development device has completed hardware checks for board and
controller detection, display modes, physical input, microSD access, RTC and
battery telemetry, P1 emulation, recoverable persistence, autosave, sleep/wake,
bounded catch-up, low-battery protection, and repeated CrossPoint recovery.
The host suite and GitHub Actions workflow also validate deterministic
emulation, state codecs, renderer policy, and reproducible application-only
firmware builds.

Release-candidate preparation must complete clean-install, missing and corrupt
file, documentation, provenance, licensing, and packaged-artifact checks.
Until then, builds remain pre-release and support is limited to the physically
validated UC8253 X3 configuration.

## Non-goals

TamaInk will not initially provide:

- EPUB or other document reading;
- a book library or file browser;
- Wi-Fi, Bluetooth, cloud services, or ROM downloading;
- bundled ROMs;
- arbitrary Tamagotchi generation support beyond P1;
- authentic audio on unmodified X3 hardware;
- support for Xteink X4 or unrelated devices;
- support for USB-locked X3 units;
- a custom bootloader or custom flash layout; or
- installation by full-chip erase or full-flash replacement.

Future expansion must not weaken flash reversibility, save integrity, or the
single-purpose user experience.

## Licensing and provenance

TamaInk must respect the license and authorship of every external dependency,
copied or adapted implementation, document passage, image, font, waveform,
table, test vector, and other asset.

- Record the upstream project, source URL, revision, relevant files, license,
  copyright notice, and whether material was used unchanged or adapted.
- Read each dependency's `LICENSE`, `NOTICE`, and file-level headers before use.
- Preserve all required copyright, permission, attribution, modification, and
  source-offer notices in source and release distributions.
- Audit transitive provenance; a permissively licensed dependency may contain
  material with additional notice requirements.
- Prefer linking or vendoring a pinned upstream dependency over copying code.
- When copying is justified, keep upstream headers, identify modifications, and
  add the origin to `THIRD_PARTY_NOTICES.md` in the same change.
- Attribute adapted documentation wording and external assets as deliberately
  as source code. Ideas may be reimplemented independently, but their source
  should still be acknowledged when it materially informed the design.
- Never assume license compatibility. Resolve uncertainty before merging or
  distributing the affected code or firmware.
- Never copy code or assets from a repository without a clear license.

The repository is currently AGPL-3.0 licensed. TamaLib and TamaTool are
GPL-2.0-or-later; FreeInk and CrossPoint are MIT licensed. This statement records
the current upstream declarations, not a substitute for a release-time license
review.

## Release criteria

An initial stable release requires evidence that:

- installation and return to CrossPoint are repeatable and safe;
- both supported X3 display variants work, or releases clearly identify and
  enforce the validated variant;
- all A/B/C input events are reliable;
- a valid P1 ROM runs deterministically;
- save/resume survives resets and simulated interrupted writes;
- emulation keeps correct time and state during normal operation and across
  reset and sleep;
- display ghosting and refresh latency remain acceptable;
- low-battery and storage failures do not corrupt the last valid save; and
- the release contains no ROM or full-flash image;
- all shipped dependencies and adapted materials appear in the provenance
  inventory; and
- required licenses, notices, corresponding source, and modification notices
  accompany the release.

TamaInk is successful when an unlocked Xteink X3 can be safely flashed into a
stable, dedicated, persistent Tamagotchi P1 device and later returned to a fully
working CrossPoint installation through its normal web-flashing workflow.
