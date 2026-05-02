/*
***************************************************************************
**  Program  : networkStuff.ino
**  Version  : v2.0.0-alpha
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
**
**  Implementation of WiFi, mDNS, LLMNR, Telnet, and NTP helpers.
**  Declarations live in networkStuff.h (included via OTGW-firmware.h).
*/

//=====[ Variable Definitions ]================================================
// Declared extern in networkStuff.h — defined here exactly once.

NtpStatus_t NtpStatus  = TIME_NOTSET;
time_t      NtpLastSync = 0;
// Note: sDhcpHostnameFixed and the platformRestartDHCP() call sites it guarded
// were removed (TASK-432, ported from dev). Calling the SDK's DHCP client
// stop/start while the station is connected takes DHCP ownership away from the
// SDK, so subsequent setAutoReconnect()-driven reassociations no longer auto-
// restart DHCP. The result was "associates but no IP after reboot" reported
// against dev's 1.5.0-beta+d40c2f6. The same call shape existed here on 2.0.0
// in three sites (startWiFi catch-all, WIFI_RECONNECTED, startNTP) and is now
// removed across the board. Hostname propagation still happens via
// platformSetHostname() before WiFi.begin(); the SDK manages DHCP autonomously.

// Debug telnet instance (port 23). SimpleTelnet replaces ESPTelnet for debug output.
// Port is fixed in the constructor; begin() needs no port argument.
SimpleTelnet<1> debugTelnet(23);

OTGWWebServer           httpServer(80);
OTGWUpdateServer        httpUpdater(true);

FSInfo LittleFSinfo;
bool   LittleFSmounted = false;

//=====[ WiFi ]================================================================

