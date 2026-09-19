---
id: TASK-1141
title: 'Log every PR: response so a capture shows whether the PIC answers at all'
status: To Do
assignee: []
created_date: '2026-09-19 15:35'
updated_date: '2026-09-19 15:36'
labels:
  - diagnostics
  - enhancement
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/684'
  - 'src/OTGW-firmware/OTGW-Core.ino:726-836'
priority: medium
ordinal: 222000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
While diagnosing GitHub #684 (Appiejs, heat pump, no OpenTherm traffic) we could not answer a basic question from his telnet capture: does the PIC reply to the PR= queries the ESP sends at boot?

His usb-serial.log shows the ESP sending GW=R three times and then PR=O,S,W,G,I,L,T,D,P,R,M,B,C,Q,N,V. His telnet.txt contains zero lines from handlePRresponse. That absence proves nothing, because handlePRresponse is almost entirely silent:

- OTGW-Core.ino:728-740 - four parse guards return without a word (len<4, missing "PR:" prefix, plen<3, payload[1] != '=').
- OTGW-Core.ino:824-829 - a register response only logs when strcmp shows the value CHANGED. A PIC answering with the same value as last time logs nothing.
- The only unconditional logs are the two error paths (unknown register, unexpected PR=M value) and the gateway-mode change.

So "no PR: lines in the log" is consistent with three very different worlds: the PIC never answered, the PIC answered with unchanged values, or the PIC answered something malformed. For #684 that difference is the whole diagnosis: it separates "PIC is dead or reset out from under us" from "PIC runs fine, its OT bus side sees nothing".

Worth recording for whoever picks this up: Appiejs's banner reported PIC gateway v6.8, and state.pic.sFwversion is transient state (ADR-051), set from the fwreportinfo callback. That callback lives in OTGWSerial's firmware-detect path, not in processOT. So the banner proves bytes flowed PIC-to-ESP during detection, but it does NOT prove processOT ever saw a line. The new logging is what would tell those two apart.

Goal is diagnosability only. No behaviour change, no new setting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A PR: line reaching handlePRresponse emits exactly one debug line naming the register and the value, whether or not the value changed
- [ ] #2 A PR: line rejected by any of the four parse guards emits a debug line stating which guard rejected it, so a malformed reply is distinguishable from no reply at all
- [ ] #3 The new logging reuses the existing OT-message debug gate (state.debug.bOTmsg via OTGWDebugTf); no new setting, no new flag, and nothing is logged when that gate is off
- [ ] #4 Every new literal uses PSTR/F per the PROGMEM rules; python evaluate.py --quick shows no new failures
- [ ] #5 python build.py --firmware exits 0
- [ ] #6 A telnet capture from the bench OTGW on COM3 with OT-message debug enabled shows one new line per PR= query the ESP sends, including for registers whose value did not change
<!-- AC:END -->
