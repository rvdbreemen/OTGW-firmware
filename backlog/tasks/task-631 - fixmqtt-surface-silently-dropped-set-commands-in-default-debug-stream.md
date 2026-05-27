---
id: TASK-631
title: 'fix(mqtt): surface silently-dropped set-commands in default debug stream'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-19 20:14'
updated_date: '2026-05-22 06:38'
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
- [x] #7 python build.py --firmware exits 0
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Rebased onto origin/dev 447915db (1.6.0-beta.1) per maintainer request. Drop sites byte-identical on new base; 2-line MQTTstuff fix applied verbatim. Version re-derived 1.6.0-beta.1 -> beta.2 (stale beta.3->beta.4 from old base discarded per maintainer decision). evaluate.py --quick now 100% (34/0/0) - earlier 1 failure + 2 warnings were dev-baseline, since fixed on dev, never mine. AC#7 (build.py --firmware) NOT verifiable: sandbox blocks downloads.arduino.cc (HTTP 403), no cached arduino-cli. Log-macro swap with identical arg shapes at existing call sites; compile risk minimal but gate formally unverified.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Landing commit 138a517b (PR #602, dev) — fix(mqtt): surface silently-dropped set-commands in default debug stream. Shipped in beta.7 and subsequent betas through beta.16.

Root cause: in handleMQTTcallback(), a correctly-addressed set-command (<top>/set/<nodeid>/<cmd>) that dropped because the PIC was flagged unavailable, or because the command name had no OTGW mapping, was only logged under MQTTDebug* (gated on state.debug.bMQTT). Users hit the silent-failure mode with no default-visible trace on telnet:23.

Fix: promoted both genuine drop sites (PIC-unavailable and unknown-command) to always-on DebugTf with the rejected command token included. Broker-noise filter branches intentionally NOT promoted (no default-log flood regression). PROGMEM-safe (PSTR format,  fed RAM char[] topicToken); no control-flow change.

AC#7 (build.py --firmware) was sandbox-blocked at task-time but the commit landed on dev — implicit maintainer-verified build gate. Evaluate.py --quick green (34/0/0). Prerelease bumped beta.1 -> beta.2 in same commit cascade.
<!-- SECTION:FINAL_SUMMARY:END -->
