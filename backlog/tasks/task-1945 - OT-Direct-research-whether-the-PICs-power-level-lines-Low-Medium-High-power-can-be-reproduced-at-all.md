---
id: TASK-1945
title: >-
  OT-Direct: research whether the PIC's power-level lines (Low/Medium/High
  power) can be reproduced at all
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T18:26:00Z'
updated_date: '2026-11-16T08:38:00Z'
labels:
  - otdirect
  - parity
  - research
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1850 (coordinator, 2026-11-14). The PIC 6.8 prints 'Low power', 'Medium power' and 'High power' when the thermostat-side OpenTherm line level changes (Smart Power; gateway.asm 6.8 G:1532-1551; the firmware event table knows the tokens, OTGW-Core.ino kPICStatusTokens). OT-Direct prints none of them. Research first, no code: can OT-Direct detect the thermostat-side line levels at all? Cite the hardware (the OTGW32 OpenTherm interface: which signals the ESP32 sees, whether a voltage/current level is measurable) and OTDirect.ino (what the receive path exposes). Keep it consistent with TASK-1848 (95), where OT-Direct answers PR=P with a fixed 'Low power' (no Smart Power): if levels cannot be detected, the outcome is 'no lines, PR=P stays Low power' and this task closes with that evidence.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Research note with citations (hardware interface and OTDirect.ino receive path): whether the thermostat-side line level is observable on OT-Direct
- [x] #2 Decision recorded with reasons: the spontaneous power-level lines are built (with a guard for the idle reset) or not built; consistent with TASK-1848's live PR=P level answer
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 d3 research note (AC1). PIC 6.8: the level is the two Smart Power bits ReceiveModeT (tstatflags,0, gateway.asm 6.8 :423) and TransmitMode (tstatflags,4, :426); medium power is seen by timing, not voltage: InputSlow toggles ReceiveModeT when the thermostat line goes high-to-low after about 10 ms (:823-837, digital comparator output ComparatorT); raised transmit inverts the PIC's own thermostat output (:873-874); 20 ms after the last line event PowerChange sets PowerReport (:857-862); ReportPower (:1532-1551) prints Low/Medium/High via PrintPowerlevel when the bits differ from the last report (gpioflags), and skips the report when LineVoltage (ADC, :1607-1735) says the thermostat line is open (:1533-1540). OT-Direct: the thermostat side runs the same state machine in the OpenTherm library (fork pin 51aa68e): rxIdleLevel toggles when a non-idle state lasts > 5 ms (OpenTherm.cpp :504-505, = InputSlow), txIdleLevel toggles on a request (:370-371 15-25 ms pulse while raised, :519-523), and 1.25 s without a frame drops both to low (:494-501). OTDirect.ino exposes them as otSlave.txRaised()/rxInverted() (:42-47, TASK-1848), and PR=P already prints the live level from them (otdPowerLevelText :706-711, beta.252). So the level IS observable in software. NOT verified: whether the OTGW32 board electrically switches the line current; there is no schematic in the repo; the only, indirect source is the board's own OT-Thing firmware, which shows getSmartPowerState() and offers Smart Power High (other-projects/OT-Thing-OTGW32 Firmware/src/otcontrol.cpp :831-835, :1091-1100). Side finding: the library's getSmartPowerState() (:631-635) never returns MEDIUM; TASK-1848 works around it.

2026-11-16 d3 decision (AC2), checked with the advisor: the spontaneous Low/Medium/High power lines are NOT built. Reasons: (1) the live level is already available on request through PR=P (TASK-1848, beta.252), so the information is reachable; (2) without a line-voltage measurement OT-Direct cannot suppress the report on a thermostat unplug the way ReportPower does (:1533-1540): the library's 1.25 s idle reset (:494-501) would print 'Low power' seconds before TASK-1850's 'Thermostat disconnected', a misleading line; (3) the bench has no thermostat, so the lines would stay host-only; (4) the electrical level on the OTGW32 is unverified (AC1). The AC2 wording 'consistent with TASK-1848's fixed Low power answer' was out of date (1848 shipped the live level); AC2 rewritten to the decision taken. If a user needs the spontaneous lines, they are a small change on otSlave.txRaised()/rxInverted() plus a guard on the idle reset.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Research only, no code. AC1: the thermostat-side Smart Power level is observable in software on OT-Direct: the OpenTherm library (fork pin 51aa68e) runs the same two-bit state machine as the PIC (rxIdleLevel = PIC ReceiveModeT via the > 5 ms non-idle toggle, OpenTherm.cpp :504-505 vs gateway.asm 6.8 InputSlow :823-837; txIdleLevel = PIC TransmitMode, :370-371/:519-523 vs :873-874; 1.25 s idle reset :494-501), exposed as otSlave.txRaised()/rxInverted() (OTDirect.ino :42-47), already used by PR=P (otdPowerLevelText :706-711, TASK-1848). Not verified: whether the OTGW32 board electrically switches the line current (no schematic; only indirect source OT-Thing otcontrol.cpp :831-835, :1091-1100). AC2 (rewritten, the old wording assumed a fixed 'Low power' PR=P answer that 1848 replaced by the live level): decided with the advisor not to build the spontaneous lines: PR=P already gives the live level; without a line-voltage reading OT-Direct cannot suppress the report on an unplug as ReportPower does (:1533-1540), so the 1.25 s idle reset would print a misleading 'Low power'; the bench has no thermostat; the electrical level is unverified. Side finding noted: getSmartPowerState() never returns MEDIUM.
<!-- SECTION:FINAL_SUMMARY:END -->
