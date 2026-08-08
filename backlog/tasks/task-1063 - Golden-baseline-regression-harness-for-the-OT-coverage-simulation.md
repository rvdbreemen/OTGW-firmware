---
id: TASK-1063
title: Golden-baseline regression harness for the OT coverage simulation
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 10:43'
updated_date: '2026-08-08 10:54'
labels:
  - test
  - tooling
dependencies: []
priority: medium
ordinal: 173000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The coverage fixture (TASK-1062) proves breadth but produces no reusable verdict: each run has to be eyeballed. Turn it into a regression gate. A run is reduced to a normalized fingerprint that is stable across runs and devices (timestamps, heap columns, uptime, RSSI, MAC/uniqueid, broker host and free-heap counters all stripped), capturing what actually matters: for every (prefix, msgtype, msgid) the decoded label and rendered value, the set of MQTT topics published with their payloads, and the message types and source prefixes exercised. That fingerprint is committed as the baseline, taken from v1.7.3-beta.3+5f852a0 which was validated on hardware. Future runs compare against it and any decode or publish drift shows up as a readable diff with a non-zero exit code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A tool reduces a telnet capture of the fixture run to a normalized JSON fingerprint, deterministic across repeated runs of the same firmware
- [x] #2 The fingerprint excludes all volatile fields: timestamps, heap and max-block, uptime, RSSI, device uniqueid/MAC, broker host
- [x] #3 A committed baseline JSON is generated from the validated v1.7.3-beta.3 capture
- [x] #4 Compare mode prints a readable diff of added/removed/changed entries and exits non-zero on any drift
- [x] #5 Re-running compare on the same capture reports zero drift (idempotence proven, not assumed)
- [x] #6 Compare on a genuinely different capture is shown to detect drift, so the gate is proven to have teeth
- [x] #7 scripts/tests/README.md documents the baseline workflow and how to refresh the baseline deliberately
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds coverage_baseline.py plus a committed baseline_coverage.json taken from v1.7.3-beta.3+5f852a0. A capture is reduced to a normalized fingerprint (per-frame decoded label and value, per-topic payload sets, message types, source prefixes) with everything volatile stripped: timestamps, heap, uptime, MAC, broker host, and non-ASCII unit suffixes that would otherwise fingerprint differently depending on log decoding. Stability proven by selftest (0 unstable keys across 370, comparing two halves of one capture) rather than assumed; teeth proven by comparing a different firmware's capture (764 differences, exit 1) against the reference (exit 0). Its first real use found TASK-1064.
<!-- SECTION:FINAL_SUMMARY:END -->
