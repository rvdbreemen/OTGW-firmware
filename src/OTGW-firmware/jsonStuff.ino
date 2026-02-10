/* 
***************************************************************************  
**  Program  : jsonStuff
**  Version  : v1.1.0-beta
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
  char jsonBuff[JSON_BUFF_MAX] = "";
  
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("{\"name\": \"%s\", \"value\": \"%s\"}"), cName, cValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
} // sendNestedJsonObj(*char, *char)
//=======================================================================
void sendNestedJsonObj(const char *cName, String sValue)
{
  char jsonBuff[JSON_BUFF_MAX] = "";
  
  if (sValue.length() > (JSON_BUFF_MAX - 65) )
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
  char jsonBuff[JSON_BUFF_MAX] = "";
  
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
  char jsonBuff[JSON_BUFF_MAX] = "";

  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": \"%s\""), cName, cValue);

  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void sendJsonMapEntry(const char *cName, String sValue)
{
  char jsonBuff[JSON_BUFF_MAX] = "";
  
  if (sValue.length() > (JSON_BUFF_MAX - 65) )
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
  char jsonBuff[JSON_BUFF_MAX] = "";
  
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
