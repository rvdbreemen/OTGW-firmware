---
id: TASK-2029
title: >-
  ADR lint: 17 supersession FAILs from partial-supersede prose (ADR-204 ->
  ADR-197 and 16 more)
status: To Do
assignee: []
created_date: '2026-11-18T14:44:00Z'
updated_date: '2026-11-20T01:13:00Z'
labels:
  - adr
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1039000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
adr-lint --strict on origin/dev c31ded494 reports 16 one-directional and 1 concurrent supersession FAILs (004/070, 005/133, 014/083, 026/139, 045/046, 048/136, 051/079, 055/057, 077/106, 097/098, 139/215, 141/146, 197/204, 199/203, 203/207, 250/253; concurrent 082 by 123 and 128). Cause: adr-lint detect_supersession_conflicts counts any 'Supersedes ADR-NNN' phrase in an Accepted ADR's text (SUPERSEDES_CLAIM_RE), including titles like '(supersedes ADR-197 decision 8)' and '(partly supersedes ADR-250)'. The lint's suggested fix (target Status 'Superseded by ADR-X') is wrong for a partial supersede: it would mark the whole target superseded. Accepted bodies are immutable, so the claimant prose cannot be edited. Options: A per-pair review: full supersedes get 'adr supersede' bookkeeping, partial ones are recorded as an accepted lint waiver; B upstream adr-kit change so 'partly supersedes' / 'supersedes ADR-X decision N' is not a full claim; C successor ADRs that formally amend. Decision for Robert.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every one of the 17 pairs classified full or partial, with file:line
- [x] #2 Chosen option recorded (Robert's decision), applied with adr-kit commands only
- [ ] #3 adr-lint --strict shows no supersession FAIL, or each remaining one is a recorded waiver
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Read-only classification A (2026-11-18, otgw-dev; no ADR file edited). Probe: adr-lint 0.61.0 detect_supersession_conflicts, SUPERSEDES_CLAIM_RE adr-lint:234, _status_line adr-lint:758; per target the lint's own parse was printed.

Class 1, real full supersede, already recorded on both sides, lint cannot read the target (7): the target has frontmatter superseded_by and a bold '**Status:** Superseded by ADR-X' line (old format); _status_line returns '' for these, so the lint sees no successor.
- 005 <- 133: ADR-005:frontmatter superseded_by ADR-133; claim ADR-133:3,14,26
- 014 <- 083: ADR-014 superseded_by ADR-083; claim ADR-083:20
- 026 <- 139: ADR-026:11,15 ADR-139; claim ADR-139:31,38
- 045 <- 046: ADR-045:11,15 ADR-046; claim ADR-046:52
- 048 <- 136: ADR-048 superseded_by ADR-136; claim in ADR-136 (status_history line 48)
- 055 <- 057: ADR-055:11,15 ADR-057; claim ADR-057:122
- 141 <- 146: ADR-141 Status '**Superseded by [ADR-146](...)**': the markdown link breaks SUPERSEDED_BY_RE (adr-lint:235 has no '\[?', adr_schema.py:101 does); claim ADR-146:52
Fix: none in our ADRs; lint parse defect (option B).

