/* 
***************************************************************************  
**  Program  : jsonStuff
**  Version  : v1.2.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// Helper function to escape JSON string values
// Replaces: " with \", \ with \\, control chars with \uXXXX
String escapeJsonString(const char* str) {
  if (!str) return String("");
  
  String result;
  result.reserve(strlen(str) + 10); // Reserve some extra space for escapes
  
  for (const char* p = str; *p; p++) {
    switch (*p) {
      case '"':  result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\b': result += "\\b";  break;
      case '\f': result += "\\f";  break;
      case '\n': result += "\\n";  break;
      case '\r': result += "\\r";  break;
      case '\t': result += "\\t";  break;
      default:
        if (*p < 0x20) {
          // Control character - use \uXXXX notation
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04X", (unsigned char)*p);
          result += buf;
        } else {
          result += *p;
        }
    }
  }
  return result;
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
  httpServer.send_P(200, PSTR("application/json"), PSTR(" "));
  httpServer.sendContent(sBuff);
  iIdentlevel++;
  bFirst = true;

} // sendStartJsonObj()


//=======================================================================
void sendEndJsonObj(const char *objName)
{
  iIdentlevel--;
  if (strlen(objName)==0){  
    httpServer.sendContent_P(PSTR("\r\n}\r\n"));
  } else {
    httpServer.sendContent_P(PSTR("\r\n]}\r\n"));
  }

} // sendEndJsonObj()
//=======================================================================
void sendStartJsonArray()
{
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send_P(200, PSTR("application/json"), PSTR(" "));
  httpServer.sendContent_P(PSTR("[\r\n"));
  iIdentlevel++;
  bFirst = true;

} // sendStartJsonObj()


//=======================================================================
void sendEndJsonArray()
{
  iIdentlevel--;
  httpServer.sendContent_P(PSTR("\r\n]\r\n"));
} // sendEndJsonObj()
//=======================================================================
void sendIdent(){
  for (int i = iIdentlevel; i >0; i--){
    httpServer.sendContent_P(PSTR("  "));
  }
}  //sendIdent()
//=======================================================================
void sendBeforenext(){
  if (!bFirst){ 
    httpServer.sendContent_P(PSTR(",\r\n"));
  }
  bFirst = false;
} //sendBeforenext()
//=======================================================================
void sendNestedJsonObj(const char *cName, const char *cValue)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": \"%s\"}"), cName, cValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
} // sendNestedJsonObj(*char, *char)
//=======================================================================
void sendNestedJsonObj(const char *cName, String sValue)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";

  if (sValue.length() > (JSON_ENTRY_BUF - 65) )
  {
    DebugTf(PSTR("[2] sValue.length() [%d]\r\n"), sValue.length());
  }
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": \"%s\"}"), cName, sValue.c_str());

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendNestedJsonObj(*char, String)


//=======================================================================
void sendNestedJsonObj(const char *cName, int32_t iValue)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %d}"), cName, iValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendNestedJsonObj(*char, int)

//=======================================================================
void sendNestedJsonObj(const char *cName, uint32_t uValue)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %u}"), cName, uValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
  
} // sendNestedJsonObj(*char, uint)


//=======================================================================
void sendNestedJsonObj(const char *cName, float fValue)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.3f}"), cName, fValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendNestedJsonObj(*char, float)


//============= build OTmonitor string ========================
void sendJsonOTmonObj(const char *cName, const char *cValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": \"%s\", \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, cValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonOTmonObj(*char, *char, *char)

//=======================================================================
void sendJsonOTmonObj(const char *cName, int32_t iValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %d, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, iValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonOTmonObj(*char, int, *char)

//=======================================================================
void sendJsonOTmonObj(const char *cName, uint32_t uValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %u, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, uValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendNestedJsonObj(*char, uint, *char, time_t)


//=======================================================================
void sendJsonOTmonObj(const char *cName, float fValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.3f, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, fValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonOTmonObj(*char, float, *char, time_t)
//=======================================================================
void sendJsonOTmonObj(const char *cName, bool bValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %s, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, CBOOLEAN(bValue), cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonOTmonObj(*char, bool, *char, time_t)

//=======================================================================
// Dallas temperature-specific helper (1 decimal precision)
//=======================================================================
void sendJsonOTmonObjDallasTemp(const char *cName, float fValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.1f, \"unit\": \"%s\", \"type\": \"dallas\", \"epoch\": %d}")
                                      , cName, fValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonOTmonObjDallasTemp(*char, float, *char, time_t)

void sendJsonOTmonObjDallasTemp(const char *cName, float fValue, const __FlashStringHelper *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.1f, \"unit\": \"%S\", \"type\": \"dallas\", \"epoch\": %d}")
                                      , cName, fValue, reinterpret_cast<PGM_P>(cUnit), (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonOTmonObjDallasTemp(*char, float, *FlashStringHelper, time_t)

//=======================================================================
// New Map-based output functions for less redundant JSON
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
  httpServer.send_P(200, PSTR("application/json"), PSTR(" "));
  httpServer.sendContent(sBuff);
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
  httpServer.sendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, int32_t iValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %d"), cName, iValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, uint32_t uValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %u"), cName, uValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, float fValue)
{
  char jsonBuff[120] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %.3f"), cName, fValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, const char *cValue)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": \"%s\""), cName, cValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, String sValue)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";

  if (sValue.length() > (JSON_ENTRY_BUF - 65) )
  {
    DebugTf(PSTR("[2] sValue.length() [%d]\r\n"), sValue.length());
  }
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": \"%s\""), cName, sValue.c_str());

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendEndJsonMap(const char *objName)
{
  iIdentlevel--;
  if (strlen(objName)==0){  
    httpServer.sendContent_P(PSTR("\r\n}\r\n"));
  } else {
    httpServer.sendContent_P(PSTR("\r\n}}\r\n"));
  }
}

void sendJsonOTmonMapEntry(const char *cName, const char *cValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[JSON_ENTRY_BUF] = "";
  
  // Compact format: "name": {"value": "val", "unit": "u", "epoch": 123}
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": \"%s\", \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, cValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, int32_t iValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %d, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, iValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, uint32_t uValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %u, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, uValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, float fValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %.3f, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, fValue, cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonOTmonMapEntry(const char *cName, bool bValue, const char *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %s, \"unit\": \"%s\", \"epoch\": %d}")
                                      , cName, CBOOLEAN(bValue), cUnit, (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
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
  httpServer.sendContent(jsonBuff);
} // sendJsonOTmonMapEntryDallasTemp(*char, float, *char, time_t)

void sendJsonOTmonMapEntryDallasTemp(const char *cName, float fValue, const __FlashStringHelper *cUnit, time_t epoch)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": {\"value\": %.1f, \"unit\": \"%S\", \"type\": \"dallas\", \"epoch\": %d}")
                                      , cName, fValue, reinterpret_cast<PGM_P>(cUnit), (uint32_t)epoch);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
} // sendJsonOTmonMapEntryDallasTemp(*char, float, *FlashStringHelper, time_t)

//=======================================================================
// ************ function to build Json Settings string ******************
//=======================================================================
void sendJsonSettingObj(const char *cName, float fValue, const char *fType, int minValue, int maxValue)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.3f, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, fValue, fType, minValue, maxValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonSettingObj(*char, float, *char, int, int)


//=======================================================================
void sendJsonSettingObj(const char *cName, float fValue, const char *fType, int minValue, int maxValue, int decPlaces)
{
  char jsonBuff[200] = "";

  switch(decPlaces) {
    case 0:
      snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.0f, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, fValue, fType, minValue, maxValue);
      break;
    case 2:
      snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.2f, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, fValue, fType, minValue, maxValue);
      break;
    case 5:
      snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %.5f, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, fValue, fType, minValue, maxValue);
      break;
    default:
      snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %f, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, fValue, fType, minValue, maxValue);

  }

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonSettingObj(*char, float, *char, int, int, int)


//=======================================================================
void sendJsonSettingObj(const char *cName, int iValue, const char *iType, int minValue, int maxValue)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": %d, \"type\": \"%s\", \"min\": %d, \"max\": %d}")
                                      , cName, iValue, iType, minValue, maxValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
} // sendJsonSettingObj(*char, int, *char, int, int)


//=======================================================================

void sendJsonSettingObj(const char *cName, const char *cValue, const char *sType, int maxLen)
{
  char jsonBuff[200] = {0};
  char buffer[100] = {0};

  str_cstrlit(cValue, buffer, sizeof(buffer));
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\":\"%s\", \"type\": \"%s\", \"maxlen\": %d}")
                                      , cName, cValue, sType, maxLen);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);

} // sendJsonSettingObj(*char, *char, *char, int, int)

//=======================================================================
void sendJsonSettingObj(const char *cName, bool bValue, const char *sType)
{
  char jsonBuff[200] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\":\"%s\", \"type\": \"%s\"}")
                                      , cName,  CBOOLEAN(bValue), sType);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
 
} // sendJsonSettingObj(*char, bool, *char)    

//=======================================================================
// Helper Overloads for PROGMEM support
//=======================================================================

// sendNestedJsonObj helpers
template <typename T>
void sendNestedJsonObj(const __FlashStringHelper* cName, T value) {
  char nameBuf[35]; 
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendNestedJsonObj(nameBuf, value);
}

void sendNestedJsonObj(const __FlashStringHelper* cName, const __FlashStringHelper* cValue) {
  char nameBuf[35];
  char valBuf[101]; 
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  strncpy_P(valBuf, (PGM_P)cValue, sizeof(valBuf));
  valBuf[sizeof(valBuf)-1] = 0;
  sendNestedJsonObj(nameBuf, (const char*)valBuf);
}

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

// For: void sendJsonSettingObj(const char *cName, float fValue, const char *fType, int minValue, int maxValue)
void sendJsonSettingObj(const __FlashStringHelper* cName, float fValue, const char *fType, int minValue, int maxValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, fValue, fType, minValue, maxValue);
}

// For: void sendJsonSettingObj(const char *cName, float fValue, const char *fType, int minValue, int maxValue, int decPlaces)
void sendJsonSettingObj(const __FlashStringHelper* cName, float fValue, const char *fType, int minValue, int maxValue, int decPlaces) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, fValue, fType, minValue, maxValue, decPlaces);
}

// For: void sendJsonSettingObj(const char *cName, int iValue, const char *iType, int minValue, int maxValue)
void sendJsonSettingObj(const __FlashStringHelper* cName, int iValue, const char *iType, int minValue, int maxValue) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, iValue, iType, minValue, maxValue);
}

// For: void sendJsonSettingObj(const char *cName, const char *cValue, const char *sType, int maxLen)
void sendJsonSettingObj(const __FlashStringHelper* cName, const char *cValue, const char *sType, int maxLen) {
  char nameBuf[35];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf)-1] = 0;
  sendJsonSettingObj(nameBuf, cValue, sType, maxLen);
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
//       Not safe to call from ISR or concurrently.
//=======================================================================
bool extractJsonField(const String& json, const __FlashStringHelper* key,
                      char* result, size_t resultSize) {
  if (!result || resultSize == 0) return false;
  result[0] = '\0';
  // Build search pattern: "keyname"
  snprintf_P(cMsg, sizeof(cMsg), PSTR("\"%S\""), (PGM_P)key);
  int idx = json.indexOf(cMsg);
  if (idx < 0) return false;

  int colon = json.indexOf(':', idx + (int)strlen(cMsg));
  if (colon < 0) return false;

  int start = colon + 1;
  int len   = (int)json.length();
  while (start < len && json[start] == ' ') start++;
  if (start >= len) return false;

  if (json[start] == '"') {
    // Quoted string value — scan for closing quote respecting backslash escapes
    int pos = start + 1;
    while (pos < len) {
      if (json[pos] == '\\' && pos + 1 < len) { pos += 2; continue; } // skip escaped char
      if (json[pos] == '"') break;
      pos++;
    }
    if (pos >= len) return false; // no closing quote found
    // Copy and unescape value into result (empty string is valid)
    size_t ri = 0;
    for (int si = start + 1; si < pos && ri < resultSize - 1; si++) {
      if (json[si] == '\\' && si + 1 < pos) {
        result[ri++] = unescapeJsonChar(json[++si]);
      } else {
        result[ri++] = json[si];
      }
    }
    result[ri] = '\0';
    return true; // field found; empty string is a valid value
  } else {
    // Unquoted value: bool literal (true/false) or number
    int end = start;
    while (end < len) {
      char c = json[end];
      if (c == ',' || c == '}' || c == ' ' || c == '\r' || c == '\n') break;
      end++;
    }
    int n = end - start;
    if (n <= 0 || (size_t)(n + 1) > resultSize) return false;
    json.substring(start, end).toCharArray(result, resultSize);
    return true;
  }
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

  String escapedKey = escapeJsonString(key);
  String escapedVal = escapeJsonString(val);

  f.print('"');
  f.print(escapedKey);
  f.print(F("\":\""));
  f.print(escapedVal);
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
