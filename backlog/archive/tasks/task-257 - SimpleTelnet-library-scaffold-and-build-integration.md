---
id: TASK-257
title: 'SimpleTelnet: library scaffold and build integration'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 19:44'
updated_date: '2026-04-12 20:50'
labels:
  - telnet
  - build
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Create the SimpleTelnet library as a fully self-contained, publishable Arduino library. The library is vendored in src/libraries/SimpleTelnet/ within this project, but is structurally ready for publication as its own GitHub repository and registration in the Arduino Library Manager and PlatformIO registry.\n\nDirectory layout (Arduino Library Specification 1.5+):\n  src/libraries/SimpleTelnet/\n    README.md\n    LICENSE                     (MIT)\n    library.properties          (Arduino Library Manager)\n    library.json                (PlatformIO registry)\n    keywords.txt                (Arduino IDE syntax highlighting)\n    CHANGELOG.md\n    API.md\n    src/\n      SimpleTelnet.h            (template class + full public API declaration)\n      SimpleTelnet_impl.tpp     (template implementation — included at bottom of .h)\n    examples/\n      StreamingMode/StreamingMode.ino\n      CLIMode/CLIMode.ino\n      DualInstance/DualInstance.ino\n    .github/ISSUE_TEMPLATE/\n      bug_report.md\n      feature_request.md\n\nTemplate implementation strategy:\nC++ template class methods cannot be compiled separately into a .cpp file without explicit instantiation. For an Arduino library this means either: (a) header-only (all impl in .h), or (b) .tpp file included at the bottom of the header. Choose (b): SimpleTelnet_impl.tpp holds all method bodies, SimpleTelnet.h ends with #include SimpleTelnet_impl.tpp. This keeps the header readable while allowing normal .h/.cpp browsing.\n\nPlatform support: ESP8266 AND ESP32. All platform-specific code behind #ifdef ARDUINO_ARCH_ESP8266 / ARDUINO_ARCH_ESP32 guards. Library must not include firmware-specific headers.\n\nCallback type: typedef void (*SimpleTelnetCallback)(const char*). Never String.\n\nConfiguration constants (all overridable via #define before include):\n  SIMPLETELNET_LINE_BUF_LEN   128\n  SIMPLETELNET_IP_LEN          16\n  SIMPLETELNET_MAX_WRITE_ERRORS 3\n  SIMPLETELNET_KEEPALIVE_MS  1000\n\nbuild.py: src/libraries/ is already passed via --libraries flag at line 445. No path change needed. Only remove TelnetStream and ESPTelnet from the library download list (lines ~212-225).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 library.properties bevat: name, version=1.0.0, author, maintainer, sentence, paragraph, category=Communication, url, architectures=esp8266,esp32
- [x] #2 LICENSE bestand aanwezig (MIT)
- [x] #3 README.md in het Engels met beschrijving, API-overzicht, en voorbeelden
- [x] #4 src/SimpleTelnet.h en src/SimpleTelnet.cpp aanwezig als compileerbare stubs
- [x] #5 examples/ map met drie voorbeeldschetsen: StreamingMode, CLIMode, DualInstance
- [x] #6 Alle platform-specifieke code achter #ifdef ARDUINO_ARCH_ESP8266 / ESP32 guards
- [x] #7 Geen includes of afhankelijkheden op firmware-specifieke bestanden
- [x] #8 build.py verwijst naar libraries/SimpleTelnet en verwijdert TelnetStream + ESPTelnet entries
- [x] #9 Project compileert na scaffold (stubs, nog geen implementatie)
- [x] #10 Library staat in src/libraries/SimpleTelnet/ (niet in root libraries/ — eigen src-submap voor publiceerbare eigen libraries)
- [x] #11 SimpleTelnet.h includes SimpleTelnet_impl.tpp at the bottom — template methods in .tpp, not in .h body
- [x] #12 All platform-specific code uses #ifdef ARDUINO_ARCH_ESP8266 and #ifdef ARDUINO_ARCH_ESP32 — no other platform detection
- [x] #13 Header includes: ESP8266WiFi.h (ESP8266) or WiFi.h (ESP32) selected via platform guard
- [x] #14 No Arduino.h implicit dependency beyond what WiFi headers pull in
- [x] #15 Library compiles with ONLY Arduino.h + ESP8266WiFi.h (ESP8266) or WiFi.h (ESP32) — no other dependencies
- [x] #16 No includes of project-specific files: no helperStuff.h, no safeTimers.h, no Debug.h, no OTGW headers
- [x] #17 All helper functionality (IP extraction, timing, stream drain) implemented as private methods within the library
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Maak directorystructuur aan: src/libraries/SimpleTelnet/{src/,examples/{StreamingMode,CLIMode,DualInstance}/,.github/ISSUE_TEMPLATE/}
2. Schrijf library.properties (Arduino Library Manager)
3. Schrijf library.json (PlatformIO)
4. Schrijf LICENSE (MIT)
5. Schrijf SimpleTelnet.h met template class stub + #include SimpleTelnet_impl.tpp
6. Schrijf SimpleTelnet_impl.tpp met method stubs (compileert, werkt nog niet)
7. Schrijf drie example sketches als stubs
8. Schrijf keywords.txt
9. Schrijf README.md en CHANGELOG.md stubs
10. Schrijf API.md stub
11. Schrijf GitHub issue templates
12. Controleer build.py op TelnetStream/ESPTelnet entries
13. Verifieer dat project compileert
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Library scaffold complete at src/libraries/SimpleTelnet/.

Created full directory structure per Arduino Library Specification 1.5+:
- src/SimpleTelnet.h with template class declaration and full public API
- src/SimpleTelnet_impl.tpp with initial method stubs (to be completed by TASK-258-261)
- examples/StreamingMode, CLIMode, DualInstance sketch stubs
- library.properties, library.json, LICENSE (MIT), keywords.txt, CHANGELOG.md, API.md, README.md
- .github/ISSUE_TEMPLATE/ bug and feature request templates

Build verified: firmware compiles with scaffold in place.
build.py: TelnetStream and ESP Telnet removed from download list (library replaced by SimpleTelnet in src/libraries/).
<!-- SECTION:FINAL_SUMMARY:END -->