//gets called when WiFiManager enters configuration mode
static void configModeCallback(WiFiManager *myWiFiManager)
{
  DebugTln(F("\nEntered config mode"));
  DebugTf(PSTR("SSID: %s\r\n"), myWiFiManager->getConfigPortalSSID().c_str());
  DebugTf(PSTR("IP address: %s\r\n"), WiFi.softAPIP().toString().c_str());
  DebugTln(F("Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  DebugTln(myWiFiManager->getConfigPortalSSID());
} // configModeCallback()

void resetWiFiSettings(void)
{
  // Is it safe to re-setup this object? other one only lives in startWiFi.
  WiFiManager manageWiFi;
  manageWiFi.resetSettings();
}

//===========================================================================================
void startWiFi(const char* hostname, int timeOut, bool forcePortal)
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  WiFiManager manageWiFi;
  uint32_t lTime = millis();
  char thisAP[64];
  strlcpy(thisAP, hostname, sizeof(thisAP));
  strlcat(thisAP, "-", sizeof(thisAP));
  // Use eFuse MAC (always valid) — WiFi.macAddress() returns 00:00:00:00:00:00 before WiFi.begin()
  strlcat(thisAP, getMacAddress() + 6, sizeof(thisAP)); // last 6 chars = last 3 MAC bytes (e.g. "21B4F8")

  DebugTln(F("\nStart Wifi ..."));
  manageWiFi.setDebugOutput(true);

  //--- set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  //--- sets timeout until configuration portal gets turned off
  manageWiFi.setConfigPortalTimeout(timeOut);  // in seconden ...

  //--- remove Info and Update buttons from Configuration Portal (security improvement 20230102)
  std::vector<const char *> wm_menu  = {"wifi", "exit"};
  manageWiFi.setShowInfoUpdate(false);
  manageWiFi.setShowInfoErase(false);
  manageWiFi.setMenu(wm_menu);
  manageWiFi.setHostname(hostname);

  bool wifiSaved     = manageWiFi.getWiFiIsSaved();
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);

  DebugTf(PSTR("Wifi status: %s\r\n"), wifiConnected ? "Connected" : "Not connected");
  DebugTf(PSTR("Wifi AP stored: %s\r\n"), wifiSaved ? "Yes" : "No");
  DebugTf(PSTR("Config portal SSID: %s\r\n"), thisAP);

  if (forcePortal)
  {
    DebugTln(F("Triple-reset detected, forcing WiFi config portal."));
    wifiConnected = false;
  }
  else if (wifiConnected)
  {
    // If the SDK auto-connected using a default/old hostname, ensure the DHCP
    // server sees the desired hostname. Avoid forcing a reconnect here so we
    // don't accidentally drop a working connection and fall back into the
    // WiFiManager config portal on transient failures.
    String currentHostname = platformGetHostname();
    if (currentHostname == hostname)
    {
      DebugTln(F("Wifi already connected with correct hostname, skipping hostname update."));
    }
    else
    {
      DebugTf(PSTR("Wifi connected with hostname '%s', updating to '%s' without reconnect.\r\n"),
              currentHostname.c_str(), hostname);
      // Update the hostname for future DHCP negotiations; the current lease
      // will typically keep using the old hostname until the next reconnect.
      platformSetHostname(hostname);
      DebugTln(F("Hostname updated; keeping existing WiFi connection active."));
    }
  }
  else if (wifiSaved)
  {
    DebugTln(F("Saved WiFi found, attempting direct connect..."));
    int directConnectTimeout = timeOut / 2;
    if (directConnectTimeout < 5) directConnectTimeout = 5;
    DebugTf(PSTR("Direct connect timeout: %d sec\r\n"), directConnectTimeout);
    platformSetHostname(hostname);  // set before begin() so DHCP sends the correct hostname
    WiFi.begin(); // use stored credentials
    DECLARE_TIMER_SEC(timeoutWifiConnectInitial, directConnectTimeout, CATCH_UP_MISSED_TICKS);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(100);
      feedWatchDog();
      if DUE(timeoutWifiConnectInitial) break;
    }
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    DebugTf(PSTR("Direct connect result: %s\r\n"), wifiConnected ? "Connected" : "Failed");
  }
  else
  {
    DebugTln(F("No saved WiFi, starting config portal."));
  }

  if (!wifiConnected)
  {
#if defined(_VERSION_PRERELEASE)
    if (wifiSaved && !forcePortal) {
      // BETA: credentials stored but WiFi unreachable at boot — enter AP fallback
      // instead of blocking in the config portal.
      DebugTln(F("BETA: WiFi unavailable at boot, skipping config portal → AP fallback"));
      startAPFallback();
      // Set up OTA updater so firmware flashing works from AP mode
      httpUpdater.setup(&httpServer);
      httpUpdater.setIndexPage(UpdateServerIndex);
      httpUpdater.setSuccessPage(UpdateServerSuccess);
      if (settings.sHTTPpasswd[0] != '\0') {
        httpUpdater.updateCredentials("admin", settings.sHTTPpasswd);
      }
      DebugTf(PSTR(" took [%lu] seconds => AP fallback\r\n"), (millis() - lTime) / 1000);
      return;
    }
#endif
    DebugTln(F("Starting config portal..."));
    if (!manageWiFi.startConfigPortal(thisAP))
    {
      DebugTln(F("failed to connect and hit timeout"));
      delay(2000);  // Enough time for messages to be sent.
      platformRestart();
      delay(5000);  // Enough time to ensure we don't return.
    }
  }
  DebugTf(PSTR("Wifi status: %s\r\n"), WiFi.status() == WL_CONNECTED ? "Connected" : "Not connected");
  DebugTf(PSTR("Connected to: %s\r\n"), WiFi.localIP().toString().c_str());
  // SDK auto-reconnect handles brief WiFi glitches (channel hops, momentary
  // interference) transparently at the radio level, often in <1 second.
  // loopWifi() (ADR-047) is the fallback for longer outages.
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  DebugTf(PSTR("Wifi status: %s\r\n"), WiFi.status() == WL_CONNECTED ? "Connected" : "Not connected");
  if (WiFi.status() != WL_CONNECTED)
  {
    DECLARE_TIMER_SEC(timeoutWifiConnectFinal, timeOut, CATCH_UP_MISSED_TICKS);
    while ((WiFi.status() != WL_CONNECTED))
    {
      delay(100);
      feedWatchDog();
      if DUE(timeoutWifiConnectFinal) break;
    }
  }

  Debugln();
  DebugT(F("Connected to " )); Debugln(WiFi.SSID());
  DebugT(F("IP address: " ));  Debugln(WiFi.localIP());
  DebugT(F("IP gateway: " ));  Debugln(WiFi.gatewayIP());
  Debugln();

  // Apply the configured hostname for the next DHCP exchange (renewal or
  // reconnect). No SDK DHCP calls here: the SDK manages DHCP autonomously
  // when user code never invokes wifi_station_dhcpc_start/stop. See TASK-432
  // for why the catch-all DHCP re-announce was removed.
  platformSetHostname(hostname);

  httpUpdater.setup(&httpServer);
  httpUpdater.setIndexPage(UpdateServerIndex);
  httpUpdater.setSuccessPage(UpdateServerSuccess);
  // Apply HTTP Basic Auth credentials to OTA update server if password is configured
  if (settings.sHTTPpasswd[0] != '\0') {
    httpUpdater.updateCredentials("admin", settings.sHTTPpasswd);
  }
  DebugTf(PSTR(" took [%lu] seconds => OK!\r\n"), (millis() - lTime) / 1000);

} // startWiFi()

