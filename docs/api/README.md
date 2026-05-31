# OTGW-firmware REST API Documentation

This directory contains formal API documentation for the OTGW-firmware REST API.

## OpenAPI Specification

The `openapi.yaml` file provides a complete OpenAPI 3.0 specification of the REST API.

### Using the Specification

**View in Swagger UI**:
1. Visit https://editor.swagger.io/
2. Click File > Import File
3. Upload `openapi.yaml`

**Generate Client Code**:
Use the OpenAPI specification to generate client libraries:
```bash
# Install OpenAPI Generator
npm install @openapitools/openapi-generator-cli -g

# Generate Python client
openapi-generator-cli generate -i openapi.yaml -g python -o ./client/python

# Generate JavaScript client
openapi-generator-cli generate -i openapi.yaml -g javascript -o ./client/javascript
```

**Validate the Specification**:
```bash
# Using swagger-cli
npm install -g @apidevtools/swagger-cli
swagger-cli validate openapi.yaml
```

## API Overview

### Base URL
```
http://{device-ip}/api
```

### API Version

**v2** is the only supported API version. v0 and v1 have been removed and return **410 Gone**.

### Authentication

Optional HTTP Basic Auth. When a password is configured in device settings, mutating endpoints require authentication with username `admin` and the configured password.

**Protected endpoints** (require auth when password is set):

- Settings: `GET/POST/PUT /api/v2/settings`
- OTGW commands: `POST /api/v2/otgw/commands`, `POST /api/v2/otgw/command/{cmd}`
- MQTT discovery: `POST /api/v2/otgw/discovery`, `POST /api/v2/otgw/autoconfigure`
- Simulation: `POST /api/v2/simulate/start`, `POST /api/v2/simulate/stop`
- Webhook test: `POST /api/v2/webhook/test`
- Diagnostic dump: `GET /api/v2/debug` (contains network credentials)
- MQTT republish: `POST /api/v2/mqtt/republish`
- File management, reboot, reset, and OTA update endpoints

**Unprotected endpoints** (always accessible):

- Health, device info/time/crashlog, OpenTherm data, sensor labels, PIC settings, firmware/filesystem info

CSRF same-origin validation is enforced for authenticated requests from browsers (Origin/Referer header must match the Host header).

**Security Note**: Do not expose the device directly to the internet. The device uses plain HTTP only (no HTTPS). Use VPN for remote access.

## Complete Endpoint Reference

### Health & Status

#### `GET /api/v2/health`

Returns the current health status of the device.

**Authentication**: Not required

**Side effect**: Each call writes a small probe file (`/.health`) to LittleFS to verify the filesystem is writable. Poll at 30-60 second intervals minimum.

**Response** `200 OK`:
```json
{
  "health": {
    "status": "UP",
    "uptime": "2d 3h 45m",
    "heap": 25600,
    "wifirssi": -65,
    "mqttconnected": "true",
    "otgwconnected": "true",
    "picavailable": "true",
    "littlefsMounted": "true"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | `"UP"` or `"DEGRADED"` |
| `uptime` | string | Human-readable uptime |
| `heap` | integer | Free heap memory in bytes |
| `wifirssi` | integer | WiFi signal strength in dBm |
| `mqttconnected` | string | `"true"` / `"false"` |
| `otgwconnected` | string | `"true"` / `"false"` |
| `picavailable` | string | `"true"` / `"false"` |
| `littlefsMounted` | string | `"true"` / `"false"` |

---

### Device Information

#### `GET /api/v2/device/info`

Returns comprehensive device information as a flat JSON map. Boolean values are proper JSON booleans in this endpoint.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "device": {
    "author": "Robert van den Breemen",
    "fwversion": "1.3.0",
    "picavailable": true,
    "picfwversion": "5.4",
    "picdeviceid": "gateway",
    "picfwtype": "gateway",
    "compiled": "Mar 26 2026 10:30:00",
    "hostname": "OTGW",
    "ipaddress": "192.168.1.100",
    "macaddress": "AA:BB:CC:DD:EE:FF",
    "freeheap": 25600,
    "maxfreeblock": 20480,
    "chipid": "1A2B3C",
    "coreversion": "3.1.2",
    "sdkversion": "2.2.2",
    "cpufreq": 80,
    "sketchsize": 524288,
    "freesketchspace": 524288,
    "flashchipid": "001640EF",
    "flashchipsize": 4.0,
    "flashchiprealsize": 4.0,
    "LittleFSsize": 1.0,
    "flashchipspeed": 40.0,
    "flashchipmode": "QIO",
    "ssid": "MyWiFi",
    "wifirssi": -55,
    "wifiquality": 80,
    "wifiquality_text": "Good",
    "ntpenable": true,
    "ntptimezone": "Europe/Amsterdam",
    "uptime": "2d 3h 45m",
    "lastreset": "Software/System restart",
    "bootcount": 42,
    "mqttconnected": true,
    "thermostatconnected": true,
    "boilerconnected": true,
    "otgwmode": "on",
    "otgwconnected": true,
    "otgwsimulation": false
  }
}
```

