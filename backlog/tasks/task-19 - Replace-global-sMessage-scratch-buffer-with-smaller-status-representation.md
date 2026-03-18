---
id: TASK-19
title: Replace global sMessage scratch buffer with smaller status representation
status: In Progress
assignee:
  - '@github-copilot'
created_date: '2026-03-18 19:44'
updated_date: '2026-03-18 20:12'
labels:
  - memory api
dependencies: []
references:
  - 'src/OTGW-firmware/OTGW-firmware.h:277'
  - 'src/OTGW-firmware/helperStuff.ino:571'
  - 'src/OTGW-firmware/OTGW-Core.ino:2474'
  - 'src/OTGW-firmware/restAPI.ino:910'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The firmware keeps a global sMessage[257] buffer in persistent RAM for API/status text. Current usage appears limited to a few status messages such as LittleFS mismatch warnings and PS=1 mode notes. This task evaluates replacing the global mutable buffer with a smaller bounded message, an enum + PROGMEM lookup, or a more targeted per-feature representation. The target is to recover about 257 bytes of persistent RAM with minimal behavioral change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Persistent RAM used for global status messaging is reduced by at least 200 bytes
- [ ] #2 REST API message fields still return meaningful values where currently expected
- [ ] #3 LittleFS mismatch and PS=1 status reporting continue to work correctly
- [ ] #4 No new heap allocations are introduced in the replacement path
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Replaced global sMessage[257] with a one-byte StatusMessage enum stored in runtime state.
- Added getStatusMessageText() to render the REST message directly from PROGMEM strings.
- Updated LittleFS mismatch and PS mode call sites to set and clear status codes instead of mutating a shared RAM buffer.

- Verified with python build.py --firmware; build completed successfully on ESP8266 core 2.7.4.
- No new file-level diagnostics were reported in the touched source files after the refactor.
<!-- SECTION:NOTES:END -->
