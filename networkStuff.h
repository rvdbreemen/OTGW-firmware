/*
***************************************************************************  
**  Program : networkStuff.h
**  Version : 2.0  10-05-2020
**
**  Copyright (c) 2020 Willem Aandewiel
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


#include <ESP8266WiFi.h>        //ESP8266 Core WiFi Library         
#include <ESP8266WebServer.h>   // Version 1.0.0 - part of ESP8266 Core https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>        // part of ESP8266 Core https://github.com/esp8266/Arduino

#include <WiFiUdp.h>            // part of ESP8266 Core https://github.com/esp8266/Arduino
//#include "ESP8266HTTPUpdateServer.h"
#include "ModUpdateServer.h"   // https://github.com/mrWheel/ModUpdateServer
#include "updateServerHtml.h"
#include <WiFiManager.h>        // version 0.16.0 - https://github.com/tzapu/WiFiManager
// included in main program: #include <TelnetStream.h>       // Version 0.0.1 - https://github.com/jandrassy/TelnetStream
#include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino


ESP8266WebServer        httpServer (80);
ESP8266HTTPUpdateServer httpUpdater(true);


static      FSInfo SPIFFSinfo;
bool        SPIFFSmounted; 
bool        isConnected = false;

//gets called when WiFiManager enters configuration mode
//===========================================================================================
void configModeCallback (WiFiManager *myWiFiManager) 
{
  DebugTln(F("Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  //if you used auto generated SSID, print it
  DebugTln(myWiFiManager->getConfigPortalSSID());

} // configModeCallback()


//===========================================================================================
void startWiFi(const char* hostname, int timeOut) 
{
  WiFiManager manageWiFi;
  uint32_t lTime = millis();
  String thisAP = String(hostname) + "-" + WiFi.macAddress();

  Serial.println("Start Wifi ...");
  manageWiFi.setDebugOutput(true);

  //--- next line in release needs to be commented out!
  // manageWiFi.resetSettings();

  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  //--- sets timeout until configuration portal gets turned off
  //--- useful to make it all retry or go to sleep in seconds
  //manageWiFi.setTimeout(240);  // 4 minuten
  manageWiFi.setTimeout(timeOut);  // in seconden ...
  
  //--- fetches ssid and pass and tries to connect
  //--- if it does not connect it starts an access point with the specified name
  //--- here  "<HOSTNAME>-<MAC>"
  //--- and goes into a blocking loop awaiting configuration
  if (!manageWiFi.autoConnect(thisAP.c_str()))
  {
    //-- fail to connect? Have you tried turning it off and on again? 
    DebugTln(F("failed to connect and hit timeout"));
    delay(2000);  // Enough time for messages to be sent.
    ESP.restart();
    delay(5000);  // Enough time to ensure we don't return.
  }
  
  Debugln();
  DebugT(F("Connected to " )); Debugln (WiFi.SSID());
  DebugT(F("IP address: " ));  Debugln (WiFi.localIP());
  DebugT(F("IP gateway: " ));  Debugln (WiFi.gatewayIP());
  Debugln();

  httpUpdater.setup(&httpServer);
  httpUpdater.setIndexPage(UpdateServerIndex);
  httpUpdater.setSuccessPage(UpdateServerSuccess);
  DebugTf(" took [%d] seconds => OK!\r\n", (millis() - lTime) / 1000);
  
} // startWiFi()


//===========================================================================================
void startTelnet() 
{
  TelnetStream.begin();
  DebugTln(F("\nTelnet server started .."));
  TelnetStream.flush();
} // startTelnet()


//=======================================================================
void startMDNS(const char *Hostname) 
{
  DebugTf("[1] mDNS setup as [%s.local]\r\n", Hostname);
  if (MDNS.begin(Hostname))               // Start the mDNS responder for Hostname.local
  {
    DebugTf("[2] mDNS responder started as [%s.local]\r\n", Hostname);
  } 
  else 
  {
    DebugTln(F("[3] Error setting up MDNS responder!\r\n"));
  }
  MDNS.addService("http", "tcp", 80);
  
} // startMDNS()

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
