/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v0.8.1
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

#define ON LOW
#define OFF HIGH

DECLARE_TIMER_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS);

// TODO need to determine interval
DECLARE_TIMER_SEC(timersetoutput, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS);
  
//=====================================================================
void setup() {
  // Serial is initialized by OTGWSerial. It resets the pic and opens serialdevice.
  // OTGWSerial.begin();//OTGW Serial device that knows about OTGW PIC
  // while (!Serial) {} //Wait for OK
  
  OTGWSerial.println(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  OTGWSerial.printf("Booting....[%s]\r\n\r\n", String(_FW_VERSION).c_str());
  rebootCount = updateRebootCount();

  //setup randomseed the right way
  randomSeed(RANDOM_REG32); //This is 8266 HWRNG used to seed the Random PRNG: Read more: https://config9.com/arduino/getting-a-truly-random-number-in-arduino/
  lastReset = ESP.getResetReason();
  OTGWSerial.printf("Last reset reason: [%s]\r\n", CSTR(ESP.getResetReason()));

  //setup the status LED
  setLed(LED1, ON);
  setLed(LED2, ON);

  LittleFS.begin();
  readSettings(true);

  // Connect to and initialise WiFi network
  OTGWSerial.println(F("Attempting to connect to WiFi network\r"));
  setLed(LED1, ON);
  startWiFi(CSTR(settingHostname), 240);  // timeout 240 seconds
  blinkLED(LED1, 3, 100);
  setLed(LED1, OFF);

  startTelnet();              // start the debug port 23
  startMDNS(CSTR(settingHostname));
  startLLMNR(CSTR(settingHostname));
  startMQTT(); 
  startNTP();
  setupFSexplorer();
  startWebserver();
  OTGWSerial.println(F("Setup finished!\r\n"));
  // After resetting the OTGW PIC never send anything to Serial for debug
  // and switch to telnet port 23 for debug purposed. 
  // Setup the OTGW PIC
  resetOTGW();          // reset the OTGW pic
  startOTGWstream();    // start port 25238 
  checkOTWGpicforupdate();
  initSensors();        // init DS18B20
  initOutputs();

  initWatchDog();       // setup the WatchDog
  sendOTGWbootcmd();   
  //Blink LED2 to signal setup done
  setLed(LED1, OFF);
  blinkLED(LED2, 3, 100);
  setLed(LED2, OFF);
}

//=====================================================================

//===[ blink status led ]===

void setLed(uint8_t led, uint8_t status){
  pinMode(led, OUTPUT);
  digitalWrite(led, status); 
}

void blinkLEDms(uint32_t delay){
  //blink the statusled, when time passed
  DECLARE_TIMER_MS(timerBlink, delay);
  if (DUE(timerBlink)) {
    blinkLEDnow();
  }
}

void blinkLED(uint8_t led, int nr, uint32_t waittime_ms){
    for (int i = nr; i>0; i--){
      blinkLEDnow(led);
      delayms(waittime_ms);
      blinkLEDnow(led);
      delayms(waittime_ms);
    }
}

void blinkLEDnow(uint8_t led = LED1){
  pinMode(led, OUTPUT);
  digitalWrite(led, !digitalRead(led));
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
  handleOTGWqueue(); //just check if there are commands to retry
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
    startWiFi(CSTR(settingHostname), 240);
    //check OTGW and telnet
    startTelnet();
    startOTGWstream(); 
  }
}

//===[ Do task every 5min ]===
void do5minevent(){
  DebugTf("Uptime seconds: %d\r\n", upTimeSeconds);
  String sUptime = String(upTimeSeconds);
  sendMQTTData("otgw-firmware/uptime", sUptime, false);
}

//===[ check for new pic version  ]===
void docheckforpic(){
  String latest = checkforupdatepic("gateway.hex");
  if (!bOTGWonline) {
    sMessage = sPICfwversion; 
  } else if (latest != sPICfwversion) {
    sMessage = "New PIC version " + latest + " available!";
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
  events();                     // trigger ezTime update etc       
  delay(1);
  handleDebug();
}

void loop()
{
  DECLARE_TIMER_SEC(timer1s, 1, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer5s, 5, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer30s, 30, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer60s, 60, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(tmrcheckpic, 1440, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer5min, 5, CATCH_UP_MISSED_TICKS);
  
  if (DUE(timer1s))         doTaskEvery1s();
  if (DUE(timer5s))         doTaskEvery5s();
  if (DUE(timer30s))        doTaskEvery30s();
  if (DUE(timer60s))        doTaskEvery60s();
  if (DUE(tmrcheckpic))     docheckforpic();
  if (DUE(timer5min))       do5minevent();
  if (DUE(timerpollsensor)) pollSensors();
  if (DUE(timersetoutput))  evalOutputs();
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
