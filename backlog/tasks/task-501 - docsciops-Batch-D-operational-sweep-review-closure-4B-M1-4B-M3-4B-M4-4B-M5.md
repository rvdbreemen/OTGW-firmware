---
id: TASK-501
title: >-
  docs(ci,ops): Batch D operational sweep + review closure (4B-M1, 4B-M3, 4B-M4,
  4B-M5)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-01 06:38'
labels:
  - follow-up
  - code-review
  - ci
  - ops
  - docs
dependencies:
  - TASK-500
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Operational Mediums from .full-review/06-followup-plan.md Batch D: weekly dependency-vulnerability scan workflow, SHA256SUMS helper script + MANUAL.md release-verification section, OTA single-slot truth correction in c4-container.md, hardware-verification log template under docs/templates/. Plus the closure summary at .full-review/07-closure-summary.md. Executed via three parallel implementer agents (D1/D2/D3) plus one closure-doc agent (F1).
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed in commit cbf713f9 docs(ci,ops,release): Batch D operational sweep + review closure (TASK-501).

Closed:
- 4B-M1 OTA partition truth: docs/c4/c4-container.md corrected. Prior text claimed "ESP32 has two OTA app slots for safe rollback"; partitions_otgw_esp32.csv is single-slot (one app0 at 1.875 MB, 2 MB LittleFS, 64 KB coredump). No second app slot to roll back to; failed image = USB reflash. True two-slot OTA needs 8 MB-flash board.
- 4B-M3 dependency visibility: .github/workflows/dependency-scan.yml — Mon 06:00 UTC weekly schedule + opt-in PR trigger on platformio.ini changes + workflow_dispatch. Visibility-only by design (some deps pinned for ADR-documented compatibility). Output: artefact bundle of pio pkg outdated + pio pkg list logs + markdown summary; weekly runs render in $GITHUB_STEP_SUMMARY.
- 4B-M4 release artefact integrity: tools/generate_release_sha256sums.py — stdlib-only Python helper, 88 lines, glob *.bin/*.zip/*.tar.gz/*.elf, 8 KB streaming chunks. Plus docs/MANUAL.md "Release artefact verification" section with sha256sum -c (Linux/macOS) and Get-FileHash (Windows PowerShell) recipes.
- 4B-M5 hardware verification template: docs/templates/hardware-verification-log.md — 119 lines, header + test environment + 12-row pre-filled AC matrix (BLE / OTDirect / MQTT / OTA / telnet soak / web UI / cross-browser / settings / multi-zone SAT) + field telemetry + sign-off + post-merge actions checklist.

Plus: .full-review/07-closure-summary.md (273 lines) — comprehensive review closure summary by phase, severity, and disposition. Cross-references each Medium to its closing TASK + commit SHA, lists deferred items with rationale, lists hardware-verification ACs still needing owner sign-off.

Execution: three parallel sub-agents (D1 dependency-scan, D2 SHA256SUMS, D3 hw-verify template) + one closure-doc agent (F1) ran concurrently with disjoint file ownership. Each verified its own scope; foreground integration build confirmed both targets clean.

Verification:
- pio run -e esp32 -j 1 SUCCESS (exit 0, integration tree)
- pio run -e nodemcuv2 -j 1 SUCCESS (exit 0, integration tree)
- Pushed: cbf713f9 → origin/feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:FINAL_SUMMARY:END -->
