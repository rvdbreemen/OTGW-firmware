---
id: TASK-512
title: 'fix(otgw): port ADR-066 PS=1 master-topic gate from dev (TASK-483)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-02 16:53'
updated_date: '2026-05-02 17:04'
labels:
  - mqtt
  - ADR-066
  - PS=1
  - port
  - esp32
  - esp8266
dependencies: []
references:
  - docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md
  - src/OTGW-firmware/OTGW-Core.ino
  - TASK-483
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the PS=1 summary path master-topic gate from dev (commit 07d67990, TASK-483) to feature-dev-2.0.0-otgw32-esp32-sat-support. Same root cause: publishPSSummaryFieldValue() in OTGW-Core.ino bypasses the ADR-066 master-topic gate, so PS=1 users see boiler Write-Ack garbage flapping in MQTT base topic and OTcurrentSystemState for non-echo MsgIDs (Tr/TrSet/TrSetCH2/TSet/TsetCH2/MaxRelModLevelSetting). Live-bus tiers were already ported via TASK-481; only the PS=1 path remains open on this branch.

Approach: manual port via Edit tool (not cherry-pick) due to divergence between dev and feature-2.0.0 in OTGW-Core.ino, evaluate.py, and ADR-066. Code content is identical; line numbers differ.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New helper is_msgid_valid_for_master_topic_in_ps_summary() added (true for OT_READ, falls back to OTlookup.bSlaveEchoesValue)
- [x] #2 publishPSSummaryFieldValue computes validForMaster once at entry from the global OTlookupitem
- [x] #3 All 6 value-bearing cases (ot_f88, ot_s16, ot_u16, ot_s8s8, ot_u8u8, ot_u8) gate sendMQTTData/publishPSSummarySplitBytes and OTcurrentSystemState writes on validForMaster
- [x] #4 ot_flag8flag8 case left unchanged (status-flag semantics, parallel to live-bus exception)
- [x] #5 setMsgLastUpdated remains called regardless (cosmetic epoch tick)
- [x] #6 One DebugTln trace per suppressed call for support correlation
- [x] #7 evaluate.py gate check_ps_summary_master_topic_gate added (ADR-080 binding-pattern CI gate)
- [x] #8 ADR-066 Amendment 1 documents PS=1 inclusion on this branch
- [x] #9 python evaluate.py --quick passes (no new violations beyond pre-existing baseline)
- [x] #10 ESP8266 build clean (python build.py --firmware)
- [x] #11 ESP32-S3 build clean (esp32 FQBN)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-02 implementation landed on feature-2.0.0 working tree (branch feature-dev-2.0.0-otgw32-esp32-sat-support).

**Edits:**
- `src/OTGW-firmware/OTGW-Core.ino`: helper `is_msgid_valid_for_master_topic_in_ps_summary` inserted after `is_value_valid_for_master_topic` (around line 1287). `publishPSSummaryFieldValue` now computes `validForMaster` once at function entry, gates `sendMQTTData` / `publishPSSummarySplitBytes` and `OTcurrentSystemState` writes across all 6 value-bearing cases (ot_f88, ot_s16, ot_u16, ot_s8s8, ot_u8u8, ot_u8). `ot_flag8flag8` untouched. `setMsgLastUpdated` retained ungated. One `DebugTln` trace per gate hit.
- `evaluate.py`: new `check_ps_summary_master_topic_gate` after `check_design_system_drift`, registered in the run-list before `check_adr_references_resolve`.
- `docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md`: Amendment 1 inserted before Future amendments.

**Verification (in progress):**
- evaluate.py --quick: 97.1% health, 59 passed (+1 vs baseline = the new PS=1 gate), 0 failures, 2 pre-existing warnings unrelated to this change. AC #9 satisfied.
- ESP8266 build (first attempt, parallel with ESP32): FAILED with `.pio/build/esp8266/.sconsign311.dblite: No such file or directory`. SCons cache corruption from concurrent .pio access by parallel builds, not a compile error on the patched code. Will rebuild with `--clean` after the ESP32 build completes.
- ESP32-S3 build: still running, will report once complete.

