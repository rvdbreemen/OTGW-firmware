
//#include <ESP8266WiFi.h>

#include <ezTime.h>             // https://github.com/ropg/ezTime
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#include <ArduinoJson.h>        // https://arduinojson.org/
#include "Debug.h"
#include "safeTimers.h"
#include "networkStuff.h"
#include "OTGWStuff.h"
#include "Wire.h"


#define SETTINGS_FILE   "/settings.ini"
#define CMSG_SIZE        512
#define JSON_BUFF_MAX   1024

bool      Verbose = false;
char      cMsg[CMSG_SIZE];
char      fChar[10];
String    lastReset   = "";
uint32_t  blinkyTimer;
char      settingHostname[41];
uint32_t   upTimeSeconds=0;
Timezone    CET; 

const char *weekDayName[]  {  "Unknown", "Zondag", "Maandag", "Dinsdag", "Woensdag"
                            , "Donderdag", "Vrijdag", "Zaterdag", "Unknown" };
const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

// eof
