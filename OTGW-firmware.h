/* 
***************************************************************************  
**  Program  : OTGW-firmware.h
**  Version  : v0.9.5
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <Arduino.h>
//#include <ezTime.h>             // https://github.com/ropg/ezTime

#include <AceTime.h>
#include <TimeLib.h>

#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#include <ArduinoJson.h>        // https://arduinojson.org/
#include "Wire.h"
#include "Debug.h"
#include "safeTimers.h"
#include "OTGWSerial.h"         // Bron Schelte's Serial class - it upgrades and more
#include "OTGW-Core.h"          // Core code for this firmware 
#include <OneWire.h>            // required for Dallas sensor library
#include <DallasTemperature.h>  // Miles Burton's - Arduino Dallas library

//OTGW Nodoshop hardware definitions
#define I2CSCL D1
#define I2CSDA D2
#define BUTTON D3
#define PICRST D5

#define LED1 D4
#define LED2 D0

#define PICFIRMWARE "/gateway.hex"

OTGWSerial OTGWSerial(PICRST, LED2);
void fwupgradestart(const char *hexfile);

void blinkLEDnow();
void blinkLEDnow(uint8_t);
void setLed(int8_t, uint8_t);

//Defaults and macro definitions
#define _HOSTNAME       "OTGW"
#define SETTINGS_FILE   "/settings.ini"
#define NTP_DEFAULT_TIMEZONE "Europe/Amsterdam"
#define NTP_HOST_DEFAULT "pool.ntp.org"
#define NTP_RESYNC_TIME 1800 //seconds = every 30 minutes
#define HOME_ASSISTANT_DISCOVERY_PREFIX   "homeassistant"  // Home Assistant discovery prefix
#define CMSG_SIZE 512
#define JSON_BUFF_MAX   1024
#define CSTR(x) x.c_str()
#define CONLINEOFFLINE(x) (x?"online":"offline")
#define CBOOLEAN(x) (x?"true":"false")
#define CONOFF(x) (x?"On":"Off")
#define CCONOFF(x) (x?"ON":"OFF")
#define CBINARY(x) (x?"1":"0")
#define EVALBOOLEAN(x) (strcasecmp(x,"true")==0||strcasecmp(x,"on")==0||strcasecmp(x,"1")==0)


//prototype
void sendMQTTData(const String, const String, const bool);
void sendMQTTData(const char*, const char*, const bool);
void addOTWGcmdtoqueue(const char* ,  int , const bool);

//Global variables
WiFiClient  wifiClient;
char        cMsg[CMSG_SIZE];
char        fChar[10];
String      lastReset = "";
uint32_t    upTimeSeconds = 0;
uint32_t    rebootCount = 0;
String      sMessage = "";    
uint32_t    MQTTautoConfigMap[8] = { 0 };

//Use acetime
using namespace ace_time;
static BasicZoneProcessor timeProcessor;
static const int CACHE_SIZE = 3;
// static BasicZoneManager<CACHE_SIZE> manager(zonedb::kZoneRegistrySize, zonedb::kZoneRegistry);
static BasicZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static BasicZoneManager timezoneManager(zonedb::kZoneRegistrySize, zonedb::kZoneRegistry, zoneProcessorCache);

const char *weekDayName[]  {  "Unknown", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Unknown" };
const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

//Information on OTGW 
String    sPICfwversion = ""; 
String    sPICdeviceid = "";
String    errorupgrade = ""; 
bool      bOTGWonline = true;
bool      bOTGWboilerstate = false;
bool      bOTGWthermostatstate = false;
bool      bOTGWgatewaystate = false;
bool      bPrintSummarymode = false;  //default to PS=0 mode
bool      bCheckOTGWPICupdate = true;  

//All things that are settings 
String    settingHostname = _HOSTNAME;

//MQTT settings
bool      statusMQTTconnection = false; 
bool      settingMQTTenable = true;
bool      settingMQTTsecure = false; 
String    settingMQTTbroker= "192.168.88.254";
int16_t   settingMQTTbrokerPort = 1883; 
String    settingMQTTuser = "";
String    settingMQTTpasswd = "";
String    settingMQTThaprefix = HOME_ASSISTANT_DISCOVERY_PREFIX;
bool      settingMQTTharebootdetection = true;
String    settingMQTTtopTopic = "otgw";
String    settingMQTTuniqueid = ""; // Intialized in readsettings
bool      settingMQTTOTmessage = false;
bool      settingNTPenable = true;
String    settingNTPtimezone = NTP_DEFAULT_TIMEZONE;
String    settingNTPhostname = NTP_HOST_DEFAULT;
bool      settingLEDblink = true;

// GPIO Sensor Settings
bool      settingGPIOSENSORSenabled = false;
int8_t    settingGPIOSENSORSpin = 13;            // GPIO 13 = D7, GPIO 10 = SDIO 3  
int16_t   settingGPIOSENSORSinterval = 20;       // Interval time to read out temp and send to MQ
byte      OTGWdallasdataid = 246;                // foney dataid to be used to do autoconfigure for temp sensors
int       DallasrealDeviceCount = 0;             // Total temperature devices found on the bus
#define   MAXDALLASDEVICES 16                    // maximum number of devices on the bus

// Define structure to store temperature device addresses found on bus with their latest tempC value
struct
{
  int id;
  DeviceAddress addr;
  float tempC;
  time_t lasttime;  
} DallasrealDevice[MAXDALLASDEVICES];
// prototype to allow use in restAPI.ino
char* getDallasAddress(DeviceAddress deviceAddress);


// S0 Counter Settings and variables with global scope
bool      settingS0COUNTERenabled = false;      
uint8_t   settingS0COUNTERpin = 12;               // GPIO 12 = D6, preferred, can be any pin with Interupt support
uint16_t  settingS0COUNTERdebouncetime = 80;      // Depending on S0 switch a debouncetime should be tailored
uint16_t  settingS0COUNTERpulsekw = 1000;         // Most S0 counters have 1000 pulses per kW, but this can be different
uint16_t  settingS0COUNTERinterval = 60;          // Sugggested measurement reporting interval
uint16_t  OTGWs0pulseCount;                       // Number of S0 pulses in measurement interval
uint32_t  OTGWs0pulseCountTot = 0;                // Number of S0 pulses since start of measurement
float     OTGWs0powerkw = 0 ;                     // Calculated kW actual consumption based on time between last pulses and settings
time_t    OTGWs0lasttime = 0;                     // Last time S0 counters have been read
byte      OTGWs0dataid = 245;                     // foney dataid to be used to do autoconfigure for counter

//boot commands
bool      settingOTGWcommandenable = false;
String    settingOTGWcommands = "";

//debug flags 
bool      bDebugOTmsg = false;    
bool      bDebugRestAPI = false;
bool      bDebugMQTT = false;
bool      bDebugSensors = false;

//GPIO Output Settings
bool      settingMyDEBUG = false;
bool      settingGPIOOUTPUTSenabled = false;
int8_t    settingGPIOOUTPUTSpin = 16;
int8_t    settingGPIOOUTPUTStriggerBit = 0;

//Now load network suff
#include "networkStuff.h"

    // That's all folks...

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
