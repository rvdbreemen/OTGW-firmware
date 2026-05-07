---
id: TASK-556
title: >-
  feat(mqtt): flip discovery topic shape to sibling-suffix (implements ADR-071,
  supersedes ADR-070 carve-out)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 11:04'
updated_date: '2026-05-07 11:27'
labels:
  - mqtt
  - discovery
  - ha-integration
  - bug
  - supersedes-adr-070
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implements ADR-071 (Proposed). Flips the discovery topic builder buildSensorDiscoveryTopic in mqtt_configuratie.cpp from nested children format (homeassistant/sensor/<id>/<label>/<src>/config) to sibling-suffix (homeassistant/sensor/<id>/<label>_<src>/config) for source-separated entities.

Why this is a real bug, not just an aesthetic change:
ADR-070 carved out the discovery topic from its sibling-suffix rule, claiming HA handles the nested format via subscription.async_prepare_subscribe_topics in-place delta logic. Empirical investigation against home-assistant/core dev branch (homeassistant/components/mqtt/discovery.py:63-66) proves ADR-070's claim was wrong: HA's TOPIC_MATCHER regex 'r"(?P<component>\\w+)/(?:(?P<node_id>[a-zA-Z0-9_-]+)/)?(?P<object_id>[a-zA-Z0-9_-]+)/config"' uses character class [a-zA-Z0-9_-]+ for object_id, which excludes the forward slash. When HA receives the nested topic homeassistant/sensor/<id>/TSet/thermostat/config, the regex fails to match, discovery.py:397-406 logs 'Received message on illegal discovery topic' and the message is silently discarded.

Field consequence on beta.21+/beta.22: every user with bSeparateSources=true currently sees the canonical entity register but the source-variant entities (TSet/thermostat, TSet/boiler, etc.) never appear in HA. The broker retains the rejected configs (no broker-side validation), creating misleading appearance that the topic shape is correct.

Coordinated with 2.0.0 sibling task (port + ADR-098 in 2.0.0 worktree).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-071 has been reviewed and approved by user; status flipped from Proposed to Accepted before implementation begins
- [x] #2 ADR-070 status line updated to 'Superseded by ADR-071, YYYY-MM-DD'; body of ADR-070 unchanged (immutability protocol)
- [x] #3 buildSensorDiscoveryTopic in mqtt_configuratie.cpp:2140-2146 changed: source-variant branch format string is '%s/sensor/%s/%s_%s/config' (sibling-suffix) instead of '%s/sensor/%s/%s/%s/config' (nested)
- [x] #4 Canonical-branch format string is unchanged ('%s/sensor/%s/%s/config') — no source attribution
- [x] #5 ADR-070 Enforcement carve-out comment in mqtt_configuratie.cpp (above line 2148) is removed or updated to reference ADR-071
- [x] #6 ADR-071 Enforcement block forbid_pattern matches the OLD nested format and would catch any regression
- [x] #7 python build.py --firmware exit 0 with no new warnings
- [x] #8 python evaluate.py --quick shows no new failures vs baseline
- [ ] #9 Field test on a beta unit with bSeparateSources=true confirms HA registers the source-variant entities (visible in HA Settings → Devices & Services → MQTT → entities list) where they did NOT register before the change
- [x] #10 docs/api/MQTT.md migration note updated: pre-ADR-071 retained nested discovery configs are zombies (HA never registered them) and may be cleaned with mosquitto_pub -t '<topic>' -r -n; included sample command for the nested paths
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read mqtt_configuratie.cpp:2132-2148 in full (buildSensorDiscoveryTopic) to confirm the exact format string and surrounding context
2. Edit the source-variant branch (line 2141-2142): change format string '%s/sensor/%s/%s/%s/config' to '%s/sensor/%s/%s_%s/config'. Use snprintf_P (PSTR) per project PROGMEM rule. Args order unchanged: haPrefix, nodeId, labelBuf, sourceTopicSegment.
3. Update the canonical-branch comment (no code change there) to reference ADR-071 alongside the existing ADR-070 reference, so the two-shape design (canonical bare; source-variant sibling-suffix) is documented at the call site.
4. Remove the ADR-070 carve-out comment elsewhere if it exists (search 'ADR-070' references in the discovery code). Replace with ADR-071 reference where appropriate.
5. Run python build.py --firmware → exit 0
6. Run python evaluate.py --quick → no new failures vs baseline
7. Verify the ADR-071 Enforcement block forbid_pattern actually catches the OLD format (run bin/adr-judge against a synthetic diff to confirm)
8. Commit on dev with feat(mqtt) prefix referencing TASK-556 and ADR-071
9. Auto-push to origin/dev (allowed per project policy: feature commit + build green + evaluator green)
10. Update docs/api/MQTT.md migration note: add the nested-discovery-zombie cleanup recipe (mosquitto_pub -t '<topic>' -r -n on the now-orphaned nested paths)
11. Mark ACs and add Final Summary; AC #9 (field test on beta unit) remains unchecked — hardware required
<!-- SECTION:PLAN:END -->
