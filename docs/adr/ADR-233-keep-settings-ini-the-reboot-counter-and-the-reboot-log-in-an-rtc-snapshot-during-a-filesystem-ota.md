---
id: "ADR-233"
title: "Keep settings.ini, the reboot counter and the reboot log in an RTC snapshot during a filesystem OTA"
status: "Proposed"
date: "2026-11-14"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "filesystem OTA"
  - "settings persistence"
  - "RTC memory"
  - "reset safety"
  - "kept files"
aliases:
  - "RTC snapshot"
  - "FS OTA settings loss"
  - "kept files across a reset"
components:
  - "RTC_NOINIT filesystem-OTA snapshot"
  - "boot restore from the RTC snapshot"
symbols:
  - "_handleUploadWrite"
  - "_restoreRuntimeFiles"
  - "_keepFiles"
  - "RTC_NOINIT_ATTR"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-233 Keep settings.ini, the reboot counter and the reboot log in an RTC snapshot during a filesystem OTA

## Status

Proposed, 2026-11-14. The RTC survival gate passed on 2026-11-14 (routes R1 and R2, see Evidence); the power-cut
control R3 has not been run. Acceptance is the maintainer's decision; the implementation has not been written.

**Decision Maker:** Agent (otgw-firmware-ba), proposed for the maintainer's decision; the coordinator asked for
it (2026-11-14) after reviewing the TASK-1774 plan.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent: otgw-firmware-ba (Claude)"
    reason: Initial proposal from the TASK-1774 plan; gated on the RTC survival probe
    changed_via: adr-kit
