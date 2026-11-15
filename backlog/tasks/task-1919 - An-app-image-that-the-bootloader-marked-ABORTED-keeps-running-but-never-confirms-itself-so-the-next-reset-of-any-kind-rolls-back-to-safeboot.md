---
id: TASK-1919
title: >-
  An app image that the bootloader marked ABORTED keeps running but never
  confirms itself, so the next reset of any kind rolls back to safeboot
status: To Do
assignee: []
created_date: '2026-11-14T04:02:00Z'
updated_date: '2026-11-15T14:32:00Z'
labels:
  - bug
  - ota
  - safeboot
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 984000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found with the TASK-1774 RTC probe (otgw-bench-tester 09, .63, 2026-11-14 04:50-04:57, 09 scratchpad b1774\). A freshly flashed image (img=PENDING_VERIFY) that panics did not roll back on the first reset: ota_0 booted again and reported img=ABORTED reset=4; only the second panic rolled back to safeboot (/info app_rolled_back=true). safebootMarkAppValid() (OTGW-ModUpdateServer-esp32.h:154-161) confirms only PENDING_VERIFY, so the app running as ABORTED never confirms itself, and any later reset (planned restart, settings change, FS OTA, crash) rolls it back to safeboot. Fits beta.217 on .63 (2026-11-12): panic during an FS write, unit came back on main, rollback only after the 2nd FS upload (TASK-1774 notes). Reading (not verified): local framework-espidf bootloader_utility.c:396-405 marks PENDING_VERIFY as ABORTED at the next boot; :407-416 falls back to factory only when both otadata entries are invalid, else :440-443 boots the active valid entry, ota_0 with one OTA slot. That implies a second, still valid otadata entry; who left it is not shown, and the pinned bootloader is a precompiled binary. Pending proof: esptool read_flash of otadata (0xE000, 2 x 4 KB) on .63 in three states (fresh probe flash, after the 1st panic, after the rollback), by 09. Any change to when an image confirms itself changes ADR-191 rollback behaviour: maintainer decision, not decided here.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otadata dumps in the three states show the mechanism (which entry is active, its state and seq) or refute this reading
- [ ] #2 Robert decides how an ABORTED-running image is handled (options with consequences, put by the coordinator)
- [ ] #3 If a change is decided: old-vs-fix bench on .63 (panic, ABORTED boot, planned restart: old rolls back, fix per the decision)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related incident (otgw-bench-tester 09, .63, 2026-11-14 ~06:40, evidence 09 scratchpad b1774c\ old-ota-app / fix-ota-app logs, esptool-readmac.log): an app OTA from probe1774c.old to probe1774c.fix through the safeboot handover hung at 'timeout waiting for safeboot updater'; .63 was off WiFi for about 7 min (router lease still bound, no AP seen). esptool read_mac --after hard_reset on COM8 brought it back ('USB reset (host)'), still on the OLD probe; the retry worked. So esptool's USB-JTAG reset works on .63 (09's own pyserial RTS sequence did not). Cause of the hang not known; not reproduced.

2026-11-15 15:19-15:27 bench 09 on .61 (OTGW32, same esp32-combo image + partition table as .63; otadata 0xE000, 2 x 4 KB). esptool read_flash 0xE000 0x2000 over USB COM4 (each read itself resets the chip: the read happens in ROM download mode BEFORE the bootloader runs again, so each dump shows the state left by the previous boot; the esptool reset then acts as 'the next reset', in place of a panic). Entry = ota_seq / state / crc (crc = crc32(seq, init 0xFFFFFFFF), checked). Decoded: REF (beta.317 running, valid): [0] seq=1 VALID, [1] seq=1 NEW, both crc ok. A (probe1774+370b324 app OTA through safeboot, first boot, telnet RTCPROBE img=PENDING_VERIFY reset=3): [0] seq=1 PENDING_VERIFY, [1] seq=1 NEW. B (after the 1st reset; the probe booted again, RTCPROBE img=ABORTED reset=11): [0] seq=1 ABORTED, [1] seq=1 PENDING_VERIFY. C (after the 2nd reset): [0] seq=1 ABORTED, [1] seq=1 ABORTED -> safeboot /info app_valid=false app_rolled_back=true. D (317 uploaded through the safeboot updater, confirmed): [0] seq=1 VALID, [1] seq=1 ABORTED. Mechanism shown: BOTH otadata sectors hold a CRC-valid entry with the SAME seq=1 (both select ota_0, the only OTA slot). At the reset the bootloader marks the PENDING_VERIFY entry ABORTED, then takes the other entry (NEW, same seq, ota_0) and turns it into PENDING_VERIFY, so the same image gets a second boot while esp_ota reports img=ABORTED for the running app; only when both entries are ABORTED does the bootloader fall back to factory (safeboot). The second NEW entry is left by an earlier write (REF already had [1] NEW next to [0] VALID; after D [1] stays ABORTED and [0] is rewritten seq=1 VALID), i.e. the updater/OTA path writes seq=1 into one sector and leaves a same-seq copy in the other instead of incrementing the seq. Which code writes the [1] NEW copy is not shown by the dumps (needs a dump right after a safeboot upload, before the first boot). Restore: .61 back on beta.317+984d60a (manual safeboot upload, ota_flash refuses from safeboot), settings.ini diff vs before empty, no crashlog, bootcount 749 -> 753. Evidence: 09 scratchpad t1919/ (otadata-*.bin, decode.py, esptool-*.log, state*-key.txt, stateC-info.json).

2026-11-15 15:35-15:45 extra dumps (09, .61) for 'which code leaves [1] NEW with the same seq'. E (POST /safeboot handover only, unit in the safeboot updater, no upload): [0] seq=1 NEW, [1] blank (FF). The handover erases the whole otadata (OTGW-ModUpdateServer-esp32.h:140-148); at its own start the safeboot updater points the boot back at ota_0 with esp_ota_set_boot_partition() (safeboot/src/main.cpp:185-192) -> [0] seq=1 NEW. G (full ota_flash: handover + upload of 317, dump at the first boot): [0] seq=1 PENDING_VERIFY, [1] seq=1 NEW -> the SECOND esp_ota_set_boot_partition(ota_0) after the upload (main.cpp:192 path) writes the other sector with the SAME seq=1, because ota_0 is already the active boot partition with seq 1 and with one OTA slot the IDF keeps the seq for the same slot. H (after confirm): [0] seq=1 VALID, [1] seq=1 NEW = the REF pattern. So every handover+upload leaves two same-seq entries; that is the 'second life' of TASK-1919. The moment between the upload and the first boot cannot be dumped over WiFi (the updater restarts at once); the E->G pair brackets it. USB merged image for comparison: [0] seq=1 UNDEFINED, [1] seq=0 UNDEFINED (not the source). Side note (09 mistake): the G dump was taken 3 s after the OTA (image still PENDING_VERIFY), so its reset aborted [0]; recovered with a second ota_flash and a dump only after 60 s uptime. .61 ends on beta.317+984d60a confirmed, settings.ini diff empty. Files: t1919/otadata-E/G/H*.bin, img-merged-otadata.bin.
<!-- SECTION:NOTES:END -->
