---
id: TASK-1797
title: >-
  OT-Direct: PR=A banner and PR=T format break pyotgw (HA opentherm_gw) on port
  25238; the banner names the board
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T06:35:00Z'
updated_date: '2026-11-11T12:28:00Z'
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
- [ ] #1 PR=A answers 'OpenTherm Gateway <version number>' that pyotgw parses (a digit at index 18) and that contains no board name
- [ ] #2 PR=T answers two digits without a separator, as the PIC
- [ ] #3 pyotgw get_reports() runs without an exception against the new replies (test on the real pyotgw parsing code)
- [ ] #4 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #5 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->
