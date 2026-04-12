## Chapter 9: API Reference

This chapter documents all programmatic interfaces exposed by the OTGW-firmware: the REST API, the WebSocket stream, the MQTT integration, and the Telnet debug console. These are the interfaces used by Home Assistant, Node-RED flows, automation scripts, and custom integrations.

---

### REST API Overview

#### Base URL and Versioning

The current REST API version is v2. All v2 endpoints are under:

```
http://{device-ip}/api/v2/
```

Older versions return HTTP 410 Gone. Never use `/api/v0/` or `/api/v1/` in new code.

The device hostname defaults to `otgw.local` (mDNS / LLMNR). Alternatively, use the IP address assigned by your DHCP server.

#### Transport

- HTTP only, no HTTPS (ADR-003). The device operates on a trusted local network. For remote access, place it behind a reverse proxy or VPN.
- Content-Type for all responses: `application/json`
- All JSON responses are UTF-8

#### Authentication

Mutation endpoints (POST, PUT, DELETE) require HTTP Basic Authentication when an HTTP password is configured in Settings. The username is the device hostname (default `OTGW`). Authentication is checked via the `Authorization: Basic` header.

GET endpoints for sensitive data (settings, PIC settings) also require authentication.

Unauthenticated GET endpoints (health, sensor data, OT values) are intentionally public.

#### Error Response Format

All errors use a consistent JSON envelope:

```json
{
  "error": {
    "status": 404,
    "message": "Resource not found"
  }
}
```

HTTP status codes follow RFC 7231. Common error codes:

| Code | Meaning |
|------|---------|
| 400 | Bad Request: malformed JSON or missing required field |
| 401 | Unauthorized: authentication required |
| 403 | Forbidden: CSRF check failed |
| 404 | Not Found: unknown endpoint or resource |
| 405 | Method Not Allowed |
| 410 | Gone: deprecated API version (v0, v1) |
| 500 | Internal Server Error |
| 503 | Service Unavailable: low heap, PIC unavailable, etc. |

---

### REST API Endpoints

#### Health

##### GET /api/v2/health

Returns system health metrics. No authentication required.

**Request:** None

**Response:**

```json
{
  "uptime": 86400,
  "heap_free": 18432,
  "heap_health": "healthy",
  "mqtt_connected": true,
  "pic_available": true,
  "otgw_online": true,
  "wifi_rssi": -58,
  "firmware_version": "2.0.0",
  "timestamp": 1712345678
}
```

| Field | Type | Description |
|-------|------|-------------|
| `uptime` | integer | Seconds since last boot |
| `heap_free` | integer | Free heap in bytes |
| `heap_health` | string | `healthy`, `low`, `warning`, `critical` |
| `mqtt_connected` | boolean | MQTT broker connection status |
| `pic_available` | boolean | PIC co-processor detected and responding |
| `otgw_online` | boolean | OpenTherm serial link active |
| `wifi_rssi` | integer | WiFi signal strength in dBm (absent when on Ethernet) |
| `firmware_version` | string | Installed firmware version |
| `timestamp` | integer | Current Unix timestamp (requires NTP sync) |

This endpoint is safe to poll frequently. Typical use: OTA update completion detection, watchdog integration, Home Assistant availability sensor.

---

#### Settings

##### GET /api/v2/settings

Returns all device settings. Authentication required.

**Request:** None

**Response:** A flat JSON object containing all configurable settings fields. Example subset:

```json
{
  "hostname": "otgw",
  "httppassword": "****",
  "mqttenable": true,
  "mqttbroker": "192.168.1.10",
  "mqttport": 1883,
  "mqttuser": "otgw",
  "mqtttoptopic": "OTGW",
  "mqttuniqueid": "otgw-AABBCCDDEEFF",
  "mqtthaprefix": "homeassistant",
  "mqtthadiscovery": true,
  "ntptimezone": "Europe/Amsterdam",
  "ntphost": "pool.ntp.org",
  "ssid": "MyHomeNetwork",
  "satenable": false
}
```

The `ssid` field is read-only and returns the currently connected WiFi SSID. On Ethernet, `ssid` is not present; check `/api/v2/device/info` for `"ssid": "Wired"`. The `httppassword` field returns a masked placeholder, never the actual password.