//===========================================================================================
#if defined(_VERSION_PRERELEASE)
// *** BETA ONLY — AP fallback ***
// All code in this block is guarded by _VERSION_PRERELEASE.
// Production builds (no prerelease tag) will never compile or run this code.
// WARNING: DO NOT enable in production — remove _VERSION_PRERELEASE tag before release.

static void startAPFallback() {
  // Build SSID: OTGW-<last 3 bytes of MAC, uppercase hex>
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char apSSID[32];
  snprintf_P(apSSID, sizeof(apSSID), PSTR("OTGW-%02X%02X%02X"), mac[3], mac[4], mac[5]);

  strlcpy(state.net.sAPSSID, apSSID, sizeof(state.net.sAPSSID));
  state.net.bAPFallback = true;

  WiFi.mode(WIFI_AP_STA);  // AP + STA so WiFi.begin() can retry without dropping AP
  WiFi.softAP(apSSID, "otgw123");

  DebugTf(PSTR("BETA AP: fallback started SSID=[%s] IP=192.168.4.1 pass=otgw123\r\n"), apSSID);
}

static void stopAPFallback() {
  if (!state.net.bAPFallback) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  state.net.bAPFallback = false;
  state.net.sAPSSID[0] = '\0';
  DebugTln(F("BETA AP: fallback stopped, WiFi connected"));
}
#endif  // _VERSION_PRERELEASE

//===========================================================================================
enum WifiState_t {
  WIFI_IDLE,         // connected, monitoring
  WIFI_DISCONNECTED, // just dropped — start reconnect
  WIFI_CONNECTING,   // waiting non-blocking for connection
  WIFI_RECONNECTED,  // just came back — restart services
  WIFI_FAILED,       // too many retries — trigger reboot
#if defined(_VERSION_PRERELEASE)
  WIFI_AP_FALLBACK,  // BETA: SoftAP active, retry WiFi every 5 min
  WIFI_AP_RETRY,     // BETA: attempting WiFi reconnect from AP mode
#endif
};
static WifiState_t wifiState = WIFI_IDLE;
static int wifiRetryCount = 0;

