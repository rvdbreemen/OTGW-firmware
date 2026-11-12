---
id: TASK-1844
title: >-
  OT-Direct: answers the PIC always gives the thermostat for MsgID 2, 6, 125 and
  an unsupported 56/57
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:56:00Z'
updated_date: '2026-11-12T06:03:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 923000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N10), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: MsgID 2 always acknowledged with the thermostat's own member ID and only the Smart Power bit (G:2837-2844; thermostat-3.stc AD002010B). MsgID 6 always READ_ACK with transfer-enable bits 0 and 1 set (G:2900-2918). MsgID 125: below 3.0 or no answer -> 3.0; new in 6.8, >=5.0 -> 4/51 (G:3327-3344; otversion-1/2/4.stc AC07D0300, A407D0433). MsgID 56/57 answered Unknown-DataID -> acknowledged with SW/SH value or upper bound, default 80/90 (G:3149-3161, :3185-3197, :262-265). These answers also come once blacklisted (fake answer via TreatMessage G:2467-2477).
- OT-Direct: replyToThermostat rewrites only MsgID 0, 9, 100 and overridden writes (O:1548-1582, :1490-1507). After 3 strikes the thermostat gets Unknown-DataID from the slot (O:1893-1897).
- Consequence: a thermostat behind a boiler without these IDs sees "not supported" and may stop offering DHW/max-CH setpoints.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host tests on the real reply path, one per ID, with the frames from the PIC test scripts; red on current code
- [x] #2 same answers after 3 Unknown strikes
- [ ] #3 bench with slave simulator where possible
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): acknowledging Smart Power commits the slave interface to medium/high power; the library tracks it (OpenTherm.cpp:446, :540-544) but the hardware side was not checked. Decide MsgID 2 separately if in doubt.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Decision (Robert, in session 2026-11-12): the PR=A banner on OT-Direct is 'PR: A=OpenTherm Gateway <version> OTGW32', e.g. 'PR: A=OpenTherm Gateway 6.8 OTGW32'. He first asked for the pre-beta.175 'PR: A=OpenTherm Gateway OTGW32'; told that this breaks pyotgw (int(ver[18]) at pyotgw.py:182 raises ValueError on 'O', so HA opentherm_gw cannot connect, the TASK-1797 cause), he chose version-then-board instead. The board name is a deliberate difference from the PIC; the version (OTD_PIC_COMPAT_VERSION, now 6.8: RT= TASK-1842 + MsgID 125 clamp TASK-1844) shows which PIC command set OT-Direct implements, so no separate PR letter or REST field is needed. Checked readers: pyotgw ver[18] = '6' (tests/test_otd_pr_reports_pyotgw.py P1 runs the real get_reports: OK); OTmonitor scan '%*s %*s %s' takes '6.8' (otmonitor.tcl:552) >= 6.2 (gui.tcl:1080); the firmware's own banner handling in processOT is skipped while OT-Direct runs (OTGW-Core.ino:6211-6217). Test A1 updated from 'no board name' to this format.

Implementation (wt-95rt): pure helper otdPicAnswer() (markers otdPicAnswer) applied after otdClockReply() in replyToThermostat() (gateway mode), otdAltSlotTaken() (3 Unknown strikes) and the UI= table branch, so the answers also come once blacklisted. MsgID 6: READ_ACK, HB |= 0x03 (MessageID6 gateway.asm:2900-2918). MsgID 125: READ_ACK 3.0 if the boiler gave Data-Invalid/Unknown (MsgUnknown = byte1 bit 5, :298) or a version below 3.0, 4.51 from 5.0 up, else the boiler's version (MessageID125 :3327-3344). MsgID 56/57 Unknown-DataID: acknowledgement of the request type with the upper bound otdUpperBoundF88(48/49): the boiler's MsgID 48/49 HB when cached, else 80/90 (MAX_DHWSETPOINT/MAX_CHSETPOINT :262-265; hotwaterreturn :3149-3161, maxchsetptret :3185-3197); Data-Invalid passes (the PIC also clears a stored SW/SH there, not copied: that is the TASK-1804 item 'boiler Data-Invalid does not clear SW/SH'). A held SW=/SH= keeps the buildOverriddenReply() answer. MsgID 2 NOT changed: split out to TASK-1861 (Smart Power needs a hardware check first). OTD_PIC_COMPAT_VERSION -> 6.8.

Banner decision (Robert, in session 2026-11-12), recorded above; final format 'PR: A=OpenTherm Gateway 6.8 OTGW32'; tests/test_otd_pr_reports_pyotgw.py A1 now asserts that format, P1 (real pyotgw get_reports) and A2 (OTmonitor 3rd word >= 6.2) green.

Evidence AC1 (for 6, 125, 56/57; MsgID 2 moved to TASK-1861) and AC2: tests/test_otd_pic_fixed_answers.py (5 tests; real otdPicAnswer with the real parity/buildOTResponse; PIC frames of otversion-1/2/4.stc: AC07D0300, AC07D0300, A407D0433; source checks of the three call sites incl. the slot and UI= paths). Fix 5/5 OK; old code (origin/dev 61b69e037, identity stub) 4 FAIL, 1 ok (the pass-through case). Harnesses kept compiling: test_otd_mode_switch_queue_reset.py (slice extended to otdUpperBoundF88 + an empty boiler cache) and test/host/build_and_run_override_reply.ps1 (optional 'PIC fixed answers' anchor): green on the lane and on origin. All 39 tests that slice the touched code rerun after the final change: green. evaluate --quick 92 checks 0 failed.

Open: AC3 bench with a slave simulator (needs a thermostat-side sim on the OT-Direct unit; .61 is in the release soak now). Note for the bench-tester: MsgID 125/6/56/57 answers are only observable with a thermostat (or sim) on the thermostat port.
<!-- SECTION:NOTES:END -->
