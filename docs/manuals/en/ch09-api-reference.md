## Chapter 9: API Reference

This chapter documents all programmatic interfaces exposed by the OTGW-firmware: the REST API, the WebSocket stream, the MQTT integration, and the Telnet debug console. These are the interfaces used by Home Assistant, Node-RED flows, automation scripts, and custom integrations.

This chapter is a user-facing narrative. For the exhaustive machine-readable specification (request/response schemas, status codes, examples), consult the OpenAPI 3.1 document in the repository:

- `docs/api/openapi.yaml` : complete REST API specification, authoritative source
- `docs/api/MQTT.md` : full MQTT topic reference and payload examples
- `docs/api/WEBSOCKET_FLOW.md` : WebSocket connection lifecycle and message framing

When this chapter and the OpenAPI document disagree, the OpenAPI document wins. Raise an issue so the chapter can be corrected.

---

### REST API Overview

#### Base URL and Versioning

Three API version prefixes are reserved. Only one is active:

| Version | Path | Status |
|---------|------|--------|
| v0 | `/api/v0/` | Legacy, removed. Returns HTTP 410 Gone. |
| v1 | `/api/v1/` | Standard (removed in 2.x). Returns HTTP 410 Gone. |
| v2 | `/api/v2/` | Current preferred. All new integrations must use this prefix. |

New code must always use `/api/v2/`:

```
http://{device-ip}/api/v2/
```

The device hostname defaults to `otgw.local` (mDNS / LLMNR). Alternatively, use the IP address assigned by your DHCP server.

#### Route Dispatch (ADR-050)

Internally, `/api/v2/` requests are dispatched by a static route table `kV2Routes[]` defined in `src/OTGW-firmware/restAPI.ino`. Each top-level resource segment maps to a single handler function:

`health`, `settings`, `sensors`, `device`, `flash`, `pic`, `otdirect` (OTGW32 only), `firmware`, `filesystem`, `simulate`, `otgw`, `webhook`, `sat`.

Adding a new endpoint is a two-line change: register a handler and add one entry to `kV2Routes[]`. This is why the API is stable across builds: the dispatch surface is small, explicit, and reviewed.

#### Transport

- HTTP only, no HTTPS (ADR-003). The device operates on a trusted local network. For remote access, place it behind a reverse proxy or VPN.
- Content-Type for all responses: `application/json`
- All JSON responses are UTF-8

#### Authentication

All POST and PUT requests require HTTP Basic Authentication when an HTTP password is configured in Settings. The username is the device hostname (default `OTGW`). Authentication is checked centrally before any mutation handler runs.

GET endpoints for sensitive data (settings, PIC settings) also require authentication.

Unauthenticated GET endpoints (health, sensor data, OT values) are intentionally public.

CSRF same-origin validation is enforced for authenticated browser requests: the `Origin`/`Referer` header must match the `Host` header (ADR-054).

#### Error Response Format

All errors are produced by the helper `sendApiError(httpCode, F("message"))` and use a consistent JSON envelope:

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
| 413 | Request Entity Too Large: command string too long |
| 414 | URI Too Long |
| 500 | Internal Server Error |
| 503 | Service Unavailable: low heap, no OT command interface, etc. |

---

### REST API Endpoints

The sections below describe the most commonly used endpoints with usage notes and examples. This is not an exhaustive list. For the complete set of endpoints, including every field, enum value, and error shape, see `docs/api/openapi.yaml`. You can render that file as interactive documentation with Swagger UI, Redoc, or any OpenAPI-compatible tool.

#### Health

##### GET /api/v2/health

Returns system health metrics. No authentication required.

**Side effect:** Each call writes a small probe file (`/.health`) to LittleFS to verify the filesystem is writable. Poll at 30-60 second intervals minimum.

**Request:** None

**Response:**

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
| `heap` | integer | Free heap in bytes |
| `networkmode` | string | `"wifi"`, `"ethernet"`, or `"ap"` |
| `wifirssi` | integer | WiFi signal strength in dBm (0 in AP or Ethernet mode) |
| `mqttconnected` | string | MQTT broker connection status |
| `otgwconnected` | string | PIC or OTDirect link active |
| `picavailable` | string | PIC co-processor detected and responding |
| `littlefsMounted` | string | LittleFS filesystem healthy |

This endpoint is safe to poll at 30-60 second intervals. Typical use: OTA update completion detection, watchdog integration, Home Assistant availability sensor.

