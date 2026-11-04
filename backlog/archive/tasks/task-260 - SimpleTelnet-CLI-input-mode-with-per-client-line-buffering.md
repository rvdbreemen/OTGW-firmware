---
id: TASK-260
title: 'SimpleTelnet: CLI input mode with per-client line buffering'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 19:45'
updated_date: '2026-04-12 20:53'
labels:
  - telnet
  - cli
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement CLI input processing. KISS: single shared buffer, not per-client. Input processing only runs when _onInputReceived is registered.\n\nSINGLE BUFFER DESIGN (KISS + memory):\n  The library has ONE input buffer: char _inputBuf[SIMPLETELNET_LINE_BUF_LEN] and ONE _inputLen.\n  Rationale:\n    - CLI mode (debugTelnet) is always MAX_CLIENTS=1 — per-client buffers waste nothing\n    - Streaming mode (OTGWstream, MAX_CLIENTS=4) never calls _processInput() at all\n    - Two simultaneous CLI typers is not a real scenario\n    - Saves (MAX_CLIENTS-1) * 128 bytes = 384B for MAX_CLIENTS=4\n  ONE _lastWasCR flag (not per-client): same rationale.\n\n_processInput() — only called when _onInputReceived != nullptr:\n  for each active client with available() > 0:\n    while _clients[i].available():\n      char c = (char)_clients[i].read();\n      _lineMode ? _handleLineInput(c) : _handleCharInput(c);\n\n_handleLineInput(char c):\n  c == '\r': _lastWasCR = true; return;\n  c == '\n':\n    _inputBuf[_inputLen] = '\0';\n    _onInputReceived(_inputBuf);\n    _inputLen = 0; _lastWasCR = false; return;\n  if _lastWasCR:  // bare CR not followed by LF — dispatch pending, continue\n    _inputBuf[_inputLen] = '\0';\n    _onInputReceived(_inputBuf);\n    _inputLen = 0; _lastWasCR = false;\n  c == 0x08 || c == 0x7F: if _inputLen > 0: _inputLen--; return;\n  c == 0x07: return;  // bell — ignore\n  c >= 0x80: return;  // non-ASCII incl. telnet IAC (0xFF) — ignore in line mode\n  c >= 0x20 && c < 0x7F:  // printable\n    if _inputLen < LINE_BUF_LEN-1: _inputBuf[_inputLen++] = c;\n    // else: silent truncation, no crash\n\n_handleCharInput(char c):\n  // Matches ESPTelnet char mode: dispatch every byte immediately\n  // Ref: ESPTelnet.cpp handleInput() non-lineMode branch\n  char buf[2] = {c, '\0'};\n  _onInputReceived(buf);  // const char*, no String allocation\n\nWHEN _processInput() IS NOT CALLED:\n  If _onInputReceived == nullptr, _processInput() is skipped entirely.\n  Incoming bytes stay in TCP rx buffer. Accessible via read() / available().\n  This is streaming mode — pure byte access, zero overhead.\n\nMEMORY IMPACT OF THIS DESIGN:\n  _inputBuf: 128B (single, always present as template member)\n  _inputLen: 1B\n  _lastWasCR: 1B\n  Total input state: 130B regardless of MAX_CLIENTS.\n  Vs per-client design for MAX_CLIENTS=4: 4*(128+1+1) = 520B.\n  Saving: 390B.\n\nNO String objects anywhere in the input path.\nNO project-specific helpers or macros.\nAll char operations use standard C string functions only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Per-client char[] input buffer, no String objects used anywhere in input path
- [x] #2 setLineMode(false): each incoming byte immediately fires onInputReceived with single char
- [x] #3 setLineMode(true): bytes accumulated until newline, then dispatched as complete line
- [x] #4 Backspace (0x08) correctly removes last character from line buffer
- [x] #5 CR+LF sequence dispatches exactly one callback event (not two)
- [x] #6 onInputReceived callback signature uses const char*, not String
- [x] #7 Buffer overflow (>128 chars) truncates gracefully, no crash
- [x] #8 debugTelnet use case: single char dispatch to handleDebug command dispatcher works correctly
- [x] #9 ESP8266 and ESP32: input processing identical — WiFiClient.read() and available() work the same on both platforms
- [x] #10 IAC bytes (0xFF+) silently ignored in line mode — prevents telnet protocol negotiation from corrupting line buffer
- [x] #11 char mode passes ALL bytes to callback including control chars — consistent with ESPTelnet char mode (ESPTelnet.cpp handleInput() non-lineMode branch)
- [x] #12 _processInput() is a no-op when _onInputReceived is nullptr — incoming bytes remain in TCP buffer for read() access (streaming mode)
- [x] #13 Reference: ESPTelnet.cpp handleInput() for line-mode and char-mode logic; TelnetStream.cpp read() for polling approach
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented CLI input mode for SimpleTelnet.\n\nChanges:\n- loop() now only calls _processInput() when _onInput != nullptr — bytes stay in TCP buffer in streaming mode\n- _processInput(): reads all available bytes from all active clients, dispatches to _handleLineInput or _handleCharInput based on _lineMode flag\n- _handleLineInput(char c): precise CR/LF handling:\n    - '\\r': set _lastWasCR = true, return (hold — wait for possible LF)\n    - '\\n' after '\\r': fire callback (deferred from CR), reset, return\n    - '\\n' bare: fire callback, reset\n    - any non-LF after pending CR: fire pending line, reset, then process c normally\n    - 0x08 or 0x7F: backspace — decrement _inputLen, no echo\n    - 0x07: bell — silently ignored\n    - >= 0x80: high byte (including telnet IAC 0xFF) — silently ignored, prevents negotiation corruption\n    - 0x20..0x7E: printable — append with silent truncation at SIMPLETELNET_LINE_BUF_LEN-1\n- _handleCharInput(char c): char buf[2] = {c, '\\0'}; _onInput(buf); — no String, no heap\n- Single shared _inputBuf/len/_lastWasCR (not per-client) — saves 390B for MAX_CLIENTS=4\n- Streaming mode (no _onInput): _processInput() never entered, bytes untouched in TCP buffer"
<!-- SECTION:FINAL_SUMMARY:END -->
