---
id: TASK-368
title: >-
  chore(ci): wire evaluate.py into GitHub Actions and add 4 regex gates (buffer,
  cooldown, ADR, VH wrap)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:48'
updated_date: '2026-04-21 17:17'
labels:
  - code-review
  - ci
  - evaluate
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 3A HIGH x4: evaluate.py has the single_caller template (check_time_boundary_single_caller) but is not wired into CI, and 4 additional regex gates would have caught the Phase 1+2 HIGH/CRITICAL findings at commit time. Four new checks of roughly 10-30 lines each, plus a GitHub Actions workflow.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New workflow .github/workflows/evaluate.yml runs python evaluate.py --quick on PRs to dev, 1.4.*, release/** branches
- [x] #2 check_json_buffer_arithmetic added: parses snprintf_P format string, computes worst-case size, fails if sizeof(buffer) insufficient
- [x] #3 check_status_burst_cooldown_bound added: fails if STATUS_BURST_COOLDOWN_MS >= 3000 unless // verified tuning escape hatch on adjacent line
- [x] #4 check_status_publishers_wrap_burst added: every publish(Master|Slave)Status.*State function must contain beginStatusBurst and endStatusBurst
- [x] #5 check_adr_references_resolve added: every ADR-\d{3} citation in src/ or docs/adr/ must resolve to an existing ADR file
- [x] #6 Dead definition_sites local removed from check_time_boundary_single_caller at evaluate.py:183
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Remove dead definition_sites local in check_time_boundary_single_caller (evaluate.py:183 area).
2. Add check_json_buffer_arithmetic scoped to sendMQTTheapdiag in MQTTstuff.ino: parse snprintf_P format, compute worst-case byte count per %lu/%ld/%u/%d/%s tokens plus literal chars, compare against sizeof(buffer).
3. Add check_status_burst_cooldown_bound: regex for STATUS_BURST_COOLDOWN_MS = N in MQTTstuff.ino, fail if N >= 3000 unless a // verified tuning marker appears within the 5 preceding lines.
4. Add check_status_publishers_wrap_burst: for every static void publish(Master|Slave)Status.*State( function in OTGW-Core.ino, walk its body and assert it contains both beginStatusBurst( and endStatusBurst( calls.
5. Add check_adr_references_resolve: scan docs/adr/*.md and src/OTGW-firmware/*.{ino,cpp,h} for ADR-\d{3} matches, assert each ADR-NNN-*.md exists; allow forward-cited references if the surrounding comment or string contains future/proposed/TBD.
6. Wire all five new checks into evaluate_all(). Quick mode runs all five (cheap regex, no I/O heavy work).
7. Create .github/workflows/evaluate.yml: pull_request trigger on dev/1.4.*/release/** branches, ubuntu-latest, actions/checkout@v4, actions/setup-python@v5 python-version 3.x, run python evaluate.py --quick.
8. Run python evaluate.py full, document which gates PASS vs FAIL today.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Five new evaluate.py checks plus one cleanup plus one GitHub Actions workflow:

1. Dead local cleanup: removed definition_sites list from check_time_boundary_single_caller; the definition line is now simply skipped via continue. 4 LOC delta. Gate still PASSes.

2. check_json_buffer_arithmetic (scope: sendMQTTheapdiag in MQTTstuff.ino). Parses snprintf_P(buf, sizeof(buf), PSTR(...)...), handles adjacent-string concatenation inside PSTR(), walks format and budgets: %lu=10, %ld/%d/%i=11, %u=10 worst-case 32-bit, %x/%X/%o=8, %c=1, %s=skipped with warning, %f/%e/%g=24, %%=1 literal, unknown=noted. Buffer extracted from "char X[N]" regex; result is PASS/WARN(< 16B headroom)/FAIL. Current: PASS, 30 bytes headroom (buf=512, worst=481, required=482).

3. check_status_burst_cooldown_bound (MQTTstuff.ino). Regex for STATUS_BURST_COOLDOWN_MS = N, fails if N >= 3000 unless "verified tuning" marker appears within 5 preceding lines. Current: PASS (2000 ms).

4. check_status_publishers_wrap_burst (OTGW-Core.ino). Regex for void publish(Master|Slave)Status\w*State(, body extracted via brace walker, asserts both beginStatusBurst( and endStatusBurst( appear. Current coverage: 4 publishers (Master/Slave for both normal and VH); all PASS.

5. check_adr_references_resolve (docs/adr/*.md + src/OTGW-firmware/*.{ino,cpp,h}). Builds set of existing ADR numbers from ADR-NNN-*.md filenames, scans every target, extracts ADR-\d{3} matches, fails on unresolved. Forward-citation escape: line containing future/proposed/TBD (case-insensitive) is treated as a known placeholder. Current: PASS (902 refs, all resolve -- TASK-355 cleaned up the ghost ADR-077/078/080 citations).

6. .github/workflows/evaluate.yml: pull_request on dev / 1.4.* / release/**, ubuntu-latest, actions/checkout@v4, actions/setup-python@v5 python 3.x, runs python evaluate.py --quick --no-color --report to .tmp/evaluation-report.json, uploads artifact on always(). Matches opentherm-v42-spec-audit.yml layout.

Local verification: python evaluate.py --quick exits 0 with 31 PASS / 0 FAIL / 0 WARN / 2 INFO (100.0% health). Full python evaluate.py exits 0 with 39 PASS / 4 WARN (pre-existing: String class usage, BUILD.md/FLASH_GUIDE.md absent, uncommitted work tree) / 6 INFO.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wired evaluate.py into CI via a new GitHub Actions workflow and added four regex gates that would have caught four of the Phase 1+2 HIGH/CRITICAL findings at commit time, plus one LOW cleanup.

Changes:
- .github/workflows/evaluate.yml (new): pull_request on dev / 1.4.* / release/**, ubuntu-latest, actions/checkout@v4, actions/setup-python@v5 python 3.x, runs python evaluate.py --quick --no-color --report, uploads the JSON report artifact on always(). Mirrors opentherm-v42-spec-audit.yml style.
- evaluate.py: removed dead definition_sites local in check_time_boundary_single_caller (Phase 3A LOW #1); the definition line is now simply skipped via continue.
- evaluate.py: added check_json_buffer_arithmetic scoped to sendMQTTheapdiag. Parses snprintf_P(buf, sizeof(buf), PSTR(...)...), walks the format string, budgets conversions (%lu=10, %ld/%d/%i=11, %u=10 worst-case 32-bit, %x/%X/%o=8, %c=1, %s=skipped with warning, %f/g/e=24, %%=1), sizes the destination buffer from a char X[N] regex, and flags FAIL if worst-case > buffer and WARN if headroom < 16 bytes. Would have caught TASK-352 before 384 was expanded to 512.
- evaluate.py: added check_status_burst_cooldown_bound. Fails if STATUS_BURST_COOLDOWN_MS >= 3000 unless a verified tuning marker appears within 5 preceding lines. Codifies the TASK-353 decision against regressing back to 10000.
- evaluate.py: added check_status_publishers_wrap_burst. Regexes every static void publish(Master|Slave)Status*State function in OTGW-Core.ino, body-walks, and asserts both beginStatusBurst and endStatusBurst appear. Covers 4 publishers (normal + VH, Master + Slave).
- evaluate.py: added check_adr_references_resolve. Builds the set of existing ADR numbers from ADR-NNN-*.md filenames under docs/adr/, scans docs/adr + src/OTGW-firmware for ADR-\d{3} matches, fails on any number without a matching file. Forward-citation escape for lines containing future/proposed/TBD. Would have caught the ADR-077/078/080 ghost citations before TASK-355.
- All five checks wired into evaluate_all() before the not-quick block so they run under --quick too.

Why: evaluate.py had the single_caller template but was never wired into CI. These four additional checks are regex-level (ms to run) and would have caught buffer arithmetic, burst-cooldown tuning regressions, VH status-burst quiesce symmetry, and ghost ADR citations at commit time.

Verification on this tree:
- python evaluate.py --quick exits 0, 31 PASS / 0 FAIL / 0 WARN / 2 INFO, health 100.0%
- python evaluate.py (full) exits 0, 39 PASS / 4 WARN (all pre-existing environmental: 22 String-class usages, missing BUILD.md / FLASH_GUIDE.md, uncommitted work tree) / 6 INFO, health 91.8%
- All four new gates plus the two ADR-062 gates from TASK-364 PASS today: STATUS_BURST_COOLDOWN_MS = 2000 (< 3000 threshold), all 4 VH/non-VH status publishers wrap begin/endStatusBurst, 902 ADR-NNN references resolve, sendMQTTheapdiag has 30 bytes headroom (buf=512, worst=481, required=482).

Risks / follow-ups:
- check_json_buffer_arithmetic is currently scoped only to sendMQTTheapdiag. Widening it to every snprintf_P call would need richer type inference (%s length, varargs argument types); a follow-up task can add that.
- Forward-citation escape in check_adr_references_resolve uses a simple keyword check; a future reviewer could tighten this to an explicit marker like "ADR-NNN (proposed)".
<!-- SECTION:FINAL_SUMMARY:END -->