---

#### Settings

##### GET /api/v2/settings

Returns all device settings. Authentication required.

**Request:** None

**Response:** A JSON object containing all configurable settings fields. Example subset:

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
  "satenable": false,
  "nightlyrestart": false,
  "nightlyrestarthour": 4
}
```

The `ssid` field is read-only and returns the currently connected WiFi SSID. On Ethernet, `ssid` is not present; check `/api/v2/device/info` for `"ssid": "Wired"`. The `httppassword` field returns a masked placeholder, never the actual password.

**Nightly restart settings:** The `nightlyrestart` boolean enables a scheduled daily reboot. The `nightlyrestarthour` integer (0-23) controls the hour at which the restart occurs.

##### POST /api/v2/settings

Update one or more settings. Authentication required.

**Request body:** JSON object with setting name and value.

```json
{
  "name": "mqttbroker",
  "value": "192.168.1.20"
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

**Settings whitelist:** Only known setting names are accepted. The full list includes: `hostname`, `httppasswd`, `ledblink`, `nightlyrestart`, `nightlyrestarthour`, `mqttenable`, `mqttbroker`, `mqttbrokerport`, `mqttuser`, `mqttpasswd`, `mqtttoptopic`, `mqtthaprefix`, `mqttharebootdetection`, `mqttuniqueid`, `mqttotmessage`, `mqttinterval`, `mqttseparatesources`, `ntpenable`, `ntptimezone`, `ntphostname`, `ntpsendtime`, `darktheme`, `gpiosensorsenabled`, `gpiosensorspin`, `gpiosensorsinterval`, `s0counterenabled`, `s0counterpin`, `s0counterpulsekw`, `s0counterinterval`, `gpiooutputsenabled`, `gpiooutputspin`, `gpiooutputstriggerbit`, `otgwcommandenable`, `otgwcommands`, `webhookenable`, `webhookurlon`, `webhookurloff`, `webhooktriggerbit`, `webhookpayload`, `webhookcontenttype`, `satenabled`, `satsystem`, `sattargettemp`, `satcoefficient`, `satdeadband`, `satinterval`, and many more SAT-related settings.

---

#### Device Information

##### GET /api/v2/device/info

Returns hardware and platform information. No authentication required.

**Response:**

```json
{
  "device": {
    "author": "Robert van den Breemen",
    "fwversion": "2.0.0",
    "hostname": "OTGW",
    "board": "esp8266",
    "hardwaremode": "pic",
    "networkmode": "wifi",
    "ipaddress": "192.168.1.50",
    "macaddress": "AA:BB:CC:DD:EE:FF",
    "ssid": "MyHomeNetwork",
    "cpufreq": 80,
    "freeheap": 25600,
    "maxfreeblock": 20480,
    "chipid": "AABBCC",
    "picavailable": true,
    "picfwversion": "5.4",
    "bootcount": 42,
    "lastreset": "Software/System restart",
    "mqttconnected": true,
    "otgwconnected": true,
    "otcommandinterface": true,
    "otdirectavailable": false
  }
}
```

On OTGW32 hardware: the `otdirectavailable` field is `true`, and additional `otd*` fields are included (`otdmode`, `otdbypass`, `otdmonitor`, `otdmaster`, `otdstepup`, `otdthermostat`, `otdsetback`, `otdschedtotal`, `otdschedactive`, `otdscheddisabled`, `otdoverrides`). On standard ESP8266+PIC builds, only `otdirectavailable: false` is present.

The `otcommandinterface` field is `true` when either a PIC or OT-direct hardware interface is present and active.

##### GET /api/v2/device/time

Returns current device time.

**Response:**

```json
{
  "devtime": {
    "dateTime": "2026-04-12T10:00:00+02:00",
    "epoch": 1712870400,
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

##### GET /api/v2/device/crashlog

Returns the most recent crash log summary from LittleFS. Returns empty object if no crash log exists. No authentication required.

---

#### OpenTherm Gateway

##### GET /api/v2/otgw/otmonitor

Returns all current OpenTherm values in a structured key-value format. Each field includes its value, unit, and last-updated timestamp. Compatible with Telegraf and OTmonitor.

Alias: `GET /api/v2/otgw/telegraf`

##### GET /api/v2/otgw/messages/{id}

Returns the current value for a specific OpenTherm message ID.

**Parameters:** `{id}` is the numeric message ID (0-127).

**Response:**

```json
{
  "label": "boilertemperature",
  "value": 62.31,
  "unit": "C"
}
```

Alias: `GET /api/v2/otgw/id/{id}`

##### GET /api/v2/otgw/label/{label}

Returns the current value for a specific OpenTherm label string.

**Parameters:** `{label}` is the label name (case-insensitive), e.g., `boilertemperature`, `roomtemperature`, `relmodlvl`.

##### POST /api/v2/otgw/commands

Send a raw command to the OTGW PIC (or OTDirect on ESP32). Authentication required.

**Request body (JSON):**

```json
{
  "command": "TT=21.50"
}
```

Alternatively, the command string can be sent as a plain text body without JSON wrapping.

**Response (HTTP 202 Accepted):**

```json
{
  "status": "queued"
}
```

**Error (HTTP 503):** Returned when no OT command interface (PIC or OTDirect) is detected.

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

Alias: `POST /api/v2/otgw/command` (backward compatibility)

##### POST /api/v2/otgw/discovery

Force a full Home Assistant MQTT auto-discovery republish. Authentication required.

**Request:** No body required.

**Response (HTTP 202 Accepted):**

```json
{
  "status": "accepted"
}
```

Discovery uses an async bitmap-driven drip publisher: all message IDs are marked pending, then published one at a time from the main loop at a controlled pace (see "MQTT Discovery Drip Publisher" below). This avoids a burst of 200+ messages that could exhaust heap or overwhelm the broker.

Alias: `POST /api/v2/otgw/autoconfigure`

---

#### Sensors

##### GET /api/v2/sensors

Returns all Dallas DS18B20 sensor data and S0 pulse counter status. No authentication required.

##### GET /api/v2/sensors/status

Alias of `GET /api/v2/sensors`. Added in 2.x so external monitoring tools can poll a predictable `/status` path per the convention used by `/health` and `/sat/status`. Both paths return the identical payload:

**Response:**

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
      "28FF9A3B71120502": {"temp": 18.3, "epoch": 1774548600}
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

##### GET /api/v2/firmware/files

Returns available firmware files on the filesystem. Used by the web UI OTA update page.

---

#### PIC Firmware

##### GET /api/v2/pic/flash-status

Returns PIC firmware flash operation status.

##### GET /api/v2/pic/update-check

Checks whether a newer PIC firmware is available (makes outbound HTTP request to otgw.tclcode.com).

##### GET /api/v2/pic/settings

Returns current PIC settings (queried via PR= commands). Only available when a PIC or OTDirect interface is detected.

---

#### SAT (Smart Autotune Thermostat)

All SAT endpoints require authentication.

##### GET /api/v2/sat

Returns full SAT status. Alias: `GET /api/v2/sat/status`.

Add `?detail=full` for extended diagnostics including synchronization health, pressure metrics, cycle diagnostics, PID error statistics, and auto-tune scores.

**Response (abbreviated):**

```json
{
  "enabled": true,
  "active": true,
  "control_mode": 1,
  "target_temp": 21.0,
  "room_temp": 20.5,
  "outside_temp": 8.0,
  "heating_curve": 42.3,
  "pid_output": 43.1,
  "final_setpoint": 43.1,
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

Valid range: 5.0 to 30.0 degrees C.

##### POST /api/v2/sat/externaltemp

Push an indoor temperature reading to the SAT PID controller. Used when the room temperature sensor is an external source (Node-RED, HA automation, BLE sensor). Expires after 5 minutes if not refreshed.

**Request body:** Plain numeric string, e.g., `"20.8"`.

##### POST /api/v2/sat/externaloutdoor

Push an outdoor temperature reading. Overrides the OT MsgID 27 value and the Open-Meteo weather source. Expires after 10 minutes if not refreshed.

**Request body:** Plain numeric string, e.g., `"7.5"`.

##### POST /api/v2/sat/humidity

Push indoor humidity reading (0-100%). Used for Summer Simmer Index calculation.

##### POST /api/v2/sat/preset

Apply a named comfort preset.

**Request body:** One of: `comfort`, `eco`, `away`, `sleep`, `activity`, `home`.

##### POST /api/v2/sat/enable

Enable or disable the SAT controller.

**Request body:** `"true"` / `"false"` or `"1"` / `"0"`.

##### POST /api/v2/sat/mode

Set SAT control mode.

**Request body:** `"continuous"`, `"pwm"`, or `"auto"`.

##### POST /api/v2/sat/window

Set window-open state. When open, SAT suppresses heating.

**Request body:** `"open"` / `"closed"`, `"1"` / `"0"`, or `"ON"`.

##### POST /api/v2/sat/flush

Flush SAT short-lived data: resets the PID integral and cycle window. Useful after system disturbances (burner technician visit, manual boiler test).

##### POST /api/v2/sat/reset_integral

Reset only the PID integral accumulator.

##### POST /api/v2/sat/area/{index}

Push a temperature reading for a specific area (multi-area mode). Index range: 0-3.

**Request body:** Plain numeric string, e.g., `"20.5"`.

##### GET /api/v2/sat/weather

Returns the current weather data used by the SAT heating curve (outside temperature source, forecast, etc.).

##### POST /api/v2/sat/settings/{name}

Update a named SAT setting. Authentication required.

**Parameters:** `{name}` is the setting name, e.g., `heating_curve`, `boiler_capacity`, `deadband`, `max_modulation`, `summer_threshold`, `ovp_value`, `ovp_enabled`, `preset_comfort`, `preset_eco`, `preset_away`, `preset_sleep`, and many more.

**Request body:** New value as plain string or JSON `{"value": "..."}`.

This endpoint mirrors all the MQTT `sat/*` setting commands, providing parity between REST and MQTT configuration.

---

#### OTDirect (ESP32 / OTGW32 only)

These endpoints are only available on OTGW32 hardware (`HAS_DIRECT_OT`). On standard ESP8266+PIC hardware, all endpoints return 503 Service Unavailable.

##### GET /api/v2/otdirect/status

Returns OTDirect operating mode, schedule counts, thermostat connection status, bypass relay state, and boiler connection state.

**Response:**

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

##### GET /api/v2/otdirect/settings

Returns the persisted OT-direct configuration settings, including heating curve and PI room compensation parameters.

**Response:**

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
    "kp": 1.0,
    "ki": 0.01,
    "kboost": 2.0
  }
}
```

##### POST /api/v2/otdirect/settings

Update OT-direct settings. Only provided parameters are updated. Accepts form parameters: `setbacktemp`, `setbacktimeout`, `chmode`, `flowtemp`, `flowmax`, `roomsetpoint`, `gradient`, `exponent`, `offset`, `roomcomp`, `kp`, `ki`, `kboost`.

##### GET /api/v2/otdirect/overrides

Returns all active stored response overrides and response modifiers. Uses chunked JSON streaming transfer for potentially large override lists.

##### POST /api/v2/otdirect/overrides

Manage per-message-ID overrides.

**Query parameters:**
- `action=sr&msgid=X&value=HHHH` : set stored response for message ID X to 4-hex-digit value
- `action=cr&msgid=X` : clear stored response for message ID X
- `action=rm&msgid=X&value=HHHH` : set response modifier (mask applied to boiler response)
- `action=cm&msgid=X` : clear response modifier
- `action=ui&msgid=X` : mark message ID as unknown (gateway returns UNKNOWN_DATA_ID)
- `action=ki&msgid=X` : mark message ID as known again

---

#### Webhook

##### POST /api/v2/webhook/test

Trigger a test webhook delivery. Sends the configured webhook URL an HTTP POST with a test payload. Authentication required.

**Query parameter:** `state=on|off|1|0` : payload state to simulate.

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

Returns the filesystem content hash compared to the firmware hash. Used to detect mismatches after a firmware-only OTA update.

---

### WebSocket API

The firmware runs a WebSocket server that streams live OpenTherm log frames to connected browsers. This is the data source for the live OT log viewer and the real-time temperature graphs in the web UI.

For the full connection lifecycle, broadcast gating rules, and heap-backpressure state machine, see `docs/api/WEBSOCKET_FLOW.md`.

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

For the complete topic inventory (every published value, every accepted command, retained/non-retained flags, and payload examples), see `docs/api/MQTT.md`. The tables in this chapter are a quick reference covering the most commonly used topics. Incoming command topics are dispatched by `handleMQTTcallback()` in `MQTTstuff.ino`, which parses the trailing topic segment and routes it to the corresponding handler.

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
{TopTopic}/value/{UniqueId}/{subtopic}      <-- published by firmware
{TopTopic}/set/{UniqueId}/{command}         <-- subscribed by firmware
```

