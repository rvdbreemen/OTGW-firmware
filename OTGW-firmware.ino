/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v0.10.0
**
**  Copyright (c) 2021-2023 Robert van den Breemen
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
#include "OTGW-firmware.h"
#include "OTGW-Display.h"

#define SetupDebugTln(...) ({ if (bPICavailable) DebugTln(__VA_ARGS__);    })
#define SetupDebugln(...)  ({ if (bPICavailable) Debugln(__VA_ARGS__);    })
#define SetupDebugTf(...)  ({ if (bPICavailable) DebugTf(__VA_ARGS__);    })
#define SetupDebugf(...)   ({ if (bPICavailable) Debugf(__VA_ARGS__);    })
#define SetupDebugT(...)   ({ if (bPICavailable) DebugT(__VA_ARGS__);    })
#define SetupDebug(...)    ({ if (bPICavailable) Debug(__VA_ARGS__);    })
#define SetupDebugFlush()  ({ if (bPICavailable) DebugFlush();    })

#define ON LOW
#define OFF HIGH

DECLARE_TIMER_SEC(timerpollsensor, settingGPIOSENSORSinterval, CATCH_UP_MISSED_TICKS);
DECLARE_TIMER_SEC(timers0counter, settingS0COUNTERinterval, CATCH_UP_MISSED_TICKS);

// Small status display interface.
OTGW_Display display;
  
