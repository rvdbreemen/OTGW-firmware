---
id: TASK-947
title: >-
  Combo: iBoardMode=2 (OTDirect) sticky mis-cache from single-shot unconfirmed
  detection
status: To Do
assignee: []
created_date: '2026-06-29 09:11'
labels:
  - async-esp32s3
dependencies: []
ordinal: 160000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Aggressive review 2026-06-29 (combo PR#667 / ADR-127 path, NOT introduced this session). In the combo auto path (OTGW-firmware.ino:343-370), when detectPIC() (a single ETX check, OTGW-Core.ino:694, can transiently fail on a busy/missed-ETX PIC) does not see a PIC, the else branch calls initOTDirect() which sets state.hw.eMode=HW_MODE_OT_DIRECT UNCONDITIONALLY (OTDirect.ino:851) — probeOTBus() only logs, does not gate the mode — then caches settings.iBoardMode=2 and persists it. On the next boot, iBoardMode==2 (OTGW-firmware.ino:343) takes the forced-OTDirect path and NEVER re-probes the PIC. So a single glitchy detectPIC on a real Classic+PIC board permanently strands it in OTDirect, recoverable only by a manual boardmode override. Asymmetric vs the PIC cache (mode 1/3) which re-runs detectPIC each boot and self-corrects. Root fragility: persisting a decision from a single-shot, hardware-unconfirmed fallback.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 iBoardMode=2 (OTDirect) is only cached when probeOTBus() actually observed a live/idle OT bus, OR the cached==2 path re-probes the PIC briefly each boot before committing, OR OTDirect is not persisted (re-detected every boot)
- [ ] #2 A single transient detectPIC miss on a real Classic+PIC board does NOT permanently strand it in OTDirect (verified by forcing a detect miss or by design argument)
- [ ] #3 Manual boardmode override still works; no regression to the PIC (1/3) or OTGW32 detection paths
- [ ] #4 Build esp32-combo + esp32-classic green; evaluate.py --quick 0 new failures
<!-- AC:END -->
