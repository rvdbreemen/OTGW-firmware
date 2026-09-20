---
id: TASK-1143
title: >-
  Investigate: temperatures and CH pressure shown without a decimal point on
  1.7.6-beta.3 (GH #684 follow-up)
status: To Do
assignee: []
created_date: '2026-09-20 11:35'
labels:
  - bug
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
- [ ] #1 The reporter has stated where the wrong values were read (web UI, HA or MQTT) and whether the beta.3 filesystem was flashed alongside the firmware
- [ ] #2 Either the defect reproduces on a bench 1.x device on beta.3 (or a local build) with a documented value path, or it is shown to be a stale filesystem or browser cache and recorded as such on the issue
- [ ] #3 If a firmware or web UI defect is found: fixed, python build.py --firmware exits 0, python evaluate.py --quick shows no new failures, and the fix ships in the next beta before beta.3 is promoted to a release
<!-- AC:END -->
