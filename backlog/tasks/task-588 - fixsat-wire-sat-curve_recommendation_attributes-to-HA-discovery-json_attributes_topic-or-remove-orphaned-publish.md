---
id: TASK-588
title: >-
  fix(sat): wire sat/curve_recommendation_attributes to HA discovery
  json_attributes_topic or remove orphaned publish
status: Done
assignee: []
created_date: '2026-05-08 17:11'
updated_date: '2026-05-16 09:07'
labels:
  - sat
  - mqtt
  - ha-discovery
dependencies: []
references:
  - 'src/OTGW-firmware/SATcontrol.ino:2083-2090'
  - src/OTGW-firmware/MQTTHaDiscovery.cpp
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit found that `SATcontrol.ino:2083-2090` publishes a JSON blob on `sat/curve_recommendation_attributes` but no HA auto-discovery entry registers this topic as a `json_attributes_topic`. The data is published on every SAT cycle but consumed by nobody.

Three options:
1. Add a `json_attributes_topic` entry in `MQTTHaDiscovery.cpp` for the `sat/curve_recommendation` sensor so HA exposes the attributes.
2. Remove the publish entirely if the data is diagnostic-only and not needed in HA.
3. Convert individual fields to separate flat scalar topics (ADR-101 compliant).

The comment in the code references "Task #72" as the intention — check backlog for context.

JSON payload published:
```json
{"error_threshold": <float>, "daily_mean_error": <float>, "daily_sample_count": <uint>, "recent_mean_error": <float>}
```
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Either: a json_attributes_topic discovery entry exists in MQTTHaDiscovery.cpp pointing to sat/curve_recommendation_attributes AND the topic is retained/QoS-0 consistent with other attribute topics
- [ ] #2 Or: the publish at SATcontrol.ino:2083-2090 is removed and any related discovery config is also removed
- [ ] #3 Or: each field is published as a separate flat scalar topic per ADR-101 and the JSON publish is replaced
- [ ] #4 No orphaned MQTT publish remains (a topic published to but never subscribed via discovery)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Resolved on dev by TASK-611: all SAT source (SATcontrol.ino et al.) removed from the dev branch, so the sat/curve_recommendation_attributes orphan no longer exists on dev. SAT remains a 2.0.0 feature; discovery for SAT topics on the 2.0.0 line is tracked separately (TASK-543).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as moot-on-dev. TASK-611 removed the entire SAT subsystem from the dev branch (5 SAT*.ino files + gated header blocks + the live climate json_attributes_topic). The orphaned sat/curve_recommendation_attributes publish this task described lived in SATcontrol.ino, which no longer exists on dev. No dev code change required here; the SAT feature and its discovery wiring remain on feature-dev-2.0.0.
<!-- SECTION:FINAL_SUMMARY:END -->
