/*
***************************************************************************
**  Program  : networkStuff.ino
**  Version  : v1.4.0-beta
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
static bool sDhcpHostnameFixed = false;  // set once after any DHCP restart to prevent double-announce

// Debug telnet instance (port 23). SimpleTelnet replaces ESPTelnet for debug output.
// Port is fixed in the constructor; begin() needs no port argument.
SimpleTelnet<1> debugTelnet(23);

OTGWWebServer           httpServer(80);
OTGWUpdateServer        httpUpdater(true);

#if defined(ESP8266)
FSInfo LittleFSinfo;
#endif
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
  strlcat(thisAP, WiFi.macAddress().c_str(), sizeof(thisAP));

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
    #if defined(ESP8266)
      String currentHostname = WiFi.hostname();
    #elif defined(ESP32)
      String currentHostname = String(WiFi.getHostname());
    #endif
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
      #if defined(ESP8266)
        WiFi.hostname(hostname);
      #elif defined(ESP32)
        WiFi.setHostname(hostname);
      #endif
      DebugTln(F("Hostname updated; keeping existing WiFi connection active."));
    }
  }
  else if (wifiSaved)
  {
    DebugTln(F("Saved WiFi found, attempting direct connect..."));
    int directConnectTimeout = timeOut / 2;
    if (directConnectTimeout < 5) directConnectTimeout = 5;
    DebugTf(PSTR("Direct connect timeout: %d sec\r\n"), directConnectTimeout);
    #if defined(ESP8266)
      WiFi.hostname(hostname);  // set before begin() so DHCP sends the correct hostname
    #elif defined(ESP32)
      WiFi.setHostname(hostname);
    #endif
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

  // Catch-all: if the hostname still doesn't match after all connection paths,
  // force a DHCP re-announce. Mark it done so startNTP() doesn't do it again.
  #if defined(ESP8266)
    WiFi.hostname(hostname);
    const char *_hn = WiFi.hostname().c_str();
  #elif defined(ESP32)
    WiFi.setHostname(hostname);
    const char *_hn = WiFi.getHostname();
  #endif
  if (!sDhcpHostnameFixed && strcmp(_hn, hostname) != 0) {
    DebugTf(PSTR("Catch-all: hostname mismatch after connect ('%s' vs '%s'), forcing DHCP re-announce.\r\n"),
            _hn, hostname);
    platformRestartDHCP();
    sDhcpHostnameFixed = true;
  }

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
  DECLARE_TIMER_SEC(timerWifiRetry, 30, CATCH_UP_MISSED_TICKS);

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
      WiFi.hostname(CSTR(settings.sHostname));
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
        if (wifiRetryCount >= 10) {
          wifiState = WIFI_FAILED;
        } else {
          wifiState = WIFI_DISCONNECTED;  // retry
        }
      }
      break;

    case WIFI_RECONNECTED:
      // Match the startup path: re-apply the configured hostname and force a
      // DHCP re-announce so the renewed lease uses the expected name.
      WiFi.hostname(CSTR(settings.sHostname));
      DebugTf(PSTR("WiFi: reconnected, re-announcing DHCP lease for hostname [%s]\r\n"),
              CSTR(settings.sHostname));
      wifi_station_dhcpc_stop();
      wifi_station_dhcpc_start();
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

//===========================================================================================
// Send the welcome banner to a freshly-connected telnet client.
// Called from the SimpleTelnet onConnect callback — receives client IP as const char*.
// Uses a series of small PROGMEM prints to stay off the stack.
static void sendTelnetBanner(const char* ip)
{
  debugTelnet.println(F("\r\n============================================"));
  debugTelnet.println(F("  OpenTherm Gateway -- OTGW-firmware"));
  _debugPrintf_P(PSTR("  Version : %s\r\n"), _VERSION);
  debugTelnet.println(F("============================================"));
  _debugPrintf_P(PSTR("  IP      : %s\r\n"), WiFi.localIP().toString().c_str());
  _debugPrintf_P(PSTR("  WiFi    : %s\r\n"), WiFi.SSID().c_str());
  _debugPrintf_P(PSTR("  OTGW    : %-10s  MQTT : %s\r\n"),
    state.otgw.bOnline     ? "online"     : "offline",
    state.mqtt.bConnected  ? "connected"  : "disconnected");
  _debugPrintf_P(PSTR("  Heap    : %u bytes free\r\n"), ESP.getFreeHeap());
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Debug flags (key to toggle):"));
  _debugPrintf_P(PSTR("    1 OT messages : %s\r\n"), CBOOLEAN(state.debug.bOTmsg));
  _debugPrintf_P(PSTR("    2 REST API    : %s\r\n"), CBOOLEAN(state.debug.bRestAPI));
  _debugPrintf_P(PSTR("    3 MQTT comms  : %s\r\n"), CBOOLEAN(state.debug.bMQTT));
  _debugPrintf_P(PSTR("    4 MQTT gating : %s\r\n"), CBOOLEAN(state.debug.bMQTTGate));
  _debugPrintf_P(PSTR("    5 Sensors     : %s\r\n"), CBOOLEAN(state.debug.bSensors));
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Press 'h' for the full debug menu."));
  _debugPrintf_P(PSTR("  Connected from: %s\r\n"), ip);
  debugTelnet.println(F("============================================\r\n"));
}

//===========================================================================================
// Forward declaration — defined in handleDebug.ino.
void handleDebugChar(char c);

// SimpleTelnet input callback: line mode off means one char per keypress (as const char*).
static void onTelnetInput(const char* s) {
  if (s && s[0] != '\0') handleDebugChar(s[0]);
}

void startTelnet()
{
  debugTelnet.onConnect(sendTelnetBanner);
  debugTelnet.setLineMode(false);
  debugTelnet.onInputReceived(onTelnetInput);
  debugTelnet.begin();             // port was fixed in the constructor (23)
  DebugT(F("\r\nTelnet debug server started on "));
  DebugT(WiFi.localIP());
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

  // Set hostname before configTime() — configTime() is known to reset the
  // station hostname to "ESP-XXXXXX" on some ESP8266 SDK versions.
  #if defined(ESP8266)
    WiFi.hostname(CSTR(settings.sHostname));
  #elif defined(ESP32)
    WiFi.setHostname(CSTR(settings.sHostname));
  #endif
  configTime(0, 0, settings.ntp.sHostname, nullptr, nullptr);
  // Capture hostname immediately after configTime() to detect if the SDK
  // reset it, *before* we restore it. This drives the DHCP re-announce
  // decision below.
  #if defined(ESP8266)
    bool hostnameWasReset = (strcmp(WiFi.hostname().c_str(), CSTR(settings.sHostname)) != 0);
    WiFi.hostname(CSTR(settings.sHostname));
  #elif defined(ESP32)
    bool hostnameWasReset = (strcmp(WiFi.getHostname(), CSTR(settings.sHostname)) != 0);
    WiFi.setHostname(CSTR(settings.sHostname));
  #endif

  // If configTime() did reset the hostname, the DHCP lease may have been
  // re-announced with the wrong name.  Force a DHCP re-announce once so the
  // router sees the correct hostname.  Only do this once to avoid dropping
  // the STA lease on every 30-min NTP resync (which would break MQTT/Telnet/
  // WebSocket connections).
  if (!sDhcpHostnameFixed && hostnameWasReset && WiFi.isConnected()) {
    platformRestartDHCP();
    sDhcpHostnameFixed = true;
  }
  NtpStatus = TIME_WAITFORSYNC;
}


void loopNTP()
{
  time_t now = time(nullptr);
  if (!settings.ntp.bEnable) return;
  switch (NtpStatus) {
    case TIME_NOTSET:
    case TIME_NEEDSYNC:
      NtpLastSync = now;
      DebugTln(F("Start time syncing"));
      startNTP();
      DebugTf(PSTR("Starting timezone lookup for [%s]\r\n"), CSTR(settings.ntp.sTimezone));
      NtpStatus = TIME_WAITFORSYNC;
      break;
    case TIME_WAITFORSYNC:
      if ((now > EPOCH_2000_01_01) && (now >= NtpLastSync)) {
        NtpLastSync = now;
        TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
        if (myTz.isError()) {
          DebugTf(PSTR("Error: Timezone Invalid/Not Found: [%s]\r\n"), CSTR(settings.ntp.sTimezone));
          strlcpy(settings.ntp.sTimezone, NTP_DEFAULT_TIMEZONE, sizeof(settings.ntp.sTimezone));
          myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
        } else {
          ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now, myTz);
          DebugTf(PSTR("%02d:%02d:%02d %02d-%02d-%04d\n\r"),
                  myTime.hour(), myTime.minute(), myTime.second(),
                  myTime.day(), myTime.month(), myTime.year());
          if (!myTime.isError()) {
            NtpStatus = TIME_SYNC;
            DebugTln(F("Time synced!"));
          }
        }
      }
      break;
    case TIME_SYNC:
      if ((now - NtpLastSync) > NTP_RESYNC_TIME) {
        DebugTln(F("Time resync needed"));
        NtpStatus = TIME_NEEDSYNC;
      }
      break;
  }
} // loopNTP()

bool isNTPtimeSet()
{
  return NtpStatus == TIME_SYNC;
}

//=====[ Helpers ]=============================================================

const char* getMacAddress()
{
  static char baseMacChr[13] = {0};
  uint8_t baseMac[6];
#if defined(ESP8266)
  WiFi.macAddress(baseMac);
  snprintf_P(baseMacChr, sizeof(baseMacChr), PSTR("%02X%02X%02X%02X%02X%02X"),
             baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
#elif defined(ESP32)
  esp_efuse_mac_get_default(baseMac);
  snprintf_P(baseMacChr, sizeof(baseMacChr), PSTR("%02X%02X%02X%02X%02X%02X"),
             baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
#else
  snprintf_P(baseMacChr, sizeof(baseMacChr), PSTR("%02X%02X%02X%02X%02X%02X"),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
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
