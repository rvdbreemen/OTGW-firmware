---
id: TASK-1014
title: >-
  fix(v2-webui): SAT onboarding save fails - serialize settings POSTs to dodge
  REST backpressure 503
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-05 21:04'
updated_date: '2026-07-05 21:05'
labels: []
dependencies: []
ordinal: 225000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT onboarding wizard's final Enable step showed 'Could not save the settings, SAT was not enabled'. commit() fired 6-7 settings POSTs concurrently via Promise.all; the REST backpressure gate (restAPI.ino:2455, TASK-884) caps concurrent /api requests at 4 (2 under <24KB maxblock, 1 under <16KB), so the excess got a cheap 503, Promise.all rejected, and the commit failed - sometimes after satenabled was the dropped write, leaving a partial save. Fix: land settings one at a time via a sequential promise chain. Same pattern applied to the MQTT onboarding commit, testConn, and the weather lat/lon detect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT onboarding commit lands settings sequentially (no Promise.all over POSTs)
- [x] #2 Never more than one settings POST in flight at once
- [x] #3 MQTT onboarding commit, testConn, and weather-detect also serialized
- [ ] #4 Prerelease bumped and LittleFS filesystem rebuilt
- [ ] #5 On-device: complete SAT onboarding, verify no 503 and SAT enabled
<!-- AC:END -->
