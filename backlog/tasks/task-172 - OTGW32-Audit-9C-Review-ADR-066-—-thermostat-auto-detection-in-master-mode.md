---
id: TASK-172
title: 'OTGW32-Audit-9C: Review ADR-066 — thermostat auto-detection in master mode'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:24'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-9
milestone: m-1
dependencies: []
references:
  - docs/adr/ADR-066-thermostat-auto-detection-master-mode.md
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review ADR-066 (thermostat auto-detection master mode) against the current implementation. Verify the auto-detection logic, mode switching behavior, and any related state management are accurately described and correctly implemented.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-066 is read and compared against current implementation
- [x] #2 Auto-detection logic in code matches ADR description
- [x] #3 Mode switching on thermostat detect/loss matches ADR
- [x] #4 ADR is updated if implementation has evolved
- [x] #5 Any gap results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-066 vs OTDirect.ino review:

Auto-detect logic: ADR says "if bAutoDetect enabled and mode is Gateway, wait 5s for thermostat frames". Code matches exactly: settings.otd.bAutoDetect && otCurrentMode == OTD_MODE_GATEWAY gate, 5000ms loop, otSlave.process() polling, feedWatchDog() inside loop.

Mode switch on no-detect: setOTDirectMode(OTD_MODE_MASTER) called if !thermostatFound. Matches ADR.

otBoilerCache[128]: present as static uint16_t[128] + bool otBoilerCacheValid[128]. ADR says 128×4 bytes; actual is 128×2 (uint16_t) + 128×1 (bool) = 384 bytes. ADR estimate of 512 bytes is slightly off but close.

bEnableSlave toggle: settings.otd.bEnableSlave, controls slave start in initOTDirect() and setOTDirectMode(OTD_MODE_MASTER). Matches ADR.

bAutoDetect persistence: settings.otd.bAutoDetect — matches ADR.

handleMasterModeSlaveFrame(): implemented. Responds to READ_DATA with cached values, WRITE_DATA with WRITE_ACK, forwards TSet/TrSet/Tr writes to boiler scheduler. ADR mentions this function by name and it exists.

Virtual boiler response with UNKNOWN_DATAID when cache empty: code uses buildOTResponse(7,...) when !otBoilerCacheValid[msgId]. Matches ADR mitigation.

Runtime thermostat disconnect: handled by checkThermostatTimeout(), called once/second. Not explicitly in ADR-066 but consistent with ADR-064 thermostat tracking state.

Mode switch does NOT happen at runtime if thermostat appears after boot: code comment confirms "user can manually switch to gateway mode via GW=1". Matches ADR trade-off.

Conclusion: ADR-066 accurately describes the implementation. Only minor note: boiler cache is uint16_t not uint32_t (so 384 bytes not 512).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-066 review against OTDirect.ino:

All core decisions match implementation:
- Boot-time 5-second blocking detection loop with feedWatchDog() inside
- setOTDirectMode(OTD_MODE_MASTER) called on no-thermostat-detected path
- otBoilerCache[128] + otBoilerCacheValid[128] present and used in handleMasterModeSlaveFrame()
- bEnableSlave controls slave start/stop in both initOTDirect() and setOTDirectMode(OTD_MODE_MASTER)
- UNKNOWN_DATAID response when cache entry not valid (mitigation for empty-cache risk)
- No runtime auto-switch when thermostat appears after boot — user must use GW=1

One minor documentation correction applied to ADR-066:
- Boiler cache size: was documented as 128x4=512 bytes; actual is 128x2 (uint16_t) + 128x1 (bool) = 384 bytes

No audit-fix task needed.
<!-- SECTION:FINAL_SUMMARY:END -->
