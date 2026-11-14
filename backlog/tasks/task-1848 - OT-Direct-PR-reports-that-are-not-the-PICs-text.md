---
id: TASK-1848
title: 'OT-Direct: PR= reports that are not the PIC''s text'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T00:02:00Z'
updated_date: '2026-11-14T22:08:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 927000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N14), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

| Letter | PIC | OT-Direct |
|---|---|---|
| P | Low power / Medium power / High power (G:2261-2268, :5499-5502) | fixed P=M (O:3508-3511); library knows the level (OpenTherm.cpp:540-544) |
| R | D by default; S, R, C, I (G:2270-2280) | fixed R=I (= iSense) (O:3512-3515); FT= ignored |
| O | lowercase t/c until thermostat accepted; n while clearing (G:2201-2226) | always uppercase (O:3446-3475) |
| C | C=4 MHz (G:5506-5507) | C=240 (O:3520-3523) |
| B | HH:MM DD-MM-YYYY (tools/build.tcl:83) | __DATE__ (O:3516-3519) |
| L | default FXOMPC (G:5462-5468) | default XXXXXX (O:537) |
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test per letter, byte-exact; red on current code
- [x] #2 PR=P follows the real power level
- [x] #3 decision for C, B and L recorded (PIC text or agreed OT-Direct value)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): which clients parse P, R, C, B; pyotgw only stores them.

Part of the PIC 6.8 parity audit, parent TASK-1851.

2026-11-14 (95) decisions, Robert by DM via the coordinator: Q1 A (DM 1571191256833331375): PR=C '240 MHz' (PIC format SpeedString gateway.asm 6.8:5506-5507 with the S3 clock), PR=B the build time as 'HH:MM DD-MM-YYYY' (PIC build stamp format, otgw-6.8 tools/build.tcl:83), PR=L default 'FXOMPC' (PIC default :5462-5468; the old XXXXXX meant six transmit LEDs). Q2 A (DM 1571191595498340544): PR=O follows PrintOverride (:2201-2226) and covers only TT=/TC=; a CS= override alone reports N (CS= never sets the PIC override state, SetSetPoint :4095). Consumers named in the question: MQTT otgw-pic/settings/setpoint_override, HA sensor PIC_Setpoint_Override, REST /api/v2/pic/settings setpoint_override, web UI PIC panel (fmtPicSetting v2.js:6300-6304, which shows lowercase t/c and n raw; TASK-1939 filed for 09). SAT does not read it. CHANGELOG line added.

Findings: (1) OpenTherm library getSmartPowerState() (OpenTherm.cpp:540-544) returns LOW or HIGH, never MEDIUM; not patched (no library changes): OTDirect.ino derives OTDSlaveLine from OpenTherm and reads the protected txIdleLevel/rxIdleLevel, mapped as PrintPowerlevel :2262-2268 (tx=TransmitMode, rx=ReceiveModeT). (2) FT= is stored as the model letter PR=R prints (C, I, S by hand, else D; SetTStatModel :4705-4727); OT-Direct has no Remeha detection so R never appears. AC2 'follows the real power level' is host-tested on the mapping only; a Smart Power thermostat on the bench would be needed for the hardware path.

2026-11-14 18:16 (95) build 2.0.0-beta.254+f12af9f from lane-95-1848 HEAD f12af9f1d on origin/dev ef0c0884b (30d80a30c TASK-1848 beta.252, e4b8362a2 TASK-1849 beta.253, f12af9f1d TASK-1940 beta.254): exit 0, 5 min, 4 Compiling, 1 ESP32S3 image line (warm), RAM 130316 B, Flash 2149603 B; sha256 ino d2c427fbdc521cd9..., littlefs 1e43423a3acc1a6e..., zip f82a7690674d93f2.... Host tests on this HEAD: test_otd_pr_reports_pyotgw 15/15, test_otd_reply_texts_pic 4/4, test_pic_temp_sensor_port 8/8, test_otd_sc_clock 8/8. Old-vs-fix before the rebase: PR test 8 new FAIL on origin/dev OTDirect.ino; reply texts 3 FAIL on the 1848-only code; pic temp 7 FAIL on origin/dev (the setting-off case is trivially green there). evaluate --quick 86/0. Bench requested from 09 (.63 for 1940 + no regression, .61 for 1848/1849).

09 bench beta.255+58b387a on .61 (OT-Direct), 2026-11-14 19:2x, evidence 09 scratchpad b255/replies.txt: PR=P 'PR: P=Low power', PR=R 'PR: R=D', PR=C 'PR: C=240 MHz', PR=B 'PR: B=18:50 14-11-2026', PR=L 'PR: L=FXOMPC', PR=O 'PR: O=N'; after CS=40 still 'PR: O=N'; TT=20.5 -> 'PR: O=t20.50' (no thermostat on .61, so it stays lowercase); TT=0 -> 'PR: O=n' (stays n without a thermostat MsgID 9 read). AC2 limit: the bench has no Smart Power thermostat, so Medium/High were only host-tested (mapping), not seen on hardware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in beta.252 (commit 9299addd5 on origin/dev, pushed 47f0ea37e, alpha post 1571285029886103774). AC1: tests/test_otd_pr_reports_pyotgw.py on the real OTDirect.ino helpers, 15/15 green on the final HEAD; 8 cases FAIL on the old code. AC2: PR=P text from the real line state (class OTDSlaveLine, otdPowerLevelText) for Low/Medium/High in the host test; on hardware 09 saw only 'Low power' on .61 (beta.255, byte-exact), Medium/High need a thermostat that raises the line and were not seen on hardware. AC3: Robert DM 1571191256833331375 (C '240 MHz', B PIC build stamp, L 'FXOMPC') and DM 1571191595498340544 (CS= alone reports PR=O N). Bench 09 on .61 (255): P, R=D, C=240 MHz, B=18:50 14-11-2026, L=FXOMPC, O=N/t20.50/n all byte-exact. Not seen on hardware: the t->T and n->N transitions with a real thermostat. Gates: evaluate --quick 86/0; esp32-combo build beta.257+47f0ea3 exit 0; 09 PASS on .61 for 257.
<!-- SECTION:FINAL_SUMMARY:END -->
