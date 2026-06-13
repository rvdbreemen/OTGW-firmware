/* 
***************************************************************************  
**  Program  : jsonStuff
**  Version  : v2.0.0-alpha.182
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// In-place buffer version to avoid heap allocations
void escapeJsonStringTo(const char* src, char* dest, size_t destSize) {
  if (!src || !dest || destSize == 0) {
    if (dest && destSize > 0) dest[0] = '\0';
    return;
  }

  size_t destIdx = 0;
  for (const char* p = src; *p && destIdx < destSize - 1; p++) {
    const char* esc = nullptr;
    char hex[7];
    
    switch (*p) {
      case '"':  esc = "\\\""; break;
      case '\\': esc = "\\\\"; break;
      case '\b': esc = "\\b";  break;
      case '\f': esc = "\\f";  break;
      case '\n': esc = "\\n";  break;
      case '\r': esc = "\\r";  break;
      case '\t': esc = "\\t";  break;
      default:
        if (*p < 0x20) {
          snprintf_P(hex, sizeof(hex), PSTR("\\u%04X"), (unsigned char)*p);
          esc = hex;
        }
    }

    if (esc) {
      size_t len = strlen(esc);
      if (destIdx + len < destSize - 1) {
        memcpy(&dest[destIdx], esc, len);
        destIdx += len;
      } else {
        break; // Out of space
      }
    } else {
      dest[destIdx++] = *p;
    }
  }
  dest[destIdx] = '\0';
}

static void sendEscapedJsonStringContent(const char* src) {
  if (!src) return;

  char chunk[24];
  size_t chunkIdx = 0;

  for (const char* p = src; *p; p++) {
    const char* esc = nullptr;
    char hex[7];

    switch (*p) {
      case '"':  esc = "\\\""; break;
      case '\\': esc = "\\\\"; break;
      case '\b': esc = "\\b";  break;
      case '\f': esc = "\\f";  break;
      case '\n': esc = "\\n";  break;
      case '\r': esc = "\\r";  break;
      case '\t': esc = "\\t";  break;
      default:
        if (*p < 0x20) {
          snprintf_P(hex, sizeof(hex), PSTR("\\u%04X"), (unsigned char)*p);
          esc = hex;
        }
        break;
    }

    if (esc) {
      const size_t escLen = strlen(esc);
      if ((chunkIdx + escLen) >= sizeof(chunk)) {
        chunk[chunkIdx] = '\0';
        restSendContent(chunk);
        chunkIdx = 0;
      }
      memcpy(chunk + chunkIdx, esc, escLen);
      chunkIdx += escLen;
    } else {
      if ((chunkIdx + 1) >= sizeof(chunk)) {
        chunk[chunkIdx] = '\0';
        restSendContent(chunk);
        chunkIdx = 0;
      }
      chunk[chunkIdx++] = *p;
    }
  }

  if (chunkIdx > 0) {
    chunk[chunkIdx] = '\0';
    restSendContent(chunk);
  }
}

static void writeEscapedJsonStringContent(File& f, const char* src) {
  if (!src) return;

  char chunk[24];
  size_t chunkIdx = 0;

  for (const char* p = src; *p; p++) {
    const char* esc = nullptr;
    char hex[7];

    switch (*p) {
      case '"':  esc = "\\\""; break;
      case '\\': esc = "\\\\"; break;
      case '\b': esc = "\\b";  break;
      case '\f': esc = "\\f";  break;
      case '\n': esc = "\\n";  break;
      case '\r': esc = "\\r";  break;
      case '\t': esc = "\\t";  break;
      default:
        if (*p < 0x20) {
          snprintf_P(hex, sizeof(hex), PSTR("\\u%04X"), (unsigned char)*p);
          esc = hex;
        }
        break;
    }

    if (esc) {
      const size_t escLen = strlen(esc);
      if ((chunkIdx + escLen) >= sizeof(chunk)) {
        chunk[chunkIdx] = '\0';
        f.print(chunk);
        chunkIdx = 0;
      }
      memcpy(chunk + chunkIdx, esc, escLen);
      chunkIdx += escLen;
    } else {
      if ((chunkIdx + 1) >= sizeof(chunk)) {
        chunk[chunkIdx] = '\0';
        f.print(chunk);
        chunkIdx = 0;
      }
      chunk[chunkIdx++] = *p;
    }
  }

  if (chunkIdx > 0) {
    chunk[chunkIdx] = '\0';
    f.print(chunk);
  }
}

// Helper function to unescape a single JSON escape character (the char after '\').
// Returns the unescaped character; unknown escapes pass through unchanged.
static inline char unescapeJsonChar(char esc) {
  switch (esc) {
    case '"':  return '"';
    case '\\': return '\\';
    case 'n':  return '\n';
    case 'r':  return '\r';
    case 't':  return '\t';
    default:   return esc;
  }
}

static int iIdentlevel = 0;
bool bFirst = true; 

static RestPerfSample* getRestPerfSample(RestPerfTarget target)
{
  switch (target) {
    case REST_PERF_SAT_STATUS:  return &state.restperf.satStatus;
    case REST_PERF_DEVICE_INFO: return &state.restperf.deviceInfo;
    case REST_PERF_SETTINGS:    return &state.restperf.settings;
    case REST_PERF_NONE:
    default:                    return nullptr;
  }
}

static void restPerfAccumulateSendTime(uint32_t deltaMs)
{
  if (state.restperf.eActiveTarget == REST_PERF_NONE) return;
  state.restperf.iActiveSendMs += deltaMs;
  state.restperf.iActiveChunkCount++;
}

#if HAS_REST_TX_COALESCING
// ── REST coalescing transmit buffer (TASK-743: HAS_REST_TX_COALESCING) ─────
// The sync WebServer stalls ~9 ms per sendContent() call on ESP32 due to
// FreeRTOS/lwIP scheduling. Batching into 4 KB chunks (covering most JSON
// responses in 2–4 flushes) cuts T_send from ~3.8 s to < 100 ms. Gated on the
// capability flag, not the platform, so app code carries no raw ESP32 ifdef.
// Static allocation — safe for the single-threaded sync WebServer.
static struct {
  char   data[4096];
  size_t len;
} sTxBuf;

static void restFlushTxBuf() {
  if (sTxBuf.len == 0) return;
  const uint32_t t0 = millis();
  // Length-based overload (WebServer.cpp): the single-arg sendContent(const
  // char*) would bind to sendContent(const String&) and construct a ~4KB heap
  // String temporary per flush -- wasteful on the fragmented ESP32-S3 heap, and
  // a failed alloc would yield length()==0 -> a 0-length chunk that ends the
  // chunked response early. Passing the explicit length avoids both (TASK-820).
  httpServer.sendContent(sTxBuf.data, sTxBuf.len);
  restPerfAccumulateSendTime(millis() - t0);
  sTxBuf.len = 0;
}

static void restTxAppend(const char* s, size_t n) {
  while (n > 0) {
    size_t space = (sizeof(sTxBuf.data) - 1) - sTxBuf.len;
    if (space == 0) { restFlushTxBuf(); space = sizeof(sTxBuf.data) - 1; }
    size_t copy = (n <= space) ? n : space;
    memcpy(sTxBuf.data + sTxBuf.len, s, copy);
    sTxBuf.len += copy;
    s += copy;
    n -= copy;
  }
}
// ─────────────────────────────────────────────────────────────────────────
#endif

static void restSendContent(const char* content)
{
#if HAS_REST_TX_COALESCING
  restTxAppend(content, strlen(content));
#else
  const uint32_t startMs = millis();
  httpServer.sendContent(content);
  restPerfAccumulateSendTime(millis() - startMs);
#endif
}

static void restSendContentP(PGM_P content)
{
#if HAS_REST_TX_COALESCING
  // On ESP32, PROGMEM strings live in flash-mapped DROM and are accessible
  // via normal pointers — strlen/memcpy are safe.
  restTxAppend(content, strlen(content));
#else
  const uint32_t startMs = millis();
  httpServer.sendContent_P(content);
  restPerfAccumulateSendTime(millis() - startMs);
#endif
}

static void restSendP(int code, PGM_P contentType, PGM_P content)
{
#if HAS_REST_TX_COALESCING
  sTxBuf.len = 0; // discard any stale data before sending new HTTP response
#endif
  const uint32_t startMs = millis();
  httpServer.send_P(code, contentType, content);
  restPerfAccumulateSendTime(millis() - startMs);
}

// Flush any buffered transmit data to the HTTP client.
// On ESP32 this drains the coalescing buffer; on ESP8266 it is a no-op
// (sendContent flushes inline). Call at the end of each chunked response.
void restFlushContent() {
#if HAS_REST_TX_COALESCING
  restFlushTxBuf();
#endif
}

void restPerfBegin(RestPerfTarget target)
{
  state.restperf.eActiveTarget = target;
  state.restperf.iActiveSendMs = 0;
  state.restperf.iActiveChunkCount = 0;
}

void restPerfCommit(RestPerfTarget target, uint32_t totalMs)
{
  RestPerfSample* sample = getRestPerfSample(target);
  if (!sample) return;

  sample->iLastTotalMs = totalMs;
  sample->iLastSendMs = state.restperf.iActiveSendMs;
  sample->iLastRenderMs = (totalMs > state.restperf.iActiveSendMs)
                        ? (totalMs - state.restperf.iActiveSendMs)
                        : 0;
  sample->iLastChunkCount = state.restperf.iActiveChunkCount;
  sample->iSampleCount++;
  if (totalMs > sample->iMaxTotalMs) {
    sample->iMaxTotalMs = totalMs;
  }

  state.restperf.eActiveTarget = REST_PERF_NONE;
  state.restperf.iActiveSendMs = 0;
  state.restperf.iActiveChunkCount = 0;
}

//=======================================================================
void sendStartJsonObj(const char *objName)
{
  char sBuff[50] = "";

  if (strlen(objName)==0){  
    snprintf_P(sBuff, sizeof(sBuff), PSTR("{\r\n"));
  }else {
    snprintf_P(sBuff, sizeof(sBuff), PSTR("{\"%s\":[\r\n"), objName);
  }
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  restSendP(200, PSTR("application/json"), PSTR(" "));
  restSendContent(sBuff);
  iIdentlevel++;
  bFirst = true;

} // sendStartJsonObj()


//=======================================================================
void sendEndJsonObj(const char *objName)
{
  iIdentlevel--;
  if (strlen(objName)==0){
    restSendContentP(PSTR("\r\n}\r\n"));
  } else {
    restSendContentP(PSTR("\r\n]}\r\n"));
  }
  restFlushContent();

} // sendEndJsonObj()
//=======================================================================
void sendIdent(){
  for (int i = iIdentlevel; i >0; i--){
    restSendContentP(PSTR("  "));
  }
}  //sendIdent()
//=======================================================================
void sendBeforenext(){
  if (!bFirst){ 
    restSendContentP(PSTR(",\r\n"));
  }
  bFirst = false;
} //sendBeforenext()
//=======================================================================
// Map-based output functions for compact JSON
//=======================================================================

void sendStartJsonMap(const char *objName)
{
  char sBuff[50] = "";

  if (strlen(objName)==0){  
    snprintf_P(sBuff, sizeof(sBuff), PSTR("{\r\n"));
  }else {
    snprintf_P(sBuff, sizeof(sBuff), PSTR("{\"%s\":{\r\n"), objName);
  }
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  restSendP(200, PSTR("application/json"), PSTR(" "));
  restSendContent(sBuff);
  iIdentlevel++;
  bFirst = true;
}

//=======================================================================
// Map entry helpers for compact JSON objects
//=======================================================================
void sendJsonMapEntry(const char *cName, bool bValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %s"), cName, CBOOLEAN(bValue));

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, int32_t iValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %d"), cName, iValue);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

// Extra overloads to resolve type ambiguity on ESP32 where int/int32_t and
// unsigned int/uint32_t are distinct types (int32_t = long on xtensa-esp32).
#if PLATFORM_INT_DISTINCT_FROM_INT32
void sendJsonMapEntry(const char *cName, int iValue) {
  sendJsonMapEntry(cName, (int32_t)iValue);
}
void sendJsonMapEntry(const char *cName, unsigned int uValue) {
  sendJsonMapEntry(cName, (uint32_t)uValue);
}
#endif
void sendJsonMapEntry(const char *cName, int8_t iValue) {
  sendJsonMapEntry(cName, (int32_t)iValue);
}
void sendJsonMapEntry(const char *cName, int16_t iValue) {
  sendJsonMapEntry(cName, (int32_t)iValue);
}
void sendJsonMapEntry(const char *cName, uint16_t uValue) {
  sendJsonMapEntry(cName, (uint32_t)uValue);
}

void sendJsonMapEntry(const char *cName, uint32_t uValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %u"), cName, uValue);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, float fValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %.3f"), cName, fValue);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, const char *cValue)
{
  sendBeforenext();
  sendIdent();
  restSendContentP(PSTR("\""));
  sendEscapedJsonStringContent(CSTR(cName));
  restSendContentP(PSTR("\": \""));
  sendEscapedJsonStringContent(CSTR(cValue));
  restSendContentP(PSTR("\""));
}

void sendJsonMapEntry(const char *cName, String sValue)
{
  if (sValue.length() > (JSON_ENTRY_BUF - 65) )
  {
    DebugTf(PSTR("[2] sValue.length() [%d]\r\n"), sValue.length());
  }

  sendJsonMapEntry(cName, sValue.c_str());
}

void sendEndJsonMap(const char *objName)
{
  iIdentlevel--;
  if (strlen(objName)==0){
    restSendContentP(PSTR("\r\n}\r\n"));
  } else {
    restSendContentP(PSTR("\r\n}}\r\n"));
  }
  restFlushContent();
}

void sendJsonOTmonMapEntry(const char *cName, const char *cValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";
  
  // Compact format: "name": {"value": "val", "unit": "u", "epoch": 123}
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": \"%s\", \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, cValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, int32_t iValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %d, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, iValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

// Extra overloads to resolve type ambiguity on ESP32
#if PLATFORM_INT_DISTINCT_FROM_INT32
void sendJsonOTmonMapEntry(const char *cName, int iValue, const char *cUnit, time_t epoch) {
  sendJsonOTmonMapEntry(cName, (int32_t)iValue, cUnit, epoch);
}
void sendJsonOTmonMapEntry(const char *cName, unsigned int uValue, const char *cUnit, time_t epoch) {
  sendJsonOTmonMapEntry(cName, (uint32_t)uValue, cUnit, epoch);
}
#endif
void sendJsonOTmonMapEntry(const char *cName, int8_t iValue, const char *cUnit, time_t epoch) {
  sendJsonOTmonMapEntry(cName, (int32_t)iValue, cUnit, epoch);
}
void sendJsonOTmonMapEntry(const char *cName, int16_t iValue, const char *cUnit, time_t epoch) {
  sendJsonOTmonMapEntry(cName, (int32_t)iValue, cUnit, epoch);
}
void sendJsonOTmonMapEntry(const char *cName, uint16_t uValue, const char *cUnit, time_t epoch) {
  sendJsonOTmonMapEntry(cName, (uint32_t)uValue, cUnit, epoch);
}

void sendJsonOTmonMapEntry(const char *cName, uint32_t uValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %u, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, uValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, float fValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %.3f, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, fValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, bool bValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %s, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, CBOOLEAN(bValue), cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
}

//=======================================================================
// Dallas temperature-specific helper for Map API (1 decimal precision)
//=======================================================================
void sendJsonOTmonMapEntryDallasTemp(const char *cName, float fValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %.1f, \"unit\": \"%s\", \"type\": \"dallas\", \"epoch\": %d}")
                                      , cName, fValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
} // sendJsonOTmonMapEntryDallasTemp(*char, float, *char, time_t)

void sendJsonOTmonMapEntryDallasTemp(const char *cName, float fValue, const __FlashStringHelper *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %.1f, \"unit\": \"%S\", \"type\": \"dallas\", \"epoch\": %d}")
                                      , cName, fValue, reinterpret_cast<PGM_P>(cUnit), (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  restSendContent(jsonBuff);
} // sendJsonOTmonMapEntryDallasTemp(*char, float, *FlashStringHelper, time_t)

//=======================================================================
// ************ function to build Json Settings string ******************
//=======================================================================
void sendJsonSettingObj(const char *cName, int iValue, const char *iType, int minValue, int maxValue)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("  \"%s\": {\"value\": %d, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, iValue, iType, minValue, maxValue);

  sendBeforenext();
  restSendContent(jsonBuff);
} // sendJsonSettingObj(*char, int, *char, int, int)


//=======================================================================

void sendJsonSettingObj(const char *cName, const char *cValue, const char *sType, int maxLen)
{
  // Send prefix
  char jsonBuff[64] = {0};
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("  \"%s\": {\"value\": \""), cName);
  sendBeforenext();
  restSendContent(jsonBuff);

  // Send value with JSON string escaping (handles " and \ in payload templates)
  char chunk[32];
  size_t ci = 0;
  for (const char *p = cValue; *p; p++) {
    if (ci >= sizeof(chunk) - 2) { chunk[ci] = '\0'; restSendContent(chunk); ci = 0; }
    char c = *p;
    if      (c == '"')  { chunk[ci++] = '\\'; chunk[ci++] = '"';  }
    else if (c == '\\') { chunk[ci++] = '\\'; chunk[ci++] = '\\'; }
    else if (c == '\n') { chunk[ci++] = '\\'; chunk[ci++] = 'n';  }
    else if (c == '\r') { chunk[ci++] = '\\'; chunk[ci++] = 'r';  }
    else if (c == '\t') { chunk[ci++] = '\\'; chunk[ci++] = 't';  }
    else                { chunk[ci++] = c; }
  }
  if (ci > 0) { chunk[ci] = '\0'; restSendContent(chunk); }

  // Send suffix
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\", \"type\": \"%s\", \"maxlen\": %d}"), sType, maxLen);
  restSendContent(jsonBuff);

} // sendJsonSettingObj(*char, *char, *char, int)

//=======================================================================
// Float setting: value is a pre-formatted string emitted as a JSON number (no quotes)
void sendJsonSettingObj(const char *cName, const char *cValue, const char *sType, int minValue, int maxValue)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("  \"%s\": {\"value\": %s, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, cValue, sType, minValue, maxValue);

  sendBeforenext();
  restSendContent(jsonBuff);
} // sendJsonSettingObj(*char, *char, *char, int, int)

//=======================================================================
void sendJsonSettingObj(const char *cName, bool bValue, const char *sType)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("  \"%s\": {\"value\": %s, \"type\": \"%s\"}")
                                      , cName, CBOOLEAN(bValue), sType);

  sendBeforenext();
  restSendContent(jsonBuff);
 
} // sendJsonSettingObj(*char, bool, *char)    

//=======================================================================
// Helper Overloads for PROGMEM support
//=======================================================================

// sendJsonMapEntry helpers
void sendJsonMapEntry(const __FlashStringHelper* cName, bool bValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, bValue);
}

void sendJsonMapEntry(const __FlashStringHelper* cName, int32_t iValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, iValue);
}

// Extra overloads to resolve type ambiguity on ESP32 (see char* overloads above)
#if PLATFORM_INT_DISTINCT_FROM_INT32
void sendJsonMapEntry(const __FlashStringHelper* cName, int iValue) {
  sendJsonMapEntry(cName, (int32_t)iValue);
}
void sendJsonMapEntry(const __FlashStringHelper* cName, unsigned int uValue) {
  sendJsonMapEntry(cName, (uint32_t)uValue);
}
#endif
void sendJsonMapEntry(const __FlashStringHelper* cName, int8_t iValue) {
  sendJsonMapEntry(cName, (int32_t)iValue);
}
void sendJsonMapEntry(const __FlashStringHelper* cName, int16_t iValue) {
  sendJsonMapEntry(cName, (int32_t)iValue);
}
void sendJsonMapEntry(const __FlashStringHelper* cName, uint16_t uValue) {
  sendJsonMapEntry(cName, (uint32_t)uValue);
}

void sendJsonMapEntry(const __FlashStringHelper* cName, uint32_t uValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, uValue);
}

void sendJsonMapEntry(const __FlashStringHelper* cName, float fValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, fValue);
}

void sendJsonMapEntry(const __FlashStringHelper* cName, const char *cValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, cValue);
}

void sendJsonMapEntry(const __FlashStringHelper* cName, String sValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, sValue);
}

void sendJsonMapEntry(const __FlashStringHelper* cName, const __FlashStringHelper* cValue) {
  char nameBuf[35];
  char valBuf[101];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  strncpy_P(valBuf, (PGM_P)cValue, sizeof(valBuf));
  valBuf[sizeof(valBuf)-1] = 0;
  sendJsonMapEntry(nameBuf, (const char*)valBuf);
}

// sendJsonSettingObj helpers

// For: void sendJsonSettingObj(const char *cName, int iValue, const char *iType, int minValue, int maxValue)
void sendJsonSettingObj(const __FlashStringHelper* cName, int iValue, const char *iType, int minValue, int maxValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, iValue, iType, minValue, maxValue);
}

// For: void sendJsonSettingObj(const char *cName, const char *cValue, const char *sType, int minValue, int maxValue)
void sendJsonSettingObj(const __FlashStringHelper* cName, const char *cValue, const char *sType, int minValue, int maxValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, cValue, sType, minValue, maxValue);
}

// For: void sendJsonSettingObj(const char *cName, const char *cValue, const char *sType, int maxLen)
void sendJsonSettingObj(const __FlashStringHelper* cName, const char *cValue, const char *sType, int maxLen) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, cValue, sType, maxLen);
}

// Overload for PROGMEM value string (avoids RAM literal for password placeholders)
void sendJsonSettingObj(const __FlashStringHelper* cName, const __FlashStringHelper* fValue, const char *sType, int maxLen) {
  // Copy flash value to small stack buffer; password placeholders are short (<20 chars).
  char valueBuf[24];
  strncpy_P(valueBuf, (PGM_P)fValue, sizeof(valueBuf));
  valueBuf[sizeof(valueBuf)-1] = '\0';
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = '\0';
  sendJsonSettingObj(nameBuf, valueBuf, sType, maxLen);
}

// For: void sendJsonSettingObj(const char *cName, bool bValue, const char *sType)
void sendJsonSettingObj(const __FlashStringHelper* cName, bool bValue, const char *sType) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, bValue, sType);
}

//=======================================================================
// Minimal streaming JSON field extractor for flat JSON object bodies.
// Extracts the value of a named field from a flat JSON object string.
// Handles:
//   - String values:  {"key":"value"}  → result = "value"
//   - Bool literals:  {"key":true}     → result = "true"
//   - Numbers:        {"key":42}       → result = "42"
// 'key' is a PROGMEM string (use F() macro at call site).
// Returns true if the field was found and result was populated.
// NOTE: Uses cMsg as scratch for building the search pattern.
//       Safe: String::indexOf() does not yield, so cMsg cannot be clobbered mid-use.
//       Not safe to call from ISR or concurrently.
//=======================================================================
bool extractJsonField(const char* json, const __FlashStringHelper* key,
                      char* result, size_t resultSize) {
  if (!json || !result || resultSize == 0) return false;
  result[0] = '\0';
  // Build search pattern: "keyname"
  snprintf_P(cMsg, sizeof(cMsg), PSTR("\"%S\""), (PGM_P)key);
  const char* found = strstr(json, cMsg);
  if (!found) return false;

  const char* colon = strchr(found + strlen(cMsg), ':');
  if (!colon) return false;

  const char* p = colon + 1;
  while (*p == ' ') p++;
  if (*p == '\0') return false;

  if (*p == '"') {
    // Quoted string value — scan for closing quote respecting backslash escapes
    p++; // skip opening quote
    size_t ri = 0;
    while (*p != '\0') {
      if (*p == '\\' && *(p + 1) != '\0') {
        p++;
        if (ri < resultSize - 1) result[ri++] = unescapeJsonChar(*p);
        p++;
        continue;
      }
      if (*p == '"') break;
      if (ri < resultSize - 1) result[ri++] = *p;
      p++;
    }
    if (*p != '"') return false; // no closing quote found
    result[ri] = '\0';
    return true;
  } else {
    // Unquoted value: bool literal (true/false) or number
    const char* end = p;
    while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\r' && *end != '\n') end++;
    size_t n = end - p;
    if (n == 0 || n + 1 > resultSize) return false;
    memcpy(result, p, n);
    result[n] = '\0';
    return true;
  }
}

// Convenience wrapper accepting Arduino String (delegates to const char* version)
bool extractJsonField(const String& json, const __FlashStringHelper* key,
                      char* result, size_t resultSize) {
  return extractJsonField(json.c_str(), key, result, resultSize);
}

//=======================================================================
// Read one "key":"value" string pair from a flat JSON object file.
// Advances the file position past the separator (comma or closing brace).
// Both key and value must be quoted strings.
// Returns false at end of object ('}') or on read error.
// Used by ensureSensorDefaultLabels() to stream dallas_labels.ini without
// loading the full file into RAM.
//=======================================================================
bool readJsonStringPair(File& f, char* key, size_t keySize,
                        char* val, size_t valSize) {
  if (!key || keySize == 0 || !val || valSize == 0) return false;
  key[0] = val[0] = '\0';

  // Skip whitespace, commas, and opening brace to reach first '"'
  int ch;
  do {
    ch = f.read();
    if (ch < 0) return false;
  } while (ch == ' ' || ch == '\n' || ch == '\r' || ch == ',' || ch == '{');

  if (ch == '}') return false; // end of object
  if (ch != '"') return false; // unexpected character

  // Read key until unescaped closing quote
  size_t i = 0;
  while ((ch = f.read()) >= 0 && ch != '"') {
    if (ch == '\\') {
      int esc = f.read(); if (esc < 0) break;
      ch = unescapeJsonChar((char)esc);
    }
    if (i < keySize - 1) key[i++] = (char)ch;
  }
  key[i] = '\0';

  // Skip whitespace and colon
  do { ch = f.read(); } while (ch >= 0 && (ch == ' ' || ch == ':'));
  if (ch != '"') return false; // value must be a quoted string

  // Read value until unescaped closing quote
  i = 0;
  while ((ch = f.read()) >= 0 && ch != '"') {
    if (ch == '\\') {
      int esc = f.read(); if (esc < 0) break;
      ch = unescapeJsonChar((char)esc);
    }
    if (i < valSize - 1) val[i++] = (char)ch;
  }
  val[i] = '\0';

  return key[0] != '\0';
}

//=======================================================================
// Write one "key":"value" pair to an open file.
// Prepends a comma separator if 'addComma' is true (for all but the first pair).
// Used by ensureSensorDefaultLabels() to rebuild dallas_labels.ini.
//=======================================================================
void writeJsonStringPair(File& f, const char* key, const char* val, bool addComma) {
  if (!key || !val) return;

  if (addComma) f.print(',');
  f.print('"');
  writeEscapedJsonStringContent(f, key);
  f.print(F("\":\""));
  writeEscapedJsonStringContent(f, val);
  f.print('"');
}

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