void loopWifi() {
  DECLARE_TIMER_SEC(timerWifiRetry,  30, CATCH_UP_MISSED_TICKS);
#if defined(_VERSION_PRERELEASE)
  DECLARE_TIMER_SEC(timerAPRetry,   300, CATCH_UP_MISSED_TICKS);  // 5 min between AP→WiFi retries
  DECLARE_TIMER_SEC(timerAPConnWait, 30, CATCH_UP_MISSED_TICKS);  // 30 s to wait for reconnect from AP
#endif

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
#if defined(_VERSION_PRERELEASE)
        // BETA: after 2 retries (≈60s) enter AP fallback instead of rebooting
        if (wifiRetryCount >= 2) {
          DebugTln(F("BETA: WiFi retries exhausted, entering AP fallback mode"));
          startAPFallback();
          RESTART_TIMER(timerAPRetry);
          wifiState = WIFI_AP_FALLBACK;
        } else {
          wifiState = WIFI_DISCONNECTED;
        }
#else
        if (wifiRetryCount >= 10) {
          wifiState = WIFI_FAILED;
        } else {
          wifiState = WIFI_DISCONNECTED;  // retry
        }
#endif
      }
      break;

    case WIFI_RECONNECTED:
      // Re-apply the configured hostname so the next DHCP exchange uses it.
      // No SDK DHCP calls here: the SDK already handled DHCP as part of the
      // association triggered by WiFi.begin() in WIFI_DISCONNECTED. See
      // TASK-432 for why platformRestartDHCP() was removed (it took DHCP
      // ownership away from the SDK and broke setAutoReconnect-driven DHCP
      // on subsequent reassociations).
#if defined(_VERSION_PRERELEASE)
      stopAPFallback();  // tear down AP if it was active
#endif
      platformSetHostname(CSTR(settings.sHostname));
      startTelnet();
      // OTGWstream is auto-started by handleOTGW() in the main loop
      startMQTT();
      startWebSocket();
      wifiState = WIFI_IDLE;
      DebugTf(PSTR("WiFi: reconnected, services restarted; IP=%s\r\n"),
              WiFi.localIP().toString().c_str());
      break;

    case WIFI_FAILED:
      doRestart("WiFi: too many reconnect failures");
      break;

#if defined(_VERSION_PRERELEASE)
    case WIFI_AP_FALLBACK:
      // Periodically attempt to reconnect to the configured WiFi network
      if (DUE(timerAPRetry)) {
        DebugTln(F("BETA AP: 5-min timer — attempting WiFi reconnect"));
        platformSetHostname(CSTR(settings.sHostname));
        WiFi.begin();  // uses stored credentials
        RESTART_TIMER(timerAPConnWait);
        wifiState = WIFI_AP_RETRY;
      }
      break;

    case WIFI_AP_RETRY:
      if (WiFi.status() == WL_CONNECTED) {
        DebugTln(F("BETA AP: WiFi reconnected from AP mode"));
        wifiState = WIFI_RECONNECTED;
        wifiRetryCount = 0;
      } else if (DUE(timerAPConnWait)) {
        DebugTln(F("BETA AP: WiFi reconnect timed out, staying in AP fallback"));
        WiFi.disconnect();
        RESTART_TIMER(timerAPRetry);
        wifiState = WIFI_AP_FALLBACK;
      }
      break;
#endif  // _VERSION_PRERELEASE
  }
}

//===========================================================================================
// Send the welcome banner to a freshly-connected telnet client.
// Called from the SimpleTelnet onConnect callback — receives client IP as const char*.
static void sendTelnetBanner(const char* ip)
{
  debugTelnet.println(F("\r\n============================================"));
  debugTelnet.println(F("  OpenTherm Gateway -- OTGW-firmware"));
  _debugPrintf_P(PSTR("  Version : %s\r\n"), _VERSION);
  debugTelnet.println(F("============================================"));
  _debugPrintf_P(PSTR("  IP      : %s\r\n"), getActiveIP().c_str());
  debugTelnet.print(F("  Network : "));
  debugTelnet.println(networkModeName());
  _debugPrintf_P(PSTR("  OTGW    : %-10s  MQTT : %s\r\n"),
    state.otBus.bOnline    ? "online"     : "offline",
    state.mqtt.bConnected  ? "connected"  : "disconnected");
  _debugPrintf_P(PSTR("  Heap    : %u bytes free\r\n"), platformFreeHeap());
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Debug flags (key to toggle):"));
  _debugPrintf_P(PSTR("    1 OT messages : %s\r\n"), CBOOLEAN(state.debug.bOTmsg));
  _debugPrintf_P(PSTR("    2 REST API    : %s\r\n"), CBOOLEAN(state.debug.bRestAPI));
  _debugPrintf_P(PSTR("    3 MQTT comms  : %s\r\n"), CBOOLEAN(state.debug.bMQTT));
  _debugPrintf_P(PSTR("    4 MQTT gating : %s\r\n"), CBOOLEAN(state.debug.bMQTTGate));
  _debugPrintf_P(PSTR("    5 Sensors     : %s\r\n"), CBOOLEAN(state.debug.bSensors));
  _debugPrintf_P(PSTR("    6 NTP sync    : %s\r\n"), CBOOLEAN(state.debug.bNTP));
#if defined(ESP32)
  _debugPrintf_P(PSTR("    7 SAT BLE     : %s\r\n"), CBOOLEAN(state.debug.bSATBLE));
#endif
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Commands:"));
  debugTelnet.println(F("    w  Open-Meteo fetch + dump"));
  debugTelnet.println(F("  Press 'h' for the full debug menu."));
  _debugPrintf_P(PSTR("  Connected from: %s\r\n"), ip);
  debugTelnet.println(F("============================================\r\n"));
}

