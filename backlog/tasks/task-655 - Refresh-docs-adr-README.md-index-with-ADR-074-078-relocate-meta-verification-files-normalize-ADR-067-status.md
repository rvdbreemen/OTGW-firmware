---
id: TASK-655
title: >-
  Refresh docs/adr/README.md index with ADR-074-078 + relocate meta verification
  files + normalize ADR-067 status
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 05:51'
updated_date: '2026-05-22 06:41'
labels:
  - docs
  - adr
dependencies: []
references:
  - docs/adr/README.md
  - docs/adr/ADR-074-ha-availability-reflects-mqtt-link-not-ot-bus.md
  - docs/adr/ADR-078-defer-ha-core-aliases-to-2-0-0-revert-from-dev.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR README index stops at ADR-073; ADR-074/075/076/077/078 are missing — five accepted/superseded decisions actively cited in CHANGELOG/README. Four upper-snake-case meta files (ADR_DATE_VERIFICATION.md, ADR_DATE_EVIDENCE_EXAMPLES.md, ADR_VERIFICATION_REPORT.md, VERIFICATION_SUMMARY.md) pollute docs/adr/ and break the ADR-NNN-kebab.md convention. ADR-067 status reads non-standard 'Deprecated, withdrawn'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/README.md index lists ADR-074 (HA availability), ADR-075 (proxy-A routing), ADR-076 (drop rate-gate), ADR-077 (Superseded by ADR-078), ADR-078 (Accepted) under appropriate sections
- [x] #2 Decision Timeline (README.md) extended through ADR-078
- [x] #3 ADR-067 listed as '*(Deprecated)*' — narrative about withdrawal moved into the ADR body, not the index label
- [x] #4 Four meta files moved to docs/audits/ with YYYY-MM-DD prefix OR to docs/process/ if normative
- [x] #5 Section counts in README.md updated to match new totals
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated docs/adr/README.md to include ADR-074 (HA availability), ADR-075 (proxy-A routing, supersedes 069), ADR-076 (drop rate-gate), ADR-077 (HA-core aliases, Superseded by 078), ADR-078 (defer aliases to 2.0.0). Decision Timeline extended through ADR-078. ADR-067 status normalized from 'Deprecated, withdrawn' to 'Deprecated' with narrative moved into the summary. Four meta verification files (ADR_DATE_VERIFICATION.md, ADR_DATE_EVIDENCE_EXAMPLES.md, ADR_VERIFICATION_REPORT.md, VERIFICATION_SUMMARY.md) relocated to docs/audits/ with YYYY-MM-DD prefix matching the existing audit-dir convention. AC #5 (section counts) was N/A — the README doesn't use numeric counts. Commits a6df21c2 (index + status fix) and 41084f1e (file moves).
<!-- SECTION:FINAL_SUMMARY:END -->
