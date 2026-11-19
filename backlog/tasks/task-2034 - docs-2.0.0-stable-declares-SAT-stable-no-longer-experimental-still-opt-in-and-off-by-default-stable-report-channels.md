---
id: TASK-2034
title: >-
  docs: 2.0.0 stable declares SAT stable (no longer experimental), still opt-in
  and off by default; stable report channels
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T17:53:00Z'
updated_date: '2026-11-19T09:15:00Z'
labels:
  - docs
  - sat
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1044000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, DM 17:48-17:51 2026-11-18 (ids 1572669805515903007, 1572669870414499931, 1572670004963451012, 1572670569961496768), relayed by the coordinator: 'Als we 2.0.0 releasen dan gaan we het woord experimenteel eraf halen. SAT krijgt stabiel niveau met de firmware. Oftewel we gaan voor A! Vanaf 2.0.0 wordt het dus stabiel verklaard. Wel nog opt-in modus.' Serves TASK-1740 AC4. Applies to the 2.0.0 stable: the commits stay local until the release is composed, so public betas before it do not call SAT stable. The beta.374 notes are not changed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Stable notes draft, README, ch01/h01 and ch05/h05 no longer call SAT experimental; each still says opt-in, off by default and the wizard confirmation (grep evidence)
- [x] #2 How to report for stable: a GitHub issue plus #english-support / #nederlandse-ondersteuning; #beta-testing only for beta testers (draft notes, ch05/h05)
- [x] #3 docs/MANUAL.md rebuilt and tests/test_manual_generated.py green
- [x] #4 Web UI strings that call SAT experimental are reported to ba (otgw-webui), not edited here
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 local commit on branch sat-stable-docs (from origin/dev 13c0a4e41), held until the 2.0.0 stable is composed. 'experimental' removed in: README :26/:45/:92 (also the beta.374 link -> RELEASE_NOTES_2.0.0.md#sat-is-opt-in-and-off-by-default, the stable notes file name /release-2x produces; the existing RELEASE_NOTES_2.0.0.md is the outdated April text, so the anchor resolves only once the draft replaces it) and :124 (report pointer -> ch05); ch01:59 / h01:68 ('SAT is stable from 2.0.0; it stays opt-in and is off by default'); ch05:12 / h05:8 (also dropped 'can still change between builds'); draft :34/:39/:45/:245 and the How to report list; the :248 review comment becomes the recorded decision. Report channels: GitHub issue + #english-support / #nederlandse-ondersteuning, beta testers #beta-testing (draft How to report, ch05:14, h05:10). Kept on purpose: ch05:259 / h05:286 'the early experimental version' (history of the model) and draft :196 'Treat it as experimental' for the 3-hour on/off heat-pump cycle (TASK-1542, bench-only on a simulated heat pump): question to Robert via the coordinator. Evidence: grep 'experiment' over README, ch01/h01, ch05/h05 and the draft leaves only those three; 'off by default/standaard uit' still present in each file; docs/MANUAL.md rebuilt, test_manual_generated OK, test_manual_sat_commands OK; draft lint TODO/TBD/em dash: none.

AC4: web-UI strings reported to ba (otgw-webui) by SendMessage 2026-11-18: v2.js:826 onb_beta_ack, :1566 sat_beta_head, :6938 SAT_BETA_HEAD, plus a check of the NL/DE packs; the heat-pump 3 h type labels (:1086, :1748, :4203) held for Robert's answer. v2.js not edited here.

Timing (coordinator, 2026-11-18): lands on dev as part of the RC commit set, together with ba's web-UI text task, not on release/2.x after the cut. The coordinator calls the RC moment; until then local. At that moment: rebase onto origin/dev, rerun scripts/build_manual.py and tests/test_manual_generated.py, re-grep 'experiment', then push and set Done.

2026-11-18 Robert 'B voor warmtepomp' (DM 1572672969744851067, relayed by the coordinator): the on/off heat-pump type (3 h cycle, TASK-1542) stays 'experimental'. Stable notes draft :196 'Treat it as experimental' and the web-UI labels '(experimental)' on that appliance type (v2.js :1086/:1748/:4203) stay as they are; only SAT as a whole is declared stable.

2026-11-19: docs/sat/field-run-recipe-2.0.0.md section 3 still sent uploads to #alpha-testing; aligned with ch05 and Robert's A (GitHub issue, #english-support / #nederlandse-ondersteuning, beta testers #beta-testing, or DM). Matches the coordinator's decision B for the UI beta notice (ba, TASK-2909 353d530d0: notice points to #beta-testing).

2026-11-19 rebase onto origin/dev 1a3422e3e for the 20:00 beta (coordinator A, together with ba's UI wording): README.md conflict on the SAT block resolved per hunk, TASK-2034 text taken; its link to RELEASE_NOTES_2.0.0.md (does not exist on dev until the stable release) replaced with Chapter 5 of the manual. TASK-2045 self-tune line and TASK-2908 notes kept (checked). MANUAL.md regenerated after the rebase; build_manual --check matches; test_manual_generated and test_manual_sat_commands OK. grep experiment: only the kept lines ch05:259, h05:286 and draft heat-pump item.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC4 ticked earlier with evidence in the notes. Final state on the rebased lane: README, ch01/h01, ch05/h05 and the 2.0.0 draft call SAT opt-in and off by default, stable from 2.0.0 (Robert's A, DM 2026-11-18); report channels: GitHub issue, #english-support / #nederlandse-ondersteuning, beta testers #beta-testing (draft How to report, ch05 report box, field-run recipe). grep -i experiment over README, manuals, draft and recipe: only the kept lines (ch05:259, h05:286 'early experimental version'; draft heat-pump item, Robert B). build_manual --check matches, test_manual_generated OK, test_manual_sat_commands OK. Not verified: the rendered README on GitHub.
<!-- SECTION:FINAL_SUMMARY:END -->
