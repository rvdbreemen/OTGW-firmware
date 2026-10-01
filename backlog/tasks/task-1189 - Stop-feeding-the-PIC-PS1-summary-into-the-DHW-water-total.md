---
id: TASK-1189
title: Stop feeding the PIC PS=1 summary into the DHW water total
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-01 17:09'
updated_date: '2026-10-01 17:09'
labels:
  - 2.0.0
  - mqtt
  - water-meter
dependencies: []
priority: medium
ordinal: 311000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-1123 (harness case R1). In PS=1 mode the PIC summary repeats the last stored MsgID 19 value. If the boiler stops answering MsgID 19 while summaries keep coming, the stale flow keeps counting: 80 L in 10 min at a stored 8 L/min. The summary carries no per-field age, so freshness cannot be told from it, and Home Assistant's total_increasing cannot correct an over-count downward.

Maintainer decision 2026-10-01: do not count the PS=1 summary, as on the 1.x line. Only real boiler Read-Ack frames for MsgID 19 (print_f88) feed the total. A gateway in PS=1 mode then reports no water total.

ADR-176 (Accepted) requires the accumulator to be called from both write sites, print_f88 and updatePSSummaryFloatState, so this needs an amending ADR before the code lands.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An amending ADR, authored through adr-kit and Accepted by the maintainer, replaces ADR-176's 'call the accumulator from both write sites' clause with 'only boiler Read-Ack frames (print_f88) feed the total; the PS=1 summary does not'
- [ ] #2 updatePSSummaryFloatState() no longer calls updateDHWWaterMeter(); print_f88() remains the only caller
- [ ] #3 Old-vs-fix on test/host/test_dhw_water_meter.py: HEAD counts the PS=1 summary (W2 6.0 L, R1 80 L), the fix counts 0 L for both, every other case and every mutant behaves as before or is re-targeted with a stated reason
- [ ] #4 User docs (docs/api/MQTT.md, the manuals, CHANGELOG) say the water total counts only real MsgID 19 frames and stays flat in PS=1 mode
- [ ] #5 Prerelease bump; build.bat green with fresh binaries; python evaluate.py exit 0; the host suite green
<!-- AC:END -->
