---
id: TASK-1465
title: Zone diagnostics publish retained on every pass, bypassing the ADR-111 shadows
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:23:00Z'
updated_date: '2026-11-01T08:41:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 625000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. sat/zone/<n>/{active,output,error} go out via sendMQTTData on every pass (SATcontrol.ino:2313-2343), not through the publish-if-changed shadows (ADR-111).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Zone diagnostics go through the shadows (publish on change), or the exception is recorded in ADR-111 with the reason
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-11-01: tests/test_sat_zone_diag_shadows.py new 3/3, old 2 FAIL (unconditional sendMQTTData, no shadows); test_sat_zone_aggregation OK; evaluate 0 FAIL. Pending: bench check of sat/zone topics on .61 with a 2-zone sim after the next build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: tests/test_sat_zone_diag_shadows.py 3/3 OK (zone diagnostics through the ADR-111 shadows). Not bench-observed: the bench unit has no zones configured, so no zone topics appear. Build: esp32-combo 2.0.0-alpha.597+d458ac3 (build/…-flash.zip 08:51, built and posted by the coordinating session); the task commit is an ancestor of d458ac3 (git merge-base --is-ancestor). evaluate.py --quick 2026-11-01: 0 FAIL, Health 100%. Bench .61 (OT-Direct) runs 2.0.0-alpha.597+d458ac3 (/api/v2/device/info), up 25 min without reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
