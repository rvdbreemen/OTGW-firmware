---
id: TASK-1143
title: >-
  Investigate: temperatures and CH pressure shown without a decimal point on
  1.7.6-beta.3 (GH #684 follow-up)
status: To Do
assignee:
  - '@claude'
created_date: '2026-09-20 11:35'
updated_date: '2026-09-28 20:13'
labels:
  - bug
  - web-ui
  - needs-info
dependencies: []
priority: high
ordinal: 224000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Appiejs on GitHub #684, 2026-09-19 21:48 UTC, as a side note after his original problem was resolved by reflashing the PIC (diagnose, then gateway 6.8).

On 1.7.6-beta.3 he saw outdoor temperature, return temperature and DHW temperature shown as hundreds (513 and 418) and Central Heating water pressure as 1008 bar. After going back to 1.7.5, on the same PIC 6.8, the same values were correct. Read literally the numbers look like 5.13, 41.8 and 1.008 with the decimal separator dropped, not a x100 scale error (the factors differ: x100, x10, x1000).

What is known from the repo: git diff v1.7.5..v1.7.6-beta.3 does not touch OTGW-Core.ino or the decode path; jsonStuff.ino changed only its version banner. The diff does carry 363 lines in data/index.js (PIC diagnose screen, TASK-1119 era), 52 in index.html, 44 in restAPI.ino and 44 in mqtt_configuratie.cpp. So if this is real, it is presentation or transport, not decoding.

Open questions the reporter must answer before anything is fixed: where did he read the values (web UI dashboard, Home Assistant, MQTT explorer)? Did he flash the beta.3 filesystem together with the firmware, or firmware only (a 1.7.5 index.js against beta.3 firmware, or a cached index.js, changes the picture entirely; the UI is loaded as index.js?v=<githash>, so a stale browser copy is possible after a flash)? A screenshot of one wrong value with the page it came from.

mrfox7688 and jaronbor run beta.3 for the MQTT fix (#682) and have not reported wrong values, which argues against a general MQTT-side regression but says nothing about the web UI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The reporter has stated where the wrong values were read (web UI, HA or MQTT) and whether the beta.3 filesystem was flashed alongside the firmware
- [ ] #2 Either the defect reproduces on a bench 1.x device on beta.3 (or a local build) with a documented value path, or it is shown to be a stale filesystem or browser cache and recorded as such on the issue
- [ ] #3 If a firmware or web UI defect is found: fixed, python build.py --firmware exits 0, python evaluate.py --quick shows no new failures, and the fix ships in the next beta before beta.3 is promoted to a release
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-28 investigation (AC#1 met: Appiejs answered on 2026-09-20: values seen in the web UI, after refresh and in a private window, and the beta.3 filesystem WAS flashed, so stale cache and an old filesystem are ruled out by the reporter).
- Code: sendJsonOTmonMapEntry(float) writes %.3f (jsonStuff.ino:365-368); refreshOTmonitor() puts entry.value into textContent unchanged on both the create and update path (index.js:4450-4454, 4479-4482); no locale formatting on this path. The display path is unchanged between v1.7.6-beta.3 and beta.7 (index.js +3 lines for port-25238 labels, jsonStuff.ino banner only). PS=1 summary parsing (publishPSSummaryFieldValue, OTGW-Core.ino:3628, strict float parse) exists since v1.3.5, so it cannot explain 1.7.5 vs beta.3.
- Bench (.88.68, beta.7, simulator with Toutside 5.13 / Tret 41.8 / Tdhw 50.25 / CHPressure 1.008 as f8.8): API 5.13 / 41.8 / 50.25 / 1.01; dashboard in headless Edge shows the same in both nl-NL and en-US locale. NOT reproduced.
- The reported numbers equal the JS number strings with the dot removed (5.13 -> 513, 41.8 -> 418, 1.008 -> 1008), which points at something between the JSON and the screen on the reporter's side (browser, translation or extension); unproven.
- Asked the reporter on GH #684 (issuecomment-5877663741) for a dashboard screenshot plus the raw /api/v2/otgw/otmonitor output at the same moment, and browser/device/extension details, on beta.7. Waiting on that evidence (needs-info).
<!-- SECTION:NOTES:END -->