##### POST /api/v2/settings

Update one or more settings. Authentication required.

**Request body:** JSON object with any subset of setting key-value pairs.

```json
{
  "mqttbroker": "192.168.1.20",
  "mqttport": 1883,
  "mqtthadiscovery": true
}
```

**Response:**

```json
{
  "status": "ok",
  "saved": true
}
```

Settings are validated server-side. Invalid values are silently ignored or constrained to safe ranges. Side effects (MQTT reconnect, NTP resync, mDNS restart) are applied within 2 seconds via a deferred-write timer.

---

#### Device Information

##### GET /api/v2/device/info

Returns hardware and platform information. No authentication required.

**Response:**

```json
{
  "hostname": "otgw",
  "firmware_version": "2.0.0",
  "platform": "ESP8266",
  "board": "NODOSHOP_ESP8266",
  "cpu_freq_mhz": 160,
  "flash_size": 4194304,
  "chip_id": "AABBCC",
  "ssid": "MyHomeNetwork",
  "ip": "192.168.1.50",
  "mac": "AA:BB:CC:DD:EE:FF",
  "reboot_count": 7,
  "reboot_reason": "Software/System restart"
}
```

On Ethernet (ESP32): `"ssid": "Wired"` and `"ip"` reflects the Ethernet address.

##### GET /api/v2/device/time

Returns current device time.

**Response:**

```json
{
  "epoch": 1712345678,
  "iso": "2026-04-06T12:34:38+02:00",
  "ntp_synced": true,
  "ntp_last_sync": 1712344678
}
```

##### GET /api/v2/device/crashlog

Returns the most recent crash log summary from LittleFS. Returns empty object if no crash log exists. No authentication required.

---

#### OpenTherm Gateway

##### GET /api/v2/otgw/status

Returns current OpenTherm state: all message ID values received from the boiler and thermostat. This is the same data available over MQTT but in a single request-response snapshot.

**Response:**

```json
{
  "Tr": 20.5,
  "Tboiler": 62.3,
  "TSet": 55.0,
  "RelModLevel": 45.2,
  "CHPressure": 1.4,
  "flamestatus": true,
  "chmodus": true,
  "dhwmode": false,
  "faultindicator": false
}
```

The exact fields depend on which OpenTherm message IDs your boiler and thermostat exchange. Fields are absent when the corresponding message ID has not yet been received.

##### GET /api/v2/otgw/messages/{id}

Returns the current value for a specific OpenTherm message ID.

**Parameters:** `{id}` is the numeric message ID (0-127).

**Response:**

```json
{
  "id": 25,
  "label": "Tboiler",
  "value": "62.31",
  "type": "f8.8",
  "last_update": 1712345600
}
```

Alias: `GET /api/v2/otgw/id/{id}`

##### GET /api/v2/otgw/label/{label}

Returns the current value for a specific OpenTherm label string.

**Parameters:** `{label}` is the label name (case-sensitive), e.g., `Tboiler`, `Tr`, `RelModLevel`.

##### GET /api/v2/otgw/otmonitor

Returns all current OpenTherm values in a flat key-value format compatible with Telegraf and OTmonitor. Alias: `GET /api/v2/otgw/telegraf`.

##### POST /api/v2/otgw/command

Send a raw command to the OTGW PIC (or OTDirect on ESP32). Authentication required.

**Request body (JSON):**

```json
{
  "command": "TT=21.50"
}
```

Alternatively, the command string can be sent as a plain text body without JSON wrapping.

**Response:**

```json
{
  "status": "queued",
  "command": "TT=21.50"
}
```

Common PIC command codes:

| Command | Description |
|---------|-------------|
| `TT=xx.xx` | Temporary room setpoint override |
| `TC=xx.xx` | Constant room setpoint override |
| `CS=xx.xx` | Control setpoint (CH flow temperature) |
| `SH=xx` | Max CH water setpoint |
| `HW=1` / `HW=0` | Hot water on / off |
| `GW=1` / `GW=0` | Gateway mode / monitor mode |
| `PS=1` / `PS=0` | Print Summary mode on / off |
| `PR=A` | Query PIC banner string |

Commands are validated and queued. Responses are processed asynchronously and published to MQTT. The REST endpoint returns `"queued"` immediately without waiting for the PIC response.

