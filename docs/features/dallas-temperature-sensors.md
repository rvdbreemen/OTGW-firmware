# Dallas Temperature Sensors

## Overview

The OTGW firmware supports **Dallas DS18x20 family** temperature sensors (DS18B20, DS18S20, DS1822) connected via a OneWire bus on a configurable GPIO pin. Sensors are auto-discovered on the bus, published via MQTT with Home Assistant Auto Discovery, displayed in the Web UI with editable labels, and plotted in the real-time temperature graph.

Up to **16 sensors** can be connected simultaneously on a single OneWire bus.

## Hardware Setup

### Wiring

Dallas sensors use the **OneWire** protocol, requiring only a single data line plus power and ground. Connect sensors to the ESP8266 as follows:

```
         ESP8266                      DS18B20
        ┌────────┐                   ┌────────┐
        │     3V3 ├───┬──────────────┤ VDD    │
        │         │   │              │        │
        │         │  [R] 4.7kΩ       │        │
        │         │   │              │        │
        │  GPIO10 ├───┴──────────────┤ DQ     │
        │         │                  │        │
        │     GND ├──────────────────┤ GND    │
        └────────┘                   └────────┘
```

- **VDD** → 3.3V
- **DQ** (data) → GPIO10 (default) with a **4.7kΩ pull-up resistor** to 3.3V
- **GND** → Ground

#### Multiple sensors

All sensors share the same bus — connect them in parallel to the same data line. Each sensor has a unique 64-bit address burned in at the factory, so they are individually addressable.

```
  3.3V ──┬──────────┬──────────┬──
         │          │          │
        [4.7kΩ]     │          │
         │          │          │
  DQ  ───┴──┤S1├────┤S2├────┤S3├──
             │          │          │
  GND ──────┴──────────┴──────────┴──
```

#### Cable length considerations

| Cable Length | Pull-up Resistor | Notes |
|---|---|---|
| < 5m | 4.7kΩ (standard) | Works reliably |
| 5–20m | 3.3kΩ–4.7kΩ | Test for reliability |
| 20–50m | 2.2kΩ | Lower resistor value compensates for cable capacitance |
| > 50m | Active pull-up circuit | Not recommended for this application |

### Default GPIO Pin

The default pin is **GPIO10** (labelled **SD3** on most NodeMCU/Wemos D1 mini boards). This pin was chosen because it is not used by the OTGW serial communication or other core functions on the Nodo board.

> **Note:** Changing the GPIO pin in settings requires a **reboot** before the new pin takes effect.

## Settings

Configure Dallas sensors in the Web UI under **Settings**, or via the REST API at `POST /api/v1/otgw/settings`.

| Setting | JSON Key | Default | Description |
|---|---|---|---|
| GPIO Sensors Enabled | `GPIOSENSORSenabled` | `false` | Master switch — enables OneWire bus scanning and sensor polling |
| GPIO pin # | `GPIOSENSORSpin` | `10` | GPIO pin for the OneWire data line (SD3 = GPIO10) |
| GPIO Publish Interval | `GPIOSENSORSinterval` | `20` | Polling interval in seconds — how often sensors are read |
| GPIO Sensors Legacy Format | `GPIOSENSORSlegacyformat` | `false` | Use legacy (pre-v1.0) truncated sensor address format |

### Enabling sensors

1. Connect your Dallas sensor(s) to the configured GPIO pin with a pull-up resistor.
2. Open the Web UI → **Settings**.
3. Check **"GPIO Sensors Enabled"**.
4. Verify the GPIO pin number matches your wiring (default: 10).
5. Click **Save** and **reboot** the device.

After reboot, discovered sensors appear in the OTmonitor table on the main page.

### Poll interval

The poll interval (default 20 seconds) controls how frequently the firmware reads temperatures from all sensors and publishes them via MQTT. Lower values give faster updates but increase bus traffic and MQTT messages. A reasonable range is 10–60 seconds.

### Legacy address format

In firmware versions prior to v1.0, a bug produced truncated sensor addresses (e.g., `2FE7983B8` instead of the correct 16-character `28F0E979970803B8`). If you are upgrading from an older version and have existing Home Assistant automations that reference the old IDs, enable **"GPIO Sensors Legacy Format"** to preserve backward compatibility.

For new installations, leave this setting **off** (default).

## Sensor Discovery and Data Flow

### Startup sequence

