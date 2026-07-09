---
id: TASK-964
title: 'v2 parity: OT-Direct manual override apply UI (action + msgID + hex + Apply)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-30 23:06'
updated_date: '2026-07-09 19:32'
labels: []
dependencies: []
ordinal: 176000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep 0 hits in v2): the v2 Home/OT-Direct surface lacks the manual-override controls the Classic UI has (Home OT-Direct panel: override action selector, message-ID input, hex value input, Apply button). Without it a user cannot send manual OT-Direct overrides from v2. POSTs to /api/v2/otdirect/overrides (the endpoint already exists; classic uses it). Action gap — a real blocker for retiring Classic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 exposes an OT-Direct override control (pick action + msgID + hex value, Apply) that POSTs /api/v2/otdirect/overrides; result reflected in the OT-Direct status
- [ ] #2 Verified on-device: an override applied from v2 takes effect (telnet/status confirms)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in the v2 Connection tab (Monitor > Connection), gated on iface==='OT-Direct' (fetchConn). New 'OT-Direct overrides' card (reuses set-group/ble-row/ble-ctrls/tbtn — no new CSS classes, drift gate clean): lists the 4 override arrays from GET /api/v2/otdirect/overrides (write/response=SR/modify=RM/unknown=UI) with a per-row Clear (cr/cm/ki), plus an apply form (6-action select SR/CR/RM/CM/UI/KI + MsgID + hex value shown only for SR/RM + Apply) that POSTs ?action=&msgid=&value=. Mirrors the classic Home override panel. Verified live on .39 (OT-Direct): panel shows, lists 'Write MsgID 1 0x2D00', Apply UI msgid 30 -> row+Clear (POST ui 200), Clear -> gone (POST ki 200), 0 console errors. AC#2 (bus effect) is field-validation: .39 has no boiler (OT bus offline) so the override is accepted+stored+listed but the on-bus effect needs a real appliance to observe.

Code complete and committed (815896f3f). AC#1 verified on device .39 (apply+clear POST 200, active-overrides list updates, 0 console errors). AC#2 (override takes effect on the OT bus) is HARDWARE-GATED: bench .39 is OT-Direct with no appliance on the bus, so bus effect cannot be observed here. Held In Progress pending field validation on a device with a live boiler.

2026-07-09 drain review: AC#1 shipped+verified (815896f3f). AC#2 (override takes effect on a live OT bus) is not bench-closeable by the current fleet: the OTGW32 bench has no boiler on the bus, and the field testers (crashevans/number3nl) are PIC/Classic, not OT-Direct. Genuinely blocked on OT-Direct-hardware-WITH-a-boiler field validation. Parked here; recruit an OT-Direct+boiler tester to close.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: added OT-Direct manual override apply/clear (closes the action gap vs Classic). New 'OT-Direct overrides' card in the v2 Connection tab (Monitor > Connection), shown only on OT-Direct hardware. Lists active overrides (Write / SR stored-response / RM response-modifier / UI unknown-id) with per-row Clear, and an apply form (action select SR/CR/RM/CM/UI/KI + MsgID 0-127 + hex value for SR/RM + Apply) POSTing /api/v2/otdirect/overrides. Mirrors the classic Home override panel; reuses existing v2 component classes (no drift). Verified on-device (.39, OT-Direct): panel visible, list renders, apply + clear both POST 200 and update the list, 0 console errors. Bus-effect observation deferred to field validation (needs a connected appliance).
<!-- SECTION:FINAL_SUMMARY:END -->
