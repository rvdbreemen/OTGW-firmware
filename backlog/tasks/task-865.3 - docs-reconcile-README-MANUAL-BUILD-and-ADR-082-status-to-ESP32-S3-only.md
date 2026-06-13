---
id: TASK-865.3
title: 'docs: reconcile README/MANUAL/BUILD and ADR-082 status to ESP32-S3-only'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-13 05:42'
updated_date: '2026-06-13 09:18'
labels:
  - async-esp32s3
dependencies: []
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (docs reconciliation, ADR-128 defers this; docs-only, no bump)
README/MANUAL/BUILD say ESP8266 "alongside" ESP32; reconcile to ESP32-S3-only. Keep a short "Migrating from ESP8266 (1.x)" callout (1.x = migrate-from baseline, S3 mini drop-in swap). Frame as "no longer a 2.0.0 build target", not "never existed".

## What
- README.md: lines 7,13 (drop "alongside the established ESP8266 path"),25,33,63,77,79-80,156 (`pio run -e esp8266`->esp32),326,328.
- docs/MANUAL.md: 11,13,19,27,53,65,69,106,109 + esp8266 upload examples (267-268,1583-1586,3591). Keep 5GHz/2.4GHz note, reattach to ESP32-S3.
- docs/guides/BUILD.md: 129 (esp8266 target example).
- docs/adr/ADR-082-*: Status line only -> "Superseded by ADR-128, 2026-06-12" (already done on this branch via b89b8b4a; verify, no-op if present).

## Acceptance Criteria
- evaluator: grep finds no "alongside the established ESP8266" in README/MANUAL.
- evaluator: grep finds no `pio run -e esp8266`/`--target esp8266` in README/MANUAL/BUILD.md; commands match seq1 default_envs.
- grep: ADR-082 Status == "Superseded by ADR-128, 2026-06-12", no other ADR-082 line changed.
- build: docs-only diff (only *.md), no src/ touched -> no prerelease bump.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Landed docs reconciliation to ESP32-S3-only: README.md, docs/MANUAL.md, docs/guides/BUILD.md. All four binding ACs verified empirically (no 'alongside the established ESP8266'; no 'pio run -e esp8266'/'--target esp8266'; ADR-082 Status='Superseded by ADR-128, 2026-06-12'; docs-only diff, no src/ touched, no prerelease bump). No field-validation ACs remain. Non-blocking follow-up recommended: scrub docs/MANUAL.md Appendix H 'PlatformIO Environment Summary' + dual-platform prose (lines ~1545/1599/3477/3550) which still reference the deleted esp8266 env but trip no binding gate.
<!-- SECTION:NOTES:END -->
