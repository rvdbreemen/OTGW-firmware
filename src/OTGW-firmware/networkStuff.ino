/*
***************************************************************************
**  Program  : networkStuff.ino
**  Version  : v1.3.6-beta
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

ESP8266WebServer        httpServer(80);
ESP8266HTTPUpdateServer httpUpdater(true);

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
    String currentHostname = WiFi.hostname();
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
      WiFi.hostname(hostname);
      DebugTln(F("Hostname updated; keeping existing WiFi connection active."));
    }
  }
  else if (wifiSaved)
  {
    DebugTln(F("Saved WiFi found, attempting direct connect..."));
    int directConnectTimeout = timeOut / 2;
    if (directConnectTimeout < 5) directConnectTimeout = 5;
    DebugTf(PSTR("Direct connect timeout: %d sec\r\n"), directConnectTimeout);
    WiFi.hostname(hostname);  // set before begin() so DHCP sends the correct hostname
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
      ESP.restart();
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

  // Ensure the hostname is set for the next DHCP exchange (renewal or reconnect).
  // loopWifi() WIFI_DISCONNECTED calls wifi_station_dhcpc_start() before WiFi.begin()
  // in the correct (not-connected) state, so no SDK DHCP call is needed here.
  WiFi.hostname(hostname);

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
      // Explicitly (re-)enable DHCP before reconnecting.  WiFi.begin() with no
      // arguments only calls wifi_station_connect() — it does NOT call
      // wifi_station_dhcpc_start().  This call ensures a fresh DHCP DISCOVER
      // is sent on every reconnection, which is required after a router reboot
      // so the device does not try to renew a stale lease that the router no
      // longer has.  See issue #525 and ADR-047.
      wifi_station_dhcpc_start();
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
      // The hostname was already set in WIFI_DISCONNECTED before WiFi.begin(),
      // and wifi_station_dhcpc_start() was called there too — so the DHCP
      // negotiation already used the correct hostname.
      // Do NOT call wifi_station_dhcpc_stop/start here: the SDK requires
      // dhcpc_start only when the station is NOT connected, and calling it
      // while connected temporarily resets the IP to 0.0.0.0.  That causes
      // WIFI_IDLE to mistake the in-progress DHCP renewal for a disconnect,
      // which triggers a new WiFi.begin() cycle that never re-enables DHCP,
      // leaving the device associated at the WiFi layer but with no IP address.
      WiFi.hostname(CSTR(settings.sHostname));
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
void startTelnet()
{
  DebugT(F("\r\nUse  'telnet "));
  DebugT(WiFi.localIP());
  DebugTln(F("' for debugging"));
  TelnetStream.begin();
  DebugTln(F("\nTelnet server started .."));
  TelnetStream.flush();
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
  DebugTf(PSTR("LLMNR setup as [%s]\r\n"), hostname);
  if (LLMNR.begin(hostname))               // Start the LLMNR responder for hostname
  {
    DebugTf(PSTR("LLMNR responder started as [%s]\r\n"), hostname);
  }
  else
  {
    DebugTln(F("Error setting up LLMNR responder!\r\n"));
  }
} // startLLMNR()

//=====[ NTP ]=================================================================

void startNTP()
{
  if (!settings.ntp.bEnable) return;
  if (strlen(settings.ntp.sTimezone) == 0) strlcpy(settings.ntp.sTimezone, NTP_DEFAULT_TIMEZONE, sizeof(settings.ntp.sTimezone));
  if (strlen(settings.ntp.sHostname) == 0) strlcpy(settings.ntp.sHostname, NTP_HOST_DEFAULT, sizeof(settings.ntp.sHostname));

  // Set hostname before configTime() — configTime() is known to reset the
  // station hostname to "ESP-XXXXXX" on some ESP8266 SDK versions.
  WiFi.hostname(CSTR(settings.sHostname));
  configTime(0, 0, settings.ntp.sHostname, nullptr, nullptr);
  // Restore hostname in case configTime() reset it.  The correct hostname will
  // be sent to the router on the next DHCP exchange (renewal or reconnect).
  // Do NOT call wifi_station_dhcpc_stop/start here: calling dhcpc_start() while
  // connected resets the IP to 0.0.0.0 — see issue #525 and analysis report in
  // docs/reviews/2026-04-07_issue-525-sdk-dhcp-analysis/ANALYSIS_REPORT.md
  WiFi.hostname(CSTR(settings.sHostname));
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
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
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
