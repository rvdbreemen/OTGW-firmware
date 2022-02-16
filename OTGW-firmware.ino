/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v0.9.3
**
**  Copyright (c) 2021-2022 Robert van den Breemen
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

//FS SystemFS = FS(FSImplPtr(new littlefs_impl::LittleFSImpl(FS_PHYS_ADDR, FS_PHYS_SIZE, FS_PHYS_PAGE, FS_PHYS_BLOCK, sFS_MAX_OPEN_FILES)));;  //the global fs
FS SystemFS = FS(FSImplPtr(new littlefs_impl::LittleFSImpl(sFS_PHYS_ADDR, sFS_PHYS_SIZE, sFS_PHYS_PAGE, sFS_PHYS_BLOCK, sFS_MAX_OPEN_FILES)));  //the global fs
FS UserFS = FS(FSImplPtr(new littlefs_impl::LittleFSImpl(uFS_PHYS_ADDR, uFS_PHYS_SIZE, uFS_PHYS_PAGE, uFS_PHYS_BLOCK, uFS_MAX_OPEN_FILES)));  //the global fs

//=====================================================================
void setup() {
  // Serial is initialized by OTGWSerial. It resets the pic and opens serialdevice.
  // OTGWSerial.begin();//OTGW Serial device that knows about OTGW PIC
  // while (!Serial) {} //Wait for OK
  
  OTGWSerial.println(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  OTGWSerial.printf("Booting....[%s]\r\n\r\n", String(_FW_VERSION).c_str());
  WatchDogEnabled(0); // turn off watchdog

  //setup randomseed the right way
  randomSeed(RANDOM_REG32); //This is 8266 HWRNG used to seed the Random PRNG: Read more: https://config9.com/arduino/getting-a-truly-random-number-in-arduino/
 
  //setup the status LED
  setLed(LED1, ON);
  setLed(LED2, ON);

  //start with setting wifi hostname
  WiFi.hostname(String(settingHostname));

  // Connect to and initialise WiFi network
  OTGWSerial.println(F("Attempting to connect to WiFi network\r"));
  setLed(LED1, ON);
  startWiFi(CSTR(settingHostname), 240);  // timeout 240 seconds
  blinkLED(LED1, 3, 100);
  setLed(LED1, OFF);

  startTelnet();              // start the debug port 23

  bool fsready = setupFilesystems();
  if (fsready) {
     SystemFS.begin();
     readSettings(true);
  } else {
    blinkLED(LED1, 6, 500);
  }

  startNTP();
  startMDNS(CSTR(settingHostname));
  startLLMNR(CSTR(settingHostname));
  setupFSexplorer();
  startWebserver();
  startMQTT();               // start the MQTT after webserver, always.
 
  initWatchDog();            // setup the WatchDog
  lastReset = ESP.getResetReason();
  OTGWSerial.printf("Last reset reason: [%s]\r\n", CSTR(lastReset));
  rebootCount = updateRebootCount();
  updateRebootLog(lastReset);
  
 
  OTGWSerial.println(F("Setup finished!\r\n"));
  // After resetting the OTGW PIC never send anything to Serial for debug
  // and switch to telnet port 23 for debug purposed. 
  // Setup the OTGW PIC
  resetOTGW();          // reset the OTGW pic
  startOTGWstream();    // start port 25238 
  checkOTWGpicforupdate();
  initSensors();        // init DS18B20
  initOutputs();
  
  WatchDogEnabled(1);   // turn on watchdog
  sendOTGWbootcmd();   
  //Blink LED2 to signal setup done
  setLed(LED1, OFF);
  blinkLED(LED2, 3, 100);
  setLed(LED2, OFF);
  sendMQTTuptime();
  sendMQTTversioninfo();
}
//=====================================================================


void printFSinfo(Stream &out, FS &aFS) {

  FSInfo fs_info;
  aFS.info(fs_info);

  out.print("totalBytes: "); out.println(fs_info.totalBytes);
  out.print("usedBytes: "); out.println(fs_info.usedBytes);
  out.print("blockSize: "); out.println(fs_info.blockSize);
  out.print("pageSize: "); out.println(fs_info.pageSize);
  out.print("maxOpenFiles: "); out.println(fs_info.maxOpenFiles);
  out.print("maxPathLength: "); out.println(fs_info.maxPathLength);
}

void printFScontents(Stream &out, FS &aFS) {
  if (aFS.begin()) {
    out.println("Filesystem contents:");
    Dir dir = aFS.openDir("/");
    while (dir.next()) {
      out.println(dir.fileName().c_str());
    }
    SystemFS.end();
  } else {
    out.println("Error: cannot print filesystem contents");
  }
}

bool testFileSystem(FS &aFS, char *testFilename) {

  bool mounted = aFS.begin();
  if (!mounted) { return false; }

  printFSinfo(OTGWSerial, aFS);

  OTGWSerial.println("Succesfully mounted the filesystem, now testing file access");
  bool _success = aFS.exists(testFilename);
  if (!_success) {
    OTGWSerial.println("Test file does not exist on the filesystem");
    return false;
  }
  
  OTGWSerial.println("Test file is reported to exist by filesystem");
  File fh = aFS.open(testFilename, "r");

  if(!fh) {
    OTGWSerial.println("Cannot open test file");
    return false;
  }

  String _line = fh.readStringUntil('\n');
  if(_line.isEmpty()) {
    OTGWSerial.printf("%s file cannot be read. Unmounting\n", testFilename);
    _success = false;
  } else {
    OTGWSerial.printf("Check file %s contains: %s; and: \n %s\n", testFilename, _line.c_str(), fh.readStringUntil('\n').c_str());
  }
  
  fh.close();
  aFS.end();

  return _success;
}

//=====================================================================
bool setupFilesystems() {
  bool s_mounted, u_mounted = false;

  #define CHECK_FILE "/index.js"  // a file over 8k in size, containing clear text, to be used to test for successful mounting of the filesystem

  LittleFSConfig cfg;
  cfg.setAutoFormat(false); // prevent the fs from being formatted if it is actually the larger size
  SystemFS.setConfig(cfg);
  OTGWSerial.println("Attempting to mount the system partition");

  s_mounted = testFileSystem(SystemFS, CHECK_FILE);

 if(s_mounted) {
    OTGWSerial.println("Attempting to mount the user partition");
    u_mounted = UserFS.begin();
    if (UserFS.exists("/")) {  //since we are creating the user partition, there is no reason to test further than this
      OTGWSerial.println("User partition is accessable");
      bUserFSpresent = true;
      printFSinfo(OTGWSerial, UserFS);
      printFScontents(OTGWSerial, UserFS);
    } else {
      OTGWSerial.printf("Formatting the user partition: %s\n", UserFS.format() ? "success" : "failed");
    }
  }

  if (!s_mounted) {  // smaller fs not mounted succesfully, try the original size with the generated parameters FS_* (i.e. no u or s prefix)
    OTGWSerial.println("Failed to mount the 1M filesystem, fall back to the original 2M sized one");
    SystemFS = FS(FSImplPtr(new littlefs_impl::LittleFSImpl(FS_PHYS_ADDR, FS_PHYS_SIZE, FS_PHYS_PAGE, FS_PHYS_BLOCK, sFS_MAX_OPEN_FILES)));
    SystemFS.setConfig(cfg);
  
    s_mounted = testFileSystem(SystemFS, CHECK_FILE);

    if (!s_mounted) { 
        OTGWSerial.println("Failed to mount the original 2M filesystem, please reflash the firmware over usb");
        return false;
    }

    OTGWSerial.println("Succesfully mounted the original 2M filesystem");
    OTGWSerial.println("Redirecting user partition to the system partion");
    UserFS = SystemFS;
    bUserFSpresent = false;  //technically not necessary since default state is false;
    u_mounted = true;
  }

  return s_mounted && u_mounted;
}

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
    // WiFi.setAutoReconnect(true);   
    // WiFi.persistent(true);
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
  DebugTf("Uptime seconds: %d\r\n", upTimeSeconds);
  String sUptime = String(upTimeSeconds);
  sendMQTTData(F("otgw-firmware/uptime"), sUptime, false);
}