Default example (with `TopTopic=OTGW`, `UniqueId=otgw-AABBCCDDEEFF`):

```
OTGW/value/otgw-AABBCCDDEEFF/boilertemperature    <-- "62.31"
OTGW/set/otgw-AABBCCDDEEFF/setpoint               <-- "21.5"
```

#### Connection Lifecycle

On connect:
1. Publishes `"online"` to `{TopTopic}/value/{UniqueId}` (retained). This is the birth message.
2. Configures Last Will to publish `"offline"` to the same topic on disconnect (retained).
3. Subscribes to `{TopTopic}/set/{UniqueId}/#` for incoming commands.
4. Subscribes to `homeassistant/status` for Home Assistant lifecycle detection.
5. Requests republish of all OT values so HA gets current state after reconnect.
6. Publishes firmware version, PIC status, and device info.

Note: Discovery state is intentionally not cleared on reconnect. MQTT retained messages survive an ESP reconnect on the broker. Discovery is only re-triggered on (a) firmware boot or MQTT settings change, and (b) Home Assistant restart (detected via `homeassistant/status` going offline then online).

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
| `otgw-firmware/board` | `"esp8266"` | Hardware board identifier |
| `otgw-firmware/hardware_mode` | `"pic"` | Active hardware mode (`pic` or `otdirect`) |
| `otgw-firmware/network_mode` | `"wifi"` | Network mode (`wifi`, `ethernet`, or `ap`) |

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

