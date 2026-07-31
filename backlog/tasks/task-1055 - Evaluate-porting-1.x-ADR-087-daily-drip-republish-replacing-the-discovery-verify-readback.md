---
id: TASK-1055
title: >-
  Evaluate porting 1.x ADR-087: daily drip republish replacing the
  discovery-verify readback
status: Done
assignee: []
created_date: '2026-07-31 19:52'
updated_date: '2026-07-31 20:27'
labels: []
dependencies: []
ordinal: 250000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PLAN-GATED, DO NOT IMPLEMENT WITHOUT USER SIGN-OFF. otgw-1.x.x commit 393db8b39 / ADR-087 replaced the retained-discovery auto-verify readback (1.x ADR-062) with an unconditional daily drip republish. dev still runs the full verify state machine: mqtt_discovery_verify.h/.cpp is a separate translation unit (TASK-363) with callers at MQTTstuff.ino:783, :1035 and :1079, plus dev ADR-062. This is an architectural change, not a cherry-pick, and dev ADR numbering is independent of 1.x. Scope: assess whether dev should follow, and if so author a dev ADR (>= 170) SUPERSEDING dev ADR-062 and citing 1.x ADR-087, then remove or repurpose the verify TU. Per CLAUDE.md auto-advance policy this is an explicit plan-approval checkpoint and must not be drained autonomously.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Written assessment of whether dev should follow 1.x here, with the MQTT-broker and HA-side consequences spelled out
- [ ] #2 Assessment shared with the maintainer and explicitly approved before any code change
- [ ] #3 If approved: dev ADR authored via adr-kit, superseding dev ADR-062 and citing 1.x ADR-087
- [ ] #4 If approved: verify state machine removed or repurposed, all three MQTTstuff.ino call sites resolved
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SUPERSEDED BY TASK-1037, not implemented separately. Created 2026-07-31 from an analysis of a local dev tree that was 6 commits behind origin/dev. origin/dev already carried 24be052f2 'feat(2.0.0): port 1.7.2-beta.4 hardening', which ported this change as ADR-170, adapted to the 2.0.0 architecture and with evaluate.py gates plus unit tests. Verified against the merged tree, not against the task description. No work remains here.
<!-- SECTION:NOTES:END -->