##### POST /api/v2/otgw/commands

Alias for `/api/v2/otgw/command`. Preferred spelling per ADR-035.

##### POST /api/v2/otgw/discovery

Force a full Home Assistant MQTT auto-discovery republish. Useful after a HA restart when discovery messages were missed. Authentication required.

**Request:** No body required.

**Response:**

```json
{
  "status": "started"
}
```

Discovery runs asynchronously in the background. All ~200 entity configurations are republished to the `homeassistant/` MQTT prefix. This is equivalent to clicking "Rediscover" in the web UI.

Alias: `POST /api/v2/otgw/autoconfigure`

---

#### Sensors

##### GET /api/v2/sensors

Returns all Dallas DS18B20 sensor addresses and their labels. No authentication required.

**Response:**

```json
{
  "sensors": [
    {
      "address": "28FF64D1841703F1",
      "label": "Attic",
      "temperature": 18.3,
      "last_seen": 1712345650
    },
    {
      "address": "28FF9A3B71120502",
      "label": "",
      "temperature": 22.1,
      "last_seen": 1712345651
    }
  ]
}
```

##### GET /api/v2/sensors/{address}

Returns data for a single sensor by its 1-Wire address.

**Parameters:** `{address}` is the 16-character hex address string.

##### GET /api/v2/sensors/labels

Returns all sensor address-to-label mappings.

##### POST /api/v2/sensors/labels

Update sensor labels. Authentication required.

**Request body:**

```json
{
  "28FF64D1841703F1": "Attic",
  "28FF9A3B71120502": "Living Room"
}
```

Labels are persisted to `/dallas_labels.ini` on LittleFS and republished to MQTT.

---

#### Flash and Firmware

##### GET /api/v2/flash/status

Returns ESP flash memory usage and OTA partition status.

**Response:**

```json
{
  "sketch_size": 524288,
  "sketch_md5": "abc123...",
  "free_sketch_space": 1048576,
  "ota_running_partition": "app0",
  "ota_update_partition": "app1"
}
```

##### GET /api/v2/firmware/files

Returns available firmware files on GitHub (fetches the releases list). Used by the web UI OTA update page.

---

#### PIC Firmware

##### GET /api/v2/pic/flash-status

Returns PIC firmware flash operation status.

##### GET /api/v2/pic/update-check

Checks whether a newer PIC firmware is available.

##### GET /api/v2/pic/settings

Returns current PIC settings (queried via PR= commands). Only available when PIC is detected (`isPICEnabled()`).

---

#### SAT (Smart Autotune Thermostat)

The SAT endpoints require authentication for all mutation operations.

##### GET /api/v2/sat

Returns full SAT status. Add `?detail=full` for extended diagnostics including pressure metrics, cycle diagnostics, PID error statistics, and sync health.

**Response (abbreviated):**

```json
{
  "enabled": true,
  "active": true,
  "mode": "continuous",
  "setpoint": 43.1,
  "target": 21.0,
  "room_temp": 20.5,
  "outside_temp": 8.0,
  "heating_curve": 42.3,
  "pid_output": 43.1,
  "pid_p": 0.82,
  "pid_i": 0.03,
  "pid_d": -0.04,
  "boiler_status": 3,
  "flame_status": "healthy",
  "cycle_class": "good",
  "safety_tripped": false,
  "pressure": 1.5,
  "pressure_alarm": false
}
```

##### POST /api/v2/sat/target

Set the target room temperature.

**Request body:** Plain numeric string or JSON `{"value": "21.0"}`.

```
21.5
```

Valid range: 5.0 to 30.0 °C.

##### POST /api/v2/sat/externaltemp

Push an indoor temperature reading to the SAT PID controller. Used when the room temperature sensor is an external source (Node-RED, HA automation, BLE sensor).

**Request body:** Plain numeric string, e.g., `"20.8"`.

##### POST /api/v2/sat/externaloutdoor

Push an outdoor temperature reading. Overrides the OT MsgID 27 value and the Open-Meteo weather source.

**Request body:** Plain numeric string, e.g., `"7.5"`.

##### POST /api/v2/sat/humidity

Push indoor humidity reading (0-100%). Used for Summer Simmer Index calculation.

