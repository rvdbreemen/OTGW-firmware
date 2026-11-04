---
id: TASK-156
title: 'OTGW32-Audit-4E: Loopback mode — simulated boiler responses'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:20'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-4
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit loopback test mode (GW=L): the firmware simulates a boiler without any hardware connected. Verify that master READ_DATA frames receive plausible READ_ACK responses, the frame bridge feeds results into processOT(), and the mode is safe to use on any OTGW32 board without risk to real hardware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Master READ_DATA frames receive simulated READ_ACK responses in loopback mode
- [x] #2 MsgID 0 status response has valid slave status byte
- [x] #3 Frame bridge correctly feeds simulated responses into processOT()
- [x] #4 No real OT bus activity occurs in loopback mode
- [x] #5 PR=M returns 'L' for loopback mode
- [x] #6 Any issue results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

- sendMasterRequestAsync() (line 866): IS_LOOPBACK_MODE() check is the very first thing — no real bus activity, all handled in-memory — correct
- simulateLoopbackResponse() (line 843): WRITE_DATA (type 1) returns WRITE_ACK (type 5) echoing data back; READ_DATA (type 0) looks up otLoopbackData[] table — correct
- otLoopbackData[]: PROGMEM table covering MsgIDs 0-127. MsgID 0 (line ~730 area) returns plausible status with slave status byte. 0xFFFF entries return UNKNOWN_DATA_ID — correct
- Frame bridge: T-frame logged then B-frame logged via bridgeFrameToParser() then both go into processOT() — correct
- boiler cache updated (line 874-876): simulated responses cached for later slave-handler use — correct
- If thermostat frame in loopback (origin==THERMOSTAT, line 879): otSlave.sendResponse() called — simulated response sent back to thermostat — correct
- PR=M (line 1960): IS_LOOPBACK_MODE() maps to char L — correct
- No real otMaster.sendRequestAsync() called in loopback — no bus activity — correct
- scheduleMasterRequest() fires normally in loopback mode (intercepted at sendMasterRequestAsync level) — this is acceptable and intentional (exercises full code path with simulated responses)
- No behavioral gaps found.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Loopback mode simulated boiler audit: all ACs pass.

- sendMasterRequestAsync() (line 866): IS_LOOPBACK_MODE() is checked first — completely bypasses real OT bus hardware
- simulateLoopbackResponse(): READ_DATA frames get READ_ACK from PROGMEM otLoopbackData[] table; WRITE_DATA frames get WRITE_ACK echoing the written value; unknown MsgIDs (0xFFFF in table) return UNKNOWN_DATA_ID
- MsgID 0 loopback data includes a valid slave status byte — correct
- bridgeFrameToParser() called for both request (R-prefix) and simulated response (B-prefix) — feeds processOT() for MQTT/REST/WebSocket
- boilerCache updated from simulated responses so slave handler can serve thermostat in loopback+thermostat scenarios
- No real otMaster.sendRequestAsync() or bus hardware accessed in loopback
- scheduleMasterRequest() still fires (intercepted at sendMasterRequestAsync level) — full code path exercised safely
- PR=M returns L — correct
- No behavioral gaps found.
<!-- SECTION:FINAL_SUMMARY:END -->
