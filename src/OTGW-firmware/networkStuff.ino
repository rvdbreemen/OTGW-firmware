/*
***************************************************************************
**  Program  : networkStuff.ino
**  Version  : v1.7.8-beta.2
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

// Debug telnet instance (port 23). SimpleTelnet replaces ESPTelnet for debug output.
// Port is fixed in the constructor; begin() needs no port argument.
SimpleTelnet<1> debugTelnet(23);

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

  // TASK-548: apply static IP before any connect attempt.
  // All fields empty (default) → let the OS keep DHCP mode unchanged.
  if (strlen(settings.wifi.sStaticIp) > 0) {
    IPAddress ip, gw, sn, dns1, dns2;
    if (ip.fromString(settings.wifi.sStaticIp) &&
        sn.fromString(settings.wifi.sSubnet)   &&
        gw.fromString(settings.wifi.sGateway)) {
      if (strlen(settings.wifi.sDns1) > 0) dns1.fromString(settings.wifi.sDns1);
      if (strlen(settings.wifi.sDns2) > 0) dns2.fromString(settings.wifi.sDns2);
      WiFi.config(ip, gw, sn, dns1, dns2);
      DebugTf(PSTR("Static IP: %s / %s gw %s\r\n"),
              settings.wifi.sStaticIp, settings.wifi.sSubnet, settings.wifi.sGateway);
    } else {
      DebugTln(F("Static IP: parse error — falling back to DHCP"));
    }
  }

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
    // WiFi.hostname() returns a String by API; copy it out in the same statement
    // so the temporary dies here (TASK-1141, ADR-049). 32 chars is the DHCP limit.
    char currentHostname[33];
    strlcpy(currentHostname, WiFi.hostname().c_str(), sizeof(currentHostname));
    if (strcmp(currentHostname, hostname) == 0)
    {
      DebugTln(F("Wifi already connected with correct hostname, skipping hostname update."));
    }
    else
    {
      DebugTf(PSTR("Wifi connected with hostname '%s', updating to '%s' without reconnect.\r\n"),
              currentHostname, hostname);
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
    DebugTf(PSTR("Starting config portal (heap: %u free, %u fragmented, %u max block)...\r\n"),
            ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
    if (!manageWiFi.startConfigPortal(thisAP))
    {
      DebugTln(F("failed to connect and hit timeout"));
      storeRebootReasonUnlessPending("WiFi: config portal timeout");   // TASK-1247
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

  // Ensure the hostname is set for the next DHCP exchange (renewal or
  // reconnect). No SDK DHCP calls anywhere in this file: the SDK manages the
  // DHCP client autonomously when user code never invokes
  // wifi_station_dhcpc_start/stop. This is the v1.2.0 baseline pattern, see
  // TASK-432 for why we restored it.
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

// Refresh services that truly depend on upstream connectivity.
// TCP listeners (telnet, OTGW stream, WebSocket server) are started once at
// boot and should not be re-bound on every WiFi reconnect.
void refreshServicesAfterWifiReconnect() {
  // Force MQTT state machine back into a clean reconnect cycle.
  doMqttDisconnect();
  startMQTT();

  // Drop stale WS clients from before the WiFi outage; server keeps listening.
  // Must be disconnect(), not close(): WebSocketsServer::close() also calls
  // _server->close() and clears _runnning, which unbinds port 81 for good —
  // begin() is only ever called from setup(), so the OT-log stream (and the
  // PIC flash progress that rides on it) stays dead until the next reboot.
  doWebSocketDisconnectAll();
}

//===========================================================================================
// TASK-1248: detect a WiFi link that stays associated with an IP while no
// traffic passes, and recover from it. loopWifi() only acts on
// WiFi.status() != WL_CONNECTED, and WL_CONNECTED means "associated and got an
// IP", so such a link was never acted on (GH #682: two ~72 min hangs).
//
// Detection: while the link is up, the default gateway gets WIFI_PROBE_PINGS
// ICMP echo requests every WIFI_PROBE_INTERVAL_SEC, sent from the loop on a
// lwIP raw ICMP pcb that exists only during the probe (about 6 s). The SDK's
// ping_start() is not used: its receive callback moves the payload of every
// ICMP packet it does not eat (esp-ping.c in the core 2.7.4 lwip2 glue), so
// echo requests to the gateway were dropped while a probe ran. The callback
// below reads with pbuf_copy_partial() and never touches the payload.
// - Armed per gateway: a gateway counts only after it has answered once. A
//   router that drops ICMP never arms the check, a network that is dead from
//   boot cannot trigger it, and a new gateway address disarms it again.
// - Witnesses: a silent gateway alone does not prove a dead link. While MQTT
//   is connected (PubSubClient drops a dead connection within two keepalives,
//   120 s), a REST request arrived, or a port 25238 client sent a byte within
//   WIFI_WITNESS_WINDOW_MS, no action is taken. A router reboot therefore
//   leaves a gateway with any LAN client alone. The broker is used only as
//   proof of traffic, never as a trigger.
// - No probe while the heap is not HEALTHY, and a probe whose pcb or echo
//   buffer could not be allocated does not count.
//
// Escalation after WIFI_PROBE_FAILS_TO_ACT dead probes in a row:
// 1. a forced reconnect with wifi_station_disconnect(), which keeps the stored
//    credentials (WiFi.disconnect() would erase them while persistent(true) is
//    set); loopWifi then reconnects as for any other drop. The DHCP lease is
//    released by the disconnect, which is why the witnesses come first;
// 2. if the link is still dead after that reconnect, doRestart(). A forced
//    reconnect that cannot get the link back within loopWifi's 10 x 30 s ends
//    in its WIFI_FAILED restart, which then counts toward the same cap.
//
// Restart cap: a link that works for a few minutes after every boot and then
// dies would otherwise restart the gateway endlessly. Dead-link restarts are
// counted in RTC user memory (ESP.rtcUserMemory offsets 113..114) and survive
// software restarts, including a config-portal timeout in between. At
// WIFI_DEAD_RESTART_CAP the gateway stops restarting and forces at most one
// reconnect per WIFI_CAPPED_RECONNECT_MIN. The count clears after the link has
// stayed alive for WIFI_DEAD_RESTART_CLEAR_MIN, and on a power-on, crash or
// watchdog reset. Core 2.7.4 restarts after a panic or abort() through the
// SDK as well, so such a crash probably also reads as a software restart and
// keeps the count (inferred from core_esp8266_postmortem.cpp, not verified).
#define WIFI_PROBE_INTERVAL_SEC       60  // one probe per minute while connected
#define WIFI_PROBE_PINGS               3  // echo requests per probe
#define WIFI_PROBE_SPACING_MS       2000  // between echoes, and the wait after the last one
#define WIFI_PROBE_FAILS_TO_ACT        8  // dead probes in a row before acting: long enough that a
                                          // typical router reboot (a few minutes) passes unharmed
#define WIFI_WITNESS_WINDOW_MS     60000  // REST or port 25238 traffic this recent proves the LAN works
#define WIFI_DEAD_RESTART_CAP          3
#define WIFI_DEAD_RESTART_CLEAR_MIN   60
#define WIFI_CAPPED_RECONNECT_MIN     15
#define WIFI_DEAD_RESTART_RTC_SLOT   113  // offsets 113..114
#define WIFI_DEAD_RESTART_MAGIC      0x4F544452UL  // "OTDR"
#define WIFI_ECHO_ID              0x4F54  // "OT", distinct from the SDK ping's 0xAFAF
#define WIFI_ECHO_DATA                16  // payload bytes per echo request

static struct raw_pcb   *wifiProbePcb      = nullptr;  // only set while a probe runs
static volatile uint8_t  wifiEchoReplyMask = 0;        // bit n: echo (seq base + n) answered
static uint16_t wifiEchoSeqBase            = 0;
static uint8_t  wifiEchoSent               = 0;
static bool     wifiEchoSendFailed         = false;
static uint32_t wifiEchoNextMs             = 0;
static uint32_t wifiProbeTarget            = 0;   // gateway address of the running probe
static uint8_t  wifiReconnectGen           = 0;   // bumped on every reconnect
static uint8_t  wifiProbeGen               = 0;   // wifiReconnectGen when the probe started
static uint32_t wifiArmedGateway           = 0;   // gateway that answered; 0 = not armed
static uint8_t  wifiDeadProbes             = 0;
static bool     wifiDeadReconnectDone      = false;  // a forced reconnect ran in this dead streak
static uint8_t  wifiDeadRestarts           = 0;   // dead-link restarts, mirrored in RTC
static bool     wifiDeadRestartsLogged     = false;
static uint32_t wifiLinkAliveSinceMs       = 0;
static uint32_t wifiCappedReconnectMs      = 0;
static bool     wifiCappedReconnectDone    = false;

struct WifiDeadRestartRtc {
  uint32_t magic;
  uint32_t count;
};

static void writeWifiDeadRestarts(uint32_t count)
{
  WifiDeadRestartRtc rec = { WIFI_DEAD_RESTART_MAGIC, count };
  ESP.rtcUserMemoryWrite(WIFI_DEAD_RESTART_RTC_SLOT, reinterpret_cast<uint32_t*>(&rec), sizeof(rec));
}

// Called once at the top of setup(), before anything can restart. A software
// restart keeps the count; any other reset reason starts from zero.
void captureWifiDeadRestarts()
{
  WifiDeadRestartRtc rec = { 0, 0 };
  rst_info *resetInfo = ESP.getResetInfoPtr();
  const bool softRestart = (resetInfo != nullptr) && (resetInfo->reason == REASON_SOFT_RESTART);
  if (softRestart && ESP.rtcUserMemoryRead(WIFI_DEAD_RESTART_RTC_SLOT, reinterpret_cast<uint32_t*>(&rec), sizeof(rec))
      && rec.magic == WIFI_DEAD_RESTART_MAGIC && rec.count <= WIFI_DEAD_RESTART_CAP) {
    wifiDeadRestarts = (uint8_t)rec.count;
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

// lwIP receive callback, runs in the SDK/lwIP context: only set flags here.
// p->payload points at the IP header and must stay where it is for every
// packet that is not eaten, or lwIP's own ICMP handling misreads it.
static u8_t wifiEchoRecv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
  (void)arg;
  (void)pcb;
  if (p == nullptr || p->tot_len < 1 || addr == nullptr) return 0;
  const uint16_t hlen = (uint16_t)((pbuf_get_at(p, 0) & 0x0F) * 4);
  struct icmp_echo_hdr hdr;
  if (pbuf_copy_partial(p, &hdr, sizeof(hdr), hlen) != sizeof(hdr)) return 0;
  if (ICMPH_TYPE(&hdr) != ICMP_ER || hdr.id != PP_HTONS(WIFI_ECHO_ID)) return 0;
  if (ip_addr_get_ip4_u32(addr) != wifiProbeTarget) return 0;
  const uint16_t n = (uint16_t)(lwip_ntohs(hdr.seqno) - wifiEchoSeqBase);
  if (n < WIFI_PROBE_PINGS) wifiEchoReplyMask |= (uint8_t)(1u << n);
  pbuf_free(p);   // our own echo reply (current or late): eat it
  return 1;
}

static bool sendWifiEcho(uint16_t seq)
{
  const uint16_t len = (uint16_t)(sizeof(struct icmp_echo_hdr) + WIFI_ECHO_DATA);
  struct pbuf *p = pbuf_alloc(PBUF_IP, len, PBUF_RAM);
  if (p == nullptr) return false;
  struct icmp_echo_hdr *echo = static_cast<struct icmp_echo_hdr *>(p->payload);
  ICMPH_TYPE_SET(echo, ICMP_ECHO);
  ICMPH_CODE_SET(echo, 0);
  echo->id     = PP_HTONS(WIFI_ECHO_ID);
  echo->seqno  = lwip_htons(seq);
  echo->chksum = 0;
  uint8_t *data = reinterpret_cast<uint8_t *>(echo) + sizeof(struct icmp_echo_hdr);
  for (uint16_t i = 0; i < WIFI_ECHO_DATA; i++) data[i] = (uint8_t)i;
  echo->chksum = inet_chksum(echo, len);
  ip_addr_t dst;
  ip_addr_set_ip4_u32(&dst, wifiProbeTarget);
  const err_t err = raw_sendto(wifiProbePcb, p, &dst);
  pbuf_free(p);
  return err == ERR_OK;
}

static void handleWifiProbeVerdict(bool gatewayAnswered)
{
  if (gatewayAnswered) {
    if (wifiArmedGateway != wifiProbeTarget) {
      wifiArmedGateway = wifiProbeTarget;
      wifiLinkAliveSinceMs = millis();
      DebugTf(PSTR("WiFi: gateway %s answers ping, dead-link check armed\r\n"),
              IPAddress(wifiArmedGateway).toString().c_str());
    }
    if (wifiDeadProbes > 0 || wifiDeadReconnectDone) {
      DebugTln(F("WiFi: gateway answers again, dead-link check reset"));
      wifiLinkAliveSinceMs = millis();
    }
    wifiDeadProbes        = 0;
    wifiDeadReconnectDone = false;
    if (wifiDeadRestarts > 0
        && (uint32_t)(millis() - wifiLinkAliveSinceMs) >= WIFI_DEAD_RESTART_CLEAR_MIN * 60000UL) {
      DebugTln(F("WiFi: link alive for an hour, dead-link restart count cleared"));
      wifiDeadRestarts        = 0;
      wifiCappedReconnectDone = false;
      writeWifiDeadRestarts(0);
    }
    return;
  }
  if (wifiArmedGateway == 0) return;   // no gateway answered yet: check stays off

  if (wifiDeadProbes < 255) wifiDeadProbes++;
  DebugTf(PSTR("WiFi: no ping reply from gateway %s (%u/%u)\r\n"),
          IPAddress(wifiArmedGateway).toString().c_str(), wifiDeadProbes, WIFI_PROBE_FAILS_TO_ACT);
  if (wifiDeadProbes < WIFI_PROBE_FAILS_TO_ACT) return;
  wifiDeadProbes = 0;

  if (wifiLanTrafficSeen()) {
    DebugTln(F("WiFi: gateway silent but MQTT, REST or port 25238 traffic flows, no action"));
    return;
  }
  if (wifiDeadRestarts >= WIFI_DEAD_RESTART_CAP) {
    // Cap reached: no more restarts, and at most one forced reconnect per
    // WIFI_CAPPED_RECONNECT_MIN so a false verdict cannot bounce every session.
    if (wifiCappedReconnectDone
        && (uint32_t)(millis() - wifiCappedReconnectMs) < WIFI_CAPPED_RECONNECT_MIN * 60000UL) return;
    DebugTf(PSTR("WiFi: no traffic, restart cap (%u) reached, reconnecting only\r\n"),
            WIFI_DEAD_RESTART_CAP);
    wifiCappedReconnectDone = true;
    wifiCappedReconnectMs   = millis();
    wifiDeadReconnectDone   = true;
    wifi_station_disconnect();
    return;
  }
  if (!wifiDeadReconnectDone) {
    wifiDeadReconnectDone = true;
    DebugTln(F("WiFi: connected but no traffic, forcing a reconnect"));
    wifi_station_disconnect();   // loopWifi sees the drop and reconnects
    return;
  }
  wifiDeadRestarts++;
  writeWifiDeadRestarts(wifiDeadRestarts);
  doRestart("WiFi: connected but no traffic");
}

// Ends the running probe: removes the pcb and hands the verdict on.
static void finishWifiProbe()
{
  raw_remove(wifiProbePcb);
  wifiProbePcb = nullptr;
  const uint8_t replies = wifiEchoReplyMask;
  // A probe that started before a reconnect says nothing about the new link,
  // and one that could not send an echo and heard nothing proves nothing.
  if (wifiProbeGen != wifiReconnectGen) return;
  if (replies == 0 && wifiEchoSendFailed) return;
  handleWifiProbeVerdict(replies != 0);
}

// Called from loopWifi() while the link is up and the state machine is idle.
static void wifiLinkProbeTick()
{
  DECLARE_TIMER_SEC(timerWifiProbe, WIFI_PROBE_INTERVAL_SEC, SKIP_MISSED_TICKS);

  if (!wifiDeadRestartsLogged) {
    wifiDeadRestartsLogged = true;
    if (wifiDeadRestarts > 0) {
      DebugTf(PSTR("WiFi: %u dead-link restart(s) since the link was last alive for an hour (cap %u)\r\n"),
              wifiDeadRestarts, WIFI_DEAD_RESTART_CAP);
    }
  }

  if (wifiProbePcb != nullptr) {   // a probe is running
    if ((int32_t)(millis() - wifiEchoNextMs) < 0) return;
    if (wifiEchoSent < WIFI_PROBE_PINGS) {
      if (!sendWifiEcho((uint16_t)(wifiEchoSeqBase + wifiEchoSent))) wifiEchoSendFailed = true;
      wifiEchoSent++;
      wifiEchoNextMs = millis() + WIFI_PROBE_SPACING_MS;
      return;
    }
    finishWifiProbe();
    return;
  }
  if (!DUE(timerWifiProbe)) return;

  IPAddress gateway = WiFi.gatewayIP();
  if (!gateway.isSet()) return;   // static IP without a gateway: nothing to probe
  if (wifiArmedGateway != 0 && (uint32_t)gateway != wifiArmedGateway) {
    DebugTln(F("WiFi: gateway address changed, dead-link check disarmed"));
    wifiArmedGateway      = 0;
    wifiDeadProbes        = 0;
    wifiDeadReconnectDone = false;
  }
  if (getHeapHealth() != HEAP_HEALTHY) return;   // skipped, not counted

  wifiProbePcb = raw_new(IP_PROTO_ICMP);
  if (wifiProbePcb == nullptr) return;           // skipped, not counted
  wifiProbeTarget     = (uint32_t)gateway;
  wifiProbeGen        = wifiReconnectGen;
  wifiEchoSeqBase     = (uint16_t)(wifiEchoSeqBase + WIFI_PROBE_PINGS);
  wifiEchoReplyMask   = 0;
  wifiEchoSent        = 0;
  wifiEchoSendFailed  = false;
  wifiEchoNextMs      = millis();
  raw_recv(wifiProbePcb, wifiEchoRecv, nullptr);
}

void loopWifi() {
  DECLARE_TIMER_SEC(timerWifiRetry, 30, CATCH_UP_MISSED_TICKS);

  switch (wifiState) {
    case WIFI_IDLE:
      if (WiFi.status() != WL_CONNECTED) {
        DebugTln(F("WiFi: connection lost, starting non-blocking reconnect"));
        wifiRetryCount = 0;
        wifiState = WIFI_DISCONNECTED;
      } else {
        wifiLinkProbeTick();
      }
      break;

    case WIFI_DISCONNECTED:
      DebugTf(PSTR("WiFi: reconnect attempt %d starting for hostname [%s]\r\n"),
              wifiRetryCount + 1,
              CSTR(settings.sHostname));
      WiFi.hostname(CSTR(settings.sHostname));
      // No SDK DHCP calls anywhere in this file — letting the SDK manage the
      // DHCP client state entirely is the v1.2.0 baseline that worked across
      // router reboots and brief glitches. Once user code calls
      // wifi_station_dhcpc_start() (in any state, connected or not), the SDK
      // considers DHCP "user-managed" and stops auto-restarting it on
      // subsequent associations driven by setAutoReconnect(true). That
      // regression is what TASK-432 surfaced as "associates but no DHCP/IP
      // after reboot". WiFi.begin() with stored credentials triggers
      // wifi_station_connect(); SDK-managed DHCP fires the DISCOVER as part
      // of the association sequence.
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
      // No SDK DHCP calls here either: the SDK handled the DHCP negotiation
      // as part of the association triggered by WiFi.begin() in
      // WIFI_DISCONNECTED. Re-applying WiFi.hostname() is harmless and keeps
      // the next DHCP exchange (renewal or reconnect) using the configured
      // name. See TASK-432 for the rationale (v1.2.0 baseline approach).
      WiFi.hostname(CSTR(settings.sHostname));
      refreshServicesAfterWifiReconnect();
      wifiReconnectGen++;   // TASK-1248: probe results from before this are stale
      wifiDeadProbes = 0;
      wifiLinkAliveSinceMs = millis();   // the alive window starts again after any drop
      wifiState = WIFI_IDLE;
      DebugTf(PSTR("WiFi: reconnected, services refreshed; IP=%s\r\n"),
              WiFi.localIP().toString().c_str());
      break;

    case WIFI_FAILED:
      // TASK-1248: when the dead-link check forced this reconnect, the restart
      // counts toward its cap; at the cap keep retrying instead of restarting.
      if (wifiDeadReconnectDone) {
        if (wifiDeadRestarts >= WIFI_DEAD_RESTART_CAP) {
          DebugTln(F("WiFi: reconnect failed, dead-link restart cap reached, retrying"));
          wifiRetryCount = 0;
          wifiState = WIFI_DISCONNECTED;
          break;
        }
        wifiDeadRestarts++;
        writeWifiDeadRestarts(wifiDeadRestarts);
      }
      doRestart("WiFi: too many reconnect failures");
      break;
  }
}

//===========================================================================================
// Send the welcome banner to a freshly-connected telnet client.
// Called from the SimpleTelnet onConnect callback — receives client IP as const char*.
// Compact log-triage snapshot: identity, network/heap health, OTGW/PIC/MQTT/NTP state,
// and the live state of every debug toggle. Mirrors the data sources of dumpDebugInfo()
// ('D' command). Each line is a separate _debugPrintf_P / println so the 256-byte vsnprintf
// stack buffer is reclaimed between fields.
static void sendTelnetBanner(const char* ip)
{
  debugTelnet.println(F("\r\n============================================"));
  debugTelnet.println(F("  OpenTherm Gateway -- OTGW-firmware"));
  _debugPrintf_P(PSTR("  FW   : %s #%u  fs:%s\r\n"),
    _VERSION,
    (unsigned)_VERSION_BUILD,
    checklittlefshash() ? "ok" : "mismatch");
  _debugPrintf_P(PSTR("  Host : %s   Up: %s   Reboots: %lu\r\n"),
    settings.sHostname,
    upTime(),
    (unsigned long)state.uptime.iRebootCount);
  debugTelnet.println(F("============================================"));
  const int wifiRssi = WiFi.RSSI();
  _debugPrintf_P(PSTR("  WiFi : %s   RSSI %d dBm (%d%% %S)   IP %s\r\n"),
    WiFi.SSID().c_str(),
    wifiRssi,
    signal_quality_perc_quad(wifiRssi),
    (PGM_P)dBmtoQuality(wifiRssi),
    WiFi.localIP().toString().c_str());
  _debugPrintf_P(PSTR("  Heap : free %u  frag %u%%  minFree %u  maxBlk %u\r\n"),
    (unsigned)ESP.getFreeHeap(),
    (unsigned)ESP.getHeapFragmentation(),
    (unsigned)getMinFreeHeap(),
    (unsigned)ESP.getMaxFreeBlockSize());
  _debugPrintf_P(PSTR("  Drops: ws %lu  mqtt %lu   low/warn/crit %lu/%lu/%lu   slow %lu\r\n"),
    (unsigned long)state.heapdiag.iWsDropsTotal,
    (unsigned long)state.heapdiag.iMqttDropsTotal,
    (unsigned long)state.heapdiag.iEnteredLowCount,
    (unsigned long)state.heapdiag.iEnteredWarningCount,
    (unsigned long)state.heapdiag.iEnteredCriticalCount,
    (unsigned long)state.heapdiag.iDripSlowModeCount);
  debugTelnet.println(F("--------------------------------------------"));
  _debugPrintf_P(PSTR("  PIC  : %s  v%s   id %s\r\n"),
    state.pic.sType, state.pic.sFwversion, state.pic.sDeviceid);
  _debugPrintf_P(PSTR("  OTGW : %s   GW-mode: %s   Boiler: %s  Thermostat: %s   PS: %s\r\n"),
    state.otgw.bOnline ? "online" : "offline",
    state.otgw.bGatewayModeKnown ? CCONOFF(state.otgw.bGatewayMode) : "detecting",
    CCONOFF(state.otgw.bBoilerState),
    CCONOFF(state.otgw.bThermostatState),
    CCONOFF(state.otgw.bPSmode));
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
  debugTelnet.println(F("  Debug toggles (press key to flip):"));
  _debugPrintf_P(PSTR("    1 OTmsg     [%s]    2 REST API  [%s]    3 MQTT      [%s]\r\n"),
    state.debug.bOTmsg    ? "1" : "0",
    state.debug.bRestAPI  ? "1" : "0",
    state.debug.bMQTT     ? "1" : "0");
  _debugPrintf_P(PSTR("    4 MQTTGate  [%s]    5 Sensors   [%s]    6 NTP       [%s]\r\n"),
    state.debug.bMQTTGate ? "1" : "0",
    state.debug.bSensors  ? "1" : "0",
    state.debug.bNTP      ? "1" : "0");
  _debugPrintf_P(PSTR("    d SensorSim [%s]    OTGW-Sim    [%s]\r\n"),
    state.debug.bSensorSim       ? "1" : "0",
    state.debug.bOTGWSimulation  ? "1" : "0");
  debugTelnet.println(F("--------------------------------------------"));
  debugTelnet.println(F("  Press 'h' for command menu, 'D' for full INI dump."));
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
  // Restore hostname in case configTime() reset it. The correct hostname will
  // be sent to the router on the next DHCP exchange (renewal or reconnect).
  // No SDK DHCP calls here either: the SDK manages the DHCP client autonomously
  // as long as user code never invokes wifi_station_dhcpc_start/stop. See
  // TASK-432 (and the historical analysis at
  // docs/reviews/2026-04-07_issue-525-sdk-dhcp-analysis/ANALYSIS_REPORT.md) for
  // why ANY manual DHCP call (even while not connected) takes ownership away
  // from the SDK and breaks subsequent SDK auto-reconnect DHCP behaviour.
  WiFi.hostname(CSTR(settings.sHostname));
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

// ADR-064 (TASK-350): signature takes pre-computed flags from the single
// dispatcher in doTaskMinuteChanged. Internal dayChanged/yearChanged calls
// are removed so each helper has exactly ONE call site firmware-wide.
void sendtimecommand(bool dayFlag, bool yearFlag){
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

  if (dayFlag){
    //Send msg id 21: month, day
    snprintf_P(msg, sizeof(msg), PSTR("SR=21:%d,%d"), myTime.month(), myTime.day());
    addOTWGcmdtoqueue(msg, strlen(msg), true, 0);
  }

  if (yearFlag){
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
