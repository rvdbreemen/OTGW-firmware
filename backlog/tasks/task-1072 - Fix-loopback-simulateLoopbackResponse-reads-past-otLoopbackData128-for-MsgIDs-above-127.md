---
id: TASK-1072
title: >-
  Fix: loopback simulateLoopbackResponse reads past otLoopbackData[128] for
  MsgIDs above 127
status: Done
assignee:
  - '@claude'
created_date: '2026-08-08 18:19'
updated_date: '2026-09-30 09:23'
labels:
  - bug
  - otdirect
dependencies: []
priority: medium
ordinal: 259000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-08-08 while assessing whether loopback mode could substitute for the PIC-only frame replay (TASK-1071). In src/OTGW-firmware/OTDirect.ino, simulateLoopbackResponse derives the message id as 'uint8_t msgId = (request >> 16) & 0xFF', so it ranges 0-255, and then indexes 'pgm_read_word(&otLoopbackData[msgId])' where otLoopbackData is declared [128]. There is no bounds check. Any loopback request for MsgID 128 or above therefore reads past the end of the table into whatever PROGMEM follows and returns that as a synthetic boiler response, either as a READ_ACK carrying garbage or, if the adjacent word happens to be 0xFFFF, as UNKNOWN_DATA_ID by accident. On ESP32 this is a valid flash read so it does not fault, which is why it has gone unnoticed: it silently fabricates boiler data. The affected range 128-255 is the OEM/vendor area and includes the Remeha ids 131-133 that TASK-1068 just made decodable, so a loopback session can now feed the decoder fabricated Remeha values.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 simulateLoopbackResponse bounds-checks msgId against the table size before indexing
- [x] #2 A loopback request for any MsgID above the table range returns UNKNOWN_DATA_ID rather than fabricated data
- [x] #3 The table size is expressed once (for example sizeof(otLoopbackData)/sizeof(otLoopbackData[0])) so the guard cannot drift from the declaration
- [x] #4 Existing in-range loopback behaviour is unchanged for ids 0-127
- [x] #5 Build green for the relevant esp32 targets and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-30 closing evidence (in-session, dev HEAD 2a8d40fd1; fix commit 9bfcd0868).
Host proof test/host/build_and_run_loopback.ps1 -OldVsFix (slices setOTParityBit, otLoopbackData, simulateLoopbackResponse and buildOTResponse from OTDirect.ino by anchor; OLD = git 9bfcd0868^, FIX = working tree; MSVC /Od; 256 ids x 8 message types):
- FIX: cases=6144 oob_reads=0 write_table_reads=0; READ of 128-255: 0/384 READ_ACK; all 2688 non-write cases for 128-255 answer UNKNOWN-DATA-ID with data 0 and valid parity; WRITE of 128-255: 384/384 WRITE_ACK echo; failures=0.
- OLD: oob_reads=3072, READ_ACK 0xBEEF (bounds-counting stand-in) for 384/384 READs past the table, failures=5760 (C1, C6).
- ids 0-127: 3072 cases byte-identical OLD vs FIX (sha256 6E18702A336A114A both).
- Review (workflow wf_fe6c4173-ec6): every slice verified verbatim against the blobs; mutants M1-M6 (guard >= vs >, read above the guard, aliasing via & 0x7F, wrong type, parity removed, WRITE branch limited) each make the harness fail, so the checks are not vacuous.
Build/eval: build.bat --target all at alpha.380 (esp32, esp32-classic, esp32-combo SUCCESS, 11:12-11:19) and evaluate.py --quick 69 passed / 0 warnings / 0 failed; earlier today IMG-0 full evaluate.py 79 passed / 5 warnings / 0 failed and tests/test_evaluate.py 61 OK. simulateLoopbackResponse is unchanged since 9bfcd0868.
AC#2 interpretation (for the maintainer): a WRITE to MsgID 128-255 is answered with a WRITE_ACK that echoes the master's own data. That is not fabricated data and matches in-range unsupported ids (TASK-1073 precedent); OT spec 4.4.2 would allow UNKNOWN-DATA-ID there too, which is a possible low-priority follow-up, not a defect of this task. Loopback never reaches the boiler; the reply only goes to a thermostat when the request came from one.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
simulateLoopbackResponse() (OTDirect.ino) bounds-checks the MsgID against sizeof(otLoopbackData)/sizeof(otLoopbackData[0]) before any table read (fix 9bfcd0868). Proven on the host with test/host/build_and_run_loopback.ps1, which compiles the real functions: the pre-fix source reads past the 128-entry table 3072 times and answers READs of 128-255 with READ_ACK and garbage, the fix reads nothing out of range, answers every non-WRITE of 128-255 with UNKNOWN-DATA-ID (data 0, valid parity), echoes WRITEs with WRITE_ACK, and leaves ids 0-127 byte-identical (3072 cases). Mutation tests confirm the checks bite. Build of all three targets and evaluate.py --quick are green at alpha.380. docs/c4/c4-code-otdirect.md now describes the loopback path as the code does.
<!-- SECTION:FINAL_SUMMARY:END -->
