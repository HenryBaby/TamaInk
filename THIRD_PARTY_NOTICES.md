# Third-Party Notices and Provenance

This document tracks external material used by or materially informing TamaInk.
It is an inventory aid and does not replace the complete license texts shipped
with source and binary distributions.

## Runtime dependencies

### TamaLib

- Project: <https://github.com/jcrona/tamalib>
- Copyright: Jean-Christophe Rona and contributors
- License: GNU General Public License, version 2 or later
- Use: pinned source dependency providing first-generation Tamagotchi
  emulation
- Distribution obligations: preserve notices and modification history; provide
  applicable license text and complete corresponding source with distributed
  firmware

The repository includes the `tamalib` Git submodule gitlink at exact revision
`ce304d55f9a73c60232ce3f552e7983db3fa399c` (upstream `master`, inspected
2026-08-01). The host smoke harness compiles and links the upstream `cpu.c`,
`hw.c`, and `tamalib.c` translation units and their public/private headers
(`cpu.h`, `hw.h`, `tamalib.h`, `hal.h`, and `hal_types.h` template). The pinned tree contains
the complete GPL text in `tamalib/LICENSE`; its source headers identify the
license as GPL-2.0-or-later. No upstream TamaLib file is modified. The host
smoke test uses a generated temporary copy solely to remove the upstream
default `E0C6S48_SUPPORT` define and explicitly select E0C6S46. The
project-owned host harness under `test/host` generates this temporary
configuration and does not alter the submodule. For the live-state host gate,
CI applies the project-owned patch `patches/tamalib-live-state.patch` to that
exact revision using `scripts/apply_tamalib_patch.sh`; the patch preserves the
upstream GPL headers and adds only POD export/import hooks for hidden CPU and
TamaLib continuation state. The repository gitlink remains pinned; CI applies
the patch to its fresh pinned checkout before host and firmware compilation and
intentionally leaves that build worktree dirty. The adapted patch is distributed
as corresponding source under the project's AGPL-3.0 terms together with the
upstream GPL notices.

### FreeInk SDK

- Project: <https://github.com/Free-Ink/freeink-sdk>
- Copyright: FreeInk and contributors
- License: MIT
- Use: pinned source dependency providing Xteink X3 hardware support
- Distribution obligations: preserve the MIT copyright and permission notice,
  FreeInk's `NOTICE`, and all applicable transitive notices

FreeInk states that portions, including e-paper initialization sequences and
waveform LUTs, derive from the MIT-licensed OpenX4 E-Paper Community SDK and
credits original e-paper driver authorship to CidVonHighwind. Its complete
`LICENSE` and `NOTICE` files must remain with source and release notices. The
firmware pins revision `92303ba5e4d4f762bb2f9126a3e31c303d66eb28`
(2026-07-28) as the `freeink-sdk` Git submodule and currently links only its
`BoardConfig`, `XteinkDetect`, `EInkDisplay`, `InputManager`, `SDCardManager`,
`BatteryMonitor`, `PowerManager`, and `FreeInkUI` libraries. The X3 RTC diagnostic uses the Arduino Wire API
and the DS3231 register behavior documented by the pinned FreeInk implementation,
but does not link FreeInk's `Rtc` library because its `begin()` routine mutates
DS3231 control state.
TamaInk does not modify those files.

#### Noto Sans bitmap font

- Project: <https://github.com/notofonts/noto-fonts>
- Copyright: 2018 The Noto Project Authors
- License: SIL Open Font License 1.1
- Source in the pinned dependency: `freeink-sdk/libs/ui/FreeInkUI/include/FreeInkUIFont.h`
- Use: FreeInkUI's 1-bit rasterization of Noto Sans Regular at 24 px is rendered
  directly for the TamaInk sleep-screen wordmark and status text

The generated header identifies the source face, raster size, bitmap format,
and OFL terms. TamaInk does not modify or rename the font data. The required
copyright notice and complete license are distributed in
`LICENSES/OFL-1.1.txt`.

### SdFat

- Project: <https://github.com/greiman/SdFat>
- Copyright: 2011-2020 Bill Greiman
- Version: 2.3.1 (`cda057318bec196183d4cc92b01bc1dd64bbfb02`)
- License: MIT
- Use: pinned transitive runtime dependency of FreeInk SDK's `SDCardManager`,
  providing FAT filesystem and SD-card access for the read-only user ROM and
  TamaInk-owned recoverable save generations
- Distribution obligations: preserve the upstream copyright and MIT permission
  notice with source and binary distributions

## Development references

### TamaTool

- Project: <https://github.com/jcrona/tamatool>
- Copyright: Jean-Christophe Rona and contributors
- License: GNU General Public License, version 2 or later
- Current use: reference for packed ROM decoding, P1 identification, and
  portable state serialization

Any copied or adapted implementation must be identified at file level and must
meet the applicable GPL source and notice requirements. The current ROM-format
documentation was independently written from inspected behavior; it does not
include ROM contents.

### CrossPoint Reader

- Project: <https://github.com/crosspoint-reader/crosspoint-reader>
- Copyright: Dave Allie and contributors
- License: MIT
- Current use: reference for Xteink initialization, partition layout, flashing,
  recovery practices, and constrained-device engineering guidance

TamaInk does not use CrossPoint as its application base. The project scope and
development skill were independently written after reviewing CrossPoint's
documentation and development guide. If code or substantial text is later
copied or adapted, record the exact revision and paths here and preserve the MIT
notice.

## Build tooling

These tools run in GitHub Actions and are not linked into TamaInk firmware.

### PlatformIO Core

- Project: <https://github.com/platformio/platformio-core>
- Version: 6.1.19
- License: Apache License 2.0
- Use: CI build orchestration

### GitHub Actions

- `actions/checkout` revision `11d5960a326750d5838078e36cf38b85af677262`
- `actions/setup-python` revision `a26af69be951a213d495a4c3e4e4022e16d87065`
- `actions/upload-artifact` revision `ea165f8d65b6e75b540449e92b4886f43607fa02`
- License: MIT at the pinned upstream revisions
- Use: CI source checkout, Python setup, and application-image artifact upload

The release-candidate audit must record the final resolved transitive
build-tool and ESP32 platform inventory associated with the reproducible
firmware artifact.

## User-supplied Tamagotchi ROM

TamaInk neither contains nor distributes a Tamagotchi ROM. A user's `rom.bin`
remains external, read-only user data and must never be committed, logged,
embedded in tests, or included in a release.
