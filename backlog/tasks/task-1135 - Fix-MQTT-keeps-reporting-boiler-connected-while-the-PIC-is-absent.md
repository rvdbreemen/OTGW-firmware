---
id: TASK-1135
title: 'Fix: MQTT keeps reporting boiler connected while the PIC is absent'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-17 20:21'
updated_date: '2026-09-18 04:53'
labels:
  - bug
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning / tranquil_kiwi_32924 / 2026-09-17'
priority: medium
ordinal: 218000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tranquil_kiwi_32924 in Discord #nederlandse-ondersteuning, 2026-09-17. On 1.7.4 with PIC 6.7 the web interface reported no PIC present while MQTT kept publishing that the boiler was connected, with both the boiler and the thermostat physically disconnected.

The web-side half (PIC detection never recovering under diagnose or interface firmware) is TASK-1126, shipped in 1.7.6-beta.2. This task covers the MQTT half.

CAUSE CORRECTED 2026-09-18 after reading the code. The original description said the flags are not retracted because contrary evidence never arrives, by analogy with the unsupported_msgids bug of GH #677. That is wrong and would have produced a fix against a cause that does not exist. Two independent defects:

1. The liveness timeout is only evaluated when a message arrives. OTGW-Core.ino:4198 computes state.otgw.bBoilerState = (now < (epochBoilerlastseen+30)) inside the OT-message processing path, and line 4205 does the same for the thermostat. If the PIC stops delivering entirely, that code never runs, so the 30 second timeout cannot fire in exactly the case it exists for. bBoilerState keeps its last value until reboot, and bOnline with it.

2. Both publish paths are gated on PIC presence. The on-change publishes at OTGW-Core.ino:4200, 4208 and 4219 sit behind if (isPICEnabled()), and the 5-minute heartbeat sendMQTTstateinformation() (MQTTstuff.ino:1291) opens with if (!isPICEnabled()) return. A gateway whose PIC is gone therefore publishes nothing on these topics at all, even if the RAM state were correct.

Retention is NOT involved: sendMQTTData takes retain = false by default (OTGW-firmware.h:174), so no retained true is pinned on the broker. Home Assistant keeps showing connected because nothing ever contradicts it and the heartbeat that would is gated off.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With no PIC detected, the boiler/thermostat connected topics publish false rather than retaining their last true value
- [ ] #2 Reproduced before the fix and verified after, on a device with the PIC absent
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures
- [ ] #5 The liveness timeout is evaluated on a periodic tick independent of message arrival, so a bus that goes completely silent still flips to false within roughly the 30s window
- [ ] #6 Evaluation is suppressed while the PIC is being flashed, so a PIC update does not flap the entities
- [ ] #7 The thermostat transition keeps its coupled publishHvacMode(false)/publishHvacAction(false) calls, so the HA climate entity does not hold a stale mode
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify the premises in code before writing anything (done: the recorded cause was wrong, corrected in the description).
2. Extract the liveness evaluation out of processOT() into evaluateOTBusLiveness(), promoting the five statics to file scope.
3. Call it from processOT() (frame arrived) and from doTaskEvery3s() (silence), above that function early return.
4. Drop the isPICEnabled() gate for the three bus-presence values in both the on-change path and the 5-minute heartbeat; keep it for gateway_mode.
5. Suppress evaluation while isFlashing().
6. Build, evaluate, host tests, commit. Hardware verification stays with the maintainer.
<!-- SECTION:PLAN:END -->