```
  Boot
   │
   ▼
  initSensors()
   │
   ├─ Sensors disabled & no simulation? → return (no-op)
   │
   ├─ Simulation enabled? → initSimulatedDallasSensors() → return
   │
   └─ Real hardware:
       ├─ oneWire.begin(pin)
       ├─ sensors.begin()
       ├─ Scan bus → numberOfDevices
       ├─ Cap at MAXDALLASDEVICES (16)
       ├─ For each device:
       │   ├─ Read address into DallasrealDevice[i].addr
       │   └─ Initialize tempC = 0, lasttime = 0
       └─ No devices found? → disable sensors, log error
```

### Polling loop

Every `settingGPIOSENSORSinterval` seconds (default 20), the main loop calls `pollSensors()`:

1. `sensors.requestTemperatures()` — sends a convert command to all sensors on the bus.
2. For each discovered sensor:
   - Read temperature via `sensors.getTempC(addr)`.
   - Store in `DallasrealDevice[i].tempC` and update timestamp.
   - If MQTT is enabled, publish temperature (formatted to 1 decimal place) to `<topic>/value/<node_id>/<sensor_address>`.
3. MQTT Auto Discovery is triggered on first poll (and repeated if Home Assistant restarts).

## Web UI Features

### OTmonitor Table

Discovered sensors appear in the main OTmonitor table with their 16-character hex address. Temperature values are displayed with **1 decimal place** precision (e.g., `35.5°C`).

### Editable Labels

Click on any Dallas sensor name in the OTmonitor table to open an **inline editor**:

- A text input replaces the sensor name, pre-filled with the current label.
- Type a custom name (max **16 characters**) — e.g., "Living Room" or "Boiler Return".
- Press **Enter** to save, or **Escape** to cancel.
- Clicking away (blur) auto-saves if the label changed.
- A pencil icon (✏️) indicates editable fields.

Labels are stored on the device filesystem in `/dallas_labels.ini` as a JSON object:

```json
{
  "28FF64D1841703F1": "Living Room",
  "28D0000000000002": "Boiler Return",
  "28D0000000000003": "Hot Water"
}
```

The file uses **zero backend RAM** — labels are streamed directly from LittleFS on read and written in one operation on save.

### Labels REST API

| Method | Endpoint | Description |
|---|---|---|
| GET | `/api/v1/sensors/labels` | Returns all labels as a JSON object. Returns `{}` if no labels file exists. |
| POST | `/api/v1/sensors/labels` | Replaces all labels. Request body must be valid JSON. Returns `{"success": true}`. |

The frontend uses a **read-modify-write** pattern: it fetches all labels, updates the one being edited, and POSTs the full set back. This avoids per-sensor endpoints and keeps the backend simple.

### Temperature Graph

Dallas sensors are **automatically added** to the real-time temperature graph (the bottom panel alongside boiler/room/setpoint temperatures).

- Sensors are detected from the API response by matching 16-character hex addresses starting with `28`, `10`, or `22`.
- Each sensor gets a unique color from a palette of **16 colors** (both light and dark themes).
- The graph legend dynamically updates to include all discovered sensors.
- If a sensor has a custom label, that label appears in the legend (updated live after label changes).

### Label Backup and Restore on Filesystem Flash

When performing a **filesystem flash** (OTA update of the LittleFS partition), the Web UI:

1. **Before flash**: Downloads both `settings.ini` and `dallas_labels.ini` as backup files to the user's browser (timestamped filenames). The labels backup is non-fatal — if the file doesn't exist, the flash proceeds normally.
2. **After reboot**: The OTA completion page automatically **restores labels** from the parent window's in-memory cache (`dallasLabelsCache`) by POSTing them back to `/api/v1/sensors/labels`. This happens after a 5-second stabilization delay.

> **Note:** `settings.ini` is auto-restored from ESP memory (it persists across filesystem flashes). Dallas labels require the active restore because they only exist on the filesystem.

## MQTT Integration

### Publishing

Each sensor publishes its temperature on every poll cycle:

- **Topic**: `<mqtt-prefix>/value/<node-id>/<sensor-address>`  
  Example: `OTGW/value/otgw-firmware/28FF64D1841703F1`
- **Payload**: Temperature in °C, 1 decimal place  
  Example: `35.5`

### Home Assistant Auto Discovery

On first detection (and on HA restart), the firmware publishes MQTT discovery configs so sensors appear automatically in Home Assistant. The discovery uses data ID `246` as a virtual OpenTherm message ID for all Dallas sensors.

