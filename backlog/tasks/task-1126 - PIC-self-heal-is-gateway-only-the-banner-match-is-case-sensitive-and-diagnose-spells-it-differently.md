---
id: TASK-1126
title: >-
  PIC self-heal is gateway-only: the banner match is case-sensitive and diagnose
  spells it differently
status: Done
assignee: []
created_date: '2026-09-04 21:22'
updated_date: '2026-09-05 08:07'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 214000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the diagnose-firmware design analysis (docs/plan/DIAGNOSE_FIRMWARE_SUPPORT_PLAN.md, Stage 0). Independent of that feature and worth fixing on its own.\n\nThe only runtime path that re-enables state.pic.bAvailable is in processOT(): OTGW-Core.ino:4562 does strstr(buf, OTGW_BANNER), with OTGW_BANNER defined at :474 as "OpenTherm Gateway". strstr is case-sensitive. The diagnose firmware announces itself as "Opentherm gateway diagnostics - Version " (src/libraries/OTGWSerial/OTGWSerial.cpp:97), with a lowercase g, and the interface firmware as "OpenTherm Interface " (:98). Neither can ever match, so neither can ever re-enable bAvailable.\n\nMeanwhile fwreportinfo() (OTGW-Core.ino:5137), the banner callback registered by detectPIC(), fires for all three firmware types and writes sFwversion, sDeviceid and sType, but never touches bAvailable. So the firmware type self-heals for every PIC and the availability flag self-heals only for a gateway.\n\nConfirmed on hardware 2026-09-04 (bench unit running diagnose 2.2). The diagnose banner is visibly falling through to the catch-all branch: processOT(4608): Not processed, received from OTGW => (Opentherm gateway diagnostics - Version 2.2) [43].\n\nSeverity is an asymmetry, not a live outage. bAvailable is still set once by the find(ETX) probe in detectPIC() (OTGW-Core.ino:574), and that probe does succeed against diagnose.hex: the same bench unit reports picavailable true with picfwtype diagnose. ETX comes from the bootloader, so it is firmware-independent. The risk is that if the probe ever misses, nothing re-enables the flag for a non-gateway PIC, and every PIC route answers 503 permanently, including /pic upgradepic() (FSexplorer.ino:264, gated via isPICEnabled()), which is the only way to flash gateway.hex back.\n\nProposed fix, in fwreportinfo() alongside the existing strlcpy calls: if bAvailable is false, set it true and log it. That generalises the gateway-only self-heal at :4563-4566 to every banner type, at the one place that already sees all three. About 40 bytes of flash, no RAM.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 fwreportinfo() sets state.pic.bAvailable when it is false, so a banner from any of the three firmware types re-enables PIC functions
- [x] #2 No other behaviour change in fwreportinfo(): sendMQTTversioninfo() is still called exactly once per banner
- [ ] #3 On a gateway PIC, picavailable stays true across boot and across a PIC reflash
- [x] #4 On a diagnose PIC, picavailable is true and picfwtype is diagnose, verified on hardware
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-05: implemented in fwreportinfo() and shipped alongside TASK-1127. Verified on the bench unit running diagnose 2.2: picavailable stays true and picfwtype stays diagnose across the deploy and across repeated banner redraws.

AC #3 (gateway PIC keeps picavailable true across boot and across a PIC reflash) is NOT checked: no gateway PIC was available, and reflashing one needs explicit authorisation. The change only ever ADDS a set of bAvailable when it is false, and touches nothing else, so it cannot take availability away from a gateway.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A banner from any PIC firmware type now re-enables PIC functions, not just a gateway banner.

The only runtime path that set state.pic.bAvailable matched OTGW_BANNER ("OpenTherm Gateway") with a case-sensitive strstr. The diagnose PIC says "Opentherm gateway diagnostics" with a lowercase g and the interface PIC says "OpenTherm Interface", so neither could ever match. Meanwhile fwreportinfo(), the banner callback that fires for all three types, already maintained the firmware type but never touched the availability flag. The type self-healed for every PIC; availability self-healed only for a gateway.

Severity was an asymmetry rather than an outage: the find(ETX) probe in detectPIC() does succeed against diagnose.hex, demonstrated by the bench unit reporting picavailable true. The risk was that one missed ETX would be permanent for a non-gateway PIC, leaving every PIC route on 503 including the /pic route that is the only way back to gateway.hex.

Found as Stage 0 of the diagnose-firmware design analysis, kept as its own change because it stands alone.
<!-- SECTION:FINAL_SUMMARY:END -->
