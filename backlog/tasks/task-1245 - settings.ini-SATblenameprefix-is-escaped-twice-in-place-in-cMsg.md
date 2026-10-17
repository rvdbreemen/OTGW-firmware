---
id: TASK-1245
title: 'settings.ini: SATblenameprefix is escaped twice, in place in cMsg'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-17 21:53'
updated_date: '2026-10-17 22:06'
labels:
  - bug
  - settings
dependencies: []
ordinal: 383000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Code reading during TASK-1244, not reproduced. writeSettings() escapes settings.sat.sBleNamePrefix into cMsg and then passes cMsg to writeJsonStringKV, which escapes cMsg into cMsg again (src == dest). escapeJsonStringTo (jsonStuff.ino:14) is not safe in place: an expanding escape writes ahead of the read pointer. A prefix with a quote or backslash would be written double-escaped or corrupted. Plain prefixes are unaffected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench: a prefix with a quote shows the defect in settings.ini on the old firmware
- [x] #2 Fix passes the raw prefix to writeJsonStringKV; same prefix round-trips through settings.ini and REST unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-18, Classic-S3 COM8/.88.63, esp32-combo, probe .tmp/bench-1245/set_probe.py (waits for the flush log line). OLD alpha.433: POST SATblenameprefix='AB"C\D' -> file line 'AB' + hundreds of backslashes (the in-place escape runs away until cMsg is full; old-a433.ini), REST still showed the value, but after /ReBoot the prefix was '' (line longer than the reader's 256-byte lineBuf, parse fails). Worse than the description assumed: silent loss of the setting, not just double escaping. FIX alpha.434+a6e7b71: file line "AB\"C\D", json.load OK, value survives /ReBoot unchanged (fix-a434.ini). Reset to '' -> settings.ini byte-identical to the alpha.433 baseline, REST 176/176 no diffs. Gate: evaluate --quick FAILED on the old code (escapeJsonStringTo inside writeSettings), PASS on the fix; tests/test_evaluate.py OK. Build combo green.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SATblenameprefix is no longer escaped in place: writeSettings() passes the raw prefix and writeJsonStringKV escapes it once. Bench old-vs-fix: a prefix with a quote and backslash became hundreds of backslashes and was empty after a reboot on alpha.433; on alpha.434 it is written correctly, parses as JSON and survives the reboot. The evaluate.py settings.ini gate now also fails on any escapeJsonStringTo call inside writeSettings(). alpha.434.
<!-- SECTION:FINAL_SUMMARY:END -->
