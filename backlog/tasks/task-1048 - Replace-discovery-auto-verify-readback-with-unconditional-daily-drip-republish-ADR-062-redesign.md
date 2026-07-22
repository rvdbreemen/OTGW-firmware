---
id: TASK-1048
title: >-
  Replace discovery auto-verify readback with unconditional daily drip republish
  (ADR-062 redesign)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-22 17:59'
updated_date: '2026-07-22 18:11'
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
- [x] #1 Discovery verify wildcard-subscribe + count + markAllPending-on-partial-read path removed/bypassed
- [x] #2 Daily (and first-run) trigger performs unconditional drip republish instead of verify readback
- [x] #3 No hourly retry-storm: trigger runs at most daily, heap-gated
- [x] #4 Superseding ADR authored (Proposed) for ADR-062 change
- [x] #5 Version bumped to 1.7.2-beta.3
- [x] #6 python build.py firmware+fs exit 0, evaluate --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Option 5 implemented (KISS): removed automatic startDiscoveryVerification() from both hourly-first-run and daily triggers in OTGW-firmware.ino. Daily trigger now does unconditional markAllMQTTConfigPending() drip republish, guarded by MQTT-connected + no-drip-in-progress + maxFreeBlock>=8000. Deleted hourly retry block. Added markAllMQTTConfigPending forward-decl to OTGW-firmware.h (concat-order fix). Manual verify paths (POST /api/v2/discovery, telnet debug) left intact.

Authored ADR-087 (Proposed) superseding ADR-062. ADR-062 NOT yet flipped to Superseded (awaits user acceptance of ADR-087; never self-approve).

Build: build.bat exit 0, fresh beta.3 bins (ino 764688B + littlefs 2072576B, 20:09). evaluate --quick 97.3%, single failure (unresolved ADR ref) confirmed pre-existing via stash test — not new.

NOT committed: pre-commit adr-judge (ADR-062 llm_judge:true) will block until ADR-087 accepted + ADR-062 status flipped.
<!-- SECTION:NOTES:END -->
