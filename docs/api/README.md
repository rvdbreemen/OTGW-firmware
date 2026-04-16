# OTGW-firmware REST API Documentation

This directory contains formal API documentation for the OTGW-firmware REST API.

## OpenAPI Specification

The `openapi.yaml` file provides a complete OpenAPI 3.1 specification of the REST API.

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
    "networkmode": "wifi",
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
| `networkmode` | string | Active network mode: `"wifi"`, `"ethernet"`, or `"ap"` |
| `wifirssi` | integer | WiFi signal strength in dBm (0 in AP or Ethernet mode) |
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
    "otcommandinterface": true,
    "otgwsimulation": false,
    "board": "esp8266",
    "hardwaremode": "pic",
    "networkmode": "wifi",
    "otdirectavailable": true,
    "otdmode": "gateway",
    "otdbypass": false,
    "otdmonitor": false,
    "otdmaster": false,
    "otdstepup": true,
    "otdthermostat": true,
    "otdsetback": false,
    "otdschedtotal": 12,
    "otdschedactive": 11,
    "otdscheddisabled": 1,
    "otdoverrides": 0
  }
}
```

The `otcommandinterface` field is `true` when either a PIC or OT-direct hardware interface is present and active.

The `board`, `hardwaremode`, and `networkmode` fields indicate the hardware platform and active modes:
- `board`: hardware identifier (e.g., `"esp8266"`, `"esp32s3"`)
- `hardwaremode`: `"pic"` for standard PIC-based OTGW, `"otdirect"` for OTGW32
- `networkmode`: `"wifi"`, `"ethernet"`, or `"ap"` (AP fallback mode)

The `otd*` fields are only present when `otdirectavailable` is `true` (OTGW32 builds). On standard ESP8266+PIC builds, only `otdirectavailable: false` is included.

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
    "maxfreeblock": 20480,
    "networkmode": "wifi",
    "wifiquality": 80
  }
}
```

| Field | Description |
|-------|-------------|
| `dateTime` | Current date/time in device timezone |
| `epoch` | Unix timestamp |
| `message` | Current status message text |
| `psmode` | Priority Service mode active |
| `otgwsimulation` | OTGW simulation mode active |
| `freeheap` | Free heap memory in bytes |
| `maxfreeblock` | Largest contiguous free block |
| `networkmode` | Active network mode: `"wifi"`, `"ethernet"`, or `"ap"` |
| `wifiquality` | WiFi signal quality 0-100 (0 in AP mode or Ethernet, 100 for wired Ethernet) |
| `apfallback` | Only present in pre-release builds when AP fallback is active: `true` |

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

**Known setting names**: `hostname`, `mqttenable`, `mqttbroker`, `mqttbrokerport`, `mqttuser`, `mqttpasswd`, `mqtttoptopic`, `mqtthaprefix`, `mqttharebootdetection`, `mqttuniqueid`, `mqttotmessage`, `mqttinterval`, `mqttseparatesources`, `ntpenable`, `ntptimezone`, `ntphostname`, `ntpsendtime`, `ledblink`, `darktheme`, `nightlyrestart`, `nightlyrestarthour`, `ui_autoscroll`, `ui_timestamps`, `ui_capture`, `ui_autoscreenshot`, `ui_autodownloadlog`, `ui_autoexport`, `ui_graphtimewindow`, `gpiosensorsenabled`, `gpiosensorslegacyformat`, `gpiosensorspin`, `gpiosensorsinterval`, `s0counterenabled`, `s0counterpin`, `s0counterdebouncetime`, `s0counterpulsekw`, `s0counterinterval`, `gpiooutputsenabled`, `gpiooutputspin`, `gpiooutputstriggerbit`, `otgwcommandenable`, `otgwcommands`, `webhookenable`, `webhookurlon`, `webhookurloff`, `webhooktriggerbit`, `webhookpayload`, `webhookcontenttype`, `httppasswd`, `satenabled`, `satsystem`, `sattargettemp`, `satcoefficient`, `satdeadband`, `satinterval`, `satexternaltemp`, `satpresetcomfort`, `satpreseteco`, `satpresetaway`, `satpwmautoswitch`

**Nightly restart settings**:

| Setting | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| `nightlyrestart` | bool | - | `false` | Enable a scheduled daily restart to recover heap memory |
| `nightlyrestarthour` | int | 0-23 | `4` | Local hour when the nightly restart runs (requires NTP to be enabled and synced) |

**SAT setting details**:

