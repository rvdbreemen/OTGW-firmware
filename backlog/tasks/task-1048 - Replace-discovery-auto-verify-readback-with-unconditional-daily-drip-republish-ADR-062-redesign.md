---
id: TASK-1048
title: >-
  Replace discovery auto-verify readback with unconditional daily drip republish
  (ADR-062 redesign)
status: To Do
assignee: []
created_date: '2026-07-22 17:59'
labels:
  - bug
  - mqtt
  - heap
dependencies: []
priority: high
ordinal: 167000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field root cause (transcript OTGW-48E72958B013): discovery auto-verify subscribes homeassistant/+/dev/# to count retained configs, gets partial readback (26/124) due to small PubSubClient buffer, falsely declares 98 missing, triggers markAllMQTTConfigPending full republish, retries hourly (epoch never set) -> heap leak -> death at 82min. Option 5 (KISS): drop the wildcard readback+count+false-missing path; keep an unconditional drip-paced daily republish as the retained-config auto-heal. Removes the runaway feedback loop entirely. Ship as 1.7.2-beta.3 for field A/B validation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Discovery verify wildcard-subscribe + count + markAllPending-on-partial-read path removed/bypassed
- [ ] #2 Daily (and first-run) trigger performs unconditional drip republish instead of verify readback
- [ ] #3 No hourly retry-storm: trigger runs at most daily, heap-gated
- [ ] #4 Superseding ADR authored (Proposed) for ADR-062 change
- [ ] #5 Version bumped to 1.7.2-beta.3
- [ ] #6 python build.py firmware+fs exit 0, evaluate --quick no new failures
<!-- AC:END -->