#### `GET /api/v2/device/time`

Returns the current device date and time.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "devtime": {
    "dateTime": "2026-03-26 10:30:00",
    "epoch": 1774548600,
    "message": "OpenTherm Gateway",
    "psmode": false,
    "otgwsimulation": false,
    "freeheap": 25600,
    "maxfreeblock": 20480
  }
}
```

#### `GET /api/v2/device/crashlog`

Returns the latest stored abnormal reboot/crash diagnostics, if available.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "crashlog": {
    "available": true,
    "summary": "Exception (28) at 0x40201234",
    "details": "epc1=0x40201234 epc2=0x00000000 ..."
  }
}
```

When no crash log is available, `available` is `false` and `summary`/`details` are empty strings.

---

### Settings

#### `GET /api/v2/settings`

Returns all device configuration settings.

**Authentication**: Required (when password is configured)

**Response** `200 OK`:

Returns a JSON map with all setting fields. Each setting includes its value, type indicator, and constraints. Settings cover: hostname, MQTT configuration, NTP, UI preferences, GPIO sensors, S0 counter, GPIO outputs, OTGW commands, webhook configuration, and HTTP password.

**Password fields** use a protected round-trip format:
- `password=0` means no password is currently stored
- `password=XX` means a password is stored and `XX` is its length

#### `POST /api/v2/settings` | `PUT /api/v2/settings`

Update a single device setting.

**Authentication**: Required (when password is configured)

**Request body**:
```json
{"name": "hostname", "value": "MyOTGW"}
```

**Password field behavior**:
- `"notthispassword"` keeps the existing stored password unchanged
- `""` clears the stored password
- Any other string replaces the stored password

**Response** `200 OK`: Echo of the submitted JSON body

**Error responses**:
- `400` - Invalid JSON, missing name, unknown setting name, or missing value
- `401` - Authentication required
- `403` - CSRF protection: invalid origin

**Known setting names**: `hostname`, `mqttenable`, `mqttbroker`, `mqttbrokerport`, `mqttuser`, `mqttpasswd`, `mqtttoptopic`, `mqtthaprefix`, `mqttharebootdetection`, `mqttuniqueid`, `mqttotmessage`, `mqttonchangepublishing`, `mqttinterval`, `mqttseparatesources`, `legacyport25238enabled`, `ntpenable`, `ntptimezone`, `ntphostname`, `ntpsendtime`, `ledblink`, `darktheme`, `ui_autoscroll`, `ui_timestamps`, `ui_capture`, `ui_autoscreenshot`, `ui_autodownloadlog`, `ui_autoexport`, `ui_graphtimewindow`, `gpiosensorsenabled`, `gpiosensorslegacyformat`, `gpiosensorspin`, `gpiosensorsinterval`, `s0counterenabled`, `s0counterpin`, `s0counterdebouncetime`, `s0counterpulsekw`, `s0counterinterval`, `gpiooutputsenabled`, `gpiooutputspin`, `gpiooutputstriggerbit`, `otgwcommandenable`, `otgwcommands`, `webhookenable`, `webhookurlon`, `webhookurloff`, `webhooktriggerbit`, `webhookpayload`, `webhookcontenttype`, `httppasswd`

---

### OpenTherm Data

#### `GET /api/v2/otgw/otmonitor`

