---
id: TASK-190
title: 'Fix: Complete MQTT topic documentation for all PIC-emulated commands'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:40'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
  - phase-10
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The MQTT.md documentation (docs/api/MQTT.md) covers about half of the PIC commands implemented in handleOTDirectCommand(). The following commands are missing or incomplete: CC= (cooling control signal, MsgID 7), CL= (cooling level alias for CC=), MH= (CH1 operating mode, MsgID 99), MW= (DHW operating mode, MsgID 99), M2= (CH2 operating mode, MsgID 99), RR= (remote request, MsgID 4), SC= (time sync, MsgID 20), BW= (DHW blocking, bit6 master status), CE= (cooling enable, bit2 master status), SM= (summer mode, bit5 master status), PS= (print summary mode), GA=/GB= (GPIO function), LA-LF= (LED function), TS= (temperature sensor), RS= (reset boiler counter HBS/HBH/HPS/HPH/WBS/WBH/WPS/WPH), MI= (message interval), FS= (fail safety), TP= (thermostat slave parameters), RM=/CM= (response modifier set/clear). Also GW= is documented as 1=gateway/0=monitor only; it also supports 2/S=master, M=monitor, L=loopback, R=reset.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All PIC commands from handleOTDirectCommand() are listed as MQTT topics
- [x] #2 GW= entry updated with all valid values (0=bypass, 1=gateway, 2/S=master, M=monitor, L=loopback, R=reset)
- [x] #3 Each new entry includes topic suffix, payload format, OT command mapping, and description
- [x] #4 New commands organized into appropriate sections (setpoints, status flags, modes, queries, schedule, no-op)
- [x] #5 Documentation covers persistence information where applicable
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Extended MQTT.md Direct Commands table with 11 new OTGW32 commands (CE=, SM=, BW=, CC=/CL=, MH=, MW=, M2=, RR=, SC=, PS=, MI=, FS=, GA=, GB=) and fixed the GW= entry to document all 6 valid values (0=bypass, 1=gateway, 2/S=master, M=monitor, L=loopback, R=reset) with a dedicated mode table. Extended Advanced Commands table with RM=, CM=, corrected SR=/CR= format, updated RS= with all 8 named counter codes and their MsgID mappings (116-123), added TP=, and added full PR= query reference table with return format for all 17 registers (A/M/O/S/W/G/I/L/T/D/P/R/B/C/Q/N/V). Added LED LA-LF no-op documentation.
<!-- SECTION:FINAL_SUMMARY:END -->