##### POST /api/v2/sat/preset

Apply a named comfort preset.

**Request body:** One of: `comfort`, `eco`, `away`, `sleep`, `activity`, `home`.

##### POST /api/v2/sat/window

Set window-open state. When open, SAT suppresses heating.

**Request body:** `"1"` (open) or `"0"` (closed).

##### POST /api/v2/sat/flush

Flush SAT short-lived data: resets the PID integral and cycle window. Useful after system disturbances (burner technician visit, manual boiler test).

##### POST /api/v2/sat/reset_integral

Reset only the PID integral accumulator.

##### POST /api/v2/sat/settings/{name}

Update a named SAT setting. Authentication required.

**Parameters:** `{name}` is the setting name, e.g., `heating_curve_coeff`, `boiler_capacity`, `deadband`.

**Request body:** New value as plain string.

---

#### OTDirect (ESP32 / OTGW32 only)

These endpoints are only available on OTGW32 hardware (`HAS_DIRECT_OT`).

##### GET /api/v2/otdirect/status

Returns OTDirect operating mode, schedule counts, thermostat connection status, and bypass relay state.

##### POST /api/v2/otdirect/mode

Set OTDirect operating mode.

**Query parameter:** `mode=gateway|monitor|bypass|master|loopback`

| Mode | Description |
|------|-------------|
| `gateway` | Full gateway: scheduler + thermostat forwarding + overrides (default) |
| `monitor` | Transparent pass-through: all frames forwarded unmodified |
| `bypass` | Thermostat wired directly to boiler via relay; OTDirect inactive |
| `master` | Standalone OT master: scheduler only, no thermostat expected |
| `loopback` | Internal test: simulated boiler responses, no hardware needed |

##### GET /api/v2/otdirect/overrides

Returns all active stored response overrides.

##### POST /api/v2/otdirect/overrides

Manage per-message-ID overrides.

**Query parameters:**
- `action=sr&msgid=X&value=HHHH` — set stored response for message ID X to 4-hex-digit value
- `action=cr&msgid=X` — clear stored response for message ID X
- `action=rm&msgid=X&value=HHHH` — set response modifier (mask applied to boiler response)
- `action=cm&msgid=X` — clear response modifier
- `action=ui&msgid=X` — mark message ID as unknown (gateway returns UNKNOWN_DATA_ID)
- `action=ki&msgid=X` — mark message ID as known again

---

#### Webhook

##### POST /api/v2/webhook/test

Trigger a test webhook delivery. Sends the configured webhook URL an HTTP POST with a test payload. No authentication required.

**Query parameter:** `state=on|off|1|0` — payload state to simulate.

---

#### Simulation

##### GET /api/v2/simulate

Returns current simulation mode status.

##### POST /api/v2/simulate/start

Enable OpenTherm message simulation mode. In this mode, fake OT frames are injected into the processing pipeline without hardware, allowing full MQTT and HA integration testing on a desk.

##### POST /api/v2/simulate/stop

Disable simulation mode.

---

#### Filesystem

##### GET /api/v2/filesystem/files

Returns a listing of all files in LittleFS with sizes.

##### GET /api/v2/filesystem/hash-check

Returns the filesystem content hash (used for cache-busting ETag on `index.html`).

---

### WebSocket API

The firmware runs a WebSocket server that streams live OpenTherm log frames to connected browsers. This is the data source for the live OT log viewer and the real-time temperature graphs in the web UI.

#### Connection

```
ws://{device-ip}:81/
```

The WebSocket server listens on **port 81** (separate from the HTTP server on port 80). No authentication is required. Maximum 3 simultaneous connections.

From JavaScript:

```javascript
const ws = new WebSocket("ws://otgw.local:81/");
ws.onopen  = () => console.log("connected");
ws.onclose = () => setTimeout(reconnect, 3000);  // auto-reconnect
ws.onmessage = (event) => handleFrame(event.data);
```

#### Message Format

Each WebSocket message is a JSON object on a single line. The `type` field discriminates the message kind.

**OpenTherm log frame:**

