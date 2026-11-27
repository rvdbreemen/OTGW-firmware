---
id: TASK-3111
title: >-
  Style review of docs and READMEs: replace em dashes and AI-sounding wording
  with the replacement strings from the ux review
status: Done
assignee:
  - '@claude'
created_date: '2026-11-27T05:06:00Z'
updated_date: '2026-11-27T06:47:00Z'
labels:
  - docs
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1169000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source: otgw-ux-designer's review, commit b433185d7 in worktree wt-ux, file docs/reviews/2026-11-26_style-em-dashes-and-ai-language/README.md (user-facing text, with replacement strings). Requested by the coordinator 2026-11-27. Scope: docs, READMEs and release notes only, not code strings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Replacements from the review applied to docs, READMEs and release notes (not to code strings)
- [x] #2 The em-dash lint test is green after the change
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 Done by otgw-docs. Scope note: the replacement table of the ux review (b433185d7) is for web UI strings (v2.js, i18n, v2.html) and firmware debug lines, which are code strings and belong to otgw-webui and otgw-dev; this task did the documentation: README (0 hits), CHANGELOG, docs/guides, docs/hardware, docs/process, docs/features, docs/templates, docs/BREAKING_CHANGES.md, the manuals and docs/api (0 hits). The rule used: colon when the second half explains or names a step, period when two thoughts stand alone, comma or parentheses for an aside; fenced code blocks and inline code spans are left alone (a few JSON placeholders such as <int - bytes> and the 1.x release title format v<version> - <Short Title> in docs/process/RELEASE_PROCESS.md stay). Not touched: the published release notes and docs/releases/archive (history), ADRs, plans, reviews, research. New test tests/test_docs_no_em_dash.py.

2026-11-27 CHECK before the push (coordinator's condition): read all 9 changed lines of CHANGELOG.md and docs/BREAKING_CHANGES.md in full (old and new) and 15 of the 180 changed lines of docs/guides (random sample, seed 7: MQTT_STALE_TOPICS_CLEANUP x2, MQTT_API_CONTRACT x2, MQTT_STALE_TOPICS_CLEANUP_NL, versioning-policy, DOMOTICZ x2, ESP_ABSTRACTION x3, OT_THING_FEATURE_PARITY, agent-build-and-bench, OPENHAB), plus all 6 changed lines that had two or more dashes. No meaning shifted in the 9 and in 14 of the 15 sampled lines. Three lines with two dashes were wrong (the pairing rule for an aside broke): the CHANGELOG licence line (the two library credits became a broken parenthesis), a MQTT_API_CONTRACT table row (dashes in two different cells) and the bold ALWAYS-use line in agent-build-and-bench.md (unbalanced bold and parenthesis); all three fixed by hand in the follow-up commit. No range or list was changed by a dash replacement in the lines read. Not read line by line: the other 165 lines of docs/guides and the files of docs/features, docs/process, docs/hardware and docs/templates.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: 293 lines in 22 documentation files rewritten without em dashes (commit dada10048; README, manuals and docs/api had none); code strings (web UI, i18n, firmware debug lines) are the ux review's other half and belong to otgw-webui and otgw-dev. AC2: new tests/test_docs_no_em_dash.py: red on the old files (the checker lists the em dashes of the unswept guides, process and CHANGELOG), green after the sweep (2 tests OK); test_manual_generated, test_agent_core_block and test_adr_governance still OK. Not verified: the quality of every rewritten sentence was sampled, not read line by line (about 30 of 293 lines read); the Dutch guides were swept with the same English rule.
<!-- SECTION:FINAL_SUMMARY:END -->
