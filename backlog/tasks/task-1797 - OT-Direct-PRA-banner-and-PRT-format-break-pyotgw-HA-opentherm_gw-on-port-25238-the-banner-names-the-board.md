---
id: TASK-1797
title: >-
  OT-Direct: PR=A banner and PR=T format break pyotgw (HA opentherm_gw) on port
  25238; the banner names the board
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T06:35:00Z'
updated_date: '2026-11-11T12:39:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 890000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report B, BC1 and BC4 (BC1 verified by the coordinator). OTD answers 'PR: A=OpenTherm Gateway OTGW32' (OTDirect.ino:3271); the PIC answers 'A=OpenTherm Gateway 6.6' (gateway.asm:5434-5435). pyotgw 2.2.3 (pinned by HA) evaluates int(ver[18]) < 5 in get_reports() (other-projects/pyotgw-master/pyotgw/pyotgw.py:177-183) outside any try/except: int('O') raises ValueError, so the HA config flow fails and an existing entry fails setup on every restart (config_flow.py:72-84, __init__.py:119). Next crash once that is fixed: PR=T answers 'T=%d/%d' (OTDirect.ino:3340) where the PIC prints two digits without separator (gateway.asm:2278-2285); pyotgw does int(tweaks[1]) = int('/') (pyotgw.py:224-230); OTmonitor's patterns 'PR: T=[01][01]' (otmonitor.tcl:628-634) match neither. The banner also contains the board name, which must not appear in public output.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PR=A answers 'OpenTherm Gateway <version number>' that pyotgw parses (a digit at index 18) and that contains no board name
- [x] #2 PR=T answers two digits without a separator, as the PIC
- [x] #3 pyotgw get_reports() runs without an exception against the new replies (test on the real pyotgw parsing code)
- [x] #4 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #5 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Answer PR=A with the PIC banner format 'OpenTherm Gateway 6.6' (OTD_PIC_COMPAT_VERSION: the PIC command set OT-Direct implements, audited against 6.6 in TASK-1795). Not the firmware version 2.0.0: pyotgw would then skip PR=D (int(ver[18]) < 5) and OTmonitor would hide 6.2+ features (gui.tcl:1080). PR=T prints two booleans without a separator (PrintTweaks). Host test cuts the real PR= branch and feeds its replies into the real pyotgw 2.2.3 get_reports().
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (wt-cyc09, 2026-11-11): new tests/test_otd_pr_reports_pyotgw.py cuts the real PR= branch of handleOTDirectCommand() and runs the replies through the real pyotgw CommandProcessor.issue_cmd() + OpenThermGateway.get_reports() (other-projects/pyotgw-master 2.2.3). Old code (OTGW_OTD_INO = HEAD copy): 4 tests, failures=2 errors=2, incl. 'ValueError: invalid literal for int() with base 10: O' raised from pyotgw.py:182 (the HA crash) and 'PR: T=1/0' != 'PR: T=10'. Fix: 4/4 OK; banner 'PR: A=OpenTherm Gateway 6.6', no board name, pyotgw asks PR=D, ignore_transitions/ovrd_high_byte parsed. evaluate.py --quick: 0 failed, 100%. Docs: MQTT.md PR=A row and c4-code-otdirect.md no longer name the board. Logs: scratchpad t1797-red.log, t1797-green.log.

Coordinator checks (2026-11-11). (1) Own firmware does not turn the OT-Direct PR=A reply into a PIC version. The reply 'PR: A=...' has ':' at index 2, so processOT() routes it to handlePRresponse() (OTGW-Core.ino:6160-6165), which ignores register A (:1325-1329). It never reaches the banner branch (:6209, also gated by !isOTDirectEnabled()). state.pic.sFwversion is written only from OTGWSerial.firmwareVersion(), the parse of bytes from the PIC UART (OTGW-Core.ino:847, :6233, handleDebug.ino:375); OT-Direct writes none (grep sFwversion OTDirect.ino: 0 hits). So device/info picfwversion (restAPI.ino:3288), MQTT otgw-pic/version (MQTTstuff.ino:2036, skipped while isPICEnabled() is false) and the PIC menu are unchanged by this fix.

(2) OTmonitor PIC upgrade over port 25238. It starts only from the menu 'Firmware upgrade ...' (gui.tcl:382-383; enabled for every non-file device, whatever the version). It never starts on the version. What changes with '6.6': upgrade::resetcommand now sends GW=R, because fwvariant=gateway and vsatisfies 4.0a6- holds (upgrade.tcl:326-353). With the old banner fwversion stayed 0 and no GW=R was sent. OT-Direct answers GW=R as a gateway reset of its own state; a PIC resets too. OTmonitor then sends only the bootloader version query cocmd 0 3 (upgrade.tcl:407), a binary line starting with STX that handleOTDirectCommand() rejects. No bootloader answers it, so OTmonitor shows 'Please manually reset', waits up to 60 s and ends 'Could not switch gateway to self-programming mode' (upgrade.tcl:408-416). All of this happens before any write packet, which needs selfprogmode=1 (:421). An OT-Direct board therefore refuses the upgrade by design: there is nothing to flash and nothing is written. No code change needed. Not run against a live OTmonitor (no OTmonitor rig): read from the code only.
<!-- SECTION:NOTES:END -->
