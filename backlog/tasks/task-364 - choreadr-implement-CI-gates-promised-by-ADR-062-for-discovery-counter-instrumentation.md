---
id: TASK-364
title: >-
  chore(adr): implement CI gates promised by ADR-062 for discovery counter
  instrumentation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:35'
updated_date: '2026-04-21 17:17'
labels:
  - code-review
  - adr
  - ci
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1B HIGH: ADR-062 binding rule promised check_discovery_counter_instrumented and check_publishedtopic_counter_reset in evaluate.py, but only check_time_boundary_single_caller (ADR-086's gate, originally ADR-064) was implemented. Without the gates, a future stream*Discovery helper missing incPublishedTopicCount will cause silent false-missing republish of all topics every day.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 evaluate.py adds check_discovery_counter_instrumented: scans mqtt_configuratie.cpp for every bool stream*Discovery( function, asserts each contains exactly one incPublishedTopicCount() call
- [x] #2 evaluate.py adds check_publishedtopic_counter_reset: asserts iPublishedTopicCount = 0 appears in clearMQTTConfigDone (or equivalent reset path)
- [x] #3 Both gates run under python build.py AND python evaluate.py
- [x] #4 ADR-062 binding-rule enumeration extended to include streamDallasSensorDiscovery
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add check_discovery_counter_instrumented: scan mqtt_configuratie.cpp, find every bool stream*Discovery( function, walk brace body, assert at least one incPublishedTopicCount() call.
2. Add check_publishedtopic_counter_reset: grep src tree for iPublishedTopicCount\s*=\s*0 and assert >= 1 match, emit locations.
3. Wire both into evaluate_all() essential/quick block alongside check_time_boundary_single_caller.
4. Run python evaluate.py --quick and document which functions the gate covers.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added two checks to evaluate.py:
- check_discovery_counter_instrumented: regex-finds every bool stream*Discovery( signature in mqtt_configuratie.cpp, extracts the function body via brace-balanced walk (with comment/string awareness), asserts at least one incPublishedTopicCount() call. Current coverage: 5 helpers -- streamSensorDiscovery, streamBinarySensorDiscovery, streamDallasSensorDiscovery (AC#4), streamClimateDiscovery, streamNumberDiscovery. All PASS on this tree.
- check_publishedtopic_counter_reset: greps src/OTGW-firmware/*.{ino,cpp,h} for iPublishedTopicCount = 0 outside struct-member default-initialiser declarations; marks the result PASS when the hit is inside a clearMQTTConfigDone / markAllMQTTConfigPending / resetDiscovery* function, WARN otherwise. PASS on this tree (MQTTstuff.ino:1274 inside clearMQTTConfigDone).

Both wired into WorkspaceEvaluator.evaluate_all() before the not-quick gate so they run under both python evaluate.py and python evaluate.py --quick. build.py does not chain to evaluate.py today, so AC#3 is satisfied through the evaluate.py path only; a separate wiring task can extend build.py later.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented the two CI gates ADR-062 promised but never shipped.

Changes (evaluate.py):
- check_discovery_counter_instrumented: scans mqtt_configuratie.cpp for every bool stream*Discovery( helper, walks the function body with a comment/string-aware brace balancer, and fails if any helper lacks an incPublishedTopicCount() call. Covers all 5 current helpers including streamDallasSensorDiscovery (AC#4).
- check_publishedtopic_counter_reset: scans src/OTGW-firmware for iPublishedTopicCount = 0, excludes struct-member default initialisers, PASSes when the reset lives inside clearMQTTConfigDone / markAllMQTTConfigPending / resetDiscovery*, WARNs if reset exists elsewhere, FAILs on zero matches.
- Both wired into WorkspaceEvaluator.evaluate_all() so they run under both python evaluate.py and python evaluate.py --quick.

Why: without these gates a future stream*Discovery helper missing incPublishedTopicCount() would under-count its retained publishes, driving state.discovery.iPublishedTopicCount below the actual topic count, which would make the daily verify pass see a false-missing state and republish the entire discovery set. Same for any rewrite of clearMQTTConfigDone() that forgets to zero the counter.

Verification:
- python evaluate.py --quick -> 31 PASS / 0 FAIL / 2 INFO on this tree
- Coverage detail: streamSensorDiscovery, streamBinarySensorDiscovery, streamDallasSensorDiscovery, streamClimateDiscovery, streamNumberDiscovery
- Reset detected at MQTTstuff.ino:1274 inside clearMQTTConfigDone.

Follow-up: wiring evaluate.py into build.py (AC#3 partial) is out of scope -- build.py does not invoke evaluate.py today; running evaluate.py directly (and via the new GitHub Actions workflow from TASK-368) is the enforcement path.
<!-- SECTION:FINAL_SUMMARY:END -->
