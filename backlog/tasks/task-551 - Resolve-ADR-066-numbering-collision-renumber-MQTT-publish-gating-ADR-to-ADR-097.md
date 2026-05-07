---
id: TASK-551
title: >-
  Resolve ADR-066 numbering collision: renumber MQTT publish-gating ADR to
  ADR-097
status: To Do
assignee: []
created_date: '2026-05-06 23:58'
updated_date: '2026-05-07 00:01'
labels:
  - adr
  - hygiene
  - 2.0.0
dependencies: []
priority: medium
ordinal: 18000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two distinct ADRs share number ADR-066 in the 2.0.0 worktree:

1. docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md
   Status: Proposed (2026-04-28). Ported from dev (1.5.x). Refined by ADR-096 (worldview).

2. docs/adr/ADR-066-thermostat-auto-detection-master-mode.md
   Status: Accepted (2026-04-04). 2.0.0-native (ESP32 + SAT scheduler).

The Accepted ADR has temporal priority and is immutable per CLAUDE.md (Accepted ADR content cannot be changed, including the number). The Proposed ADR is mutable and must be renumbered. Next available ADR number: ADR-097 (095 = bSeparateSources XOR, 096 = worldview semantics).

Discovered during TASK-550 (port of ADR-069 worldview semantics from dev to 2.0.0). The 2.0.0 agent flagged the collision but left it untouched as out-of-scope; status-line append on the publish-gating ADR-066 was applied to the correct file based on content disambiguation.

Blast radius: at least 9 files reference \"ADR-066\" textually, mixing both meanings. A contextual review per reference is required to determine which ADR is meant before mass-renaming.

Files known to reference \"ADR-066\" (per grep on 2026-05-07):
  - docs/adr/README.md (index)
  - docs/adr/ADR-068-ot-direct-schedule-tuning-constants.md
  - docs/adr/ADR-074-adr-audit-sat-integration-phase.md
  - docs/adr/ADR-094-ha-discovery-state-reconciliation-on-ota-upgrade.md
  - docs/adr/ADR-095-bseparatesources-mutually-exclusive-base-and-source-variants.md
  - docs/adr/ADR-096-mqtt-source-topic-worldview-semantics.md (Related Decisions section — references publish-gating ADR-066)
  - docs/api/MQTT.md
  - src/OTGW-firmware/MQTTstuff.ino
  - the publish-gating ADR-066 itself (self-title and any internal back-refs)

The auto-detect ADR-066 references in ADR-074 / ADR-094 etc. likely belong to the auto-detect ADR (SAT-related context). The MQTT.md, MQTTstuff.ino, and ADR-095/096 references almost certainly belong to the publish-gating ADR. README.md indexes both. Each grep hit must be disambiguated before edit.

This is a structural-hygiene task with no behavior change to the firmware. The renumber + reference updates can land in a single commit on the feature branch.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md renamed to ADR-097-mqtt-publish-gating-by-source-and-slave-echo.md
- [ ] #2 Title heading inside the renumbered ADR updated from "ADR-066" to "ADR-097"; all internal back-references (Verification gates, Related Decisions, References sections) updated consistently
- [ ] #3 Every textual "ADR-066" reference in docs/, src/, and other ADR files reviewed in context and updated to ADR-097 ONLY where it refers to the publish-gating decision; auto-detect ADR-066 references left untouched
- [ ] #4 ADR-096 Related Decisions / References sections updated: "refines ADR-066" becomes "refines ADR-097"
- [ ] #5 ADR-066-thermostat-auto-detection-master-mode.md is NOT modified (Accepted ADR, immutable)
- [ ] #6 docs/adr/README.md index updated to list ADR-097 instead of duplicate ADR-066
- [ ] #7 Build still passes (python3 build.py --firmware exit 0) — no functional code change expected, but verify nothing was accidentally broken in MQTTstuff.ino refactor
- [ ] #8 Evaluator clean (python3 evaluate.py --quick) and ADR-judge does not report broken cross-references
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Insights captured 2026-05-07 from the TASK-549 / TASK-550 parallel implementation session — apply when executing this cleanup.

