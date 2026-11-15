---
id: "ADR-247"
title: "The settings help and the built-in replay fixture may be stored deflated in firmware flash (amends ADR-194)"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "flash budget"
  - "progmem assets"
  - "gzip"
  - "settings help"
  - "replay fixture"
aliases:
  - "settings-help.json gzip"
  - "OTGW_SIM_FIXTURE deflate"
  - "ROM tinfl"
components:
  - "settingsHelp.h"
  - "otgwSimulationFixture.h"
symbols:
  - "SETTINGS_HELP_JSON"
  - "OTGW_SIM_FIXTURE"
  - "serveProgmemAsset"
  - "tinfl_decompress"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-247 The settings help and the built-in replay fixture may be stored deflated in firmware flash (amends ADR-194)

## Status

Accepted, 2026-11-15.

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: "Claude (session 95) for the coordinator"
    reason: Initial proposal (TASK-1955 options 1 and 2)
    changed_via: adr-kit
  - date: 2026-11-15
    status: Accepted
    changed_by: Robert van den Breemen (Discord DM 1571521326060146782 on question 1571520520153735261, option A), recorded by Agent (95)
    reason: "Robert chose A: accept the ADR-194 amendment as written (TASK-1955 options 1 and 2)"
    changed_via: adr-kit lifecycle
```

## Context

The esp32-combo app image is 2 157 711 B of a 2 293 760 B slot (94.1 %, beta.320, TASK-1955). The compiler already
runs -Os with LTO (link-time optimisation) and section garbage collection, and most of the rest is precompiled (WiFi/BT blobs, lwip, mbedtls
for WPA (Wi-Fi Protected Access)). The two largest text assets of our own are the two ADR-194 moved into flash (TASK-1294):

- `SETTINGS_HELP_JSON` (`src/OTGW-firmware/settingsHelp.h`), 17 767 B, served at `/settings-help.json` by
  `serveProgmemAsset()` (FSexplorer.ino) and read only by the browser (`data/v2.js`); gzip-9: 6 660 B.
- `OTGW_SIM_FIXTURE` (`src/OTGW-firmware/otgwSimulationFixture.h`), 11 561 B, the built-in OpenTherm replay read
  byte by byte through `pgm_read_byte` (OTGW-Core.ino) when no `/otgw_simulation.log` is on LittleFS; gzip-9: 235 B.

ADR-194's contract forbids this ("Must Not: Gzip or otherwise compress-encode an asset, in firmware or on
LittleFS"). ADR-215 already serves the web pages gzipped, ADR-221 the language packs, and the update page
(`UpdateServerIndex`, updateServerHtml.h) is a gzipped PROGMEM array (TASK-1640). The ESP32-S3 ROM (mask ROM, read-only code in the chip) has the miniz
inflater (`tinfl_decompress` at 0x40000828, esp32s3.rom.ld:83; header `esp_rom/include/miniz.h`), so inflating on the
device adds no inflate code to the image.

## Decision

The two PROGMEM assets of ADR-194 may be stored deflated in firmware flash (coordinator decision for TASK-1955 options
1 and 2; this ADR amends ADR-194 for these two assets only):

1. `/settings-help.json` is a gzip byte array in flash and is sent with `Content-Encoding: gzip`; the browser inflates
   it. ETag, `Cache-Control: no-cache` and the 304 path of `serveProgmemAsset()` stay as ADR-194 defines them.
2. The built-in replay fixture is a deflate byte array in flash. When a built-in replay starts, the firmware inflates
   it once with the ROM `tinfl` into a heap buffer, replays from that buffer and frees it when the replay ends; if the
   allocation fails, the replay does not start and says so in the log.
3. The readable source stays in the repository (a JSON file and the fixture log); a generator writes the byte arrays,
   as `scripts/webui_pages.py` does for the update page, and a test checks that the inflated bytes equal the source.

Estimated saving about 22 KB of flash (11 107 B + 11 326 B, measured gzip-9 on the real bytes).

## Decision Contract

### Must

- Keep the readable JSON and fixture log as the single source of truth in the repository and generate the byte arrays
  from them; the generated headers say so and are not edited by hand.
- Serve `/settings-help.json` with `Content-Encoding: gzip` through `serveProgmemAsset()` (ETag = git hash plus path,
  `Cache-Control: no-cache`, 304 on a match), streamed from flash without a heap copy.
- Inflate the fixture with the ROM `tinfl`, into one heap buffer that lives only while a built-in replay runs, and
  refuse the replay with a log line when the buffer cannot be allocated.
- Host tests: the inflated help JSON equals the source and parses; the inflated fixture is byte-identical to
  `tests/fixtures/otgw_simulation.builtin.log`.

### Must Not

- Compress any other asset under this ADR; ADR-194's rule stands for everything else.
- Inflate the help JSON on the device or copy any PROGMEM asset to the heap to serve it.
- Keep the fixture inflated outside a running built-in replay.

### Exceptions

- An uploaded `/otgw_simulation.log` on LittleFS still takes precedence over the built-in fixture (ADR-194).

### Verification

- New host tests for both round trips (TASK-1955); `tests/webui/settings-help.test.mjs` and
  `tests/test_sim_fixture_builtin.py` read the readable sources.
- Bench: `curl --compressed http://<unit>/settings-help.json` returns the old JSON, `Content-Encoding: gzip`; a
  built-in replay runs as before and frees its buffer (heap back to the level before the replay).
