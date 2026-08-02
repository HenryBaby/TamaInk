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
| Physical buttons | Validated on a UC8253 X3 |
| microSD, RTC, and battery | microSD read-only validated on a UC8253 X3; RTC and battery gate in progress (not hardware validated) |
| Tamagotchi P1 emulation | Device serial path plus UC8253 X3 sampled LCD rendering integrated; hardware validation pending |
| ROM validation (read-only packed P1 loading) | Device `/rom.bin` streaming validation integrated; hardware validation pending |
| Persistent storage diagnostic | In progress (two-generation recovery gate) |

Deep-sleep resume now applies bounded emulator catch-up after a valid RTC
resume, before normal startup. Execution is limited to 300 planned seconds,
2,000,000 instruction attempts, or a 10-second boot watchdog; hardware
validation remains pending. A temporary fast-forward clock is used only during
this catch-up, then normal wall-clock pacing is restored; hardware retest is
pending.

The first rendering increment is build-integrated but hardware-pending. On the
X3, it maps the 32x16 LCD counterclockwise into a centered portrait footprint:
16x scale, 256x512 pixels at origin (268,8), producing an upright 512x256 view
when the device buttons are at the bottom. In y-down coordinates, logical
(column,row) maps to physical (row,31-column); the renderer also retains an
explicit unrotated mode for host tests. The current P1 icon layout draws
project-owned monochrome glyphs (rotated CCW with the LCD) within 48x48 extents
in a single physical row along the bottom edge of the portrait display
(framebuffer x=744 on the 792px X3 panel; tops at
y=464,400,336,272,208,144,80,16 for bits 0 through 7). Inactive icons show
only their glyph; active icons add four open corner brackets around the 48px
extent, with 3px thickness and 12px arms leaving edge midpoints open. Hardware
confirms the semantic order and rotated top/bottom mapping:
Food, Light, Game, Medicine, Toilet, Health, Discipline, Attention. The new
bottom-edge placement still requires its hardware validation gate.
UC8279d and other controllers
keep the serial emulator active with rendering disabled. The renderer performs
one initial full refresh, then UC8253 fast refreshes no more often than once per
400 ms; no periodic cleaning refresh is enabled in this increment.

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
Optional local real-ROM integration testing is deferred; host CI uses synthetic
buffers only and never requires or exposes a ROM file.

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

The host-tested portable emulator state codec is a bounded, versioned snapshot
format with a live TamaLib bridge. Host builds apply the reviewed patch in
`patches/tamalib-live-state.patch` to the pinned TamaLib revision before
compiling the live bridge. The repository gitlink remains pinned; CI
intentionally leaves the build worktree patched and dirty. The
patch exposes complete CPU/TamaLib continuation state for deterministic
save/resume tests using synthetic ROM buffers only. This is host evidence,
not hardware validation.

The live X3 gate scans `/.tamaink/state-a.bin` and `state-b.bin` after ROM
validation, validates both the outer record and nested 694-byte emulator codec
against a domain-separated `TINK` + CRC32 identity, and resumes the newest
valid generation. Autosave runs every 15 minutes with a 60-second retry
backoff; `a` requests the same staged transaction immediately. LCD frame dumps
are disabled by default; `l` toggles them
and prints at most one current frame when enabled. Serial writes are manual:
`p` begins export to the inactive
slot; repeated `n` advances
partial/remainder/sync/verify/CRC-commit phases; `c` corrupts the newest slot
for fallback testing; and `x` removes only the owned state files. Reset may be
requested between phases. Hardware validation remains pending.

While the emulator is active, battery telemetry is read-only and sampled at
most once per minute. The `b` command requests an immediate sample and resets
the timer. Warning state uses 15%/20% hysteresis (low at 15% or below, cleared
at 20% or above); unknown samples retain the prior warning state. Hardware
validation of this telemetry remains pending.

For the development low-battery gate, uppercase `B` injects one synthetic
unplugged 15% sample (it never writes gauge hardware or telemetry). Two
consecutive qualifying samples request the normal staged autosave; deep sleep
is entered only after commit reread verification succeeds. Unknown samples,
charging, busy persistence, and 16-19% readings suppress or reset confirmation.
Lowercase `b` remains the real read-only battery check; USB charging normally
suppresses the gate. Hardware validation of this safety path remains pending.

Deep-sleep wake resumes now execute bounded emulator catch-up before normal
renderer startup when RTC elapsed time is valid. Execution is capped at 300
planned seconds, 2,000,000 instruction attempts, or a 10-second boot watchdog;
the final snapshot is retained on every stop outcome. This is host-tested and
hardware validation remains pending.

## Contributing

The `w` command is an explicit wake diagnostic: when the emulator, SD, and
persistence are idle it performs the same durable save, then stops rendering,
waits for release, and deep-sleeps armed only for the confirmed GPIO3 power
button. ADC button ladders cannot identify individual wake buttons; DS3231
alarm wake is unavailable/unknown, and no timer or automatic sleep is used.
After the durable save is verified and the power button is released, the
renderer shows a centered white TamaInk/SLEEPING terminal screen, performs a
blocking full refresh, then turns off the panel and enters ESP deep sleep.
Wake cause and GPIO status are logged at boot; the serial GPIO3 wake path has
been hardware validated. USB reset or power cycle is the recovery path.

The same flow can be requested without serial: hold the physical `BACK` and
`POWER` buttons together continuously for at least 2000 ms, then release both.
A short or broken chord is canceled, and one hold/release produces at most one
request. Normal individual BACK/CONFIRM/POWER press and release events remain
mapped to emulator A/B/C input.
Physical gesture hardware validation remains pending.

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
