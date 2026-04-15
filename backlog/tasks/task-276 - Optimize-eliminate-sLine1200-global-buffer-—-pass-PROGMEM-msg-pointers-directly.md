---
id: TASK-276
title: >-
  Optimize: eliminate sLine[1200] global buffer — pass PROGMEM msg pointers
  directly
status: Done
assignee: []
created_date: '2026-04-15 21:28'
updated_date: '2026-04-15 21:43'
labels:
  - performance
  - mqtt
  - memory
  - stap-1b
dependencies:
  - TASK-272
references:
  - src/OTGW-firmware/MQTTstuff.ino
  - src/OTGW-firmware/OTGW-firmware.h
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-272, sLine is only used as a staging buffer: strcpy_P from PROGMEM msg pool into sLine, then pass sLine to rendering functions. On ESP8266 core 3.x, PROGMEM is memory-mapped flash at 0x40200000+ and directly accessible via regular C pointers (*ptr works). The strcpy_P staging is therefore unnecessary.

Elimination: replace strcpy_P(sLine, mqttHaMsgPool + entry.msgOff) with a direct PROGMEM pointer (const char *msgTemplate = mqttHaMsgPool + entry.msgOff). Pass this pointer to strstr(), sendMQTTTemplateStreaming(), and expandAndPublishSourceTemplates(). All these functions take const char* and iterate byte-by-byte — works identically for PROGMEM and RAM on ESP8266.

Impact: 1200 bytes DRAM freed (BSS segment reduction). sLine declaration and SLINE_SIZE define removed from OTGW-firmware.h. Session lock comments updated.

Agent inventory confirmed: sLine is referenced ONLY in MQTTstuff.ino (2 writes, 7 strstr reads, 1 strlen, 4 function pointer passes). No other file uses sLine or SLINE_SIZE.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sLine[SLINE_SIZE] declaration removed from OTGW-firmware.h
- [x] #2 SLINE_SIZE #define removed from OTGW-firmware.h
- [x] #3 doAutoConfigureMsgid() passes mqttHaMsgPool + entry.msgOff directly to strstr and rendering functions, no strcpy_P to sLine
- [x] #4 doAutoConfigure() passes mqttHaMsgPool + entry.msgOff directly, no strcpy_P to sLine
- [x] #5 Debug strlen uses strlen_P(msgTemplate) for explicit PROGMEM safety
- [x] #6 Session lock and workspace comments updated (sLine no longer referenced)
- [x] #7 Build succeeds, firmware BSS reduced by ~1200 bytes compared to TASK-272 baseline
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Eliminated sLine[1200] global buffer. PROGMEM msg template pointers passed directly to strstr, sendMQTTTemplateStreaming, and expandAndPublishSourceTemplates. 1200 bytes DRAM freed. Build: 863,808 bytes.
<!-- SECTION:FINAL_SUMMARY:END -->
