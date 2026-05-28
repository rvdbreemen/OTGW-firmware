---
id: TASK-746
title: >-
  ESP abstraction Tier 6: promote platform.h / boards.h / platform_*.h into
  src/libraries/Platform/
status: To Do
assignee: []
created_date: '2026-05-28 08:29'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 73000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Today the platform abstraction lives in src/OTGW-firmware/ as application-tier neighbour headers. Promote them into a real library at src/libraries/Platform/ (with library.properties) so the boundary is physical, not just nominal. Application code includes <Platform.h> instead of platform.h. Verify the relocated files still work for both compilation paths (Arduino IDE concat + PlatformIO if used). Update the abstraction guardrail in evaluate.py to allowlist src/libraries/Platform/ as the new home. This is purely structural - no logic changes. Depends on Tier 5.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 src/libraries/Platform/ exists with platform.h, platform_esp8266.h, platform_esp32.h, boards.h and a minimal library.properties
- [ ] #2 All application code includes through the library path (no relative #include from src/OTGW-firmware)
- [ ] #3 evaluate.py guardrail allowlist updated to src/libraries/Platform/*.h instead of src/OTGW-firmware/platform*.h
- [ ] #4 python build.py --firmware exits 0 on both platforms
- [ ] #5 An ADR documents the move and the abstraction-layer contract
<!-- AC:END -->
