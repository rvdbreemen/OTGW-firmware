/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
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

#define SetupDebugTln(...) ({  DebugTln(__VA_ARGS__);    })
#define SetupDebugln(...)  ({  Debugln(__VA_ARGS__);    })
#define SetupDebugTf(...)  ({  DebugTf(__VA_ARGS__);    })
#define SetupDebugf(...)   ({  Debugf(__VA_ARGS__);    })
#define SetupDebugT(...)   ({  DebugT(__VA_ARGS__);    })
#define SetupDebug(...)    ({  Debug(__VA_ARGS__);    })
#define SetupDebugFlush()  ({  DebugFlush();    })

#define ON LOW
#define OFF HIGH

DECLARE_TIMER_SEC(timerpollsensor, settings.sensors.iInterval, CATCH_UP_MISSED_TICKS);
DECLARE_TIMER_SEC(timers0counter, settings.s0.iInterval, CATCH_UP_MISSED_TICKS);

#define WIFI_PORTAL_RESET_MAGIC           0x4F544750UL  // "OTGP"
#define WIFI_PORTAL_RESET_RTC_SLOT        96            // RTC user memory slot (4-byte units)
#define WIFI_PORTAL_RESET_TRIGGER_COUNT   3
#define WIFI_PORTAL_RESET_WINDOW_MS       10000UL

struct WifiPortalResetState {
  uint32_t magic;
  uint32_t resetCount;
};

uint32_t wifiPortalResetWindowDeadline = 0;
bool wifiPortalResetWindowOpen = false;

bool readWifiPortalResetState(WifiPortalResetState &portalState) {
  return platformRtcRead(WIFI_PORTAL_RESET_RTC_SLOT, reinterpret_cast<uint32_t*>(&portalState), sizeof(portalState));
}

bool writeWifiPortalResetState(const WifiPortalResetState &portalState) {
  return platformRtcWrite(WIFI_PORTAL_RESET_RTC_SLOT, reinterpret_cast<const uint32_t*>(&portalState), sizeof(portalState));
}

void clearWifiPortalResetState() {
  WifiPortalResetState portalState = { WIFI_PORTAL_RESET_MAGIC, 0 };
  writeWifiPortalResetState(portalState);
}

bool isExternalSystemReset() {
  return platformIsExternalReset();
}

bool shouldForceWifiConfigPortal() {
  // Use 'portalState' to avoid shadowing the global 'OTGWState state' (review K1)
  WifiPortalResetState portalState = { WIFI_PORTAL_RESET_MAGIC, 0 };
  WifiPortalResetState storedState = { 0, 0 };
  if (readWifiPortalResetState(storedState) && (storedState.magic == WIFI_PORTAL_RESET_MAGIC)) {
    portalState = storedState;
  }

  bool externalReset = isExternalSystemReset();
  if (externalReset) {
    if (portalState.resetCount < 255) {
      portalState.resetCount++;
    }
  } else {
    portalState.resetCount = 0;
  }

  bool forcePortal = (portalState.resetCount >= WIFI_PORTAL_RESET_TRIGGER_COUNT);
  if (forcePortal) {
    SetupDebugTf(PSTR("Detected %u external resets -> force WiFi config portal\r\n"), (unsigned int)portalState.resetCount);
    portalState.resetCount = 0;
    wifiPortalResetWindowOpen = false;
    wifiPortalResetWindowDeadline = 0;
  } else if (portalState.resetCount > 0) {
    wifiPortalResetWindowOpen = true;
    wifiPortalResetWindowDeadline = millis() + WIFI_PORTAL_RESET_WINDOW_MS;
    SetupDebugTf(PSTR("External reset count: %u/%u (window %lu ms)\r\n"),
      (unsigned int)portalState.resetCount,
      (unsigned int)WIFI_PORTAL_RESET_TRIGGER_COUNT,
      (unsigned long)WIFI_PORTAL_RESET_WINDOW_MS);
  } else {
    wifiPortalResetWindowOpen = false;
    wifiPortalResetWindowDeadline = 0;
  }

  writeWifiPortalResetState(portalState);
  return forcePortal;
}

bool wifiPortalResetWindowExpired() {
  return wifiPortalResetWindowOpen && ((int32_t)(millis() - wifiPortalResetWindowDeadline) >= 0);
}
  
