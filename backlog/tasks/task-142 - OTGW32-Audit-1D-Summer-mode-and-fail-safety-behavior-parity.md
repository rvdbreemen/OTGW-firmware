---
id: TASK-142
title: 'OTGW32-Audit-1D: Summer mode and fail safety behavior parity'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:16'
updated_date: '2026-04-08 22:35'
labels:
  - audit
  - otgw32
  - phase-1
milestone: m-1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare SM= (summer mode, bit5 of MsgID 0 master status) and FS= (fail safety on thermostat disconnect) behavior against OT-Thing. Verify persistence of SM= setting across reboots, and that FS=0 correctly disables setback even when thermostat is disconnected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SM=1 sets bit5 of master status in every MsgID 0 frame
- [x] #2 SM= setting is persisted to flash (settings.otd.bSummerMode)
- [x] #3 FS=0 disables thermostat disconnect setback
- [x] #4 FS= setting is persisted to flash (settings.otd.bFailSafe)
- [x] #5 Behavior matches OT-Thing reference; gaps produce audit-fix tasks
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Review SM= command handler in OTDirect.ino
2. Verify SM=1 sets bit5 of master status in MsgID 0
3. Verify SM= persistence (settings.otd.bSummerMode)
4. Review FS= command handler
5. Verify FS=0 disables setback
6. Verify FS= persistence (settings.otd.bFailSafe)
7. Compare against OT-Thing summer mode (boilerConfig.summerMode in buildSetBoilerStatusRequest)
8. Document findings and check ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SUMMER MODE (SM=) ANALYSIS:

OT-Thing: boilerConfig.summerMode is passed as parameter 6 of buildSetBoilerStatusRequest() (otcontrol.cpp:509). Set via JSON config. This sets bit5 of the MsgID 0 master status byte, which tells the boiler summer mode is active (CH lockout).

OTDirect.ino:
- SM= handler (line 1789): if value==1, sets otMasterStatusFlags |= 0x20 (bit5) and otSummerMode=true; if 0, clears. Sets settings.otd.bSummerMode = otSummerMode.
- On init (line 489): otSummerMode restored from settings.otd.bSummerMode; if true, immediately sets bit5: otMasterStatusFlags |= 0x20
- buildStatusRequest() uses otMasterStatusFlags which includes bit5, so every MsgID 0 frame carries summer mode bit.
- CONFIRMED: bit5 set in every MsgID 0, persisted, restored on boot.

OT-Thing vs OTDirect summer mode:
- OT-Thing: no SM= command, configured via web portal JSON config (boilerConfig.summerMode)
- OTDirect: SM= command, persisted to flash, PR= queryable
- OTDirect has a more complete implementation (PIC-compatible command interface + persistence)

FAIL SAFETY (FS=) ANALYSIS:

OT-Thing: NO equivalent. OT-Thing does not have thermostat timeout or fail-safe setback.

OTDirect.ino:
- FS= handler (line 2306): if value==1, otFailSafeEnabled=true, settings.otd.bFailSafe=true; if 0, false.
- On init (line 491): otFailSafeEnabled restored from settings.otd.bFailSafe
- In checkThermostatTimeout() (line 1453): guard is "if (otFailSafeEnabled && gateway mode && !otSetbackEngaged)"
- FS=0: guard fails, setback never applied even if thermostat disconnects.
- FS=1 (default): setback applied when thermostat absent for iSetbackTimeout seconds.
- State tracking (bThermostatConnected) continues regardless of FS= value.
- CONFIRMED: FS=0 cleanly disables setback, FS=1 enables, persisted to flash.

VERIFICATION RESULTS:
1. SM=1 sets bit5 of master status: CONFIRMED (otMasterStatusFlags |= 0x20)
2. SM= persisted to flash: CONFIRMED (settings.otd.bSummerMode)
3. FS=0 disables setback: CONFIRMED (checkThermostatTimeout() guard)
4. FS= persisted to flash: CONFIRMED (settings.otd.bFailSafe)
5. Matches OT-Thing: OT-Thing uses same bit5 for summer mode; OTDirect has MORE (FS= is OTDirect-only)

No gaps found. All ACs satisfied.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Verified summer mode (SM=) and fail safety (FS=) in OTDirect.ino against OT-Thing.

SUMMER MODE:
- OT-Thing uses boilerConfig.summerMode in buildSetBoilerStatusRequest() (same bit5), configured via web JSON.
- OTDirect has SM= command: sets bit5 of otMasterStatusFlags, persisted to settings.otd.bSummerMode, restored on init. Every MsgID 0 frame via buildStatusRequest() carries the bit. Full parity plus command-interface advantage.

FAIL SAFETY:
- OT-Thing has no equivalent (no thermostat timeout mechanism at all).
- OTDirect FS= command toggles otFailSafeEnabled, persisted to settings.otd.bFailSafe. When FS=0, checkThermostatTimeout() skips setback engagement entirely. State tracking continues.

All ACs satisfied. No gaps vs OT-Thing. No audit-fix tasks created.
<!-- SECTION:FINAL_SUMMARY:END -->
