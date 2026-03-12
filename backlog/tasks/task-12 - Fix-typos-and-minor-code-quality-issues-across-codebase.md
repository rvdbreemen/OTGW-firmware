---
id: TASK-12
title: Fix typos and minor code quality issues across codebase
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:53'
updated_date: '2026-03-12 22:00'
labels:
  - cleanup
dependencies: []
references:
  - 'src/OTGW-firmware/OTGW-Core.ino:296'
  - 'src/OTGW-firmware/OTGW-Core.ino:1902'
  - 'src/OTGW-firmware/jsonStuff.ino:827-877'
  - 'src/OTGW-firmware/MQTTstuff.ino:750'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Several minor issues found during code review that should be cleaned up:

1. **Typo "MQWTT"** (OTGW-Core.ino:296): Section header reads `//=====[ Send useful information to MQWTT ]====` — should be "MQTT". Also needs updating in the Section Map TOC.

2. **Typo "implementatoin"** (OTGW-Core.ino:1902): Section header reads `//=====[ Command Queue implementatoin ]====` — should be "implementation". Also needs updating in the Section Map TOC.

3. **extractJsonField uses String class** (jsonStuff.ino:827-877): This function uses `String&` parameters with `.indexOf()`, `.substring()`, `.toCharArray()`. While not in the hottest path (called from REST API settings POST), it still causes heap fragmentation. Could be rewritten using `strstr`/`strchr` on `const char*`. Lower priority since it's not called per-request.

4. **Unused 1200-byte static buffer** (MQTTstuff.ino:750): The `sendMQTTData(FlashStringHelper*, FlashStringHelper*, bool)` overload has a `static char payloadBuf[MQTT_MSG_MAX_LEN]` (1200 bytes) permanently allocated in RAM. Check if this overload is actually called; if rarely used, consider removing it or using a smaller buffer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Typo 'MQWTT' corrected to 'MQTT' in section header and TOC
- [ ] #2 Typo 'implementatoin' corrected to 'implementation' in section header and TOC
- [ ] #3 extractJsonField rewritten to use const char* and strstr instead of String class
- [ ] #4 sendMQTTData FlashStringHelper overload audited: removed if unused or buffer right-sized if needed
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed typos and code quality issues:

1. Fixed "MQWTT" → "MQTT" in OTGW-Core.ino section header and TOC.
2. Fixed "implementatoin" → "implementation" in OTGW-Core.ino section header and TOC.
3. Rewrote extractJsonField to use const char* with strstr/strchr instead of String class methods (indexOf/substring/toCharArray). Added String& wrapper for backward compatibility. Eliminates heap allocations from JSON field extraction.
4. Audited sendMQTTData(F(),F(),bool) overload — confirmed it IS used (OTGW-Core.ino:182 for PROGMEM event messages). The 1200-byte static buffer is justified and retained.

Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
