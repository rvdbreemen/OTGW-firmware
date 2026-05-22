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

`health`, `settings`, `sensors`, `device`, `flash`, `pic`, `otdirect` (OTGW32 only), `firmware`, `filesystem`, `simulate`, `otgw`, `webhook`, `sat`, `discovery`, `debug`, `network`.

Adding a new endpoint is a two-line change: register a handler and add one entry to `kV2Routes[]`. This is why the API is stable across builds: the dispatch surface is small, explicit, and reviewed.

Pre-flight `OPTIONS` requests on any `/api/v2/...` path are handled centrally and return HTTP 204 with the CORS headers required by browser clients.

#### Transport

- HTTP only, no HTTPS (ADR-003). The device operates on a trusted local network. For remote access, place it behind a reverse proxy or VPN.
- Content-Type for all responses: `application/json`
- All JSON responses are UTF-8

#### Authentication

All POST and PUT requests require HTTP Basic Authentication when an HTTP password is configured in Settings. The username is the device hostname (default `OTGW`). Authentication is checked centrally before any mutation handler runs (ADR-056).

GET endpoints for sensitive data (`/api/v2/settings`, `/api/v2/sat/*`, `/api/v2/debug`) also require authentication. Unauthenticated GET endpoints (health, sensor data, OT values, device info) are intentionally public.

CSRF same-origin validation is enforced for authenticated browser requests: the `Origin`/`Referer` header must match the `Host` header (ADR-054). The same-origin check is paired with HTTP Basic Auth; both must succeed for the mutation to be executed.

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
| 409 | Conflict: action refused due to state (verification already active, marker quota reached) |
| 410 | Gone: deprecated API version (v0, v1) |
| 413 | Request Entity Too Large: command string too long |
| 414 | URI Too Long: request URI exceeds the 50-byte parser buffer |
| 429 | Too Many Requests: cooldown active (e.g. `/api/v2/discovery/republish`) |
| 500 | Internal Server Error: low heap (< 4 KB), filesystem write failure |
| 503 | Service Unavailable: MQTT not connected, no OT command interface, PIC flash in progress |

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

The `otcommandinterface` field is a string identifying the active OT interface: `"PIC"`, `"OT-Direct"`, or `"None"` (compatibility note: earlier releases returned a boolean).

On Ethernet-capable hardware (OTGW32), additional fields surface the wired-link state: `ethernetpresent` (PHY detected), `ethernetlink` (cable plugged in and link up). When the active network mode is `ethernet`, the `ssid` field returns the literal string `"Wired"` and `wifirssi` is reported as `0`. The runtime can fail over between WiFi and Ethernet without a reboot (TASK-581); `networkmode` reflects the currently active transport.

When the device is running in AP-fallback mode (a prerelease-only safety net for unreachable WiFi), `apfallback: true` is included and `ssid` carries the AP SSID.

Additional fields surface MQTT discovery telemetry (`disc_published_topics`, `disc_pending_ids`, `disc_verify_runs`, `disc_republish_triggered`, `disc_last_missing`, `disc_last_orphan`, `disc_last_outcome`) and REST handler timing (`perf_device_info_total_ms`, `perf_settings_total_ms`, `perf_sat_status_total_ms`, …) for ADR-062 and TASK-361 observability.

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

**Parameters:** `{name}` is the setting name. The handler accepts the full SAT setting vocabulary, including:

`heating_curve`, `boiler_capacity`, `deadband`, `max_modulation`, `dhw_setpoint`, `dhw_enabled`, `dhw_enable`, `interval`, `ovp_value`, `ovp_enabled`, `ovp_start`, `ovp_stop`, `push_setpoint`, `flame_off_offset`, `force_pwm`, `flow_offset`, `summer_simmer`, `summer_threshold`, `summer_min_hours`, `comfort_adjust`, `comfort_humidity`, `comfort_max_offset`, `simulation`, `ble_enable`, `ble_mac`, `ble_interval`, `preset_sync`, `preset_sync_topic`, `multi_area`, `multi_area_count`, `auto_tune`, `auto_tune_rate`, `mod_sup_delay`, `mod_sup_offset`, `target_temp_step`, `min_pressure`, `max_pressure`, `max_pressure_drop`, `preset_comfort`, `preset_eco`, `preset_away`, `preset_sleep`, `preset_activity`, `preset_home`, `solar_gain`, `solar_freeze_integral`, `window_detection`, `pwm_auto_switch`, `sensor_max_age`, `error_monitoring`, `auto_gains_value`, `cycles_per_hour`, `valve_offset`, `heating_mode`, `heating_system`, `manufacturer`, `control_mode`, `preset`, `reset_integral`.

