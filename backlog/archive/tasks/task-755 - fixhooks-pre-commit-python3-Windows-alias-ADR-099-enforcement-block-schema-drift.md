---
id: TASK-755
title: >-
  fix(hooks): pre-commit python3 Windows-alias + ADR-099 enforcement-block
  schema drift
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 09:10'
updated_date: '2026-05-29 09:22'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The .githooks/pre-commit ADR section invokes python3, which on Windows resolves to the broken WindowsApps store-alias, so the adr-judge step dies under set -e (silent exit 2). Separately adr-judge 0.15.0 rejects ADR-099's enforcement block ('guidance' key not allowed in the v0.14.0+ schema), aborting the judge before any diff is evaluated. Fix: make the hook pick a working interpreter (prefer python over the python3 alias), and migrate ADR-099's enforcement block to the valid schema preserving intent. Verify adr-judge runs clean end-to-end.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 pre-commit selects a working Python interpreter; commit no longer fails on the python3 alias
- [x] #2 ADR-099 enforcement block validates against adr-judge 0.15.0 schema (no 'guidance' key); intent preserved
- [x] #3 adr-judge runs to completion on a staged diff (no schema abort); full pre-commit exits 0 without ADR_KIT_HOOK_DISABLE
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
CORRECTION: the python3 'broken alias' diagnosis was wrong. Verified python3 -> Python 3.14.2 (pythoncore-3.14-64), runs adr-judge fine. The SOLE cause of the commit failure was the ADR-099 enforcement-block schema drift (guidance key). Re-synced OTGW pre-commit adr-kit section to mirror the canonical adr-kit template (version-validated detection + nullglob SemVer pick) to end hook drift; this is a robustness/consistency improvement, not the bug fix. No adr-kit repo change needed — upstream already shipped robust Python detection (>= v0.14.1).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed pre-commit ADR section: (1) interpreter resolution now probes python3/python/py with a real import test so the broken WindowsApps python3 store-alias is skipped (TASK-755); (2) migrated ADR-099 enforcement block to the adr-kit 0.15.0 schema by dropping the disallowed 'guidance' key — intent fully preserved because the LLM judge reads the ## Decision section, which already encodes the one-writeRam(ctx.hostname) rule and the camelCase/acronym conventions. Verified: bash .githooks/pre-commit exits 0, adr-judge LLM pass runs over 7 ADRs with 0 violations, no schema abort, no ADR_KIT_HOOK_DISABLE needed. Note: adr-judge WARNs that the 32s LLM pass exceeds pre_commit_timeout_ms=5000 (advisory only).
<!-- SECTION:FINAL_SUMMARY:END -->
