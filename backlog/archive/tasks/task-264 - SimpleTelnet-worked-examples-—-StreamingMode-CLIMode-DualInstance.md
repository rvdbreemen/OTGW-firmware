---
id: TASK-264
title: 'SimpleTelnet: worked examples — StreamingMode, CLIMode, DualInstance'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 20:10'
updated_date: '2026-04-12 20:52'
labels:
  - telnet
  - docs
  - examples
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write three complete, working example sketches demonstrating all SimpleTelnet use cases. Examples live in src/libraries/SimpleTelnet/examples/. They are the primary documentation for new users discovering the library.\n\nStreamingMode/StreamingMode.ino:\n  PURPOSE: Serial-to-TCP proxy (equivalent to TelnetStream / OTGWstream use case)\n  SimpleTelnet<4> telnet(25238);  // 4 clients, streaming port\n  setup(): WiFi connect, telnet.begin(false), onConnect callback showing multi-client count\n  loop(): telnet.loop(); if Serial.available() telnet.write(Serial.read());\n          if telnet.available() Serial.write(telnet.read());\n  Comments explain: why begin(false), what MAX_CLIENTS=4 means, broadcast semantics\n  Reference: TelnetStream examples/TelnetStreamEsp8266Test/TelnetStreamEsp8266Test.ino\n             for the Serial-to-telnet proxy pattern\n\nCLIMode/CLIMode.ino:\n  PURPOSE: Interactive CLI terminal with welcome banner (equivalent to ESPTelnet / debugTelnet)\n  SimpleTelnet<1> telnet(23);\n  onConnect callback: sends welcome banner using telnet.println(F("...")) and printf_P()\n  setLineMode(false): char-by-char dispatch\n  onInputReceived: simple switch/case command dispatcher ('h'=help, 'i'=info, etc.)\n  loop(): telnet.loop(); periodically send status via telnet.printf_P(PSTR("uptime: %lu\r\n"), millis()/1000);\n  Comments explain: CLI mode vs line mode, how output and input coexist\n  Reference: ESPTelnet examples/TelnetServerExample/TelnetServerExample.ino\n             and examples/TelnetServerLineMode/TelnetServerLineMode.ino\n\nDualInstance/DualInstance.ino:\n  PURPOSE: Two independent instances on different ports, different modes\n  SimpleTelnet<4> stream(25238);  // streaming proxy\n  SimpleTelnet<1> cli(23);        // interactive CLI\n  setup(): both begin(), cli gets callbacks, stream gets onConnect for logging\n  loop(): stream.loop(); cli.loop(); — both must be called\n  Demonstrates: complete independence, different MAX_CLIENTS, different modes\n  Key comment: 'each SimpleTelnet instance has its own WiFiServer — they do not share state'\n  Reference: ESPTelnet examples/ for callback pattern; TelnetStream for streaming pattern\n\nPLATFORM COMPATIBILITY:\n  All three examples must compile on ESP8266 AND ESP32.\n  Use #ifdef ARDUINO_ARCH_ESP8266 only where platform-specific setup is unavoidable.\n  WiFi connection in examples: use WiFi.begin(ssid, pass) + while(WiFi.status()!=WL_CONNECTED) delay(500);\n  This pattern works on both platforms.\n  Do NOT use ESPTelnet DebugMacros.h — SimpleTelnet has no built-in debug macros.\n  Do NOT use String class — use const char*, F(), PSTR(), snprintf_P() as appropriate.\n\nCOMMENT STYLE:\n  Every non-obvious line gets a comment.\n  Section headers: // ── Setup ──────────────\n  Explain WHY not just WHAT: // begin(false) skips WiFi check — server binds regardless\n  Reference the key API design choices in comments where relevant.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 StreamingMode.ino compileert op ESP8266 en ESP32 zonder errors
- [x] #2 CLIMode.ino compileert op ESP8266 en ESP32 zonder errors
- [x] #3 DualInstance.ino compileert op ESP8266 en ESP32 zonder errors
- [x] #4 Alle voorbeelden hebben uitgebreide inline comments in het Engels
- [x] #5 Voorbeelden tonen WiFi setup, begin(), loop() aanroep, en minstens één callback of stream operatie
- [x] #6 DualInstance toont duidelijk dat twee instanties onafhankelijk werken op verschillende poorten
- [x] #7 Geen gebruik van String class in de voorbeelden (const char* consequent gebruikt)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced three stub example sketches with complete, production-quality working examples.

Changes:
- StreamingMode.ino: Serial-to-TCP proxy for up to 4 simultaneous clients on port 25238. Uses begin(false) for unconditional bind, bidirectional Serial/telnet forwarding, onConnect/onDisconnect callbacks that log client count. Explains broadcast semantics and begin(false) in comments.
- CLIMode.ino: Single-client interactive terminal on port 23 with setLineMode(false) char-by-char dispatch. Commands: h=help, i=info, u=uptime, q=quit, default=echo unknown key. Periodic 30-second status line demonstrates output and input coexisting. Welcome banner via println(F(...)), formatted lines via printf_P(PSTR(...)).
- DualInstance.ino: Two independent instances (stream port 25238 MAX_CLIENTS=4, cli port 23 MAX_CLIENTS=1). Stream connect/disconnect events are cross-logged to the CLI channel demonstrating inter-instance output. CLI commands: h=help, s=status of both instances, k=kick all stream clients, q=quit. Both loop() calls clearly annotated as mandatory.

All examples:
- Compile on ESP8266 and ESP32 (platform guards only where unavoidable)
- Use const char*, F(), PSTR() throughout — no String variables declared
- Every non-obvious line commented explaining WHY
- Standard WiFi.begin()/while loop pattern compatible with both platforms
- All API calls verified against SimpleTelnet.h (begin, loop, setLineMode, connectedCount, getIP, printf_P, printf, println, write, read, available, onConnect, onDisconnect, onInputReceived, disconnectClient, isConnected)
<!-- SECTION:FINAL_SUMMARY:END -->
