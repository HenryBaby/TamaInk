# Flash Safety Contract

Read this reference for any work involving builds, uploads, OTA, partitions,
release artifacts, recovery, or USB-locked devices.

## Compatible layout

Preserve this CrossPoint-compatible 16 MiB partition map:

| Region | Offset | Size |
| --- | ---: | ---: |
| NVS | `0x9000` | `0x5000` |
| OTA metadata | `0xE000` | `0x2000` |
| Application 0 | `0x10000` | `0x640000` |
| Application 1 | `0x650000` | `0x640000` |
| SPIFFS | `0xC90000` | `0x360000` |
| Coredump | `0xFF0000` | `0x10000` |

Use the matching partition definition at build time. Do not include its binary
in a normal release.

## Release artifact

Release only an ESP32 application image named `firmware.bin` that:

- is position-compatible with either OTA application slot;
- is smaller than `0x640000`;
- has valid ESP32 image and application descriptor magic;
- contains no bootloader, partition table, OTA metadata, NVS, SPIFFS, coredump,
  or device-specific data; and
- is not a merged, factory, or full-flash image.

Do not publish a full-flash recovery image through the normal release path.

## Installation behavior

Use an OTA-aware flasher that:

1. validates the existing partition layout;
2. reads OTA metadata;
3. chooses the inactive application slot;
4. writes and verifies the complete image;
5. updates boot selection only after verification; and
6. leaves the previously active slot bootable until the switch.

Never blindly write `0x10000`: the active slot may be `app0` or `app1`.

## Prohibited operations

Do not normally:

- write below `0x10000`;
- erase the entire 16 MiB flash;
- replace the bootloader or partition table;
- write raw OTA metadata from TamaInk;
- modify eFuses;
- enable secure boot or flash encryption;
- disable USB/JTAG or the ROM serial downloader;
- change flash mode away from the known X3-compatible DIO setting; or
- repartition internal storage for TamaInk data.

## Round-trip qualification

Before broad device testing:

1. Back up and hash the relevant bootloader and partition-table regions.
2. Start from a working CrossPoint installation.
3. Install minimal TamaInk through the intended flasher.
4. Verify only the inactive app slot and controlled OTA metadata changed.
5. Boot and exercise minimal TamaInk.
6. Reinstall CrossPoint through its normal web-flashing workflow.
7. Verify CrossPoint and all X3 hardware operate normally.
8. Verify bootloader and partition-table hashes are unchanged.
9. Repeat in both OTA-slot directions.
10. Interrupt inactive-slot writes at controlled stages and confirm the old slot
    remains bootable.

Record device revision, panel controller, flasher version, firmware hashes, and
observed results.

## USB-locked devices

Treat USB-locked X3 units as unsupported. Do not recommend TamaInk installation
on them until serial recovery, OTA escape, failure recovery, and return to
CrossPoint have all been demonstrated on that exact class of device.