Class 2, prose mention of another ADR's supersession, not a claim by this ADR (3):
- 004 <- 070: ADR-070:151 quotes 'ADR-053 ... (supersedes ADR-004)'; ADR-004:11,15 is Superseded by ADR-053
- 077 <- 106: ADR-106:180 'ADR-078 supersedes ADR-077'; ADR-077 is the dev-line sibling
- 082 <- 123 (half of the concurrent FAIL): ADR-123:21,192,250,266 all say 'ADR-128 supersedes ADR-082'; the real claimant ADR-128:3,14,73 and ADR-082 Status 'Superseded by ADR-128' agree
Fix: none; lint false positive (option B: a claim is the ADR's own title/Status/Related, not a quoted sentence).

Class 3, real partial supersede, target correctly stays Accepted (6):
- 051 <- 079: ADR-079:19 'Supersedes ADR-051's single-file layout choice; keeps ...'
- 139 <- 215: ADR-215:3,48 'partly supersedes ADR-139, ADR-190 and ADR-192'
- 197 <- 204: ADR-204:3,43 'supersedes ADR-197 decision 8'; ADR-204 Status says do not run adr supersede
- 199 <- 203: ADR-203:3,49 'supersedes ADR-199 decision 4'
- 203 <- 207: ADR-207:3,45,72 'partly supersedes ADR-203'
- 250 <- 253: ADR-253:3,41 'partly supersedes ADR-250'
Fix: must NOT get 'Superseded by' (would retire the still-valid rest). Waiver per pair (option A) or option B. ADR-259 now says 'amends ADR-197' so it does not add a 7th.

Class 4, wrong ADR number, needs a human (1): 097 <- 098. ADR-098:124 'Supersedes ADR-097 (MQTT Source-Topic Sibling-Suffix Shape, the 2.0.0 port of ADR-070)', but ADR-097 is 'MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification', a BINDING ADR in CLAUDE.md. ADR-098 points at a number that holds a different decision (probably a renumber). Running adr supersede ADR-097 --by ADR-098 would retire a binding gate: do not. Which ADR ADR-098 really superseded is still to be found.

Summary: of 18 claims (17 FAIL lines), 0 need a 'Superseded by' edit; 10 are lint parse/mention defects, 6 are legitimate partial supersedes, 1 is a wrong reference in ADR-098 that needs Robert. Recommendation: B (upstream adr-lint: read the bold/linked Status and frontmatter superseded_by; count only the ADR's own claim, and not 'partly'/'decision N') plus a recorded waiver for the 6 partials until B ships; ADR-098 as its own question.

Class 4 resolved (read-only search, 2026-11-18): ADR-098 really supersedes the sibling-suffix ADR that is now ADR-119, and this is already recorded correctly on both sides.
- History: 4a2a5b9ad (2026-05-07) filed 'sibling-suffix source topic shape' as ADR-097 (TASK-554); b537beffb (2026-05-07) ADR-098 'supersedes ADR-097' (TASK-557); e9ace18cc (2026-05-27) renumbered the publish-gating ADR-066 to ADR-097 (TASK-551), creating a duplicate 097; a13ba9fa8 (2026-06-02, Robert) renumbered the dead suffix-shape ADR from 097 to ADR-119 and deliberately left the frozen bodies (095/096/098/101/105) saying 'ADR-097' under the immutability rule, with a renumber note in ADR-119.
- Frontmatter, the source of truth: ADR-098:11-12 supersedes ['ADR-119']; ADR-119:4 status Superseded, ADR-119:10 superseded_by 'ADR-098'; ADR-119:3,14 title 'renumbered from ADR-097', ADR-119:16 renumber note.
- What the lint sees: the frozen prose ADR-098:3,14,124 'Supersedes ADR-097' is counted as a claim on the live, binding ADR-097 (publish gating).
- Conclusion: no human decision about the content is needed. It is a lint false positive of class 2 (prose claim that contradicts the frontmatter). Fix: B (prefer the frontmatter supersedes list over prose) or a recorded waiver. Never run 'adr supersede ADR-097' (binding gate, check_ps_summary_master_topic_gate).
Updated totals: 18 claims, 0 need a Status edit; 11 lint defects (classes 1, 2 and 4) and 6 legitimate partial supersedes.

Decision (Robert, Discord DM 1572637750618886297, 2026-11-18 15:40, "A+B"; relayed and confirmed in DM 1572638924659105984). Read at the source by otgw-dev.

A, waiver recorded: the 6 legitimate partial supersedes below keep their target Accepted. Their adr-lint --strict 'one-directional supersession' FAIL is accepted as known noise until B (the adr-kit fix) is released. Nobody runs `adr supersede` and nobody writes 'Superseded by' on these targets:
- ADR-051 <- ADR-079 (ADR-079:19, single-file layout choice only)
- ADR-139 <- ADR-215 (ADR-215:3,48, partly)
- ADR-197 <- ADR-204 (ADR-204:3,43, decision 8 only)
- ADR-199 <- ADR-203 (ADR-203:3,49, decision 4 only)
- ADR-203 <- ADR-207 (ADR-207:3,45,72, partly)
- ADR-250 <- ADR-253 (ADR-253:3,41, partly)
The waiver ends when the adr-kit release with B is installed here and adr-lint --strict shows no supersession FAIL.
B: rvdbreemen/adr-kit branch fix/supersession-claim-parsing fdf10bb (local); the PR opens after the full suite on fdf10bb is green; merging is Robert's call.

2026-11-20 AC2 ticked (otgw-docs, coordinator's assignment): the chosen option is recorded above (Robert 'A+B', Discord DM 1572637750618886297, confirmed DM 1572638924659105984); A (waiver for the 6 partial supersedes) needs no ADR edit, so no adr-kit command was required; B is the adr-kit fix fdf10bb, not released yet. AC3 NOT met: adr-lint 0.61.1 --strict on origin/dev b5158b75e (run 2026-11-20 by otgw-docs) still reports 18 supersession FAILs: the 17 classified pairs plus a NEW one, ADR-102 <- ADR-255.

2026-11-20 classification of ADR-102 <- ADR-255 (read-only, otgw-docs): class 3, real PARTIAL supersede; the target correctly stays Accepted. ADR-255:3 and :40 title '(partly supersedes ADR-102)'; :112 'This partly supersedes ADR-102: its rule stays for the base topic and for every other entity; for this one switch, availability also reflects whether the action it offers can succeed.'; :165 Related 'partly superseded for the SAT simulation switch only; the base-topic rule and its gate stay.'; frontmatter ADR-255:10 supersedes [] and ADR-102:4 status Accepted, :11 superseded_by null (consistent). Fix: none in the ADRs; must NOT get 'Superseded by' (would retire ADR-102's base-topic rule). Belongs in the A waiver list as a 7th partial (needs Robert's extension, coordinator is asking) until B is installed. Updated totals: 19 claims, 0 need a Status edit; 11 lint defects, 7 legitimate partial supersedes.
<!-- SECTION:NOTES:END -->
