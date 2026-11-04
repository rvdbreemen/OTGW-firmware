---
id: TASK-1189
title: Stop feeding the PIC PS=1 summary into the DHW water total
status: Done
assignee:
  - '@claude'
created_date: '2026-10-01 17:09'
updated_date: '2026-10-01 18:15'
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
- [x] #1 An amending ADR, authored through adr-kit and Accepted by the maintainer, replaces ADR-176's 'call the accumulator from both write sites' clause with 'only boiler Read-Ack frames (print_f88) feed the total; the PS=1 summary does not'
- [x] #2 updatePSSummaryFloatState() no longer calls updateDHWWaterMeter(); print_f88() remains the only caller
- [x] #3 Old-vs-fix on test/host/test_dhw_water_meter.py: HEAD counts the PS=1 summary (W2 6.0 L, R1 80 L), the fix counts 0 L for both, every other case and every mutant behaves as before or is re-targeted with a stated reason
- [x] #4 User docs (docs/api/MQTT.md, the manuals, CHANGELOG) say the water total counts only real MsgID 19 frames and stays flat in PS=1 mode
- [x] #5 Prerelease bump; build.bat green with fresh binaries; python evaluate.py exit 0; the host suite green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-01, commit 19ee510b0 (alpha.396).

AC#1 (ADR-181):
- Written through adr-kit and related to ADR-176 on both sides (adr relate).
- adr-lint: every gate PASS for ADR-176 and ADR-181.
- Accepted by the maintainer in this session after the acceptance packet (adr accept --confirm).
- Precision fix before the first commit: the draft said a PS=1 gateway's total 'stays where it is'. dhwWaterMeterHasData() is set only by the first updateDHWWaterMeter() call, so booted in PS=1 mode no total is published at all (the entity shows unknown). The total only stays flat on a gateway that switched to PS=1 after counting. The decision itself did not change.

The mechanism, checked in the PIC source:
- gateway.asm:1644/1648: SummaryReport prints stored values through PrintStoredVal.
- gateway.asm:2429/2436: HandleResponse stores the boiler's value through StoreValue.
- gateway.asm:117: PS=1 described.

AC#2 (code):
- updatePSSummaryFloatState() case 19 only writes OTcurrentSystemState.DHWFlowRate.
- print_f88() is the only caller of updateDHWWaterMeter() (grep: OTGW-Core.ino:2712).
- dhwWaterMeter.ino header comments updated.

AC#3 (old vs fix, test/host/test_dhw_water_meter.py, real code sliced by anchor):
- Red: the harness with the new expectations against the unchanged firmware (OTGW-Core.ino as at HEAD 92cfcc2b7) fails exactly:
  - W2: 6.0000 L, want 0, with state 6.0;
  - W6: 5002.1667 L, want 5000;
  - R1: 80.0000 L, want 0.
  Every other case passes.
- Green: all cases pass, RESULT PASS, exit 0.
- Mutants:
  - M3 (the old feed restored, outside OT-Direct mode) fails R1, W2, W6. That is the same set as the red run, so M3 is this change's OLD.
  - M3b (the feed in every mode) fails R1, W2, W5, W6.
  - M25 (NaN counted) is re-targeted from W6 to U7, because no PS=1 field reaches the meter any more; it fails U7. M26 fails U7.
- The --old-rev mode against 4bbceed61^ (before the meter existed) still passes: OLD fails exactly W1, W4, W5 (OLD_DEFECT updated from W1,W2,W4,W5,W6,R1, since W2/W6/R1 now expect no counting) and has none of the wiring; 119 checks yes.

AC#4 (docs):
- docs/api/MQTT.md: 'What counts' rewritten; the 'Open point, pending a maintainer decision' paragraph and the 1.x PS=1 difference removed; the flow-bound sentence trimmed; PS=1 added to 'When it is published' (the entity stays unknown).
- docs/manuals/en/ch09-api-reference.md and docs/manuals/nl/h09-api-referentie.md: one sentence at the reset endpoint, the only place the manuals mention the total, plus a link to MQTT.md.
- docs/c4/c4-code-otgw-core.md: the caller list and host-proof text; the open point removed.
- CHANGELOG [Unreleased]: the TASK-1123 entry corrected, since it was never released, and tagged TASK-1189 / ADR-181.
- docs/adr/README.md: ADR-181 entry; ADR-176 marked as amended.

AC#5:
- bin/bump-prerelease.sh alpha.395 -> alpha.396 in the same commit.
- build.bat --target all:
  - esp32, esp32-classic and esp32-combo each show [SUCCESS] for firmware and filesystem;
  - 'Build completed successfully!', exit 0;
  - artifacts OTGW-firmware-{esp32-otgw32,esp32-classic,esp32-combo}-2.0.0-alpha.396+19ee510.ino.bin / .littlefs.bin, written 19:47:54-19:54:20 after the 19:44:41 start.
- python evaluate.py: exit 0, 81 passed / 4 warnings / 0 failed (unchanged).
- Host suite, every test file in test/host, tests and scripts/tests, run one at a time after the build:
  - all 19 host-test files pass: test/host x10, tests x6 (adr_governance, build, evaluate, refresh_storm with 2 skips, static_integrity, webui_asset_versioning), scripts/tests extract_json_field, heap_soak_driver and run_coverage_test;
  - the other 5 files in that sweep are live-device probes that otgw-test.py normally runs against a gateway (test_load, test_mqtt_reliability, test_telnet, test_webserver, test_ws_liveload). They failed with 'device not reachable', 'cannot even connect' or a missing module, because no gateway was online. They are not host tests and need the bench.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stops feeding the PIC PS=1 summary into the DHW water total, as the maintainer decided on 2026-10-01.

Why: a PIC builds its summary from stored values without their age (gateway.asm SummaryReport -> PrintStoredVal, HandleResponse -> StoreValue), so a stale MsgID 19 flow kept counting: 80 L in 10 minutes in host case R1.

What changed:
- updatePSSummaryFloatState() now only writes DHWFlowRate.
- print_f88() B frames are the only meter feed, as on 1.x.
- A PIC gateway in PS=1 mode adds no water; booted in that mode it shows the entity as unknown.
- ADR-181 (Accepted) amends ADR-176's two-write-sites clause.
- Docs updated: MQTT.md, both manuals, c4-code-otgw-core.md, CHANGELOG, ADR index.

Commit 19ee510b0, alpha.396.

Evidence:
- AC#1: ADR-181 adr-lint PASS on every gate; accepted by the maintainer.
- AC#2: grep shows a single updateDHWWaterMeter() call, OTGW-Core.ino:2712 in print_f88().
- AC#3, red run: the new expectations against the old firmware fail exactly W2/W6/R1 (6.0 L, 5002.17 L, 80 L).
- AC#3, green run: all cases pass. Mutant M3, the old feed, fails R1/W2/W6; M3b also fails W5; M25 re-targeted to U7. The --old-rev mode against 4bbceed61^ still passes.
- AC#4: docs as listed.
- AC#5: build.bat --target all [SUCCESS] x3 with fresh alpha.396+19ee510 images; evaluate.py exit 0 (81/4/0); 19/19 host-test files pass.

Note on AC#5: the sweep also ran 5 live-device probes from scripts/tests (test_load, test_mqtt_reliability, test_telnet, test_webserver, test_ws_liveload). They need a reachable gateway, failed with 'device not reachable' or a missing module, and are not host tests.
<!-- SECTION:FINAL_SUMMARY:END -->