//=====================================================================
void setup() {

 
  // Serial is initialized by OTGWSerial. It resets the pic and opens serialdevice.
  // OTGWSerial.begin();//OTGW Serial device that knows about OTGW PIC
  // while (!Serial) {} //Wait for OK
  WatchDogEnabled(0); // turn off watchdog
  
  SetupDebugln(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  SetupDebugf(PSTR("Booting....[%s]\r\n\r\n"), _VERSION);
  
  detectPIC();

  //setup randomseed the right way
  randomSeed(platformHardwareRandom()); // Hardware RNG to seed the Random PRNG
 
  //setup the status LED
  setLed(LED1, ON);
  setLed(LED2, ON);

  LittleFSmounted = LittleFS.begin();
  if (!LittleFSmounted) SetupDebugln(F("*** ERROR: LittleFS mount FAILED - running on compile-time defaults ***"));
  readSettings(true);
  checklittlefshash();

  // Set hostname ASAP after loading settings.  WiFi.persistent(true) from a
  // previous boot lets the SDK auto-connect before startWiFi() is reached;
  // without this early call the DHCP request carries the default "ESP-XXXXXX".
  platformSetHostname(CSTR(settings.sHostname));

  // Connect to and initialise WiFi network
  setLed(LED1, ON);
  SetupDebugln(F("Attempting to connect to WiFi network\r"));

  bool forceWifiPortal = shouldForceWifiConfigPortal();
  startWiFi(CSTR(settings.sHostname), 240, forceWifiPortal);  // timeout 240 seconds

  //setup NTP after WiFi; startNTP() restores hostname after configTime()
  startNTP();
  blinkLED(LED1, 3, 100);
  setLed(LED1, OFF);

  startTelnet();              // start the debug port 23
  startMDNS(CSTR(settings.sHostname));
  startLLMNR(CSTR(settings.sHostname));
  setupFSexplorer();
  startWebserver();
  startWebSocket();          // start the WebSocket server for OT log streaming
  startMQTT();               // start the MQTT after webserver, always.
 
  { char wdReason[64]; initWatchDog(wdReason, sizeof(wdReason)); }  // setup the WatchDog
  platformResetReason(lastReset, sizeof(lastReset));
  SetupDebugf(PSTR("Last reset reason: [%s]\r\n"), CSTR(lastReset));
  state.uptime.iRebootCount = updateRebootCount();
  updateRebootLog(lastReset);
  
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
  if (!LittleFSmounted) sendMQTTData(F("otgw-firmware/error"), "LittleFS mount failed - running on defaults", false);
  initS0Count();        // init S0 counter
  initSensors();        // init DS18B20 (after MQ is up!)
  // Clear the triple-reset portal counter: a successful setup() proves the device is healthy.
  // This prevents USB flash resets or stale RTC data from triggering the portal on next boot.
  clearWifiPortalResetState();
  triggerPICsettingsReadout();  // Start initial PIC settings discovery cycle
  state.bSetupComplete = true; // ADR-036: allow doBackgroundTasks() to run service handlers
}
//=====================================================================


//====[ Non-blocking WiFi Reconnect State Machine (ADR-047) ]===
// Replaces blocking restartWifi() — no delay() in reconnect path.
// Called every loop iteration from doBackgroundTasks().

enum WifiState_t {
  WIFI_IDLE,         // connected, monitoring
  WIFI_DISCONNECTED, // just dropped — start reconnect
  WIFI_CONNECTING,   // waiting non-blocking for connection
  WIFI_RECONNECTED,  // just came back — restart services
  WIFI_FAILED        // too many retries — trigger reboot
};
static WifiState_t wifiState = WIFI_IDLE;
static int wifiRetryCount = 0;

void loopWifi() {
  DECLARE_TIMER_SEC(timerWifiRetry, 5, CATCH_UP_MISSED_TICKS);

  switch (wifiState) {
    case WIFI_IDLE:
      if (WiFi.status() != WL_CONNECTED) {
        DebugTln(F("WiFi: connection lost, starting non-blocking reconnect"));
        wifiRetryCount = 0;
        wifiState = WIFI_DISCONNECTED;
      }
      break;

    case WIFI_DISCONNECTED:
      DebugTf(PSTR("WiFi: reconnect attempt %d starting for hostname [%s]\r\n"),
              wifiRetryCount + 1,
              CSTR(settings.sHostname));
      platformSetHostname(CSTR(settings.sHostname));
      WiFi.begin();  // uses stored credentials
      RESTART_TIMER(timerWifiRetry);
      wifiState = WIFI_CONNECTING;
      break;

    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WIFI_RECONNECTED;
        wifiRetryCount = 0;
      } else if (DUE(timerWifiRetry)) {
        wifiRetryCount++;
        DebugTf(PSTR("WiFi: connect attempt %d failed\r\n"), wifiRetryCount);
        if (wifiRetryCount >= 15) {
          wifiState = WIFI_FAILED;
        } else {
          wifiState = WIFI_DISCONNECTED;  // retry
        }
      }
      break;

    case WIFI_RECONNECTED:
      // Match the startup path: re-apply the configured hostname and force a
      // DHCP re-announce so the renewed lease uses the expected name.
      platformSetHostname(CSTR(settings.sHostname));
      DebugTf(PSTR("WiFi: reconnected, re-announcing DHCP lease for hostname [%s]\r\n"),
              CSTR(settings.sHostname));
      platformRestartDHCP();
      startTelnet();
      startOTGWstream();
      startMQTT();
      startWebSocket();
      wifiState = WIFI_IDLE;
      DebugTf(PSTR("WiFi: reconnected, services restarted; IP=%s\r\n"),
              WiFi.localIP().toString().c_str());
      break;

    case WIFI_FAILED:
      doRestart("WiFi: too many reconnect failures");
      break;
  }
}

