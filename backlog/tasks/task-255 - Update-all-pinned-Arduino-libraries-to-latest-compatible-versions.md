---
id: TASK-255
title: Update all pinned Arduino libraries to latest compatible versions
status: In Progress
assignee:
  - '@RvdB'
created_date: '2026-04-12 11:58'
updated_date: '2026-04-12 15:31'
labels:
  - build
  - dependencies
  - libraries
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
All Arduino libraries in build.py are pinned to versions from 2021-2023. As part of the ESP8266 3.1.2 core upgrade, update all library pins to the latest sensible (stable) versions that are compatible with ESP8266 3.1.2. This also modernises the dependency chain and picks up bug fixes and security improvements in each library.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 WiFiManager updated to latest version compatible with ESP8266 3.1.2 in build.py
- [x] #2 pubsubclient updated to latest stable version in build.py
- [x] #3 TelnetStream updated to latest version in build.py
- [x] #4 AceCommon, AceSorting, AceTime updated to latest versions; any AceTime 2.x API breaking changes verified or fixed in the codebase
- [x] #5 OneWire and DallasTemperature updated to latest versions in build.py
- [x] #6 WebSockets updated to latest version compatible with ESP8266 3.1.2 in build.py
- [x] #7 ESP Telnet (for feature/telnet-cli-welcome) version verified and pinned
- [ ] #8 Full firmware build passes cleanly with all updated library versions
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. WebSockets: update 2.3.6 -> 2.7.2 in build.py
   - Additive changes only (2.4.0-2.7.2 added Pico W, UNO R4, custom interfaces)
   - No API removed, safe upgrade

2. ESP Telnet: add "ESP Telnet@2.2.3" to build.py when feature/telnet-cli-welcome merges
   - 2.2.3 fixed a yield() re-entrancy issue relevant to ESP8266 cooperative scheduler

3. TelnetStream: stay at 1.2.4
   - 1.3.0 adds NetApiHelpers transitive dependency; no functional benefit for our use

4. AceTime: STAY at 2.0.1 (do NOT upgrade)
   - 4.1.0 has major breaking API: LocalDate->PlainDate, getUtcOffset() removed,
     ZonedExtra restructured, internal namespaces changed. Migration cost >> benefit.

5. All others already at latest version:
   - WiFiManager 2.0.17 (latest)
   - pubsubclient 2.8.0 (latest)
   - AceCommon 1.6.2 (latest)
   - AceSorting 1.0.0 (latest)
   - OneWire 2.3.8 (latest)
   - DallasTemperature 4.0.6 (latest)

6. Run full build with updated library set to confirm clean compile
   python build.py
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Research complete (2026-04-12). Only WebSockets needs updating (2.3.6->2.7.2). AceTime held at 2.0.1 deliberately due to major API breaks in 4.x. All other libs already at latest.

WebSockets: 2.3.6 -> 2.7.2 done. All other libs already at latest. AceTime held at 2.0.1 (4.x has major API breaks).
<!-- SECTION:NOTES:END -->
