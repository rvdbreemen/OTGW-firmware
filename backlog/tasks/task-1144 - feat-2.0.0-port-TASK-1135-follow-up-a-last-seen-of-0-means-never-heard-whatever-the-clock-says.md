---
id: TASK-1144
title: >-
  feat-2.0.0: port TASK-1135 follow-up: a last-seen of 0 means never heard,
  whatever the clock says
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-20 12:12'
updated_date: '2026-09-20 12:12'
labels:
  - port
  - otbus
dependencies: []
priority: medium
ordinal: 283000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the 1.x bench on 2026-09-20 while verifying TASK-1135 on hardware: right after boot, with no OpenTherm frame ever received, boiler_connected, thermostat_connected and otgw_connected read TRUE for about 30 seconds and then fell. Cause: the liveness verdict is `now < lastSeen + 30`. Before NTP sync the ESP clock counts up from 0, so with lastSeen still 0 the window is in the future. Pre-fix builds never evaluated without a frame, so they never showed it.

The 1.x fix (otBusLiveness.h, TASK-1135) adds `lastSeen != 0 &&` to both comparisons, with host test case (g) pinning it.

The 2.0.0 port (TASK-1137, evaluateOTBusLiveness in OTGW-Core.ino around line 4811) uses the same comparison on state.otBus.tBoilerLastSeen / tThermostatLastSeen. On the bench OTGW32 the flags read false 5 s after a flash, which argues the ESP32 clock is not near 0 at that moment, but that is one observation on one boot, not a proof; the guard costs nothing and removes the dependency on what time() returns before sync.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 evaluateOTBusLiveness treats a last-seen stamp of 0 as absent regardless of now, for both the boiler and the thermostat stamp
- [ ] #2 Right after a cold boot with nothing on the bus, /api/v2/device/info reports boilerconnected and thermostatconnected false from the first successful poll, verified on the bench OTGW32
- [ ] #3 Build green for esp32-combo and evaluate.py --quick shows no new failures
<!-- AC:END -->