Returns all available OpenTherm data including temperatures, status flags, pressures, sensor data, and Dallas temperature sensors.

**Authentication**: Not required

**Aliases**: `GET /api/v2/otgw/telegraf` (same response, compatibility alias)

**Response** `200 OK`:
```json
{
  "otmonitor": {
    "flamestatus": {"value": "ON", "unit": "", "lastupdated": 1234567},
    "chmodus": {"value": "ON", "unit": "", "lastupdated": 1234567},
    "chenable": {"value": "ON", "unit": "", "lastupdated": 1234567},
    "ch2modus": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "ch2enable": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "dhwmode": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "dhwenable": {"value": "ON", "unit": "", "lastupdated": 1234567},
    "diagnosticindicator": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "faultindicator": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "coolingmodus": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "coolingactive": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "otcactive": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "servicerequest": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "lockoutreset": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "lowwaterpressure": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "gasflamefault": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "airtemp": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "waterovertemperature": {"value": "OFF", "unit": "", "lastupdated": 1234567},
    "outsidetemperature": {"value": 12.5, "unit": "\u00b0C", "lastupdated": 1234567},
    "roomtemperature": {"value": 21.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "roomsetpoint": {"value": 20.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "remoteroomsetpoint": {"value": 0.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "controlsetpoint": {"value": 55.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "relmodlvl": {"value": 50.0, "unit": "%", "lastupdated": 1234567},
    "maxrelmodlvl": {"value": 100.0, "unit": "%", "lastupdated": 1234567},
    "boilertemperature": {"value": 45.5, "unit": "\u00b0C", "lastupdated": 1234567},
    "returnwatertemperature": {"value": 35.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "dhwtemperature": {"value": 55.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "dhwsetpoint": {"value": 60.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "maxchwatersetpoint": {"value": 80.0, "unit": "\u00b0C", "lastupdated": 1234567},
    "chwaterpressure": {"value": 1.5, "unit": "bar", "lastupdated": 1234567},
    "oemdiagnosticcode": {"value": 0, "unit": "", "lastupdated": 1234567},
    "oemfaultcode": {"value": 0, "unit": "", "lastupdated": 1234567},
    "s0powerkw": {"value": 0.0, "unit": "kW", "lastupdated": 1234567},
    "s0intervalcount": {"value": 0, "unit": "", "lastupdated": 1234567},
    "s0totalcount": {"value": 0, "unit": "", "lastupdated": 1234567},
    "sensorsimulation": {"value": false, "unit": "", "lastupdated": 1234567},
    "numberofsensors": {"value": 2, "unit": "", "lastupdated": 1234567}
  }
}
```

S0 counter fields (`s0powerkw`, `s0intervalcount`, `s0totalcount`) are only included when S0 counter is enabled in settings. Dallas temperature sensor entries are included when sensors are enabled or sensor simulation is active.

#### `GET /api/v2/otgw/messages/{msgid}`

Retrieve the current value for a single OpenTherm message by its numeric ID.

**Authentication**: Not required

**Parameters**:
- `msgid` (path, required) - Integer 0-127

**Response** `200 OK`:
```json
{"label": "boilertemperature", "value": 45.5, "unit": "\u00b0C"}
```

The `value` field type depends on the message type: `float` for `f88` (fixed-point) types, `integer` for all others.

**Error responses**:
- `400` - Invalid or missing message ID

#### `GET /api/v2/otgw/id/{msgid}`

Alias for `/api/v2/otgw/messages/{msgid}`. Prefer the primary endpoint for new integrations.

#### `GET /api/v2/otgw/label/{msglabel}`

Retrieve the current value for an OpenTherm message by its human-readable label name. Label matching is case-insensitive.

**Authentication**: Not required

**Parameters**:
- `msglabel` (path, required) - Label string (e.g., `boilertemperature`)

**Response** `200 OK`: Same format as `/api/v2/otgw/messages/{msgid}`

---

### Commands

#### `POST /api/v2/otgw/commands` | `PUT /api/v2/otgw/commands`

Send a command to the OpenTherm Gateway. The command is queued for asynchronous processing.

**Authentication**: Not required

**Request body** (JSON preferred):
```json
{"command": "TT=20.5"}
```

