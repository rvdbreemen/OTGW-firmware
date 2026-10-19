---
id: TASK-1256
title: >-
  MANUAL.md: SAT command topic names and source-separated topic suffixes do not
  match the code
status: Done
assignee:
  - '@claude'
created_date: '2026-10-19 17:32'
updated_date: '2026-10-19 17:54'
labels:
  - docs
  - mqtt
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 388000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter 2026-10-19, verified in code. SAT command table (docs/MANUAL.md ~3161-3165) lists sat/externaltemp, sat/externaloutdoor, sat/enable, sat/mode; kSatMqttCmds[] (MQTTstuff.ino ~658-665) accepts sat/indoor_temp, sat/outdoor_temp, sat/enabled, sat/control_mode (MANUAL ~863-866 already has them right). Source-separated topics (MANUAL ~495, ~3062-3064) say <label>/thermostat, /boiler, /gateway; publishToSourceTopic() publishes <label>_thermostat and <label>_boiler, no gateway variant.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MANUAL.md SAT command table matches kSatMqttCmds[]
- [x] #2 MANUAL.md source-separated topic text matches publishToSourceTopic() (underscore suffix, no gateway)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (docs only, uncommitted). docs/MANUAL.md: SAT Command Topics table rewritten from kSatMqttCmds[] (MQTTstuff.ino:655-743): 15 handler commands with payloads read from the handlers, the area/zone sub-token commands, and a 64-row passthrough table (command -> setting key), generated from the code. Removed sat/enable, sat/mode, sat/externaltemp, sat/externaloutdoor. Source-separated text (line 495 + section) now <label>_thermostat / <label>_boiler, no gateway variant (publishToSourceTopic, MQTTstuff.ino:1923-1988). Same wrong names fixed in docs/manuals/en/ch03 (/thermostat,/boiler,/gateway), en/ch09 and nl/h09 (_gateway and _answer topics that do not exist). Also MANUAL line ~863: control_mode 'off' removed, because satHandleControlMode maps 'off' (atoi==0) to auto. Verification: scratchpad check1256.py parses the MANUAL table vs kSatMqttCmds: HEAD = 4 documented-not-in-code + 75 missing; fix = 0/0, 64 setting keys match. After-grep leaves only docs/api/MQTT.md:337 (sat/mode PUBLISH topic, real: SATcontrol.ino:2435) and MQTT.md:712/714 (history of the old nested shape, intended). evaluate.py --quick exit 0. Not changed, flagged: control_mode 'off' maps to auto (code quirk); ch04/h04 still list 'off'; ch05/h05 list ovp_* MQTT commands that are not in kSatMqttCmds; MQTT.md routing table says A never reaches _boiler but proxy A does (ADR-103).

Review round: fixed docs/api/MQTT.md:695 A row (proxy A reaches _boiler and canonical, answer-override A does not; MQTTstuff.ino:1971 toBoiler=!bAnswerOverride, OTGW-Core.ino:1816 canonical gate), so the MANUAL pointer to MQTT.md no longer contradicts ch09/h09. Dropped 'off' from sat/control_mode in en/ch04:385 and nl/h04:399 (same note as MANUAL:863). ch05/h05 ovp_* (feature removed in TASK-891.6, no ovp symbol in src) and ch09/h09 SAT table drift filed as TASK-1260. Re-check: check1256.py 0 documented-not-in-code / 0 missing, 64 passthrough keys match; evaluate.py --quick exit 0. Remaining greps for sat/mode and /api/v2/sat/externaltemp|externaloutdoor|enable are real (published state topic SATcontrol.ino:2435, REST restAPI.ino:1295/1310/1415). Stale comment OTGW-Core.ino:1795-1796 ('Suppress canonical for A') predates ADR-103; code-comment only, not changed in this docs task.

Finalize run 2026-10-19: fresh re-run of scratchpad check1256.py against the working MANUAL.md: 82 documented rows vs 79 kSatMqttCmds entries (+3 special-case area/zone rows), documented-not-in-code [], in-code-not-documented [], duplicates [], 64 passthrough setting keys match code. python evaluate.py --quick exit 0: Passed 72, Warnings 0, Failed 0, Health 100.0%. Docs-only: no build, no version bump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Docs-only fix: MANUAL.md SAT command topics and source-separated topic suffixes now match the code. AC1 evidence: check1256.py parses kSatMqttCmds[] (MQTTstuff.ino) and the MANUAL SAT Command Topics section; HEAD shows 4 documented-not-in-code (enable, mode, externaltemp, externaloutdoor) and 75 missing; the fix shows 0/0, no duplicates, 64 passthrough setting keys matching. AC2 evidence: grep for {label}/thermostat|boiler|gateway, _gateway, _answer across MANUAL.md, docs/manuals, docs/api, docs/guides, docs/c4, docs/features: before = 8 MANUAL lines + ch03/ch09/h09 hits; after only docs/api/MQTT.md:337 (real sat/mode state topic, SATcontrol.ino:2435) and 712/714 (documented history of the old nested shape). Text now says <label>_thermostat / <label>_boiler, no gateway variant, per publishToSourceTopic(). Review fixes in the same change: MQTT.md routing table A row (proxy A reaches _boiler/canonical, answer-override A does not; MQTTstuff.ino:1971, OTGW-Core.ino:1816) and control_mode 'off' dropped from en/ch04 and nl/h04 (maps to auto). Out of scope, filed as TASK-1260: ovp_* commands in ch05/h05 and ch09/h09 SAT table drift. evaluate.py --quick exit 0 (72 passed, 0 warn, 0 fail). No build or bump: no firmware path touched.
<!-- SECTION:FINAL_SUMMARY:END -->
