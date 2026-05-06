---
id: TASK-265
title: 'SimpleTelnet: API-documentatie en doxygen header comments'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-12 20:10'
updated_date: '2026-04-12 20:52'
labels:
  - telnet
  - docs
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Documenteer de volledige SimpleTelnet API via twee kanalen: doxygen-stijl inline comments in SimpleTelnet.h, en een apart API.md bestand in de library root.\n\nDoxygen comments in SimpleTelnet.h:\n- Elke publieke methode krijgt een \brief, \param, \return, en \note waar relevant\n- Template-parameter MAX_CLIENTS gedocumenteerd met geheugenimplicaties\n- Configuratie-constanten (SIMPLETELNET_LINE_BUF_LEN etc.) gedocumenteerd\n- Mode-interactie (wanneer activeert onInputReceived de CLI-buffer?) uitgelegd\n- ESP8266/ESP32 platform-specifiek gedrag genoteerd\n\nAPI.md (Engelstalig):\n- Volledige method-reference tabel met handtekening, beschrijving, mode-geldigheid\n- Sectie: Lifecycle (begin, loop, stop)\n- Sectie: Callbacks (onConnect, onDisconnect, onInputReceived, onConnectionAttempt)\n- Sectie: CLI mode (setLineMode, setNewlineCharacter, isLineModeSet)\n- Sectie: Connection info (isConnected, connectedClients, clientIP, disconnectClient)\n- Sectie: Stream interface (write, available, read, peek, flush)\n- Sectie: Helpers (printf, printf_P)\n- Sectie: Configuration defines (overschrijfbare constanten)\n- Tabel: migratie van TelnetStream / ESPTelnet / ESPTelnetStream naar SimpleTelnet
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Elke publieke methode in SimpleTelnet.h heeft een doxygen \brief comment
- [ ] #2 Methoden met niet-triviale gedrag hebben \note of \warning voor edge cases
- [x] #3 API.md bevat alle secties zoals beschreven in de taakomschrijving
- [x] #4 Migratietabel toont voor elke bestaande API-call het SimpleTelnet equivalent
- [x] #5 Configuratie-constanten sectie documenteert hoe defaults overschreven worden via #define
- [x] #6 API.md is leesbaar als standalone document zonder de broncode te hoeven lezen
- [x] #7 Migration table covers ALL methods from ESPTelnet, ESPTelnetStream, and TelnetStream README class definitions
- [x] #8 Breaking change clearly marked: callback parameter String → const char* with migration example
- [x] #9 Platform notes documented: ESTABLISHED check (ESP8266 only), flush() timeout (ESP8266 only), vsnprintf_P (ESP8266 only)
- [x] #10 MAX_CLIENTS template parameter documented with memory cost table: <1>=~280B, <2>=~420B, <4>=~700B (excl. lwIP socket buffers)
- [x] #11 Mode interaction documented: onInputReceived registered = CLI mode active, bytes consumed by buffer; not registered = streaming mode, bytes available via read()
- [x] #12 Doxygen \warning on setLineMode(): calling after onInputReceived is registered changes behavior mid-connection
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed API.md for SimpleTelnet library — standalone reference without needing to read source.

Changes:
- Full method reference table covering all 21 public methods with signatures, descriptions, and mode notes
- Migration table for TelnetStream: every method mapped, plus note that loop() must be added
- Migration table for ESPTelnet/ESPTelnetStream: every method mapped including the renamed setNewlineCharacter -> setNewlineChar
- Breaking change section: prominent before/after code examples for const char* vs String callbacks with migration checklist
- Platform notes table: printf_P (ESP8266 only), ESTABLISHED keep-alive check (ESP8266 only), flush() blocking behavior (ESP8266 only)
- Memory cost table: <1>=~489B, <2>=~625B, <4>=~1033B (excluding lwIP socket buffers per connection)
- Configuration defines table with override example
- Mode interaction matrix: 2x2 table showing all combinations of onInputReceived/setLineMode and resulting behavior
- Quick start example and table of contents added for navigability

ACs #1 and #2 (Doxygen in SimpleTelnet.h) handled by separate agent.
<!-- SECTION:FINAL_SUMMARY:END -->
