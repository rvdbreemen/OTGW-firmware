---
id: TASK-258
title: 'SimpleTelnet: core connection management engine'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 19:44'
updated_date: '2026-04-12 20:52'
labels:
  - telnet
  - core
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the core of SimpleTelnet. KISS and minimal RAM are the primary design constraints. Every byte counts on ESP8266 (~40KB usable DRAM).\n\nKISS MEMORY RULES:\n  - No dynamic allocation (no new/malloc in normal operation)\n  - Template member arrays sized by MAX_CLIENTS only where truly per-client\n  - Single shared input buffer (not per-client) — saves (MAX_CLIENTS-1)*128 bytes\n  - uint16_t for keepAliveInterval (max 65535ms is sufficient, saves 2 bytes vs int)\n  - No project-specific helpers — millis() for timing, snprintf() for IP, nothing else\n  - No includes beyond Arduino.h + platform WiFi header\n\nINTERNAL STATE — complete member list with sizes:\n  WiFiServer  _server                        // ~20B struct\n  WiFiClient  _clients[MAX_CLIENTS]          // 136B × N\n  bool        _clientActive[MAX_CLIENTS]     // 1B × N\n  char        _ip[MAX_CLIENTS][16]           // 16B × N\n  char        _attemptIp[16]                 // 16B — last rejected IP\n  uint8_t     _writeErrors[MAX_CLIENTS]      // 1B × N\n  uint8_t     _connectedCount                // 1B\n  uint16_t    _port                          // 2B\n  uint16_t    _keepAliveInterval             // 2B (NOT int — saves 2B)\n  uint32_t    _lastKeepAliveCheck            // 4B — millis() snapshot\n  bool        _lineMode                      // 1B\n  char        _newlineCharacter              // 1B\n  bool        _lastWasCR                     // 1B — SINGLE flag, not per-client\n  uint8_t     _inputLen                      // 1B — SINGLE counter, not per-client\n  char        _inputBuf[LINE_BUF_LEN]        // 128B — SINGLE buffer, not per-client\n  SimpleTelnetCallback _onConnect            // 4B function pointer\n  SimpleTelnetCallback _onDisconnect         // 4B\n  SimpleTelnetCallback _onInputReceived      // 4B\n  SimpleTelnetCallback _onConnectionAttempt  // 4B\n  SimpleTelnetCallback _onReconnect          // 4B\n\nMEMORY TOTALS (excluding lwIP socket buffers which are unavoidable):\n  SimpleTelnet<1>: ~353B struct + 136B WiFiClient = ~489B\n  SimpleTelnet<4>: ~489B struct + 544B WiFiClients = ~1033B\n  (vs ESPTelnet: ~300B + String heap allocations + socket)\n  (vs TelnetStream hidden global: ~300B extra + socket)\n\nWHY SINGLE INPUT BUFFER:\n  _inputBuf is only used when _onInputReceived is registered.\n  OTGWstream (MAX_CLIENTS=4) never registers _onInputReceived — buffer unused.\n  debugTelnet (MAX_CLIENTS=1) uses char-mode — buffer not used in char mode either.\n  Line mode with multiple simultaneous typers is not a real use case.\n  Benefit: saves (MAX_CLIENTS-1) * 128 bytes — 384B for MAX_CLIENTS=4.\n\nCONSTRUCTOR:\n  SimpleTelnet(uint16_t port = 23) : _server(port), _port(port)\n  Zero-initialise all arrays. Set _keepAliveInterval = SIMPLETELNET_KEEPALIVE_MS.\n  Set _lineMode = true, _newlineCharacter = newline.\n  Note: WiFiClient default constructor creates an unconnected client — valid.\n\nbegin() OVERLOADS:\n  bool begin(bool checkWiFi = true)\n    checkWiFi: verify WiFi.status()==WL_CONNECTED or softAPIP set\n    ESP8266 softAP check: WiFi.softAPIP().isSet()\n    ESP32 softAP check: WiFi.softAPIP().toString() != "0.0.0.0"\n    server.begin(); server.setNoDelay(true);\n    _lastKeepAliveCheck = millis();  // NOT DECLARE_TIMER_MS — standalone lib\n    Zero all client state. Return false if WiFi check fails.\n  bool begin(uint16_t port, bool checkWiFi = true)\n    _port = port; _server = WiFiServer(port); then call begin(checkWiFi).\n\nloop() — 3 steps, in order:\n  _acceptNewClients();\n  _checkKeepAlive();\n  if (_onInputReceived) _processInput();\n\n_acceptNewClients():\n  if (!_server.hasClient()) return;\n  WiFiClient c = _server.accept();\n  for (uint8_t i = 0; i < MAX_CLIENTS; i++):\n    if (!_clientActive[i]):\n      _connectClient(i, c); return;\n  // All slots full — store attempt IP, fire callback, close\n  _extractIP(c.remoteIP(), _attemptIp);\n  if (_onConnectionAttempt) _onConnectionAttempt(_attemptIp);\n  // Reconnect check (MAX_CLIENTS=1 + same IP = reconnect)\n  if (MAX_CLIENTS == 1 && strcmp(_attemptIp, _ip[0]) == 0 && _clientActive[0]):\n    _disconnectClient(0, false);\n    _connectClient(0, c);\n    if (_onReconnect) _onReconnect(_ip[0]);\n    return;\n  c.stop();\n\n_connectClient(idx, WiFiClient& c):\n  _clients[idx] = c;  // WiFiClient copy is valid on both platforms\n  _clients[idx].setNoDelay(true);\n  _clients[idx].setTimeout(_keepAliveInterval);\n  _extractIP(_clients[idx].remoteIP(), _ip[idx]);\n  _clientActive[idx] = true;\n  _writeErrors[idx] = 0;\n  _connectedCount++;\n  _drainClient(idx);  // flush+drain WITHOUT delay(50)\n  if (_onConnect) _onConnect(_ip[idx]);\n\n_disconnectClient(idx, bool trigger):\n  _drainClient(idx);\n  _clients[idx].stop();\n  if (trigger && _onDisconnect) _onDisconnect(_ip[idx]);\n  _ip[idx][0] = 0;\n  _clientActive[idx] = false;\n  if (_connectedCount > 0) _connectedCount--;\n\n_drainClient(idx):  // replaces ESPTelnet emptyClientStream() WITHOUT delay(50)\n  #ifdef ARDUINO_ARCH_ESP8266\n    _clients[idx].flush(_keepAliveInterval);\n  #else\n    _clients[idx].flush();\n  #endif\n  while (_clients[idx].available()) _clients[idx].read();\n\n_checkKeepAlive():  // pure millis() — no project macros\n  if (millis() - _lastKeepAliveCheck < _keepAliveInterval) return;\n  _lastKeepAliveCheck = millis();\n  for each active slot:\n    #ifdef ARDUINO_ARCH_ESP8266\n      if (_clients[i].status() != ESTABLISHED): _disconnectClient(i, true);\n    #else\n      if (!_clients[i].connected()): _disconnectClient(i, true);\n    #endif\n\n_extractIP(IPAddress addr, char* buf):  // private helper, no String\n  snprintf(buf, SIMPLETELNET_IP_LEN, "%d.%d.%d.%d",\n           addr[0], addr[1], addr[2], addr[3]);\n  Works on both ESP8266 and ESP32. IPAddress operator[] is universal.\n\nstop():\n  for each active slot: _disconnectClient(i, true);\n  _server.stop();\n\nNO external helpers. NO project headers. NO safeTimers.h. NO DebugMacros.\nThe library must compile standalone with only Arduino.h + ESP8266WiFi.h or WiFi.h.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Template parameter MAX_CLIENTS controls client-slot array size at compile time
- [x] #2 begin(port) starts WiFiServer and sets up state
- [x] #3 handle() accepts new clients up to MAX_CLIENTS, calls onConnect callback with char* IP
- [x] #4 handle() detects stale connections via keep-alive timer, calls onDisconnect callback
- [x] #5 isConnected() returns true when at least one client is active
- [x] #6 connectedClients() returns count of currently connected clients
- [x] #7 clientIP(idx) returns char* IP for slot idx (no String)
- [x] #8 No global instance auto-created anywhere in library source
- [x] #9 begin(uint16_t port, bool checkWiFi=true) overload ondersteunt ESPTelnet-patroon (port in begin)
- [x] #10 Default constructor SimpleTelnet() werkt met port=23 als default (ESPTelnet-patroon)
- [x] #11 getIP(uint8_t idx=0) aanwezig als alias voor clientIP() — retourneert const char* (implicit conv naar String)
- [x] #12 getLastAttemptIP() aanwezig en gevuld bij afgewezen verbindingspoging
- [x] #13 onReconnect(SimpleTelnetCallback) aanwezig voor ESPTelnet-compatibiliteit
- [x] #14 ESP8266: isConnected() uses client.status() == ESTABLISHED (lwIP TCP state, value 4)
- [x] #15 ESP32: isConnected() uses client.connected() (no status() equivalent)
- [x] #16 ESP8266: server.setNoDelay(true) in begin() + client.setNoDelay(true) per accept (ref: ESPTelnetBase.cpp begin() + connectClient())
- [x] #17 ESP32: setNoDelay behaviour identical — both platforms support it via WiFiClient
- [x] #18 IP extraction uses snprintf with IPAddress operator[] — works on both platforms (no String, no toString())
- [x] #19 WiFiClient copy assignment (clients[idx] = newClient) is valid on both ESP8266 and ESP32 — WiFiClient is copyable
- [x] #20 server.hasClient() + server.accept() pattern used (ref: ESPTelnetBase.cpp processClientConnection()) — works on both platforms
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented full connection management engine for SimpleTelnet in SimpleTelnet.h and SimpleTelnet_impl.tpp.\n\nChanges:\n- Added begin(uint16_t port, bool checkWiFi=true) overload for ESPTelnet compatibility\n- Fixed ESP32 softAP check: WiFi.softAPIP().toString() != \"0.0.0.0\" (ESP32 has no isSet())\n- begin() now sets _lastKeepAliveCheck = millis() to anchor the timer correctly\n- Renamed all private helpers to match spec: _acceptNewClients, _connectClient, _disconnectClient, _checkKeepAlive, _drainClient\n- _connectClient() sequence: copy, setNoDelay(true), setTimeout, extractIP, mark active, increment count, drain, fire onConnect\n- _disconnectClient(): drain, stop, fire onDisconnect, clear IP, clear active flag, decrement count\n- _drainClient(): ESP8266 uses flush(timeout_ms), ESP32 uses flush(); no delay() anywhere\n- _checkKeepAlive(): ESP8266 uses client.status() == 4 (ESTABLISHED literal), ESP32 uses client.connected()\n- _acceptNewClients(): reconnect logic for MAX_CLIENTS==1 same-IP case: evict old (no event), connect new, fire onReconnect\n- _extractIP() signature changed from WiFiClient& to IPAddress — callers pass .remoteIP() explicitly\n- Added clientIP(uint8_t idx=0) as canonical multi-client IP accessor\n- getIP() retained as ESPTelnet compat alias returning first active slot IP\n- Full Doxygen on all public methods (part of TASK-265 coverage)"
<!-- SECTION:FINAL_SUMMARY:END -->
