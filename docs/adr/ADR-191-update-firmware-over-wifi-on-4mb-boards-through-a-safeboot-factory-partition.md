---
id: "ADR-191"
title: "Update firmware over WiFi on 4MB boards through a safeboot factory partition"
status: "Proposed"
date: "2026-10-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-166"
  - "ADR-190"
  - "ADR-192"
topics:
  - "ota"
  - "partitions"
  - "safeboot"
  - "esp32s3"
  - "firmware-update"
aliases:
  - "safeboot ota"
  - "factory updater"
  - "4mb wifi ota"
  - "recovery partition"
components:
  - "4MB partition table with a safeboot factory partition"
  - "safeboot updater app"
  - "main-firmware handover to safeboot"
symbols:
  - "hasSpareAppOtaSlot"
  - "app_ota_available"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-191 Update firmware over WiFi on 4MB boards through a safeboot factory partition

## Status

Proposed, 2026-10-15.

**Decision Maker:** User: Robert van den Breemen (maintainer). On 2026-10-15 he
chose the v2-only web UI, the 960K factory size, updater authentication on par
with the main firmware, WiFi-only safeboot, and investigating crash-loop
rollback.

## Status History

```yaml
status_history:
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-166
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-190
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-192
    changed_via: adr-kit lifecycle
```

## Context

All 2.0.0 targets (esp32, esp32-classic, esp32-combo) run on ESP32-S3
(Espressif dual-core microcontroller) modules with 4 MB of flash. Their
partition table has one app slot, so an OTA (over-the-air) firmware write would
overwrite the running image. ADR-166 therefore made firmware updates USB-only:
`_handleUploadStart` refuses them when `hasSpareAppOtaSlot()` is false
(`src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:70`, `:299`). Every firmware
update needs a laptop and a cable at the unit; only the LittleFS (Little File
System) image can still be updated over WiFi.

Native A/B OTA needs two app slots of about 2 MB each, which does not fit in
4 MB beside the filesystem. A safeboot layout does: a small updater app lives in
the `factory` partition, and the main firmware lives in one large `ota_0` slot.
The ESP-IDF (Espressif IoT Development Framework) bootloader boots `factory`
when the OTA data partition (`otadata`) is blank or when `ota_0` fails image
validation. Tasmota and MycilaSafeBoot use this pattern.

Spike TASK-1222 measured it and proved it on the bench
(`docs/research/2026-10-14-spike-1222-safeboot-ota-4mb.md`):

- Our own minimal updater is 946,327 B. MycilaSafeBoot v3.3.9 is 980,059 B on
  our platform, and its 630,576 B release asset needs the Tasmota platform,
  which could not be reproduced.
- Combo firmware is 2,023,296 B.
- On a proof layout, handover by erasing `otadata` worked, and safeboot reused
  the stored WiFi credentials.
- A 2 MB upload took 11.0 s. A truncated upload was rejected
  (`esp_image: Checksum failed`), and a broken `ota_0` fell back to safeboot.
- An interrupted upload recovered over WiFi.

The filesystem must shrink to fit. With both web UIs it needs 1080 KiB, even
minified (ADR-190). The v2 UI alone needs 668 KiB.

## Decision

The 4MB targets switch to a safeboot partition layout. Firmware updates over
WiFi go through our own minimal updater in the `factory` partition, and the web
UI becomes v2-only so that the filesystem fits.

Layout (4 MB flash; nvs, otadata and phy_init stay where they are):

| Partition | Type | Offset | Size | Contents |
|---|---|---|---|---|
| safeboot | app/factory | 0x10000 | 0xF0000 (960K) | updater, 946,327 B measured |
| app0 | app/ota_0 | 0x100000 | 0x230000 (2240K) | main firmware; combo 2,023,296 B leaves 270,464 B (13.4%) |
| spiffs | data/spiffs (LittleFS) | 0x330000 | 0xC0000 (768K) | v2-only minified assets, 668 KiB minimum, about 100 KiB runtime room |
| coredump | data/coredump | 0x3F0000 | 0x10000 | unchanged |