- Build log: the app image shrinks by about 22 KB.

## Alternatives Considered

- **Keep ADR-194 as is.** Rejected: 22 KB of the 133 KB headroom for two text files, while pages and language packs
  are gzipped already.
- **Pack the fixture as binary (letter + 4 bytes per frame, about 5.8 KB).** Rejected: saves half as much; deflate
  needs no new inflate code (ROM) and only a transient buffer.
- **Inflate the fixture in a streaming way.** Rejected: tinfl's streaming mode needs a 32 KB dictionary, more than the
  one-shot buffer.
- **Move both back to LittleFS.** Rejected: ADR-194 moved them out to keep them in step with the firmware.

## Consequences

**Positive:**

- About 22 KB of flash back (1.0 % of the slot) at low risk; the help JSON also travels in a third of the bytes.

**Negative:**

- A built-in replay needs about 23 KB of heap while it runs (11.5 KB output buffer plus the tinfl decompressor
  struct); a unit too short of heap refuses the built-in replay with a log line instead of running it.
- Two generated headers and a generator step to keep in step; the source-equality tests catch a drift.
- A client that does not accept gzip gets bytes it cannot read; the only client is the web UI (`v2.js`).

## Open Questions

- [x] Robert: accept this amendment of ADR-194 (TASK-1955 options 1 and 2)?
  Answered A (accept as written) by Discord DM 1571521326060146782 (reply to question DM 1571520520153735261) on 2026-11-15.

## Related Decisions

- **ADR-194** (static text assets in firmware flash): amended for these two assets only; its rule stands for every other asset.
- **ADR-215** (pages served gzipped with an ETag) and **ADR-221** (gzipped language packs): the same browser-side
  inflate this ADR uses for the help JSON.
- **ADR-190** (minified, not gzipped LittleFS staging): unchanged.

## References

- TASK-1955 (size analysis, beta.320+5b9efcc: image 2 157 711 B of 2 293 760 B; gzip-9 of the two assets measured on
  the bytes in `.flash.rodata`).
- `src/OTGW-firmware/settingsHelp.h`, `src/OTGW-firmware/otgwSimulationFixture.h`, `src/OTGW-firmware/updateServerHtml.h`
  (an existing gzipped PROGMEM page), `scripts/webui_pages.py` (its generator).
- ESP32-S3 ROM inflater: `framework-arduinoespressif32-libs/esp32s3/ld/esp32s3.rom.ld:83` (`tinfl_decompress`),
  `esp_rom/include/miniz.h`.