```json
{
  "type": "otlog",
  "frame": "B4001FF04",
  "msgid": 0,
  "msgtype": "READ_ACK",
  "source": "boiler",
  "decoded": {
    "flamestatus": true,
    "chmodus": true,
    "dhwmode": false,
    "faultindicator": false
  },
  "timestamp": 1712345678
}
```

| Field | Description |
|-------|-------------|
| `type` | Always `"otlog"` for OT frames |
| `frame` | Raw 9-character OT frame string as received from PIC |
| `msgid` | OpenTherm message ID (0-127) |
| `msgtype` | Frame type: `READ_DATA`, `WRITE_DATA`, `READ_ACK`, `WRITE_ACK`, `DATA_INVALID`, `UNKNOWN_DATA_ID` |
| `source` | Frame origin: `thermostat`, `boiler`, `gateway`, `answer` |
| `decoded` | Key-value pairs of decoded fields for this message ID (structure varies by ID) |
| `timestamp` | Unix timestamp at frame receipt |

**Keepalive ping:**

The server sends a ping frame every 15 seconds. The WebSocket library handles ping/pong automatically. If two consecutive pings go unanswered, the server closes the connection.

**Flash progress event:**

During OTA firmware updates, progress events are broadcast:

```json
{
  "type": "flash",
  "progress": 42,
  "total": 100,
  "phase": "writing"
}
```

**SAT status event:**

When SAT is active, state change events are broadcast on significant transitions:

```json
{
  "type": "sat",
  "event": "flame_on",
  "setpoint": 43.1,
  "mode": "continuous"
}
```

#### Reconnection Behavior

The browser SPA implements automatic reconnection with a 3-second delay after disconnection. If the device reboots, the browser detects the close event and reconnects when the device comes back online. There is no message history; the stream is live-only.

#### Heap Backpressure

When free heap falls below the warning threshold, the server stops sending WebSocket messages (`canSendWebSocket()` returns false). This protects against heap exhaustion under heavy MQTT + HTTP + WebSocket concurrent load. Messages dropped during this period are lost; the stream resumes when heap recovers.

---

### MQTT API

The MQTT interface is the primary integration method for Home Assistant and other home automation platforms.

#### Broker Configuration

Configure in Settings (web UI or REST API):

| Setting | Default | Description |
|---------|---------|-------------|
| `mqttbroker` | (empty) | Broker hostname or IP |
| `mqttport` | 1883 | Broker TCP port |
| `mqttuser` | (empty) | Username (optional) |
| `mqttpassword` | (empty) | Password (optional) |
| `mqttenable` | false | Enable MQTT client |
| `mqtttoptopic` | `OTGW` | Top-level topic namespace |
| `mqttuniqueid` | `otgw-{MAC}` | Unique device ID in topics |

#### Topic Namespace

All topics follow this structure:

```
{TopTopic}/value/{UniqueId}/{subtopic}      ← published by firmware
{TopTopic}/set/{UniqueId}/{command}         ← subscribed by firmware
```

Default example (with `TopTopic=OTGW`, `UniqueId=otgw-AABBCCDDEEFF`):

```
OTGW/value/otgw-AABBCCDDEEFF/boilertemperature    ← "62.31"
OTGW/set/otgw-AABBCCDDEEFF/setpoint               ← "21.5"
```

#### Connection Lifecycle

On connect:
1. Publishes `"online"` to `{TopTopic}/value/{UniqueId}` (retained). This is the birth message.
2. Configures Last Will to publish `"offline"` to the same topic on disconnect (retained).
3. Subscribes to `{TopTopic}/set/{UniqueId}/#` for incoming commands.
4. Subscribes to `homeassistant/status` for Home Assistant lifecycle detection.
5. Clears discovery state and triggers JIT (just-in-time) republishing of HA discovery payloads.
6. Publishes firmware version, PIC status, and device info.

#### Published Topics

All topics under the publish namespace are retained by default unless noted otherwise.

##### Firmware and Device Information

