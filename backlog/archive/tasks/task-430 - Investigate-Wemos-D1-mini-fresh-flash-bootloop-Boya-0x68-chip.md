---
id: TASK-430
title: 'Investigate: Wemos D1 mini fresh flash bootloop (Boya 0x68 chip)'
status: To Do
assignee: []
created_date: '2026-04-26 09:57'
updated_date: '2026-04-30 00:46'
labels:
  - bug
  - needs-info
  - rescued
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/554'
priority: high
ordinal: 6000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter ArnoudPJ (GitHub issue #554) reports that OTGW-firmware 1.3.5 bootloops when flashed fresh onto a Wemos D1 mini with a Boya flash chip (manufacturer 0x68). Flashing v1.2 first and then OTA-upgrading works. The maintainer is already engaged in the thread asking for diagnostic info.

## Hardware and reproduction context

- Hardware: Wemos D1 mini classic, ESP8266EX, 4MB flash, Boya chip (mfr 0x68), OTGW PCB v2.12
- esptool command used: `--baud 9600 --no-stub write_flash --flash_mode qio --flash_size 4MB --flash_freq 40m 0x0 firmware.bin 0x200000 littlefs.bin`
- Serial output: `rst cause:2` repeating (bootloop at ROM stage, never reaches firmware)

## Known suspects

- Boya 0x68 chips often require `--flash_mode dout/dio` instead of `qio`
- `0x200000` is the 1.4.x LittleFS offset (2 MB FS); 1.3.5 uses 1 MB FS at `0x300000`. Mismatched offset would explain the bootloop on 1.3.5 specifically.
- Maintainer already engaged on GitHub; waiting for reporter to confirm 1.4.1 direct-flash result.
- Related stale branch: `copilot/fix-wemos-d1-bootloop-issue` (2026-01-28, unmerged).

## Provenance

This task was rescued on 2026-04-26 from an abandoned local worktree at `.claude/worktrees/modest-noether-582eda` (branch `claude/modest-noether-582eda`, last touched 2026-04-23). The original carried task ID 388 in the worktree's local backlog/, but TASK-388 was assigned in the main branch to a different task (`Fix-MQTT-binary_sensor-discovery-via-flag-driven-otgw-pic-prefix`). Renumbering to the next free ID was the cleanest resolution. The worktree itself was removed in the same cleanup commit; this task preserves the substantive investigation.

Related main backlog task: TASK-384 (Fix v1.3.5 bootloop on fresh flash to Wemos D1 — same hardware family, possibly same root cause).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Confirm whether 1.4.1 direct fresh flash also bootloops on this hardware (waiting on GitHub #554 reporter feedback)
- [ ] #2 Identify root cause: wrong flash_mode (qio vs dout/dio for Boya chip) and/or wrong littlefs offset (0x200000 used for 1.3.5, which expects 1MB FS at 0x300000)
- [ ] #3 If a firmware or documentation fix is needed, implement and verify it resolves the issue. If duplicate of TASK-384, close as duplicate with cross-reference
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-30 priority bump: cross-reference with TASK-384 reveals a SECOND reporter (dvd77, 2026-04-29) confirming the same bootloop 'rst cause:2, boot mode:(3,7)' across 1.3.5, 1.4.1 AND 1.5.0-beta on a fresh Wemos D1 mini, ESP-only without OTGW PCB attached. This is no longer a single-hardware-batch suspicion. Promoting to high priority. NOT archiving — awaiting serial-during-boot capture from either reporter. Consider opening a unified investigation task that supersedes 384/430 if the third occurrence lands.
<!-- SECTION:NOTES:END -->
