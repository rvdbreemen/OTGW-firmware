---
id: TASK-1244
title: 'settings.ini writer: helpers own the comma separator (no per-call withComma)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-17 21:34'
updated_date: '2026-10-17 21:53'
labels:
  - settings
  - refactor
dependencies: []
ordinal: 382000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1242. writeSettings() passes withComma per call (~185 calls), so appending a key, or a build whose last key sits in an #if block, can silently produce invalid JSON; the line-based reader never notices. The TASK-1242 guard test runs nowhere automatically. Fix: the writeJson*KV helpers write the separator before every entry except the first (static first-entry flag reset by writeSettings), and the withComma parameter is removed. Output must stay byte-identical.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Guard check in evaluate.py fails on the current code (withComma still present) and passes on the fix
- [x] #2 settings.ini written by the fix is byte-identical to the alpha.431 file for the same settings (bench, combo)
- [x] #3 json.load passes and REST settings are unchanged (176/176) after the update
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-17, Classic-S3 COM8/.88.63, esp32-combo. Gate: evaluate --quick FAIL on the old code (withComma + 187 flagged calls), PASS on the fix; tests/test_evaluate.py 80 OK. alpha.432 regression caught on the bench: the BLE roster loop kept its own printf with a trailing comma, settings.ini became invalid ('"SATblenamefilteringest": false  "SATblemac0"' + stray ',' line; .tmp/bench-1244/a432-241.ini). Sharpened gate (no direct file writes in writeSettings() but the braces) FAILED on alpha.432 (3 direct writes) and PASSES on alpha.433. alpha.433+b040190: write_probe.py waits for the telnet 'Settings saved successfully' line per POST: 241 -> valid JSON 211 keys pvmax=241; 240 -> valid JSON, sha256 703f470a... byte-identical to the alpha.431 file (a431.ini vs a433-240.ini). REST 176/176 no diffs. Then alpha.433 filesystem + restore: defaults valid JSON 211 keys, restored file served unchanged, REST 176/176 no diffs, version.hash = fw githash b040190. Lesson: an early 'byte-identical' (alpha.432) was a false positive, two quick POSTs collapse into a no-op and the flush is skipped (TASK-564); wait for the flush log line.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The settings.ini writer helpers now own the JSON separator (',\n' before every entry but the first, reset by writeSettings()); the 187 per-call withComma flags and the roster loop's own printf are gone. evaluate.py check_settings_writer_separator FAILs on any comma flag or direct file write in writeSettings() (red on old code and on the alpha.432 regression, green on alpha.433). Bench: output byte-identical to alpha.431 for the same settings, valid JSON after each write, REST 176/176 unchanged, restore path works. alpha.432 (roster regression) superseded by alpha.433.
<!-- SECTION:FINAL_SUMMARY:END -->