If the body is not valid JSON with a `command` field, the raw body text is used as the command string.

**Command format**: Two uppercase letters + `=` + value (e.g., `TT=20.5`, `GW=1`, `PR=A`).

**Response** `202 Accepted`:
```json
{"status": "queued"}
```

**Error responses**:
- `400` - Missing command, or invalid format (must be `LL=value`)
- `413` - Command too long

#### `POST /api/v2/otgw/command/{cmd}` | `PUT /api/v2/otgw/command/{cmd}`

Backward compatibility alias. The command is passed in the URL path instead of the request body.

**Prefer** `/api/v2/otgw/commands` (JSON body) for new integrations.

**Parameters**:
- `cmd` (path, required) - OTGW command string (e.g., `TT=20.5`)

**Response** `202 Accepted`: Same as above.

---

### Discovery

#### `POST /api/v2/otgw/discovery` | `PUT /api/v2/otgw/discovery`

Triggers a full MQTT autodiscovery cycle, sending all HA discovery configs from the `mqttha.cfg` file.

**Authentication**: Not required

**Aliases**: `POST /api/v2/otgw/autoconfigure` (backward compatibility)

**Response** `202 Accepted`:
```json
{"status": "accepted"}
```

#### Discovery verification and republish (v1.4.1+)

Version 1.4.1 adds three endpoints under `/api/v2/discovery/` that complement the existing unconditional publish at `/api/v2/otgw/discovery`. They cover broker-side retained-state loss, which is invisible to the legacy MQTT reconnect and HA-restart recovery paths. See [ADR-062](../adr/ADR-062-retained-discovery-verification.md) for the mechanism rationale.

- `GET /api/v2/discovery` — returns `verification.*` (active flag, last epoch, last missing and orphan counts), `counters.*` (published topics, pending IDs, verify runs, republishes triggered) and `settings.auto_verify`. Read-only; does not publish anything.
- `POST /api/v2/discovery/verify` — subscribes to `<haprefix>/+/<nodeId>/#` for 15 seconds, counts retained configs that arrive, and triggers re-publication (non-OT configs queued immediately; OT ID configs re-publish JIT as messages arrive) only when `received < expected`. Returns `202 Accepted` with `{status, expected, window_ms}` on success, `409` when a verify or drip is already in progress, and `503` when MQTT is down, heap is low, or the verification layer refuses to start.
- `POST /api/v2/discovery/republish` — unconditionally marks every discovery ID pending in the drip pipeline. Use this only when you already know the broker's retained state is bad, for example after a broker reinstall without persistence. Returns `200 OK` with `{status, count}` or `503` when MQTT is down.

For normal troubleshooting prefer the verify endpoint: it only re-announces when something is actually missing and avoids a full flood of ~80 retained messages.

---

### Sensors

#### `GET /api/v2/sensors/labels`

Returns all configured Dallas temperature sensor labels as a JSON object mapping sensor addresses to labels.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF94E2841703F2": "Kitchen"
}
```

Returns `{}` if no labels file exists.

#### `POST /api/v2/sensors/labels` | `PUT /api/v2/sensors/labels`

Update all Dallas temperature sensor labels. Provide a JSON object mapping sensor addresses to their new labels.

**Authentication**: Not required

**Request body**:
```json
{
  "28FF64D1841703F1": "Living Room",
  "28FF94E2841703F2": "Kitchen"
}
```

---

### PIC Gateway

#### `GET /api/v2/pic/flash-status`

Returns the PIC microcontroller flash status. Used for polling during PIC firmware upgrades.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "flashstatus": {
    "flashing": false,
    "progress": 0,
    "filename": "",
    "error": ""
  }
}
```

#### `GET /api/v2/pic/update-check`

