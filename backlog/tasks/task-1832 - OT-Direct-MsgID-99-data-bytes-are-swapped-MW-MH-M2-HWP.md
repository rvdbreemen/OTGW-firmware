---
id: TASK-1832
title: 'OT-Direct: MsgID 99 data bytes are swapped (MW=, MH=, M2=, HW=P)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:39:00Z'
updated_date: '2026-11-12T02:37:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 915000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N2), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: HB = DHW mode (low nibble) + manual push (bit 4); LB = HC1 (low nibble), HC2 (high nibble). OperModeData G:3247-3250, SetOperModeDHW G:4482-4494, SetOperModeHC1 G:4466-4478, SetOperModeHC2 G:5250-5262. tests/dhw-5.stc HW=P -> R10631000; opermode-1.stc MH=2 -> A40630002. Spec v4.2:2249-2277 same.
- OT-Direct: `data99 = DHW | (CH1 | CH2<<4) << 8` (O:3208-3209, :3319, :3332, :3345): DHW and push in LB, heating in HB. Own decoder uses PIC layout (OTGW-Core.ino:3629-3634).
- Consequence: MW=4 tells the boiler "HC1 reduced"; HW=P sets HC2=Auto and never requests a push.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test on real handlers: MW=4 -> 0x0400, MH=2 -> 0x0002, M2=3 -> 0x0030, HW=P -> bit 12; red on current code
- [x] #2 decoded MQTT topics for MsgID 99 match the command sent
- [x] #3 bench capture of the R frames
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): boiler reaction.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Ground truth checked in this session (3 sources): spec v4.2 MsgID 99 LB = HC1 bits 0..3, HC2 bits 4..7; HB = DHW bits 0..3, bit 4 manual DHW push (docs/opentherm specification/...v4.2.md:2249-2289); PIC 6.8 OperModeData operatingmode1 -> byte 3 (HB), operatingmode2 -> byte 4 (LB) (gateway.asm:3247-3250), SetOperModeDHW low nibble of operatingmode1 (gateway.asm:4482-4494); PIC test dhw-5.stc HW=P -> R10631000. Fix: otdOperModeData(push) builds (DHW|push<<4)<<8 | HC1 | HC2<<4; the MH, MW, M2 and HW=P branches use it. Decoder OTGW-Core.ino print_remote_override_operating_mode reads LB = HC1/HC2, HB = DHW+push, so decode now matches the command (AC2: confirm topics on the bench).

Test tests/test_otd_msgid99_bytes.py (real MH/MW/M2/HW branches cut from handleOTDirectCommand + the helper): old code 6 FAIL (e.g. MW=1, MH=4, HW=P -> 0x0411 instead of 0x1104), fix 6 OK. Neighbours: test_otd_tt_pic_parity 40 OK, test_otd_pr_reports_pyotgw OK, test_otd_ps1_summary_pic OK. evaluate.py --quick in wt-ba-otd: 92 checks, 0 FAIL.

Integrated and built: rebased on ac5967ad2, one bump per task (1823 beta.191, 1828 192, 1830 193, 1832 194, 1831 195, 1833 196, 1845 197) + test commit 4cafc97eb; full run_tests.py 208/210 (test_override_reply fixed in 4cafc97eb, test_stream_real_cbuf -j4-only, unrelated); evaluate --quick 93 checks 0 FAIL; build.bat esp32-combo 'Build completed successfully', build/OTGW-firmware-esp32-combo-2.0.0-beta.197+4cafc97-flash.zip (6158.3 KB). Bench requested from 09 via the coordinator.

Bench (otgw-bench-tester 09, .61 loopback, commands via TCP 25238, telnet + MQTT captured; beta.197+4cafc97 vs 190+1a4c0a4): flash OK, githash 4cafc97, health 200. Pushed ac5967ad2..4cafc97eb, zip posted (Discord 1570265549747781763). TASK-1832 PASS: MW=4 R10630004 -> R10630400; MH=2 0x0204 -> 0x0402; M2=3 0x3204 -> 0x0432. MQTT after M2=3 old -> new: dhw_mode anti_legionella(2) -> reduced(4), hc1_mode reduced(4) -> comfort(2), hc2_mode no_override(0) -> precomfort(3) (topics OTGW/value/<id>/RemoteOverrideOperatingMode_{dhw_mode,hc1_mode,hc2_mode}[_code]).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 host test on the real branches: 6 FAIL old / 6 OK fix. AC2 MQTT decode matches the command: 09 bench old vs new topics. AC3 bench R frames: 09 (R10630400 etc.). Commit c76efbe01 (beta.194), pushed, posted.
<!-- SECTION:FINAL_SUMMARY:END -->
