---
id: TASK-259
title: 'SimpleTelnet: Stream interface — broadcast write and polling read'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 19:45'
updated_date: '2026-04-12 20:52'
labels:
  - telnet
  - stream
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the Arduino Stream interface for SimpleTelnet. write() broadcasts to ALL connected clients simultaneously. available()/read()/peek() poll across client slots to find the first one with data. flush() handles ESP8266/ESP32 platform differences.\n\nwrite(uint8_t b):\n  if _connectedCount == 0: return 0\n  size_t ok = 0;\n  for i in 0..MAX_CLIENTS-1:\n    if _clientActive[i]:\n      if _clients[i].write(b) == 0:\n        _onWriteError(i)\n      else:\n        _writeErrors[i] = 0; ok++;\n  return (ok > 0) ? 1 : 0;\n  // Return value: 1 if at least one client received it, 0 if all failed.\n  // NOT the count of clients — Stream contract expects bytes written, not client count.\n\nwrite(const uint8_t* buf, size_t len):\n  Same pattern as write(uint8_t) but calls client.write(buf, len).\n  Return len if at least one client received it, 0 if all failed.\n  Note: client.write(buf, len) on ESP8266 returns size_t bytes written.\n  If written != len: increment write error counter.\n\n_onWriteError(idx):\n  _writeErrors[idx]++;\n  if (_writeErrors[idx] >= SIMPLETELNET_MAX_WRITE_ERRORS):\n    _writeErrors[idx] = 0;\n    _disconnectClient(idx, true);\n\navailable():\n  for i in 0..MAX_CLIENTS-1:\n    if _clientActive[i]:\n      int n = _clients[i].available();\n      if n > 0: return n;   // return first non-zero, not sum\n  return 0;\n  // Rationale: returning sum could give misleading total across clients.\n  // Callers typically loop: while(available()) read(). First-client approach\n  // is consistent and avoids interleaving bytes from different clients.\n\nread():\n  for i in 0..MAX_CLIENTS-1:\n    if _clientActive[i] && _clients[i].available() > 0:\n      return _clients[i].read();\n  return -1;  // Stream contract: -1 when no data\n\npeek():\n  Same as read() but calls client.peek() without consuming.\n  Return -1 if no client has data.\n\nflush():\n  for i in 0..MAX_CLIENTS-1:\n    if _clientActive[i]:\n      #ifdef ARDUINO_ARCH_ESP8266\n        // ESP8266 WiFiClient::flush(timeout_ms) returns bool\n        // timeout = keepAliveInterval to avoid blocking indefinitely\n        if (!_clients[i].flush(_keepAliveInterval)):\n          _onWriteError(i);  // flush failure counts as write error\n      #else\n        _clients[i].flush();  // ESP32: void return\n      #endif\n\nNOTE on write() vs onInputReceived interaction:\n  write() always broadcasts to all clients, regardless of whether onInputReceived\n  is registered. The write path and the input-processing path are completely\n  independent. A client connected in CLI mode still receives all write() output.\n\nNOTE on server.write() alternative:\n  TelnetStream uses server.write() for broadcast. This is simpler but gives no\n  per-client feedback. We choose explicit client iteration for write-error tracking\n  and future per-client filtering capability. The behavior is equivalent.\n\nNOTE on WiFiClient validity check:\n  Use _clientActive[i] as the guard, NOT (!_clients[i]) or similar.\n  WiFiClient's bool operator is unreliable in some edge cases on ESP8266.\n  The _clientActive flag is explicitly managed by _connectClient/_disconnectClient.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 write(uint8_t) broadcasts to all connected clients, returns bytes written
- [x] #2 write(const uint8_t* buf, size_t len) broadcasts buffer to all connected clients
- [x] #3 available() returns number of bytes ready to read from any connected client
- [x] #4 read() returns next byte from first client with data, -1 if none
- [x] #5 peek() returns next byte without consuming it, -1 if none
- [x] #6 flush() calls ESP8266-specific client.flush(timeout) per connected client
- [x] #7 Failed write tracking disconnects misbehaving client after MAX_ERRORS_ON_WRITE
- [x] #8 OTGWstream use case: two simultaneous telnet clients both receive all PIC serial output
- [x] #9 ESP8266: flush() calls client.flush(timeout_ms) which returns bool — failure triggers write error (ref: ESPTelnetBase.cpp flush())
- [x] #10 ESP32: flush() calls client.flush() with no timeout parameter — void return, no error detection possible
- [x] #11 write() implementation uses explicit client iteration (not server.write()) for per-client error tracking — unlike TelnetStream which uses server.write()
- [x] #12 Both platforms: WiFiClient.write(buf, len) returns size_t bytes written — 0 indicates failure
- [x] #13 KISS: write() loops over _clients[N] directly — no intermediate buffers, no heap allocation
- [x] #14 KISS: available() returns first non-zero result — simple loop, no accumulation
- [x] #15 No dependency on project helpers — all Stream methods self-contained
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented Arduino Stream interface for SimpleTelnet.\n\nChanges:\n- write(uint8_t): returns 1 if at least one client received it, 0 if all failed — correct Stream contract semantics\n- write(buf, len): returns len on success, 0 if all failed — same rationale\n- Both write() methods use _clientActive[i] guard (not WiFiClient bool operator, unreliable on ESP8266)\n- _onWriteError(idx): private helper increments counter; evicts at >= SIMPLETELNET_MAX_WRITE_ERRORS\n- available(): returns first non-zero result (not sum) — consistent polling semantics\n- read(): first client with available data; returns -1 when none\n- peek(): same as read() but non-consuming\n- flush(): ESP8266 calls client.flush(_keepAliveInterval) returning bool — failure triggers _onWriteError(i); ESP32 calls client.flush() void — no error detection possible\n- All methods guard with _clientActive[i], never with WiFiClient bool operator"
<!-- SECTION:FINAL_SUMMARY:END -->
