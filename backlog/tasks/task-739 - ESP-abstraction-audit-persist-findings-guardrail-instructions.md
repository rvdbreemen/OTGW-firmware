---
id: TASK-739
title: 'ESP abstraction audit: persist findings, guardrail, instructions'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-28 08:27'
updated_date: '2026-05-28 08:37'
labels: []
dependencies: []
ordinal: 66000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Persist the senior ESP-IoT audit findings of the abstraction layer leaks identified across the 2.0.0 feature branch. Write the audit document, add a build-time guardrail in evaluate.py, update CLAUDE.md/AGENTS.md with the no-#ifdef-outside-abstraction rule. The remediation tier tasks (Tier 0-6) are created separately and tracked as siblings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Audit document persisted at docs/audits/esp-abstraction-leak-audit-YYYY-MM-DD.md with full findings, categories, file:line references, and prioritised tier roadmap
- [x] #2 CLAUDE.md gains an ESP Platform Abstraction rule under Critical Coding Rules clearly stating: no #ifdef ESP8266/ESP32/ARDUINO_ARCH_ESP* outside abstraction layer; call platform*() shims or use HAS_* board flags
- [x] #3 AGENTS.md gains the equivalent rule in its Architecture rules section
- [x] #4 evaluate.py adds a check that fails on new #if(def) ESP8266/ESP32/ARDUINO_ARCH_ESP* lines outside the abstraction file allowlist; existing violations enumerated in the audit are baselined so existing code does not break the gate
- [x] #5 Existing-violation baseline is documented (count + file list) so the count can only decrease over time
- [ ] #6 python build.py --firmware exits 0 on this branch
- [x] #7 python evaluate.py --quick exits 0 (no new failures beyond the documented baseline)
- [ ] #8 Branch pushed to origin and draft PR created
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Map abstraction layer surface (platform.h / platform_esp*.h / boards.h / OTGW-ModUpdateServer trio).
2. Enumerate every #if/#ifdef ESP8266/ESP32/ARDUINO_ARCH_ESP*/BOARD_NODOSHOP_ESP* outside the allowlist; capture file:line:context.
3. Group findings by remediation pattern (file-level feature modules, struct-field divergence, tuning constants, missing shims, missing stubs, board-macro leaks, OLED FreeRTOS coupling, shim-bypass, type-ambiguity overloads).
4. Write docs/audits/2026-05-28-esp-abstraction-leak-audit.md with full findings and tier roadmap.
5. Create Tier 0..6 backlog tasks each scoped to a single remediation theme.
6. Implement evaluate.py::check_esp_abstraction_boundary() with baseline ratchet (FAIL on regression, WARN until zero, PASS at zero).
7. Add ESP Platform Abstraction rule to CLAUDE.md (Critical Coding Rules) and AGENTS.md (Architecture rules).
8. Verify guardrail: import-test, dry-run check, simulated regression FAIL.
9. Commit, push, draft PR.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-28: Audit complete. Baseline locked at 78 violations across 18 files (commit 9be88a0d).

Deliverables landed:
- docs/audits/2026-05-28-esp-abstraction-leak-audit.md
- evaluate.py: scan_esp_abstraction_violations() + check_esp_abstraction_boundary() + ESP_ABSTRACTION_BASELINE constant + allowlist + wiring into evaluate_all essentials
- CLAUDE.md: ESP Platform Abstraction section under Critical Coding Rules
- AGENTS.md: matching ESP Platform Abstraction section under Architecture rules
- TASK-740..746: Tier 0..6 remediation tasks (To Do)

Validation:
- python3 evaluate.py --quick: 64 PASS, 1 WARN (the new gate at baseline), 0 FAIL, 7 INFO. Health 98.6%, exit 0.
- Simulated regression (BASELINE=77 with count=78): gate FAILs as designed.
- Cannot run python build.py --firmware in this sandbox (outbound SSL to github.com blocked for esp32-3.3.5.tar.xz toolchain fetch). All changes are docs + Python lint, no firmware C/C++ touched.
<!-- SECTION:NOTES:END -->