Checks for available PIC firmware updates by making an outbound HTTP request to `otgw.tclcode.com`. Only call on-demand (e.g., when user opens a firmware tab).

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "pic_update": {
    "current": "5.4",
    "latest": "5.5",
    "update_available": true
  }
}
```

#### `GET /api/v2/pic/settings`

Returns the cached PIC gateway settings last queried via `PR=` commands. Triggers a new readout cycle (one `PR=` command every 3 seconds, approximately 45 seconds for a full cycle). Empty string values mean "not yet queried" or "not supported by this firmware version".

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "pic_settings": {
    "setpoint_override": "20.00",
    "setback": "15.00",
    "dhw_override": "",
    "gpio": "0/1",
    "gpio_states": "0/0",
    "led": "F/X/O/M/P/C",
    "tweaks": "1/1/1/1",
    "temp_sensor": "O=19.50",
    "smart_power": "Low",
    "thermostat_detect": "I",
    "builddate": "2023-01-01",
    "clock_mhz": "4",
    "reset_cause": "Power-on",
    "standalone_interval": "0",
    "voltage_ref": "3.3"
  }
}
```

| Field | Description |
|-------|-------------|
| `setpoint_override` | Current temperature setpoint override (PR=A) |
| `setback` | Setback temperature (PR=B) |
| `dhw_override` | DHW override state |
| `gpio` | GPIO pin configuration (PR=G) |
| `gpio_states` | GPIO pin states (PR=I) |
| `led` | LED function assignments (PR=L) |
| `tweaks` | Tweak flags (PR=T) |
| `temp_sensor` | Temperature sensor reading (PR=O) |
| `smart_power` | Smart power mode (PR=P) |
| `thermostat_detect` | Thermostat detection mode (PR=D) |
| `builddate` | PIC firmware build date (PR=B) |
| `clock_mhz` | PIC clock speed (PR=C) |
| `reset_cause` | Last PIC reset cause (PR=Q) |
| `standalone_interval` | Standalone mode interval (PR=M) |
| `voltage_ref` | Voltage reference (PR=V) |

See the [OTGW firmware documentation](https://otgw.tclcode.com/firmware.html) for details on PIC `PR=` responses.

---

### Flash & Firmware

#### `GET /api/v2/flash/status`

Returns unified flash status for both ESP8266 and PIC firmware upgrades.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "flashstatus": {
    "flashing": false,
    "pic_flashing": false,
    "pic_progress": 0,
    "pic_filename": "",
    "pic_error": ""
  }
}
```

#### `GET /api/v2/firmware/files`

Lists available PIC firmware files on the filesystem.

**Authentication**: Not required

**Response** `200 OK`: JSON array of firmware file objects.

#### `GET /api/v2/filesystem/files`

Lists all files on the LittleFS filesystem.

**Authentication**: Not required

**Response** `200 OK`: JSON array of file objects.

#### `GET /api/v2/filesystem/hash-check`

Compares the firmware git hash with the filesystem git hash to detect mismatches (e.g., after a firmware-only OTA update without flashing the new LittleFS image).

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "filesystem_check": {
    "match": true,
    "fw_hash": "abc1234",
    "fs_hash": "abc1234"
  }
}
```

---

### Simulation

#### `GET /api/v2/simulate`

