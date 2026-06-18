/* 
***************************************************************************  
**  Program  : OTGW-firmware.ino
**  Version  : v2.0.0-alpha.214
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

// Last millis() timestamp of a valid OT message received from processOT().
// Updated by OTGW-Core.ino. Used by LED heartbeat to detect "no OT traffic".
uint32_t lastOTmsgMs = 0;

bool readWifiPortalResetState(WifiPortalResetState &portalState) {
  return platformRtcRead(WIFI_PORTAL_RESET_RTC_SLOT, reinterpret_cast<uint32_t*>(&portalState), sizeof(portalState));
}

bool writeWifiPortalResetState(const WifiPortalResetState &portalState) {
  return platformRtcWrite(WIFI_PORTAL_RESET_RTC_SLOT, reinterpret_cast<const uint32_t*>(&portalState), sizeof(portalState));
}

void clearWifiPortalResetState() {
  // Skip the write when the stored state is already cleared: on ESP32 this is
  // an NVS commit, and a normal boot would otherwise burn a no-op flash write
  // every cycle (platform_esp32.h: call platformRtcWrite() infrequently).
  WifiPortalResetState storedState = { 0, 0 };
  if (readWifiPortalResetState(storedState) &&
      storedState.magic == WIFI_PORTAL_RESET_MAGIC && storedState.resetCount == 0) {
    return;
  }
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

  // Only persist when something actually changed — on a normal power-on boot
  // (no external reset, count already 0) this skips an NVS commit on ESP32.
  if (portalState.magic != storedState.magic ||
      portalState.resetCount != storedState.resetCount) {
    writeWifiPortalResetState(portalState);
  }
  return forcePortal;
}

bool wifiPortalResetWindowExpired() {
  return wifiPortalResetWindowOpen && ((int32_t)(millis() - wifiPortalResetWindowDeadline) >= 0);
}

#if HAS_RUNTIME_HW_DETECT
// ADR-127: persist the boot hardware-detection result to a capped, rolling
// LittleFS log so it can be read after the fact via FSexplorer when no
// USB/IDF console is attached. The same data goes to the console via log_e();
// this is the durable copy. Newest entry first, oldest trimmed at the cap
// (mirrors updateRebootLog() in helperStuff.ino).
static void appendBootDetectLog()
{
  #define BOOTDETECT_FILE     "/bootdetect.log"
  #define BOOTDETECT_TMP      "/bootdetect.t.log"
  #define BOOTDETECT_LINES    30
  #define BOOTDETECT_LINE_LEN 128

  if (!LittleFSmounted) return;

  // Monotonic boot number: parse the newest existing entry (first line) and +1.
  // Self-contained — survives independently of the global reboot counter, which
  // is not updated until later in setup().
  unsigned int bootNum = 1;
  File infh = LittleFS.open(BOOTDETECT_FILE, "r");
  if (infh) {
    char first[BOOTDETECT_LINE_LEN] = {0};
    int n = infh.readBytesUntil('\n', first, sizeof(first) - 1);
    first[n] = '\0';
    unsigned int prev = 0;
    // sscanf_P does not exist on ESP32 (this code is HAS_RUNTIME_HW_DETECT /
    // combo only). PSTR() is a no-op on ESP32, so plain sscanf is correct here.
    if (sscanf(first, PSTR("boot#%u"), &prev) == 1) bootNum = prev + 1;
    infh.close();
  }

  // Build the new detection line (mirrors the log_e console format).
  char newline[BOOTDETECT_LINE_LEN] = {0};
  snprintf_P(newline, sizeof(newline),
    PSTR("boot#%u t=%lums eMode=%d pic=%d mode=%d RST=%d RX=%d TX=%d I2C(classic)=%d/%d\n"),
    bootNum, (unsigned long)millis(),
    (int)state.hw.eMode, isPICEnabled() ? 1 : 0, (int)settings.iBoardMode,
    PIN_PIC_RST, PIN_PIC_RX, PIN_PIC_TX, PIN_CLASSIC_I2C_SDA, PIN_CLASSIC_I2C_SCL);

  // Write newest-first into a temp file, then append up to LINES-1 prior lines.
  File outfh = LittleFS.open(BOOTDETECT_TMP, "w");
  if (!outfh) {
    SetupDebugln(F("*** WARN: bootdetect log: temp open failed"));
    return;
  }
  outfh.print(newline);

  File oldfh = LittleFS.open(BOOTDETECT_FILE, "r");
  if (oldfh) {
    int i = 1;
    while (oldfh.available() && (i < BOOTDETECT_LINES)) {
      char line[BOOTDETECT_LINE_LEN] = {0};
      int n = oldfh.readBytesUntil('\n', line, sizeof(line) - 1);
      line[n] = '\0';
      if (n > 3) {                 // skip empty/short lines, keep file clean
        outfh.print(line);
        outfh.print('\n');
      }
      i++;
    }
    oldfh.close();
  }
  outfh.close();

  // Swap temp -> live (mirrors updateRebootLog()).
  if (LittleFS.exists(BOOTDETECT_FILE)) LittleFS.remove(BOOTDETECT_FILE);
  LittleFS.rename(BOOTDETECT_TMP, BOOTDETECT_FILE);
}

// ADR-127: after hardware-mode resolution the live pin map may differ from the
// pre-detection Classic default (or from the previous persisted mode). Move
// I2C, the ledc LED channels and the OLED (incl. its button pinMode) onto the
// resolved pins. All three are idempotent, so calling this when nothing
// changed is safe — boot-only cost.
static void applyResolvedComboPins()
{
  Wire.end();                                    // core 3.x: no silent re-pin
  Wire.begin(activeI2cSda(), activeI2cScl());
  reinitLedDriver();
  initOLED();                                    // re-probe 0x3C, re-pin button
}
#endif // HAS_RUNTIME_HW_DETECT

//=====================================================================
void setup() {

 
  // Serial is initialized by OTGWSerial. It resets the pic and opens serialdevice.
  // OTGWSerial.begin();//OTGW Serial device that knows about OTGW PIC
  // while (!Serial) {} //Wait for OK

  // I2C must be on the board's pins BEFORE the watchdog-disarm below. On the
  // ESP8266 the Wire defaults happen to equal PIN_I2C_SDA/SCL (4/5), but on the
  // ESP32-S3 Classic they do not (35/36 vs core defaults) — without this the
  // 0x26 disarm goes out on the wrong pins and an armed watchdog from the
  // previous session can reset the board mid-WiFi-portal. On the combo board
  // (ADR-127) settings are not read yet, so activeI2c*() resolves to the
  // CLASSIC pins — exactly where a real 0x26 lives; on an OTGW32 those pins
  // float and the disarm is a NACKed no-op. Fixed boards: PIN_I2C_SDA/SCL.
  Wire.begin(activeI2cSda(), activeI2cScl());
  WatchDogEnabled(0); // turn off watchdog

  SetupDebugln(F("\r\n[OTGW firmware - Nodoshop version]\r\n"));
  SetupDebugf(PSTR("Booting....[%s]\r\n\r\n"), _VERSION);

  //setup randomseed the right way
  randomSeed(platformHardwareRandom()); // Hardware RNG to seed the Random PRNG

  // TASK-865.5 (ADR-123 Phase-1): create the OT-frame queue + OTGWState mutex
  // ONCE, before any OT path can enqueue a frame. doBackgroundTasks() is gated
  // on bSetupComplete, so nothing parses OT before setup() finishes, but the
  // queue/mutex must exist so the very first drain in loop() has its handles.
  setupOTConcurrency();

  //setup the status LED
  setLed(LED1, ON);
  setLed(LED2, ON);

  LittleFSmounted = LittleFS.begin();
  if (!LittleFSmounted) SetupDebugln(F("*** ERROR: LittleFS mount FAILED - running on compile-time defaults ***"));
  cacheBootFlashInfo();   // A: cache static flash/FS values once; used by /api/v2/device/info
  readSettings(true);
  checklittlefshash();
  loadOtSupportFiles();  // TASK-693 port: warm the in-RAM support bitmaps from prior-boot knowledge

  // Set hostname ASAP after loading settings.  WiFi.persistent(true) from a
  // previous boot lets the SDK auto-connect before startWiFi() is reached;
  // without this early call the DHCP request carries the default "ESP-XXXXXX".
  platformSetHostname(CSTR(settings.sHostname));

#if HAS_RUNTIME_HW_DETECT
  // Settings are in: the persisted board mode may point at the other pin map
  // than the pre-settings Classic default that the 0x26 disarm above used.
  // Re-pin I2C so the OLED below comes up on the right bus. ESP32 core 3.x
  // does not silently re-pin on a second begin(); end() first.
  Wire.end();
  Wire.begin(activeI2cSda(), activeI2cScl());

  // Tear down UART1 (opened by the OTGWSerial global ctor) BEFORE the WiFi
  // portal: on an OTGW32 the PIC-RX pin floats and an RX interrupt storm can
  // starve the cooperative portal web server (ADR-125 field finding). The
  // hardware-mode block below re-opens it for the PIC probe.
  OTGWSerial.end();
#endif

  // [TASK-750] Bring up the OLED BEFORE startWiFi(): the WiFiManager config
  // portal blocks inside startWiFi(), so the boot splash and the "how to
  // connect" config screen must already be on the display by the time the
  // portal opens. I2C is already up (top of setup, before the watchdog disarm).
  initOLED();

  // Connect to and initialise WiFi network
  setLed(LED1, ON);
  SetupDebugln(F("Attempting to connect to WiFi network\r"));

  bool forceWifiPortal = shouldForceWifiConfigPortal();
  startWiFi(CSTR(settings.sHostname), 240, forceWifiPortal);  // timeout 240 seconds

#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) {
    SetupDebugf(PSTR("BETA: running in AP fallback mode, SSID=[%s]\r\n"), state.net.sAPSSID);
  }
#endif

  // ===== Hardware-mode resolution (ADR-127) =====
  // Runs AFTER WiFi configuration (TASK-853): the captive portal on a fresh
  // flash must never be blocked by the PIC probe or a starved UART.
  // Fixed boards: detect PIC, then init OTDirect if compiled in (unchanged).
  // Combo board: honour the persisted settings.iBoardMode override, else
  // PIC-probe-first (the PIC is the only reliable discriminator; OLED/W5500 are
  // optional on OTGW32) and cache the resolved mode so later boots skip probing.
#if HAS_RUNTIME_HW_DETECT
  if (settings.iBoardMode == 2) {
    SetupDebugln(F("Board mode: forced OT-Direct (no PIC probe)"));
    initOTDirect();              // UART1 already torn down before startWiFi()
  } else {
    // UART1 was closed before the WiFi portal; re-open it for the PIC probe.
    OTGWSerial.begin(9600, SERIAL_8N1, PIN_PIC_RX, PIN_PIC_TX);
    if (settings.iBoardMode == 1) {
      SetupDebugln(F("Board mode: forced PIC"));
      detectPIC();               // sets HW_MODE_PIC, or HW_MODE_DEGRADED if dead
    } else {
      SetupDebugln(F("Board mode: auto — PIC-probe-first"));
      detectPIC();               // drives only the PIC pins; benign on OTGW32
      if (isPICEnabled()) {
        settings.iBoardMode = 1; // cache: this is a PIC (Classic) board
      } else {
        // No PIC: tear down UART1 again BEFORE OT-direct, so a floating PIC-RX
        // pin can't drive an RX interrupt storm (ADR-125 field finding).
        OTGWSerial.end();
        initOTDirect();          // no PIC → OTDirect; sets HW_MODE_OT_DIRECT
        if (state.hw.eMode == HW_MODE_OT_DIRECT) settings.iBoardMode = 2;  // cache
      }
      if (settings.iBoardMode != 0) writeSettings(false);  // persist the decision
    }
  }
  // The resolved mode may select the other pin map than the pre-detection
  // default (Classic): re-pin I2C, re-attach the ledc LEDs and re-init the
  // OLED (incl. its button pinMode) on the live pins.
  applyResolvedComboPins();
  // Boot-time detection result to the ESP-IDF/USB console (ERROR level = always
  // printed, regardless of CORE_DEBUG_LEVEL; appears next to any task_wdt lines).
  // This is the ground-truth probe: did detection find the PIC, on which pins?
  log_e("[combo] detect: eMode=%d picEnabled=%d boardMode=%d (RST=%d RX=%d TX=%d I2C(classic)=%d/%d)",
        (int)state.hw.eMode, isPICEnabled() ? 1 : 0, (int)settings.iBoardMode,
        PIN_PIC_RST, PIN_PIC_RX, PIN_PIC_TX, PIN_CLASSIC_I2C_SDA, PIN_CLASSIC_I2C_SCL);
  // Durable copy of the detection result to LittleFS (/bootdetect.log),
  // readable via FSexplorer after a power cycle when no console is attached.
  appendBootDetectLog();
#else
  detectPIC();
  #if HAS_DIRECT_OT
  initOTDirect();         // initialize OT-direct GPIO (OTGW32 only)
  #endif
#endif

  // TASK-865.17: universal human-readable boot banner for the resolved hardware
  // mode on the telnet/debug stream (all three targets). The combo's log_e above
  // only reaches the USB console; detectPIC()/initOTDirect() log their own
  // detail but not one clean resolved-mode line. The indicator must be VISIBLE,
  // not a silent guess (combo board-delta audit). Mirrors the retained MQTT
  // otgw-firmware/hardware_mode topic published from sendMQTTversioninfo().
  SetupDebugf(PSTR("Hardware mode: %S\r\n"), (PGM_P)hardwareModeName());

  //setup NTP after WiFi; startNTP() restores hostname after configTime()
#if defined(_VERSION_PRERELEASE)
  if (!state.net.bAPFallback)
#endif
  startNTP();
  blinkLED(LED1, 3, 100);
  setLed(LED1, OFF);

  startTelnet();              // start the debug port 23
#if defined(_VERSION_PRERELEASE)
  if (strstr(_SEMVER_FULL, "alpha") || strstr(_SEMVER_FULL, "beta"))
    enableDebugForPrerelease();
#endif
  startMDNS(CSTR(settings.sHostname));
  startLLMNR(CSTR(settings.sHostname));
  setupFSexplorer();
  startWebserver();
  startWebSocket();          // start the WebSocket server for OT log streaming
#if defined(_VERSION_PRERELEASE)
  if (!state.net.bAPFallback)
#endif
  startMQTT();               // start the MQTT after webserver, always.

#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // [TASK-751] Probe the W5500; if a wired link with an IP is present, switch to
  // Ethernet and disable WiFi. Placed after startMDNS()/startWebSocket()/startMQTT()
  // because switchToEthernet() rebinds those services to the wired interface —
  // running it earlier would double-init them. OLED was already brought up above.
  // ADR-127: skip W5500 in combo PIC mode — the SPI probe clocks GPIO12, which
  // on a Classic PCB is the PIC reset hole, and a Classic PCB has no Ethernet.
  // isPICEnabled() is compile-time false on the fixed OTGW32, so this is a
  // no-op there.
  if (!isPICEnabled()) initEthernet();
#endif

  { char wdReason[64]; initWatchDog(wdReason, sizeof(wdReason)); }  // setup the WatchDog
  platformResetReason(lastReset, sizeof(lastReset));
  SetupDebugf(PSTR("Last reset reason: [%s]\r\n"), CSTR(lastReset));
  state.uptime.iRebootCount = updateRebootCount();
  updateRebootLog(lastReset);

  // One-line boot signature for field diagnostics (TASK-394 Phase 2).
  // Captured AFTER full init so heap/fragmentation reflect steady-state setup.
  logBootSignature("boot:");

  // TASK-396: warn once if flash hardware doesn't match the 4M2M DIO build.
  // Silent on matching boards; emits one or more [flash] WARN lines otherwise.
  // Dual-target via platformFlashChipMode()/RealSize()/Size(); on ESP32 the
  // DIO-mode sub-check is automatically skipped (mode=4 = platform unknown).
  maybeWarnFlashMismatch();


  SetupDebugln(F("Setup finished!\r\n"));

  // After resetting the OTGW PIC never send anything to Serial for debug
  // and switch to telnet port 23 for debug purposed. 
  // Setup the OTGW PIC
  resetOTGW();          // reset the OTGW pic
  startPICStream();    // start port 25238
  // TASK-865.6 (ADR-123 Phase-1): hand the PIC UART to its dedicated FreeRTOS
  // task. Started AFTER resetOTGW()/detectPIC() so boot-time control I/O
  // (resetPic/find/version readout) has already run loop-side; from here the
  // task is the sole runtime owner of OTGWSerial byte I/O. On an OTDirect combo
  // boot the task immediately parks (picSerialTaskShouldPark) and never touches
  // the closed UART. No-op on a no-PIC build.
  startPICSerialTask();
  // TASK-865.13 (ADR-123 Phase-4): start the dedicated webhook sender task. It
  // owns the blocking HTTPClient send off the loop; evalWebhook() and the REST
  // test endpoint only detect+enqueue. Created once (ADR-044); blocks on an
  // empty queue until an edge fires, so order vs settings load is immaterial.
  startWebhookTask();
 // initSensors();        // init DS18B20 (after MQ is up! )
  initOutputs();
  
  WatchDogEnabled(1);   // turn on watchdog
  sendPICBootCommands();   
  //Blink LED2 to signal setup done
  setLed(LED1, OFF);
  blinkLED(LED2, 3, 100);
  setLed(LED2, OFF);
  sendMQTTuptime();
  sendMQTTversioninfo();
  if (!LittleFSmounted) sendMQTTData(F("otgw-firmware/error"), "LittleFS mount failed - running on defaults", false);
  initS0Count();        // init S0 counter
  initSensors();        // init DS18B20 (after MQ is up!)
  initSAT();            // init SAT thermostat controller
  // Clear the triple-reset portal counter: a successful setup() proves the device is healthy.
  // This prevents USB flash resets or stale RTC data from triggering the portal on next boot.
  clearWifiPortalResetState();
  triggerPICsettingsReadout();  // Start initial PIC settings discovery cycle
  state.bSetupComplete = true; // ADR-036: allow doBackgroundTasks() to run service handlers
}
//=====================================================================

//===[ blink status led ]===

#if HAS_LEDC_LED
// --- ESP32-S3: PWM-dimmed LEDs via ledc ------------------------------------
// Active-LOW status LEDs (PIN_LED1/PIN_LED2 from boards.h).
// ledc output inversion: duty=0 → pin HIGH → LED off;
//                        duty=LED_BRIGHTNESS → pin briefly LOW → LED dim on.
// Same technique as OT-Thing firmware (LED_BRIGHTNESS=5, ~2% on-time).

#define LED1_LEDC_CHANNEL  0
#define LED2_LEDC_CHANNEL  1
#define LED_FREQ_HZ        5000
#define LED_RESOLUTION     8      // 8-bit: 0..255

static bool _led_state[2] = { false, false };  // index 0=LED1, 1=LED2
static bool _ledsInitDone = false;

static inline uint8_t _ledIdx(uint8_t led) { return (led == LED2) ? 1 : 0; }

static void _ensureLEDsInit() {
  if (_ledsInitDone) return;
  ledcAttachChannel(LED1, LED_FREQ_HZ, LED_RESOLUTION, LED1_LEDC_CHANNEL);
  ledcOutputInvert(LED1, true);   // active LOW → invert PWM
  ledcWrite(LED1, 0);
  ledcAttachChannel(LED2, LED_FREQ_HZ, LED_RESOLUTION, LED2_LEDC_CHANNEL);
  ledcOutputInvert(LED2, true);
  ledcWrite(LED2, 0);
  _ledsInitDone = true;
}

#if HAS_RUNTIME_HW_DETECT
// ADR-127: the LED driver attaches on first setLed() call — that is BEFORE
// settings/detection, so on the combo the channels may sit on the wrong
// board's pins. Detach both candidate positions and re-attach on the live map
// (LED1/LED2 resolve through activeLed*()). ledcDetach on a never-attached pin
// is a harmless no-op.
void reinitLedDriver() {
  if (!_ledsInitDone) return;
  ledcDetach(PIN_CLASSIC_LED1);
  ledcDetach(PIN_CLASSIC_LED2);
  ledcDetach(PIN_LED1);
  ledcDetach(PIN_LED2);
  _ledsInitDone = false;
  _led_state[0] = _led_state[1] = false;
  _ensureLEDsInit();
}
#endif

void setLed(uint8_t led, uint8_t status) {
  _ensureLEDsInit();
  bool on = (status == ON);
  _led_state[_ledIdx(led)] = on;
  ledcWrite(led, on ? LED_BRIGHTNESS : 0);
}

void blinkLEDnow(uint8_t led) {
  _ensureLEDsInit();
  if (settings.bLEDblink) {
    bool on = !_led_state[_ledIdx(led)];
    _led_state[_ledIdx(led)] = on;
    ledcWrite(led, on ? LED_BRIGHTNESS : 0);
  } else {
    setLed(led, OFF);
  }
}

#else  // plain digitalWrite LEDs (ESP8266) ------------------------------------

void setLed(uint8_t led, uint8_t status){
  pinMode(led, OUTPUT);
  digitalWrite(led, status);
}

void blinkLEDnow(uint8_t led){
  pinMode(led, OUTPUT);
  if (settings.bLEDblink) {
    digitalWrite(led, !digitalRead(led));
  } else setLed(led, OFF);
}

#endif  // HAS_LEDC_LED

// Zero-argument wrapper — calls LED1 variant. Defined once for both boards.
void blinkLEDnow() { blinkLEDnow(LED1); }

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

//===[ cooperative delay running background tasks in ms ]===
// Previously used DECLARE_TIMER_MS, which expands to static locals: init-once
// semantics made the wait collapse to 0 ms on the first call and freeze at the
// first interval ever passed on later calls. Use a local timestamp so each call
// honours its own delay_ms parameter and millis() rollover is handled by the
// unsigned subtraction.
void delayms(unsigned long delay_ms)
{
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < delay_ms) {
    doBackgroundTasks();
  }
}

//=====================================================================

//===[ Do task every 1s ]===
void doTaskEvery1s(){
  //== do tasks ==
  handleCommandQueue(); //just check if there are commands to retry
  state.uptime.iSeconds++;

  // LED status indicators (evaluated every 1s):
  //   No WiFi          → LED2 blinks 1x/s, LED1 off
  //   WiFi + no OT >10s → LED1 blinks 1x/s, LED2 blinks 2x/s (handled by timer500ms in loop)
  //   WiFi + OT active → LEDs off (normal operation takes over)
  {
    static bool _hbLed2    = false;
    static bool _hbLed1    = false;
    static bool _wasNoWiFi = false;
    static bool _wasNoOT   = false;

    bool noWiFi = (WiFi.status() != WL_CONNECTED);
    bool noOT   = !noWiFi &&
                  ((lastOTmsgMs == 0) || ((millis() - lastOTmsgMs) > 10000UL));

    if (noWiFi) {
      _hbLed2 = !_hbLed2;
      setLed(LED2, _hbLed2 ? ON : OFF);
      setLed(LED1, OFF);
      _wasNoWiFi = true;
    } else {
      if (_wasNoWiFi) {
        // Just got WiFi: clear LED2 so normal op can take over
        setLed(LED2, OFF);
        _wasNoWiFi = false;
      }

      if (noOT) {
        // LED1 @ 1x/s; LED2 @ 2x/s is driven by timer500ms in loop()
        _hbLed1 = !_hbLed1;
        setLed(LED1, _hbLed1 ? ON : OFF);
        _wasNoOT = true;
      } else if (_wasNoOT) {
        // OT traffic resumed: turn both indicators off
        setLed(LED1, OFF);
        setLed(LED2, OFF);
        _wasNoOT = false;
      }
    }
  }

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
#if HAS_PIC
  // ADR-127: on a combo running OTDirect the PIC UART is closed and the retry
  // would be pure noise forever — gate on !isOTDirectEnabled(). On the fixed
  // PIC boards isOTDirectEnabled() is compile-time false, so behaviour there
  // is unchanged.
  if (!isOTDirectEnabled() &&
      ((strcmp_P(state.pic.sDeviceid, PSTR("unknown")) == 0)
      || (strcmp_P(state.pic.sDeviceid, PSTR("no pic found")) == 0)
      || (state.pic.sDeviceid[0] == '\0'))) {
    DebugTln(F("PIC is unknown, probe pic using PR=A"));
    // TASK-865.6: route through the PIC task's TX queue (sole UART writer)
    // instead of writing OTGWSerial directly.
    static const uint8_t kProbePRA[] = { 'P','R','=','A','\r','\n' };
    enqueuePICTx(kProbePRA, sizeof(kProbePRA));
  }
#endif
  
  // Log heap statistics every minute for monitoring
  logHeapStats();

  // Hourly/daily dispatch moved to doTaskMinuteChanged (ADR-086 / TASK-350):
  // wall-clock aligned instead of boot-relative 60s drift. See that function
  // for the single call site of hourChanged/dayChanged/yearChanged.
}

// Extracted from the old hourly block so the new dispatcher reads cleanly
// (ADR-086). Preserves existing guards: bNightlyRestart + ntp.bEnable +
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
// per ADR-086. The four helpers (minuteChanged + hourChanged/dayChanged/
// yearChanged) each have EXACTLY ONE call site in the firmware:
//   - minuteChanged() : main loop gate (OTGW-firmware.ino:366)
//   - hourChanged()   : this function
//   - dayChanged()    : this function
//   - yearChanged()   : this function
// Downstream consumers read the local flag, never re-call the helper.
// evaluate.py::check_time_boundary_single_caller enforces this rule.
void doTaskMinuteChanged(){
  // ADR-086: these three helpers are called here and only here. Downstream
  // consumers read the local flags below, never re-call the helper.
  // Enforced by evaluate.py::check_time_boundary_single_caller.
  const bool hourFlag = hourChanged();
  const bool dayFlag  = dayChanged();
  const bool yearFlag = yearChanged();

  // Per-minute work (unconditional).
  // WiFi reconnect is handled by loopWifi() state machine in doBackgroundTasks().
  sendtimecommand(dayFlag, yearFlag);

  // TASK-692 port (dev TASK-686): republish boiler unsupported-msgID CSV only
  // when a new id has been observed since the last publish. After boot warm-up
  // the dirty flag fires a handful of times then stays clean, so this is at
  // most one extra publish per minute and zero in steady state.
  if (getBoilerUnsupportedDirty()) {
    publishBoilerUnsupportedMsgids();
    clearBoilerUnsupportedDirty();
  }

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

//===[ Do task every 15min — TASK-693 port ]===
// Debounced flush of /ot-thermo.json and /ot-boiler.json. The per-file dirty
// flag in OTGW-Core.ino gates the actual write, so a quiet boiler/thermostat
// causes zero filesystem writes. Ceiling: 2 writes per 15-min window per file,
// roughly 192 writes per day on a part rated 100k+ erase cycles per sector.
void do15minevent(){
  saveOtSupportFilesIfDirty();
}

static void handleEspFlashBackgroundTasks()
{
  handleDebug();              // Keep telnet debug active for monitoring
  // TASK-865.9: HTTP runs on the AsyncTCP task — no handleClient() pump needed.
  // TASK-865.10: WebSocket is serviced on the AsyncTCP task too — no handleWebSocket()
  // pump needed during flash. fwupgradestep() progress messages keep the socket warm.
#if MDNS_NEEDS_UPDATE
  MDNS.update();
#endif              // Keep MDNS active for network discovery
}

static void handlePicFlashBackgroundTasks()
{
  handleDebug();              // Keep telnet debug active for monitoring
  // TASK-865.9: HTTP runs on the AsyncTCP task — no handleClient() pump needed.
#if MDNS_NEEDS_UPDATE
  MDNS.update();
#endif              // Keep MDNS active for network discovery
#if HAS_PIC
  // TASK-865.6: the dedicated PIC task is parked during a flash; the loop-side
  // path must drive the OTGWSerial upgrade state machine. A single available()
  // poll per call ticks upgradeTick() (OTGWSerial byte-I/O short-circuits to the
  // upgrade FSM while busy()). This REPLACES the old handlePICSerial() drain.
  picSerialPumpUpgrade();
#endif
  // TASK-865.10: WebSocket runs on the AsyncTCP task — no handleWebSocket() pump
  // needed during flash. fwupgradestep() progress messages keep the socket warm.
}


//===[ Do the background tasks ]===
void doBackgroundTasks()
{
  feedWatchDog();               // Feed the dog before it bites!

  // ADR-036: block service handlers until setup() completes.
  // blinkLED/delayms in setup() would otherwise invoke handleMQTT() before
  // startMQTT() sets the 1350-byte buffer, and handlePICSerial() before resetOTGW().
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
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (!isFlashing()) loopEthernet();   // W5500 link-poll + WiFi↔Ethernet failover (every 5s)
#endif

  // Check for critically low heap and attempt recovery if needed
  if (getHeapHealth() == HEAP_CRITICAL) {
    emergencyHeapRecovery();
  }

  if (isNetworkUp()) {
    if (state.flash.bESPactive) {
      handleEspFlashBackgroundTasks();
    } else if (state.flash.bPICactive) {
      handlePicFlashBackgroundTasks();
    } else {
      //while connected handle everything that uses network stuff
      handleDebug();
      handleOTGWstream();          // OTGW serial bridge on TCP port 25238
      handleMQTT();                 // MQTT transmissions
      handlePICSerial();            // OTGW/PIC handling
#if HAS_DIRECT_OT
      // Run the OT-direct engine only when the OT-direct hardware is active
      // (guards against a degraded init on the fixed OTGW32).
      if (isOTDirectEnabled()) {
        handleOTDirectBridgeStream(); // OTGW32/TCP 25238 command bridge
        loopOTDirect();               // OT-direct GPIO poll
      }
#endif
      // TASK-865.10: WebSocket serving moved onto the AsyncTCP service task — there
      // is no longer a per-loop library poll pump. handleWebSocket() now only does
      // periodic housekeeping (cleanupClients + 30s keepalive), so it runs on a 1s
      // timer rather than every loop turn.
      {
        DECLARE_TIMER_SEC(timerWsHousekeeping, 1, SKIP_MISSED_TICKS);
        if (DUE(timerWsHousekeeping)) handleWebSocket();
      }
      // TASK-865.9: HTTP serving moved onto the AsyncTCP service task — there is
      // no longer a per-loop handleClient() drain. The sat-slider stall / XHR
      // latency ramp (TASK-817) is gone because every parallel socket is served
      // on the async task instead of one-per-loop-turn here.
    #if MDNS_NEEDS_UPDATE
  MDNS.update();
#endif
      loopNTP();
    }
  } //otherwise, just wait until reconnected gracefully
  yield();
  return;
}

void loop()
{
  // TASK-866/879 loop-stall detector. AsyncTCP runs on core 1
  // (CONFIG_ASYNC_TCP_RUNNING_CORE=1) where the Arduino loopTask also runs, so a
  // long synchronous section in this loop starves AsyncTCP (multi-second HTTP
  // latency) and, past the 30s loop-task TWDT, panics into a 'Boot: Task watchdog'
  // reboot. Measure the wall-clock gap between consecutive loop() entries: keep the
  // max as a banner watermark, and timestamp any gap >200ms so the preceding
  // per-section debug line names the culprit. Unsigned subtraction is millis()
  // rollover-safe.
  {
    static uint32_t s_lastLoopMs = 0;
    uint32_t nowMs = millis();
    if (s_lastLoopMs != 0) {
      uint32_t gapMs = nowMs - s_lastLoopMs;
      if (gapMs > state.heapdiag.iMaxLoopGapMs) state.heapdiag.iMaxLoopGapMs = gapMs;
      if (gapMs > 200) DebugTf(PSTR("[loop-stall] %lu ms gap\r\n"), (unsigned long)gapMs);
    }
    s_lastLoopMs = nowMs;
  }

  DECLARE_TIMER_SEC(timer1s,   1,   SKIP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer3s,   3,   SKIP_MISSED_TICKS);
  DECLARE_TIMER_MS(timer500ms, 500, SKIP_MISSED_TICKS);
  DECLARE_TIMER_SEC(timer60s, 60, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer5min, 5, CATCH_UP_MISSED_TICKS);
  DECLARE_TIMER_MIN(timer15min, 15, CATCH_UP_MISSED_TICKS);  // TASK-693 port

  if (!isFlashing()) {
    // Only run these tasks when NOT flashing firmware (ESP or PIC)
    if (DUE(timerFlushSettings))      flushSettings();  // coalesced settings write + service restarts
    if (DUE(timerpollsensor))         pollSensors();    // poll the temperature sensors connected to 2wire gpio pin
    if (DUE(timers0counter))          sendS0Counters(); // poll the s0 counter connected to gpio pin when due
    if (DUE(timer15min))              do15minevent();   // TASK-693 port: persist /ot-thermo.json + /ot-boiler.json
    if (DUE(timer5min))               do5minevent();
    if (DUE(timer60s))                doTaskEvery60s();
    if (DUE(timer3s))                 doTaskEvery3s();
    if (DUE(timer1s))                 doTaskEvery1s();
    if (DUE(timer500ms)) {
      // LED2 fast blink (2x/s) when WiFi is up but no OT traffic for >10s
      bool noOT = (WiFi.status() == WL_CONNECTED) &&
                  ((lastOTmsgMs == 0) || ((millis() - lastOTmsgMs) > 10000UL));
      if (noOT) {
        static bool _led2Fast = false;
        _led2Fast = !_led2Fast;
        setLed(LED2, _led2Fast ? ON : OFF);
      }
    }
    if (minuteChanged())              doTaskMinuteChanged(); //ADR-086: sole minuteChanged() caller; hour/day/year dispatch lives inside
    loopMQTTDiscovery();              // async MQTT discovery drip (self-timed, 2s normal / 10s slow)
    runTopicCleanupStep();            // ADR-106: drain stale-mode discovery topics after bUseLegacyOtTopics toggle
    evalOutputs();                    // when the bits change, the output gpio bit will follow
    evalWebhook();                    // when the trigger bit changes, fire the webhook
    satControlLoop();                 // SAT thermostat control loop (timer-guarded internally)
    satBLELoop();                     // BLE temperature sensor scan (timer-guarded, Task #20). TASK-742: no-op stub on ESP8266.
    weatherLoop();                    // Weather data fetch (timer-guarded, Task #50)
#if HAS_PIC
    handlePendingUpgrade();           // Check if we need to start an upgrade
    handlePendingPicHttp();           // TASK-865.14: run deferred PIC update-check/refresh outbound HTTP off the AsyncTCP task
#endif
    loopOLED();                       // OLED display refresh and button handling (no-op if no OLED detected)
  }

  doBackgroundTasks();              // run background tasks

  // TASK-865.5 (ADR-123 Phase-1): consume OT frames produced this iteration.
  // handlePICSerial()/loopOTDirect() ran inside doBackgroundTasks() and enqueued
  // frames; drain them HERE (loop() proper) — never inside doBackgroundTasks(),
  // which re-enters via doAutoConfigure's file-reading loop and could nest the
  // OTStateLock. processOT() runs from loop() context (not a task) in Phase 1.
  drainOTFrameQueue();

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
