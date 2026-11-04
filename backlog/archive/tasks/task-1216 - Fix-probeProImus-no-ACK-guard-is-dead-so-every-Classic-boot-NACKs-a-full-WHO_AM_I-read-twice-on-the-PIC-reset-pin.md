---
id: TASK-1216
title: >-
  Fix: probeProImu's no-ACK guard is dead, so every Classic boot NACKs a full
  WHO_AM_I read twice on the PIC reset pin
status: Done
assignee:
  - '@claude'
created_date: '2026-10-11 22:25'
updated_date: '2026-10-17 15:05'
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
- [x] #1 probeProImu sends a zero-length presence probe first (beginTransmission + endTransmission(), which goes through i2c_master_probe) and reads WHO_AM_I only when the address ACKs; S3 Mini Pro detection still returns pro=1 on a Pro (or the Pro path is shown unchanged by code reading if no Pro is on the bench)
- [x] #2 Bench old-vs-fix on the Classic + S3 Mini: the USB console boot capture shows the 4 IMU NACK lines on OLD and none on FIX; /bootdetect.log still shows pro=0 pic=1 mode=1 on FIX
- [x] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (alpha.430): probeProImu() first sends a zero-length presence probe (beginTransmission + endTransmission(), which arduino-esp32 3.3.5 routes to i2c_master_probe with only a log_v on failure: esp32-hal-i2c-ng.c:259-263), and reads WHO_AM_I only on ACK. On ACK the read is the same as before, so the Pro path is unchanged (code reading; no Pro on the bench). Bench Classic + LOLIN S3 Mini (COM8), USB console captured across a /ReBoot (an RTS reset dropped the USB-CDC handle, so the capture keeps the port open and reboots over HTTP): alpha.429 shows the 4 IMU lines (i2c-ng.c:369 + Wire.cpp:520, twice; .tmp/bench-1216/old-a429-boot.txt); alpha.430 shows 0 (fix-a430-boot.txt). The remaining i2c-ng.c:275 i2cWrite line is the 0x26 watchdog disarm, not the IMU probe (TASK-1217 scope). /bootdetect.log on alpha.430: pic=1 mode=1 pro=0 (fix-a430-bootdetect.log). The zero-length probe still clocks START + address on SDA=12/SCL=11, so on a plain S3 Mini the PIC reset line still sees one short address pulse per address instead of a write + read; detectPIC() resets the PIC afterwards as before. Gates: build.bat SUCCESS for esp32-combo, esp32 and esp32-classic (.tmp/build-a430-*.log); evaluate --quick 0 FAIL.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
probeProImu() now checks for an ACK with a zero-length I2C probe before reading WHO_AM_I. The old guard used endTransmission(false), which never touches the bus in arduino-esp32 3.x, so every Classic boot without the Pro's IMU logged 4 I2C error lines (which also reach the PIC RX before the UART0 mute, see TASK-1217). Old-vs-fix USB console capture on the Classic-S3: 4 -> 0 IMU lines; bootdetect still pic=1 mode=1 pro=0. Three targets build green, evaluator 0 FAIL. alpha.430.
<!-- SECTION:FINAL_SUMMARY:END -->
