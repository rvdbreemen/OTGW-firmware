---
id: TASK-2925
title: >-
  docs: audit the manual against beta.393-396 (update-check chip, phone notice,
  beta-only AP fallback guard, dashed legend swatch of task 2037)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T18:42:00Z'
updated_date: '2026-11-19T19:20:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1061000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator request 2026-11-19: check that the manual text (EN/NL) matches the UI and behaviour of beta.393 (task 2918), beta.394 (task 2923, beta-only), beta.396 (task 2037, ba). Fix locally, push after ba's 396 push.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each item checked against the code on origin with file:line; mismatches fixed in EN and NL
- [x] #2 MANUAL.md rebuilt; build_manual --check and test_manual_* green
- [x] #3 Pushed only after ba's 396 is on origin, via the coordinator's slot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit 2026-11-19 (origin 22bfbb835 + ba's 95f898778 for 396, not yet on origin). (1) Update chip (393): ch03/h03 text matches v2.js updAsk (:8604-8661: chip after .hdr-net :8577, popover on click, waits while the onboarding/simulation backdrop is open), no change. (2) Phone notice (393): matches (:8577), no change. (3) AP fallback guard (394, beta-only): ch06/h06 6.1.3 said every runtime exhaustion goes to AP fallback; added: skipped when free < 8 KB (HEAP_LOW_THRESHOLD, helperStuff.ino:1128) or largest block < 4 KB (WIFI_STARVED_MAXBLOCK, networkStuff.ino:649), log line quoted from networkStuff.ino:1015, then release behaviour (retry, restart after 10). Only the runtime path is guarded (wifiApFallbackHeapOk at :1007); the boot path :431 is not, so the text says 'runtime'. (4) Dashed series (396): the manual did not mention dashed lines at all; ch03/h03 graph bullet now names control setpoint / thermostat setpoint / SAT target as dashed (graphSeriesDashed, 95f898778 v2.js:3669), the dashed legend swatch on screen and in the PNG from beta.396, and click-to-hide on a legend entry remembered in the browser (localStorage otgw-v2-goff, v2.js:3303). Gates: build_manual --check exit 0, test_manual_* 10 passed, no em dashes.

Push check on origin 43dd466b9 (beta.397): graphSeriesDashed v2.js:3669/:3679 as in 95f898778; AP-skip log line networkStuff.ino:1014, guard :1007; otgw-v2-goff v2.js:3303/:3685. d3's 7e31bc2bf (task 2924, beta.397) moved the H) help line into the 'h' menu (handleDebug.ino:479), so ch07/h07 H rows now say 'from beta.397 the h menu lists it'. Gates after the rebase: build_manual --check exit 0, test_manual_* 10 passed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: four items checked against origin 43dd466b9: update chip and phone notice (393) already matched v2.js updAsk :8577-8661; 6.1.3 EN/NL gained the beta.394 AP-fallback guard (networkStuff.ino:1007, log line :1014, thresholds helperStuff.ino:1128 / networkStuff.ino:649; runtime path only); the ch03/h03 graph bullet names the dashed series, the dashed legend swatch from beta.396 (v2.js:3669/:3679) and click-to-hide remembered in the browser (:3303/:3685); ch07/h07 H row notes the h-menu listing from beta.397 (handleDebug.ino:479). AC2: MANUAL.md rebuilt, build_manual --check exit 0, test_manual_* 10 passed. AC3: pushed after ba's 396 and d3's 397 were on origin, in the coordinator's slot. Not verified: the UI on a device (code reading only).
<!-- SECTION:FINAL_SUMMARY:END -->
