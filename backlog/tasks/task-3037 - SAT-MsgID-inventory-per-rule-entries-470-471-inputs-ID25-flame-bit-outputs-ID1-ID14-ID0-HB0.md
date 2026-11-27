---
id: TASK-3037
title: >-
  SAT: MsgID inventory per rule (entries 470/471: inputs ID25 + flame bit;
  outputs ID1, ID14, ID0 HB0)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-22T16:44:00Z'
updated_date: '2026-11-27T05:50:00Z'
labels:
  - sat
milestone: 2.1.0
dependencies: []
priority: high
ordinal: 1145000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entries 470/471 (private SAT register): SAT's inputs are ID25 (flow) and the flame bit, its outputs ID1 (control setpoint), ID14 (max modulation) and ID0 high byte bit 0 (CH enable). This task inventories every SAT control rule against that, so Sergeant D can decide per rule. No code until the token budget hold ends (Wed 25 Nov).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A table of every SAT control rule with the MsgIDs it reads and writes, each with file:line on origin/dev
- [ ] #2 Each rule that reads anything beyond ID25 and the flame bit is flagged with its fallback (e.g. the ID28 return temperature, SATcontrol.ino:1300-1310; the ID17 users incl. satUpdateModulationReliability; the outside temperature)
- [ ] #3 The result is posted to Sergeant D as one page for his decisions
- [ ] #4 The inventory becomes a reference document of how SAT works, under docs/sat/: per rule its inputs, outputs, source entry and file:line (Robert, #dev-sat-mqtt 1574103281544859800: 'feed the knowledge on how it's done and validate. The implementation and the code with that.')
- [ ] #5 Per rule a host test that calls the real code and goes red when the rule is broken
- [ ] #6 Rules without an entry and entries without a rule are listed as findings
- [ ] #7 Sergeant D reviews the document
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22 step 1 (Robert: 'Go ahead. Do it.', #dev-sat-mqtt 1574112435315548282): docs/sat/sat-msgid-inventory.md, read-only inventory of origin/dev 9caac3519: one table row per SAT rule (source entry/ADR, OT inputs, outputs, fallback, file:line, steers yes/no), rows reading beyond ID25 + flame bit flagged BEYOND, and an 'Open for Sergeant D' section (5 questions: ID28 in the flame-off hold; ID28 in the no-return path; ID17 in solar gain; ID0 LB fault/DHW bits; ID24/ID27 as inputs out of scope). Method: a script listed every OT read (direct and through the six getters) per enclosing function; the steering ID28/ID17 uses were opened and read. Rows marked 'not traced' were not opened. Not done yet (after Wednesday): per-rule host tests, the findings list, Sergeant D's review.

2026-11-22 BRANCH RULE (Robert, #dev-sat-mqtt 1574113439813603421: 'this is a separate branch for now'): all code for the ID25/flame principle (TASK-3036, the TASK-3037 per-rule host tests and doc follow-ups, and the Q-466a change once Sergeant D decides) goes on ONE branch off origin/dev, sat/id25-principle, not on dev. Keep it rebased on origin/dev, bumps per the usual rule. Merging it into dev needs Robert's explicit yes (hard stop). Docs-only task notes and docs/sat/sat-msgid-inventory.md stay on dev. Code after Wednesday's reset.

2026-11-25 table fix (entries 486, 470-474): docs/sat/sat-msgid-inventory.md now lists SH= (ID57, SATcontrol.ino:396-412) and SW= (ID56, :7902-7916) as kept optional outputs (entry 486, '3: A'), and the 'Open for Sergeant D' questions 1-5 are marked settled: 1 and 2 by entry 474 (ID28 stays when available), 3 to 5 by entry 473 points a, c, d (derived by the coordinator, not asked; solar gain only on branch sat/id25-principle). Docs only; line refs checked on origin/dev f178d164c. AC1 and AC2 stay unticked: the per-rule rows marked 'not traced' are still open.

2026-11-27 moved to milestone 2.1.0 by the coordinator's decision (D10). The inventory part (docs/sat/sat-msgid-inventory.md, entries 473/474/486 settled the questions of the one-page post to Sergeant D) is the 2.0.0 deliverable as far as it goes; rows marked 'not traced' remain open. The per-rule host tests and Sergeant D's review (AC5-AC7) move with the task. No ACs ticked in this step.
<!-- SECTION:NOTES:END -->