Published at startup, on reconnect, and every 5 minutes.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otgw-firmware/version` | `"2.0.0"` | Firmware version string |
| `otgw-firmware/reboot_count` | `"7"` | Total reboots since first flash |
| `otgw-firmware/reboot_reason` | `"Software/System restart"` | Last reboot cause |
| `otgw-firmware/uptime` | `"86400"` | Uptime in seconds (not retained) |

##### PIC Gateway Status

Published at startup, on reconnect, and every 5 minutes.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otgw-pic/version` | `"6.6"` | PIC firmware version |
| `otgw-pic/picavailable` | `"ON"` / `"OFF"` | PIC detected and responding |
| `otgw-pic/gateway_mode` | `"ON"` / `"OFF"` | Gateway mode active |
| `otgw-pic/boiler_connected` | `"ON"` / `"OFF"` | Boiler OT traffic seen |
| `otgw-pic/thermostat_connected` | `"ON"` / `"OFF"` | Thermostat OT traffic seen |
| `otgw-pic/otgw_connected` | `"ON"` / `"OFF"` | Serial link to PIC alive |

##### OpenTherm Status Flags (Message ID 0)

Published when flag values change.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `chenable` | `"ON"` / `"OFF"` | Central heating enable (thermostat to boiler) |
| `dhwenable` | `"ON"` / `"OFF"` | DHW (hot water) enable |
| `flamestatus` | `"ON"` / `"OFF"` | Burner flame active |
| `chmodus` | `"ON"` / `"OFF"` | Central heating mode active (boiler to thermostat) |
| `dhwmode` | `"ON"` / `"OFF"` | DHW mode active |
| `faultindicator` | `"ON"` / `"OFF"` | Boiler fault indicator |
| `diagnosticindicator` | `"ON"` / `"OFF"` | Diagnostic indicator |
| `status_master` | text | Full master status word as text |
| `status_slave` | text | Full slave status word as text |

##### OpenTherm Numeric Values

Published when the corresponding message ID is received from the boiler or thermostat. Values are decimal strings. All temperatures in degrees Celsius, pressure in bar, modulation in percent.

| Topic suffix | Unit | OpenTherm Message ID |
|-------------|------|---------------------|
| `controlsetpoint` | °C | 1 |
| `roomsetpoint` | °C | 16 |
| `roomtemperature` | °C | 24 |
| `boilertemperature` | °C | 25 |
| `dhwtemperature` | °C | 26 |
| `outsidetemperature` | °C | 27 |
| `returnwatertemperature` | °C | 28 |
| `dhwsetpoint` | °C | 56 |
| `maxchwatersetpoint` | °C | 57 |
| `chwaterpressure` | bar | 18 |
| `relmodlvl` | % | 17 |
| `maxrelmodlvl` | % | 14 |
| `dhw_flowrate` | l/min | 13 |
| `exhaust_temperature` | °C | 33 |
| `boiler_fan_speed` | rpm | 35 |
| `electrical_current_burner_flame` | µA | 36 |

The exact set of published topics depends on which message IDs your boiler and thermostat exchange. Any message ID with a defined label generates a corresponding topic.

##### Source-Separated Topics (Optional)

When `mqttseparatesources` is enabled:

```
{TopTopic}/value/{UniqueId}/{label}/thermostat   ← thermostat-side value (T-prefix frame)
{TopTopic}/value/{UniqueId}/{label}/boiler       ← boiler-side value (B-prefix frame)
{TopTopic}/value/{UniqueId}/{label}/gateway      ← gateway-injected value (A-prefix frame)
```

This allows Home Assistant automations to distinguish between what the thermostat requested and what the boiler confirmed.

##### S0 Pulse Counter

Published when S0 counting is enabled.

| Topic suffix | Unit | Description |
|-------------|------|-------------|
| `s0pulsecount` | pulses | Pulse count in current interval |
| `s0pulsecounttot` | pulses | Total pulse count since boot |
| `s0powerkw` | kW | Calculated instantaneous power |

##### Dallas Temperature Sensors

Published when sensors are enabled. The topic suffix is the 1-Wire address (e.g., `28FF64D1841703F1`) or the assigned label if `gpiosensorslegacyformat` is off.

| Topic suffix | Unit | Description |
|-------------|------|-------------|
| `{address}` | °C | Temperature from DS18B20 sensor |

##### SAT Topics

Published every SAT control loop interval (default 30 seconds) when SAT is enabled.

