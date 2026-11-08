/*
***************************************************************************
**  Program  : networkStuff.ino
**  Version  : v2.0.0-beta.150
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
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
AsyncSimpleTelnet<1> debugTelnet(23);

// TASK-865.9 (ADR-123 Phase 3): the synchronous WebServer moved to
// ESPAsyncWebServer. The async server runs every handler on the AsyncTCP
// service task (no loop() handleClient() polling). seq10 (WebSocket) and
// seq11 (OTA) attach to this same instance.
AsyncWebServer          server(WEB_HTTP_PORT);
OTGWUpdateServer        httpUpdater(true);

// Per-request context for the async bridge (webServerCompat.h). Single point
// of definition (ADR-044). Safe as file-static because ESPAsyncWebServer
// serializes all handlers on the one async_tcp task.
AsyncWebServerRequest*  currentRequest  = nullptr;
AsyncResponseStream*    g_restStream    = nullptr;
bool                    g_responseSent  = false;
bool                    g_restSlotHeld  = false;
WebPendingHeaders       g_pendingHeaders{};
WebRequestBody          g_requestBody{};

FSInfo LittleFSinfo;
bool   LittleFSmounted = false;

//=====[ WiFi ]================================================================

// TASK-1446 record begin
// WiFi station link record (TASK-1446 telnet banner, TASK-1444 device/info; GH #682;
// port of the 1.x outage record). The driver raises STA_CONNECTED/STA_DISCONNECTED on
// the Arduino network event task, concurrently with the loop task (log drain) and the
// async_tcp task (telnet banner, device/info). The callbacks only write the record
// under g_wifiLinkMux: no logging, no allocation, no String. wifiLinkEventsLog() logs
// from loopWifi(); readers take a copy under the lock and format outside it.
//
// One outage runs from the first disconnect event after a connect until the next
// connect; the driver repeats the event (e.g. 201 NO_AP_FOUND) while it retries, so
// those count as events of the same outage. A disconnect that the firmware causes
// itself (Ethernet takeover, BETA AP retry timeout, dead-link forced reconnect) is
// marked through wifiLinkMarkSelf() right before the call: the drop is recorded with
// its cause but not counted in iOutages. The mark only applies to a drop that starts
// while the link is up, and every disconnect or connect event clears it, so it can
// never land on a later field outage.
static portMUX_TYPE   g_wifiLinkMux            = portMUX_INITIALIZER_UNLOCKED;
static WifiLinkRecord g_wifiLink;                               // guarded by g_wifiLinkMux
static WifiSelfCause  g_wifiSelfPending        = WIFI_SELF_NONE;  // guarded
static bool           g_wifiDropLogPending     = false;           // guarded
static bool           g_wifiJoinLogPending     = false;           // guarded
static bool           g_wifiLinkEventsBegun    = false;           // loop task only

void wifiFormatBssid(char *buf, size_t size, const uint8_t *b)
{
  snprintf_P(buf, size, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), b[0], b[1], b[2], b[3], b[4], b[5]);
}

// arduino-esp32 3.3.5 names the wifi_err_reason_t codes itself (STA.cpp); it returns ""
// for a code it does not know (e.g. 209..212).
const char *wifiReasonName(uint8_t reason)
{
  const char *name = WiFi.disconnectReasonName((wifi_err_reason_t)reason);
  return (name && name[0]) ? name : "?";
}

static const char *wifiSelfCauseName(WifiSelfCause cause)
{
  switch (cause) {
    case WIFI_SELF_ETHERNET:  return "Ethernet takeover";
    case WIFI_SELF_AP_RETRY:  return "AP retry timeout";
    case WIFI_SELF_DEAD_LINK: return "dead-link check";
    case WIFI_SELF_NONE:      break;
  }
  return "";
}

static void wifiLinkOnConnected(const uint8_t *bssid, uint8_t channel, uint32_t now)
{
  portENTER_CRITICAL(&g_wifiLinkMux);
  memcpy(g_wifiLink.joinedBssid, bssid, sizeof(g_wifiLink.joinedBssid));
  g_wifiLink.iJoinedChannel = channel;
  g_wifiLink.bJoinSeen      = true;
  if (!g_wifiLink.bLinkUp && g_wifiLink.bDropValid) g_wifiLink.iDropDurationMs = now - g_wifiLink.iDropStartMs;
  g_wifiLink.bLinkUp        = true;
  g_wifiSelfPending         = WIFI_SELF_NONE;
  g_wifiJoinLogPending      = true;
  portEXIT_CRITICAL(&g_wifiLinkMux);
}

static void wifiLinkOnDisconnected(const uint8_t *bssid, uint8_t reason, uint32_t now)
{
  portENTER_CRITICAL(&g_wifiLinkMux);
  const WifiSelfCause self = g_wifiSelfPending;
  g_wifiSelfPending = WIFI_SELF_NONE;
  g_wifiLink.iDisconnectEvents++;
  if (g_wifiLink.bLinkUp) {   // first event after a connect: a new drop starts
    g_wifiLink.bLinkUp          = false;
    if (self == WIFI_SELF_NONE) g_wifiLink.iOutages++;
    g_wifiLink.bDropValid       = true;
    g_wifiLink.iDropFirstReason = reason;
    memcpy(g_wifiLink.dropBssid, bssid, sizeof(g_wifiLink.dropBssid));
    g_wifiLink.iDropChannel     = g_wifiLink.iJoinedChannel;
    g_wifiLink.iDropEvents      = 0;
    g_wifiLink.iDropStartMs     = now;
    g_wifiLink.iDropDurationMs  = 0;
    g_wifiLink.eDropSelf        = self;
    g_wifiDropLogPending        = true;
  }
  if (g_wifiLink.bDropValid) {
    g_wifiLink.iDropEvents++;
    g_wifiLink.iDropLastReason = reason;
  }
  portEXIT_CRITICAL(&g_wifiLinkMux);
}

// Network event task. Plain function pointer (WiFiEventSysCb): no std::function.
static void onWifiLinkEvent(arduino_event_t *ev)
{
  if (!ev) return;
  const uint32_t now = millis();
  if (ev->event_id == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    wifiLinkOnConnected(ev->event_info.wifi_sta_connected.bssid, ev->event_info.wifi_sta_connected.channel, now);
  } else if (ev->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiLinkOnDisconnected(ev->event_info.wifi_sta_disconnected.bssid, ev->event_info.wifi_sta_disconnected.reason, now);
  }
}

// Called from startWiFi() right after WiFi.mode(WIFI_STA); registers once.
static void wifiLinkEventsBegin()
{
  if (g_wifiLinkEventsBegun) return;
  g_wifiLinkEventsBegun = true;
  WiFi.onEvent(onWifiLinkEvent, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWifiLinkEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

void wifiLinkMarkSelf(WifiSelfCause cause)
{
  portENTER_CRITICAL(&g_wifiLinkMux);
  if (g_wifiLink.bLinkUp) g_wifiSelfPending = cause;   // a running outage is never relabelled
  portEXIT_CRITICAL(&g_wifiLinkMux);
}

void wifiLinkSnapshot(WifiLinkRecord *out)
{
  if (!out) return;
  portENTER_CRITICAL(&g_wifiLinkMux);
  *out = g_wifiLink;
  portEXIT_CRITICAL(&g_wifiLinkMux);
}

// Called from loopWifi() on every pass, before the Ethernet early return, so a drop
// recorded before or during an Ethernet takeover is still logged.
static void wifiLinkEventsLog()
{
  WifiLinkRecord r;
  portENTER_CRITICAL(&g_wifiLinkMux);
  r = g_wifiLink;
  const bool dropLog = g_wifiDropLogPending;
  const bool joinLog = g_wifiJoinLogPending;
  g_wifiDropLogPending = false;
  g_wifiJoinLogPending = false;
  portEXIT_CRITICAL(&g_wifiLinkMux);

  char bssid[18];
  if (dropLog) {
    wifiFormatBssid(bssid, sizeof(bssid), r.dropBssid);
    DebugTf(PSTR("WiFi: link lost, reason %u %s, AP %s ch %u%s%s%s\r\n"),
            (unsigned)r.iDropFirstReason, wifiReasonName(r.iDropFirstReason), bssid, (unsigned)r.iDropChannel,
            r.eDropSelf != WIFI_SELF_NONE ? " (self: " : "", wifiSelfCauseName(r.eDropSelf),
            r.eDropSelf != WIFI_SELF_NONE ? ")" : "");
  }
  if (joinLog) {
    wifiFormatBssid(bssid, sizeof(bssid), r.joinedBssid);
    if (!r.bDropValid) {
      DebugTf(PSTR("WiFi: joined AP %s ch %u\r\n"), bssid, (unsigned)r.iJoinedChannel);
    } else {
      DebugTf(PSTR("WiFi: joined AP %s ch %u after %lu s, %lu disconnect event(s), last reason %u %s\r\n"),
              bssid, (unsigned)r.iJoinedChannel, (unsigned long)(r.iDropDurationMs / 1000),
              (unsigned long)r.iDropEvents, (unsigned)r.iDropLastReason, wifiReasonName(r.iDropLastReason));
    }
  }
  // The boot association can complete before wifiLinkEventsBegin() registers the
  // callbacks; take the joined AP from the driver then.
  if (!r.bJoinSeen && WiFi.status() == WL_CONNECTED) {
    uint8_t b[6];
    const bool haveBssid = (WiFi.BSSID(b) != nullptr);
    const uint8_t ch = (uint8_t)WiFi.channel();
    portENTER_CRITICAL(&g_wifiLinkMux);
    if (!g_wifiLink.bJoinSeen) {
      if (haveBssid) memcpy(g_wifiLink.joinedBssid, b, sizeof(g_wifiLink.joinedBssid));
      g_wifiLink.iJoinedChannel = ch;
      g_wifiLink.bJoinSeen      = true;
      g_wifiLink.bLinkUp        = true;
    }
    portEXIT_CRITICAL(&g_wifiLinkMux);
  }
}

// Two telnet banner lines, each its own _debugPrintf_P (256-byte buffer).
static void printWifiLinkBanner()
{
  WifiLinkRecord r;
  wifiLinkSnapshot(&r);
  const uint32_t now = millis();
  char bssid[18];
  wifiFormatBssid(bssid, sizeof(bssid), r.joinedBssid);
  _debugPrintf_P(PSTR("  Link : AP %s ch %u%s   outages %lu   disconnect events %lu\r\n"),
    bssid, (unsigned)r.iJoinedChannel, r.bLinkUp ? "" : " (down)",
    (unsigned long)r.iOutages, (unsigned long)r.iDisconnectEvents);
  if (!r.bDropValid) {
    debugTelnet.println(F("  Drop : none since boot"));
    return;
  }
  char down[16];
  if (r.bLinkUp) snprintf_P(down, sizeof(down), PSTR("%lu s"), (unsigned long)(r.iDropDurationMs / 1000));
  else           strlcpy(down, "ongoing", sizeof(down));
  wifiFormatBssid(bssid, sizeof(bssid), r.dropBssid);
  _debugPrintf_P(PSTR("  Drop : %lu s ago, down %s, reason %u %s (last %u %s, %lu events), AP %s ch %u%s%s\r\n"),
    (unsigned long)((now - r.iDropStartMs) / 1000), down,
    (unsigned)r.iDropFirstReason, wifiReasonName(r.iDropFirstReason),
    (unsigned)r.iDropLastReason, wifiReasonName(r.iDropLastReason),
    (unsigned long)r.iDropEvents, bssid, (unsigned)r.iDropChannel,
    r.eDropSelf != WIFI_SELF_NONE ? ", self: " : "", wifiSelfCauseName(r.eDropSelf));
}
// TASK-1446 record end

//gets called when WiFiManager enters configuration mode
static void configModeCallback(WiFiManager *myWiFiManager)
{
  DebugTln(F("\nEntered config mode"));
  DebugTf(PSTR("SSID: %s\r\n"), myWiFiManager->getConfigPortalSSID().c_str());
  DebugTf(PSTR("IP address: %s\r\n"), WiFi.softAPIP().toString().c_str());
  DebugTln(F("Entered config mode\r"));
  DebugTln(WiFi.softAPIP().toString());
  DebugTln(myWiFiManager->getConfigPortalSSID());
  // [TASK-750] Paint the "how to connect" screen on the OLED while the (blocking)
  // WiFiManager config portal is open. The portal AP is open, so no password.
  // oledShowConfigMode is a no-op when no OLED was detected at runtime.
  char oledUrl[28];
  snprintf_P(oledUrl, sizeof(oledUrl), PSTR("http://%s/"), WiFi.softAPIP().toString().c_str());
  oledShowConfigMode(myWiFiManager->getConfigPortalSSID().c_str(), nullptr, oledUrl);
} // configModeCallback()

void resetWiFiSettings(void)
{
  // Is it safe to re-setup this object? other one only lives in startWiFi.
  WiFiManager manageWiFi;
  manageWiFi.resetSettings();
}

// TASK-1455: skip-portal mark, defined with the WiFi state machine below.
static bool wifiSkipPortalDecide(bool wifiConnected, bool wifiSaved, bool forcePortal);

//===========================================================================================
void startWiFi(const char* hostname, int timeOut, bool forcePortal)
{
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  wifiLinkEventsBegin();   // TASK-1446: WiFi link record (reason, AP, outages)

  // Keep DHCP-supplied NTP servers (option 42) out of SNTP: we run our own NTP
  // in startNTP(). lwIP already rejects them by default on this framework; the
  // call pins that default (TASK-1052). It sits under WiFi.mode() because that
  // call brings the TCP/IP stack up. See platformIgnoreDhcpNtp().
  platformIgnoreDhcpNtp();

  // TASK-1397: scan all channels and join the strongest AP of the SSID. Set here,
  // before the WiFiManager portal can call WiFi.begin(ssid, ...) itself.
  platformWifiPreferStrongestAp();

  // TASK-725 (port of TASK-548): apply static IP before any connect attempt.
  // Empty sStaticIp = DHCP (default) → leave OS DHCP mode unchanged.
  if (settings.wifi.sStaticIp[0] != '\0') {
    IPAddress ip, gw, sn, dns1, dns2;
    if (ip.fromString(settings.wifi.sStaticIp) &&
        sn.fromString(settings.wifi.sSubnet)   &&
        gw.fromString(settings.wifi.sGateway)) {
      if (settings.wifi.sDns1[0] != '\0') dns1.fromString(settings.wifi.sDns1);
      if (settings.wifi.sDns2[0] != '\0') dns2.fromString(settings.wifi.sDns2);
      WiFi.config(ip, gw, sn, dns1, dns2);
      DebugTf(PSTR("WiFi static IP: %s / %s gw %s\r\n"),
              settings.wifi.sStaticIp, settings.wifi.sSubnet, settings.wifi.sGateway);
    } else {
      DebugTln(F("WiFi static IP: parse error — falling back to DHCP"));
    }
  }

  WiFiManager manageWiFi;
  uint32_t lTime = millis();
  char thisAP[64];
  strlcpy(thisAP, hostname, sizeof(thisAP));
  strlcat(thisAP, "-", sizeof(thisAP));
  // Full 6-byte MAC with colons to match the 1.x.x captive-portal naming scheme
  // (e.g. "OTGW-A4:CF:12:21:B4:F8"). Sourced from the eFuse MAC, which is always
  // valid — WiFi.macAddress() returns 00:00:00:00:00:00 before WiFi.begin() on ESP32.
  uint8_t apMac[6];
  platformGetMacAddress(apMac);
  char apMacStr[18];
  snprintf_P(apMacStr, sizeof(apMacStr), PSTR("%02X:%02X:%02X:%02X:%02X:%02X"),
             apMac[0], apMac[1], apMac[2], apMac[3], apMac[4], apMac[5]);
  strlcat(thisAP, apMacStr, sizeof(thisAP));

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
    // ADR-043 / TASK-760: the triple-reset is the deliberate credential-recovery
    // gesture, so actually CLEAR the stored WiFi credentials here — not just force
    // the portal. Without this, old credentials survive a config-portal timeout,
    // contradicting ADR-043 ("clear saved WiFi credentials, then force the portal").
    manageWiFi.resetSettings();
    wifiConnected = false;
    wifiSaved     = false;   // credentials are gone now; keep downstream logic consistent
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
    platformWifiPreferStrongestAp();
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

  // TASK-1455 (ADR-211): the boot after a WiFi supervisor restart opens neither the config
  // portal nor the prerelease AP fallback while the saved network is away. setup() goes on
  // without WiFi and loopWifi() retries the saved network every 30 s.
  const bool portalSkipped = wifiSkipPortalDecide(wifiConnected, wifiSaved, forcePortal);
  if (portalSkipped)
  {
    if (settings.wifi.bNoPortal)   // TASK-1645
      DebugTln(F("WiFi: saved network not reachable, skipping the config portal (setting wifinoportal); loopWifi keeps retrying. Triple reset to reconfigure WiFi"));
    else
      DebugTln(F("WiFi: not connected after a WiFi supervisor restart, skipping the config portal; loopWifi keeps retrying"));
  }
  else if (!wifiConnected)
  {
#if defined(_VERSION_PRERELEASE)
    if (wifiSaved && !forcePortal) {
      // BETA: credentials stored but WiFi unreachable at boot — enter AP fallback
      // instead of blocking in the config portal.
      DebugTln(F("BETA: WiFi unavailable at boot, skipping config portal → AP fallback"));
      startAPFallback();
      // TASK-865.11: the OTA /update route is registered once on the
      // AsyncWebServer in setupFSexplorer() (idempotent, guarded by
      // _routesRegistered). Here we only keep the HTTP Basic Auth credentials
      // current so a password change applies without re-registering the route.
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
      storeRebootReasonUnlessPending("WiFi: config portal timeout");   // TASK-1447
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
  platformWifiDisableSleep();  // keep radio always-on for a responsive web UI (TASK-817)

  DebugTf(PSTR("Wifi status: %s\r\n"), WiFi.status() == WL_CONNECTED ? "Connected" : "Not connected");
  if (WiFi.status() != WL_CONNECTED && !portalSkipped)   // a skipped boot goes on without WiFi
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

  // TASK-865.11: the OTA /update route is registered once on the AsyncWebServer
  // in setupFSexplorer() (idempotent, guarded by _routesRegistered). Here we only
  // keep the HTTP Basic Auth credentials current so a password change applies
  // without re-registering the route.
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
  // Passphrase MUST be >= 8 chars: esp_wifi rejects a shorter WPA2 key with
  // "passphrase too short!" and the fallback AP never starts, stranding a device
  // that has lost STA (no STA, no AP). "otgw123" was 7 chars -> too short (TASK-940).
  WiFi.softAP(apSSID, "otgw1234");

  DebugTf(PSTR("BETA AP: fallback started SSID=[%s] IP=192.168.4.1 pass=otgw1234\r\n"), apSSID);
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

//===========================================================================================
// TASK-1455 begin (1.x skip-portal port, ADR-211): a WiFi supervisor restart marks the next
// boot. On that boot startWiFi() opens neither the WiFiManager config portal nor the
// prerelease AP fallback when the saved network is still away after the direct-connect
// window: setup() finishes without WiFi and loopWifi() keeps retrying. WiFiManager 2.0.17
// switches the station off while the portal runs, so a gateway in the portal could not
// rejoin an access point that came back.
// The mark lives in RTC_NOINIT memory: it survives esp_restart() and is garbage after a
// power-on, so it needs the magic word and a software reset reason. setup() clears it, so it
// applies to one boot. No NVS or flash write. A power cycle, a boot without saved
// credentials, the triple reset and every other restart still get the portal.
#define WIFI_SKIP_PORTAL_MAGIC  0x4F545350UL   // "OTSP"

struct WifiSkipPortalRtc {
  uint32_t magic;
  uint32_t marked;   // 1: the previous run ended in a WiFi supervisor restart
};
static RTC_NOINIT_ATTR WifiSkipPortalRtc g_rtcWifiSkipPortal;
static bool wifiSkipPortalBoot = false;   // this boot follows a WiFi supervisor restart

// Called right before a WiFi supervisor doRestart(). Not while Ethernet is the active link:
// loopWifi() returns before both restart sites then, and this keeps it that way.
static void wifiMarkSkipPortal()
{
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) return;
#endif
  g_rtcWifiSkipPortal.magic  = WIFI_SKIP_PORTAL_MAGIC;
  g_rtcWifiSkipPortal.marked = 1;
}

// Called once near the top of setup(). Always clears the record. It counts only after a
// software restart: a panic or watchdog reset in doRestart()'s shutdown, or a power-on with
// garbage that happens to match, gets the portal as before.
void captureWifiSkipPortal()
{
  wifiSkipPortalBoot = platformResetWasSoftware()
                       && g_rtcWifiSkipPortal.magic == WIFI_SKIP_PORTAL_MAGIC
                       && g_rtcWifiSkipPortal.marked == 1;
  g_rtcWifiSkipPortal.magic  = 0;
  g_rtcWifiSkipPortal.marked = 0;
}

// startWiFi(): true when this boot must not open the portal (or the AP fallback). Takes the
// flag, so a later startWiFi() in the same boot (telnet debug menu) behaves as before.
static bool wifiSkipPortalDecide(bool wifiConnected, bool wifiSaved, bool forcePortal)
{
  const bool marked = wifiSkipPortalBoot;
  wifiSkipPortalBoot = false;
  // TASK-1645: with settings.wifi.bNoPortal every startWiFi() skips too, not only the one boot after a
  // supervisor restart. No saved credentials and the triple reset (forcePortal) still open the portal.
  return (marked || settings.wifi.bNoPortal) && !wifiConnected && wifiSaved && !forcePortal;
}
// TASK-1455 end

//===========================================================================================
// TASK-1450 begin (port of 1.x TASK-1248, GH #682): detect a WiFi link that stays
// associated with an IP while no traffic passes, and recover from it. loopWifi() only acts
// on WiFi.status() != WL_CONNECTED, and WL_CONNECTED means "associated and got an IP", so
// such a link was never acted on.
//
// Detection: while the link is up and loopWifi() is idle, the default gateway gets
// WIFI_PROBE_PINGS ICMP echo requests every WIFI_PROBE_INTERVAL_SEC, WIFI_PROBE_SPACING_MS
// apart, sent from the loop task through the platformGatewayProbe*() shims (one raw ICMP
// pcb that exists only during the probe, every lwIP call under the core lock). A probe is
// judged WIFI_PROBE_SPACING_MS after its last echo: any reply means the gateway answered.
// - Armed per gateway: a gateway counts only after it has answered once. A router that
//   drops ICMP never arms the check, a network that is dead from boot cannot trigger it,
//   and a new gateway address disarms it again.
// - Witnesses: a silent gateway alone does not prove a dead link. While MQTT is connected
//   (espMqttClient drops a connection after 2 keepalives without server traffic,
//   MqttClient.cpp:469, keepalive 60 s), a REST request arrived, or a port 25238 client
//   sent a byte within WIFI_WITNESS_WINDOW_MS, no action is taken. A router reboot
//   therefore leaves a gateway with any LAN client alone.
// - No probe while the heap is not HEALTHY, and a probe whose pcb or echo buffers could
//   not be allocated does not count (a TASK-1162-style overload is not a dead link). A
//   probe that took far longer than planned (the loop stalled) is discarded too.
//
// Escalation after WIFI_PROBE_FAILS_TO_ACT dead probes in a row:
// 1. a forced reconnect that keeps the stored credentials; loopWifi() then reconnects as
//    for any other drop. The DHCP lease is released by it, which is why the witnesses
//    come first. A prerelease build that cannot get the link back goes to its AP fallback;
// 2. if the link is still dead after that reconnect, doRestart(). A forced reconnect that
//    ends in WIFI_FAILED (release builds) restarts there and counts toward the same cap.
//
// Restart cap: a link that works for a few minutes after every boot and then dies would
// otherwise restart the gateway endlessly. Dead-link restarts are counted in RTC_NOINIT
// memory, kept only over a software restart (platformResetWasSoftware()). At
// WIFI_DEAD_RESTART_CAP the gateway stops restarting and forces at most one reconnect per
// WIFI_CAPPED_RECONNECT_MIN. The count clears after the link has stayed alive for
// WIFI_DEAD_RESTART_CLEAR_MIN, and on a power-on, panic or watchdog reset.
#define WIFI_PROBE_INTERVAL_SEC       60   // one probe per minute while connected
#define WIFI_PROBE_PINGS               3   // echo requests per probe
#define WIFI_PROBE_SPACING_MS       2000   // between echoes, and the wait after the last one
#define WIFI_PROBE_MAX_SPAN_MS     15000   // a probe open longer than this (normal: 6 s) is discarded
#define WIFI_PROBE_FAILS_TO_ACT        8   // dead probes in a row before acting: a typical router
                                           // reboot (a few minutes) passes unharmed
#define WIFI_WITNESS_WINDOW_MS     60000   // REST or port 25238 traffic this recent proves the LAN works
#define WIFI_DEAD_RESTART_CAP          3
#define WIFI_DEAD_RESTART_CLEAR_MIN   60
#define WIFI_CAPPED_RECONNECT_MIN     15
#define WIFI_DEAD_RESTART_MAGIC  0x4F544452UL   // "OTDR"
#define WIFI_IP4_ARGS(a) (unsigned)((a) & 0xFF), (unsigned)(((a) >> 8) & 0xFF), \
                         (unsigned)(((a) >> 16) & 0xFF), (unsigned)(((a) >> 24) & 0xFF)

struct WifiDeadRestartRtc {
  uint32_t magic;
  uint32_t count;
};
static RTC_NOINIT_ATTR WifiDeadRestartRtc g_rtcWifiDeadRestarts;

static bool     wifiProbeScheduled      = false;  // wifiProbeNextMs is valid
static uint32_t wifiProbeNextMs         = 0;
static bool     wifiProbeOpen           = false;  // a probe runs (shim pcb exists)
static uint32_t wifiProbeStartMs        = 0;
static uint32_t wifiProbeTarget         = 0;      // gateway of the running probe
static uint16_t wifiEchoSeqBase         = 0;
static uint8_t  wifiEchoSent            = 0;
static bool     wifiEchoAllocFailed     = false;
static uint32_t wifiEchoNextMs          = 0;
static bool     wifiProbeSkipLogged     = false;  // "probe skipped" logged since the last judged probe
static uint32_t wifiArmedGateway        = 0;      // gateway that answered; 0 = not armed
static uint8_t  wifiDeadProbes          = 0;
static bool     wifiDeadReconnectDone   = false;  // a forced reconnect ran in this dead streak
static bool     wifiWitnessLogged       = false;  // "traffic flows, no action" logged; quiet until an answer
static uint8_t  wifiDeadRestarts        = 0;      // dead-link restarts, mirrored in RTC
static bool     wifiDeadRestartsLogged  = false;
static uint32_t wifiLinkAliveSinceMs    = 0;
static uint32_t wifiCappedReconnectMs   = 0;
static bool     wifiCappedReconnectDone = false;

static void writeWifiDeadRestarts(uint32_t count)
{
  g_rtcWifiDeadRestarts.magic = WIFI_DEAD_RESTART_MAGIC;
  g_rtcWifiDeadRestarts.count = count;
}

// Called once near the top of setup(), before anything can restart. A software restart
// keeps the count; any other reset reason starts from zero (RTC_NOINIT is garbage after
// power-on, so the magic word is mandatory).
void captureWifiDeadRestarts()
{
  if (platformResetWasSoftware() && g_rtcWifiDeadRestarts.magic == WIFI_DEAD_RESTART_MAGIC
      && g_rtcWifiDeadRestarts.count <= WIFI_DEAD_RESTART_CAP) {
    wifiDeadRestarts = (uint8_t)g_rtcWifiDeadRestarts.count;
  } else {
    wifiDeadRestarts = 0;
    writeWifiDeadRestarts(0);
  }
}

// Proof that the LAN still carries traffic while the gateway is silent.
static bool wifiLanTrafficSeen()
{
  if (settings.mqtt.bEnable && state.mqtt.bConnected) return true;
  if (restRequestWithin(WIFI_WITNESS_WINDOW_MS)) return true;
  return otgwStreamRxWithin(WIFI_WITNESS_WINDOW_MS);
}

static void wifiProbeSkipped(PGM_P why)
{
  if (wifiProbeSkipLogged) return;
  wifiProbeSkipLogged = true;
  DebugTf(PSTR("WiFi: gateway probe skipped (%s), not counted\r\n"), why);
}

// Ends a running probe without a verdict (link dropped, transport switch, stalled probe).
static void wifiLinkProbeAbort()
{
  if (!wifiProbeOpen) return;
  (void)platformGatewayProbeEnd();
  wifiProbeOpen = false;
}

// Back to the boot state of the check (WiFi -> Ethernet). The restart count stays.
// Cheap when there is nothing to reset: loopWifi() calls it on every pass while wired.
static void wifiLinkProbeReset()
{
  if (!wifiProbeOpen && !wifiProbeScheduled && wifiArmedGateway == 0 && wifiDeadProbes == 0
      && !wifiDeadReconnectDone) return;
  wifiLinkProbeAbort();
  if (wifiArmedGateway != 0) DebugTln(F("WiFi: wired link active, dead-link check disarmed"));
  wifiProbeScheduled    = false;
  wifiArmedGateway      = 0;
  wifiDeadProbes        = 0;
  wifiDeadReconnectDone = false;
  wifiWitnessLogged     = false;
}

static void wifiDeadLinkReconnect()
{
  wifiDeadReconnectDone = true;
  wifiLinkMarkSelf(WIFI_SELF_DEAD_LINK);   // TASK-1446: not a field outage
  platformWifiForceReconnect();   // loopWifi() sees the drop and reconnects
}

static void handleWifiProbeVerdict(bool gatewayAnswered)
{
  const uint32_t now = millis();
  if (gatewayAnswered) {
    if (wifiArmedGateway != wifiProbeTarget) {
      wifiArmedGateway = wifiProbeTarget;
      wifiLinkAliveSinceMs = now;
      DebugTf(PSTR("WiFi: gateway %u.%u.%u.%u answers ping, dead-link check armed\r\n"),
              WIFI_IP4_ARGS(wifiArmedGateway));
    }
    if (wifiDeadProbes > 0 || wifiDeadReconnectDone || wifiWitnessLogged) {
      DebugTln(F("WiFi: gateway answers again, dead-link check reset"));
      wifiLinkAliveSinceMs = now;
    }
    wifiDeadProbes        = 0;
    wifiDeadReconnectDone = false;
    wifiWitnessLogged     = false;
    if (wifiDeadRestarts > 0
        && (uint32_t)(now - wifiLinkAliveSinceMs) >= WIFI_DEAD_RESTART_CLEAR_MIN * 60000UL) {
      DebugTln(F("WiFi: link alive for an hour, dead-link restart count cleared"));
      wifiDeadRestarts        = 0;
      wifiCappedReconnectDone = false;
      writeWifiDeadRestarts(0);
    }
    return;
  }
  if (wifiArmedGateway == 0) return;   // no gateway answered yet: check stays off

  if (wifiDeadProbes < 255) wifiDeadProbes++;
  if (wifiDeadProbes == 1 && !wifiWitnessLogged) {
    DebugTf(PSTR("WiFi: gateway %u.%u.%u.%u stopped answering ping (acting after %u probes)\r\n"),
            WIFI_IP4_ARGS(wifiArmedGateway), (unsigned)WIFI_PROBE_FAILS_TO_ACT);
  }
  if (wifiDeadProbes < WIFI_PROBE_FAILS_TO_ACT) return;
  wifiDeadProbes = 0;

  if (wifiLanTrafficSeen()) {
    if (!wifiWitnessLogged) {   // once, until the gateway answers again
      wifiWitnessLogged = true;
      DebugTln(F("WiFi: gateway silent but MQTT, REST or port 25238 traffic flows, no action"));
    }
    return;
  }
  wifiWitnessLogged = false;
  if (wifiDeadRestarts >= WIFI_DEAD_RESTART_CAP) {
    // Cap reached: no more restarts, and at most one forced reconnect per
    // WIFI_CAPPED_RECONNECT_MIN so a false verdict cannot bounce every session.
    if (wifiCappedReconnectDone
        && (uint32_t)(now - wifiCappedReconnectMs) < WIFI_CAPPED_RECONNECT_MIN * 60000UL) return;
    DebugTf(PSTR("WiFi: no traffic, restart cap (%u) reached, reconnecting only\r\n"),
            (unsigned)WIFI_DEAD_RESTART_CAP);
    wifiCappedReconnectDone = true;
    wifiCappedReconnectMs   = now;
    wifiDeadLinkReconnect();
    return;
  }
  if (!wifiDeadReconnectDone) {
    DebugTln(F("WiFi: connected but no traffic, forcing a reconnect"));
    wifiDeadLinkReconnect();
    return;
  }
  wifiDeadRestarts++;
  writeWifiDeadRestarts(wifiDeadRestarts);
  wifiMarkSkipPortal();   // TASK-1455
  doRestart("WiFi: connected but no traffic");
}

// Called from loopWifi() while the link is up and the state machine is idle.
static void wifiLinkProbeTick()
{
  const uint32_t now = millis();
  if (!wifiDeadRestartsLogged) {
    wifiDeadRestartsLogged = true;
    if (wifiDeadRestarts > 0) {
      DebugTf(PSTR("WiFi: %u dead-link restart(s) since the link was last alive for an hour (cap %u)\r\n"),
              (unsigned)wifiDeadRestarts, (unsigned)WIFI_DEAD_RESTART_CAP);
    }
  }
  if (!wifiProbeScheduled) {   // first idle pass after boot, a reconnect or a transport switch
    wifiProbeScheduled = true;
    wifiProbeNextMs = now + WIFI_PROBE_INTERVAL_SEC * 1000UL;
    return;
  }

  if (wifiProbeOpen) {
    if ((uint32_t)(now - wifiProbeStartMs) > WIFI_PROBE_MAX_SPAN_MS) {
      wifiLinkProbeAbort();   // the loop stalled during the probe: its result says nothing
      return;
    }
    if ((int32_t)(now - wifiEchoNextMs) < 0) return;
    if (wifiEchoSent < WIFI_PROBE_PINGS) {
      if (!platformGatewayProbeSend((uint16_t)(wifiEchoSeqBase + wifiEchoSent))) wifiEchoAllocFailed = true;
      wifiEchoSent++;
      wifiEchoNextMs = now + WIFI_PROBE_SPACING_MS;
      return;
    }
    const uint8_t replies = platformGatewayProbeEnd();
    wifiProbeOpen = false;
    if (replies == 0 && wifiEchoAllocFailed) {   // heard nothing and could not send everything
      wifiProbeSkipped(PSTR("no echo buffer"));
      return;
    }
    wifiProbeSkipLogged = false;
    handleWifiProbeVerdict(replies != 0);
    return;
  }

  if ((int32_t)(now - wifiProbeNextMs) < 0) return;
  wifiProbeNextMs = now + WIFI_PROBE_INTERVAL_SEC * 1000UL;

  // Read once per probe, not per loop pass: it reads the station netif's address fields.
  const uint32_t gateway = platformWifiGatewayV4();
  if (gateway == 0) return;   // static IP without a gateway: nothing to probe
  if (wifiArmedGateway != 0 && gateway != wifiArmedGateway) {
    DebugTf(PSTR("WiFi: gateway address changed to %u.%u.%u.%u, dead-link check disarmed\r\n"),
            WIFI_IP4_ARGS(gateway));
    wifiArmedGateway      = 0;
    wifiDeadProbes        = 0;
    wifiDeadReconnectDone = false;
    wifiWitnessLogged     = false;
  }
  if (getHeapHealth() != HEAP_HEALTHY) { wifiProbeSkipped(PSTR("heap not healthy")); return; }

  wifiEchoSeqBase = (uint16_t)(wifiEchoSeqBase + WIFI_PROBE_PINGS);
  if (!platformGatewayProbeBegin(gateway, wifiEchoSeqBase)) { wifiProbeSkipped(PSTR("no pcb")); return; }
  wifiProbeOpen       = true;
  wifiProbeStartMs    = now;
  wifiProbeTarget     = gateway;
  wifiEchoSent        = 0;
  wifiEchoAllocFailed = false;
  wifiEchoNextMs      = now;
}

// The link came back through loopWifi(): a fresh dead streak and alive window, first probe
// one interval from now. Arming and wifiDeadReconnectDone stay: a reconnect that did not
// bring traffic back leads to the restart.
static void wifiLinkProbeOnReconnect()
{
  wifiDeadProbes       = 0;
  wifiLinkAliveSinceMs = millis();
  wifiProbeScheduled   = false;
}

// WIFI_FAILED: true when loopWifi() must keep retrying instead of restarting (a forced
// dead-link reconnect failed while the restart cap is reached). Otherwise a restart that a
// forced reconnect led to is counted toward the cap.
static bool wifiDeadLinkFailedRetry()
{
  if (!wifiDeadReconnectDone) return false;
  if (wifiDeadRestarts >= WIFI_DEAD_RESTART_CAP) {
    DebugTln(F("WiFi: reconnect failed, dead-link restart cap reached, retrying"));
    return true;
  }
  wifiDeadRestarts++;
  writeWifiDeadRestarts(wifiDeadRestarts);
  return false;
}
// TASK-1450 end

void loopWifi() {
  wifiLinkEventsLog();   // TASK-1446: every pass, before the Ethernet early return
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  // When wired Ethernet is the active transport, the WiFi radio is powered down
  // by switchToEthernet() (WIFI_OFF). Do not run the reconnect state machine —
  // it would call WiFi.begin() and resurrect the station, fighting the wired
  // link. loopEthernet() owns the transition back to WiFi on cable removal.
  // A connected wired link also preempts the BETA AP fallback: WIFI_OFF in
  // switchToEthernet() tears down the softAP, and this early return keeps the
  // AP-retry states dormant while Ethernet is active.
  // The dead-link check is WiFi-only: reset it, so a later switch back starts unarmed.
  if (state.net.eMode == NET_ETHERNET) { wifiLinkProbeReset(); return; }
#endif
  DECLARE_TIMER_SEC(timerWifiRetry,  30, CATCH_UP_MISSED_TICKS);
#if defined(_VERSION_PRERELEASE)
  DECLARE_TIMER_SEC(timerAPRetry,   300, CATCH_UP_MISSED_TICKS);  // 5 min between AP→WiFi retries
  DECLARE_TIMER_SEC(timerAPConnWait, 30, CATCH_UP_MISSED_TICKS);  // 30 s to wait for reconnect from AP
#endif

  switch (wifiState) {
    case WIFI_IDLE:
      if (WiFi.status() != WL_CONNECTED) {
        DebugTln(F("WiFi: connection lost, starting non-blocking reconnect"));
        wifiLinkProbeAbort();   // TASK-1450: a probe across the drop says nothing
        wifiRetryCount = 0;
        wifiState = WIFI_DISCONNECTED;
      } else {
        wifiLinkProbeTick();   // TASK-1450
      }
      break;

    case WIFI_DISCONNECTED:
      DebugTf(PSTR("WiFi: reconnect attempt %d starting for hostname [%s]\r\n"),
              wifiRetryCount + 1,
              CSTR(settings.sHostname));
      platformSetHostname(CSTR(settings.sHostname));
      platformWifiPreferStrongestAp();
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
        // BETA: after 2 retries (≈60s) enter AP fallback instead of rebooting. TASK-1645: not with
        // settings.wifi.bNoPortal, which promises no portal and no AP; loopWifi then keeps retrying.
        if (wifiRetryCount >= 2 && !settings.wifi.bNoPortal) {
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
      platformWifiDisableSleep();  // re-assert: modem-sleep can reset across reassociation (TASK-817)
      startTelnet();
      // OTGWstream is auto-started by handleOTGW() in the main loop
      startMQTT();
      startWebSocket();
      wifiLinkProbeOnReconnect();   // TASK-1450
      wifiState = WIFI_IDLE;
      DebugTf(PSTR("WiFi: reconnected, services restarted; IP=%s\r\n"),
              WiFi.localIP().toString().c_str());
      break;

    case WIFI_FAILED:
      // TASK-1450: when the dead-link check forced this reconnect, the restart counts toward
      // its cap; at the cap keep retrying instead of restarting.
      if (wifiDeadLinkFailedRetry()) {
        wifiRetryCount = 0;
        wifiState = WIFI_DISCONNECTED;
        break;
      }
      wifiMarkSkipPortal();   // TASK-1455
      doRestart("WiFi: too many reconnect failures");
      break;

#if defined(_VERSION_PRERELEASE)
    case WIFI_AP_FALLBACK:
      // Periodically attempt to reconnect to the configured WiFi network
      if (DUE(timerAPRetry)) {
        DebugTln(F("BETA AP: 5-min timer — attempting WiFi reconnect"));
        platformSetHostname(CSTR(settings.sHostname));
        platformWifiPreferStrongestAp();
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
        wifiLinkMarkSelf(WIFI_SELF_AP_RETRY);   // TASK-1446: only applies if the link came up (connect without IP)
        WiFi.disconnect();
        RESTART_TIMER(timerAPRetry);
        wifiState = WIFI_AP_FALLBACK;
      }
      break;
#endif  // _VERSION_PRERELEASE
  }
}

//===========================================================================================
// SAT enum stringifiers — used only by the welcome banner below. Short literal
// strings; the compiler dedupes and places them in flash via the literal pool.
static const char* satControlModeStr(SATControlMode m) {
  switch (m) {
    case SAT_MODE_OFF:        return "off";
    case SAT_MODE_CONTINUOUS: return "continuous";
    case SAT_MODE_PWM:        return "pwm";
  }
  return "?";
}
static const char* satBoilerStatusStr(SATBoilerStatus s) {
  switch (s) {
    case SAT_BS_OFF:               return "off";
    case SAT_BS_IDLE:              return "idle";
    case SAT_BS_PREHEATING:        return "preheat";
    case SAT_BS_AT_SETPOINT:       return "at_setpoint";
    case SAT_BS_MODULATING_UP:     return "mod_up";
    case SAT_BS_MODULATING_DOWN:   return "mod_down";
    case SAT_BS_IGNITION_SURGE:    return "ign_surge";
    case SAT_BS_STALLED_IGNITION:  return "stalled";
    case SAT_BS_ANTI_CYCLING:      return "anti_cycle";
    case SAT_BS_PUMP_STARTING:     return "pump_start";
    case SAT_BS_WAITING_FLAME:     return "wait_flame";
    case SAT_BS_OVERSHOOT_COOLING: return "over_cool";
    case SAT_BS_POST_CYCLE:        return "post_cycle";
    case SAT_BS_HEATING:           return "heating";
    case SAT_BS_COOLING:           return "cooling";
  }
  return "?";
}
static const char* satCycleClassStr(SATCycleClass c) {
  switch (c) {
    case SAT_CYCLE_NONE:      return "none";
    case SAT_CYCLE_GOOD:      return "good";
    case SAT_CYCLE_OVERSHOOT: return "overshoot";
    case SAT_CYCLE_UNDERHEAT: return "underheat";
    case SAT_CYCLE_SHORT:     return "short";
    case SAT_CYCLE_UNCERTAIN: return "uncertain";
    case SAT_CYCLE_UNDERHEAT_PWM: return "underheat_pwm";
    case SAT_CYCLE_INSUFFICIENT:  return "insufficient";
  }
  return "?";
}

//===========================================================================================
// Send the welcome banner to a freshly-connected telnet client.
// Called from the SimpleTelnet onConnect callback — receives client IP as const char*.
// Compact log-triage snapshot covering: firmware identity + reset reason, hostname/uptime,
// network transport (WiFi or Ethernet, AP-fallback if active), heap (computed fragmentation
// via platform_esp32.h helpers), heap-diag drop counters, PIC, OT-bus, MQTT, NTP, SAT
// control state, BLE sensor health (ESP32), and live state of every debug toggle. Each
// line is its own _debugPrintf_P / println so the 256-byte vsnprintf stack buffer is
// reclaimed between fields.
static void sendTelnetBanner(const char* ip)
{
  char rstReason[40];
  platformResetReason(rstReason, sizeof(rstReason));

  debugTelnet.println(F("\r\n============================================"));
  debugTelnet.println(F("  OpenTherm Gateway -- OTGW-firmware"));
  _debugPrintf_P(PSTR("  FW   : %s #%u  fs:%s\r\n"),
    _VERSION,
    (unsigned)_VERSION_BUILD,
    checklittlefshash() ? "ok" : "mismatch");
  _debugPrintf_P(PSTR("  Boot : %s\r\n"), rstReason);
  _debugPrintf_P(PSTR("  Host : %s   Up: %s   Reboots: %lu\r\n"),
    settings.sHostname,
    upTime().c_str(),
    (unsigned long)state.uptime.iRebootCount);
  debugTelnet.println(F("============================================"));
  debugTelnet.print(F("  Net  : "));
  debugTelnet.print(networkModeName());
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  if (state.net.eMode == NET_ETHERNET) {
    _debugPrintf_P(PSTR("   IP %s\r\n"), getActiveIP().c_str());
  } else
#endif
  {
    const int wifiRssi = WiFi.RSSI();
    _debugPrintf_P(PSTR("   SSID %s   RSSI %d dBm (%d%% %s)   IP %s\r\n"),
      WiFi.SSID().c_str(), wifiRssi, signal_quality_perc_quad(wifiRssi),
      dBmtoQuality(wifiRssi).c_str(), getActiveIP().c_str());
  }
#if defined(_VERSION_PRERELEASE)
  if (state.net.bAPFallback) {
    _debugPrintf_P(PSTR("         AP-Fallback active: %s\r\n"), state.net.sAPSSID);
  }
#endif
  printWifiLinkBanner();   // TASK-1446: Link + Drop lines (WiFi station record, also while wired)
  _debugPrintf_P(PSTR("  Heap : free %u  frag %u%%  minFree %u  maxBlk %u  sketchFree %u\r\n"),
    (unsigned)platformFreeHeap(),
    (unsigned)platformHeapFragmentation(),
    (unsigned)platformMinFreeHeap(),
    (unsigned)platformMaxFreeBlock(),
    (unsigned)platformFreeSketchSpace());
  _debugPrintf_P(PSTR("  Drops: ws %lu  mqtt %lu   low/warn/crit %lu/%lu/%lu   slow %lu   maxLoopGap %lu ms\r\n"),
    (unsigned long)state.heapdiag.iWsDropsTotal,
    (unsigned long)state.heapdiag.iMqttDropsTotal,
    (unsigned long)state.heapdiag.iEnteredLowCount,
    (unsigned long)state.heapdiag.iEnteredWarningCount,
    (unsigned long)state.heapdiag.iEnteredCriticalCount,
    (unsigned long)state.heapdiag.iDripSlowModeCount,
    (unsigned long)state.heapdiag.iMaxLoopGapMs);
  debugTelnet.println(F("--------------------------------------------"));
  _debugPrintf_P(PSTR("  PIC  : %s  v%s   id %s\r\n"),
    state.pic.sType, state.pic.sFwversion, state.pic.sDeviceid);
  _debugPrintf_P(PSTR("  OTbus: %s   GW-mode: %s   Boiler: %s  Thermostat: %s   PS: %s\r\n"),
    state.otBus.bOnline ? "online" : "offline",
    state.otBus.bGatewayModeKnown ? CCONOFF(state.otBus.bGatewayMode) : "detecting",
    CCONOFF(state.otBus.bBoilerState),
    CCONOFF(state.otBus.bThermostatState),
    CCONOFF(state.otBus.bPSmode));
  _debugPrintf_P(PSTR("  MQTT : %s   broker %s:%d   ha-prefix: %s\r\n"),
    state.mqtt.bConnected ? "connected" : "disconnected",
    settings.mqtt.sBroker,
    (int)settings.mqtt.iBrokerPort,
    settings.mqtt.sHaprefix);
  _debugPrintf_P(PSTR("  NTP  : %s   tz %s   sendtime: %s\r\n"),
    settings.ntp.bEnable ? "on" : "off",
    settings.ntp.sTimezone,
    CCONOFF(settings.ntp.bSendtime));
  debugTelnet.println(F("--------------------------------------------"));
  _debugPrintf_P(PSTR("  SAT  : %s   mode %s   sp %.1fC   mod %u%%\r\n"),
    state.sat.bActive ? "active" : "inactive",
    satControlModeStr(state.sat.eControlMode),
    (double)state.sat.fFinalSetpoint,
    (unsigned)state.sat.iCurrentModulation);
  _debugPrintf_P(PSTR("         status %s   lastCycle %s   fallback %s   wx %s\r\n"),
    satBoilerStatusStr(state.sat.eBoilerStatus),
    satCycleClassStr(satReportedLastCycleClass(state.sat, millis())),
    state.sat.bFallbackActive ? "ON" : "off",
    state.sat.weather.bValid ? "ok" : "stale");
#if HAS_SAT_BLE
  // TASK-742: gate the BLE status line on the capability flag (not raw ESP32)
  // so a BLE-less board does not print a misleading "sensors 0" line.
  _debugPrintf_P(PSTR("  BLE  : sensors %u   batt %u%%   rssi %d dBm\r\n"),
    (unsigned)state.sat.iBleSensorCount,
    (unsigned)state.sat.iBleBattery,
    (int)state.sat.iBleRssi);
#endif
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Debug toggles (press key to flip):"));
  _debugPrintf_P(PSTR("    1 OTmsg     [%s]    2 REST API  [%s]    3 MQTT      [%s]\r\n"),
    state.debug.bOTmsg    ? "1" : "0",
    state.debug.bRestAPI  ? "1" : "0",
    state.debug.bMQTT     ? "1" : "0");
  _debugPrintf_P(PSTR("    4 Sensors   [%s]    5 SAT       [%s]    6 OTDirect  [%s]\r\n"),
    state.debug.bSensors  ? "1" : "0",
    state.debug.bSAT      ? "1" : "0",
    state.debug.bOTDirect ? "1" : "0");
#if HAS_SAT_BLE
  _debugPrintf_P(PSTR("    7 SATBLE    [%s]    g MQTTGate  [%s]    n NTP       [%s]\r\n"),
    state.debug.bSATBLE   ? "1" : "0",
    state.debug.bMQTTGate ? "1" : "0",
    state.debug.bNTP      ? "1" : "0");
#else
  _debugPrintf_P(PSTR("    g MQTTGate  [%s]    n NTP       [%s]\r\n"),
    state.debug.bMQTTGate ? "1" : "0",
    state.debug.bNTP      ? "1" : "0");
#endif
  _debugPrintf_P(PSTR("    d SensorSim [%s]    OTGW-Sim    [%s]    8 SATtrace  [%s]\r\n"),
    state.debug.bSensorSim       ? "1" : "0",
    state.debug.bOTGWSimulation  ? "1" : "0",
    state.debug.bSATTrace        ? "1" : "0");
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Press 'h' for command menu."));
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

// TASK-1644: stop() closes a connected client from the server side, which leaves that connection in
// TIME_WAIT on local port 23 for 2 x CONFIG_LWIP_TCP_MSL (120 s). AsyncTCP binds the listener without
// SO_REUSEADDR, so a begin() inside that window fails its tcp_bind and port 23 stays closed (bench .61,
// 2026-11-05: refused until the window passed). After a stop, startTelnet() opens a retry window in
// which telnetRelistenTick() calls begin() again every 10 s; begin() returns at once while listening.
static const uint32_t TELNET_RELISTEN_WINDOW_MS = 180000UL;   // TIME_WAIT 120 s plus margin
static bool     telnetStoppedSinceBoot = false;
static uint32_t telnetRelistenStartMs  = 0;
static bool     telnetRelistenActive   = false;

//===========================================================================================
void startTelnet()
{
  // TASK-1644: settings.bTelnetEnabled false closes port 23 and drops a connected client. Debug output
  // then goes nowhere: AsyncSimpleTelnet::write() returns 0 with no client. Every caller (boot, WiFi
  // reconnect, the deferred settings apply) comes through here, so the setting holds on every path.
  if (!settings.bTelnetEnabled) {
    DebugTln(F("Telnet debug console disabled (setting telnetenabled), closing port 23"));
    debugTelnet.stop();
    telnetStoppedSinceBoot = true;
    telnetRelistenActive = false;
    return;
  }
  if (telnetStoppedSinceBoot) {
    telnetRelistenStartMs = millis();
    telnetRelistenActive = true;
  }
  debugTelnet.onConnect(sendTelnetBanner);
  debugTelnet.setLineMode(false);
  // TASK-1333: Nagle on for the debug console only, so the TCP stack merges the
  // small fragments of each log line. With every fragment sent at once, a debug
  // burst held ~40 KB of heap in send buffers (minimum free heap 1064 B on the
  // bench). Port 25238 (OTGWstream) keeps NoDelay for the PIC request/response.
  debugTelnet.setNoDelay(false);
  debugTelnet.onInputReceived(onTelnetInput);
  debugTelnet.begin(false);        // port was fixed in the constructor (23)
  DebugT(F("\r\nTelnet debug server started on "));
  DebugT(getActiveIP());
  DebugTln(F(":23"));
} // startTelnet()

//===========================================================================================
// TASK-1644: called from doBackgroundTasks(). Only active for TELNET_RELISTEN_WINDOW_MS after a
// re-enable that followed a stop(); does nothing otherwise.
void telnetRelistenTick()
{
  if (!telnetRelistenActive) return;
  if (!settings.bTelnetEnabled || (uint32_t)(millis() - telnetRelistenStartMs) > TELNET_RELISTEN_WINDOW_MS) {
    telnetRelistenActive = false;
    return;
  }
  DECLARE_TIMER_SEC(timerTelnetRelisten, 10, SKIP_MISSED_TICKS);
  if (DUE(timerTelnetRelisten)) debugTelnet.begin(false);
}

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

  // platformNtpHostnameFix() guards an ESP8266 SDK bug where configTime()
  // resets the WiFi station hostname; it is a no-op on ESP32. Call before and
  // after configTime() so this path stays platform-neutral (see TASK-432).
  platformNtpHostnameFix(CSTR(settings.sHostname));
  configTime(0, 0, settings.ntp.sHostname, nullptr, nullptr);
  platformNtpHostnameFix(CSTR(settings.sHostname));
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
            if (diagRec.active) diagRec.recTimeSync((uint32_t)now);   // TASK-1665 (ADR-213)
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

//=====[ Outbound HTTP by IP (TASK-1605) ]=====================================
// Application code never resolves through Network/WiFi.hostByName() and never lets
// HTTPClient or NetworkClient connect by hostname, because that path clears the lwIP DNS
// cache without the core lock (see platformResolveHost() in platform_esp32.h).
// tests/test_no_hostbyname.py enforces this.

static const size_t NET_HOST_MAX = 64;

// Host and port of an "http://host[:port]/..." URL, host resolved with platformResolveHost().
// Port 80 when the URL has none. False on a malformed URL or a failed lookup (ip is 0.0.0.0).
bool netResolveUrlHost(const char* url, IPAddress& ip, uint16_t& port)
{
  ip = IPAddress(static_cast<uint32_t>(0));
  port = 80;
  if (url == nullptr || strncasecmp_P(url, PSTR("http://"), 7) != 0) return false;
  const char* host = url + 7;
  size_t len = 0;
  while (host[len] && host[len] != '/' && host[len] != ':' && host[len] != '?') len++;
  if (len == 0 || len >= NET_HOST_MAX) return false;
  char hostBuf[NET_HOST_MAX];
  memcpy(hostBuf, host, len);
  hostBuf[len] = '\0';
  if (host[len] == ':') {
    const char* s = host + len + 1;
    uint32_t p = 0;
    size_t digits = 0;
    while (*s >= '0' && *s <= '9' && digits < 6) { p = p * 10 + (uint32_t)(*s - '0'); s++; digits++; }
    if (digits == 0 || p == 0 || p > 65535) return false;
    port = (uint16_t)p;
  }
  return platformResolveHost(hostBuf, ip);
}

// Connect client by IP and set its read timeout. HTTPClient only sets the read timeout on
// its own connect path (HTTPClient.cpp:1115, arduino-esp32 3.3.5), which a pre-connected
// client skips.
bool netConnectIp(WiFiClient& client, const IPAddress& ip, uint16_t port,
                  uint32_t connectTimeoutMs, uint32_t readTimeoutMs)
{
  if (!client.connect(ip, port, (int32_t)connectTimeoutMs)) return false;
  client.setTimeout(readTimeoutMs);
  return true;
}

// Resolve the host of url and connect client to it by IP, for an HTTPClient that then reuses
// the connection. The order at the call site matters (HTTPClient.cpp, arduino-esp32 3.3.5):
//   1. http.begin(client, url): begin() on a client that is already connected stops it when
//      the host differs from the previous one, and a fresh HTTPClient has none (:283-287).
//   2. netConnectUrl(client, url, ...): resolve with platformResolveHost(), connect(ip, port).
//   3. http.GET() / sendRequest(): connect() finds the client connected and reuses it without
//      DNS (:1075-1085). The Host header still carries the hostname from the URL (:288, :1145).
// If this returns false the caller must not send the request: HTTPClient would then connect by
// hostname itself.
bool netConnectUrl(WiFiClient& client, const char* url, uint32_t connectTimeoutMs, uint32_t readTimeoutMs)
{
  IPAddress ip;
  uint16_t port = 80;
  if (!netResolveUrlHost(url, ip, port)) {
    DebugTf(PSTR("Net: DNS lookup failed for %s\r\n"), url);
    return false;
  }
  if (!netConnectIp(client, ip, port, connectTimeoutMs, readTimeoutMs)) {
    DebugTf(PSTR("Net: connect to %u.%u.%u.%u:%u failed\r\n"), ip[0], ip[1], ip[2], ip[3], port);
    return false;
  }
  return true;
}

/***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
****************************************************************************
*/
