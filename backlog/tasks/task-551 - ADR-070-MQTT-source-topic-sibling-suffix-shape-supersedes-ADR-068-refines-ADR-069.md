---
id: TASK-551
title: >-
  ADR-070: MQTT source-topic sibling-suffix shape (supersedes ADR-068, refines
  ADR-069)
status: Done
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-07 07:55'
updated_date: '2026-05-07 08:23'
labels:
  - feat-mqtt-suffix-shape
  - adr
  - mqtt
  - ha-discovery
  - dev-1.5x
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Beta testers report perception that HA does not follow nested per-source MQTT topics shipped under ADR-069 (commit cbc21af6, 2026-05-07).

> "I don't think this nested topology is working. Are you sure HA checks the nested configs?"
> "I haven't noticed any difference between that option enabled and disabled." — Andre, 2026-05-07

Verified against HA source (homeassistant/components/mqtt/sensor.py, subscription.py, util.py) and the HA MQTT integration docs that the nested shape is technically valid: HA subscribes to whatever literal string is in state_topic with no recursion or wildcards. So HA does not "fail to follow" nested topics — but the structural pattern (parent topic carrying payload AND having children) is unconventional, breaks topic-browser tools, and creates user doubt.

This ADR codifies the decision to:
1. Use sibling-suffix shape (<id>_<view>) instead of nested children (<id>/<view>).
2. Supersede ADR-068's mutual-exclusion rule (drop it — siblings make the canonical entity additive, not duplicate).
3. Refine ADR-069 (worldview routing semantics retained; only topic shape changes).

Includes Enforcement block with declarative forbid_pattern to prevent regression to slash-nested PSTR literals in the publish path.

This task covers ONLY the ADR authorship. Implementation is the sibling Task B (will be created with this same label).

Related: ADR-069, ADR-068, ADR-067 (boot-time discovery republish — the trigger that delivers the new shape to HA without manual intervention). 2.0.0 mirror is ADR-097 (sibling Task C).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/ADR-070-mqtt-source-topic-sibling-suffix-shape.md exists with Status: Proposed
- [x] #2 ADR cites Andre's beta feedback (2026-05-07) and the HA source verification (mqtt/sensor.py, mqtt/subscription.py) in Context
- [x] #3 Decision section states sibling-suffix shape (<id>_<view>), drops ADR-068 mutual exclusion, preserves ADR-069 routing semantics
- [x] #4 At least 3 alternatives documented with rejection reasons (keep nested + better docs; sibling but keep mutual exclusion; separate top-level prefix per view; non-underscore separator)
- [x] #5 Consequences cover positive (clean leaves; stable canonical for dashboards) and negative (orphan retained values at old topics; ~3 entities per dual-source MsgID under bSeparateSources=true)
- [x] #6 Related section names: Supersedes ADR-068; Refines ADR-069; Preserves ADR-065, ADR-066, ADR-067; cross-references 2.0.0 mirror ADR-097
- [x] #7 Enforcement block (JSON) with forbid_pattern for PSTR("%s/(thermostat|boiler)") literals scoped to src/OTGW-firmware/MQTTstuff.ino; mqtt_configuratie.cpp explicitly excluded since buildSensorDiscoveryTopic legitimately uses slash there for HA discovery topic identifiers
- [x] #8 All four ADR-kit verification gates pass (/adr-kit:lint clean)
- [x] #9 Status flipped to Accepted, YYYY-MM-DD ONLY after explicit human approval — never self-approved (CLAUDE.md ADR workflow rule)
- [x] #10 ADR-068 status line edited to 'Superseded by ADR-070, YYYY-MM-DD.' (one line only; rest of ADR-068 immutable)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read ADR-068, ADR-069 for cross-reference content and supersession chain.
2. Author docs/adr/ADR-070-mqtt-source-topic-sibling-suffix-shape.md as Status: Proposed using the canonical body from the plan file.
3. Verify all four ADR-kit gates pass (Completeness, Evidence, Clarity, Consistency).
4. STOP — present ADR for human review (CLAUDE.md ADR workflow rule: never self-approve).
5. After human approval, flip Status to Accepted, edit ADR-068 status line to Superseded by ADR-070, mark ACs done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-07: ADR-070 authored as Proposed, then flipped to Accepted after explicit human approval ("Approve 070, go execute your tasks as fast as you can"). All four ADR-kit gates pass on manual review:
- Completeness: all required sections present (Status, Context, Decision, Alternatives, Consequences, Related, References, Enforcement); filename matches heading number; ADR-068 status updated.
- Evidence: Andre quote verbatim; HA source paths/line numbers; HA docs URLs; firmware code paths.
- Clarity: single concrete decision; imperative voice; no hedging; concrete code-site table.
- Consistency: filename ADR-070 matches heading; Supersedes ADR-068 + Refines ADR-069 chain resolves; cross-reference to 2.0.0 ADR-097 documented as cross-worktree.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-070 (MQTT Source-Topic Sibling-Suffix Shape) authored as Proposed and accepted on 2026-05-07.

Key decisions documented:
- Use sibling-suffix shape (TSet_thermostat) instead of nested children (TSet/thermostat).
- Drop ADR-068 mutual-exclusion rule: canonical entity stays advertised alongside source variants under bSeparateSources=true (three additive entities).
- Discovery topic identifiers stay nested (HA-internal); only state_topic shape changes; HA handles the transition in-place via subscription.async_prepare_subscribe_topics.
- Enforcement block forbids `%s/thermostat` and `%s/boiler` PSTR literals in MQTTstuff.ino; discovery file exempt.

ADR-068 status line updated to "Superseded by ADR-070, 2026-05-07." Other content of ADR-068 preserved per immutability rule. The 2.0.0 mirror (ADR-097) is authored under TASK-553. Implementation tracked under TASK-552 (dev) and TASK-554 (2.0.0).
<!-- SECTION:FINAL_SUMMARY:END -->