//===========================================================================================
// Forward declaration — defined in handleDebug.ino.
void handleDebugChar(char c);

// SimpleTelnet input callback: line mode off means one char per keypress.
static void onTelnetInput(const char* s) {
  if (s && s[0] != '\0') handleDebugChar(s[0]);
}

//===========================================================================================
void startTelnet()
{
  debugTelnet.onConnect(sendTelnetBanner);
  debugTelnet.setLineMode(false);
  debugTelnet.onInputReceived(onTelnetInput);
  debugTelnet.begin(false);        // port was fixed in the constructor (23)
  DebugT(F("\r\nTelnet debug server started on "));
  DebugT(getActiveIP());
  DebugTln(F(":23"));
} // startTelnet()

//=======================================================================
void startMDNS(const char *hostname)
{
  DebugTf(PSTR("mDNS setup as [%s.local]\r\n"), hostname);
  if (MDNS.begin(hostname))               // Start the mDNS responder for Hostname.local
  {
    DebugTf(PSTR("mDNS responder started as [%s.local]\r\n"), hostname);
  }
  else
  {
    DebugTln(F("Error setting up MDNS responder!\r\n"));
  }
  MDNS.addService("http", "tcp", 80);
} // startMDNS()

void startLLMNR(const char *hostname)
{
#if HAS_LLMNR
  DebugTf(PSTR("LLMNR setup as [%s]\r\n"), hostname);
  if (LLMNR.begin(hostname))               // Start the LLMNR responder for hostname
  {
    DebugTf(PSTR("LLMNR responder started as [%s]\r\n"), hostname);
  }
  else
  {
    DebugTln(F("Error setting up LLMNR responder!\r\n"));
  }
#else
  (void)hostname;  // LLMNR not available on this platform
#endif
} // startLLMNR()

//=====[ NTP ]=================================================================

void startNTP()
{
  if (!settings.ntp.bEnable) return;
  if (strlen(settings.ntp.sTimezone) == 0) strlcpy(settings.ntp.sTimezone, NTP_DEFAULT_TIMEZONE, sizeof(settings.ntp.sTimezone));
  if (strlen(settings.ntp.sHostname) == 0) strlcpy(settings.ntp.sHostname, NTP_HOST_DEFAULT, sizeof(settings.ntp.sHostname));

#if defined(ESP8266)
  // ESP8266 SDK bug: configTime() resets the WiFi station hostname to "ESP-XXXXXX"
  // on some SDK versions. Save hostname before and restore after. Not needed on ESP32
  // (configTime() does not touch the hostname there).
  platformSetHostname(CSTR(settings.sHostname));
#endif
  configTime(0, 0, settings.ntp.sHostname, nullptr, nullptr);
#if defined(ESP8266)
  // Restore hostname in case configTime() reset it. The corrected hostname
  // will be sent on the next DHCP exchange (renewal or reconnect). No SDK
  // DHCP calls here: any wifi_station_dhcpc_start while connected takes DHCP
  // ownership away from the SDK and breaks setAutoReconnect-driven DHCP on
  // subsequent reassociations. See TASK-432.
  platformSetHostname(CSTR(settings.sHostname));
#endif
  NtpStatus = TIME_WAITFORSYNC;
}


