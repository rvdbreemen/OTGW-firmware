---
id: TASK-3145
title: >-
  flash_esp.py: the refusal message (no partition table with the
  firmware/filesystem image) goes to stderr after the 'Flashing...' header and
  is missing from a combined log
status: To Do
assignee: []
created_date: '2026-11-27T22:24:00Z'
labels:
  - tooling
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1191000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-11-27 on the ADR-298 guard test A2 (beta.482+ba0857b, .76 on the old table, otgw-bench-tester): 'flash_esp.py --board esp32 -p COM4 -f <new ino> -s <new littlefs> -y' exits 1 as intended (nothing written: table, NVS-independent unit state and fwversion unchanged), but the refusal text ('Refusing to write the firmware or filesystem image without a partition table: this tool cannot check which table the unit has...') is written to stderr AFTER the stdout 'Flashing Nodoshop OTGW32 (ESP32-S3)' header. With stdout and stderr merged into one file (2>&1 under Windows buffering) the log ended at the header with exit 1 and no reason; only a separate stderr capture showed the message. A tester or a CI log reading only stdout sees a silent exit 1. Fix: print the refusal on stdout as well (or flush both in order), and use the same wording as flash_otgw (which says 'Run a one-time migration first: --migrate'). Evidence: OTGW-field-data/bench-61/2026-11-28_adr298/run/A2_flash_esp_py.log (merged, no message) and A2_flash_esp_py_run2.log + .err (message on stderr). Small; may ride the Sunday tool merge.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The refusal text of flash_esp.py appears in the stdout log (and stays on stderr), in order after the header; a host test runs the refusal path and asserts the text on stdout, red on the current code
<!-- AC:END -->
