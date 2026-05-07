/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v1.5.0-beta.28
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
  return ESP.rtcUserMemoryRead(WIFI_PORTAL_RESET_RTC_SLOT, reinterpret_cast<uint32_t*>(&portalState), sizeof(portalState));
}

bool writeWifiPortalResetState(const WifiPortalResetState &portalState) {
  return ESP.rtcUserMemoryWrite(WIFI_PORTAL_RESET_RTC_SLOT, const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(&portalState)), sizeof(portalState));
}

void clearWifiPortalResetState() {
  WifiPortalResetState portalState = { WIFI_PORTAL_RESET_MAGIC, 0 };
  writeWifiPortalResetState(portalState);
}

bool isExternalSystemReset() {
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return (resetInfo != nullptr) && (resetInfo->reason == REASON_EXT_SYS_RST);
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

// ---------------------------------------------------------------------------
// TASK-397: always-on BGTRACE instrumentation to diagnose random
// doBackgroundTasks() stalls introduced somewhere between v1.3.5 and dev.
// When BGTASKS_TRACE is 1, every handler in the chain emits a one-line
// telnet log with name, duration (microseconds), free heap, and max free
// block. Volume is HIGH (hundreds of lines/sec at idle); disable by setting
// BGTASKS_TRACE to 0 after the culprit has been identified.
//
// Stall-detection pattern: the LAST BGTRACE line in the log identifies the
// previous handler that returned normally. The handler whose name appears
// NEXT in the code but has NO corresponding BGTRACE line is the one hung.
// ---------------------------------------------------------------------------
#define BGTASKS_TRACE 0

#if BGTASKS_TRACE
  #define BGTRACE(name) do { \
      uint32_t _now = micros(); \
      DebugTf(PSTR("[bg] %s %luus heap=%u max=%u\r\n"), \
              name, (unsigned long)(_now - _bgPrev), \
              (unsigned)ESP.getFreeHeap(), \
              (unsigned)ESP.getMaxFreeBlockSize()); \
      _bgPrev = _now; \
    } while(0)
#else
  #define BGTRACE(name) ((void)0)
#endif

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
  randomSeed(RANDOM_REG32); //This is 8266 HWRNG used to seed the Random PRNG: Read more: https://config9.com/arduino/getting-a-truly-random-number-in-arduino/
 
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
  WiFi.hostname(CSTR(settings.sHostname));

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
  strlcpy(lastReset, ESP.getResetReason().c_str(), sizeof(lastReset));
  SetupDebugf(PSTR("Last reset reason: [%s]\r\n"), CSTR(lastReset));
  state.uptime.iRebootCount = updateRebootCount();
  updateRebootLog(lastReset);

  // One-line boot signature for field diagnostics (TASK-395, port from 2.0.0
  // TASK-394 Phase 2). Captured AFTER full init so heap/fragmentation reflect
  // steady-state setup.
  logBootSignature("boot:");

  // TASK-396: warn once if flash hardware doesn't match the 4M2M DIO build.
  // Silent on matching boards; emits one or more [flash] WARN lines otherwise.
  maybeWarnFlashMismatch();

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

  // Hourly/daily dispatch moved to doTaskMinuteChanged (ADR-064 / TASK-350):
  // wall-clock aligned instead of boot-relative 60s drift. See that function
  // for the single call site of hourChanged/dayChanged/yearChanged.
}

// Extracted from the old hourly block so the new dispatcher reads cleanly
// (ADR-064). Preserves existing guards: bNightlyRestart + ntp.bEnable +
// uptime>3600 + NTP-synced sanity.
static void runNightlyRestartCheck() {
  if (!settings.bNightlyRestart) return;
  if (!settings.ntp.bEnable) return;
  if (state.uptime.iSeconds <= 3600) return;
  const int64_t now_sec = time(nullptr);
  if (now_sec <= 946684800) return;     // sanity: after 2000-01-01 (NTP synced)
  TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now_sec, myTz);
  if (myTime.hour() != settings.iRestartHour) return;
  DebugTf(PSTR("Nightly restart triggered at %02d:00 (uptime=%lu s)\r\n"),
          settings.iRestartHour, (unsigned long)state.uptime.iSeconds);
  doRestart("[nightly] scheduled restart");
}

