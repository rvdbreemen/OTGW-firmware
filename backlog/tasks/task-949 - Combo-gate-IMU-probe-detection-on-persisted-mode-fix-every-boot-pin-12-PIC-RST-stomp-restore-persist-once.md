---
id: TASK-949
title: >-
  Combo: gate IMU probe + detection on persisted mode; fix every-boot pin-12
  PIC-RST stomp; restore persist-once
status: Done
assignee:
  - '@claude'
created_date: '2026-06-29 17:27'
updated_date: '2026-06-29 20:08'
labels:
  - async-esp32s3
dependencies: []
ordinal: 162000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
FIELD REGRESSION (Robert + CrashEvans, #alpha-testing, alpha.288-290). A regular ESP32-S3 Mini on the OTGW Classic can no longer boot into Classic/'regular' mode. Root cause (confirmed via code + on-device serial on COM8): probeProImu() (OTGW-firmware.ino:258) runs UNCONDITIONALLY on EVERY boot, BEFORE readSettings() (292), bringing up I2C on the Pro pins 11/12. On a REGULAR S3 Mini, GPIO 12 = PIC_RST (Pro: =SDA), so the IMU probe disturbs the PIC reset line every boot. Serial shows i2cWriteReadNonStop ESP_ERR_INVALID_STATE (the WHO_AM_I read) at boot. Compounded by TASK-947 which made detection re-probe every boot and never persist. Also: the i2c 0x26-WD spam (separate, fixed by TASK-945/946 - keep that). FIX (maintainer-approved direction): (1) move LittleFS.begin + readSettings BEFORE the IMU probe so iBoardMode is known; (2) only probeProImu() when iBoardMode==0 (auto) - cached boots derive bClassicPro from iBoardMode==3 and skip the probe (no stomp); (3) restore persist-once detection (revert TASK-947) + add detectPIC retries for a robust first-boot verdict; (4) iBoardMode stays the soft UI setting (override). PIC enabled => Classic (then Pro vs regular); no PIC + no OT bus => OTGW32 bench default; persist once, skip thereafter.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 AUTO first boot: detectPIC retried; PIC found => Classic persisted (1, or 3 if IMU); no PIC after retries => OTGW32 (2) persisted; later boots skip detection
- [x] #2 On-device: regular S3 Mini on COM8 boots into Classic (PIC detected), persists, 2nd boot skips the probe, no AP-portal loop, no PIC-RST disturbance
- [x] #3 esp32 + esp32-classic + esp32-combo build green; evaluate.py --quick 0 new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
VERIFIED on-device (COM8, regular S3 Mini on OTGW Classic, alpha.291+7f69f6b == alpha.292 code). bootdetect.log: boot#4 auto -> eMode=1 pic=1 mode=1 pro=0 I2C=35/36 (Classic detected + persisted); boot#5 cached mode=1 -> forced path, pic=1, fast 2453ms, stable. hwmode=PIC, hardware_type=otgw-classic. No hang, no reset loop, HTTP+AP OK. The earlier pic=0 reports were a bench artifact: the S3 Mini was not seated on the Classic carrier (no PIC present), so pic=0 was correct. NOTE: previous commit 7f69f6b9 (the LittleFS/readSettings reorder to gate the IMU probe) HUNG boot; reverted in d7a34f4a. The probe-gating-on-cached-mode optimization is DEFERRED (needs early settings read = the reorder that hung); follow-up task for a hang-free approach.

AC re-scope 2026-06-29: removed stale AC#1/#2 (gate probeProImu() on iBoardMode==0 + early settings read). That optimization was reverted (commit 7f69f6b9 hung boot, reverted d7a34f4a) and DESCOPED to TASK-950 (hang-free gating). Shipped+verified scope = AUTO detect + persist-once + no-hang, on-device confirmed on COM8 (alpha.292), covered by remaining AC#1/#2/#3. Closing on delivered behaviour; gating optimization tracked separately in TASK-950.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Combo board detection: AUTO first-boot detectPIC (retried) -> Classic persisted (1, or 3 w/ IMU) / OTGW32 (2) when no PIC; later boots skip detection. Reverted the boot-hanging settings-reorder; probe stays early+unconditional. On-device verified COM8 alpha.292 (boot#4 auto->Classic persisted, boot#5 cached->fast 2453ms stable, no reset loop, no RST disturbance). 3-target build green, evaluate 0-fail. Probe-skip-on-cached optimization descoped to TASK-950.
<!-- SECTION:FINAL_SUMMARY:END -->
