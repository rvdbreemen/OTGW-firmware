---
id: TASK-1126
title: >-
  PIC self-heal is gateway-only: the banner match is case-sensitive and diagnose
  spells it differently
status: To Do
assignee: []
created_date: '2026-09-04 21:22'
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
- [ ] #1 fwreportinfo() sets state.pic.bAvailable when it is false, so a banner from any of the three firmware types re-enables PIC functions
- [ ] #2 No other behaviour change in fwreportinfo(): sendMQTTversioninfo() is still called exactly once per banner
- [ ] #3 On a gateway PIC, picavailable stays true across boot and across a PIC reflash
- [ ] #4 On a diagnose PIC, picavailable is true and picfwtype is diagnose, verified on hardware
<!-- AC:END -->