void loopNTP()
{
  time_t now = time(nullptr);
  if (!settings.ntp.bEnable) return;

  // Periodic NTP telemetry (every 60s unsynced, 300s synced) — toggle with key '6'
  if (state.debug.bNTP) {
    static uint32_t lastNtpTelemetry = 0;
    uint32_t interval = (NtpStatus == TIME_SYNC) ? 300000 : 60000;
    if ((millis() - lastNtpTelemetry) > interval) {
      lastNtpTelemetry = millis();
      const char *stateStr = "?";
      switch (NtpStatus) {
        case TIME_NOTSET:       stateStr = "NOTSET"; break;
        case TIME_SYNC:         stateStr = "SYNC"; break;
        case TIME_WAITFORSYNC:  stateStr = "WAITFORSYNC"; break;
        case TIME_NEEDSYNC:     stateStr = "NEEDSYNC"; break;
      }
      DebugTf(PSTR("[NTP] state=%s now=%lu (0x%08lX) NtpLastSync=%lu (0x%08lX) delta=%ld host=[%s] tz=[%s]\r\n"),
              stateStr,
              (unsigned long)now, (unsigned long)now,
              (unsigned long)NtpLastSync, (unsigned long)NtpLastSync,
              (long)(now - NtpLastSync),
              settings.ntp.sHostname,
              CSTR(settings.ntp.sTimezone));
      DebugTf(PSTR("[NTP] now>EPOCH2000=%s now<EPOCH2038=%s now>=LastSync=%s\r\n"),
              (now > EPOCH_2000_01_01) ? "Y" : "N",
              (now < EPOCH_2038_01_19) ? "Y" : "N",
              (now >= NtpLastSync) ? "Y" : "N");
    }
  }

  switch (NtpStatus) {
    case TIME_NOTSET:
    case TIME_NEEDSYNC:
    {
      // Guard: only store a valid timestamp as the sync baseline. If time() still
      // returns the SDK bogus initial value (0xFFFFFFFF = year 2106) or a small
      // pre-epoch value, use 0 instead. This prevents NtpLastSync from being
      // poisoned to 4294967295, which would make the TIME_WAITFORSYNC check
      //   (now >= NtpLastSync)  always fail once real NTP time arrives.
      time_t prevSync = NtpLastSync;
      bool nowValid = (now > EPOCH_2000_01_01) && (now < EPOCH_2038_01_19);
      NtpLastSync = nowValid ? now : 0;
      if (state.debug.bNTP) DebugTf(PSTR("[NTP] NEEDSYNC: now=%lu (0x%08lX) valid=%s prevSync=%lu -> NtpLastSync=%lu\r\n"),
              (unsigned long)now, (unsigned long)now,
              nowValid ? "Y" : "N",
              (unsigned long)prevSync, (unsigned long)NtpLastSync);
      DebugTln(F("Start time syncing"));
      startNTP();
      DebugTf(PSTR("Starting timezone lookup for [%s]\r\n"), CSTR(settings.ntp.sTimezone));
      NtpStatus = TIME_WAITFORSYNC;
      break;
    }
    case TIME_WAITFORSYNC:
    {
      // Guard: ESP8266 SDK initialises time() to 0xFFFFFFFF (year 2106) before
      // SNTP sync. That value passes the lower-bound check alone, so we also
      // require an upper bound to reject the bogus SDK initial value.
      bool aboveEpoch = (now > EPOCH_2000_01_01);
      bool belowMax   = (now < EPOCH_2038_01_19);
      bool aboveSync  = (now >= NtpLastSync);
      if (aboveEpoch && belowMax && aboveSync) {
        NtpLastSync = now;
        if (state.debug.bNTP) DebugTf(PSTR("[NTP] SYNC candidate: now=%lu (0x%08lX) -> NtpLastSync updated\r\n"),
                (unsigned long)now, (unsigned long)now);
        TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
        if (myTz.isError()) {
          DebugTf(PSTR("[NTP] Error: Timezone Invalid/Not Found: [%s]\r\n"), CSTR(settings.ntp.sTimezone));
          strlcpy(settings.ntp.sTimezone, NTP_DEFAULT_TIMEZONE, sizeof(settings.ntp.sTimezone));
          myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
        } else {
          ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
          if (state.debug.bNTP) DebugTf(PSTR("[NTP] ZonedDateTime: %02d:%02d:%02d %02d-%02d-%04d isError=%s\r\n"),
                  myTime.hour(), myTime.minute(), myTime.second(),
                  myTime.day(), myTime.month(), myTime.year(),
                  myTime.isError() ? "Y" : "N");
          if (!myTime.isError()) {
            NtpStatus = TIME_SYNC;
            DebugTln(F("[NTP] Time synced!"));
          } else {
            if (state.debug.bNTP) DebugTln(F("[NTP] ZonedDateTime error, staying in WAITFORSYNC"));
          }
        }
      }
      break;
    }
    case TIME_SYNC:
      if ((now - NtpLastSync) > NTP_RESYNC_TIME) {
        if (state.debug.bNTP) DebugTf(PSTR("[NTP] Resync needed: now=%lu NtpLastSync=%lu delta=%ld > %d\r\n"),
                (unsigned long)now, (unsigned long)NtpLastSync,
                (long)(now - NtpLastSync), NTP_RESYNC_TIME);
        NtpStatus = TIME_NEEDSYNC;
      }
      break;
  }
} // loopNTP()