2026-05-02 ESP32-S3 build: SUCCESS exit 0. Artifact `OTGW-firmware-esp32-2.0.0-alpha+f12a753.ino.bin` (1.81 MB) plus `-merged.bin` (1.87 MB). No compiler errors or warnings on the patched code. AC #11 satisfied.

ESP8266 clean rebuild (`build.py --firmware --target esp8266 --clean`) started after ESP32 completed, running sequentially this time to avoid the SCons cache race condition seen on the first parallel attempt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of the ADR-066 PS=1 master-topic gate from dev (TASK-483, commit 07d67990) to feature-dev-2.0.0-otgw32-esp32-sat-support. Closes the same flapping-regression-via-PS=1 gap on the 2.0.0 branch.

### What changed

- **`src/OTGW-firmware/OTGW-Core.ino`:** new helper `is_msgid_valid_for_master_topic_in_ps_summary(OTlookup_t)` (inserted after `is_value_valid_for_master_topic`, around line 1287). `publishPSSummaryFieldValue()` now caches `validForMaster` once at function entry from the global `OTlookupitem` and gates `sendMQTTData` / `publishPSSummarySplitBytes` and every `OTcurrentSystemState.X = ...` write across the six value-bearing cases (`ot_f88`, `ot_s16`, `ot_u16`, `ot_s8s8`, `ot_u8u8`, `ot_u8`). `ot_flag8flag8` left untouched (status-flag semantics, parallel to live-bus exception). `setMsgLastUpdated` left ungated (cosmetic epoch tick). One `DebugTln` trace per gate hit, format `"PS=1 master-topic gate suppressed MsgID %u (%s): bSlaveEchoesValue=false"`.
- **`evaluate.py`:** new `check_ps_summary_master_topic_gate` (ADR-080 binding-pattern CI gate) added after `check_design_system_drift` and registered in the run-list before `check_adr_references_resolve`. Walks the function body, verifies `validForMaster` is computed from the helper, then confirms every publish/state-write site in each value-bearing case is wrapped in an `if (validForMaster` guard.
- **`docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md`:** Amendment 1 inserted before "Future amendments". Documents the PS=1 inclusion, decision rationale (4 numbered points), per-MsgID effect on existing PS=1 users, and the corresponding CI gate per ADR-080.

### Why

`_reuzenpanda_`'s log + screenshot on Discord `#beta-testing` 2026-04-30 confirmed PS=1 mode in his setup and showed Tr / MaxRelModLevelSetting Write-Ack garbage flowing through the only ungated writer (`publishPSSummaryFieldValue`) on dev's beta.3/beta.4. Same code path exists on feature-2.0.0; same gap; same fix.

### Verification

- `python evaluate.py --quick`: 97.1% health, 59 passed (+1 vs baseline = the new PS=1 gate), 0 failures, 2 pre-existing warnings unrelated to this change.
- ESP8266 build clean: RAM 84.6% (69344/81920 bytes), Flash 77.4% (808932/1044464 bytes). Artifact `OTGW-firmware-esp8266-2.0.0-alpha+f12a753.ino.bin` + merged binary (0.78 MB).
- ESP32-S3 build clean: artifact `OTGW-firmware-esp32-2.0.0-alpha+f12a753.ino.bin` (1.81 MB) + merged binary (1.87 MB). No compile errors or warnings on patched code.
- First ESP8266 build attempt failed on `.pio/build/esp8266/.sconsign311.dblite: No such file or directory` (SCons cache race when run in parallel with the ESP32 build). Resolved by sequencing: clean, then ESP8266 alone.

### Risk and follow-up

For boilers where Tr / TrSet / TrSetCH2 / TRoomCH2 / MaxRelModLevelSetting / RFsensorStatus appear in the PS=1 summary, those six MsgIDs stop publishing to the base topic and stop updating `OTcurrentSystemState`. HA entities tied to those base topics will go stale until cleared. This is the intended outcome (the prior PS=1 values were the same protocol-zero garbage the live-bus path already suppresses).

No new task needed; this is the 2.0.0 mirror of TASK-483 and is now self-contained on this branch.
<!-- SECTION:FINAL_SUMMARY:END -->
