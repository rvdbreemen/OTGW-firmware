/*
***************************************************************************  
**  Program : networkStuff.h
**
**  Version  : v0.9.5
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
**      Usage:
**      
**      #define HOSTNAME  thisProject      
**      
**      setup()
**      {
**        startTelnet();
**        startWiFi(_HOSTNAME, 240);  // timeout 4 minuten
**        startMDNS(_HOSTNAME);
**        httpServer.on("/index",     <sendIndexPage>);
**        httpServer.on("/index.html",<sendIndexPage>);
**        httpServer.begin();
**      }
**      
**      loop()
**      {
**        handleWiFi();
**        MDNS.update();
**        httpServer.handleClient();
**        .
**        .
**      }
*/


#include <ESP8266WiFi.h>        // ESP8266 Core WiFi Library         
#include <ESP8266WebServer.h>   // Version 1.0.0 - part of ESP8266 Core https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>        // part of ESP8266 Core https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
#include <ESP8266LLMNR.h>

#include <WiFiUdp.h>            // part of ESP8266 Core https://github.com/esp8266/Arduino
//#include "ESP8266HTTPUpdateServer.h"
#include "OTGW-ModUpdateServer.h"   // <<special version for Nodoshop Watchdog needed>>
#include "updateServerHtml.h"
#include <WiFiManager.h>        // version 2.0.4-beta - use latest development branch  - https://github.com/tzapu/WiFiManager
// included in main program: #include <TelnetStream.h>       // Version 0.0.1 - https://github.com/jandrassy/TelnetStream

//#include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino
#include <LittleFS.h>

//Use the NTP SDK ESP 8266 
#include <time.h>
extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

enum NtpStatus_t {
	TIME_NOTSET,
	TIME_SYNC,
	TIME_WAITFORSYNC,
  TIME_NEEDSYNC
};

NtpStatus_t NtpStatus = TIME_NOTSET;
static const time_t EPOCH_2000_01_01 = 946684800;
time_t NtpLastSync = 0; //last sync moment in EPOCH seconds

ESP8266WebServer        httpServer (80);
ESP8266HTTPUpdateServer httpUpdater(true);

static      FSInfo LittleFSinfo;
bool        LittleFSmounted; 
bool        isConnected = false;

#define WM_DEBUG_PORT OTGWSerial