##### OT Direct Status (OTGW32 only)

Published only when OT-direct hardware is enabled. On standard ESP8266+PIC, only `otgw-otdirect/available` is published with value `"OFF"`.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otgw-otdirect/available` | `"ON"` / `"OFF"` | OT-direct hardware present |
| `otgw-otdirect/mode` | `"gateway"` | Current operating mode |
| `otgw-otdirect/bypass` | `"ON"` / `"OFF"` | Bypass relay active |
| `otgw-otdirect/thermostat_connected` | `"ON"` / `"OFF"` | Thermostat frame received within timeout |
| `otgw-otdirect/setback_active` | `"ON"` / `"OFF"` | Setback override engaged |
| `otgw-otdirect/schedule_active` | `"11"` | Active schedule entry count |
| `otgw-otdirect/overrides_active` | `"2"` | Active write-override count |
| `otgw-otdirect/boiler_connected` | `"ON"` / `"OFF"` | Boiler OT bus active |
| `otgw-otdirect/ot_online` | `"ON"` / `"OFF"` | OT serial bus alive |

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
| `controlsetpoint` | C | 1 |
| `roomsetpoint` | C | 16 |
| `roomtemperature` | C | 24 |
| `boilertemperature` | C | 25 |
| `dhwtemperature` | C | 26 |
| `outsidetemperature` | C | 27 |
| `returnwatertemperature` | C | 28 |
| `dhwsetpoint` | C | 56 |
| `maxchwatersetpoint` | C | 57 |
| `chwaterpressure` | bar | 18 |
| `relmodlvl` | % | 17 |
| `maxrelmodlvl` | % | 14 |
| `dhw_flowrate` | l/min | 13 |
| `exhaust_temperature` | C | 33 |
| `boiler_fan_speed` | rpm | 35 |
| `electrical_current_burner_flame` | uA | 36 |

The exact set of published topics depends on which message IDs your boiler and thermostat exchange. Any message ID with a defined label generates a corresponding topic.

##### Source-Separated Topics (Optional)

When `mqttseparatesources` is enabled:

```
{TopTopic}/value/{UniqueId}/{label}/thermostat   <-- thermostat-side value (T-prefix frame)
{TopTopic}/value/{UniqueId}/{label}/boiler       <-- boiler-side value (B-prefix frame)
{TopTopic}/value/{UniqueId}/{label}/gateway      <-- gateway-injected value (A-prefix frame)
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

