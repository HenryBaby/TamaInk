---
name: tamaink-development
description: Develop, review, test, or release TamaInk firmware for the ESP32-C3 Xteink X3. Use for repository setup, FreeInk or TamaLib integration, ROM handling, display/input/storage/RTC/power work, flash tooling, persistence, hardware validation, dependency updates, and incremental Git commits where stability and reversible CrossPoint flashing matter.
---

# TamaInk Development

Build TamaInk as a dedicated Tamagotchi P1 appliance on the Xteink X3. Optimize
for correctness, recoverability, and reviewable increments before features.

## Establish context

1. Read `SCOPE.md` completely before planning or changing product behavior.
2. Inspect the current branch, remotes, upstream, status, staged diff, and
   unstaged diff before Git operations. Never assume branch or remote names.
3. Detect the actual host shell and use native commands. Do not assume Bash;
   this repository may be developed from PowerShell on Windows.
4. Inspect the pinned FreeInk and TamaLib sources before relying on an API or
   hardware behavior. Do not substitute CrossPoint application abstractions for
   SDK APIs without a demonstrated need.
5. Identify the current delivery gate from `SCOPE.md`. Do not couple later
   subsystems into an unproven foundation.
6. Read [references/licensing-and-provenance.md](references/licensing-and-provenance.md)
   completely before adding or adapting external code, wording, data, assets,
   dependencies, or release contents.

## Preserve architecture boundaries

- Treat TamaInk as the complete native application. CrossPoint never runs
  underneath it.
- Use TamaLib for P1 CPU, timing, buttons, LCD/icons, and emulated sound state.
- Use only required FreeInk libraries for X3 hardware.
- Use CrossPoint as evidence for initialization, flashing, and workarounds, not
  as the application base.
- Pin toolchains, FreeInk, and TamaLib to reviewed revisions. Isolate dependency
  updates from feature changes.
- Track every external source and its transitive notices in
  `THIRD_PARTY_NOTICES.md` as part of the same change that introduces it.
- Exclude reader, EPUB, theme, font, localization, network, and general UI code
  unless a scoped requirement proves it necessary.

## Work incrementally

1. Select one coherent, bounded change aligned with the current delivery gate.
2. Define its observable success and failure behavior before implementation.
3. Add host tests first when hardware is not required.
4. Keep flash, dependency, emulator, rendering, persistence, and power changes
   in separate review units.
5. Run the narrowest meaningful validation during iteration and the complete
   applicable gate validation before declaring the step complete.
6. Clearly label evidence as inspected, host-tested, build-tested, or
   hardware-tested. Never imply physical verification from a successful build.
7. Stop at the gate when physical X3 validation is required; provide exact test
   steps and expected observations.

## Protect constrained hardware

- Assume an ESP32-C3 single-core RISC-V MCU with about 380 KB usable RAM and no
  PSRAM.
- Use one 792x528 1-bit framebuffer unless measured evidence justifies more.
- Keep emulator execution independent of slow e-ink refresh work.
- Avoid allocation churn in loops. Prefer bounded/static storage or allocate
  reusable buffers once.
- Justify significant heap allocations and check every fallible allocation.
  With exceptions disabled, never use bare fallible `new`; use nothrow patterns.
- Keep large buffers off small task stacks. Measure stack high-water marks and
  minimum free heap for new tasks or substantial buffers.
- Never dereference unaligned wider pointers into byte buffers on RISC-V; decode
  explicitly or use `memcpy`.
- Put flash-cache-sensitive ISRs in IRAM and ISR-accessed data in DRAM. Use only
  ISR-safe synchronization primitives from interrupt context.
- Avoid long blocking loops and service the watchdog deliberately.
- Serialize SD access if more than one task can reach the storage driver.
- Do not claim speed, memory, power, or wear improvements without explaining the
  mechanism and supplying measurements.

## Handle ROMs and state safely

Read [references/rom-and-state.md](references/rom-and-state.md) completely for
ROM parsing, identification, sensitive-file, and persistence work.

- Never commit, embed, redistribute, download, transform in place, or expose the
  contents of a user ROM.
- Treat the user's `rom.bin` as read-only.
- Keep ROM and saves on microSD rather than repurposing CrossPoint SPIFFS.
- Use versioned, checksummed, multi-generation state with atomic replacement.
- Preserve the last valid state across reset, power loss, missing SD, and
  interrupted writes.

## Preserve flash reversibility

Read [references/flash-safety.md](references/flash-safety.md) completely before
changing PlatformIO configuration, partitions, OTA, release packaging, upload
commands, or recovery behavior.

- Produce an application-only `firmware.bin` valid in either OTA slot.
- Preserve the CrossPoint-compatible 16 MiB partition contract.
- Never erase the whole chip or modify bootloader, partition table, eFuses,
  secure-boot state, flash encryption, or ROM download capability in normal
  development or installation.
- Support only directly USB-flashable X3 units until locked-device recovery is
  independently proven.
- Require CrossPoint -> TamaInk -> CrossPoint round-trip evidence before broad
  hardware testing or release.

## Apply repository discipline

- Preserve unrelated user changes and adapt around a dirty worktree.
- Stage explicit in-scope paths; never use broad staging when unrelated changes
  exist.
- Do not commit or push unless the user explicitly requests it.
- When authorized to commit, create small Conventional Commits with one concern
  each. Do not mix formatting, dependency pins, behavior, and documentation.
- Do not amend, rebase, reset, clean, force-push, bypass hooks, or change Git
  configuration without explicit authorization.
- Never claim a build, test, hardware check, commit, or push succeeded without
  directly observed evidence.
- Never commit local configuration, build products, device dumps, crash logs,
  ROMs, saves, credentials, or full-flash images.

Forbidden or local-only artifacts include:

- `rom.bin` and other ROM images;
- `.pio/` and generated firmware binaries;
- `platformio.local.ini`;
- save files and SD-card images;
- `flash.bin`, merged images, and full-device backups; and
- unsanitized serial logs or coredumps.

## Validate proportionally

Use these evidence levels:

- **Inspection:** source paths, configuration, dependency revisions, and image
  layout agree with the intended design.
- **Host tests:** ROM decoding, P1 identification, deterministic stepping,
  serialization, CRC, and recovery logic.
- **Build tests:** clean release build at dependency/toolchain boundaries;
  incremental builds during ordinary iteration; application image type and
  size below `0x640000`.
- **Static checks:** formatting, compiler diagnostics, forbidden artifacts, and
  unsafe flash commands.
- **Device tests:** panel variants, buttons, SD, RTC, battery, sleep/wake, heap,
  stack, watchdog, and long-duration behavior.
- **Resilience tests:** reset during state-write phases and interruption while
  flashing the inactive OTA slot.
- **Human acceptance:** ghosting, refresh latency, responsiveness, and the
  single-purpose Tamagotchi experience.

Classify failures as implementation, dependency, environment, hardware,
permission, flaky, or pre-existing. Report blocked validation accurately.

## Finish a change

Report:

1. the outcome and changed files;
2. the delivery gate advanced;
3. exact validations run and their observed results;
4. any remaining hardware-only checks;
5. flash or persistence risk, if applicable; and
6. provenance or license changes, if applicable; and
7. the next smallest safe increment.

