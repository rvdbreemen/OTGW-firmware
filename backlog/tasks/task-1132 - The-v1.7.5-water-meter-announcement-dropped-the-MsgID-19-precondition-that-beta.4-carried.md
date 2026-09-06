---
id: TASK-1132
title: The water-meter docs state the MsgID 19 precondition but never the remedy
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-06 16:42'
updated_date: '2026-09-06 16:48'
labels:
  - documentation
dependencies: []
priority: medium
ordinal: 216000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The cumulative DHW water total (TASK-1091) is announced to Home Assistant only after a MsgID 19 frame has actually decoded. That gate is deliberate (TASK-1093, ADR-093).

Premise correction, recorded because the task was opened on the wrong one: RELEASE_NOTES_1.7.5.md:69, RELEASE_GITHUB_1.7.5.md:26 and README.md:17 all DO state that precondition. Verified by reading them. The omission was in the Discord release announcement, which condensed the item to 'Nieuw: een cumulatieve warmwatermeter voor het Energy-dashboard van Home Assistant. Geen helper en geen YAML nodig.' That message is sent and is not being retracted.

What survives is the half none of the three documents carries: what the reader should DO about it. They state the fact and stop. A thermostat that never requests MsgID 19 will never produce the entity, and the gateway does not poll that id on its own, so the remedy is AA=19 (addalternative), which makes the gateway substitute the request itself. That command appears in the repository exactly once, as a row in the command table at docs/api/MQTT.md:474, with no connection to the water meter.

Observed cost of the gap: stefan_24213 in Discord #nederlandse-ondersteuning on 2026-09-06 updated to 1.7.5, saw no sensor, and asked. The maintainer answered by hand with MsgID 19 and the AA=19 hint. Every later user on a bus without DHW flow data needs the same answer.

Documentation only: no firmware change, no new build, no reflash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The reader is told what to do about it: AA=19 makes the gateway request MsgID 19 itself when the thermostat never asks
- [x] #2 The published GitHub release body for v1.7.5 carries the same correction
- [x] #3 README.md, RELEASE_NOTES_1.7.5.md and RELEASE_GITHUB_1.7.5.md each name the remedy alongside the precondition they already state
- [x] #4 The remedy is stated accurately: the gateway does not poll MsgID 19, AA=19 adds it to the alternative-message table, and that table is finite (NS - No Space)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verified the premise first: all three documents already state the precondition, so the change is additive, not corrective. Only the Discord announcement dropped it, and that message stands.
2. Append the remedy to the three reader-facing places, in the sentence that already states the precondition, so the fact and the action sit together rather than in separate paragraphs.
3. Keep it one clause: the gateway does not poll MsgID 19, so AA=19 makes it ask. Do not restate the OpenTherm version or the entity attributes, which are already there.
4. Update the published GitHub release body with gh release edit --notes-file, per the /release skill Phase 7.
5. Carry the same edit onto main, because main is what the published v1.7.5 tag points at and what a reader reaches from the release page. Return the worktree to otgw-1.x.x afterwards.
6. Docs-only, so the build and evaluator gates do not apply under the push policy.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Premise was wrong and is corrected in the description: all three documents already stated the MsgID 19 precondition. Verified by reading RELEASE_NOTES_1.7.5.md:69, RELEASE_GITHUB_1.7.5.md:26 and README.md:17. Only the Discord announcement dropped it, and that message stands.
- What was actually missing everywhere: the remedy. Added AA=19 beside the precondition in all three, de-duplicated in the release notes where the paragraph already opened with "The gateway does not poll MsgID 19".
- Published release body updated and verified live. Worth recording: v1.7.5 reports isImmutable true, and gh release edit --notes-file still succeeded. Immutability covers assets and the tag, not the notes.
- main NOT synced. origin/main is 0 behind and otgw-1.x.x is 61 ahead, so a fast-forward would drag the whole 1.7.6-beta line onto the release branch. A cherry-pick of the docs commit is the right tool and needs per-instance confirmation for an origin/main push.
<!-- SECTION:NOTES:END -->