void sendtimecommand(){
  if (!settingNTPenable) return;        // if NTP is disabled, then return
  if (NtpStatus != TIME_SYNC) return;   // only send time command when time is synced
  //send time command to OTGW
  //send time / weekday
  char msg[15]={0};
  #define calc_ot_dow(dow) ((dow+5)%7+1) 
  sprintf(msg,"SC=%d:%02d/%ld", hour(), minute(), calc_ot_dow(dayOfWeek(now())));
  addOTWGcmdtoqueue(msg, strlen(msg), true);

  static int lastDay = 0;
  if (day(now())!=lastDay){
    //Send msg id 21: month, day
    lastDay = day(now());
    sprintf(msg,"SR=21:%d,%d", month(now()), day(now()));
    addOTWGcmdtoqueue(msg, strlen(msg), true);  
  }
  
  static int lastYear = 0;
  if (year(now())!=lastYear){
    lastYear = year(now());
    //Send msg id 22: HB of Year, LB of Year 
    sprintf(msg,"SR=22:%d,%d", (lastYear >> 8) & 0xFF, lastYear & 0xFF);
    addOTWGcmdtoqueue(msg, strlen(msg), true);
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
  
}

//===[ Do task every 30s ]===
void doTaskEvery30s(){
  //== do tasks ==
}

//===[ Do task every 60s ]===
void doTaskEvery60s(){
  //== do tasks ==
  //if no wifi, try reconnecting (once a minute)
  if (WiFi.status() != WL_CONNECTED) restartWifi();
  sendtimecommand();
}

//===[ Do task every 5min ]===
void do5minevent(){
  sendMQTTuptime();
  sendMQTTversioninfo();
  sendMQTTstateinformation();
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
  DECLARE_TIMER_MIN(tmrcheckpic, 1440, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer5min, 5, CATCH_UP_MISSED_TICKS);
  
  if (DUE(timerpollsensor)) pollSensors();    // poll the temperature sensors connected to 2wire gpio pin 
  if (DUE(timer5min))       do5minevent();
  if (DUE(timer60s))        doTaskEvery60s();
  if (DUE(timer30s))        doTaskEvery30s();
  if (DUE(timer5s))         doTaskEvery5s();
  if (DUE(timer1s))         doTaskEvery1s();
  if (DUE(tmrcheckpic))     docheckforpic();
  evalOutputs();                              // when the bits change, the output gpio bit will follow
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
