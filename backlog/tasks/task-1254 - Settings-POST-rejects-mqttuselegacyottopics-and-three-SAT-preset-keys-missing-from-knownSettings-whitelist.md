---
id: TASK-1254
title: >-
  Settings POST rejects mqttuselegacyottopics and three SAT preset keys (missing
  from knownSettings whitelist)
status: To Do
assignee: []
created_date: '2026-10-19 17:31'
labels:
  - bug
  - webui
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 386000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter in #alpha-testing 2026-10-19 (alpha.436, Classic PIC): toggling 'Legacy OT topic layout' and saving gives 'Save failed' and the change stays unsaved. Cause (read in code): knownSettings[] in restAPI.ino (~4177-4231) lacks mqttuselegacyottopics, satpresetsleep, satpresetactivity, satpresethome, so postSettings() returns 400 Unknown setting name although GET exposes them and updateSetting handles them. Also: the 'Save failed' toast uses the green .toast style (no error variant), and one rejected key aborts the rest of the sequential save chain.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The four keys are accepted by POST /api/v2/settings and persist in settings.ini across a reboot (curl evidence)
- [ ] #2 A failed save shows a red error toast, success stays green (screenshot)
- [ ] #3 A test fails when a GET /api/v2/settings key that the UI can edit is missing from knownSettings[] (red on old code, green on fix)
<!-- AC:END -->
