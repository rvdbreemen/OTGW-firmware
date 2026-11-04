---
id: TASK-389
title: 'Create ADR-065: otgw-pic/ MQTT subtree is stable public topic API'
status: Done
assignee:
  - '@rvdbreemen'
created_date: '2026-04-23 17:57'
updated_date: '2026-04-23 18:14'
labels:
  - architecture
  - adr
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The otgw-pic/ MQTT subtree exists since v1.3.0 and is used by end users in HA configs, NodeRED flows, Prometheus exporters and custom dashboards. The string otgw-pic/ is currently hardcoded on 26 places in the firmware (24 publish call-sites + 2 discovery-code locations). There is no explicit architectural record that this is a stable public contract.

TASK-388 introduces kPicSubtreePrefix as a central constant and activates the MQTT_HA_FLAG_IS_PIC_ENTRY flag semantics in discovery generators. This ADR records the rationale and constraints around the subtree: why it must remain stable, how the flag contract works, and what a future migration strategy would look like if the subtree ever needs to change.

Since the CLAUDE.md rules mandate that Accepted status can only be set after explicit developer approval, this task covers: authoring the ADR in Proposed status, presenting for review, iterating on feedback, and moving to Accepted after approval. The ADR supersedes any implicit contract that existed previously.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/ADR-065-otgw-pic-mqtt-subtree.md exists with correct ADR template (Status, Context, Decision, Consequences, Related)
- [x] #2 Status field is set to Proposed at first save
- [x] #3 Content lists all publish call-sites of otgw-pic/ with file:line references (MQTTstuff.ino and OTGW-Core.ino)
- [x] #4 Content describes how kPicSubtreePrefix and MQTT_HA_FLAG_IS_PIC_ENTRY work together to produce discovery stat_t matching the publish path
- [x] #5 Content explains why the subtree is stable public API (3+ years installed base since v1.3.0, external tooling dependency)
- [x] #6 Content documents a migration strategy for any future subtree rename/split (dual-publish with deprecation period of at least 2 minor releases)
- [x] #7 Related section references TASK-388, ADR-004 (no String class), and v1.3.0 release where subtree was introduced
- [x] #8 After developer review and explicit approval, Status is updated to Accepted (never self-approved)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-065 authored as part of TASK-388 implementation work (the code-side comments referenced ADR-065, which created dangling ADR references flagged by evaluate.py -- writing the ADR now was the natural path).

docs/adr/ADR-065-otgw-pic-mqtt-subtree.md created 2026-04-23 with Status: Proposed. Contents cover all required sections: Status, Context (with full history of the v1.3.x -> v1.4.x regression), Decision (declaring the subtree public API + single source of truth + flag-driven discovery contract), Consequences (benefits, trade-offs, migration strategy with 30-day announcement + dual-publish + 2-minor-release deprecation window), Call-site inventory (informative reference of the 24 publish sites + 2 discovery sites + 2 flagged table entries), Related section linking TASK-388, TASK-389, TASK-390, ADR-004, v1.3.0 release, and the mqttha.cfg archive.

ACs 1-7 self-verified complete.
AC 8 (Status -> Accepted) requires developer review per CLAUDE.md ADR workflow: cannot be self-approved. After review: `backlog task edit 389 --check-ac 8` + change ADR-065 Status field from Proposed to Accepted.

2026-04-23: ADR-065 Status moved from Proposed to Accepted by developer. All 8 ACs now satisfied.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Authored docs/adr/ADR-065-otgw-pic-mqtt-subtree.md documenting that the otgw-pic/ MQTT subtree is a stable public topic API. ADR is in Proposed status awaiting developer review.

The content covers the full history of the regression that TASK-388 fixed (mqttha.cfg takeover at commits bc9bd6a2/3e1872ce introduced the IS_PIC_ENTRY flag but never read it in the discovery generators), declares the single-source-of-truth contract (kPicSubtreePrefix + flag-driven discovery), and documents a deliberately-heavy migration strategy for any future subtree change (30-day announcement + dual-publish + 2-minor-release deprecation window + retained-topic cleanup instructions).

AC 8 -- move Status from Proposed to Accepted -- is the only remaining AC and requires explicit developer approval per CLAUDE.md rules (ADRs are never self-approved). After approval: edit docs/adr/ADR-065-otgw-pic-mqtt-subtree.md line 5 from "Proposed" to "Accepted" and run `backlog task edit 389 --check-ac 8 -s Done`.

No code changes (ADR is pure documentation).
<!-- SECTION:FINAL_SUMMARY:END -->
