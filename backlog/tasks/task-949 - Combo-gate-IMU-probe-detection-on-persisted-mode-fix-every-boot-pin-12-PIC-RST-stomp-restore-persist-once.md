---
id: TASK-949
title: >-
  Combo: gate IMU probe + detection on persisted mode; fix every-boot pin-12
  PIC-RST stomp; restore persist-once
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-29 17:27'
updated_date: '2026-06-29 17:27'
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
- [ ] #1 probeProImu() runs ONLY when iBoardMode==0; on cached boots (1/2/3) it is skipped and bClassicPro derives from iBoardMode==3 (verify via serial: no i2cWriteReadNonStop on a cached boot)
- [ ] #2 LittleFS+readSettings load before the IMU probe / WD disarm; WD disarm still targets the correct (cached or probed) I2C pins
- [ ] #3 AUTO first boot: detectPIC retried; PIC found => Classic persisted (1, or 3 if IMU); no PIC after retries => OTGW32 (2) persisted; later boots skip detection
- [ ] #4 On-device: regular S3 Mini on COM8 boots into Classic (PIC detected), persists, 2nd boot skips the probe, no AP-portal loop, no PIC-RST disturbance
- [ ] #5 esp32 + esp32-classic + esp32-combo build green; evaluate.py --quick 0 new failures
<!-- AC:END -->