**Request body:** New value as plain string or JSON `{"value": "..."}`. The `ovp_start`, `ovp_stop`, and `reset_integral` setting names take no value (the action is implicit).

**Response (HTTP 200):**

```json
{"status": "ok", "setting": "heating_curve", "value": "1.8"}
```

Unknown setting names return HTTP 404. This endpoint mirrors the full MQTT `sat/<setting>` command vocabulary, providing parity between REST and MQTT configuration.

##### GET / POST / DELETE /api/v2/sat/markers

Manage user-placed heating-curve calibration markers (TASK-586). Markers are persisted in `/sat_markers.json` on LittleFS. The web UI renders them as anchor points on the heating-curve graph.

- **GET** returns the marker array (or `[]` when no markers exist).
- **POST** appends a marker. Body: `{"outside_temp": 5.0, "flow_temp": 55.0, "label": "optional text"}`. `outside_temp` must lie in -15..25, `flow_temp` in 10..90. Returns HTTP 201 with `{"id": N}`. Maximum 20 markers; further `POST` returns HTTP 409.
- **DELETE** `/api/v2/sat/markers/{id}` removes the marker with the given integer id.

##### GET / PATCH /api/v2/sat/sensor-areas

Manage the DS18B20-to-SAT-area mapping for multi-area mode (TASK-587). Each of four areas (`0..3`) can be bound to a Dallas sensor by its 16-character ROM address.

- **GET** returns the current mapping:
  ```json
  {"areas":[{"index":0,"sensor":"28FF64D1841703F1"},{"index":1,"sensor":""},{"index":2,"sensor":""},{"index":3,"sensor":""}]}
  ```
- **PATCH** (or POST / PUT) updates one mapping. Body: `{"area": 0, "sensor": "28FF64D1841703F1"}`. An empty `sensor` string clears the slot. The 16-character ROM address must be hex; non-hex returns HTTP 400.

##### GET /api/v2/sat/weather

Returns the current weather data used by the SAT heating curve (Open-Meteo source).

##### GET /api/v2/sat/weather/needs-setup

Reports whether the SAT weather onboarding wizard should be shown. Returns `{"needs_setup": true|false, "has_key": true|false}`. The flag becomes `true` after the 5-minute startup grace window if neither OT MsgID 27 (outside temperature) nor a valid weather record is available (TASK-511).

##### SAT BLE roster (ESP32 only)

The BLE roster surfaces nearby BLE temperature sensors discovered by the ESP32-S3 radio and lets the user promote one to the active room-temperature source (TASK-508).

| Method | Path | Description |
|---|---|---|
| GET | `/api/v2/sat/ble/discovery` | Stream the current roster as JSON (MAC, RSSI, label, last seen). |
| POST | `/api/v2/sat/ble/select` | Body `{"mac": "AA:BB:CC:DD:EE:FF"}` — promote the roster entry to the active SAT BLE sensor. |
| POST | `/api/v2/sat/ble/label` | Body `{"mac": "AA:BB:CC:DD:EE:FF", "label": "Living room"}` — set the persistent label for a roster slot. |
| POST or DELETE | `/api/v2/sat/ble/forget` | Body `{"mac": "AA:BB:CC:DD:EE:FF"}` — drop the slot and clean up its HA discovery entries. |

All four endpoints return HTTP 404 when the supplied MAC is not in the roster. These endpoints are not present on ESP8266 builds (route returns 404).

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

| Mode | PIC code | Description |
|------|----------|-------------|
| `gateway` | `GW=1` | Full gateway: scheduler + thermostat forwarding + overrides (default) |
| `monitor` | `GW=0` | Transparent pass-through: all frames forwarded unmodified (PIC parity, TASK-438) |
| `bypass` | `GW=P` | Thermostat wired directly to boiler via relay; OTDirect inactive |
| `master` | `GW=S` | Standalone OT master: scheduler only, no thermostat expected |
| `loopback` | `GW=L` | Internal test: simulated boiler responses, no hardware needed |

Note (TASK-438): `monitor` is now mapped to `GW=0` for parity with PIC firmware conventions. The pre-2.0 mapping that used `GW=0` for bypass has been moved to the `GW=P` alias.

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

