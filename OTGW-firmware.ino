/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v0.4.0
**
**  Copyright (c) 2020 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

/*
 *  How to install the OTGW on your nodeMCU
 *  
 *  Make sure you have all required library's installed:
 *  - ezTime - https://github.com/ropg/ezTime
 *  - TelnetStream - https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
 *  - ArduinoJson - https://arduinojson.org/
 *  All the library's can be installed using the library manager.
 *  
 *  How to upload to your SPIFF?
 *  Just install the SPIFF upload plugin (https://github.com/esp8266/arduino-esp8266fs-plugin) 
 *  and upload it to your SPIFF after first flashing the device.
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

#define _HOSTNAME   "OTGW"
#include "OTGW-firmware.h"

// WiFi Server object and parameters
WiFiServer server(80);

//=====================================================================
void setup()
{
  rebootCount = updateRebootCount();

  Serial.begin(9600, SERIAL_8N1);
  while (!Serial) {} //Wait for OK 

  //setup randomseed the right way
  randomSeed(RANDOM_REG32); //This is 8266 HWRNG used to seed the Random PRNG: Read more: https://config9.com/arduino/getting-a-truly-random-number-in-arduino/

  lastReset     = ESP.getResetReason();

  //setup the status LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); //OFF

  //start the debug port 23
  startTelnet();

  Serial.println(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  Serial.printf("Booting....[%s]\r\n\r\n", String(_FW_VERSION).c_str());
  
//================ SPIFFS ===========================================
  if (SPIFFS.begin()) 
  {
    Serial.println(F("SPIFFS Mount succesfull\r"));
    SPIFFSmounted = true;
  } else { 
    Serial.println(F("SPIFFS Mount failed\r"));   // Serious problem with SPIFFS 
    SPIFFSmounted = false;
  }

  readSettings(true);

  // Connect to and initialise WiFi network
  Serial.println(F("Attempting to connect to WiFi network\r"));
  digitalWrite(LED_BUILTIN, HIGH);
  startWiFi(_HOSTNAME, 240);  // timeout 240 seconds
  digitalWrite(LED_BUILTIN, LOW);
  
  startMDNS(settingHostname);
  
  delay(1000);
  
  // Start MQTT connection
  startMQTT(); 

  // Initialisation ezTime
  setDebug(INFO);  
  waitForSync(); 
  CET.setLocation(F("Europe/Amsterdam"));
  CET.setDefault(); 
  
  Serial.println("UTC time: "+ UTC.dateTime());
  Serial.println("CET time: "+ CET.dateTime());

  Serial.printf("Last reset reason: [%s]\r\n", ESP.getResetReason().c_str());
  Serial.print("Gebruik 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println("' voor verdere debugging");


//================ Start HTTP Server ================================
  setupFSexplorer();
  httpServer.serveStatic("/FSexplorer.png",   SPIFFS, "/FSexplorer.png");
  httpServer.on("/",          sendIndexPage);
  httpServer.on("/index",     sendIndexPage);
  httpServer.on("/index.html",sendIndexPage);
  httpServer.serveStatic("/index.css", SPIFFS, "/index.css");
  httpServer.serveStatic("/index.js",  SPIFFS, "/index.js");
  // all other api calls are catched in FSexplorer onNotFounD!
  httpServer.on("/api", HTTP_GET, processAPI);


  httpServer.begin();
  Serial.println("\nHTTP Server started\r");
  
  // Set up first message as the IP address
  sprintf(cMsg, "%03d.%03d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  Serial.printf("\nAssigned IP[%s]\r\n", cMsg);

  initWatchDog();       // setup the WatchDog
  startOTGWstream();    

  Serial.printf("Reboot count = [%d]\r\n", rebootCount);
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
