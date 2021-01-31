/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v0.7.2
**
**  Copyright (c) 2021 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

/*
 *  How to install the OTGW on your nodeMCU:
 *  Read this: https://github.com/rvdbreemen/OTGW-firmware/wiki/How-to-compile-OTGW-firmware-yourself
 *  
 *  How to upload to your LittleFS?
 *  Read this: https://github.com/rvdbreemen/OTGW-firmware/wiki/Upload-files-to-LittleFS-(filesystem)
 * 
 *  How to compile this firmware?
 *  - NodeMCU v1.0
 *  - Flashsize (4MB - FS:2MB - OTA ~1019KB)
 *  - CPU frequentcy: 160MHz 
 *  - Normal defaults should work fine. 
 *  First time: Make sure to flash sketch + wifi or flash ALL contents.
 *  
 */

#include "version.h"
#define _FW_VERSION _VERSION

#include "OTGW-firmware.h"

//=====================================================================
void setup()
{
  rebootCount = updateRebootCount();

  Serial.println(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  Serial.printf("Booting....[%s]\r\n\r\n", String(_FW_VERSION).c_str());

  Serial.begin(9600, SERIAL_8N1);
  while (!Serial) {} //Wait for OK 

  //setup randomseed the right way
  randomSeed(RANDOM_REG32); //This is 8266 HWRNG used to seed the Random PRNG: Read more: https://config9.com/arduino/getting-a-truly-random-number-in-arduino/

  lastReset     = ESP.getResetReason();
  Serial.printf("Last reset reason: [%s]\r\n", ESP.getResetReason().c_str());

  //setup the status LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); //OFF

  //start the debug port 23
  startTelnet();
  Serial.print("Use  'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println("' for debugging");
  
//================ LittleFS ===========================================
  if (LittleFS.begin()) 
  {
    Serial.println(F("LittleFS Mount succesfull\r"));
    LittleFSmounted = true;
  } else { 
    Serial.println(F("LittleFS Mount failed\r"));   // Serious problem with LittleFS 
    LittleFSmounted = false;
  }

  readSettings(true);

  // Connect to and initialise WiFi network
  Serial.println(F("Attempting to connect to WiFi network\r"));
  digitalWrite(LED_BUILTIN, HIGH);
  startWiFi(_HOSTNAME, 240);  // timeout 240 seconds
  digitalWrite(LED_BUILTIN, LOW);
  
  startMDNS(CSTR(settingHostname));
  
  delay(1000);
  
  // Start MQTT connection
  startMQTT(); 

  // Initialisation ezTime
  setDebug(INFO); 
  updateNTP();        //force NTP sync
  waitForSync(60);    //wait until valid time
  setInterval(1800);  //every 30minutes NTP sync
  //no TZ cached, then try to GeoIP locate your TZ, otherwise fallback to default
  if (!myTZ.setCache(0)) { 
    //ezTime will try to determine your location based on your IP using GeoIP
    if (myTZ.setLocation()) {
      settingTimezone = myTZ.getTimezoneName();
      DebugTf("GeoIP located your timezone to be: %s\n", CSTR(settingTimezone));
    } else {
      if (myTZ.setLocation(settingTimezone)){
        DebugTf("Timezone set to (using default): %s\n", CSTR(settingTimezone));
        settingTimezone = myTZ.getTimezoneName();
      } else DebugTln(errorString());
    }
  }
  myTZ.setDefault();
  
  DebugTln("UTC time  : "+ UTC.dateTime());
  DebugTln("local time: "+ myTZ.dateTime());

//================ Start HTTP Server ================================
  setupFSexplorer();
  if (!LittleFS.exists("/index.html")) {
    httpServer.serveStatic("/",           LittleFS, "/FSexplorer.html");
    httpServer.serveStatic("/index",      LittleFS, "/FSexplorer.html");
    httpServer.serveStatic("/index.html", LittleFS, "/FSexplorer.html");
  } else{
    httpServer.serveStatic("/",           LittleFS, "/index.html");
    httpServer.serveStatic("/index",      LittleFS, "/index.html");
    httpServer.serveStatic("/index.html", LittleFS, "/index.html");
  } 
  // httpServer.on("/",          sendIndexPage);
  // httpServer.on("/index",     sendIndexPage);
  // httpServer.on("/index.html",sendIndexPage);
  httpServer.serveStatic("/FSexplorer.png",   LittleFS, "/FSexplorer.png");
  httpServer.serveStatic("/index.css", LittleFS, "/index.css");
  httpServer.serveStatic("/index.js",  LittleFS, "/index.js");
  // all other api calls are catched in FSexplorer onNotFounD!
  httpServer.on("/api", HTTP_ANY, processAPI);  //was only HTTP_GET (20210110)

  httpServer.begin();
  Serial.println("\nHTTP Server started\r");
  
  // Set up first message as the IP address
  sprintf(cMsg, "%03d.%03d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  Serial.printf("\nAssigned IP[%s]\r\n", cMsg);

  // Setup the OTGW PIC
  resetOTGW();          // reset the OTGW pic
  initWatchDog();       // setup the WatchDog
  startOTGWstream();    // start port 1023 
  sPICfwversion = getCommand("PR=A"); // fetch the firmware version
  DebugTf("OTGW PIC firmware version = [%s]\r\n", CSTR(sPICfwversion));

  DebugTf("Reboot count = [%d]\r\n", rebootCount);
  Serial.println(F("Setup finished!"));
}

//=====================================================================

//===[ blink status led ]===
void blinkLEDms(uint32_t iDelay){
  //blink the statusled, when time passed
  DECLARE_TIMER_MS(timerBlink, iDelay);
  if (DUE(timerBlink)) {
    blinkLEDnow();
  }
}

void blinkLEDnow(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

//===[ no-blocking delay with running background tasks in ms ]===
void delayms(unsigned long delay_ms)
{
  DECLARE_TIMER_MS(timerDelayms, delay_ms);
  while (DUE(timerDelayms))
    doBackgroundTasks();
}

//=====================================================================

//===[ Do task every 1s ]===
void doTaskEvery1s(){
  //== do tasks ==
  upTimeSeconds++;
}

//===[ Do task every 5s ]===
void doTaskEvery5s(){
  //== do tasks ==
}

//===[ Do task every 30s ]===
void doTaskEvery30s(){
  //== do tasks ==
}

//===[ Do task every 60s ]===
void doTaskEvery60s(){
  //== do tasks ==
  //if no wifi, try reconnecting (once a minute)
  if (WiFi.status() != WL_CONNECTED)
  {
    //disconnected, try to reconnect then...
     startWiFi(_HOSTNAME, 240);
    //check OTGW and telnet
    startTelnet();
    startOTGWstream(); 
  }
}

//===[ Do the background tasks ]===
void doBackgroundTasks()
{
  feedWatchDog();               // Feed the dog before it bites!
  handleMQTT();                 // MQTT transmissions
  handleOTGW();                 // OTGW handling
  httpServer.handleClient();
  MDNS.update();
  events();                     // trigger ezTime update etc.
  blinkLEDms(1000);             // 'blink' the status led every x ms
  delay(1);
}

void loop()
{

  DECLARE_TIMER_SEC(timer1s, 1, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer5s, 5, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer30s, 30, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer60s, 60, CATCH_UP_MISSED_TICKS);


  if (DUE(timer1s))       doTaskEvery1s();
  if (DUE(timer5s))       doTaskEvery5s();
  if (DUE(timer30s))      doTaskEvery30s();
  if (DUE(timer60s))      doTaskEvery60s();

  doBackgroundTasks();
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
