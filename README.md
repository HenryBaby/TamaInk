# TamaInk

Dedicated Tamagotchi P1 emulator firmware for the Xteink X3.

Development is intentionally incremental. See [SCOPE.md](SCOPE.md) for the
stability gates, flash-safety contract, and supported feature set.

## Build

Firmware is built only by the repository's GitHub Actions workflow. Pull
requests and pushes to `main` produce two clean builds and require their
application images to be byte-for-byte identical.

Successful workflow runs publish `firmware.bin` as a short-lived build artifact.
A post-build guard rejects images that are not valid ESP application images or
do not fit CrossPoint's OTA application slot. Bootloader, partition-table, and
full-flash artifacts are never uploaded.

Do not use full-chip erase or flash commands. Device installation is not
qualified until the flash-safety gate in `SCOPE.md` has been completed on an
unlocked X3.