Update flow:

```
main firmware  --(admin clicks "update firmware")-->  erase otadata, restart
bootloader     --(otadata blank)-->                     boot safeboot (factory)
safeboot       --(HTTP upload, verify, set boot ota_0)--> restart into new main
bootloader     --(ota_0 invalid at any boot)-->         boot safeboot again
```

Updater rules:

1. The updater joins WiFi with the credentials the main firmware stored in NVS
   (non-volatile storage). It falls back to an access point when there are
   none, and does not include W5500 Ethernet.
2. It serves one HTTP upload page; no HTTPS (ADR-003).
3. Authentication matches the main firmware (ADR-056). The main firmware mirrors
   the admin password into NVS. Safeboot requires HTTP Basic Auth when one is
   set and is open when none is set.
4. It writes nothing to UART0 beyond unavoidable ROM/bootloader output, because
   on Classic boards UART0 is the PIC link.

## Decision Contract

### Must

- Use the layout above for every 4MB target, with all offsets taken from the
  partition CSV and kept in sync in `build.py`, `flash_otgw.bat` and `flash_esp.py`.
- Fail the build when the updater image exceeds the factory partition, or the
  LittleFS content plus the runtime reserve exceeds the filesystem partition.
- Let the main firmware hand over only through an admin-protected endpoint that
  erases `otadata` (ADR-056).
- Verify the uploaded image (ESP-IDF image check) before setting `ota_0` as the
  boot partition.
- Keep filesystem OTA from the main firmware working as it does today.
- Use the bootloader's app rollback, which is already enabled in the shipped bootloader:
  - The main firmware defines `verifyRollbackLater()` returning true, and calls
    `esp_ota_mark_app_valid_cancel_rollback()` after a short health gate, before
    any planned restart.
  - Safeboot checks `esp_ota_get_last_invalid_partition()` before it points the
    boot partition back at `ota_0`, so a rolled-back image does not run again.
  - The handover erases the whole `otadata` partition (0xE000, 0x2000).
  - The one-time migration image writes `bootloader.bin` at 0x0.

### Must Not

- Add HTTPS, TLS or WSS to the updater (ADR-003).
- Write application output to UART0 from the updater.
- Mount or write LittleFS from the updater.
- Ship the classic web UI in the 4MB filesystem image.
- Enable anti-rollback (`CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK`). It assumes
  there is no factory partition (`bootloader_utility.c:431`).

### Exceptions

- Boards with 8 MB or more of flash keep native A/B OTA through
  `hasSpareAppOtaSlot()`; this layout does not apply to them.

### Verification

- TASK-1223.9 bench run on the final layout: a normal update and an interrupted
  update on an OTDirect board and a PIC board, plus a corrupt-image update on at
  least one of them (capture transcripts).
- Build log: updater size against 960K and LittleFS minimum against 768K
  (TASK-1223.3, TASK-1223.6).

## Alternatives Considered

- **Keep USB-only firmware updates (ADR-166).** No work and no migration, but
  every update needs a site visit with a laptop. Rejected by the maintainer as
  the long-term state.
- **Gzip the browser assets and keep both web UIs.** Smallest filesystem
  (604 KiB). Rejected: the maintainer forbids build-time gzip
  (`src/OTGW-firmware/FSexplorer.ino:83`, reaffirmed 2026-10-14).
- **MycilaSafeBoot as the updater.** MIT-licensed and proven. Rejected because
  it is larger on our platform (980,059 B vs 946,327 B), and it reuses WiFi only
  through its own ESPConnect config, not our NVS credentials. We keep its trick
  of pointing the boot partition back at `ota_0` on start.
- **Factory partition of 1024K.** This leaves room for Ethernet or extra
  features in the updater, but costs 64K of firmware headroom (270 KB to about
  205 KB). Rejected for now: 960K plus a build size gate is enough.
- **Wait for 8MB modules.** That would give true A/B OTA, but the existing 4MB
  fleet would never benefit, and there is no timeline.

## Consequences

**Positive:**

- Firmware can be updated over WiFi on every 4MB board: about 11 s for a 2 MB
  image on the bench.