//=====================================================================
void setup() {

 
  // Serial is initialized by OTGWSerial. It resets the pic and opens serialdevice.
  // OTGWSerial.begin();//OTGW Serial device that knows about OTGW PIC
  // while (!Serial) {} //Wait for OK
  WatchDogEnabled(0); // turn off watchdog

  SetupDebugln(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  SetupDebugf("Booting....[%s]\r\n\r\n", _VERSION);


  detectPIC();

  //setup randomseed the right way
  randomSeed(RANDOM_REG32); //This is 8266 HWRNG used to seed the Random PRNG: Read more: https://config9.com/arduino/getting-a-truly-random-number-in-arduino/
 
  //setup the status LED
  setLed(LED1, ON);
  setLed(LED2, ON);

  LittleFS.begin();
  readSettings(true);

  // Connect to and initialise WiFi network
  setLed(LED1, ON);
  SetupDebugln(F("Attempting to connect to WiFi network\r"));

  //setup NTP before connecting to wifi will enable DHCP to overrule the NTP setting
  startNTP();

  //start with setting wifi hostname
  WiFi.hostname(String(settingHostname));
  startWiFi(CSTR(settingHostname), 240);  // timeout 240 seconds
  blinkLED(LED1, 3, 100);
  setLed(LED1, OFF);

  startTelnet();              // start the debug port 23
  startMDNS(CSTR(settingHostname));
  startLLMNR(CSTR(settingHostname));
  setupFSexplorer();
  startWebserver();
  startMQTT();               // start the MQTT after webserver, always.
 
  initWatchDog();            // setup the WatchDog
  lastReset = ESP.getResetReason();
  SetupDebugf("Last reset reason: [%s]\r\n", CSTR(lastReset));
  rebootCount = updateRebootCount();
  updateRebootLog(lastReset);
  
  // Initialize the display.
  // We need to do this after the watchdog has initialized the TwoWire (IIC)
  // module.
  display.begin();
  SetupDebugln(F("Setup finished!\r\n"));

  // After resetting the OTGW PIC never send anything to Serial for debug
  // and switch to telnet port 23 for debug purposed. 
  // Setup the OTGW PIC
  resetOTGW();          // reset the OTGW pic
  startOTGWstream();    // start port 25238 
 // initSensors();        // init DS18B20 (after MQ is up! )
  initOutputs();
  
  WatchDogEnabled(1);   // turn on watchdog
  sendOTGWbootcmd();   
  //Blink LED2 to signal setup done
  setLed(LED1, OFF);
  blinkLED(LED2, 3, 100);
  setLed(LED2, OFF);
  sendMQTTuptime();
  sendMQTTversioninfo();
  initS0Count();        // init S0 counter
  initSensors();        // init DS18B20 (after MQ is up!)
}
//=====================================================================


//====[ restartWifi ]===
/*
  The restartWifi function takes tries to just reconnect to the wifi. When the wifi is restated, it then waits for maximum of 30 seconds (timeout).
  It keeps count of how many times it tried, when it tried to reconnect for 15 times. It goes into failsafe mode, and reboots the ESP.
  The watchdog is turned off during this process
*/
void restartWifi(){
  static int iTryRestarts = 0; //So if we have more than 15 restarts, then it's time to reboot
  iTryRestarts++;          //Count the number of attempts

  WiFi.hostname(settingHostname);  //make sure hostname is set
  if (WiFi.begin()) // if the wifi ssid exist, you can try to connect. 
  {
    //active wait for connections, this can take seconds
    DECLARE_TIMER_SEC(timeoutWifiConnect, 30, CATCH_UP_MISSED_TICKS);
    while ((WiFi.status() != WL_CONNECTED))
    {  
      delay(100);
      feedWatchDog();  //feeding the dog, while waiting activly
      if DUE(timeoutWifiConnect) break; //timeout, then break out of this loop
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  { //when reconnect, restart some services, just to make sure all works
    // Turn off ESP reconnect, to make sure that's not the issue (16/11/2021)
    startTelnet();
    startOTGWstream(); 
    startMQTT();
    iTryRestarts = 0; //reset attempt counter
    return;
  }

  //if all fails, and retry 15 is hit, then reboot esp
  if (iTryRestarts >= 15) doRestart("Too many wifi reconnect attempts");
}

void sendMQTTuptime(){
  DebugTf(PSTR("Uptime seconds: %d\r\n"), upTimeSeconds);
  String sUptime = String(upTimeSeconds);
  sendMQTTData(F("otgw-firmware/uptime"), sUptime, false);
}

void sendtimecommand(){
  if (!settingNTPenable) return;        // if NTP is disabled, then return
  if (NtpStatus != TIME_SYNC) return;   // only send time command when time is synced
  if (!bPICavailable) return;           // only send when pic is available
  if (OTGWSerial.firmwareType() != FIRMWARE_OTGW) return; //only send timecommand when in gateway firmware, not in diagnostic or interface mode

  //send time command to OTGW
  //send time / weekday
  time_t now = time(nullptr);
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  //DebugTf(PSTR("%02d:%02d:%02d %02d-%02d-%04d\r\n"), myTime.hour(), myTime.minute(), myTime.second(), myTime.day(), myTime.month(), myTime.year());

  char msg[15]={0};
  //Send msg id xx: hour:minute/day of week
  int day_of_week = (myTime.dayOfWeek()+6)%7+1;
  sprintf(msg,"SC=%d:%02d/%d", myTime.hour(), myTime.minute(), day_of_week);
  sendOTGW(msg, strlen(msg)); //bypass command queue, no delays
  
  if (dayChanged()){
    //Send msg id 21: month, day
    sprintf(msg,"SR=21:%d,%d", myTime.month(), myTime.day());
    addOTWGcmdtoqueue(msg, strlen(msg), true, 0); 
    handleOTGWqueue(); //send command right away
  }
  
  if (yearChanged()){
    //Send msg id 22: HB of Year, LB of Year 
    sprintf(msg,"SR=22:%d,%d", (myTime.year() >> 8) & 0xFF, myTime.year() & 0xFF);
    addOTWGcmdtoqueue(msg, strlen(msg), true, 0);
    handleOTGWqueue(); //send command right away
  }
}

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
  if (settingLEDblink) {
    digitalWrite(led, !digitalRead(led));
  } else setLed(led, OFF);

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
  //== Update Display ==
  display.tick();
}

//===[ Do task every 30s ]===
void doTaskEvery30s(){
  //== do tasks ==
}

//===[ Do task every 60s ]===
void doTaskEvery60s(){
  //== do tasks ==
  if (sPICdeviceid=="unknown"){
    //keep trying to figure out which pic is used!
    DebugTln("PIC is unknown, probe pic using PR=A");
    //Force banner fetch
    getpicfwversion();
    //This should retreive the information here
    sPICfwversion = String(OTGWSerial.firmwareVersion());
    DebugTf(PSTR("Current firmware version: %s\r\n"), CSTR(sPICfwversion));
    sPICdeviceid = OTGWSerial.processorToString();
    DebugTf(PSTR("Current device id: %s\r\n"), CSTR(sPICdeviceid));    
    sPICtype = OTGWSerial.firmwareToString();
    DebugTf(PSTR("Current firmware type: %s\r\n"), CSTR(sPICtype));
  }
}

//===[ Do task exactly on the minute ]===
void doTaskMinuteChanged(){
  //== do tasks ==
  //if no wifi, try reconnecting (once a minute)
  if (WiFi.status() != WL_CONNECTED) restartWifi();
  DebugTln("Minute changed:");
  sendtimecommand();
}

//===[ Do task every 5min ]===
void do5minevent(){
  sendMQTTuptime();
  sendMQTTversioninfo();
  sendMQTTstateinformation();
  if (bCheckOTGWPICupdate) {
    bCheckOTGWPICupdate = false;
    checkOTWGpicforupdate();
  }
}

//===[ Do task every 24 hours ]===
void doTaskEvery24h(){
  bCheckOTGWPICupdate = true;
}

//===[ Do the background tasks ]===
void doBackgroundTasks()
{
  feedWatchDog();               // Feed the dog before it bites!
  if (WiFi.status() == WL_CONNECTED) {
    //while connected handle everything that uses network stuff
    handleDebug();
    handleMQTT();                 // MQTT transmissions
    handleOTGW();                 // OTGW handling
    httpServer.handleClient();
    MDNS.update();
    loopNTP();
  } //otherwise, just wait until reconnected gracefully
  delay(1);
}

void loop()
{
  DECLARE_TIMER_SEC(timer1s, 1, SKIP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer5s, 5, SKIP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer30s, 30, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer60s, 60, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer5min, 5, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer24h, 1440, CATCH_UP_MISSED_TICKS);
  
  if (DUE(timerpollsensor))         pollSensors();    // poll the temperature sensors connected to 2wire gpio pin 
  if (DUE(timers0counter))          sendS0Counters(); // poll the s0 counter connected to gpio pin when due
  if (DUE(timer5min))               do5minevent();
  if (DUE(timer60s))                doTaskEvery60s();
  if (DUE(timer30s))                doTaskEvery30s();
  if (DUE(timer5s))                 doTaskEvery5s();
  if (DUE(timer1s))                 doTaskEvery1s();
  if (DUE(timer24h))                doTaskEvery24h();
  if (minuteChanged())              doTaskMinuteChanged(); //exactly on the minute
  evalOutputs();                                // when the bits change, the output gpio bit will follow
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