##### SAT Topics

Published every SAT control loop interval (default 30 seconds) when SAT is enabled.

| Topic suffix | Retained | Description |
|-------------|---------|-------------|
| `sat/mode` | yes | `"off"`, `"continuous"`, `"pwm"` |
| `sat/active` | yes | `"true"` when SAT is actively controlling |
| `sat/setpoint` | yes | Final flow temperature setpoint sent to boiler (C) |
| `sat/target` | yes | Target room temperature (C) |
| `sat/heating_curve` | yes | Heating curve calculated value (C) |
| `sat/pid_output` | yes | PID controller output = curve + P + I + D |
| `sat/error` | no | PID error = target - room temperature (C) |
| `sat/pid_p` | no | Proportional term |
| `sat/pid_i` | no | Integral term |
| `sat/pid_d` | no | Derivative term |
| `sat/room_temp` | no | Room temperature used by PID (C) |
| `sat/outside_temp` | no | Outside temperature used by heating curve (C) |
| `sat/boiler_status` | no | Boiler state code (0=Off ... 14=Cooling) |
| `sat/flame_status` | no | `"healthy"`, `"stuck_on"`, `"short_cycling"`, etc. |
| `sat/cycle_class` | no | `"good"`, `"overshoot"`, `"underheat"`, `"short"` |
| `sat/safety_tripped` | no | `"true"` when safety shutdown is active |
| `sat/pressure` | no | Smoothed system water pressure (bar) |
| `sat/pressure_alarm` | no | `"true"` when pressure alarm is active |
| `sat/power` | no | Estimated boiler power (kW) |
| `sat/energy_total` | yes | Cumulative energy usage (kWh, for HA energy dashboard) |
| `sat/curve_recommendation` | no | `"increase"`, `"decrease"`, `"hold"`, `"insufficient"` |
| `sat/preset_comfort` | yes | Comfort preset temperature (C) |
| `sat/preset_eco` | yes | Eco preset temperature (C) |
| `sat/preset_away` | yes | Away preset temperature (C) |
| `sat/preset_sleep` | yes | Sleep preset temperature (C) |
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

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#`. The trailing segment after `set/{UniqueId}/` selects the command; `handleMQTTcallback()` routes each match to the matching OTGW PIC command or internal handler.

