# Review Scope

## Target

Merge commit of dev branch into feature-dev-2.0.0-otgw32-esp32-sat-support branch.
This merge integrates ESP8266 heap pressure fixes, async bitmap-driven MQTT discovery drip publisher,
PROGMEM-based MQTT HA config index, nightly restart scheduling, and related optimizations into the
2.0.0 multi-platform (ESP8266/ESP32) firmware branch.

The merge also required conflict resolution across core firmware files, adding missing forward
declarations, struct members, and fixing type name mismatches between the two branches.

## Files

Source files changed in the merge (11 files, 2770 insertions, 194 deletions):

- `src/OTGW-firmware/MQTTstuff.ino` - MQTT discovery drip publisher, PROGMEM index, heap guards (432 lines changed)
- `src/OTGW-firmware/OTGW-Core.ino` - Non-blocking setMQTTConfigPending() integration (24 lines changed)
- `src/OTGW-firmware/OTGW-firmware.h` - Forward declarations, MQTTautoCfgPendingMap, nightly restart settings (7 lines added)
- `src/OTGW-firmware/OTGW-firmware.ino` - loopMQTTDiscovery() in main loop, nightly restart logic (18 lines added)
- `src/OTGW-firmware/data/index.js` - Web UI nightly restart labels (4 lines added)
- `src/OTGW-firmware/mqttha_progmem.cpp` - Generated PROGMEM MQTT HA config tables (2145 lines, new file)
- `src/OTGW-firmware/mqttha_progmem.h` - PROGMEM index header (36 lines, new file)
- `src/OTGW-firmware/restAPI.ino` - REST API nightly restart settings exposure (4 lines changed)
- `src/OTGW-firmware/sensors_ext.ino` - Minor sensor changes (6 lines changed)
- `src/OTGW-firmware/settingStuff.ino` - Nightly restart settings persistence (5 lines added)
- `tools/generate_mqttha_progmem.py` - Build tool: generates PROGMEM tables from mqttha.cfg (283 lines, new file)

## Project Context

- **Platform**: ESP8266 (NodeMCU/Wemos D1 mini, ~40KB usable RAM) + ESP32-S3 (OTGW32)
- **Language**: Arduino C/C++ (.ino files), single translation unit per platform
- **Critical constraints**: PROGMEM mandatory for strings, no String class in hot paths, no ArduinoJson,
  stream files (never load into RAM), HTTP/WS only (no HTTPS), cooperative scheduling with feedWatchDog()
- **Serial**: Reserved for PIC communication, debug via Telnet port 23

## Flags

- Security Focus: no
- Performance Critical: yes (ESP8266 heap pressure is the primary motivation for these changes)
- Strict Mode: no
- Framework: Arduino/ESP8266+ESP32
- Relevant ADRs: ADR-004 (no String in hot paths), ADR-040 (MQTT discovery), ADR-051 (Settings/State)

## Review Phases

1. Code Quality & Architecture
2. Security & Performance
3. Testing & Documentation
4. Best Practices & Standards
5. Consolidated Report