Returns the current OTGW simulation status.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "simulation": {
    "active": false,
    "file": "/otgw_simulation.log",
    "interval_ms": 1000
  }
}
```

#### `POST /api/v2/simulate/start` | `PUT /api/v2/simulate/start`

Enables OTGW simulation mode. The gateway replays data from `/otgw_simulation.log` instead of reading from the PIC serial port.

**Authentication**: Not required

**Response** `200 OK`: Same format as `GET /api/v2/simulate` with `active: true`.

#### `POST /api/v2/simulate/stop` | `PUT /api/v2/simulate/stop`

Disables OTGW simulation mode. Resumes reading from the live PIC serial port.

**Authentication**: Not required

**Response** `200 OK`: Same format as `GET /api/v2/simulate` with `active: false`.

---

### Webhook

#### `POST /api/v2/webhook/test` | `PUT /api/v2/webhook/test`

Triggers a test webhook call to verify the configured webhook URL is reachable.

**Authentication**: Required (when password is configured)

**Parameters**:
- `state` (query, required) - `on` / `1` or `off` / `0`

**Response** `200 OK`:
```json
{"status": "ok"}
```

**Error responses**:
- `400` - Missing or invalid `state` parameter
- `401` - Authentication required
- `403` - CSRF protection

---

### Debug Diagnostics

#### `GET /api/v2/debug`

Returns a flat JSON map containing all device settings and current runtime state. This is the REST equivalent of the `D` telnet command.

**Authentication**: Required when an HTTP password is configured. The response contains network credentials (SSID, broker address).

**Response** `200 OK`:
```json
{
  "debug": {
    "build.version": "1.5.0-beta.29",
    "build.number": 29,
    "build.githash": "abc1234",
    "build.date": "May  8 2026",
    "runtime.heap_free": 26800,
    "runtime.heap_frag_pct": 5,
    "runtime.heap_min_free": 22400,
    "runtime.heap_max_block": 18432,
    "runtime.uptime_sec": 86400,
    "runtime.reboots": 3,
    "runtime.wifi_rssi": -60,
    "runtime.wifi_ip": "192.168.1.100",
    "runtime.wifi_ssid": "MyHomeNetwork",
    "runtime.wifi_connected": true,
    "settings.hostname": "OTGW",
    "settings.led_blink": true,
    "settings.http_auth": false,
    "settings.mqtt.broker": "homeassistant.local",
    "settings.mqtt.port": 1883,
    "settings.mqtt.user": "otgw",
    "settings.mqtt.passwd": "***",
    "settings.mqtt.toptopic": "OTGW",
    "settings.mqtt.ha_prefix": "homeassistant",
    "settings.mqtt.unique_id": "otgw-1a2b3c",
    "settings.mqtt.on_change": true,
    "settings.mqtt.interval": 60,
    "settings.mqtt.enabled": true,
    "settings.mqtt.disc_verify": true,
    "settings.mqtt.sep_src": false,
    "settings.legacy.port_25238": false,
    "settings.ntp.server": "pool.ntp.org",
    "settings.ntp.tz": "Europe/Amsterdam",
    "settings.ntp.enabled": true,
    "settings.sensors.enabled": false,
    "settings.sensors.gpio": 4,
    "settings.sensors.interval": 30,
    "settings.s0.enabled": false,
    "settings.s0.gpio": 14,
    "settings.s0.interval": 60,
    "state.mqtt.connected": true,
    "state.otgw.online": true,
    "state.otgw.ps_mode": false,
    "state.pic.available": true,
    "state.pic.fwversion": "5.4",
    "state.debug.otgw_sim": false,
    "state.debug.sensor_sim": false,
    "state.debug.restapi": false,
    "state.debug.mqtt": false
  }
}
```

Key notes:
- HTTP and MQTT passwords are never included in the response. `settings.http_auth` is a boolean that indicates whether a password is configured.
- `settings.mqtt.passwd` is always `"***"`.
- All fields use dot-notation keys inside the `debug` wrapper object.

---

### MQTT Runtime Actions

#### `POST /api/v2/mqtt/republish`

Forces an immediate republish of all OpenTherm measurement values to MQTT. Use this after a broker wipe or when retained state needs to be restored without waiting for the normal publish cadence.

This is distinct from `POST /api/v2/discovery/republish`, which re-publishes Home Assistant autodiscovery configurations. This endpoint re-publishes the actual OpenTherm measurement values.

**Authentication**: Required when an HTTP password is configured.

**Response** `200 OK`:
```json
{"status": "ok", "message": "OT value republish requested"}
```

**Error responses**:
- `405` - Method not allowed (only POST is accepted)
- `503` - MQTT is not connected

---

### Non-API Routes

These routes are served directly by the web server (not under `/api`):

| Route | Description |
|-------|-------------|
| `/` or `/index` or `/index.html` | Web UI (index.html) |
| `/index.css` | Web UI stylesheet (serves dark theme variant if enabled) |
| `/index.js` | Web UI JavaScript |
| `/graph.js` | Graph visualization JavaScript |
| `/pic` | PIC firmware upload page |
| `/upload` (POST) | File upload handler |
| `/ReBoot` | Reboot the ESP8266 |
| `/ResetWireless` | Reset WiFi settings |
| `/update` | OTA firmware update (provided by ESP8266HTTPUpdateServer) |
| `/FSexplorer` or `/FSexplorer.html` | Filesystem explorer (debug builds only) |

**Deprecated unversioned routes** (will be removed in v1.3.0):
| Route | Replacement |
|-------|-------------|
| `/api/firmwarefilelist` | `GET /api/v2/firmware/files` |
| `/api/listfiles` | `GET /api/v2/filesystem/files` |

---

## Response Formats

### Standard JSON Response

All v2 endpoints return JSON with a named wrapper object:
```json
{"health": {...}}
{"device": {...}}
{"settings": {...}}
```

### Error Responses

All errors return structured JSON (ADR-035):
```json
{"error": {"status": 400, "message": "Invalid message ID"}}
```

### 405 Method Not Allowed

All 405 responses include an `Allow` header listing valid HTTP methods (RFC 7231 ss 6.5.5):
```
HTTP/1.1 405 Method Not Allowed
Allow: GET
Content-Type: application/json