- If an upload is broken or interrupted, the unit stays reachable through
  safeboot and can be fixed over WiFi without USB.
- The firmware keeps 270,464 B (13.4%) of headroom, and the filesystem keeps
  about 100 KiB of runtime room. The bench has about 75 KiB today.

**Negative:**

- **There is no A/B rollback.** The updater erases `ota_0` as it writes, so a
  failed upload destroys the old firmware. The unit is not a gateway until a
  good image is uploaded.
  Mitigation: safeboot stays reachable over WiFi, and the image is verified
  before it is marked bootable.
- **A valid image that crash-loops is caught only up to the health gate.**
  Bootloader rollback returns to safeboot when a new image fails before it marks
  itself valid. A crash loop that starts later is not caught.
  Mitigation: keep the health gate short. An app-level boot counter is an
  optional add-on (`docs/research/2026-10-15-crash-loop-rollback-feasibility.md`).
- **Every field unit needs one USB migration**, because the partition table and
  the LittleFS offset change. Settings are lost unless they are backed up.
  Mitigation: `flash_otgw.bat` gets a migration mode that saves and restores
  settings (TASK-1223.8).
- **The classic web UI goes away on the 2.0.0 line.** Mitigation: a gap audit
  ports what v2 lacks before removal (TASK-1223.11, TASK-1223.12).
- **The factory partition has only about 36 KB of slack.** Mitigation: a build
  size gate catches growth.
- **An Ethernet-only unit in safeboot is reachable only through the fallback
  access point.** Mitigation: document it; this path is rare.

## Open Questions

- [x] Crash-loop rollback: is `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` feasible with the pioarduino precompiled bootloader (TASK-1223.7)? The answer decides what the one-time migration image contains. — **Answered 2026-10-15 by User: Robert van den Breemen:** Yes, with the stock pioarduino bootloader: CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y is already in the shipped esp32s3 sdkconfig (libs sdkconfig:423) and bootloader. The main firmware defines verifyRollbackLater() and marks itself valid after a health gate; safeboot checks esp_ota_get_last_invalid_partition() before pointing boot back at ota_0; handover erases the whole otadata partition; the migration image writes bootloader.bin. Anti-rollback stays off. Evidence: docs/research/2026-10-15-crash-loop-rollback-feasibility.md. Bench proof in TASK-1223.9.
- [ ] Final-layout bench proof (TASK-1223.9): the spike proved the flow on a proof layout with a 2048K ota_0 and an empty filesystem, not on this exact layout.

## Related Decisions

- **Supersedes ADR-166 (single-app-slot boards: firmware OTA is USB-only)** for the 4MB targets once accepted.
- **ADR-190 (minified LittleFS staging)**: a precondition; it supplies the 668 KiB v2-only image size.
- **ADR-056 (protected admin endpoints)**: the handover endpoint and safeboot authentication follow it.
- **ADR-003 (HTTP only)**: the updater serves plain HTTP.
- **ADR-152 (v2 UI coexisting with classic)**: its coexistence part is retired by the classic-UI removal ADR (TASK-1223.12).

## References

- TASK-1222 (spike, GO), TASK-1223 (epic) with TASK-1223.2 to TASK-1223.13.
- `docs/research/2026-10-14-spike-1222-safeboot-ota-4mb.md` (measurements and bench proof).
- `docs/research/2026-10-14-4mb-ota-asset-options.md` (filesystem options).
- `partitions_otgw_esp32.csv`, `partitions_otgw_esp32_combo.csv` (current tables), `platformio.ini:232`, `:282`.
- `build.py:80-81`, `build.py:128` and `flash_otgw.bat:44-45` (current hardcoded offsets).
- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:70`, `:299`; `src/OTGW-firmware/restAPI.ino:3345` (`app_ota_available`).
- ESP-IDF OTA and bootloader fallback: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html>
- Tasmota safeboot: <https://tasmota.github.io/docs/Safeboot/>; MycilaSafeBoot: <https://github.com/mathieucarbou/MycilaSafeBoot>
