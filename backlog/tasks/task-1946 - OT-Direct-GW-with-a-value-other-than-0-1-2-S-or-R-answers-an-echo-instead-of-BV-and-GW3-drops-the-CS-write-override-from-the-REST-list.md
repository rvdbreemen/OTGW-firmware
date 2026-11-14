---
id: TASK-1946
title: >-
  OT-Direct: GW= with a value other than 0, 1, 2/S or R answers an echo instead
  of BV, and GW=3 drops the CS write override from the REST list
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T19:06:00Z'
updated_date: '2026-11-14T21:24:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 996000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .61 (beta.252+941a224, TASK-1839 bench, 2026-11-14). GW=3 gets the echo 'GW: 3' with no mode change and no NG/BV. The PIC 6.8 answers BadValue (BV) to any GW= value that is not 0, 1 or R (gateway.asm CheckBoolean :4072-4082, SetGatewayMode :4729-4747, ResetGateway :4776-4780); GW=2/S (master) is an OT-Direct extension. Also observed after GW=3: /api/v2/otdirect/overrides no longer listed the write override [1:10240] while PR=O still answered C40.00; cause not traced (read from 09's bench log only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GW= with an unsupported value answers BV, as the PIC, and changes no state; host test red on current code
- [x] #2 The REST override list and PR=O agree after an unsupported GW= (cause of the lost [1:10240] found and fixed, or shown not reproducible)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (d3, lane-1924): the GW= branch of handleOTDirectCommand() answers 'GW: SE' to a value longer than one character and 'GW: BV' to a value outside 0, 1, R and the OT-Direct extensions M, 2/S, P, L, and changes nothing. PIC 6.6 (other-projects/otgw-6.6/gateway.asm): SerialCommand prints 'GW: ' before the command runs (:3695-3703), so the error carries the prefix; SetGatewayMode checks the length (:4687-4690, SyntaxError), CheckBoolean accepts 0/1 (:4041-4051), ResetGateway answers BadValue to anything but R (:4734-4738). Before: GW=3 echoed 'GW: 3'; only the first character counted, so GW=10 switched to gateway and GW=RR ran a full reset.

AC1 evidence: new tests/test_otd_gw_bad_value.py compiles the real GW= branch. Fix: 4/4 OK. On HEAD b3e17f0d2 (OTGW_OTD_INO=copy): 8 subtests FAIL (GW=3, GW=9, GW=X, GW=r echo; GW=10/11 -> gateway, GW=RR -> reset, 'GW=0 ' -> monitor). All 38 tests/test_otd*.py OK. evaluate.py --quick: 93 checks, 0 failed.

AC2 (not a defect, shown from 09's bench log b1839/telnet.raw + replies.txt): CS=40 at 19:32:11; PR=O answered C40.00 at 19:33:11.82; at 19:33:12.36 'OTD: CS heartbeat expired, clearing MsgID 1 override' (the ~64 s CS expiry, TASK-1811). GW=3 changed nothing; the REST list was read after the expiry and PR=O just before it, both from the same otOverrides[] table (sendOTDirectOverridesJSON OTDirect.ino:1376, PR=O case 'O'). They agree. Side note, not in scope: other OT-Direct handlers answer a bare 'BV'/'NG' (otDirectBridgeProcessStatus) where the PIC prints 'XX: BV'; only PS= and now GW= carry the prefix. Not verified on the bench (field-only? no: bench-checkable, part of the lane bench request).

Correction of the previous note's last sentence: the GW= replies are bench-checkable on .61 and go into the lane bench request; until then only host-tested.
<!-- SECTION:NOTES:END -->