| Topic suffix | Retained | Description |
|-------------|---------|-------------|
| `sat/mode` | yes | `"off"`, `"continuous"`, `"pwm"` |
| `sat/active` | yes | `"true"` when SAT is actively controlling |
| `sat/setpoint` | yes | Final flow temperature setpoint sent to boiler (°C) |
| `sat/target` | yes | Target room temperature (°C) |
| `sat/heating_curve` | yes | Heating curve calculated value (°C) |
| `sat/pid_output` | yes | PID controller output = curve + P + I + D |
| `sat/error` | no | PID error = target - room temperature (°C) |
| `sat/pid_p` | no | Proportional term |
| `sat/pid_i` | no | Integral term |
| `sat/pid_d` | no | Derivative term |
| `sat/room_temp` | no | Room temperature used by PID (°C) |
| `sat/outside_temp` | no | Outside temperature used by heating curve (°C) |
| `sat/boiler_status` | no | Boiler state code (0=Off ... 14=Cooling) |
| `sat/flame_status` | no | `"healthy"`, `"stuck_on"`, `"short_cycling"`, etc. |
| `sat/cycle_class` | no | `"good"`, `"overshoot"`, `"underheat"`, `"short"` |
| `sat/safety_tripped` | no | `"true"` when safety shutdown is active |
| `sat/pressure` | no | Smoothed system water pressure (bar) |
| `sat/pressure_alarm` | no | `"true"` when pressure alarm is active |
| `sat/power` | no | Estimated boiler power (kW) |
| `sat/energy_total` | yes | Cumulative energy usage (kWh, for HA energy dashboard) |
| `sat/curve_recommendation` | no | `"increase"`, `"decrease"`, `"hold"`, `"insufficient"` |
| `sat/preset_comfort` | yes | Comfort preset temperature (°C) |
| `sat/preset_eco` | yes | Eco preset temperature (°C) |
| `sat/preset_away` | yes | Away preset temperature (°C) |
| `sat/preset_sleep` | yes | Sleep preset temperature (°C) |
| `sat/thermal_coeff` | yes | Learned thermal drop coefficient |
| `sat/window_open` | no | Window-open detection state |
| `sat/solar_gain` | no | Solar gain compensation active |
| `sat/summer_active` | no | Summer simmer suppression active |

Additional settings echo topics, BLE sensor topics, and multi-zone topics are documented in `backlog/docs/doc-1 - sat-mqtt-topics.md`.

##### Raw OpenTherm Message (Optional)

When `mqttotmessage` is enabled:

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otmessage` | `"T80000200"` | Raw OpenTherm frame string as received from PIC |

#### Subscribed Topics (Commands)

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#`.

##### Direct Control Commands

Publish a plain text payload to these topics:

| Topic suffix | Payload | OTGW command | Description |
|-------------|---------|-------------|-------------|
| `setpoint` | `"21.5"` | `TT=21.50` | Temporary room temperature setpoint |
| `constant` | `"21.5"` | `TC=21.50` | Constant room temperature setpoint (persists over thermostat cycles) |
| `outside` | `"8.0"` | `OT=8.0` | Outside temperature override |
| `hotwater` | `"1"` / `"0"` / `"P"` | `HW=1` / `HW=0` / `HW=P` | Hot water on / off / one-time push |
| `gatewaymode` | `"1"` / `"0"` | `GW=1` / `GW=0` | Gateway mode / monitor mode |
| `maxchsetpt` | `"80"` | `SH=80` | Max CH water setpoint (°C) |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max DHW setpoint (°C) |
| `maxmodulation` | `"100"` | `MM=100` | Max relative modulation level (%) |
| `ctrlsetpt` | `"55.0"` | `CS=55.0` | Direct control setpoint (bypasses thermostat) |
| `chenable` | `"1"` / `"0"` | `CH=1` / `CH=0` | Central heating enable bit |
| `summermode` | `"1"` / `"0"` | `SM=1` / `SM=0` | Summer mode (persisted to flash) |
| `command` | `"TT=21.50"` | (raw) | Raw OTGW command passthrough |

##### SAT Command Topics