Discovery messages are generated from the `/mqttha.cfg` template file on LittleFS, with placeholders replaced at runtime (`%sensor_id%`, `%hostname%`, `%node_id%`, etc.).

## Debug Simulation Mode

### Purpose

The simulation mode creates **3 virtual Dallas sensors** without any physical hardware. This is useful for:

- **Developing and testing** the Web UI (labels, graph, table display) without sensors connected.
- **Verifying MQTT integration** and Home Assistant discovery without hardware.
- **Demonstrating** the sensor feature.
- **Debugging** label save/load, graph rendering, and OTA backup/restore flows.

### Activation

Connect to the device via **Telnet** (port 23) and press `d` to toggle simulation:

```
--- Debug Toggles ---
1) Toggle debuglog - OT msg handeling: ON
2) Toggle debuglog - API handeling: OFF
3) Toggle debuglog - MQTT module: OFF
4) Toggle debuglog - Sensor modules: OFF
d) Toggle debug helper - Dallas sensor simulation: OFF
```

Pressing `d` toggles `bDebugSensorSimulation` and calls `initSensors()` to immediately activate or deactivate the virtual sensors. **No reboot required.** The simulation works even if `GPIOSENSORSenabled` is `false` in settings.

### Virtual Sensors

| Sensor | Address | Initial Temp |
|---|---|---|
| 0 | `28D0000000000001` | 30.0°C |
| 1 | `28D0000000000002` | 35.0°C |
| 2 | `28D0000000000003` | 40.0°C |

### Behavior

- Temperatures **drift randomly** by ±0.5°C per update step (random walk).
- Values are **clamped** to the range **20.0°C – 60.0°C**.
- Updates occur every **10 seconds** (independent of the configured poll interval).
- Sensors appear in the OTmonitor table, graph, and MQTT — identical to real hardware.
- Debug logging for simulated sensors is gated behind the `bDebugSensors` flag (press `4` in telnet) to reduce telnet output.

### Simulation vs Real Sensors

| Aspect | Real Hardware | Simulation |
|---|---|---|
| Activation | `GPIOSENSORSenabled` in Settings | Telnet key `d` |
| Sensors discovered | Auto-scan OneWire bus | 3 fixed virtual sensors |
| Update interval | `GPIOSENSORSinterval` (default 20s) | Fixed 10 seconds |
| Temperature source | `sensors.getTempC()` | Random walk ±0.5°C |
| Requires reboot | Yes (for pin change) | No — instant toggle |
| MQTT publishing | Yes | Yes |
| Labels & graph | Yes | Yes |

## File Reference

| File | Role |
|---|---|
| [sensors_ext.ino](../../src/OTGW-firmware/sensors_ext.ino) | Sensor init, polling, simulation, MQTT config |
| [OTGW-firmware.h](../../src/OTGW-firmware/OTGW-firmware.h) | Settings variables, `DallasrealDevice` struct, debug flags |
| [restAPI.ino](../../src/OTGW-firmware/restAPI.ino) | Labels API endpoints (`getDallasLabels`, `updateAllDallasLabels`) |
| [index.js](../../src/OTGW-firmware/data/index.js) | Frontend: label cache, inline editor, graph integration |
| [graph.js](../../src/OTGW-firmware/data/graph.js) | Dynamic sensor detection, color palettes, graph series |
| [index.css](../../src/OTGW-firmware/data/index.css) | Light theme styles for label editor |
| [index_dark.css](../../src/OTGW-firmware/data/index_dark.css) | Dark theme styles for label editor |
| [updateServerHtml.h](../../src/OTGW-firmware/updateServerHtml.h) | OTA flash: label backup and restore logic |
| [settingStuff.ino](../../src/OTGW-firmware/settingStuff.ino) | Settings read/write (sensor settings in `settings.ini`) |
| [handleDebug.ino](../../src/OTGW-firmware/handleDebug.ino) | Telnet debug menu: simulation toggle (`d` key) |
| [jsonStuff.ino](../../src/OTGW-firmware/jsonStuff.ino) | Dallas-specific JSON helpers (1-decimal formatting) |

## Related ADRs

- **ADR-020**: Dallas DS18B20 Sensor Integration
- **ADR-033**: Dallas Sensor Custom Labels & Graph Visualization
- **ADR-029**: Non-Blocking Modal Dialogs (inline label editor pattern)
