---
id: TASK-1216
title: >-
  Fix: probeProImu's no-ACK guard is dead, so every Classic boot NACKs a full
  WHO_AM_I read twice on the PIC reset pin
status: To Do
assignee: []
created_date: '2026-10-11 22:25'
labels:
  - bug
  - esp32
  - i2c
dependencies: []
priority: low
ordinal: 336000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the Classic + LOLIN S3 Mini bench, 2026-10-12 (esp32-combo alpha.418). probeProImu() (OTGW-firmware.ino:212-227, ADR-158 S3 Mini Pro detection) reads WHO_AM_I at 0x6A and 0x6B on SDA=12/SCL=11. Its guard 'if (Wire.endTransmission(false) != 0) continue' is dead code: in arduino-esp32 3.3.5 endTransmission(false) only sets nonStop and returns 0 without touching the bus (framework Wire.cpp:457-469), so requestFrom always runs and a board without the QMI8658 logs 4 error lines per boot (i2c-ng.c:369 + Wire.cpp:520, twice). On a plain S3 Mini GPIO12 is the PIC reset line (active low, OTGWSerial resetPic), so the probe's START and address bits pulse the PIC reset for microseconds; harmless today because detectPIC() resets the PIC again later and bootdetect shows pic=1, but not intended. Root cause traced and adversarially verified (workflow wf_337ef6e6-edd).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 probeProImu sends a zero-length presence probe first (beginTransmission + endTransmission(), which goes through i2c_master_probe) and reads WHO_AM_I only when the address ACKs; S3 Mini Pro detection still returns pro=1 on a Pro (or the Pro path is shown unchanged by code reading if no Pro is on the bench)
- [ ] #2 Bench old-vs-fix on the Classic + S3 Mini: the USB console boot capture shows the 4 IMU NACK lines on OLD and none on FIX; /bootdetect.log still shows pro=0 pic=1 mode=1 on FIX
- [ ] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