```

## Context

A filesystem OTA (over-the-air update) writes a new LittleFS image over the whole 768 KB `spiffs` partition. The
runtime files that are not in the image survive it only in RAM: `settings.ini` as the RAM `settings` struct, and
ten other files (KEEP_LIST, TASK-1266/1286) as heap copies. Both are written back after the remount.

The window in which those files exist only in RAM, traced in `OTGW-ModUpdateServer-esp32.h` (origin/dev
5cc1612c0):

1. `:387` `_keepFiles()` copies the KEEP_LIST files to the heap; `:388` `LittleFS.end()`; `:393`
   `Update.begin()`. The Updater erases lazily per sector (comment `:375`), so the old filesystem is still intact.
2. `:445` the first `Update.write()` erases sector 0. **The window opens.**
3. All chunks of the image (786432 B; on .61, 40 % took 2.6 s, so about 6-7 s in total).
4. `:470` `Update.end(true)`, `:479` `_remountAfterFsWrite()` (format on a short upload, then mount).
5. `:542` `writeSettings()` (atomic since TASK-1913), `:545` `_restoreKeptFiles()`. **The window closes** after the
   last restored file.

A reset in that window (a panic, a watchdog, a software reset or a power cut) boots into a partial or fresh image.
The image ships a default `settings.ini` (`src/OTGW-firmware/data/settings.ini`, 400 B, hostname `OTGW2`, broker
`homeassistant.local`; present in the beta.233 `littlefs.bin`, `build.py` staging does not drop it). Once the
image is written over it, `OTGW-firmware.ino:334` `readSettings()` reads that default; when the write stopped
before it, the file is missing and `readSettings()` creates one with the compile-time defaults
(`settingStuff.ino:571-576`). Either way the user's settings are gone, and `reboot_count.txt` is gone too, so the
counter starts over. This is what bench unit .63 showed on 2026-11-10 (16 settings changed, the hostname among them,
boot count 178 to 3), and the beta.217 panic during a filesystem write on .63 (09, 2026-11-12, 1 of 9 writes) is a
reset in this window. The cause is read from the code; bench proof is part of TASK-1774.

Measured sizes on the bench units (09, 2026-11-14): `settings.ini` 5640 B (.61) and 5659 B (.63);
`reboot_count.txt` 4-5 B; `reboot_log.txt` 1435-1470 B (a 20-line ring, so this is its steady state); the rest of
KEEP_LIST 331-335 B on these units, but with caps that add up to 9824 B (`OTGW-ModUpdateServer-esp32.h:632-645`).

The memory that can carry data across a reset without a flash write is RTC slow memory: RTC_NOINIT (RTC memory
that the startup code does not initialise, so it keeps its content across a software, panic or watchdog reset but
is garbage after power-on). On the ESP32-S3 with the pinned Arduino libs: `memory.ld` `rtc_slow_seg` starts at
0x50000000 + 512 (`CONFIG_ULP_COPROC_RESERVE_MEM 512`), so the region is 0x50000200-0x50002000, 7680 B. The
beta.233+6afbade ELF uses 0x64 B of it (`.rtc_noinit` 0x44, `.rtc.force_slow` 0x20). That leaves about 7.5 KB.

The safeboot updater (ADR-191) is a separate binary that runs after an app rollback, often for minutes. Its ELF
has a LOAD segment of 0x20 B at 0x50000200 (`readelf -l`, the IDF variable `s_sleep_sub_mode_ref_cnt` in
`.rtc.force_slow`), so every safeboot start overwrites 0x50000200-0x5000021F (TASK-1918). It has no other RTC slow
sections. Whether IDF startup, the PHY or WiFi touch the rest of RTC slow in either binary has not been verified;
the probe in TASK-1774 tests exactly that.

## Decision

At the start of a filesystem OTA the firmware copies `settings.ini`, `reboot_count.txt` and `reboot_log.txt`,
in that order, into one RTC_NOINIT snapshot above 0x50000220 (magic, lengths, CRC32) and arms it at the first
written chunk. A boot that finds it armed and valid first formats the partition when the image write itself was
cut off, then takes `settings.ini` from the snapshot, and the counter and the log only where they are missing or
empty; every successful settings write on a sound filesystem and every boot clear it.

The snapshot is one buffer of the real budget (about 7.4 KB, the free RTC slow memory minus a margin; fixed at
build time from the ELF). `settings.ini` is taken whole or not at all: when it alone does not fit, the snapshot
holds no settings and the firmware logs that on telnet ("settings.ini N B does not fit the RTC snapshot of M B:
not protected during this filesystem write"). The counter and the log follow while they fit, each whole or not at
all. The other KEEP_LIST files keep their existing heap copies only (TASK-1266/1286) and are not in the snapshot.

The copy is made before `LittleFS.end()` (`:388`), the last moment the files can be read, and stays unarmed;
it is armed at the first successful `Update.write()` (`:445`), when the old filesystem starts to disappear. An
upload refused before any byte (image too large `:392`, `Update.begin()` failed `:395`) wipes it.

`settings.ini` must win over a file that exists, because the image ships a default one; an armed snapshot is itself
the proof that a filesystem write started and its restore never finished. Staleness is closed by the other side:
every successful `writeSettings()` drops the settings slot, because the file on flash is then newer than the copy.
During the write, `flushSettings()` does not run (it is behind `isFlashing()`), so the only settings write in that
window is the restore itself (`:542`). A settings save after a failed OTA drops the slot too. The counter and the
log are not in the image, so for them "missing or empty" is the right test, and a counter that the device already
wrote again after the OTA is never replaced. The snapshot is wiped after a restore whose settings write succeeded
(`_restoreRuntimeFiles()`), and at every boot, also when the restore there fails, so it lives at most one boot
session. A failed end (no remount) or a write error keep it armed, so that the next boot can still restore.
The boot restore runs in `setup()` after the mount (`:330`) and before `readSettings()` (`:334`), before
`startWebserver()`, so no filesystem OTA can run at the same time (the TASK-1909 ordering). Each restored file is
written through a temp file and `fsCommitTempFile()` (TASK-1913).

A reset in the middle of the image write leaves a half-written image: the new superblock and metadata, the rest
of the partition still old blocks. It mounts and reads, but it is full and takes no write that needs a new block
(bench round 1, see Evidence). So the snapshot carries one more field, `consistent`: 0 when armed, 1 as soon as
the partition holds a complete image (right after a successful `Update.end()` of a complete upload) or a fresh
format (the in-session format of a partial image, `_remountAfterFsWrite()`). A boot that finds it armed and not
consistent formats LittleFS before the restore and writes the TASK-1495 marker (`FS_UPDATE_INCOMPLETE_FILE`): the
same end state as an upload that was aborted in-session today, settings, counter and log present and the web UI
files gone until the filesystem is uploaded again. The settings slot is only dropped by a settings write on a
consistent filesystem; on a half-written one the boot formats anyway and the snapshot's copy is the one left.

## Decision Contract

### Must

- Fill the snapshot before `LittleFS.end()` at upload start; arm it at the first successful `Update.write()` of a
  filesystem target, not at upload start.
- Wipe it when an upload is refused before any write (too large, `Update.begin()` failed), after a restore whose
  settings write succeeded, and at every boot, also when the boot restore fails.
- Drop the settings slot after every successful `writeSettings()` while the snapshot is consistent.
- Mark it consistent right after a successful `Update.end()` of a complete image and after the in-session format of
  a partial image; at boot, format LittleFS before the restore when it is armed and not consistent, and write the
  TASK-1495 marker.
- At boot, take `settings.ini` from an armed, valid snapshot even when the file exists (the image ships a
  default); restore the counter and the log only when they are missing or 0 B.
- Check magic, every length against its slot, and the CRC32 over the whole payload before using any of it.
- Store `settings.ini` whole or not at all; log on telnet when it does not fit.
- Keep the snapshot outside 0x50000200-0x5000021F, checked at boot (address of the buffer >= 0x50000220, else the
  snapshot is disabled and that is logged).

### Must Not

- Must not write the snapshot to flash (NVS, LittleFS or a partition) in this decision.
- Must not keep a partial `settings.ini`.
- Must not use RTC FAST memory: `CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP=1` in the pinned libs gives it to
  the heap in both the main firmware and the safeboot updater.
- Must not print the snapshot content (it holds secrets, see Consequences).

### Exceptions

- None.

### Verification

- `test/host/test_fs_ota_rtc_snapshot.py` (TASK-1774): the real snapshot block, `fsCommitTempFile()`,
  `writeSettings()` and `updateRebootCount()` against a LittleFS model with a reset after the image replaced the
  partition. The old code boots on the image's default `settings.ini` with the counter at 1, the fix restores
  both. Further cases: a reset after the restore's own settings write, a settings save after a failed OTA, a
  refused upload, an oversized `settings.ini`, a damaged CRC, an existing counter. The call order in the upload
  server and in `setup()` is checked on the source text; five mutants are each caught.
- Bench (otgw-bench-tester): the RTC survival probe through the safeboot route on .63, then a forced reset in the
  middle of a filesystem OTA on .63, old build versus fix, comparing `/api/v2/settings` and the boot count.

## Evidence

RTC survival probe, a throwaway build `2.0.0-probe1774+370b324` (lane `lane/ba-1774-probe`, never pushed): a
7168 B seeded pattern with magic and CRC32 in RTC_NOINIT at 0x50000244 (`nm`, 0x1C10 B), the image confirming itself
only on a telnet key. Run by otgw-bench-tester 09 on .63 (Classic with PIC), 2026-11-14 04:50-04:57; logs in 09's
scratchpad `b1774\` (`telnet-keys.log`, `r1-info.json`, OTA logs).

- **R1, rollback route: PASS.** Armed (seed 0xb2f06eeb, img=PENDING_VERIFY), panic. The unit booted ota_0 again
  with img=ABORTED (see below); a second panic rolled back to safeboot (`/info`: `"app_valid":false,
  "app_rolled_back":true`). About 3.7 min in safeboot, then the app re-upload to `/update` (HTTP 200) and the main
  boot: `addr=0x50000244 size=7184 len=7168 magic=ok crc=ok seed=0xb2f06eeb img=PENDING_VERIFY reset=3`.
- **R2, plain panic on a confirmed image: PASS.** Seed 0xf9060ac3, panic, boot line `magic=ok crc=ok` with the same
  seed, `img=VALID reset=4`.
- **R3, power cut: not run** (needs a physical power cut by the maintainer). RTC slow memory losing its content
  without power is the documented behaviour, not the claim under test; the Consequences list it as not protected.

Deviation in R1, relevant to ADR-191: the first panic of a PENDING_VERIFY image did not roll back. The local
framework-espidf `bootloader_utility.c:396-405` marks the entry ABORTED at the next boot (matches `img=ABORTED`),
and `:407-416` defaults to factory only when both otadata entries are invalid; otherwise `:440-443` boots the
active valid entry, which with one OTA slot is ota_0 again. The observation fits a second, still valid otadata
entry; who left it is not shown by the code (and the bootloader in the pinned libs is a precompiled binary, not
proven identical to that source). The app running as ABORTED never confirms itself (`safebootMarkAppValid()` acts
on PENDING_VERIFY only, `OTGW-ModUpdateServer-esp32.h:154-161`), so the next reset of any kind rolls back. For this
decision it means the snapshot must survive two resets, which R1 shows it does. Tracked in TASK-1774.

Bench round 1 of the implementation (otgw-bench-tester 09, .63, 2026-11-14 ~06:30-06:55, 09's scratchpad
`b1774c\`): a panic at 300000 B of a filesystem OTA (throwaway hook, key K) on a confirmed image, the same hook on the
real old code (`2.0.0-probe1774c.old+483b285`) and on the first implementation (`2.0.0-probe1774c.fix+ce53698`).
Old: 16 settings changed, hostname `OTGW2`, boot count 27 -> 1, reboot log 20 -> 2 lines. First implementation: hostname
and counter kept (30 -> 31), but 14 settings at defaults and the log lost; both left a full filesystem (freeBytes 0,
every new file 507) and the settings write kept failing after the boot. That is the half-written image described
above: the restore of `settings.ini` could not land, `readSettings()` read a truncated file (the first wrong key is
the one written right after `MQTTseparatesources`), the 4 B counter fitted an inline write. The format at boot is the
answer; host case T9 reproduces the round-1 result on the first implementation and passes on the rework.

## Alternatives Considered

- **NVS blob (the ESP-IDF key-value store in flash).** It would also survive a power cut. Rejected for now: the
  `nvs` partition is 0x5000 B (20 KB, 5 pages, about 16 KB usable after the page kept free for garbage collection,
  `partitions_otgw_esp32.csv`), shared with the WiFi credentials and the safeboot `host`/`pw` keys
  (`OTGW-ModUpdateServer-esp32.h:128-135`, ADR-191). A 7 KB blob can fail for lack of space, or leave too little
  for a later WiFi or safeboot write, which is exactly the data a recovery needs. It also costs a flash write of
  several hundred milliseconds on the AsyncTCP task at upload start. Remains the fallback if the probe shows RTC
  does not survive the safeboot route.
- **Compress `settings.ini` with the ROM miniz.** `tdefl_compress` and `tinfl_decompress` are in the S3 ROM
  (`esp32s3.rom.ld:73-84`). Rejected for v1: `esp_rom/include/miniz.h` builds with `TDEFL_LESS_MEMORY 1` and
  `MINIZ_NO_MALLOC`, and its constants put the compressor state at about 100 KB of heap (32 KB dictionary, 24 KB
  LZ code buffer, about 31 KB output buffer, hash tables; an estimate from the header, not measured), allocated
  at upload start on a Classic without PSRAM. A follow-up can measure it if `settings.ini` grows past the budget.
- **The free tail of the safeboot partition (about 70 KB after the 0.89 MB updater).** Survives everything.
  Rejected: it relies on the bootloader ignoring data after the image end, and on nobody growing the updater;
  clever, not clear.
- **The coredump partition.** Rejected: a panic writes the coredump there, exactly when the snapshot is needed.
- **Refuse a filesystem OTA while the new app is still unconfirmed (PENDING_VERIFY).** Addresses the rollback
  half only, not the settings loss, and changes the web UI update flow. Kept as option D in TASK-1774, only if
  this decision fails its gate.
- **Do nothing.** Every reset during a filesystem write keeps resetting the settings to defaults, including the
  WiFi-independent settings such as MQTT and SAT, and the boot counter. Rejected: seen on a bench unit twice.

## Consequences

**Positive:**

- A panic, a watchdog reset or a software reset during a filesystem OTA keeps `settings.ini`, the boot counter and
  the reboot log, today's measured 7.1-7.2 KB of them, without any extra flash write.
- With the snapshot surviving the safeboot route, an app rollback during the write becomes the recovery ADR-191
  intends (upload the app again over WiFi) without data loss.

**Negative:**

- Not protected: a power cut or brownout during the write or during the recovery (RTC slow memory loses its
  content without power);
- Not protected, and unchanged by this decision: a power cut during the image write leaves no snapshot, so the next
  boot mounts the half-written image as before: it runs on whatever that image shows, takes no write, and needs a
  filesystem upload (seen on .63 in the old build). Detecting that state without a snapshot is not part of this
  decision; the SAT state files, Dallas labels and heating-curve markers (heap copies only, as
  today); `settings.ini` when it alone is larger than the snapshot (logged, then not protected at all); the
  counter or the log when they no longer fit after `settings.ini`.
- The budget margin is thin: 7.2 KB of about 7.4 KB on the bench units. An install with BLE bind keys and more
  SAT settings can push `settings.ini` past 6 KB, and then the counter and log drop out first, and later the
  settings themselves. *Mitigation*: the size is logged at every filesystem OTA; compression is the named
  follow-up.
- Secrets: `settings.ini` holds the HTTP and MQTT passwords in clear text (ADR-056), so the snapshot holds them in
  RTC slow memory across a reset. *Mitigation*: cleared on every OTA exit and right after the boot restore; never
  printed. A physical attacker with a debugger could read RTC memory, but could read flash too.
- About 7.4 KB of RTC slow memory is reserved for a rare event, which leaves little for other RTC_NOINIT records.

## Open Questions

- [x] Does an armed snapshot survive panic, rollback, minutes in safeboot, the app re-upload and the main boot on
  .63 (the TASK-1774 probe)? If not, this ADR is withdrawn in favour of the NVS alternative or option D.
  — **Answered 2026-11-14 by Agent: otgw-firmware-ba (Claude):** Yes, on .63 (otgw-bench-tester 09, 2026-11-14 04:50-04:57, probe 2.0.0-probe1774+370b324): seed 0xb2f06eeb with magic and CRC ok after panic, an ABORTED boot, a second panic, about 3.7 min in safeboot, the app re-upload and the main boot (route R1). See Evidence.
- [x] Does IDF startup, the PHY or WiFi write into RTC slow memory above 0x50000220 in either binary? (Covered by
  the probe; not readable from the ELF.)
  — **Answered 2026-11-14 by Agent: otgw-firmware-ba (Claude):** Not observed: the 7184 B probe at 0x50000244 came back intact through both routes (R1, R2), so nothing in that range was written by IDF startup, the PHY, WiFi or the safeboot updater on that run. Not a proof for other builds; the implementation keeps the boot address check.
- [ ] The exact snapshot size: the free RTC slow memory of the build that implements this, minus what margin?

## Related Decisions

- ADR-191 (Update firmware over WiFi on 4MB boards through a safeboot factory partition): the rollback to safeboot
  and the 30 s self-confirmation that make the reset window also a rollback window.
- ADR-056 (Protected admin endpoint security and secret-handling contract): the secrets in `settings.ini`.

## References

- TASK-1774 (plan and evidence), TASK-1909 (`bFsImageWriting`, `_remountAfterFsWrite`), TASK-1913
  (`fsCommitTempFile`), TASK-1918 (safeboot overwrites 0x50000200-0x5000021F), TASK-1266/1286 (KEEP_LIST).
- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:342-596` (upload start, write, end, remount, abort) and
  `:612-721` (KEEP_LIST); `src/OTGW-firmware/OTGW-firmware.ino:330-334` (boot mount and `readSettings()`),
  `:1175-1179` (30 s self-confirmation); `src/OTGW-firmware/settingStuff.ino:563-590`;
  `src/OTGW-firmware/helperStuff.ino:280-292` (the existing RTC_NOINIT reboot-reason record).
- Pinned libs: `framework-arduinoespressif32-libs/esp32s3/ld/memory.ld:78-91`, `esp32s3.rom.ld:73-84`,
  `include/esp_rom/include/miniz.h:39,628,694`, `*/include/sdkconfig.h` (`CONFIG_ULP_COPROC_RESERVE_MEM 512`,
  `CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP 1`).
- IDF bootloader (local framework-espidf copy, not proven identical to the IDF in the pinned libs):
  `components/bootloader_support/src/bootloader_utility.c:396-405,446-451`.
- Bench sizes: otgw-bench-tester 09, 2026-11-14 ~03:40, .61 and .63.
