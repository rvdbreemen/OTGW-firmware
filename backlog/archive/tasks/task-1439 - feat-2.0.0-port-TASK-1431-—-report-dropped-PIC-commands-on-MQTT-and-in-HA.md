---
id: TASK-1439
title: 'feat-2.0.0: port TASK-1431 — report dropped PIC commands on MQTT and in HA'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T22:37:00Z'
updated_date: '2026-10-29T06:35:00Z'
labels:
  - enhancement
  - port
dependencies: []
priority: low
ordinal: 600000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x TASK-1431 (commit 28efefe8a on otgw-1.x.x) adds an HA-facing contract that 2.0.0 should match: otgw-firmware/command_dropped (not retained, payload = command text), otgw-firmware/command_retry_exceeded (retained ON/OFF; ON on a drop, OFF on the next confirmed non-PR command, republished on MQTT connect), cmd_dropped in /api/v2/device/info, HA sensor 'Last Dropped Command' + binary sensor 'Command Dropped', and TP= commands appended in the queue instead of replaced. Check the 2.0.0 queue (incl. OTDirect synthesizeResponse paths) and the binary-sensor discovery label sanitizing before porting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Same topics, payloads and retain flags as 1.x TASK-1431
- [x] #2 TP= commands are appended, not replaced, in the 2.0.0 command queue
- [x] #3 Build for the 2.0.0 targets and evaluator green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Same contract as the 1.x line (commit 28efefe8a): publishCommandDroppedState/reportCommandDropped/clearCommandDropped in OTGW-Core.ino, state in PICSection.
2. handleCommandQueue drop path reports; checkCommandResponse clears unless PR= or PM= (SAT PM chain is internal on 2.0.0).
3. addCommandToQueue: TP= append-only.
4. Discovery: faux id 240 (241 is OTGWdhwmeterid on dev), sensor row appended, binary row at index 60 (end of indexed region), counts and LUTs, deviceForOTId + topoDeviceForPseudoId -> Gateway, queueNonOTDiscoveryIds.
5. cmd_dropped in device/info; retained flag republished on MQTT connect.
6. Host harness slicing the real queue, old vs fix; build + evaluator.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 (wt-1439 on acab024d7):
- OT-Direct: addCommandToQueue() hands commands to handleOTDirectCommand() synchronously (no queue), so nothing can be dropped there; synthesizeResponse() still feeds checkCommandResponse() for PIC-style confirmation.
- Binary-sensor label sanitizing already exists on 2.0.0 (TASK-1204), nothing to port.
- Pseudo-ID 240 instead of 1.x 241 (taken by OTGWdhwmeterid); HA device Gateway (next to gateway_mode/otgw_connected). Topics, payloads and retain flags are the 1.x ones.
- Harness: python test/host/test_cmd_queue_drop.py --old-rev acab024d7 -> RESULT: PASS. Real queue sliced from OTGW-Core.ino (#define OTGW_CMD_RETRY .. end of checkCommandResponse) and PICSection from PICtypes.h. FIX passes D1 (PR=Z sent 5x -> command_dropped PR=Z retain 0, command_retry_exceeded ON retain 1, cmd count 1), D2 (PR=/PM= confirmations keep the flag), D3 (confirmed TT= -> OFF retain 1), D4 (two TP= -> two slots), C1 (two TT= -> one slot), C2 (answered command not reported). OLD fails D1-D4, passes C1-C2.
- build.bat OTGW_BUILD_JOBS=4 esp32-combo alpha.524+acab024 + these changes: .ino.bin/.elf 06:42, strings otgw-firmware/command_dropped, command_retry_exceeded, Last_Dropped_Command, Command_Dropped, cmd_dropped present in the image. evaluate.py --quick 79 passed 0 failed.
- NOT verified: HA discovery output at runtime. test_ha_discovery_json.py does not compile on clean origin/dev either (Ethernet shim, TASK-1443), and the PIC bench .63 was offline (not associated for 4h48m, not on USB). Discovery checked by code and image strings only.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the 1.x dropped-command feedback to 2.0.0: a PIC command dropped after 5 unanswered tries is published on otgw-firmware/command_dropped (not retained), latches otgw-firmware/command_retry_exceeded ON (retained, republished on MQTT connect) until the next confirmed command that is not PR= or PM=, and is counted in cmd_dropped in /api/v2/device/info. HA: sensor Last Dropped Command and binary sensor Command Dropped on the Gateway device, faux id 240. TP= commands are appended in the queue instead of replaced.

Tests: host harness test_cmd_queue_drop.py on the real queue, old (acab024d7) fails D1-D4, fix passes all; build esp32-combo green; evaluate.py 79/0.

Not verified: discovery at runtime (TASK-1443 host test broken on dev, PIC bench offline). OT-Direct boards never drop a command (synchronous handler).

Builds: combo only (maintainer 2026-10-29): esp32-combo alpha.526 built and green; bench not run: no PIC board available (Classic .63 offline). evaluate.py --quick 79 passed 0 failed after the rebase on b07e3194a; both harnesses PASS old-vs-fix against b07e3194a.

After the rebase on af831bac2 (TASK-1443 repaired the discovery harness): python test/host/test_ha_discovery_json.py -> RESULT: PASS. It calls doAutoConfigureMsgid() for every id 0..255 under four settings permutations, so the two id-240 configs are composed and pass the JSON gate, the HA topic pattern and the no-duplicate-uniq_id check.
<!-- SECTION:FINAL_SUMMARY:END -->