{"error":{"status":405,"message":"Method not allowed"}}
```

### 410 Gone (Legacy API Versions)

Requests to `/api/v0/...` or `/api/v1/...` return:
```json
{"error": {"status": 410, "message": "API version removed; use /api/v2"}}
```

### CORS Support

All responses include `Access-Control-Allow-Origin: *`.

All v2 endpoints support **OPTIONS preflight** for cross-origin requests:
```
OPTIONS /api/v2/health HTTP/1.1

HTTP/1.1 204 No Content
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS
Access-Control-Allow-Headers: Content-Type
Access-Control-Max-Age: 86400
```

### Queued Operations

Commands and discovery return **202 Accepted**:
```json
{"status": "queued"}
```

## Boolean Values

Most endpoints return boolean values as strings (`"true"` / `"false"`).
The `/v2/device/info` and `/v2/device/time` endpoints return proper JSON booleans (`true`/`false`).

## OpenTherm Message IDs

Message IDs range from 0-127. Common IDs:

| ID | Label | Description | Unit |
|----|-------|-------------|------|
| 0 | Status | Master/Slave status flags | - |
| 1 | controlsetpoint | Control setpoint | C |
| 5 | ASFflags | Application-specific fault flags | - |
| 17 | relmodlvl | Relative modulation level | % |
| 25 | boilertemperature | Boiler water temperature | C |
| 26 | dhwtemperature | DHW temperature | C |
| 28 | returnwatertemperature | Return water temperature | C |
| 56 | dhwsetpoint | DHW setpoint | C |

See OpenTherm Protocol specification for complete list.

## OTGW Commands

Commands use two-letter codes followed by `=value`:

| Command | Description | Example |
|---------|-------------|---------|
| TT | Temporary temperature override | `TT=20.5` |
| TC | Temperature constant | `TC=20.5` |
| OT | Outside temperature | `OT=12.0` |
| HW | Hot water on/off/push | `HW=1` |
| GW | Set gateway mode | `GW=1` |
| SB | Setback temperature | `SB=15.0` |
| SH | Set max CH water setpoint | `SH=80` |
| SW | Set DHW setpoint | `SW=60` |
| MM | Max modulation | `MM=100` |
| CS | Control setpoint | `CS=55` |
| C2 | Control setpoint 2 | `C2=0` |
| CH | CH enable | `CH=1` |
| H2 | CH2 enable | `H2=0` |
| VS | Ventilation setpoint | `VS=50` |
| PR | Print report | `PR=A` |
| PS | Print summary | `PS=1` |

See [OTGW firmware documentation](https://otgw.tclcode.com/firmware.html) for complete command reference.

## Rate Limiting

### Recommended Polling Intervals

- **Health checks**: 30-60 seconds minimum -- **each call writes a probe file to LittleFS flash** (see note below)
- **OpenTherm data**: 5-10 seconds
- **Flash status (during upgrade)**: 1-2 seconds
- **Settings**: On-demand only
- **PIC update check**: On-demand only (makes outbound HTTP request)

> **Health endpoint flash write**: `GET /v2/health` calls `updateLittleFSStatus()` on every
> request, which writes a small probe file (`/.health`) to LittleFS to verify the filesystem is writable.
> This is intentional -- it confirms the flash is not just mounted but actively writeable -- but it means
> each health request incurs a LittleFS write cycle. The designed use-case is post-OTA polling, which stops
> immediately once `status: UP` is received. Avoid using this endpoint as a high-frequency external monitor.

### Memory Protection

The API enforces a minimum 4KB free heap before processing requests. If heap is below this threshold, the API returns:
```
500: internal server error (low heap)
```

This prevents crashes and ensures stable operation.

## Examples

### Get Device Health

```bash
curl http://otgw.local/api/v2/health
```

### Get Device Information

```bash
curl http://otgw.local/api/v2/device/info
```

### Get Boiler Temperature

```bash
curl http://otgw.local/api/v2/otgw/messages/25
```

### Send Temperature Override

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"command":"TT=21.5"}' \
  http://otgw.local/api/v2/otgw/commands
```

