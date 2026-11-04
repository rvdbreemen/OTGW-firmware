---
id: TASK-277
title: >-
  Optimize: eliminate topicBuf[200] stack buffer — pass PROGMEM topic pointers
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
  - TASK-276
references:
  - src/OTGW-firmware/MQTTstuff.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Companion to TASK-276. After sLine elimination, topicBuf[MQTT_TOPIC_MAX_LEN] (200 bytes on stack) is the remaining staging buffer. It copies the PROGMEM topic template to RAM before passing to renderTemplateToBuffer(), strstr(), and expandAndPublishSourceTemplates().

Same principle as TASK-276: ESP8266 PROGMEM is memory-mapped, so the PROGMEM pointer can be passed directly to all receiving functions. The renderTemplateToBuffer() function iterates via *cursor which works for PROGMEM on ESP8266. The strstr() for PIC detection and source token detection also works directly on PROGMEM.

One subtlety: the debug log MQTTDebugTf(PSTR("...[%s]..."), topicBuf) passes the topic to printf via %s. On ESP8266, printf reading *ptr from PROGMEM works through memory mapping.

Impact: ~200 bytes stack freed per doAutoConfigureMsgid/doAutoConfigure call. Reduces CONT stack pressure during discovery.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 char topicBuf[MQTT_TOPIC_MAX_LEN] declaration removed from doAutoConfigureMsgid() and doAutoConfigure()
- [x] #2 PROGMEM topic pointer (mqttHaTopicPool + entry.topicOff) passed directly to renderTemplateToBuffer()
- [x] #3 PIC detection check uses strstr() on PROGMEM topic pointer directly
- [x] #4 Source token strstr() calls use PROGMEM topic pointer directly
- [x] #5 expandAndPublishSourceTemplates() receives PROGMEM pointer for topicTemplate parameter
- [x] #6 Build succeeds, no stack overflow or alignment issues
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Eliminated topicBuf[200] stack allocation. PROGMEM topic pointers passed directly to renderTemplateToBuffer, strstr (PIC check + source tokens), and expandAndPublishSourceTemplates. ~200 bytes stack freed per discovery call.
<!-- SECTION:FINAL_SUMMARY:END -->