//===[ Do task exactly on the minute ]===
// Single dispatcher for all sub-minute consume-on-read time-boundary helpers
// per ADR-064. The four helpers (minuteChanged + hourChanged/dayChanged/
// yearChanged) each have EXACTLY ONE call site in the firmware:
//   - minuteChanged() : main loop gate (OTGW-firmware.ino:366)
//   - hourChanged()   : this function
//   - dayChanged()    : this function
//   - yearChanged()   : this function
// Downstream consumers read the local flag, never re-call the helper.
// evaluate.py::check_time_boundary_single_caller enforces this rule.
void doTaskMinuteChanged(){
  // ADR-064: these three helpers are called here and only here. Downstream
  // consumers read the local flags below, never re-call the helper.
  // Enforced by evaluate.py::check_time_boundary_single_caller.
  const bool hourFlag = hourChanged();
  const bool dayFlag  = dayChanged();
  const bool yearFlag = yearChanged();

  // Per-minute work (unconditional).
  // WiFi reconnect is handled by loopWifi() state machine in doBackgroundTasks().
  sendtimecommand(dayFlag, yearFlag);

  // Hourly consumers. New hourly tasks extend THIS block, never add a second
  // hourChanged() call elsewhere.
  if (hourFlag) {
    runNightlyRestartCheck();     // TASK-345: moved from doTaskEvery60s
    sendMQTTheapdiag();            // TASK-346: moved from doTaskEvery60s
  }

  // Daily consumers (TASK-351).
  if (dayFlag) {
    // Daily MQTT discovery verification. Opt-in via settings.mqtt.bDiscoveryAutoVerify
    // (default true). Preconditions (NTP sync, uptime>3600, heap>=6000, no pending
    // drip, MQTT connected) are enforced inside startDiscoveryVerification(), so
    // this call is unconditional here and startup-safe.
    if (settings.mqtt.bDiscoveryAutoVerify) startDiscoveryVerification();
  }

  // Yearly consumers: SR=22 via sendtimecommand(dayFlag, yearFlag) above is the
  // only current consumer. New yearly work extends THIS block (and keeps the
  // single-caller rule for yearChanged()).
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
  debugTelnet.loop();         // Process new connections and disconnections
  OTGWstream.loop();          // Keep OTGWstream clients alive during flash
  handleDebug();              // Keep telnet debug active for monitoring
  httpServer.handleClient();  // MUST continue - processes upload chunks
  MDNS.update();              // Keep MDNS active for network discovery
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
      uint32_t _bgPrev = micros();
      debugTelnet.loop();          BGTRACE("debugTelnet");
      OTGWstream.loop();           BGTRACE("OTGWstream");
      handleDebug();               BGTRACE("handleDebug");
      handleMQTT();                BGTRACE("handleMQTT");
      handleOTGW();                BGTRACE("handleOTGW");
      handleWebSocket();           BGTRACE("handleWebSocket");
      httpServer.handleClient();   BGTRACE("httpServer");
      MDNS.update();               BGTRACE("mdns");
      loopNTP();                   BGTRACE("ntp");
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
      if (minuteChanged())              doTaskMinuteChanged(); //ADR-064: sole minuteChanged() caller; hour/day/year dispatch lives inside
      {
        uint32_t _bgPrev = micros();
        loopMQTTDiscovery();       BGTRACE("loopMQTTDiscovery");
        evalOutputs();             BGTRACE("evalOutputs");
        evalWebhook();             BGTRACE("evalWebhook");
        handlePendingUpgrade();    BGTRACE("handlePendingUpgrade");
      }
    }

  doBackgroundTasks();              // run background tasks

  // TASK-396: heap watermark tick + deferred-reboot gate. The watermark runs
  // every loop so slow leaks are visible in the minHeap field of the boot
  // signature on the next reboot. The deferred-reboot check re-uses the
  // existing isFlashing() guard so we never reset mid-OTA. When a callback
  // (e.g. OTA success handler) or timer (nightly restart) has set the pending
  // flag, the actual reset fires here — OUTSIDE the callback context so any
  // pending HTTP response bytes have already left the socket.
  rebootHeapWatermarkTick();
  if (isRebootPending() && !isFlashing()) performDeferredReboot();
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