##### Direct Control Commands (Quick Reference)

Publish a plain text payload to these topics. This table is a quick reference for the most commonly used command topics; the authoritative list lives in `docs/api/MQTT.md`.

| Topic suffix | Payload | OTGW command | Description |
|-------------|---------|-------------|-------------|
| `setpoint` | `"21.5"` | `TT=21.50` | Temporary room temperature setpoint |
| `constant` | `"21.5"` | `TC=21.50` | Constant room temperature setpoint (persists over thermostat cycles) |
| `outside` | `"8.0"` | `OT=8.0` | Outside temperature override |
| `hotwater` | `"1"` / `"0"` / `"P"` | `HW=1` / `HW=0` / `HW=P` | Hot water on / off / one-time push |
| `gatewaymode` | `"1"` / `"0"` | `GW=1` / `GW=0` | Gateway mode / monitor mode |
| `maxchsetpt` | `"80"` | `SH=80` | Max CH water setpoint (C) |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max DHW setpoint (C) |
| `maxmodulation` | `"100"` | `MM=100` | Max relative modulation level (%) |
| `ctrlsetpt` | `"55.0"` | `CS=55.0` | Direct control setpoint (bypasses thermostat) |
| `chenable` | `"1"` / `"0"` | `CH=1` / `CH=0` | Central heating enable bit |
| `coolingenable` | `"1"` / `"0"` | `CE=1` / `CE=0` | Cooling enable bit |
| `summermode` | `"1"` / `"0"` | `SM=1` / `SM=0` | Summer mode (persisted to flash) |
| `command` | `"TT=21.50"` | (raw) | Raw OTGW command passthrough |

Note: Commands are only processed when an OT command interface (PIC or OTDirect) is detected. If no interface is available, MQTT commands are silently ignored with a debug log message.

##### SAT Command Topics