=== Disambiguation patterns that work ===

1. Grep \"ADR-066\" alone is ambiguous; classify each hit by surrounding context BEFORE editing:
   - References in MQTT/discovery/source-separation/echo-gate context → publish-gating ADR (rename to ADR-097)
   - References in thermostat/auto-detect/master-mode/SAT context → auto-detect ADR (leave as ADR-066)
   - README.md index has BOTH; expect to update one entry, leave the other.

2. Confirmed disambiguation table from the 2026-05-07 grep:
   - docs/api/MQTT.md         → publish-gating (rename)
   - src/OTGW-firmware/MQTTstuff.ino → publish-gating (rename)
   - docs/adr/ADR-095-bseparatesources... → publish-gating (rename); ADR-095 amends the publish-gating ADR
   - docs/adr/ADR-096-mqtt-source-topic-worldview-semantics.md → publish-gating (rename); ADR-096 refines the publish-gating ADR via Refined-by line
   - docs/adr/ADR-068-ot-direct-schedule-tuning-constants.md → likely auto-detect (SAT context); verify
   - docs/adr/ADR-074-adr-audit-sat-integration-phase.md → auto-detect (SAT audit); verify
   - docs/adr/ADR-094-ha-discovery-state-reconciliation-on-ota-upgrade.md → likely publish-gating (HA discovery context); verify
   - docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md → self (the file being renamed)
   - docs/adr/ADR-066-thermostat-auto-detection-master-mode.md → unrelated; do not touch
   - docs/adr/README.md → both entries; rename one

3. Dont sed/awk a global replace. Open each file, read the context, replace per-occurrence.

=== Execution approach ===

Phase A — Inventory (do this first, NOT in the rename commit):
  grep -rn \"ADR-066\" docs/ src/ > /tmp/adr066-refs.txt
  Manually classify each hit (P = publish-gating, A = auto-detect, S = self/skip) in the file.

Phase B — Rename + edit (single commit):
  git mv docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md \
         docs/adr/ADR-097-mqtt-publish-gating-by-source-and-slave-echo.md
  - Edit the renamed files heading (\"# ADR-066: MQTT Publish Gating...\" → \"# ADR-097:\").
  - Edit each P-classified reference site to use ADR-097.
  - Update README.md index entry.
  - Verify by grep that no \"ADR-066\" remains EXCEPT for hits referring to the auto-detect ADR.

Phase C — Verification:
  - python3 build.py --firmware: exit 0 (no functional code change but MQTTstuff.ino touched, so confirm).
  - python3 evaluate.py --quick: 0 failures.
  - bin/adr-judge --staged (the pre-commit hook will also run this): 0 violations expected.
  - Single commit, push to feature branch per standing permission.

=== Pitfalls to avoid ===

- The Refined-by line in the renamed ADR-097 itself stays \"Refined by: ADR-096\" (ADR-096 is the worldview ADR; that number does not change).
- ADR-096 has a Refined-by-style mention of ADR-066 in its Related Decisions section; THAT must be updated to ADR-097.
- Do NOT add a new ADR for this hygiene change; renumbering a Proposed ADR is permitted directly.
- Do NOT edit the Accepted ADR-066 (auto-detect) at all — not even cosmetically.
- The publish-gating ADR is still Proposed; this is a chance to also flip it to Accepted (it has been in code for weeks). But that is a SEPARATE decision and not part of this hygiene task; flag it but defer.

=== Estimated effort ===

45 minutes for an experienced operator: 5 min inventory, 15 min classification, 15 min edits, 10 min build/eval/commit/push. Lower if scripted, higher if any P-classified references turn out to actually mean the auto-detect ADR (then more disambiguation iterations).

=== Why we did not execute now ===

Pure documentation hygiene with zero runtime impact. The collision is invisible to firmware users and tools (the publish-gating ADR is correctly identified by content, not by number). Higher-priority work — hardware verification of TASK-549 / TASK-550 ACs #6 — gates the headline release work. This cleanup is fine to land in a quiet commit window, ideally bundled with another low-risk doc update.
<!-- SECTION:NOTES:END -->
