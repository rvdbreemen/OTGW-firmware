---
id: TASK-1072
title: >-
  Fix: loopback simulateLoopbackResponse reads past otLoopbackData[128] for
  MsgIDs above 127
status: To Do
assignee: []
created_date: '2026-08-08 18:19'
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
- [ ] #1 simulateLoopbackResponse bounds-checks msgId against the table size before indexing
- [ ] #2 A loopback request for any MsgID above the table range returns UNKNOWN_DATA_ID rather than fabricated data
- [ ] #3 The table size is expressed once (for example sizeof(otLoopbackData)/sizeof(otLoopbackData[0])) so the guard cannot drift from the declaration
- [ ] #4 Existing in-range loopback behaviour is unchanged for ids 0-127
- [ ] #5 Build green for the relevant esp32 targets and python evaluate.py --quick shows no new failures
<!-- AC:END -->
