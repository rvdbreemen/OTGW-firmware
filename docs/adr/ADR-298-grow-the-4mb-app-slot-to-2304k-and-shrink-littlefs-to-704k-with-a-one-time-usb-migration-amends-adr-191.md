---
id: "ADR-298"
title: "Grow the 4MB app slot to 2304K and shrink LittleFS to 704K, with a one-time USB migration (amends ADR-191)"
status: "Accepted"
date: "2026-11-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "partition-table"
  - "flash-layout"
  - "migration"
  - "app-size"
aliases:
  - "repartition"
  - "safeboot2"
components:
  - "4MB partition layout"
  - "USB layout detection"
symbols: []
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-298 Grow the 4MB app slot to 2304K and shrink LittleFS to 704K, with a one-time USB migration (amends ADR-191)

## Status

Accepted, 2026-11-26.

## Status History

```yaml
status_history:
  - date: 2026-11-26
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-26
    status: Accepted
    changed_by: "Robert van den Breemen (decision, interactive answer to the coordinator, relayed), recorded by Agent: Claude (otgw-architect)"
    reason: "Accepted: decision Robert van den Breemen, 2026-11-26 about 21:00 local, verbatim 'Accepteren, migratie in de stable' (interactive answers to coordinator questions, relayed; Q2 'Eén tabel voor alle drie (Recommended)' about 21:10), recorded by Agent: Claude (otgw-architect); four gates passed in adr-kit"
    changed_via: adr-kit lifecycle
```

## Context

Terms used in names and paths below: OTGW (OpenTherm Gateway), OT (OpenTherm), ESP (Espressif), RAM (random-access
memory), TASK (a backlog task id).

The esp32-combo image with remote telemetry and outbound TLS (Transport Layer Security, ADR-218) no longer fits the
build's 98 % size gate for the app slot of ADR-191.

Line numbers are on `dev` at 9bec44df4 unless marked otherwise. Evidence labels in this ADR: *measured* = from a build or a file read with a tool; *read* = read in the source, with
`file:line`; *estimate* = reasoning, not measured; *not verified* = still to be proven on a bench unit.

- *measured* (otgw-poc-dev, `OTGW-field-data/_comms/telemetry_size_options.md`): the merged image
  `OTGW-firmware-esp32-combo-2.0.0-beta.433+477c06d.ino.bin` (githash 477c06d, prerelease build, build log
  build-20261126-053115.log, artifacts in `OTGW-build-artifacts/merged-poc-477c06d/`) is 2,263,808 B = 98.69 % of
  app0 (2,293,760 B), 15,924 B over the 98 % gate. Dev before the merge (bfae2b8) was 2,159,984 B. The temporary
  99.5 % limit in `scripts/check_app_size.py` ends on 2026-11-29.
- *measured* (symbol diff of the two ELF (Executable and Linkable Format) files): the growth is mbedTLS +54.1 KB (kilobytes), telemetry code and data +28.4 KB,
  other +7.7 KB. The telemetry code is a flat long tail; about 2-3 KB could be cut (*estimate*).
- *read*: the flash map (`partitions_otgw_esp32.csv`) assigns all 4 MB (megabytes) of flash. app0 can grow only by taking space from the
  partition behind it, LittleFS. App offsets and sizes are 64K-aligned, so the smallest step is +64K.
- *measured* (on the staging copy of the same build, not on a unit): the LittleFS image is 433,880 B of payload in 23
  files, 495,616 B in 4 KB blocks = 63.0 % of 768K. Runtime files (settings.ini, logs, uploaded PIC (the gateway
  microcontroller) hex files) come on top on a real unit.
- *read*: the partition table at 0x8000 is not written by any over-the-air update. A unit keeps its table until it is
  flashed over USB (Universal Serial Bus).