//gets called when WiFiManager enters configuration mode
//===========================================================================================
void configModeCallback (WiFiManager *myWiFiManager) 
{
  DebugTln(F("Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  //if you used auto generated SSID, print it
  DebugTln(myWiFiManager->getConfigPortalSSID());

} // configModeCallback()

void resetWiFiSettings(void)
{
  // Is it safe to re-setup this object? other one only lives in 
  // startWiFi.
  WiFiManager manageWiFi;
  manageWiFi.resetSettings();
}

//===========================================================================================
void startWiFi(const char* hostname, int timeOut) 
{    
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  WiFiManager manageWiFi;
  uint32_t lTime = millis();
  String thisAP = String(hostname) + "-" + WiFi.macAddress();

  OTGWSerial.println("Start Wifi ...");
  manageWiFi.setDebugOutput(true);

  //--- next line in release needs to be commented out!
  // manageWiFi.resetSettings();

  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  //--- sets timeout until configuration portal gets turned off
  //--- useful to make it all retry or go to sleep in seconds
  //manageWiFi.setTimeout(240);  // 4 minuten
  manageWiFi.setTimeout(timeOut);  // in seconden ...

  //--- remove Info and Update buttons from Configuration Portal (security improvement 20230102)
  std::vector<const char *> wm_menu  = {"wifi", "exit"};
  manageWiFi.setShowInfoUpdate(false);
  manageWiFi.setShowInfoErase(false);
  manageWiFi.setMenu(wm_menu);
  
  //--- fetches ssid and pass and tries to connect
  //--- if it does not connect it starts an access point with the specified name
  //--- here  "<HOSTNAME>-<MAC>"
  //--- and goes into a blocking loop awaiting configuration
  OTGWSerial.printf("AutoConnect to: %s\r\n", thisAP.c_str());
  if (!manageWiFi.autoConnect(thisAP.c_str()))
  {
    //-- fail to connect? Have you tried turning it off and on again? 
    DebugTln(F("failed to connect and hit timeout"));
    delay(2000);  // Enough time for messages to be sent.
    ESP.restart();
    delay(5000);  // Enough time to ensure we don't return.
  }

  // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Debugln();
  DebugT(F("Connected to " )); Debugln(WiFi.SSID());
  DebugT(F("IP address: " ));  Debugln(WiFi.localIP());
  DebugT(F("IP gateway: " ));  Debugln(WiFi.gatewayIP());
  Debugln();

  httpUpdater.setup(&httpServer);
  httpUpdater.setIndexPage(UpdateServerIndex);
  httpUpdater.setSuccessPage(UpdateServerSuccess);
  DebugTf(" took [%lu] seconds => OK!\r\n", (millis() - lTime) / 1000);
  
} // startWiFi()

//===========================================================================================
void startTelnet() 
{
  OTGWSerial.print(F("\r\nUse  'telnet "));
  OTGWSerial.print(WiFi.localIP());
  OTGWSerial.println(F("' for debugging"));
  TelnetStream.begin();
  DebugTln(F("\nTelnet server started .."));
  TelnetStream.flush();
} // startTelnet()

//=======================================================================
void startMDNS(const char *hostname) 
{
  DebugTf("mDNS setup as [%s.local]\r\n", hostname);
  if (MDNS.begin(hostname))               // Start the mDNS responder for Hostname.local
  {
    DebugTf("mDNS responder started as [%s.local]\r\n", hostname);
  } 
  else 
  {
    DebugTln(F("Error setting up MDNS responder!\r\n"));
  }
  MDNS.addService("http", "tcp", 80);
} // startMDNS()

void startLLMNR(const char *hostname) 
{
  DebugTf("LLMNR setup as [%s]\r\n", hostname);
  if (LLMNR.begin(hostname))               // Start the LLMNR responder for hostname
  {
    DebugTf("LLMNR responder started as [%s]\r\n", hostname);
  } 
  else 
  {
    DebugTln(F("Error setting up LLMNR responder!\r\n"));
  }
} // startLLMNR()


//==[ NTP stuff ]==============================================================


void startNTP(){
  // Initialisation ezTime
  if (!settingNTPenable) return;
  if (settingNTPtimezone.length()==0) settingNTPtimezone = NTP_DEFAULT_TIMEZONE; //set back to default timezone
  if (settingNTPhostname.length()==0) settingNTPhostname = NTP_HOST_DEFAULT; //set back to default timezone

  //void configTime(int timezone_sec, int daylightOffset_sec, const char* server1, const char* server2, const char* server3)
  configTime(0, 0, CSTR(settingNTPhostname), nullptr, nullptr);
  // Configure NTP before WiFi, so DHCP can override the NTP server(s)
  
  NtpStatus = TIME_WAITFORSYNC;
}


void getNTPtime(){
  struct timespec tp;   //to enable clock_gettime()
  double tNow;
  long dt_sec, dt_ms, dt_nsec;
  clock_gettime(CLOCK_REALTIME, &tp);  
  tNow = tp.tv_sec+(tp.tv_nsec/1.0e9);
  dt_sec = tp.tv_sec;
  dt_ms = tp.tv_nsec / 1000000UL;
  dt_nsec = tp.tv_nsec;
  DebugTf("tNow=%20.10f tNow_sec=%16.10ld tNow_nsec=%16.10ld dt_sec=%16li(s) dt_msec=%16li(sm) dt_nsec=%16li(ns)\r\n", (double)tNow, tp.tv_sec,tp.tv_nsec, dt_sec, dt_ms, dt_nsec);
  DebugFlush();
}

void loopNTP(){
time_t now;
now = time(nullptr); //this is now...
if (!settingNTPenable) return;
  switch (NtpStatus){
    case TIME_NOTSET:
    case TIME_NEEDSYNC:
      NtpLastSync = now; //remember last sync
      DebugTln(F("Start time syncing"));
      startNTP();
      DebugTf("Starting timezone lookup for [%s]\r\n", CSTR(settingNTPtimezone));
      NtpStatus = TIME_WAITFORSYNC;
      break;
    case TIME_WAITFORSYNC:
      if ((now > EPOCH_2000_01_01) && (now >= NtpLastSync)) { 
        //DebugTf("Waited for sync: epoch: %lld\r\n", time(nullptr));
        NtpLastSync = now; //remember last sync         
        TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
        if (myTz.isError()){
          DebugTf("Error: Timezone Invalid/Not Found: [%s]\r\n", CSTR(settingNTPtimezone));
          settingNTPtimezone = NTP_DEFAULT_TIMEZONE;
          myTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone)); //try with default Timezone instead
        } else {
          //found the timezone, now set the time 
          ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
          DebugTf("%02d:%02d:%02d %02d-%02d-%04d\n\r", myTime.hour(), myTime.minute(), myTime.second(), myTime.day(), myTime.month(), myTime.year());
          if (!myTime.isError()) {
            //finally time is synced!
            //setTime(myTime.hour(), myTime.minute(), myTime.second(), myTime.day(), myTime.month(), myTime.year());
            NtpStatus = TIME_SYNC;
            DebugTln(F("Time synced!"));
          }
        }
      } 
    break;
    case TIME_SYNC:
      if ((now -  NtpLastSync) > NTP_RESYNC_TIME){
        //when xx seconds have passed, resync using NTP
         DebugTln(F("Time resync needed"));
        NtpStatus = TIME_NEEDSYNC;
      }
    break;
  } 
 
  // DECLARE_TIMER_SEC(timerNTPtime, 10, CATCH_UP_MISSED_TICKS);
  // if DUE(timerNTPtime) 
  // {
  //   //DebugTf("Epoch Seconds: %lld\r\n", now); //timeout, then break out of this loop
  //   DebugT("Now: ");
  //   Debug(now);
  //   Debugln();
  //   DebugT("Timezone : ");
  //   Debugln(CSTR(settingNTPtimezone));
  //   TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  //   ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
    
  //   DebugTf("%02d:%02d:%02d %02d-%02d-%04d\r\n", myTime.hour(), myTime.minute(), myTime.second(), myTime.day(), myTime.month(), myTime.year());
  // }
  // if DUE(timerNTPtime) getNTPtime();
}