| Setting | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| `satenabled` | bool | - | `false` | Enable/disable SAT controller |
| `satsystem` | int | 0-1 | `0` | Heating system (0=radiator, 1=underfloor) |
| `sattargettemp` | float | 5-30 | `20.0` | Target room temperature (°C) |
| `satcoefficient` | float | 0-5 | `1.5` | Heating curve coefficient |
| `satdeadband` | float | 0-2 | `0.25` | PID deadband (°C) |
| `satinterval` | int | 10-300 | `30` | Control loop interval (seconds) |
| `satexternaltemp` | bool | - | `false` | Prefer external indoor sensor over OT bus |
| `satpresetcomfort` | float | 15-28 | `21.0` | Comfort preset temperature (°C) |
| `satpreseteco` | float | 10-22 | `18.0` | Eco preset temperature (°C) |
| `satpresetaway` | float | 5-18 | `15.0` | Away preset temperature (°C) |
| `satpwmautoswitch` | bool | - | `true` | Auto-switch between PWM/continuous mode |

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

Triggers a full MQTT autodiscovery cycle, marking all discovery IDs as pending and drip-publishing their configs asynchronously (one every 3 seconds). Discovery templates are compiled from PROGMEM (not read from LittleFS at runtime).

**Authentication**: Not required

**Aliases**: `POST /api/v2/otgw/autoconfigure` (backward compatibility)

**Response** `202 Accepted`:
```json
{"status": "accepted"}
```

---

### Sensors

#### `GET /api/v2/sensors` | `GET /api/v2/sensors/status`

