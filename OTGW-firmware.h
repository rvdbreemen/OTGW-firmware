/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v0.4.2
**
**  Copyright (c) 2020 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//#include <ESP8266WiFi.h>

#include <ezTime.h>             // https://github.com/ropg/ezTime
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#include <ArduinoJson.h>        // https://arduinojson.org/
#include "Debug.h"
#include "safeTimers.h"
#include "networkStuff.h"
#include "OTGWStuff.h"
#include "Wire.h"

#define _HOSTNAME   "OTGW"
#define SETTINGS_FILE   "/settings.ini"
#define CMSG_SIZE 512
#define JSON_BUFF_MAX   1024
#define CSTR(x) x.c_str()

WiFiClient  wifiClient;
bool        Verbose = false;
char        cMsg[CMSG_SIZE];
char        fChar[10];
String      lastReset   = "";
uint32_t    upTimeSeconds=0;
uint32_t    rebootCount=0;
Timezone    CET; 

const char *weekDayName[]  {  "Unknown", "Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag", "Unknown" };
const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };


//All things that are settings 
char      settingHostname[64];
//MQTT settings
String    settingMQTTbroker= "192.168.88.254";
int32_t   settingMQTTbrokerPort = 1883;
String    settingMQTTuser = "";
String    settingMQTTpasswd = "";
String    settingMQTTtopTopic = "OTGW";

// eof
