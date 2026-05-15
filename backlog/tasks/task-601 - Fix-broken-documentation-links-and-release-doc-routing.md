---
id: TASK-601
title: Fix broken documentation links and release-doc routing
status: Done
assignee: []
created_date: '2026-05-15 09:09'
updated_date: '2026-05-15 09:18'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Repair broken README/docs release links after documentation moves, add release index and doc link policy, and add PR checklist item for link validation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README links updated to existing canonical docs paths
- [x] #2 Broken relative links in docs/releases and docs/releases/archive are corrected
- [x] #3 docs/releases/README.md added with current vs archive navigation
- [x] #4 CI includes markdown link validation
- [x] #5 PR template includes link validation checkbox
- [x] #6 Documentation link policy exists and explains canonical doc locations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated README and release documentation links to canonical locations, fixed broken relative links in docs/releases and docs/releases/archive, added docs/releases/README.md index, added docs/process/DOCUMENTATION_LINKS_POLICY.md, added .github/pull_request_template.md checkbox for link validation, and added CI markdown internal link validation in .github/workflows/evaluate.yml. Changes were applied on dev task branch (commits e64ad833, 1b02acb5) and mirrored on feature 2.0.0 worktree branch copilot/update-documentation-links-2.0.0 (commit e6092a57).
<!-- SECTION:FINAL_SUMMARY:END -->
