---
id: TASK-1141
title: 'Log every PR: response so a capture shows whether the PIC answers at all'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-19 15:35'
updated_date: '2026-09-19 15:49'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
VERIFICATION KILLED THIS TASK. The premise is false. No code was written.

I re-read Appiejs's actual telnet capture instead of trusting my summary of it, and the capture contains 17 PR: lines, not zero. Every one of the 16 PR= queries got an answer from the PIC, and the existing logging showed it:

  handlePRresp( 828): handlePRresponse: PR=S updated to [1.00]
  PR: S=1.00

Two mechanisms already cover what this task wanted to add:

1. handlePRresponse's own change-log fired for all 16 registers (OTGW-Core.ino:828).
2. More importantly, the CALLER prints the raw line unconditionally. OTGW-Core.ino:4556 does a bare Debugln(buf), and Debugln is debugTelnet.println with no gate at all (Debug.h:20). It is not behind state.debug.bOTmsg.

Point 2 falsifies the whole task, including the "unchanged value logs nothing" gap that was its strongest argument. The tail of the capture proves it directly: the repeat PR=M polls at 16:56:37 and 16:57:37 produced NO handlePRresp line, because the value had not changed, and yet "PR: M=M" still printed both times.

AC #2's first guard is also dead code: the caller at OTGW-Core.ino:4552-4555 already tests buf[2]==':' and buf[0]=='P' && buf[1]=='R' before calling, so len<4 and the prefix check can never fail.

So "no PR: lines in a capture" does mean the PIC did not answer. The diagnostic question this task existed to answer is already answerable with shipped firmware.

What the capture actually shows, for whoever picks up #684: the PIC is healthy and answers everything (v6.8, 4MHz, build 25-08-2026), and reports PR: M=M three times over two minutes. M = monitor mode, not gateway mode. That, plus zero OT frames in four minutes, is the real lead.
<!-- SECTION:NOTES:END -->
