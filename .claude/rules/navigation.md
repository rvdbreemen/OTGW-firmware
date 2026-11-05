---
paths:
  - "src/**"
  - "docs/c4/**"
  - "other-projects/**"
---

# Project navigation: what to read before touching code

**Always start with** `docs/c4/c4-context.md` (system overview) and `docs/c4/c4-component.md` (component index).

| Scenario | Read before starting |
|---|---|
| Bug in MQTT publishing | `c4-component-integration-layer.md` + `docs/api/MQTT.md` + `c4-code-mqtt.md` |
| Bug in OT message parsing | `c4-component-opentherm-core.md` + OT spec v4.2 + `c4-code-otgw-core.md` |
| Bug in web UI | `c4-component-web-interface.md` + `c4-code-web-assets.md` |
| New REST endpoint | `c4-component-integration-layer.md` + `c4-code-rest-api.md` |
| New MQTT topic | `docs/api/MQTT.md` + `c4-code-mqtt.md` + relevant ADRs |
| Settings/config change | `c4-component-configuration-state.md` + ADR-051 + `c4-code-settings.md` |
| Network/WiFi/OTA change | `c4-component-network.md` + `c4-code-network.md` |
| SAT/BLE feature | `c4-component-smart-thermostat.md` + `other-projects/SAT-releases-thermo-nova/` + `docs/sat/thermo-nova-spec.md` (see "SAT ground truth" below) |
| ESP32 port/feature | `c4-container.md` + `other-projects/OT-Thing-OTGW32/` + relevant ADRs |
| OpenTherm protocol/message IDs | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` |
| MQTT/REST/WebSocket API change | `docs/api/MQTT.md` + `docs/api/WEBSOCKET_FLOW.md` — breaking silently breaks HA integrations |
| Architecture change | `c4-context.md` + `c4-component.md` + all relevant ADRs + create new ADR |
| New dependency | Relevant ADRs + PROGMEM/RAM budget check + create ADR |

**C4 component files** (all in `docs/c4/`):

| File | Covers |
|---|---|
| `c4-component-opentherm-core.md` | OT protocol, OTGW-Core, OTDirect, PIC serial |
| `c4-component-integration-layer.md` | MQTT, REST API, WebSocket, HA auto-config |
| `c4-component-web-interface.md` | Web UI, FSexplorer, file serving |
| `c4-component-network.md` | WiFi, Ethernet, OTA, mDNS, NTP |
| `c4-component-smart-thermostat.md` | SAT, BLE, simulation mode |
| `c4-component-sensors-hardware.md` | Dallas, S0 pulse counter, OLED |
| `c4-component-configuration-state.md` | Settings persistence, OTGWSettings, OTGWState |

**Code-level docs** (in `docs/c4/`, one per source area): `mqtt`, `network`, `otdirect`, `otgw-core`, `rest-api`, `sat`, `sensors`, `settings`, `utilities`, `web-assets`.

**Reference implementations** in `other-projects/` — read-only, never copy verbatim:

| Directory | When to read |
|---|---|
| `OT-Thing-OTGW32/` | ESP32 port, OTDirect, Ethernet, BLE |
| `SAT-releases-thermo-nova/` | SAT subsystem, BLE protocol |
| `otgw-6.6/` | PIC command set, response timing |
| `otmonitor-6.6/` | How a well-tested client drives the PIC |

**If you cannot name the C4 component that owns the code you are about to change, stop and read c4-context.md first.**
