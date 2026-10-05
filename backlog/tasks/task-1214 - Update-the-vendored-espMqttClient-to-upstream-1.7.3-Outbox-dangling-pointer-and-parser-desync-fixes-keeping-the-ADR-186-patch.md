---
id: TASK-1214
title: >-
  Update the vendored espMqttClient to upstream 1.7.3+ (Outbox dangling-pointer
  and parser-desync fixes), keeping the ADR-186 patch
status: Done
assignee:
  - '@claude'
created_date: '2026-10-05 09:35'
updated_date: '2026-10-05 12:38'
labels:
  - mqtt
  - dependency
dependencies: []
priority: medium
ordinal: 334000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while preparing the upstream PR for the ADR-186 patch (2026-10-05). The vendored copy in src/libraries/espMqttClient is 1.7.2. Upstream has since shipped:
- v1.7.3 'Fix memory issue' (2026-06-22): PR #188, fix(Outbox), which re-anchors it._prev after _remove() to prevent a dangling pointer.
- On main after 1.7.3: 'Fix parser desync when PUBLISH topic exceeds EMC_MAX_TOPIC_LEN'.
The dangling-pointer fix may matter for heap corruption reports; the parser fix for long incoming topics.
Our non-blocking ClientSync::write() is offered upstream as https://github.com/bertmelis/espMqttClient/pull/191. If it is merged and released, the vendored copy can return to lib_deps (ADR-186 would then be superseded).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 src/libraries/espMqttClient is updated to the newest upstream release or main that includes PR #188, with the ADR-186 ClientSync::write() patch re-applied and marked
- [x] #2 Bench: the TASK-1213 slow-broker test (500 ms netem) still shows a max loop gap under 1 s and all discovery configs present
- [x] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-05: src/libraries/espMqttClient replaced by upstream main de31cb2 (library.json 1.7.3; includes PR #188, the Outbox dangling-pointer fix, plus the parser-desync fix for topics over EMC_MAX_TOPIC_LEN, NetworkClient support for Arduino-ESP32 3.x and an AsyncTCP close() compatibility fix). The ADR-186 ClientSync::write() patch is re-applied (MSG_DONTWAIT, marked with ADR-186 and upstream PR #191). Version policy: ADR-187 (accepted). Bench alpha.416+9f64919, OTGW32 combo, 500 ms netem broker delay, reboot then a discovery republish: max loop gap 124 ms, 0 gaps over 200 ms, hd_min_free_heap 16880, 398 retained configs, MQTT connected. Builds: esp32, esp32-classic and esp32-combo SUCCESS (alpha.416); evaluate.py --quick 71/0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The vendored espMqttClient now follows upstream main de31cb2 (1.7.3) instead of 1.7.2. This brings in the Outbox dangling-pointer fix (PR #188), the parser-desync fix for long topics and Arduino-ESP32 3.x NetworkClient support. The ADR-186 non-blocking ClientSync::write() patch is re-applied, and it is offered upstream as PR #191. ADR-187 replaces ADR-186's '1.7.2 sources' wording with an upstream-tracking policy. Evidence: bench slow-broker test, max loop gap 124 ms, 0 gaps over 200 ms, all 398 configs, MQTT connected. Three builds green, evaluate clean.
<!-- SECTION:FINAL_SUMMARY:END -->
