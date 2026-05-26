---
id: TASK-403
title: >-
  Tune MQTT gated fanout spacing from 1000ms to 250ms + disable BGTRACE/OTTRACE
  instrumentation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 12:57'
updated_date: '2026-04-24 13:00'
labels:
  - mqtt
  - fanout
  - diagnostics
  - production-hygiene
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two production-hygiene tweaks after TASK-402 hardware validation: (1) tighten MQTT_GATED_PUBLISH_SPACING_MS from 1000ms to 250ms so the 60s heartbeat storm spreads over ~4s instead of ~16s (16 bits at 250ms each), and boot-time fanout completes in ~11s instead of ~44s. 250ms is fast enough that HA barely notices the spread, slow enough to keep handleMQTT cycles below 5ms. (2) Disable BGTASKS_TRACE and OTPROCESS_TRACE macros (flag 1 -> 0). These were left on during the TASK-397/400/401/402 bug-hunt; now that the burst behaviour is fixed they can be switched off to stop the high-volume telnet log spam. Flags remain one-char flip to re-enable if regressions surface.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT_GATED_PUBLISH_SPACING_MS constant changed from 1000 to 250 in OTGW-Core.ino; comment updated to describe the rationale and the change-detect-bypass-no-timer-update invariant
- [x] #2 BGTASKS_TRACE flag changed from 1 to 0 in OTGW-firmware.ino
- [x] #3 OTPROCESS_TRACE flag changed from 1 to 0 in OTGW-Core.ino
- [x] #4 Build clean on ESP8266 (python build.py --firmware): no warnings, no errors
- [x] #5 python evaluate.py --quick shows 0 new failures
- [ ] #6 Hardware verification (user-performed): MQTT subscriber confirms heartbeat storm spreads over ~4 seconds instead of ~16, and boot-time fanout completes within 15 seconds
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Post-TASK-402 production hygiene: tightened the rate-gate spacing from 1000ms to 250ms per user preference (4x faster fanout: 16-bit heartbeat storm now spreads over ~4 seconds instead of ~16), and disabled the BGTASKS_TRACE + OTPROCESS_TRACE diagnostic macros now that the TASK-397/400/401/402 bug hunt is complete.

Changes:
- OTGW-Core.ino: MQTT_GATED_PUBLISH_SPACING_MS 1000 -> 250. Comment updated to explain the change-detect-bypass-no-timer-update invariant (was mis-worded in TASK-402 comment) and to document the new boot-fanout timing (~11s total) and heartbeat spread (~4s).
- OTGW-Core.ino: OTPROCESS_TRACE 1 -> 0. Macro compiles out to (void)0, zero runtime cost.
- OTGW-firmware.ino: BGTASKS_TRACE 1 -> 0. Same treatment.
- Both flags keep their #if guards + re-enable comments in place; flip back is one-char if a regression surfaces.

Verification:
- python build.py --firmware: exit 0, no warnings, no errors.
- python evaluate.py --quick: 31/31 passed, 2 pre-existing warnings, 94.3% health.
- Runtime verification (user-performed): AC #6 — observe on hardware that heartbeat storm spreads over roughly 4 seconds and boot fanout completes within ~15 seconds.
<!-- SECTION:FINAL_SUMMARY:END -->
