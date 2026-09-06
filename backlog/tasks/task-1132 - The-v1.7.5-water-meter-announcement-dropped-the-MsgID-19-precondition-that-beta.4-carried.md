---
id: TASK-1132
title: The water-meter docs state the MsgID 19 precondition but never the remedy
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-06 16:42'
updated_date: '2026-09-06 17:10'
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The water-meter documentation stated the precondition and stopped there. It now names the remedy.

Premise correction first, because the task was opened on the wrong one. I reported that the v1.7.5 release notes had dropped the MsgID 19 precondition the beta.4 announcement carried. Reading them showed otherwise: RELEASE_NOTES_1.7.5.md:69, RELEASE_GITHUB_1.7.5.md:26 and README.md:17 all stated it. Only the Discord announcement condensed it away, and that message is sent and stands. The description and title were rewritten rather than quietly reinterpreted.

What was genuinely missing, in all three: what the reader should do about it. Each said "nothing is published until a MsgID 19 frame has actually decoded" and left it there. The gateway does not poll that id, so a thermostat that never asks for it produces no entity and no next step. `AA=19` (`addalternative`) makes the gateway substitute the request itself. That command appeared in the repository exactly once before this change, as a row in the command table at docs/api/MQTT.md:474, with nothing connecting it to the water meter.

Changes, documentation only, no firmware and no reflash:
- README.md, RELEASE_NOTES_1.7.5.md and RELEASE_GITHUB_1.7.5.md each name the remedy in the sentence that already carried the precondition, so the fact and the action sit together. In the release notes the added clause was reworded, because that paragraph already opened with "The gateway does not poll MsgID 19" and would otherwise have said it twice.
- The published v1.7.5 release body was updated in place and verified live.
- Cherry-picked onto main as da73b283c. main matters here because the release body links to blob/main/README.md and blob/main/RELEASE_NOTES_1.7.5.md, so a reader following those links from the release page lands on main's copies. Verified over raw.githubusercontent.com: both now carry it.

Two facts worth keeping:
- v1.7.5 reports `isImmutable: true` and `gh release edit --notes-file` still succeeded. Immutability covers the assets and the tag, not the notes, so a wording correction after publication is possible. The memory note that said otherwise is corrected.
- main was not merged. origin/main was 0 behind while otgw-1.x.x was 61 ahead, so a fast-forward would have dragged the entire 1.7.6-beta line onto the release branch. A cherry-pick of the single docs commit was the right tool, and the three `.claude/*_last_checked.txt` stamps in that commit were deliberately left behind as sweep working state rather than release text.

Origin: a support question, not a defect. stefan_24213 in Discord #nederlandse-ondersteuning updated to 1.7.5 on 2026-09-06, saw no sensor, and asked whether he had to do something himself. He did, and the answer had to be typed by hand. The next user on a bus without DHW flow data now finds it in the documentation.
<!-- SECTION:FINAL_SUMMARY:END -->