| Topic suffix | Payload | Description |
|-------------|---------|-------------|
| `sat/target` | `"21.0"` | Set target room temperature |
| `sat/enabled` | `"true"` / `"false"` | Enable / disable SAT |
| `sat/control_mode` | `"continuous"` / `"pwm"` / `"auto"` | Set control mode |
| `sat/preset` | `"comfort"` / `"eco"` / `"away"` / `"sleep"` | Apply comfort preset |
| `sat/indoor_temp` | `"20.8"` | Push external room temperature |
| `sat/outdoor_temp` | `"7.5"` | Push external outdoor temperature |
| `sat/humidity` | `"58"` | Push indoor humidity (%) |
| `sat/window` | `"open"` / `"closed"` / `"1"` / `"0"` | Set window open / closed state |
| `sat/valves_open` | `"true"` / `"false"` | Report TRV valves open state |
| `sat/flush` | (any) | Flush short-lived data (PID integral + cycle window) |
| `sat/reset_integral` | (any) | Reset PID integral accumulator |
| `sat/area/{0-3}` | `"20.5"` | Push area temperature (multi-area mode) |
| `sat/sun_elevation` | `"32.5"` | Push sun elevation from HA |
| `sat/heating_curve` | `"1.5"` | Update heating curve coefficient |
| `sat/deadband` | `"0.25"` | Update PID deadband |
| `sat/boiler_capacity` | `"24.0"` | Update boiler capacity (kW) |

Many more SAT settings can be updated via MQTT. The full list of 40+ setting commands is documented in `backlog/docs/doc-1 - sat-mqtt-topics.md`.

##### OTGW32 Command Topics

On OTGW32 builds only, the following command topics are available under `{TopTopic}/set/{UniqueId}/otgw32/`:

| Topic suffix | Payload | Description |
|-------------|---------|-------------|
| `otgw32/room_temp` | `"20.5"` | Set room temperature for OT-direct heating curve |
| `otgw32/room_setpoint` | `"21.0"` | Set room setpoint for OT-direct |

#### Home Assistant Auto-Discovery

When `mqtthadiscovery` is enabled, the firmware publishes MQTT discovery payloads to the `homeassistant/` prefix (configurable via `mqtthaprefix`). Home Assistant processes these payloads and creates entities automatically.

##### Discovery Drip Publisher

Discovery uses an async bitmap-driven drip publisher. Instead of sending all 200+ entity configurations in a burst, the firmware:

1. Marks all message IDs as "pending" in a bitmap.
2. From the main loop, publishes one discovery message per interval (typically 2 seconds).
3. When free heap drops below 8000 bytes, the interval slows down to reduce memory pressure.
4. When heap recovers, the interval returns to normal.

This approach prevents heap exhaustion and MQTT broker flooding that could occur with bulk discovery. A full discovery cycle takes approximately 6-8 minutes at normal pace.

Discovery is triggered:
- On firmware boot or MQTT settings change (all IDs marked pending for drip publish).
- When `POST /api/v2/otgw/discovery` is called (all IDs marked pending).
- When Home Assistant sends `"online"` to `homeassistant/status` after being offline (all IDs marked pending).

**Integration note:** After triggering a discovery republish, entities appear in Home Assistant gradually over several minutes as the drip publisher works through the pending bitmap. This is normal behavior, not an error.

Discovery payloads include device metadata, entity names, unit of measurement, device class, and state topics. The firmware creates approximately 200+ entities covering all sensors, binary sensors, and a climate entity for SAT.

Discovery templates are now compiled into PROGMEM (flash memory) at build time from the `mqttha.cfg` file, eliminating LittleFS I/O during discovery publishing.

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
- REST API access log (when `bRestAPI` debug flag is enabled): method, URI, HTTP status code, matched route segment, and total handler wall-clock time in milliseconds.

Example output:

```
[OTGW] handleOTGW: B4001FF04 msgid=0 (Status flags)
[OTGW]   flamestatus=ON chmodus=ON
[MQTT] sendMQTTData: OTGW/value/otgw-AABBCC/flamestatus = ON
[SAT]  Control loop: Tr=20.5 Tboiler=62.3 setpoint=43.1 error=0.50
[HEAP] Free: 19234 bytes (healthy)
REST GET /api/v2/health => 200 v2/health 12ms
REST POST /api/v2/otgw/commands => 202 v2/otgw 4ms
```

The trailing number is the time spent in the handler, measured from URI parse to response flush. Use it to spot slow endpoints or flash/LittleFS stalls without needing an external profiler.

The verbosity of each category is controlled by boolean flags in `state.debug`:

| Flag | Category | Default |
|------|---------|---------|
| `bOTmsg` | OpenTherm message trace | true |
| `bMQTT` | MQTT publish/receive trace | false |
| `bRestAPI` | REST API request trace | false |
| `bSensors` | Dallas sensor scan trace | false |
| `bSAT` | SAT control loop trace | true |
| `bOTDirect` | OTDirect frame handling | true |
| `bMQTTGate` | MQTT publish gating and value-change dedup | false |

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