bool isNTPtimeSet(){
  return NtpStatus == TIME_SYNC;
}

// void waitforNTPsync(int16_t timeout = 30){  
//   //wait for time is synced to NTP server, for maximum of timeout seconds
//   //feed the watchdog while waiting 
//   //update NTP status
//   time_t t = time(nullptr); //get current time
//   DebugTf("Waiting for NTP sync, timeout: %d\r\n", timeout);
//   DECLARE_TIMER_SEC(waitforNTPsync, timeout, CATCH_UP_MISSED_TICKS);
//   DECLARE_TIMER_SEC(timerWaiting, 5, CATCH_UP_MISSED_TICKS);
//   while (true){
//     //feed the watchdog while waiting
//     Wire.beginTransmission(0x26);   
//     Wire.write(0xA5);   
//     Wire.endTransmission();
//     delay(100);
//     if DUE(timerWaiting) DebugTf("Waiting for NTP sync: %lu seconds\r\n", (time(nullptr)-t));
//     // update NTP status
//     loopNTP();
//     //stop waiting when NTP is synced 
//     if (isNTPtimeSet()) {
//       Debugln(F("NTP time synced!"));
//       break;
//     }
//     //stop waiting when timeout is reached 
//     if DUE(waitforNTPsync) {
//       DebugTln(F("NTP sync timeout!"));
//       break;
//     } 
//   }
// }


//==[ end of NTP stuff ]=======================================================

String getMacAddress() {
  uint8_t baseMac[6];
  char baseMacChr[13] = {0};
#  if defined(ESP8266)
  WiFi.macAddress(baseMac);
  sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
#  elif defined(ESP32)
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
#  else
  sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#  endif
  return String(baseMacChr);
}

String getUniqueId() {
  String uniqueId = "otgw-"+(String)getMacAddress();
  return String(uniqueId);
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