Returns the current hardware sensor status. Both paths return the same response.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "sensors": {
    "dallas_enabled": true,
    "dallas_detected": true,
    "dallas_count": 2,
    "dallas_gpio": 4,
    "dallas_poll_sec": 30,
    "simulated": false,
    "devices": {
      "28FF64D1841703F1": {"temp": 21.5, "epoch": 1774548600},
      "28FF94E2841703F2": {"temp": 18.3, "epoch": 1774548600}
    },
    "s0": {
      "enabled": false,
      "gpio": 0,
      "poll_sec": 10,
      "pulses": 0,
      "total": 0,
      "power_kw": 0.000,
      "epoch": 0
    }
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `dallas_enabled` | boolean | Whether Dallas sensors are enabled in settings |
| `dallas_detected` | boolean | Whether at least one Dallas sensor was found on the bus |
| `dallas_count` | integer | Number of detected Dallas sensors |
| `dallas_gpio` | integer | GPIO pin used for the 1-Wire bus |
| `dallas_poll_sec` | integer | Sensor polling interval in seconds |
| `simulated` | boolean | Whether sensor simulation mode is active |
| `devices` | object | Per-device readings (only present when sensors are detected or simulation is active). Keys are 16-character 1-Wire addresses; values contain `temp` (°C) and `epoch` (seconds since boot). |
| `s0.enabled` | boolean | Whether S0 pulse counter is enabled |
| `s0.gpio` | integer | GPIO pin for S0 input |
| `s0.poll_sec` | integer | S0 reporting interval in seconds |
| `s0.pulses` | integer | Pulse count in the current interval |
| `s0.total` | integer | Total pulse count since boot |
| `s0.power_kw` | number | Calculated power in kW |
| `s0.epoch` | integer | Timestamp of last pulse (seconds since boot) |

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

### SAT (Smart Autotune Thermostat)

The SAT endpoints provide access to the embedded smart heating controller. SAT runs entirely on the ESP  - these endpoints are for monitoring state and pushing external inputs.

#### `GET /api/v2/sat/status`

Returns the full SAT runtime state including temperatures, PID terms, cycle data, and safety status.

**Authentication**: Required (when password is configured)

**Response** `200 OK`:
```json
{
  "enabled": true,
  "active": true,
  "control_mode": 1,
  "boiler_status": 3,
  "target_temp": 21.0,
  "room_temp": 20.5,
  "outside_temp": 8.0,
  "heating_curve": 42.3,
  "pid_output": 43.1,
  "final_setpoint": 43.1,
  "error": 0.50,
  "pid_p": 0.82,
  "pid_i": 0.03,
  "pid_d": -0.04,
  "kp": 1.6350,
  "ki": 0.000195,
  "kd": 960.60,
  "coefficient": 1.5,
  "deadband": 0.25,
  "cycle_count": 12,
  "last_cycle_class": 1,
  "cycle_max_flow": 45.2,
  "cycle_overshoot_sec": 0,
  "pwm_duty": 0.00,
  "pwm_flame_req": false,
  "heating_system": 0,
  "external_temp_valid": false,
  "external_outdoor_valid": false,
  "safety_tripped": false
}
```

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | boolean | Whether SAT is enabled in settings |
| `active` | boolean | Whether SAT is actively controlling |
| `control_mode` | integer | 0=Off, 1=Continuous, 2=PWM |
| `boiler_status` | integer | Boiler status enum (0-14, see below) |
| `target_temp` | float | Desired room temperature (°C) |
| `room_temp` | float | Current room temperature (°C, from OT bus or external sensor) |
| `outside_temp` | float | Current outside temperature (°C, from OT bus or external sensor) |
| `heating_curve` | float | Calculated heating curve setpoint (°C) |
| `pid_output` | float | PID output = curve + P + I + D (°C) |
| `final_setpoint` | float | Setpoint sent to boiler after mode/clamp (°C) |
| `error` | float | PID error = target - room temp (°C) |
| `pid_p` | float | Proportional term |
| `pid_i` | float | Integral term |
| `pid_d` | float | Derivative term |
| `kp` | float | Current Kp gain |
| `ki` | float | Current Ki gain |
| `kd` | float | Current Kd gain |
| `coefficient` | float | Heating curve coefficient setting |
| `deadband` | float | PID deadband setting (°C) |
| `cycle_count` | integer | Total completed flame cycles |
| `last_cycle_class` | integer | 0=None, 1=Good, 2=Overshoot, 3=Underheat, 4=Short, 5=Uncertain |
| `cycle_max_flow` | float | Maximum flow temp in last cycle (°C) |
| `cycle_overshoot_sec` | float | Seconds flow temp exceeded setpoint+margin in last cycle |
| `pwm_duty` | float | PWM duty cycle (0.0–1.0) |
| `pwm_flame_req` | boolean | Whether PWM is requesting flame |
| `heating_system` | integer | 0=Radiator, 1=Underfloor |
| `external_temp_valid` | boolean | Whether an external indoor sensor is active |
| `external_outdoor_valid` | boolean | Whether an external outdoor sensor is active |
| `safety_tripped` | boolean | Whether safety shutdown is active |

**Boiler status values**: 0=Off, 1=Idle, 2=Preheating, 3=At Setpoint, 4=Modulating Up, 5=Modulating Down, 6=Ignition Surge, 7=Stalled Ignition, 8=Anti-Cycling, 9=Pump Starting, 10=Waiting Flame, 11=Overshoot Cooling, 12=Post-Cycle, 13=Heating, 14=Cooling

#### `POST /api/v2/sat/target`

Set the SAT target room temperature. Persists to flash via deferred settings write.

**Authentication**: Required (when password is configured)

**Request body**: Raw value or JSON:
```
21.5
```
or:
```json
{"value": "21.5"}
```

**Valid range**: 5.0–30.0 °C

**Response** `200 OK`:
```json
{"status": "ok"}
```

#### `POST /api/v2/sat/externaltemp`

Push an external indoor temperature reading (e.g., from a more accurate room sensor). The reading expires after the configured `sensor_max_age` (default 6 hours) if not refreshed, falling back to the OT bus room temperature.

**Authentication**: Required (when password is configured)

**Request body**: Raw value or JSON:
```
20.8
```
or:
```json
{"value": "20.8"}
```

**Valid range**: -50.0–100.0 °C

**Response** `200 OK`:
```json
{"status": "ok"}
```

#### `POST /api/v2/sat/externaloutdoor`

Push an external outdoor temperature reading. Expires after `sensor_max_age` if not refreshed, falling back to the OT bus outdoor temperature.

**Authentication**: Required (when password is configured)

**Request body**: Same format as `externaltemp`.

**Valid range**: -50.0–100.0 °C

**Response** `200 OK`:
```json
{"status": "ok"}
```

#### `POST /api/v2/sat/flush`

Flushes short-lived SAT data (PID integral accumulator and cycle statistics window). Useful after large configuration changes or when troubleshooting.

**Authentication**: Required (when password is configured)

**Response** `200 OK`:
```json
{"result": "ok", "flushed": ["pid", "cycles"]}
```

---

### OT Direct -- OTGW32 Only

The `/api/v2/otdirect/*` endpoints are only present in OTGW32 builds (`HAS_DIRECT_OT=1`).
On standard ESP8266+PIC hardware all these endpoints return `503 Service Unavailable`.

#### `GET /api/v2/otdirect/status`

Returns the full OTGW32 OT-direct runtime state.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "otdirect_status": {
    "mode": "gateway",
    "bypass": false,
    "stepup": true,
    "monitor_mode": false,
    "master_mode": false,
    "thermostat_connected": true,
    "setback_active": false,
    "schedule_total": 12,
    "schedule_active": 11,
    "schedule_disabled": 1,
    "overrides_active": 2,
    "ot_online": true,
    "thermostat": true,
    "boiler": true
  }
}
```

All fields are **read-only** runtime state. See the field reference table below.

#### `POST /api/v2/otdirect/mode`

Switch the OT-direct engine to the requested operating mode. Send `mode` as a form or JSON parameter.

**Authentication**: Not required

**Valid mode values**:

| Value | Equivalent command | Description |
|-------|--------------------|-------------|
| `gateway` | `GW=1` | Full gateway with scheduler, thermostat forwarding, and overrides (default) |
| `monitor` | `GW=M` | Transparent pass-through; all frames forwarded unmodified |
| `bypass` | `GW=0` | Thermostat direct to boiler via hardware relay (requires bypass relay) |
| `master` | `GW=2` | OTGW32 is the sole OT master; scheduler only, no thermostat expected |
| `loopback` | `GW=L` | Internal loopback test with simulated boiler; no hardware needed |

**Response** `200 OK`: Full `otdirect_status` object reflecting the mode after the switch command has been queued.

**Error responses**:
- `400`  - Missing or invalid `mode` parameter
- `503`  - OT Direct hardware not available

#### `GET /api/v2/otdirect/settings`

Returns the persisted OT-direct configuration settings.

**Authentication**: Not required

**Response** `200 OK`:
```json
{
  "otdirect_settings": {
    "mode": 1,
    "setback_temp": 15.0,
    "setback_timeout": 300,
    "ch_mode": 0,
    "flow_temp": 55.0,
    "flow_max": 80.0,
    "room_setpoint": 20.0,
    "gradient": 1.5,
    "exponent": 1.3,
    "offset": 0.0,
    "room_comp": false,
    "kp": 2.0,
    "ki": 0.001,
    "kboost": 5.0
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `mode` | integer | Persisted mode index: 0=bypass, 1=gateway, 2=monitor, 3=master, 4=loopback |
| `setback_temp` | float | Setback temperature applied when thermostat disconnects (C, range 1-30) |
| `setback_timeout` | integer | Seconds without a thermostat frame before setback activates |
| `ch_mode` | integer | CH control mode (0=fixed setpoint, 1=heating curve, 2=PI room compensation) |
| `flow_temp` | float | Fixed flow temperature setpoint (C, used when ch_mode=0) |
| `flow_max` | float | Maximum flow temperature limit (C) |
| `room_setpoint` | float | Room temperature setpoint for PI compensation (C) |
| `gradient` | float | Heating curve gradient |
| `exponent` | float | Heating curve exponent |
| `offset` | float | Heating curve offset (C) |
| `room_comp` | boolean | Whether PI room compensation is enabled |
| `kp` | float | PI proportional gain |
| `ki` | float | PI integral gain |
| `kboost` | float | PI boost gain for large errors |

#### `POST /api/v2/otdirect/settings`

Update OT-direct settings. Only provided parameters are updated.

**Authentication**: Not required

**Form parameters**: `setbacktemp` (float, 1.0-30.0), `setbacktimeout` (integer, seconds), `chmode` (int), `flowtemp` (float), `flowmax` (float), `roomsetpoint` (float), `gradient` (float), `exponent` (float), `offset` (float), `roomcomp` (bool), `kp` (float), `ki` (float), `kboost` (float)

**Response** `200 OK`: Full `otdirect_status` object.

#### `GET /api/v2/otdirect/overrides`

Returns all active write overrides and response overrides in the OT-direct engine.

**Authentication**: Not required

**Response** `200 OK`: JSON object with override lists (structure defined by `getOTDirectOverridesJSON()`).

#### `POST /api/v2/otdirect/overrides`

Add, clear, or manage override entries. All actions require `action` and `msgid`. Actions `sr` and `rm` additionally require `value`.

**Form parameters**:

| Parameter | Required | Description |
|-----------|----------|-------------|
| `action` | Yes | One of: `sr`, `cr`, `rm`, `cm`, `ui`, `ki` |
| `msgid` | Yes | OpenTherm Message ID (0-127) |
| `value` | For `sr`/`rm` | 4-hex-digit data word (e.g., `3C00`) |

| Action | OT command | Description |
|--------|------------|-------------|
| `sr` | `SR=MsgID:HHHH` | Set stored response (gateway answers thermostat with fixed data for this MsgID) |
| `cr` | `CR=MsgID` | Clear stored response |
| `rm` | `RM=MsgID:HHHH` | Set response modifier (modify boiler response data sent to thermostat) |
| `cm` | `CM=MsgID` | Clear response modifier |
| `ui` | `UI=MsgID` | Mark MsgID as unknown (gateway replies UNKNOWN_DATAID) |
| `ki` | `KI=MsgID` | Mark MsgID as known again (restore normal forwarding) |

**Response** `200 OK`: Updated override list.

**Error responses**:
- `400`  - Invalid or missing parameters
- `503`  - OT Direct hardware not available

---

#### OT Direct State Field Reference

The `state.otd.*` struct fields are exposed via two REST endpoints:
1. `/api/v2/otdirect/status`  - as an `otdirect_status` object with unprefixed field names
2. `/api/v2/device/info`  - as flattened fields with `otd` prefix (only when `otdirectavailable` is `true`)

All fields are **read-only** runtime state except where noted.

| `otdirect_status` field | `device/info` field | Struct field | Type | Access | Description |
|-------------------------|---------------------|--------------|------|--------|-------------|
| `mode` | `otdmode` | `eMode` | string enum | read-only | Current operating mode: `gateway`, `monitor`, `bypass`, `master`, or `loopback` |
| `bypass` | `otdbypass` | `bBypassActive` | boolean | read-only | True when thermostat is routed directly to boiler via bypass relay |
| `monitor_mode` | `otdmonitor` | `bMonitorMode` | boolean | read-only | True when engine is in transparent pass-through mode |
| `master_mode` | `otdmaster` | `bMasterMode` | boolean | read-only | True when OTGW32 is acting as sole OT master with no thermostat |
| `stepup` | `otdstepup` | `bStepUpEnabled` | boolean | read-only | True when the 24V step-up converter is active |
| `thermostat_connected` | `otdthermostat` | `bThermostatConnected` | boolean | read-only | True when a thermostat frame has been received within the setback timeout |
| `setback_active` | `otdsetback` | `bSetbackActive` | boolean | read-only | True when thermostat is disconnected and setback override is engaged |
| `schedule_total` | `otdschedtotal` | `iScheduleTotal` | integer | read-only | Total number of OT message IDs in the polling schedule |
| `schedule_active` | `otdschedactive` | `iScheduleActive` | integer | read-only | Number of schedule entries currently active (not disabled by boiler) |
| `schedule_disabled` | `otdscheddisabled` | `iScheduleDisabled` | integer | read-only | Number of entries disabled (boiler returned UNKNOWN_DATA_ID three times) |
| `overrides_active` | `otdoverrides` | `iOverrideCount` | integer | read-only | Number of active write-override slots (CS=, TT=, SH=, SW= etc.) |
| `ot_online` | _(not in device/info)_ | _(otBus.bOnline)_ | boolean | read-only | True when the OT bus is alive (frames recently received) |
| `thermostat` | _(not in device/info)_ | _(otBus.bThermostatState)_ | boolean | read-only | True when the thermostat OT bus side is active |
| `boiler` | _(not in device/info)_ | _(otBus.bBoilerState)_ | boolean | read-only | True when the boiler OT bus side is active |
| _(not exposed)_ | _(not in device/info)_ | `iLastThermostatMs` | uint32 | internal | `millis()` timestamp of last thermostat frame received; used internally for setback timeout, not exposed via REST |

The `otdirectavailable` field in `/api/v2/device/info` is always present (boolean). When `false`, all other `otd*` fields are absent from the response.

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

**Deprecated unversioned routes** (may be removed in a future release):
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

### Get SAT Status

```bash
curl http://otgw.local/api/v2/sat/status
```

### Set SAT Target Temperature

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"value":"21.5"}' \
  http://otgw.local/api/v2/sat/target
```

### Push External Room Temperature to SAT

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"value":"20.8"}' \
  http://otgw.local/api/v2/sat/externaltemp
```

### Push External Outdoor Temperature to SAT

```bash
curl -X POST -d "8.5" http://otgw.local/api/v2/sat/externaloutdoor
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