**Decision maker:** Robert van den Breemen (maintainer), Discord DM (direct message) 1575388418098069604,
2026-11-26 06:51 local, verbatim "A", a Discord reply to the question 1575385616374562847 ("Vraag 7: welke route
sluit het image-gat voor de stable?"), where A read: "herpartitioneren +64K (USB-ronde voor bestaande units)". The
reply reference was checked through the Discord API (application programming interface) by otgw-architect. Robert's answer is the choice of route; this
ADR still needs his acceptance.

## Decision

All three 4MB targets (esp32, esp32-classic, esp32-combo) move to one new table. It is the ADR-191 layout with the
border between app0 and LittleFS moved by 64K; every partition before app0, and the coredump partition, stay where
they are.

| Partition | Type | Offset | Size | Change |
|---|---|---|---|---|
| nvs, otadata, safeboot | unchanged | unchanged | unchanged | none |
| app0 | app/ota_0 | 0x100000 | 0x240000 (2304K) | +64K (was 0x230000) |
| spiffs | data/spiffs (LittleFS) | 0x340000 | 0xB0000 (704K) | moved +64K, -64K (was 0x330000, 0xC0000) |
| coredump | data/coredump | 0x3F0000 | 0x10000 | none |

The new layout has its own name, `safeboot2`, so that every tool can tell it apart from the ADR-191 `safeboot`
layout by the app0 size in the table, not by the string "safeboot".

*estimate* (arithmetic, otgw-poc-dev): the merged image would be 95.95 % of the new app0, about 48 KB under the 98 %
gate. LittleFS would be 68.7 % used by the shipped image, about 225 KB free. *measured*: the build log of the same
build (`build-20261126-053115.log:240`) reads "LittleFS budget OK: 484 KiB assets + 96 KiB reserve <= 768 KiB
partition"; 484 + 96 = 580 KiB also fits 704K, so the budget check (`build.py:926-967`) still passes. ADR-191's
"668 KiB minimum" is its 2026-10-15 figure; the assets have shrunk since (ADR-190 minified staging).

Existing units move once, over USB, with `flash_otgw --migrate`, the same flow as the earlier legacy-to-safeboot
change (TASK-1223.8). Settings are saved over HTTP (Hypertext Transfer Protocol) first and put back; NVS (non-volatile storage, WiFi credentials,
admin password) is not touched.

**A unit that is not migrated, handed a release with the new table** (*read* unless marked):

- Firmware through the browser on the old firmware's `/update` page: accepted while the app image is at most
  0x230000 B; a larger image is classified `usb` and refused (`update.html:192`, `:208`). The new 704K file-system
  image is refused as `fssize` (`update.html:205`).
- Firmware through safeboot's own upload page or `scripts/ota_flash.py`: no size check in our code; the Arduino
  Updater bounds the write by the on-flash ota_0 (*not verified*). An image above 2240K then fails and the unit
  stays in safeboot until a smaller image or a USB migration (ADR-191 recovery path).
- While the app still fits 2240K (2,263,808 B today), the unit runs the new firmware with its old LittleFS content,
  because the file-system step is refused. That is a firmware and web assets mismatch. A firmware that is older than
  the guards of this ADR cannot be patched, so this path stays open for units that update straight from such a
  release (Open Question 3 and the release-order Must).
- The new firmware on the old table, at first boot (*read*, otgw-dev, lane/d3-trim f384db4cb; not built or run on a
  unit): it mounts the old 768K file system with settings intact, logs a warning at boot ("partition table is X,
  built for Y ... migrate once over USB"), reports `partition_layout` different from `partition_layout_expected` in
  device info, and its `/update` page blocks the one-action update and refuses images for the other table. It writes
  neither the table nor the settings.

The table changes before the 2.0.0 stable, not after: once a stable is out, every stable unit would need the USB
round (*estimate*, otgw-poc-dev).

## Decision Contract

### Must

- Change the layout in one place, `partitions_otgw_esp32.csv`, and keep every mirror equal to it in the same commit:
  `build.py:92-98` and its migrate images, `flash_otgw.bat:74-79`, `flash_otgw.sh:104-106`, `flash_esp.py:52-53`,
  `scripts/check_app_size.py`, `scripts/otgw-test.py:52-53`, `scripts/device-verify.sh`, and
  `src/OTGW-firmware/webui/update.html:159` (`APP_MAX`, `FS_SIZE`), and the firmware's `OTGW_PARTITION_LAYOUT`. A
  host test fails when a mirror differs from the CSV (comma-separated values): `tests/test_partition_mirrors.py`
  (otgw-dev, lane/d3-trim 76ad1afaf).
- Detect the layout of a connected unit by the app0 size in its table (0x230000 = `safeboot`, 0x240000 =
  `safeboot2`) in `flash_otgw.bat`, `flash_otgw.sh` and `flash_esp.py`. Refuse `--update` when the unit's layout
  differs from the images' layout and point to `--migrate`. Reason (*read*, otgw-dev, `flash_otgw.bat:471`): the
  tools now match the string "safeboot", which both tables contain, so `--update` with new images would write the
  file system at 0x340000, inside the old LittleFS partition, on an unmigrated unit.
- Make the main firmware's file-system update refuse, before it erases anything, an image whose size differs from
  the partition (a mklittlefs image spans the whole partition; the superblock block count is not read), and an
  upload without `size=` in the query string, which cannot be compared. Reason (*read*, otgw-dev): `OTGW-ModUpdateServer-esp32.h:653-673` accepts an image smaller
  than the partition, so a 704K image on an old 768K partition is written, and the mount then fails on the block
  count (`lfs.c:4608-4613` in joltwallet/littlefs); `LittleFS.begin()` does not format (`OTGW-firmware.ino:337`). The
  unit boots, but the web UI (user interface) files and settings.ini are gone until a matching image is flashed.
- Report the layout: `partition_layout` (`legacy`, `safeboot`, `safeboot2`, `unknown`) and
  `partition_layout_expected` in `/api/v2/device/info`, and a boot warning on telnet when the two differ.
- Mark every app image with the table it is built for, as the string `otgw-layout=<layout>` in the image. The
  `/update` page and `scripts/ota_flash.py` read the marker from the image before they send it and refuse an image
  whose marker differs from the unit's `partition_layout`; the page builds its own search string at run time, so its
  text in the image is no false match. Reason (review, otgw-architect): comparing the unit only with what the
  RUNNING firmware expects misses the transition itself, the first upload of a release for the other table.
  An image without a marker (older releases) is not blocked by this check.
- The `/update` page blocks the one-action update while `partition_layout` differs from
  `partition_layout_expected`, and points to `--migrate`.
- `flash_esp.py` writes a partition table only together with both the firmware and the file-system image, so a new
  table never points at a file system that was not written.
- Build the migrate image pair for the new table and let `--migrate` accept a `safeboot` unit as its source.
- Release order (Robert, Open Question 3): first a bridge release on the old table that carries the guards (layout
  report, image marker check, file-system size and `size=` checks) and marks its own image `otgw-layout=safeboot`;
  only a later release changes the table. A unit that runs the bridge refuses the table-change release over WiFi
  and points to `--migrate`.
- Fallback for a unit that skips the bridge (case A of question 9, accepted by Robert): its older firmware and the
  safeboot updater cannot check the marker, so the new app can land on the old table. The unit then runs the new
  firmware on its old 768K file system with settings intact and old web files, warns at boot and in `/update` that
  a USB migration is needed, and refuses further images for the other table. The release notes and the
  #beta-testing announcement say this.
- Announce the change in #beta-testing with the first public beta that carries the new table, prepared by
  BlueFlame or otgw-docs: it must be flashed over USB once and it is a breaking change. Robert, same source as
  Open Question 1, verbatim: "Leg uit in het beta-kanaal bij de volgende publieke beta waar de partition table
  verandert, dat het via USB geflasht moet worden en dat het een breaking change betreft."
- Say in the release notes of the first release with this table: every existing 4MB unit needs one `--migrate` over
  USB; an over-the-air update alone is not enough; a migrated unit is rolled back only with the old release's full
  merged image, never with the old tool's `--update`.

### Must Not

- Rewrite the partition table from the running firmware or from safeboot. *read* (otgw-dev, in the hybrid-build tree wt-d3-trim,
  `sdkconfig.defaults:3151`; the shipped libs `sdkconfig.h:1438` alike) sets `CONFIG_SPI_FLASH_DANGEROUS_WRITE_ABORTS=y`, and moving LittleFS also moves data,
  which is a power-loss hazard with no recovery path.
- Change the safeboot, nvs, otadata or coredump partitions.
- Ship a release with the new table while any tool still detects the layout by the "safeboot" string only.

### Exceptions

- None. There is no 8MB target in the tree today (*read*, otgw-dev: `platformio.ini:117-120`, one CSV); the ADR-191
  exception for 8MB boards stays as it is.

### Verification

- Host tests (to be written with the code, red on the old code): every mirror equals the CSV; the three USB tools
  refuse `--update` on a layout mismatch; the file-system update refuses an image whose size differs and an upload
  without `size=`; the `/update` page and `ota_flash.py` refuse an image whose layout marker differs from the unit;
  `flash_esp.py` refuses a table without both images. otgw-dev reports these red on the old code and green on
  lane/d3-trim (b86614dec to f384db4cb); the counts are in his commit messages, not re-run by otgw-architect.
- Review status (otgw-architect, 2026-11-26): findings F1 (the image marker), F2 (no upload without `size=`), F3
  (`flash_esp.py` full set) and F4 (mirror test) were raised on b86614dec..bd58685cb and fixed in 76ad1afaf and
  f384db4cb. Read again: the f384db4cb diff of `OTGW-ModUpdateServer-esp32.h`. *not verified*: the `scripts/ota_flash.py`
  and `update.html` changes of f384db4cb were not re-read in full, and nothing was built or run on a unit.
- Build log: app image against the new app0 under the 98 % gate; LittleFS budget against 704K.
- Bench, by otgw-bench-tester, on one unit still on the `safeboot` table and on .63, with capture transcripts. *not verified* until it has run:
  - an over-the-air update with the new zip through the browser and through `scripts/ota_flash.py`: the file-system
    step is refused and nothing changes;
  - `--update` with new images on the old table: refused;
  - `--migrate`: settings.ini and dallas_labels.ini with 0 differences, WiFi kept, duration measured;
  - rollback with the old release's full merged image: the old layout comes back and boots.
- Whether the ADR-233 RTC (real-time clock) snapshot restores settings after a failed mount: *not verified*, to be checked on the same
  bench run.

## Alternatives Considered

- **Trim instead (mbedTLS switches, settings help back to LittleFS, Home Assistant discovery tables out of the
  app).** About 31 KB in total (*estimate*; the mbedTLS part alone was computed at 9.7-11.5 KB by otgw-dev, TASK-2980).
  Rejected by Robert's choice: three changes with their own risk. The mbedTLS switches need a hybrid IDF (IoT
  Development Framework) library build, a build-tooling change; moving settings help reverses ADR-194; the discovery
  tables belong to ADR-077 and would then fail without the file system. The trim alone leaves the image about 6 KB
  over the gate. The mbedTLS numbers stay on the list for 2.1.0.
- **Own encryption instead of TLS (server public key in the firmware, X25519 + XChaCha20-Poly1305).** Saves about
  50 KB (*estimate*) and hides the content from Cloudflare too, but needs a server sidecar, a protocol design and a
  review: 4-6 days (*estimate*). Not safe before the stable; a candidate for 2.1.0 with its own ADR.
- **Keep the temporary 99.5 % limit for the stable.** No work and no USB round, but it leaves 18 KB for every fix
  after the stable and loosens a gate that exists for over-the-air update and safeboot margin. A policy choice;
  Robert chose A instead.
- **A second CSV only for esp32-combo.** Only combo needs the room, but two tables double the mirrors and the
  detection cases, and every unit needs one USB flash either way (*estimate*, otgw-dev). Rejected for one shared table,
  subject to Open Question 2.
- **+128K.** More headroom, but LittleFS would drop to 640K with about 160 KB free before runtime files (*estimate*);
  +64K already gives 48 KB under the gate.

## Consequences

**Positive:**

- Remote telemetry fits under the 98 % gate with about 48 KB of headroom (*estimate*), without a trim and without
  touching ADR-194 or ADR-077.
- The tools learn to tell layouts apart by size; this also closes the gap that `--update` cannot tell two tables
  with the same partition names apart.
- The file-system update stops accepting an image it cannot mount.

**Negative:**

- Every existing 4MB unit (OT-Direct boards and Classic boards with an S3 module) needs one USB flash with a
  laptop. Robert's estimate: about 10 to 15 units, all beta testers (Open Question 1).
- Files on LittleFS other than settings.ini and dallas_labels.ini are lost in the migration (*read*, otgw-dev,
  `flash_otgw.bat:236-261`).
- LittleFS loses 64K: about 225 KB stays free for runtime files (*estimate*).
- A unit that is not migrated keeps working on its old table as long as the app image fits 2240K: the new app finds
  app0 and LittleFS by type at runtime (*read*, otgw-dev; no layout constants in `src/` apart from `update.html:159`).
  Once the app grows past 2240K, safeboot's update fails and the unit stays in safeboot until a smaller image or a USB
  migration (*estimate*: the Arduino Updater bounds the write by the partition; Updater source not read, *not verified*).

## Open Questions

- [x] 1. Robert: how many units (his own and the testers') run the current `safeboot` table? This sets the size of
  the USB round and whether the migration needs a guided step in the release notes or per-tester help. — **Answered
  2026-11-26 by Robert (interactive in the coordinator's session, at or before 07:08 local, 2026-11-26; no DM id):** verbatim "Er zijn op dit moment een beperkt aantal beta-testers die de huidige
  partition-indeling gebruiken. Denk aan een stuk of 10 tot 15." His own estimate: about 10 to 15 units, all
  beta testers. It supports the +64K route: the USB round is small and happens before the stable.
- [x] 2. Robert: one table for all three 4MB targets (recommended: one set of mirrors, one detection rule), or a second
  table for esp32-combo only?
  — **Answered 2026-11-26 by Robert van den Breemen (decision, interactive answer to the coordinator, relayed), recorded by Agent: Claude (otgw-architect):** verbatim 'Eén tabel voor alle drie (Recommended)', Robert, about 21:10 local, interactive answer to a coordinator question in the coordinator's session (relayed by the coordinator; not seen by otgw-architect). One table for esp32, esp32-classic and esp32-combo, as the Decision states.
- [x] 3. Robert: on a unit that is not migrated, should a release with the new table refuse the firmware step
  too, so firmware and web assets never mismatch? The block is built (image marker, see Must), but it works only
  on a unit that already runs a firmware with the guards. Options: (A) one public beta with the guards and the old
  table first, then the table change (the release-order Must); (B) ship guards and table in one beta and accept
  that units updating from an older beta get new firmware with old web files until they migrate (they warn at
  boot and keep their settings). Recommendation: A. It costs one beta cycle and closes the gap for every unit that
  takes that beta; B saves the cycle but leaves the mismatch for the 10 to 15 tester units, which have to migrate
  anyway. — **Answered 2026-11-26 07:26 local by Robert** (Discord DM 1575397374891786251, verbatim "B met terugval
  naar A"), answering the coordinator's question 9 (DM 1575397129101254667, 07:25 local). His letters follow that
  question's labels, not this list: there B = "tussenrelease" (a bridge release on the old table with the marker
  check and the guards), A = accept that a unit which skips the bridge ends with new firmware on the old table.
  His DM has no reply reference; it is the next message after question 9, read as its answer (coordinator and
  otgw-architect, checked through the Discord API). Outcome: option A of this list, the bridge first, with the
  fallback case kept as a known consequence.
- [x] 4. Robert: acceptance of this ADR. It amends the layout table and the first Must of ADR-191; the rest of ADR-191
  stays.
  — **Answered 2026-11-26 by Robert van den Breemen (decision, interactive answer to the coordinator, relayed), recorded by Agent: Claude (otgw-architect):** verbatim 'Accepteren, migratie in de stable', Robert, about 21:00 local, interactive answer to the coordinator's question 'ADR-298 ... Wat doen we ermee?' (relayed by the coordinator; not seen by otgw-architect). Accepted consequence: every 4MB unit needs one USB migration (settings saved first), over-the-air updates work again afterwards; the migration ships before the stable. Release order (plan of record, coordinator 2026-11-26 21:1x): beta N = bridge (guards and image marker, old table); beta N+1 = the table change with the USB round; beta N+2 = the REST change of ADR-300.

## Related Decisions

- **ADR-191 (Update firmware over WiFi on 4MB boards through a safeboot factory partition)**: amended. Its layout
  table and the first Must ("Use the layout above for every 4MB target") take the sizes of this ADR; its updater
  rules, rollback and migration rules stay.
- **ADR-218 (outbound TLS on boards with PSRAM (pseudo-static RAM))**: the cause of the growth.
- **ADR-194 (static text assets in firmware flash)** and **ADR-077 (streaming MQTT (Message Queuing Telemetry Transport) discovery)**: kept as they are by
  this choice.
- **ADR-233 (RTC snapshot during a file-system update)**: its behaviour after a failed mount is part of the bench.
- **ADR-210 (core dump)**: the coredump partition does not move.

## References

- `OTGW-field-data/_comms/telemetry_size_options.md` (otgw-poc-dev, measurements and options, 2026-11-26).
- `OTGW-field-data/_comms/partition_repartition_facts.md` (otgw-dev, facts A-I with `file:line`, 2026-11-26).
- `partitions_otgw_esp32.csv`, `build.py:92-98`, `build.py:926`, `flash_otgw.bat:74-79`, `flash_otgw.bat:471`,
  `flash_esp.py:52-53`, `safeboot/src/main.cpp:162`, `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:653-699` (the main firmware refuses app images on 4MB tables at :685-699),
  `src/OTGW-firmware/webui/update.html:159`.
- TASK-2980 (size trim, re-scoped to the repartition), TASK-1223.8 (earlier migration).
- ESP-IDF (Espressif IoT Development Framework) partition tables (offsets and sizes of app partitions are 64K-aligned; the table lives at 0x8000): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html
