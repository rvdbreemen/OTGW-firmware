/* 
***************************************************************************  
**  Program  : jsonStuff
**  Version  : v2.0.0-alpha.245
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

// TASK-865.9: the restSend* layer is the imperative-push -> async-pull bridge.
// Every restSendContent(P) write lands in the per-request AsyncResponseStream
// (webServerCompat.h g_restStream), which is opened lazily on the first write
// and flushed exactly once by restFlushContent() at the end of the response.
// AsyncResponseStream buffers internally, so the old HAS_REST_TX_COALESCING
// sTxBuf path (which existed to batch the sync WebServer's ~9 ms/sendContent
// stalls) is no longer used on the write path.
static void restSendContent(const char* content)
{
  const uint32_t startMs = millis();
  AsyncResponseStream* s = restBeginStream("application/json");
  if (s) s->print(content);
  restPerfAccumulateSendTime(millis() - startMs);
}

static void restSendContentP(PGM_P content)
{
  const uint32_t startMs = millis();
  AsyncResponseStream* s = restBeginStream("application/json");
  // On ESP32, PROGMEM strings live in flash-mapped DROM and are accessible
  // via normal pointers — FPSTR()/print handles them.
  if (s) s->print(FPSTR(content));
  restPerfAccumulateSendTime(millis() - startMs);
}

// Begin a fresh JSON response. The sync WebServer needed an explicit status +
// content-type line here; the async stream carries the content type from
// beginResponseStream() and the status defaults to 200, so this just opens the
// stream and (defensively) discards any stale buffered bytes.
static void restSendP(int code, PGM_P contentType, PGM_P content)
{
  (void)code; (void)content;
  const uint32_t startMs = millis();
  restBeginStream(contentType);
  restPerfAccumulateSendTime(millis() - startMs);
}

// Flush the buffered JSON response to the client — sends the AsyncResponseStream
// exactly once. Call at the end of each chunked response (sendEndJsonMap/Obj).
void restFlushContent() {
  restFinalize();
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
  // CORS header is queued before the stream opens so it lands on the response.
  webPushHeader(F("Access-Control-Allow-Origin"), F("*"));
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
  // CORS header is queued before the stream opens so it lands on the response.
  webPushHeader(F("Access-Control-Allow-Origin"), F("*"));
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
// Hand-rolled inbound JSON field extractor (TASK-886, full ADR-141 revert).
// Replaces the ArduinoJson deserializeJson path with a bounded, NON-THROWING,
// no-heap flat top-level scanner. Extracts the value of a named TOP-LEVEL field
// from a flat JSON object string:
//   - String values:  {"key":"value"}  -> result = "value" (escapes resolved,
//                      \uXXXX decoded to UTF-8, strlcpy-style truncation)
//   - Bool literals:   {"key":true}     -> result = "true" / "false"
//   - Numbers:         {"key":42}       -> result = "42" (source token verbatim)
//   - Explicit null / container value / absent / malformed -> returns false
// CORRECTNESS: every value (string, nested object, nested array) is skipped
// WHOLESALE, so a key-looking substring inside a value (e.g.
// {"other":"value","value":5}) is never mis-matched as a key -- the exact bug
// the original strstr scanner had, fixed here without re-introducing the
// ArduinoJson dependency. 'key' is PROGMEM (use F() at the call site).
// Algorithm validated host-side: scripts/tests/test_extract_json_field.py (32
// cases incl. key-in-value, escapes, \u, truncation, adversarial no-throw).
// Not safe from ISR (uses a caller buffer); fine under doBackgroundTasks.
//=======================================================================
static inline int xjfHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
// Append codepoint as UTF-8 (1-4 bytes) into out[*oi], bounded by cap-1.
// Drops the char whole rather than writing a partial multibyte sequence.
static inline void xjfPutUtf8(char* out, size_t* oi, size_t cap, uint32_t cp) {
  size_t need = (cp < 0x80) ? 1 : (cp < 0x800) ? 2 : (cp < 0x10000) ? 3 : 4;
  if (*oi + need > cap - 1) return;
  if (cp < 0x80) {
    out[(*oi)++] = (char)cp;
  } else if (cp < 0x800) {
    out[(*oi)++] = (char)(0xC0 | (cp >> 6));
    out[(*oi)++] = (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out[(*oi)++] = (char)(0xE0 | (cp >> 12));
    out[(*oi)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[(*oi)++] = (char)(0x80 | (cp & 0x3F));
  } else {
    out[(*oi)++] = (char)(0xF0 | (cp >> 18));
    out[(*oi)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[(*oi)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[(*oi)++] = (char)(0x80 | (cp & 0x3F));
  }
}
static inline const char* xjfSkipWs(const char* p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}
// Read a JSON string starting AT the opening '"'. If out!=NULL, write unescaped
// bytes (NUL-terminated, TRUNCATED to cap-1 = strlcpy semantics, matching the
// old strlcpy(result, v.as<const char*>(), resultSize)). Truncation never fails
// the parse: scanning continues to the closing quote so the caller position
// stays well-formed. Returns pointer PAST the closing '"', or NULL only if
// unterminated/malformed.
static const char* xjfReadString(const char* p, char* out, size_t cap) {
  if (*p != '"') return NULL;
  p++;
  size_t oi = 0;
  bool full = false;
  while (*p && *p != '"') {
    if (*p == '\\') {
      char e = p[1];
      if (!e) return NULL;
      if (e == 'u') {
        // Bound the 4-digit read FIRST. p is NUL-terminated; a body truncated
        // mid-escape ("...\u", "...\u1") would otherwise let the comma-operator
        // reads of p[3..5] run past the terminator (the old form evaluated all
        // four xjfHex() before the <0 guard). The || short-circuits at the first
        // NUL, so no byte past the terminator is ever dereferenced. Matches the
        // host oracle's `i+5 >= len -> None` bound (test_extract_json_field.py).
        if (!p[2] || !p[3] || !p[4] || !p[5]) return NULL;
        int h0 = xjfHex(p[2]), h1 = xjfHex(p[3]), h2 = xjfHex(p[4]), h3 = xjfHex(p[5]);
        if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return NULL;
        uint32_t cp = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
        p += 6;
        // Surrogate-pair combining (RFC 8259): a non-BMP code point arrives as a
        // UTF-16 pair \uD800-DBFF (high) + \uDC00-DFFF (low). Combine into the one
        // supplementary code point (emitted as 4-byte UTF-8 by xjfPutUtf8). A high
        // surrogate not followed by a valid low, or a lone low surrogate, is
        // replaced with U+FFFD — matching the reverted ArduinoJson path and lenient
        // decoders, so an unpaired surrogate never lands on the wire as invalid UTF-8.
        if (cp >= 0xD800 && cp <= 0xDBFF) {
          if (p[0] == '\\' && p[1] == 'u' && p[2] && p[3] && p[4] && p[5]) {
            int g0 = xjfHex(p[2]), g1 = xjfHex(p[3]), g2 = xjfHex(p[4]), g3 = xjfHex(p[5]);
            if (g0 >= 0 && g1 >= 0 && g2 >= 0 && g3 >= 0) {
              uint32_t lo = (uint32_t)((g0 << 12) | (g1 << 8) | (g2 << 4) | g3);
              if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                p += 6;                            // consumed the low surrogate too
              } else { cp = 0xFFFDu; }             // high not followed by a valid low
            } else { cp = 0xFFFDu; }
          } else { cp = 0xFFFDu; }                 // high not followed by \u
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
          cp = 0xFFFDu;                            // lone low surrogate
        }
        if (out && !full) {
          size_t before = oi;
          xjfPutUtf8(out, &oi, cap, cp);
          if (oi == before) full = true;          // didn't fit -> truncate here on
        }
        continue;
      }
      char c;
      switch (e) {
        case '"':  c = '"';  break;
        case '\\': c = '\\'; break;
        case '/':  c = '/';  break;
        case 'b':  c = '\b'; break;
        case 'f':  c = '\f'; break;
        case 'n':  c = '\n'; break;
        case 'r':  c = '\r'; break;
        case 't':  c = '\t'; break;
        default:   c = e;    break;                // unknown escape -> literal char
      }
      if (out && !full) {
        if (oi + 1 <= cap - 1) out[oi++] = c;
        else full = true;
      }
      p += 2;
      continue;
    }
    if (out && !full) {
      if (oi + 1 <= cap - 1) out[oi++] = *p;
      else full = true;
    }
    p++;
  }
  if (*p != '"') return NULL;                       // unterminated
  if (out) out[oi] = '\0';
  return p + 1;
}
// Skip a value that begins at '{' or '['; balanced, respecting string contents
// (so braces/brackets inside a string don't unbalance the count). Returns the
// pointer past the matching closer, or NULL if unbalanced/unterminated.
static const char* xjfSkipValue(const char* p) {
  int depth = 0;
  while (*p) {
    char c = *p;
    if (c == '"') { p = xjfReadString(p, NULL, 0); if (!p) return NULL; continue; }
    if (c == '{' || c == '[') { depth++; p++; continue; }
    if (c == '}' || c == ']') { depth--; p++; if (depth == 0) return p; continue; }
    p++;
  }
  return NULL;
}
// Pure-char* core (host-tested). 'key' is already resolved out of PROGMEM.
static bool extractJsonFieldImpl(const char* json, const char* key,
                                 char* result, size_t resultSize) {
  if (!json || !key || !result || resultSize == 0) return false;
  result[0] = '\0';
  const char* p = json;
  while (*p && *p != '{') p++;                      // tolerate leading junk
  if (*p != '{') return false;
  p++;
  for (;;) {
    p = xjfSkipWs(p);
    if (*p == '}' || *p == '\0') return false;      // end of object: not found
    if (*p != '"') return false;                    // malformed at key position
    char kbuf[48];
    const char* after = xjfReadString(p, kbuf, sizeof(kbuf));
    if (!after) return false;
    p = xjfSkipWs(after);
    if (*p != ':') return false;
    p = xjfSkipWs(p + 1);
    bool match = (strcmp(kbuf, key) == 0);
    if (*p == '"') {                                // string value
      if (match) return (xjfReadString(p, result, resultSize) != NULL);
      const char* np = xjfReadString(p, NULL, 0);   // skip wholesale
      if (!np) return false;
      p = np;
    } else if (*p == '{' || *p == '[') {            // container value
      const char* np = xjfSkipValue(p);
      if (!np) return false;
      if (match) return false;                      // matched, but not a scalar
      p = np;
    } else {                                        // bare token: number/true/false/null
      const char* start = p;
      while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\t'
             && *p != '\n' && *p != '\r') p++;
      size_t len = (size_t)(p - start);
      if (match) {
        if (len == 4 && strncmp_P(start, PSTR("null"), 4) == 0) return false;
        if (len > resultSize - 1) len = resultSize - 1;   // strlcpy-style truncation
        memcpy(result, start, len);
        result[len] = '\0';
        return true;
      }
    }
    p = xjfSkipWs(p);
    if (*p == ',') { p++; continue; }
    return false;                                   // '}' / end / junk -> not found
  }
}

bool extractJsonField(const char* json, const __FlashStringHelper* key,
                      char* result, size_t resultSize) {
  char keyBuf[48];
  strncpy_P(keyBuf, (PGM_P)key, sizeof(keyBuf));
  keyBuf[sizeof(keyBuf) - 1] = '\0';
  return extractJsonFieldImpl(json, keyBuf, result, resultSize);
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
