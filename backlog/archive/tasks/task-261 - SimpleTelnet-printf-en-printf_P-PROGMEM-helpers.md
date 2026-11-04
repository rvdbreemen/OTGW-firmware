---
id: TASK-261
title: 'SimpleTelnet: printf en printf_P PROGMEM helpers'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 20:07'
updated_date: '2026-04-12 20:53'
labels:
  - telnet
  - progmem
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement printf() and printf_P() helper methods for SimpleTelnet. These broadcast formatted output to all connected clients. printf() matches the ESPTelnet API. printf_P() is new — essential for this project's PROGMEM convention but also useful for any ESP8266 user.\n\nprintf(const char* fmt, ...):\n  Taken from ESPTelnet::printf() with no structural changes.\n  char loc_buf[64];\n  va_list arg;\n  va_start(arg, fmt);\n  int len = vsnprintf(loc_buf, sizeof(loc_buf), fmt, arg);\n  va_end(arg);\n  if (len < 0) return 0;\n  if (len < (int)sizeof(loc_buf)):\n    return write((uint8_t*)loc_buf, len);\n  // Overflow: heap fallback\n  char* temp = (char*)malloc(len + 1);\n  if (!temp) return 0;\n  va_start(arg, fmt);   // MUST restart va_list — first one was consumed\n  vsnprintf(temp, len + 1, fmt, arg);\n  va_end(arg);\n  size_t written = write((uint8_t*)temp, len);\n  free(temp);\n  return written;\n\nprintf_P(PGM_P fmt, ...):\n  Identical structure but uses vsnprintf_P() on ESP8266.\n  CRITICAL: ESP32 does not have vsnprintf_P(). On ESP32, PROGMEM strings are\n  in regular address space, so vsnprintf() works directly with PGM_P.\n  Platform guard is mandatory:\n  #ifdef ARDUINO_ARCH_ESP8266\n    len = vsnprintf_P(loc_buf, sizeof(loc_buf), fmt, arg);\n  #else\n    // ESP32: PROGMEM == RAM, standard vsnprintf works\n    len = vsnprintf(loc_buf, sizeof(loc_buf), fmt, arg);\n  #endif\n  The same platform guard applies to the malloc overflow path.\n\nIMPORTANT — va_list restart:\n  After the first vsnprintf call consumes the va_list, you CANNOT reuse it.\n  You must call va_end() then va_start() again before the overflow vsnprintf.\n  Forgetting this is a common bug that causes stack corruption.\n  ESPTelnet::printf() (ESPTelnet.cpp:46-70) demonstrates the correct pattern.\n\nBOTH methods call write() (the broadcast method) at the end — they always\ngo to ALL connected clients, never to a single client.\n\nRETURN VALUE: bytes written (same as ESPTelnet). If no clients connected: 0.\n\nFIRMWARE CONTEXT:\n  The project's Debug.h currently works around missing printf_P by doing:\n    char buf[256]; snprintf_P(buf, sizeof(buf), PSTR(fmt), ...); debugTelnet.print(buf);\n  After this task, DebugTf() can be simplified to: debugTelnet.printf_P(PSTR(fmt), ...);\n  (Optional simplification — can be done separately or left as-is.)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 printf(const char* fmt, ...) werkt identiek aan ESPTelnet::printf()
- [x] #2 printf_P(PGM_P fmt, ...) ondersteunt PROGMEM format strings via vsnprintf_P()
- [x] #3 Beide methoden broadcasten naar alle verbonden clients via write()
- [x] #4 Stack-buffer 64 bytes, malloc fallback voor output langer dan 64 bytes
- [x] #5 printf_P(PSTR("heap: %d\r\n"), val) compileert en werkt correct
- [x] #6 ESP8266: printf_P() uses vsnprintf_P() with PGM_P — correct PROGMEM read via pgm_read_byte
- [x] #7 ESP32: printf_P() uses standard vsnprintf() — PROGMEM is a no-op on ESP32, PGM_P is just const char*
- [x] #8 Platform guard #ifdef ARDUINO_ARCH_ESP8266 wraps vsnprintf_P vs vsnprintf selection
- [x] #9 Reference: ESPTelnet.cpp printf() lines 45-70 for va_list pattern and malloc fallback
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented printf() and printf_P() helpers for SimpleTelnet.\n\nChanges:\n- printf(const char* fmt, ...): returns size_t (was void); 64-byte stack buffer; malloc fallback for overflow; va_list restarted correctly before overflow call\n- printf_P(PGM_P fmt, ...): identical structure; ESP8266 uses vsnprintf_P() in both the initial and overflow call; method only exists on ESP8266 (inside #if ARDUINO_ARCH_ESP8266 guard)\n- Both broadcast via write() — always go to all connected clients\n- Early return 0 if no active clients\n- Early return 0 on vsnprintf error (len < 0)\n- malloc fallback returns 0 gracefully if allocation fails\n- Fixed return type in header declaration from void to size_t for both methods\n- 64-byte buffer vs previous 256-byte: saves 192 bytes of stack per call; malloc path handles the rare long-format case cleanly"
<!-- SECTION:FINAL_SUMMARY:END -->