### Get All OpenTherm Data

```bash
curl http://otgw.local/api/v2/otgw/otmonitor
```

### Trigger MQTT Autodiscovery

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

### Get PIC Gateway Settings

```bash
curl http://otgw.local/api/v2/pic/settings
```

### Check Simulation Status

```bash
curl http://otgw.local/api/v2/simulate
```

### Start Simulation

```bash
curl -X POST http://otgw.local/api/v2/simulate/start
```

### Test Webhook

```bash
curl -X POST "http://otgw.local/api/v2/webhook/test?state=on"
```

### Get Diagnostic Dump

```bash
curl http://otgw.local/api/v2/debug
# With auth:
curl -u admin:password http://otgw.local/api/v2/debug
```

### Force MQTT Value Republish

```bash
curl -X POST http://otgw.local/api/v2/mqtt/republish
```

## Integration Examples

### Home Assistant REST Sensor

```yaml
sensor:
  - platform: rest
    name: "OTGW Boiler Temperature"
    resource: "http://otgw.local/api/v2/otgw/messages/25"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "\u00b0C"
    scan_interval: 10
```

### Python Script

```python
import requests

# Get health status
response = requests.get('http://otgw.local/api/v2/health')
health = response.json()['health']
print(f"Status: {health['status']}, Heap: {health['heap']} bytes")

# Get device info
response = requests.get('http://otgw.local/api/v2/device/info')
device = response.json()['device']
print(f"Firmware: {device['fwversion']}, Gateway: {device['otgwmode']}")

# Set temperature override (JSON body, returns 202)
cmd = requests.post('http://otgw.local/api/v2/otgw/commands',
                     json={"command": "TT=21.5"})
print(cmd.json())  # {"status": "queued"}

# Get PIC settings
response = requests.get('http://otgw.local/api/v2/pic/settings')
pic = response.json()['pic_settings']
print(f"Setpoint override: {pic['setpoint_override']}")
```

### JavaScript Fetch

```javascript
// Get OpenTherm data
fetch('http://otgw.local/api/v2/otgw/otmonitor')
  .then(response => {
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.json();
  })
  .then(data => console.log('OpenTherm Data:', data.otmonitor))
  .catch(error => console.error('Error:', error));

// Send command (JSON body)
fetch('http://otgw.local/api/v2/otgw/commands', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({command: 'TT=21.5'})
})
  .then(response => response.json())
  .then(data => console.log(data));  // {status: "queued"}
```

## Testing Tools

### Swagger Editor
View and test the API interactively:
1. Go to https://editor.swagger.io/
2. Import `openapi.yaml`
3. Use "Try it out" feature to test endpoints

### Postman
Import the OpenAPI specification into Postman:
1. File > Import
2. Select `openapi.yaml`
3. Create requests from the imported collection

### cURL
All examples in this documentation use cURL and can be run directly from command line.

## Related Documentation

- **MQTT Topics**: See [MQTT.md](MQTT.md) for complete MQTT topic documentation
- **Main README**: See repository root for general firmware documentation
- **OpenTherm Specification**: `docs/opentherm specification/` directory in repository
- **OTGW Hardware**: https://otgw.tclcode.com/
- **Issues**: https://github.com/rvdbreemen/OTGW-firmware/issues

## Contributing

Found an error in the API documentation? Please open an issue or submit a pull request.

When updating the API:
1. Update `openapi.yaml` with new endpoints/changes
2. Validate the specification: `swagger-cli validate openapi.yaml`
3. Test endpoints on actual hardware
4. Update examples in this README if needed