bool isNTPtimeSet()
{
  return NtpStatus == TIME_SYNC;
}

// ADR-086 (TASK-350): signature takes pre-computed flags from the single
// dispatcher in doTaskMinuteChanged. Internal dayChanged/yearChanged calls
// are removed so each helper has exactly ONE call site firmware-wide.
void sendtimecommand(bool dayFlag, bool yearFlag){
  if (state.otBus.bPSmode) return;                  // when in Print Summary mode (PS=1), no timesync commands (improving legacy/Domoticz compatibility)
  if (!settings.ntp.bEnable) return;        // if NTP is disabled, then return
  if (!settings.ntp.bSendtime) return;      // if NTP send time is disabled, then return
  if (NtpStatus != TIME_SYNC) return;   // only send time command when time is synced
  // TASK-441: widen gate so OTDirect builds also receive time/date sync.
  // OTDirect now parses PIC-style SR=21:m,d and SR=22:hi,lo decimal bytes.
  if (!hasOTCommandInterface()) return;        // PIC OR OTDirect must be ready
  if (!isGatewayFirmware()) return; // only send timecommand when in gateway firmware, not in diagnostic or interface mode

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
  addCommandToQueue(msg, strlen(msg), false, 0);

  if (dayFlag){
    //Send msg id 21: month, day
    snprintf_P(msg, sizeof(msg), PSTR("SR=21:%d,%d"), myTime.month(), myTime.day());
    addCommandToQueue(msg, strlen(msg), true, 0);
  }

  if (yearFlag){
    //Send msg id 22: HB of Year, LB of Year
    snprintf_P(msg, sizeof(msg), PSTR("SR=22:%d,%d"), (myTime.year() >> 8) & 0xFF, myTime.year() & 0xFF);
    addCommandToQueue(msg, strlen(msg), true, 0);
  }
}

//=====[ Helpers ]=============================================================

const char* getMacAddress()
{
  static char baseMacChr[13] = {0};
  uint8_t baseMac[6];
  platformGetMacAddress(baseMac);
  snprintf_P(baseMacChr, sizeof(baseMacChr), PSTR("%02X%02X%02X%02X%02X%02X"),
             baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  return baseMacChr;
}

const char* getUniqueId()
{
  static char uniqueId[32];
  snprintf_P(uniqueId, sizeof(uniqueId), PSTR("otgw-%s"), getMacAddress());
  return uniqueId;
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