void sendMQTTuptime(){
  DebugTf(PSTR("Uptime seconds: %lu\r\n"), (unsigned long)state.uptime.iSeconds);
  char uptimeBuf[11] = {0};
  snprintf_P(uptimeBuf, sizeof(uptimeBuf), PSTR("%lu"), (unsigned long)state.uptime.iSeconds);
  sendMQTTData(F("otgw-firmware/uptime"), uptimeBuf, false);
}

void sendtimecommand(){
  if (state.otgw.bPSmode) return;                  // when in Print Summary mode (PS=1), no timesync commands (improving legacy/Domoticz compatibility)
  if (!settings.ntp.bEnable) return;        // if NTP is disabled, then return
  if (!settings.ntp.bSendtime) return;      // if NTP send time is disabled, then return
  if (NtpStatus != TIME_SYNC) return;   // only send time command when time is synced
  if (!state.pic.bAvailable) return;           // only send when pic is available
  if (OTGWSerial.firmwareType() != FIRMWARE_OTGW) return; //only send timecommand when in gateway firmware, not in diagnostic or interface mode

  //send time command to OTGW
  //send time / weekday
  time_t now = time(nullptr);
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
  //DebugTf(PSTR("%02d:%02d:%02d %02d-%02d-%04d\r\n"), myTime.hour(), myTime.minute(), myTime.second(), myTime.day(), myTime.month(), myTime.year());

  char msg[15]={0};
  //Send msg id xx: hour:minute/day of week
  int day_of_week = (myTime.dayOfWeek()+6)%7+1;
  snprintf_P(msg, sizeof(msg), PSTR("SC=%d:%02d/%d"), myTime.hour(), myTime.minute(), day_of_week);
  addOTWGcmdtoqueue(msg, strlen(msg), false, 0);

  if (dayChanged()){
    //Send msg id 21: month, day
    snprintf_P(msg, sizeof(msg), PSTR("SR=21:%d,%d"), myTime.month(), myTime.day());
    addOTWGcmdtoqueue(msg, strlen(msg), true, 0);
  }

  if (yearChanged()){
    //Send msg id 22: HB of Year, LB of Year
    snprintf_P(msg, sizeof(msg), PSTR("SR=22:%d,%d"), (myTime.year() >> 8) & 0xFF, myTime.year() & 0xFF);
    addOTWGcmdtoqueue(msg, strlen(msg), true, 0);
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
  if (settings.bLEDblink) {
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
  state.uptime.iSeconds++;

  if (wifiPortalResetWindowExpired()) {
    SetupDebugTln(F("Reset trigger window expired, clearing pending reset count"));
    clearWifiPortalResetState();
    wifiPortalResetWindowOpen = false;
    wifiPortalResetWindowDeadline = 0;
  }
}

//===[ Do task every 3s ]===
void doTaskEvery3s(){
  //== do tasks ==
  if (!picSettingsCycleActive) return;
  queryNextPICsetting();
}


//===[ Do task every 60s ]===
void doTaskEvery60s(){

  //== do tasks ==

  // Re-check FS/firmware hash match every 60s so the warning persists
  // even if other runtime status messages are set and cleared elsewhere.
  checklittlefshash();

  // Query gateway mode from PIC — non-blocking, queues PR=M.
  // State update + MQTT publish handled by handlePRresponse() when response arrives.
  // Only gateway firmware supports PR=M; no dependency on OT bus traffic.
  if (isPICEnabled() && isGatewayFirmware()) {
    queryOTGWgatewaymode();
  }

  // Probe PIC firmware version if still unknown.
  // Runs regardless of isPICEnabled() so a transient boot-probe miss can recover:
  // detectPIC() relies on a single ETX check; if that fails, this 60s retry is the
  // only automatic path to re-detect a real PIC and re-enable all PIC functions.
  // Writes directly to serial (bypassing the guarded command queue).
  // Banner response in processOT() sets state.pic.bAvailable = true on success.
  if ((strcmp_P(state.pic.sDeviceid, PSTR("unknown")) == 0)
      || (strcmp_P(state.pic.sDeviceid, PSTR("no pic found")) == 0)
      || (state.pic.sDeviceid[0] == '\0')) {
    DebugTln(F("PIC is unknown, probe pic using PR=A"));
    OTGWSerial.write("PR=A\r\n");
    OTGWSerial.flush();
  }
  
  // Log heap statistics every minute for monitoring
  logHeapStats();
}

//===[ Do task exactly on the minute ]===
void doTaskMinuteChanged(){
  //== do tasks ==
  // WiFi reconnect is now handled by loopWifi() state machine in doBackgroundTasks()
  sendtimecommand();
}

//===[ Do task every 5min ]===
void do5minevent(){
  sendMQTTuptime();
  sendMQTTversioninfo();
  sendMQTTstateinformation();
  publishAllPICsettings();  // Re-publish cached PIC settings every 5 min
}

static void handleEspFlashBackgroundTasks()
{
  handleDebug();              // Keep telnet debug active for monitoring
  httpServer.handleClient();  // MUST continue - processes upload chunks
#if MDNS_NEEDS_UPDATE
  MDNS.update();
#endif              // Keep MDNS active for network discovery
  handleWebSocket();          // Keep WebSocket service responsive during flash
}

static void handlePicFlashBackgroundTasks()
{
  handleDebug();              // Keep telnet debug active for monitoring
  httpServer.handleClient();  // Keep HTTP active
#if MDNS_NEEDS_UPDATE
  MDNS.update();
#endif              // Keep MDNS active for network discovery
  handleOTGW();               // REQUIRED for PIC flash - processes serial communication
  handleWebSocket();          // Keep WebSocket service responsive during flash
}

//===[ Do the background tasks ]===
void doBackgroundTasks()
{
  feedWatchDog();               // Feed the dog before it bites!

  // ADR-036: block service handlers until setup() completes.
  // blinkLED/delayms in setup() would otherwise invoke handleMQTT() before
  // startMQTT() sets the 1350-byte buffer, and handleOTGW() before resetOTGW().
  if (!state.bSetupComplete) return;
  // ADR-047: Non-blocking WiFi reconnect state machine.
  // Guard: skip during any flash operation (ESP or PIC).
  // During Update.write() the ESP8266 suspends flash reads, starving the WiFi
  // stack. After the write, WiFi.status() may transiently return WL_DISCONNECTED,
  // causing loopWifi() to initiate a reconnect mid-upload. If the reconnect
  // completes while the upload is still in progress, WIFI_RECONNECTED calls
  // startWebSocket()/startMQTT(), potentially tearing down the HTTP connection
  // carrying the OTA data and leaving the LittleFS partition partially written.
  if (!isFlashing()) loopWifi();

  // Check for critically low heap and attempt recovery if needed
  if (getHeapHealth() == HEAP_CRITICAL) {
    emergencyHeapRecovery();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (state.flash.bESPactive) {
      handleEspFlashBackgroundTasks();
    } else if (state.flash.bPICactive) {
      handlePicFlashBackgroundTasks();
    } else {
      //while connected handle everything that uses network stuff
      handleDebug();
      handleMQTT();                 // MQTT transmissions
      handleOTGW();                 // OTGW handling
      handleWebSocket();            // WebSocket handling for OT log streaming
      httpServer.handleClient();
    #if MDNS_NEEDS_UPDATE
  MDNS.update();
#endif
      loopNTP();
    }
  } //otherwise, just wait until reconnected gracefully
  delay(1);
  return;
}

void loop()
{
  DECLARE_TIMER_SEC(timer1s, 1, SKIP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer3s, 3, SKIP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer60s, 60, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer5min, 5, CATCH_UP_MISSED_TICKS);

  if (!isFlashing()) {
    // Only run these tasks when NOT flashing firmware (ESP or PIC)
      if (DUE(timerFlushSettings))      flushSettings();  // coalesced settings write + service restarts
      if (DUE(timerpollsensor))         pollSensors();    // poll the temperature sensors connected to 2wire gpio pin
      if (DUE(timers0counter))          sendS0Counters(); // poll the s0 counter connected to gpio pin when due
      if (DUE(timer5min))               do5minevent();
      if (DUE(timer60s))                doTaskEvery60s();
      if (DUE(timer3s))                 doTaskEvery3s();
      if (DUE(timer1s))                 doTaskEvery1s();
      if (minuteChanged())              doTaskMinuteChanged(); //exactly on the minute
      evalOutputs();                    // when the bits change, the output gpio bit will follow
      evalWebhook();                    // when the trigger bit changes, fire the webhook
      handlePendingUpgrade();           // Check if we need to start an upgrade
    } 

  doBackgroundTasks();              // run background tasks
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
