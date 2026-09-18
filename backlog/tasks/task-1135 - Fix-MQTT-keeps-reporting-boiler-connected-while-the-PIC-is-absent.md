---
id: TASK-1135
title: 'Fix: MQTT keeps reporting boiler connected while the PIC is absent'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-17 20:21'
updated_date: '2026-09-18 05:10'
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
- [x] #1 With no PIC detected, the boiler/thermostat connected topics publish false rather than retaining their last true value
- [ ] #2 Reproduced before the fix and verified after, on a device with the PIC absent
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
- [x] #5 The liveness timeout is evaluated on a periodic tick independent of message arrival, so a bus that goes completely silent still flips to false within roughly the 30s window
- [x] #6 Evaluation is suppressed while the PIC is being flashed, so a PIC update does not flap the entities
- [x] #7 The thermostat transition keeps its coupled publishHvacMode(false)/publishHvacAction(false) calls, so the HA climate entity does not hold a stale mode
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in 020e8198.

Design choices and why:
- Reused the existing doTaskEvery3s() slot instead of adding a timer. 3 s against a 30 s window is ample and costs no new timer state. The call sits ABOVE that function early return on picSettingsCycleActive, which would otherwise skip it most of the time.
- The five liveness statics (epochBoilerlastseen, epochThermostatlastseen and the three previous-state flags) moved from processOT() locals to file scope in OTGW-Core.ino rather than into OTGWState. Smaller surface, and it keeps them out of the state struct that an open RAM audit is already looking at.
- otgw_connected is published ungated together with the other two. It is derived from them (bOnline = bBoilerState || bThermostatState), so gating it on PIC presence while its inputs are ungated would make the heartbeat contradict the on-change path.
- isFlashing() is checked inside evaluateOTBusLiveness() even though the doTaskEvery3s() caller is already inside an if (!isFlashing()) block, because processOT() can also reach it.

Gates: build.bat green with fresh artifacts; evaluate.py --quick 35/37, 0 failed; run_tests.bat 51 checks 0 failures; adr-judge 0 violations.

Host test added in 5e668887.

The decision moved into src/OTGW-firmware/otBusLiveness.h as evalOtBusLiveness(), a pure function of the clock, the two last-seen stamps, the previously published values, a force-publish flag and a flashing flag. It returns the three presence values plus which ones the caller must publish. evaluateOTBusLiveness() now only applies and publishes that verdict.

Flash suppression deliberately moved INTO the verdict instead of being an early return in the caller. As an early return it was untestable; as part of the verdict it is AC6 covered by test.

test/host/test_otBusLiveness.cpp, 23 checks: clock movement alone flips the verdict with no new frame, the exact boundary (now < lastSeen + 30, so exactly 30 counts as gone), the two sides independent with bOnline as their OR, a gateway that never heard anything, the flash freeze including that force-publish does not punch through and that absence is preserved as faithfully as presence, and a 100-tick silent run that converges to absent and publishes exactly once.

HONEST LIMIT, do not let this read as more than it is: the tests pin the DECISION, not the WIRING. The original defect was that the decision was only reachable from the message path, which is a call-site property no unit test of this function can observe. A test suite that is green here would still be green if someone deleted the evaluateOTBusLiveness(false) call from doTaskEvery3s(). That call site is guarded by review only. An evaluator rule asserting a periodic caller exists would close it; not built.

Suite is now 74 checks over four files. evaluate.py --quick picked the new file up by itself: 38 checks, 36 pass, 0 failed.
<!-- SECTION:NOTES:END -->
