---
id: TASK-1214
title: >-
  Update the vendored espMqttClient to upstream 1.7.3+ (Outbox dangling-pointer
  and parser-desync fixes), keeping the ADR-186 patch
status: To Do
assignee: []
created_date: '2026-10-05 09:35'
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
- [ ] #1 src/libraries/espMqttClient is updated to the newest upstream release or main that includes PR #188, with the ADR-186 ClientSync::write() patch re-applied and marked
- [ ] #2 Bench: the TASK-1213 slow-broker test (500 ms netem) still shows a max loop gap under 1 s and all discovery configs present
- [ ] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
