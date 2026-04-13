---
id: TASK-266
title: 'SimpleTelnet: publicatiebestanden Arduino Library Manager en PlatformIO'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 20:11'
updated_date: '2026-04-12 20:52'
labels:
  - telnet
  - docs
  - publication
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maak alle bestanden aan die nodig zijn voor publicatie in de Arduino Library Manager en het PlatformIO registry. Beide registries hebben eigen formaten maar kunnen coexisteren in dezelfde repository.\n\nArduino Library Manager (library.properties):\n- Verplichte velden: name, version, author, maintainer, sentence, paragraph, category, url, architectures\n- category=Communication\n- architectures=esp8266,esp32\n- sentence: één regel, geen punt aan het eind (Arduino-eis)\n- paragraph: uitgebreider, max 3 regels\n\nPlatformIO registry (library.json):\n- JSON formaat, uitgebreidere metadata\n- keywords array voor vindbaarheid: telnet, esp8266, esp32, stream, cli, multi-client\n- dependencies leeg (geen externe afhankelijkheden)\n- frameworks: arduino\n- platforms: espressif8266, espressif32\n- export.include = src/ voor correcte header-resolving\n\nCHANGELOG.md:\n- Keep-a-Changelog formaat (https://keepachangelog.com)\n- v1.0.0 sectie met initial release beschrijving\n- Vermeldt inspiratiebronnen en wat er verbeterd is\n\n.github/ISSUE_TEMPLATE/:\n- bug_report.md template\n- feature_request.md template\n- Helpt bij community-gebruik na publicatie\n\nKeywords.txt (Arduino IDE syntax highlighting):\n- SimpleTelnet KEYWORD1\n- Alle publieke methoden als KEYWORD2\n- Constanten als LITERAL1
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 library.properties aanwezig met alle verplichte velden correct ingevuld
- [x] #2 library.json aanwezig met correcte PlatformIO-structuur en keywords
- [x] #3 CHANGELOG.md aanwezig in Keep-a-Changelog formaat met v1.0.0 entry
- [x] #4 keywords.txt aanwezig met klasse en methoden voor Arduino IDE syntax highlighting
- [x] #5 .github/ISSUE_TEMPLATE/ aanwezig met bug_report.md en feature_request.md
- [x] #6 library.properties voldoet aan Arduino Library Specification validatie (geen verboden tekens)
- [x] #7 library.json valideert als correcte JSON
- [x] #8 library.properties sentence field: 'A multi-client Telnet library for ESP8266 and ESP32 supporting both CLI and streaming modes' (no trailing period)
- [x] #9 library.json frameworks field is array: ["arduino"] — not a string
- [x] #10 library.json platforms: ["espressif8266", "espressif32"] — exact PlatformIO platform identifiers
- [x] #11 library.json export.include set to 'src' for correct header resolution when used as PlatformIO dependency
- [x] #12 keywords.txt includes: SimpleTelnet KEYWORD1, all public methods as KEYWORD2, all config constants as LITERAL1
- [x] #13 CHANGELOG.md v1.0.0 entry lists: multi-client support, no-String callbacks, printf_P, CLI and streaming modes, ESP8266+ESP32 support
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Verified and corrected all SimpleTelnet publication files for Arduino Library Manager and PlatformIO.

Changes made:
- library.properties: sentence field updated to exact required value without trailing period: 'A multi-client Telnet library for ESP8266 and ESP32 supporting both CLI and streaming modes'
- library.json: frameworks field changed from string "arduino" to array ["arduino"] (PlatformIO spec requires array)
- library.json: keywords changed from comma-separated string to proper JSON array, added "serial" keyword
- All other fields were already correct: platforms=["espressif8266","espressif32"], export.include="src", architectures=esp8266,esp32

Files already correct (no changes needed):
- CHANGELOG.md: Keep a Changelog format, v1.0.0 entry covers all required items
- keywords.txt: all 20 public methods as KEYWORD2, all 4 config constants as LITERAL1, SimpleTelnet as KEYWORD1
- .github/ISSUE_TEMPLATE/bug_report.md and feature_request.md: both present
<!-- SECTION:FINAL_SUMMARY:END -->
