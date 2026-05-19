---
id: TASK-631
title: 'fix(mqtt): surface silently-dropped set-commands in default debug stream'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-19 20:14'
updated_date: '2026-05-19 20:14'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
A 1.5.0 field report (outside-temperature MQTT override 'stopped working') exposed a diagnosability gap, not a functional regression. In handleMQTTcallback() a correctly-addressed set-command (<top>/set/<nodeid>/<cmd>) dropped because the PIC is flagged unavailable, or because the command name has no OTGW mapping, was only logged under MQTTDebug* (gated on state.debug.bMQTT). Users hit the silent-failure mode with no default-visible trace on telnet:23. Promote both genuine drop sites to the always-on debug stream and include the rejected command token so users can self-diagnose. Rebased onto current origin/dev (1.6.0-beta.x line) per maintainer request.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PIC-unavailable drop site in handleMQTTcallback() logs via always-on DebugTf (not gated on state.debug.bMQTT) and includes the rejected command token
- [x] #2 Unknown-command drop site (findMQTTSetCommandIndex miss) logs via always-on DebugTf and includes the rejected command token
- [x] #3 Broker-noise filter branches (wrong top topic / missing 'set' token) intentionally NOT promoted, so no default-log flood regression
- [x] #4 PROGMEM-safe: PSTR format strings, %s fed RAM char[] topicToken; no control-flow/drop-behaviour change
- [x] #5 Prerelease re-derived from the rebased base and bumped per versioning policy, cascade staged in the same commit
- [x] #6 python evaluate.py --quick shows no new failures vs baseline
- [ ] #7 python build.py --firmware exits 0
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the two drop sites on current origin/dev (1.6.0-beta.1) - confirmed byte-identical, lines 690-691 / 721-722
2. Replant branch on origin/dev; re-apply the 2-line MQTTstuff fix verbatim
3. Re-derive version bump from new base (1.6.0-beta.1 -> beta.2) per maintainer decision
4. evaluate.py --quick (no new failures)
5. build.py --firmware (gate)
6. Force-push working branch; PR #602 auto-updates
<!-- SECTION:PLAN:END -->