#### MQTT Discovery Verification (ADR-062)

These endpoints expose the bitmap-driven discovery publisher and its verification window. They are the runtime contract used by the web UI Maintenance page and by Home Assistant integration scripts.

##### GET /api/v2/discovery

Returns the current discovery state and counters.

```json
{
  "verification": {
    "active": false,
    "last_epoch": 1774548600,
    "last_missing": 0,
    "last_orphan": 0,
    "last_outcome": "clean"
  },
  "counters": {
    "published_topics": 217,
    "pending_ids": 0,
    "verify_runs": 4,
    "republish_triggered": 1
  },
  "settings": {
    "auto_verify": true
  }
}
```

`last_outcome` is one of `clean`, `missing`, `aborted_heap`, `aborted_disconnect`, `unknown`.

##### POST /api/v2/discovery/verify

Starts a verification window. The firmware subscribes to its own discovery prefix, counts retained config topics, and reports `missing` and `orphan` totals. Authentication required.

Refused with HTTP 503 when MQTT is disconnected or free heap is below the verification threshold. Refused with HTTP 409 when a verification window is already active or when the drip publisher still has pending IDs. Returns HTTP 202 on success with `{"status":"verification_started","expected":N,"window_ms":15000}`.

##### POST /api/v2/discovery/republish

Marks every discovery topic pending for the drip publisher. Authentication required.

Subject to a 60-second cooldown to prevent the drip queue being held permanently non-empty (TASK-356). Successive invocations within the cooldown return HTTP 429 with the remaining seconds. Returns HTTP 200 with `{"status":"marked_pending","count":N}` on success.

#### Debug Dump

##### GET /api/v2/debug

Returns a single flat JSON object summarising build identity, runtime state, settings, MQTT state, OT bus state, heap and discovery counters, SAT runtime, and OTDirect runtime where applicable. Authentication required.

This is the primary diagnostic snapshot used when filing issues; the field set is intentionally broad and not part of the public contract — fields may be added in any release. Sensitive values (`http_passwd`, `mqtt.passwd`, `sat.weather_key`) are masked to `"***"` when set.

#### Network

##### GET /api/v2/network/scan

Initiates and polls an asynchronous WiFi scan (TASK-585). Used by the Settings page WiFi-picker.

- First call: starts the scan and returns `{"status":"scanning"}`.
- Subsequent calls while the scan is in progress: returns `{"status":"scanning"}`.
- After the scan completes: returns `{"status":"ready","count":N,"networks":[...]}` once. Each `networks` entry is `{"ssid":"...", "rssi":-60, "channel":6, "secured":true, "connected":true}`.

Refused with HTTP 503 while a PIC flash operation is in progress (the radio cannot share CPU). The scan buffer is released after the result is served.

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

The SPA hardens the reconnect against re-entry of `initOTLogWebSocket()` so a fast page reload cannot leak duplicate WebSocket clients on the firmware (TASK-563).

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

When `mqttseparatesources` is enabled, OpenTherm values are additionally published with a sibling-suffix shape (ADR-097):

```
{TopTopic}/value/{UniqueId}/{label}_thermostat   <-- thermostat-side value (T-prefix frame)
{TopTopic}/value/{UniqueId}/{label}_boiler       <-- boiler-side value (B-prefix frame)
{TopTopic}/value/{UniqueId}/{label}_gateway      <-- gateway-injected value (R-prefix frame)
{TopTopic}/value/{UniqueId}/{label}_answer       <-- proxy/answer (A-prefix frame)
```

Sibling-suffix names (`<label>_<source>`) replace the older subtopic shape so HA-discovery `state_topic` regex stays anchored to a single segment. Proxy answers (A-prefix) are routed to `_answer`, not collapsed onto `_thermostat`, per ADR-103.

This allows Home Assistant automations to distinguish between what the thermostat requested, what the boiler confirmed, and what the gateway injected.

##### Self-Describing Topic Names (ADR-106, default in 2.0.0)

Released 2.0.0 firmware publishes OpenTherm values under self-describing topic names: each label conveys its OT semantic role and unit in the topic itself (for example, `tboiler_c`, `controlsetpoint_c`, `chmodus`, `flame_status`). The legacy short names from the 1.4.x line (`boilertemperature`, `controlsetpoint`, …) remain available via the toggle `settings.mqtt.bUseLegacyOtTopics = true` for users who cannot migrate dashboards or automations. This is a breaking change for fresh installs; the toggle is the supported downgrade path.

