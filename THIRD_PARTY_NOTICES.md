# Third-Party Notices and Provenance

This document tracks external material used by or materially informing TamaInk.
It is an inventory aid and does not replace the complete license texts shipped
with source and binary distributions.

## Planned runtime dependencies

### TamaLib

- Project: <https://github.com/jcrona/tamalib>
- Copyright: Jean-Christophe Rona and contributors
- License: GNU General Public License, version 2 or later
- Intended use: pinned source dependency providing first-generation Tamagotchi
  emulation
- Distribution obligations: preserve notices and modification history; provide
  applicable license text and complete corresponding source with distributed
  firmware

The exact revision and imported paths will be recorded when the dependency is
added.

### FreeInk SDK

- Project: <https://github.com/Free-Ink/freeink-sdk>
- Copyright: FreeInk and contributors
- License: MIT
- Intended use: pinned source dependency providing Xteink X3 hardware support
- Distribution obligations: preserve the MIT copyright and permission notice,
  FreeInk's `NOTICE`, and all applicable transitive notices

FreeInk states that portions, including e-paper initialization sequences and
waveform LUTs, derive from the MIT-licensed OpenX4 E-Paper Community SDK and
credits original e-paper driver authorship to CidVonHighwind. Its complete
`LICENSE` and `NOTICE` files must remain with source and release notices. The
firmware pins revision `92303ba5e4d4f762bb2f9126a3e31c303d66eb28`
(2026-07-28) as the `freeink-sdk` Git submodule and currently links only its
`BoardConfig`, `XteinkDetect`, `EInkDisplay`, `InputManager`, and
`SDCardManager` libraries.
TamaInk does not modify those files.

### SdFat

- Project: <https://github.com/greiman/SdFat>
- Copyright: 2011-2020 Bill Greiman
- Version: 2.3.1 (`cda057318bec196183d4cc92b01bc1dd64bbfb02`)
- License: MIT
- Use: pinned transitive runtime dependency of FreeInk SDK's `SDCardManager`,
  providing FAT filesystem and SD-card access; TamaInk's current storage
  diagnostic uses it only for read operations
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

The complete transitive build-tool and ESP32 platform inventory will be
captured from the first successful CI build before a firmware release.

## User-supplied Tamagotchi ROM

TamaInk neither contains nor distributes a Tamagotchi ROM. A user's `rom.bin`
remains external, read-only user data and must never be committed, logged,
embedded in tests, or included in a release.