| Topic suffix | Payload | Description |
|-------------|---------|-------------|
| `sat/target` | `"21.0"` | Set target room temperature |
| `sat/enable` | `"on"` / `"off"` | Enable / disable SAT |
| `sat/mode` | `"continuous"` / `"pwm"` / `"off"` | Set control mode |
| `sat/preset` | `"comfort"` / `"eco"` / `"away"` / `"sleep"` | Apply comfort preset |
| `sat/externaltemp` | `"20.8"` | Push external room temperature |
| `sat/externaloutdoor` | `"7.5"` | Push external outdoor temperature |
| `sat/humidity` | `"58"` | Push indoor humidity (%) |
| `sat/window` | `"1"` / `"0"` | Set window open / closed state |

#### Home Assistant Auto-Discovery

When `mqtthadiscovery` is enabled, the firmware publishes MQTT discovery payloads to the `homeassistant/` prefix (configurable via `mqtthaprefix`). Home Assistant processes these payloads and creates entities automatically.

Discovery is triggered:
- On MQTT connect (just-in-time, per message ID as data is received).
- When `POST /api/v2/otgw/discovery` is called.
- When Home Assistant sends `"online"` to `homeassistant/status`.

Discovery payloads include device metadata, entity names, unit of measurement, device class, and state topics. The firmware creates approximately 200+ entities covering all sensors, binary sensors, and a climate entity for SAT.

---

### Telnet Debug Console

The firmware streams diagnostic output to a Telnet server on port 23.

#### Connecting

```bash
telnet otgw.local 23
# or by IP
telnet 192.168.1.50 23
```

On Windows, use PuTTY (Telnet mode, port 23) or install the optional Windows Telnet client.

No authentication is required. The debug output is read-only; the Telnet connection does not accept commands.

#### Output Format

The stream is plain ASCII, one line per event. Output lines include:

- OpenTherm message decoding: message ID, type, values.
- MQTT state machine transitions: connecting, connected, disconnected, retrying.
- Settings load/save events.
- Sensor readings (Dallas, S0) on each measurement cycle.
- SAT control loop output: setpoint calculation, PID terms, boiler state.
- Heap statistics: free heap, heap health level.
- Network events: WiFi connect/disconnect, IP address.
- Error messages and warnings.

Example output:

```
[OTGW] handleOTGW: B4001FF04 msgid=0 (Status flags)
[OTGW]   flamestatus=ON chmodus=ON
[MQTT] sendMQTTData: OTGW/value/otgw-AABBCC/flamestatus = ON
[SAT]  Control loop: Tr=20.5 Tboiler=62.3 setpoint=43.1 error=0.50
[HEAP] Free: 19234 bytes (healthy)
```

The verbosity of each category is controlled by boolean flags in `state.debug`:

| Flag | Category | Default |
|------|---------|---------|
| `bOTmsg` | OpenTherm message trace | true |
| `bMQTT` | MQTT publish/receive trace | false |
| `bRestAPI` | REST API request trace | false |
| `bSensors` | Dallas sensor scan trace | false |
| `bSAT` | SAT control loop trace | true |
| `bOTDirect` | OTDirect frame handling | true |

Debug flags can be toggled at runtime via the REST API settings endpoint or via web UI settings. High-verbosity flags (`bMQTT`, `bRestAPI`) should be disabled in normal operation to avoid flooding the Telnet stream and consuming heap.

#### TCP Serial Bridge (Port 25238)

A separate TCP socket on port 25238 exposes the raw OTGW serial stream. This is for tools like OTmonitor that need raw protocol access:

```bash
nc otgw.local 25238
# or
telnet otgw.local 25238
```

The output is the unprocessed ASCII stream from the PIC: lines like `T80000200`, `B4001FF04`, `R4001FF04`. You can also send raw PIC commands by typing them followed by a newline. This port is separate from the debug Telnet console.

---

### Port Reference

| Port | Protocol | Service |
|------|----------|---------|
| 80 | TCP (HTTP) | Web UI, REST API v2, file serving |
| 81 | TCP (WebSocket) | Live OT log stream, real-time graphs |
| 23 | TCP (Telnet) | Debug console output (read-only) |
| 25238 | TCP (raw) | TCP serial bridge to OTGW PIC |
| 1883 | TCP (MQTT) | MQTT broker connection (outbound) |
| 123 | UDP (SNTP) | NTP time synchronization (outbound) |
| 5353 | UDP (mDNS) | `otgw.local` name resolution |
| 5355 | UDP (LLMNR) | Windows name resolution |
