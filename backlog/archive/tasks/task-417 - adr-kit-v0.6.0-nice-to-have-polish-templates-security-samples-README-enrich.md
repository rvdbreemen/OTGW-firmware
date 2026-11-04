---
id: TASK-417
title: >-
  adr-kit v0.6.0: nice-to-have polish (templates, security, samples, README
  enrich)
status: Done
assignee: []
created_date: '2026-04-25 19:02'
updated_date: '2026-04-25 19:17'
labels:
  - adr-kit
  - external-repo
  - polish
  - nice-to-have
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add the polish items that round off the public-plugin experience.

Scope:
1. .github/ISSUE_TEMPLATE/bug.yml with structured bug-report fields.
2. .github/ISSUE_TEMPLATE/feature_request.yml with structured feature-request fields.
3. .github/pull_request_template.md with a checklist (verification gates passed, CHANGELOG updated, tests pass, etc.).
4. SECURITY.md with a minimal security disclosure policy (the plugin does not handle secrets, but make the disclosure path explicit).
5. CODE_OF_CONDUCT.md (Contributor Covenant 2.1, standard text).
6. examples/ enriched with two sample ADRs: a "do" example demonstrating the four verification gates passing, and a "before / after" example showing how a vague ADR fails the Evidence gate and how to fix it.
7. README enriched: add a short FAQ section ("Where are ADRs stored?", "How do I customize the conventions?", "What if my project already has ADRs?") and a short "Comparison" or "How is this different from a plain ADR template" section.
8. Bump to v0.6.0; tag adr-kit--v0.6.0; GitHub Release.
9. Sync mirror in OTGW-firmware/tools/adr-kit/.

Out of scope:
- Demo GIF (requires manual recording, leave for the user to add).
- Official marketplace submission (ongoing process, not a one-shot deliverable).</description>
<parameter name="acceptanceCriteria">[".github/ISSUE_TEMPLATE/bug.yml exists with structured fields", ".github/ISSUE_TEMPLATE/feature_request.yml exists with structured fields", ".github/pull_request_template.md exists with verification-gates checklist", "SECURITY.md exists with disclosure policy", "CODE_OF_CONDUCT.md exists (Contributor Covenant 2.1)", "examples/ contains at least two named sample ADRs that exercise the four verification gates", "README.md has a FAQ section with at least three Q&A pairs", "README.md has a short Comparison section distinguishing adr-kit from a plain ADR template", "Plugin manifest version is 0.6.0, tag adr-kit--v0.6.0 exists, GitHub Release v0.6.0 is published", "OTGW-firmware tools/adr-kit/ mirror is in sync with the canonical repo at v0.6.0", "All edits are em-dash-free, English, no emojis"]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 .github/ISSUE_TEMPLATE/bug.yml with structured fields
- [x] #2 .github/ISSUE_TEMPLATE/feature_request.yml with structured fields
- [x] #3 .github/pull_request_template.md with verification-gates checklist
- [x] #4 SECURITY.md with disclosure policy
- [x] #5 CODE_OF_CONDUCT.md (Contributor Covenant 2.1)
- [x] #6 examples/ has two named sample ADRs exercising the four verification gates
- [x] #7 README.md has FAQ section with at least three Q&A pairs
- [x] #8 README.md has Comparison section vs plain ADR template
- [x] #9 plugin.json version is 0.6.0, tag adr-kit--v0.6.0 + Release published
- [x] #10 OTGW-firmware tools/adr-kit/ mirror in sync with v0.6.0
- [x] #11 All edits em-dash-free, English, no emojis
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v0.6.0 published. New: bug.yml, feature_request.yml, pull_request_template.md, SECURITY.md, CODE_OF_CONDUCT.md (Contributor Covenant 2.1 by reference, full canonical text linked at upstream to avoid filter issues), 2 sample ADRs in examples/ (ADR-sample-001 do-example with all gates passing, ADR-sample-002 before/after for the Evidence gate), README FAQ section (5 Q&A) and Comparison section (table vs plain ADR template). plugin.json bumped to 0.6.0. CHANGELOG.md [0.6.0] entry plus link references. Tagged adr-kit--v0.6.0, GitHub Release published with --latest. Mirror in OTGW-firmware/tools/adr-kit/ synced (10 files: 7 new, 3 updated). Closes the three-release production-grade-baseline series (v0.4.0 must-have, v0.5.0 should-have, v0.6.0 nice-to-have).
<!-- SECTION:FINAL_SUMMARY:END -->
