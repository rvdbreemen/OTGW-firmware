---
id: TASK-701
title: Reduce device info endpoint heap pressure
status: Done
assignee: []
created_date: '2026-05-25 12:06'
updated_date: '2026-05-25 19:59'
labels: []
dependencies: []
---

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Addressed Simon Templar beta.21 field feedback (three fixes, all committed and pushed to origin/dev and origin/feature-dev-2.0.0-otgw32-esp32-sat-support):

1. LittleFS Size shows 1 MB (restAPI.ino): removed floorf() from littleFSSizeMB calculation — floorf(1.863) = 1.0, now reports correct ~1.86 MB float.
2. Auto-scroll checkbox resets on tab switch (index.js): added logTabActivatedAt timestamp; scroll handler ignores reflow-scroll events within 300 ms of Log tab becoming active, covering both openLogTab (inner tab) and showMainPage (outer navigation) paths.
3. Pre-commit hook exits 1 on clean commits (.githooks/pre-commit): two shell bugs fixed — grep -avE exits 1 when all lines match the invert pattern (under set -e), fixed with || true; missing exit 0 at end of script caused test-false exit code to propagate, fixed with explicit exit 0.

Builds green on both branches. No new evaluator failures.
<!-- SECTION:FINAL_SUMMARY:END -->
