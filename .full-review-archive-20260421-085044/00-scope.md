# Review Scope

## Target

All changes since the v1.3.2 release to current HEAD on the `dev` branch. This covers the v1.3.3 release and ongoing v1.3.4-beta development. 28 source files changed with 309 insertions and 125 deletions.

## Key Commits

- `afd77dc` fix: defer MQTT throttle slot update until publish succeeds
- `c3c1184` fix: add tooltips to Debug Info page, rename OTGW Connected to OpenTherm Active, support thermostat-only setups
- `5f5ee4f` feat: Bump version to v1.3.4-beta for development
- `541cb1b` release: v1.3.3
- `008bc6f` Fix gateway mode detection and misleading HA Integration label (#528)
- `ae4487a` fix: hide unsupported OT values from dashboard
- `fad2ce7` Disable all PIC-related functions when no PIC is detected at boot (#522)
- `10f8a59` docs: add ADR-060 PIC Availability Guard Pattern

## Files (Source Code)

### Core Firmware (.ino/.h)
- src/OTGW-firmware/OTGW-firmware.ino
- src/OTGW-firmware/OTGW-firmware.h
- src/OTGW-firmware/OTGW-Core.ino
- src/OTGW-firmware/OTGW-Core.h
- src/OTGW-firmware/MQTTstuff.ino
- src/OTGW-firmware/networkStuff.ino
- src/OTGW-firmware/networkStuff.h
- src/OTGW-firmware/restAPI.ino
- src/OTGW-firmware/settingStuff.ino
- src/OTGW-firmware/jsonStuff.ino
- src/OTGW-firmware/helperStuff.ino
- src/OTGW-firmware/FSexplorer.ino
- src/OTGW-firmware/sensors_ext.ino
- src/OTGW-firmware/outputs_ext.ino
- src/OTGW-firmware/s0PulseCount.ino
- src/OTGW-firmware/webSocketStuff.ino
- src/OTGW-firmware/webhook.ino
- src/OTGW-firmware/version.h

### Frontend (Web UI)
- src/OTGW-firmware/data/index.html
- src/OTGW-firmware/data/index.js
- src/OTGW-firmware/data/index.css
- src/OTGW-firmware/data/index_dark.css
- src/OTGW-firmware/data/graph.js
- src/OTGW-firmware/data/FSexplorer.html
- src/OTGW-firmware/data/FSexplorer.css
- src/OTGW-firmware/data/FSexplorer_dark.css
- src/OTGW-firmware/data/mqttha.cfg
- src/OTGW-firmware/data/version.hash

### Documentation
- docs/adr/ADR-060-pic-availability-guard-pattern.md
- RELEASE_NOTES_1.3.3.md
- README.md

## Flags

- Security Focus: no
- Performance Critical: no
- Strict Mode: no
- Framework: Arduino/ESP8266

## Review Phases

1. Code Quality & Architecture
2. Security & Performance
3. Testing & Documentation
4. Best Practices & Standards
5. Consolidated Report
