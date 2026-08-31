# FreeInk display-driver compile harness

This is a repository-local PlatformIO compile-only harness for the display
backends exposed by the checked-out `freeink-sdk` (currently c9f7525). Run it
through the repository's Docker service; no upload, hardware access, or host
toolchain is required:

```sh
docker compose run --rm test drivers
```

The canonical container runner invokes PlatformIO with `-j 8`, the maximum
recommended build parallelism for this harness. The sketch only constructs a
`FreeInkDisplay`; it deliberately does not call
`begin()` or any hardware operation.

## Environment-to-driver matrix

| Environment | Device flags | Display backend compiled |
| --- | --- | --- |
| `xteink` | `X3` + `X4` | UC8253-X3 + SSD1677/UC8179/UC8279-X4 |
| `xteink_x4` | `X4` | SSD1677/UC8179/UC8279-X4 |
| `m5paper` | `M5` | native ED2208 |
| `m5paper_official` | `M5` + `M5_OFFICIAL` | M5Unified/M5GFX ED2208 |
| `delink` | `DELINK` | SSD1677 |
| `murphy` | `MURPHY` | UC8253-Murphy |
| `m5paper_v11` | `M5PAPER` | IT8951 |
| `papers3` | `PAPERS3` | LovyanGFX `Panel_EPD` |
| `eego_a4` | `EEGO_A4` | UC8279C |
| `sticky` | `STICKY` | SSD1677 |
| `x4pro` | `X4PRO` | SSD1677/UC8179/UC8279-X4 |
| `x4c` | `X4CLASSIC` | SSD1677/UC8179/UC8279-X4 |
| `papermono` | `PAPERMONO` | Paper Mono SSD1677 |

The upstream `lilygo_t5s3` sample environment remains excluded. Its
`LgfxEpdDriver` requires an application-injected `LgfxEpdConfig` symbol
containing board-specific parallel-bus pins and power hooks
(`FREEINK_LGFX_EPD_CONFIG`); the SDK intentionally provides no universal
default. A fake config would only verify a harness invention, not the upstream
sample contract. `papers3` covers the same LovyanGFX `Panel_EPD` backend with
the SDK-provided `BoardPaperS3` configuration.

The library dependencies use `symlink://../../freeink-sdk/...`, resolving
from this harness directory to the sibling SDK checkout. This is intentionally
a display-only harness: the base environment links only `BoardConfig` and
`EInkDisplay`; it does not pull unrelated input, battery, SD, UI, or other
firmware libraries. The M5 official and PaperS3 environments add only their
display backend's required M5GFX/M5Unified/M5PM1 or BoardPaperS3 dependencies.