##### HA-Core Alias Topics (ADR-105, opt-in)

When `settings.mqtt.bHaCoreAliasEnable` is `true`, 37 additional topic aliases are published under names that match the Home Assistant Core `opentherm_gw` integration. This gives users migrating from the HA-Core integration drop-in compatibility for existing automations. The aliases are pure mirrors; firmware-side semantics remain unchanged.

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
| `outside_temp` | `"8.0"` | `OT=8.0` | Outside temperature override (alias of `outside`) |
| `setback` | `"15.0"` | `SB=15.0` | Setback temperature override |
| `ventsetpt` | `"50"` | `VS=50` | Ventilation setpoint |
| `temperaturesensor` | `"O"` / `"R"` | `TS=O` / `TS=R` | Temperature sensor function (outside / return) |
| `addalternative` | `"34"` | `AA=34` | Add OT MsgID to alternative-request rotation |
| `delalternative` | `"34"` | `DA=34` | Remove OT MsgID from alternative-request rotation |
| `unknownid` / `knownid` | `"34"` | `UI=34` / `KI=34` | Mark MsgID as unknown / known |
| `priomsg` | `"5"` | `PM=5` | Set priority message |
| `setresponse` | `"34:0000"` | `SR=34:0000` | Set stored response |
| `clearrespons` | `"34"` | `CR=34` | Clear stored response |
| `resetcounter` | `"H"` | `RS=H` | Reset PIC counter (`H`, `T`, `B`, `M`, `S`) |
| `gpioa` / `gpiob` | `"0".."7"` | `GA=…` / `GB=…` | Set GPIO A / B function |
| `leda`..`ledf` | letter | `LA=…`..`LF=…` | Set LED A..F function (B/C/E/F/H/M/O/P/R/T/W/X) |
| `resetgateway` | (ignored) | hardware reset | Hardware-reset the PIC (payload ignored) |
| `command` | `"TT=21.50"` | (raw) | Raw OTGW command passthrough |

Note: Commands are only processed when an OT command interface (PIC or OTDirect) is detected. If no interface is available, MQTT commands are surfaced in the default debug stream and discarded (no longer silently dropped, per change set commit 5571d9b7).

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

1. Marks message IDs as "pending" in a bitmap.
2. From the main loop, publishes one discovery message per interval (typically 2 seconds).
3. When free heap drops below 8000 bytes, the interval slows down to reduce memory pressure.
4. When heap recovers, the interval returns to normal.

This approach prevents heap exhaustion and MQTT broker flooding that could occur with bulk discovery.

##### Just-in-time discovery (TASK-578)

The 2.0.0 firmware publishes OT-value discovery configs only when the corresponding MsgID has been observed at least once on the bus. The drip publisher walks all known IDs, but only adds an ID to the pending bitmap after `msglastupdated[id]` has been set. This eliminates the previous behaviour of advertising entities for MsgIDs that the boiler never exchanges, keeping the HA entity list aligned with what the device actually reports.

Discovery is triggered:
- On firmware boot or MQTT settings change (all eligible IDs marked pending).
- When `POST /api/v2/otgw/discovery` or `POST /api/v2/discovery/republish` is called (all eligible IDs marked pending).
- When Home Assistant sends `"online"` to `homeassistant/status` after being offline (all eligible IDs marked pending), provided the broker-restart heuristic (ADR-100) has not also signalled a session loss.
- When a new MsgID is observed for the first time after boot (single-ID JIT publish).

**Integration note:** After triggering a discovery republish, entities appear in Home Assistant gradually over several minutes as the drip publisher works through the pending bitmap. This is normal behaviour, not an error.

Discovery payloads include device metadata, entity names, unit of measurement, device class, state topics, and an availability topic. The `availability` reference is bound to the MQTT-client link (ADR-102), not to OT-bus liveness; HA marks entities `unavailable` only when the firmware is genuinely off-broker.

Discovery templates are compiled into PROGMEM (flash memory) at build time from the `mqttha.cfg` file, eliminating LittleFS I/O during discovery publishing.

Pending-state fan-out for retained sensors (cycle class, flame status, OEM fault code) is no longer throttled by the 250 ms rate-gate (ADR-104); first-seen values reach HA on the first publish loop tick.

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
