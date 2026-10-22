# C4 Code Level: REST API & File Server Module

## Overview

- **Name**: REST API & File Server Module
- **Description**: ESP32-S3 Arduino module providing the versioned REST API (v2) and LittleFS file serving on the shared `AsyncWebServer`. Handlers run on the AsyncTCP task. JSON is emitted by the streaming `JsonEmit` writer (no ArduinoJson, ADR-146); REST concurrency is capped at 2 in flight (ADR-165); the polled endpoints are rate limited with 429 (ADR-172).
- **Location**: `/src/OTGW-firmware/restAPI.ino`, `/src/OTGW-firmware/FSexplorer.ino`, `/src/OTGW-firmware/jsonStuff.ino`, `/src/OTGW-firmware/webServerCompat.h`, `/src/OTGW-firmware/jsonEmit.h`, `/src/OTGW-firmware/jsonChunked.h`
- **Language**: Arduino C++ (ESP32-S3; ESP8266 dropped, ADR-128)
- **Purpose**: Gives the web UI and external clients access to OpenTherm Gateway state, settings, sensors and SAT control; serves the v2 web UI assets with ETag validation; orchestrates PIC and firmware update helpers.

## Key Architecture

### API Design Pattern (ADR-050)

The REST API v2 uses a **central route dispatch table**:

1. **Single entry point**: `processAPI(AsyncWebServerRequest*)` in `restAPI.ino`, reached from the explicit `/api` route and from the `onNotFound` catch-all for `/api/*`
2. **URL parsing**: URI split into tokens (`API_MAX_WORDS` 8 words of `API_WORD_LEN` 32 chars; URI limit 50 chars, longer gets 414)
3. **Route matching**: `kV2Routes[]` (PROGMEM, `{segment, handler}` rows plus a `{nullptr, nullptr}` sentinel) maps `words[3]` to a handler
4. **Handler isolation**: every resource has one handler `void handleXxx(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI)`

Adding an endpoint takes one handler and one `kV2Routes[]` row.

Current `kV2Routes[]` segments: `health`, `settings`, `sensors`, `device`, `flash`, `pic`, `otdirect` (only `HAS_DIRECT_OT`), `firmware`, `filesystem`, `simulate`, `otgw`, `webhook`, `sat`, `discovery`, `debug`, `network`, `mqtt`.

### Concurrency Gates and Backpressure (ADR-165, ADR-147, ADR-188)

- **REST in-flight cap**: `REST_MAX_INFLIGHT` = 2 (`restInFlight`). `restEffectiveInflightCap()` tightens it to 1 when `platformMaxFreeBlock()` is below 16000 and keeps 2 below 24000. Excess requests get a cheap 503 with `Retry-After: 1` from `sendApiBusy()`. `REST_MAX_INFLIGHT` of 255 disables the gate (A/B arm).
- **File-serve cap**: `WEB_FILE_MAX_INFLIGHT` = 2, same heap clamp, `webFileGateTryAdmit()` / `webFileGateRelease()`; the REST and file slots are released through `webArmSlotRelease()` on request disconnect.
- **Connection cap** (ADR-188): `webConnCapExceeded()` refuses a request with 503 while more than `WEB_MAX_TX_CONNECTIONS` (4) port-80 connections hold send data (WebSocket clients are subtracted).
- Counters in `state.heapdiag`: `iRest503Count`, `iWebfile503Count`, `iWebConn503Count`, `iRestInflightHwm`, `iWebfileInflightHwm`.
- Frontend side: at most 2 calls in flight; the v2 bundle enforces this with a FIFO queue around `window.fetch` (ADR-184).

### Rate Limiting (ADR-172)

`checkApiRateLimit()` runs after route match and before the handler, for GET only. A GCRA budget (`ApiRateLimitBudget`, burst 2) applies per endpoint and globally, not per client:

| Budget | Routes | Window |
|--------|--------|--------|
| `RL_BUDGET_OTMONITOR` | `GET /api/v2/otgw/otmonitor` and its alias `/otgw/telegraf` (shared budget) | 1500 ms |
| `RL_BUDGET_DEVICE_TIME` | `GET /api/v2/device/time` | 4000 ms |

Over budget gives `429` with `application/problem+json` (RFC 9457; `retry_after` repeated in the body), `Retry-After`, `Cache-Control: no-store`, `RateLimit` and `RateLimit-Policy`. `evaluate.py` gates the coupling to the UI poll intervals (`check_poll_window_coupling`) and the alias coverage (`check_api_rate_limit_alias_coverage`). Separate 60 s cooldowns (429 JSON error) guard `POST /api/v2/discovery/republish` and `POST /api/v2/mqtt/republish` (CWE-770).

### Security Model (ADR-056)

- **HTTP Basic Auth**: user `admin`, password `settings.sHTTPpasswd`; empty password disables auth.
- **Central mutation gate**: `processAPI()` calls `checkHttpAuth()` for POST, PUT, PATCH and DELETE on `/api/v2/*`. `handleSettings()` and `handleSAT()` also authenticate GET.
- **CSRF**: `isSameOriginRequest()` compares the Origin (else Referer) host with the Host header; no Origin and no Referer is allowed (curl, Home Assistant). The check only engages when a password is set.
- **OPTIONS**: CORS preflight is answered upstream with 204 by `sendApiOptions()` before any handler; `checkHttpAuth()` does not short-circuit OPTIONS.
- **CORS**: `sendCorsOriginHeader()` echoes the request Origin instead of a wildcard.
- **Error format**: `{"error":{"status":N,"message":"..."}}` via `sendApiError()`; `sendApiMethodNotAllowed()` adds the `Allow` header; v0/v1 paths return 410.

### File Serving & Caching Strategy (ADR-139)

- **Stable URLs, ETag revalidation**: `serveVersionedAsset()` sends `ETag: "<fsHash>-<path>"` (filesystem hash plus served path) and `Cache-Control: no-cache`. A matching `If-None-Match` gets 304; a reflash or filesystem OTA changes the hash. Missing files return a no-cache 404 without validator. There is no `?v=` query versioning and no HTML rewriting.
- **Immutable fonts**: `serveImmutableAsset()` serves the three `.woff2` fonts with `max-age=31536000, immutable`.
- **Streaming**: `webSendFile()` serves from flash through `AsyncFileResponse`; files are never loaded into RAM.
- **Shell**: only the v2 UI ships (`v2.html`, `v2.js`, `v2-bundle.css`, ADR-192). `sendIndex()` falls back to `/FSexplorer.html` when `/v2.html` is missing.

## Code Elements

### Main API Handler

#### `void processAPI(AsyncWebServerRequest *request)`
- **Location**: `restAPI.ino`
- **Behavior** (in order):
  - `webBeginRequest(request)` binds the per-request context
  - `webConnCapExceeded()` then the in-flight cap, both answering 503 via `sendApiBusy()`
  - Rejects URIs of 50 chars or more (414) and `platformFreeHeap() < 4096` (500)
  - Tokenizes the URI with `strtok_r()` into static buffers
  - `/api/v2/*`: OPTIONS gets 204; mutating methods go through `checkHttpAuth()`; `kV2Routes[]` lookup; `checkApiRateLimit()`; handler call
  - `/api/v0` and `/api/v1` get 410; anything else gets `sendApiNotFound()`
  - One access-log line per request when `state.debug.bRestAPI` is set (telnet key `2`): `REST <METHOD> <URI> => <status> v2/<segment> <ms>ms`
- **Notes**: static buffers are safe because all handlers serialize on the AsyncTCP task

### Resource Handlers (Route Dispatch)

#### `handleHealth`
- `GET /api/v2/health` via `sendHealth()` (status, uptime, heap, network mode, RSSI, MQTT/OTGW/thermostat/boiler connectivity, `otcommandinterface`, `otgwmode`, `picfwtype`, `ntpenable`). Not authenticated.

#### `handleSettings`
- `GET` (`sendDeviceSettings()`) and `POST`/`PUT` (`postSettings()`); auth required for all methods.
- `postSettings()` takes one setting per call: body `{"name":"<setting>","value":<v>}`, parsed with `extractJsonField()`; unknown names get 400 (`isKnownSetting()`). Booleans must be unquoted JSON `true`/`false`. The write is debounced (2 s flush timer), not synchronous.
- Each setting is emitted as a typed object: `{"value":..,"type":"s|i|f|b",...,"maxlen"|"min"/"max"}`. `ssid` is read-only (type `r`).

#### `handleSensors`
- `GET /api/v2/sensors` or `/sensors/status` (Dallas and S0 readings), `GET|POST|PUT /sensors/labels` (`getDallasLabels()` / `updateAllDallasLabels()`).

#### `handleDevice`
- `GET /api/v2/device/info` (`sendDeviceInfoV2()`), `/device/time` (`sendDeviceTimeV2()`, rate limited), `/device/crashlog` (`sendDeviceCrashLog()`).
- `device/info` carries chip fields for the Debug screen (`psram_found`, `psram_size`, `psram_free`, `flash_size`, `internal_free`, `internal_maxblk`, `chip_model_est`), `status_pixel_gpio` (TASK-1221), `ssid` = `Wired` on Ethernet, and the OTA capability flags `app_ota_available` (`hasSpareAppOtaSlot()`) and `safeboot_available` (`hasSafeboot()`, ADR-191) that the `/update` page uses to choose between direct upload and `POST /safeboot`.

#### `handleFlash`, `handlePic`, `handleFirmware`, `handleFilesystem`
- `GET /api/v2/flash/status` (`sendFlashStatus()`).
- `GET /api/v2/pic/flash-status|update-check|settings`; answers 503 when `isPICEnabled()` is false.
- `GET /api/v2/firmware/files` (`apifirmwarefilelist()`).
- `GET /api/v2/filesystem/files` (`apilistfiles()`) and `/filesystem/hash-check` (`sendFilesystemHashCheck()`).

#### `handleOTDirect` (`HAS_DIRECT_OT` only; 503 unless `isOTDirectEnabled()`)
- `GET /api/v2/otdirect/status`
- `POST|PUT /otdirect/mode?mode=gateway|monitor|bypass|master|loopback` (queues `GW=1|0|P|S|L`)
- `GET|POST|PUT /otdirect/settings` (read returns the `otdirect_settings` object; write takes query args that map to `OTD*` settings)
- `GET /otdirect/overrides`; `POST|PUT /otdirect/overrides?action=sr|cr|rm|cm|ui|ki&msgid=0..127[&value=HHHH]` (queues `SR=`/`CR=`/`RM=`/`CM=`/`UI=`/`KI=`)

#### `handleSimulate`
- `GET /api/v2/simulate`, `POST|PUT /simulate/start`, `POST|PUT /simulate/stop`. Controls `state.debug.bOTGWSimulation` (frame replay of `/otgw_simulation.log` when that file is on LittleFS, else of the fixture built into the firmware; `source` in the JSON says which; a start re-picks the source, TASK-1294).

#### `handleOtgw`
- `GET /otgw/otmonitor` and `/otgw/telegraf` (aliases, `sendOTmonitorV2()`, rate limited)
- `GET /otgw/messages/{id}` and `/otgw/id/{id}` (`sendOTValue()`), `GET /otgw/label/{label}` (`sendOTLabel()`)
- `POST|PUT /otgw/commands` (body `{"command":"LL=value"}` or raw) and `/otgw/command/{cmd}` (legacy): `handleCommandSubmit()` validates `LL=value`, answers 202 `{"status":"queued"}`, 503 without an OT command interface
- `POST|PUT /otgw/diagnose`: raw keystrokes (field `data`, max 32 printable chars plus CR) straight to the PIC TX queue, only when the PIC runs diagnostic firmware (409 otherwise, 503 without PIC)
- `POST|PUT /otgw/discovery` and `/otgw/autoconfigure`: 202, then `doAutoConfigure()`
- `POST /otgw/reset_water_total`: `queueDHWWaterMeterReset()`, 200 `{"status":"ok","dhw_water_total":0}` (ADR-176)
- `GET /otgw/boiler-support` (unsupported read/write MsgID lists), `GET /otgw/ot-support` (bilateral support map), `GET /otgw/overrides` (active gateway overrides, ADR-118)

#### `handleWebhook`
- `POST|PUT /api/v2/webhook/test?state=on|off|1|0` (`testWebhook()`).

#### `handleSAT` (`checkHttpAuth()` first; all methods authenticated)
- `GET /sat` and `/sat/status` (`?detail=full` for health diagnostics, `satSendHealthJSON()`)
- `POST|PUT` setters: `/sat/target`, `/externaltemp`, `/externaloutdoor`, `/humidity`, `/pvsurplus` (TASK-640), `/window`, `/preset`, `/enable`, `/mode` (continuous/pwm), `/area/<0-3>`, `/flush`, `/reset_integral`, `/settings/<name>`
- `POST|PUT /sat/force-boiler` (test-only transient override); `POST /sat/sim/event` (bench scenario injection, 409 unless simulation is active)
- `GET|POST|DELETE /sat/markers[/<id>]` (calibration markers in `/sat_markers.json`, max 20)
- `GET|PATCH|POST|PUT /sat/sensor-areas` (area to DS18B20 mapping)
- `GET /sat/weather/needs-setup` (onboarding gate)
- BLE (`HAS_SAT_BLE`): `GET /sat/ble/discovery`; `POST /sat/ble/select|label|bindkey|forget|rescan`; `GET|PUT|POST|DELETE /sat/ble/roster` (8 slots, bindkeys write-only)

#### `handleDiscovery`
- `GET /api/v2/discovery` (verification status, counters, `auto_verify`)
- `POST /discovery/verify`: starts the 15 s verification window (503 without MQTT or on low heap, 409 when active or while the drip is pending; 202)
- `POST /discovery/republish`: `markAllMQTTConfigPending()`; 60 s cooldown (429)

#### `handleMqtt`
- `POST /api/v2/mqtt/republish`: `queueMQTTRepublishAll()` (re-emits observed OT values); 503 without MQTT; 60 s cooldown (429). Distinct from `discovery/republish`, which re-announces HA discovery configs.

#### `handleDebugDump`
- `GET /api/v2/debug`: every `state.debug.*` flag plus telemetry snapshot and local IP, sent through `restSendChunked()` from a frozen `DebugDumpSnap`.

#### `handleNetwork`
- `GET /api/v2/network/scan`: asynchronous. The first call starts `WiFi.scanNetworks(true)` and returns `{"status":"scanning"}`; later calls return `{"status":"ready","count":N,"networks":[{ssid,rssi,channel,secured,connected}]}`. 503 during PIC flash.

### JSON Response Layer

#### `JsonEmit` (`jsonEmit.h`, ADR-146)
- Streaming, non-throwing writer over an Arduino `Print` sink: `beginObject()`, `beginArray()`, `field(name, value)`, `endObject()`, `endArray()`, `fieldRaw()`. NaN and Inf serialize as `null`. No document tree, no `new`.
- Pattern: `AsyncResponseStream* strm = restBeginStream("application/json"); JsonEmit je(*strm); ... restFinalize();` (`webServerCompat.h`).

#### `restSendChunked(contentType, emitFn)` (`jsonChunked.h`, TASK-883)
- Pull-based `AsyncChunkedResponse`: the emit closure is re-run per TCP window into a windowing sink, so no whole-response buffer exists. The closure must be deterministic, so volatile values (heap, uptime, live telemetry) are frozen in a per-response snapshot (`DeviceInfoSnap`, `DebugDumpSnap`). Used by `device/info`, `debug`, `GET /settings` (the largest response, about 8.6 KB, reading only request-stable state so it needs no snapshot) and the SAT status in `SATcontrol.ino`.

#### `jsonStuff.ino`
- Escaping and parsing: `escapeJsonStringTo()`, `extractJsonField(json, key, result, size)` (flat-object field extraction), `readJsonStringPair()` / `writeJsonStringPair()` (streaming `dallas_labels.ini` I/O).
- Legacy chunked emitters that remain: `sendStartJsonMap()` / `sendEndJsonMap()`, `sendJsonMapEntry()` overloads, `sendJsonOTmonMapEntry()`, `sendJsonOTmonMapEntryDallasTemp()` and `sendJsonSettingObj()` overloads. `sendOTmonitorV2()` and the settings writer now use `JsonEmit`.

### Security & Validation Helpers

#### `bool checkHttpAuth()`
- Returns true when no password is set. Otherwise `authenticateCompat("admin", settings.sHTTPpasswd)` (401 via `webRequestAuth()`), then `isSameOriginRequest()` (403 "CSRF protection: invalid origin").

#### `static bool isSameOriginRequest()`
- Host part of Origin (else Referer) must equal the Host header; malformed origin is rejected; no header is allowed.

#### `sendApiError(int httpCode, const __FlashStringHelper* message)`, `sendApiMethodNotAllowed(allowed)`, `sendApiBusy(message)`, `sendApiRateLimited(...)`, `sendApiNotFound(URI)`
- Standard error responses; `sendApiBusy` adds `Retry-After: 1` for transient pressure (TASK-1124).

### File Serving (FSexplorer.ino)

#### `void startWebserver()`
- Registers `/`, `/index`, `/index.html` (`sendIndex`, behind `checkHttpAuth()`), `/FSexplorer.png` (`serveStatic`), `/fsexplorer.css`, `/v2.html`, `/v2.js`, `/v2-bundle.css`, the three font routes, `/pic` (`upgradepic`, `HAS_PIC`), and the `/api` route (`processAPI` with body capture via `webCaptureBody`). Calls `server.begin()`. A failed port-80 bind is retried every 5 s by `handleWebserverListener()` (TASK-1130).

#### `void setupFSexplorer()`
- Registers `/FSexplorer.html` and `/FSexplorer`, the deprecated unversioned `/api/firmwarefilelist` and `/api/listfiles`, `POST /upload`, `/ReBoot` and `/ResetWireless` (both `checkHttpAuth()`), the OTA handler (`httpUpdater.setup(&server)`, which also registers `POST /safeboot`), and the `onNotFound` catch-all (routes `/api/*` to `processAPI`, anything else to `handleFile()`, else 404).

#### `apifirmwarefilelist()` / `apilistfiles()`
- Streaming JSON listings (max `MAX_FILES_IN_LIST` = 40; hidden files skipped; `apilistfiles` adds a final storage entry and supports `?delete=<path>` behind auth). Both replicate the `platformFreeHeap() < 4096` guard because they bypass `processAPI()`.

#### `bool handleFile(String&& path)`
- Serves a LittleFS file with MIME detection via `contentType()`; `/settings.ini` is behind `checkHttpAuth()` because it holds the passwords in cleartext (ADR-056). Shows the helper form when `FSexplorer.html` is absent.

#### `void handleFileUpload(request, filename, index, data, len, final)`
- Async per-chunk upload to LittleFS. Auth and same-origin are decided at `index == 0`, before the file is opened; the `POST /upload` completion handler owns the response (303 redirect, 401/403, or 507 when nothing was stored). State lives on `request->_tempFile`, filenames are trimmed to 30 chars.

#### `contentType()`, `formatBytes()`, `freeSpace()`, `reBootESP()`, `resetWirelessButton()`, `doRedirect()`
- Helpers; `doRedirect(..., reboot=true)` defers the restart to the main loop so the response can drain.

## API Endpoints Reference

### v2 API Routes

| Method | Endpoint | Handler | Auth | Purpose |
|--------|----------|---------|------|---------|
| GET | `/api/v2/health` | handleHealth | No | System health |
| GET | `/api/v2/settings` | handleSettings | Yes | All settings with type metadata |
| POST/PUT | `/api/v2/settings` | handleSettings | Yes | Update one setting (`{"name","value"}`) |
| GET | `/api/v2/sensors`, `/sensors/status` | handleSensors | No | Dallas and S0 readings |
| GET/POST/PUT | `/api/v2/sensors/labels` | handleSensors | POST/PUT | Dallas labels |
| GET | `/api/v2/device/info` | handleDevice | No | Device metadata, OTA capability flags |
| GET | `/api/v2/device/time` | handleDevice | No | Clock state (rate limited) |
| GET | `/api/v2/device/crashlog` | handleDevice | No | Crash log |
| GET | `/api/v2/flash/status` | handleFlash | No | Flash info |
| GET | `/api/v2/pic/flash-status`, `/pic/update-check`, `/pic/settings` | handlePic | No | PIC info (503 without PIC) |
| GET | `/api/v2/otdirect/status`, `/settings`, `/overrides` | handleOTDirect | No | OT-direct state (`HAS_DIRECT_OT`) |
| POST/PUT | `/api/v2/otdirect/mode`, `/settings`, `/overrides` | handleOTDirect | Yes | OT-direct control |
| GET | `/api/v2/firmware/files` | handleFirmware | No | Firmware file inventory |
| GET | `/api/v2/filesystem/files`, `/filesystem/hash-check` | handleFilesystem | No | LittleFS listing, hash |
| GET | `/api/v2/simulate` | handleSimulate | No | Simulation status |
| POST/PUT | `/api/v2/simulate/start`, `/simulate/stop` | handleSimulate | Yes | Frame replay control |
| GET | `/api/v2/otgw/otmonitor`, `/otgw/telegraf` | handleOtgw | No | OT values (rate limited, shared budget) |
| GET | `/api/v2/otgw/messages/{id}`, `/otgw/id/{id}`, `/otgw/label/{label}` | handleOtgw | No | Single OT value |
| GET | `/api/v2/otgw/boiler-support`, `/otgw/ot-support`, `/otgw/overrides` | handleOtgw | No | Support maps, active overrides |
| POST/PUT | `/api/v2/otgw/commands`, `/otgw/command/{cmd}` | handleOtgw | Yes | Queue a command (202) |
| POST/PUT | `/api/v2/otgw/diagnose` | handleOtgw | Yes | PIC diagnostic menu keystrokes |
| POST/PUT | `/api/v2/otgw/discovery`, `/otgw/autoconfigure` | handleOtgw | Yes | Re-announce discovery |
| POST | `/api/v2/otgw/reset_water_total` | handleOtgw | Yes | Reset DHW water total |
| POST/PUT | `/api/v2/webhook/test?state=` | handleWebhook | Yes | Test webhook |
| GET | `/api/v2/sat`, `/sat/status` | handleSAT | Yes | SAT status (`?detail=full`) |
| POST/PUT | `/api/v2/sat/{target,externaltemp,externaloutdoor,humidity,pvsurplus,window,preset,enable,mode,flush,reset_integral,force-boiler}` | handleSAT | Yes | SAT inputs and control |
| POST/PUT | `/api/v2/sat/area/<0-3>`, `/sat/settings/<name>` | handleSAT | Yes | Area temperature, SAT setting |
| POST | `/api/v2/sat/sim/event` | handleSAT | Yes | Bench event injection |
| GET/POST/DELETE | `/api/v2/sat/markers[/<id>]` | handleSAT | Yes | Calibration markers |
| GET/PATCH/POST/PUT | `/api/v2/sat/sensor-areas` | handleSAT | Yes | Area sensor mapping |
| GET | `/api/v2/sat/weather/needs-setup` | handleSAT | Yes | Weather onboarding gate |
| GET/POST/PUT/DELETE | `/api/v2/sat/ble/{discovery,select,label,bindkey,forget,rescan,roster}` | handleSAT | Yes | BLE sensors (`HAS_SAT_BLE`) |
| GET | `/api/v2/discovery` | handleDiscovery | No | Discovery verification status |
| POST | `/api/v2/discovery/verify` | handleDiscovery | Yes | Start verification window |
| POST | `/api/v2/discovery/republish` | handleDiscovery | Yes | Re-announce all discovery configs (60 s cooldown) |
| POST | `/api/v2/mqtt/republish` | handleMqtt | Yes | Re-publish OT values (60 s cooldown) |
| GET | `/api/v2/debug` | handleDebugDump | No | Debug flags and telemetry |
| GET | `/api/v2/network/scan` | handleNetwork | No | Async WiFi scan |
| OPTIONS | `/api/v2/*` | sendApiOptions | N/A | CORS preflight (204) |

### File Serving and Admin Routes

| Method | Path | Handler | Purpose |
|--------|------|---------|---------|
| GET | `/`, `/index`, `/index.html` | sendIndex | v2 UI shell (auth, ETag); falls back to FSexplorer |
| GET | `/v2.html`, `/v2.js`, `/v2-bundle.css`, `/fsexplorer.css` | serveVersionedAsset | ETag + `no-cache` assets |
| GET | `/fonts/*.woff2` (inter-400, inter-700, jetbrains-mono-400) | serveImmutableAsset | One-year immutable cache |
| GET | `/FSexplorer.png` | serveStatic | Icon |
| GET | `/FSexplorer.html`, `/FSexplorer` | sendFSexplorerHtml | Recovery file explorer |
| POST | `/upload` | handleFileUpload | Upload to LittleFS |
| ANY | `/ReBoot`, `/ResetWireless` | reBootESP, resetWirelessButton | Reboot, reset WiFi (auth) |
| ANY | `/pic` | upgradepic | PIC firmware flash (`HAS_PIC`) |
| POST | `/update`, `/safeboot` | httpUpdater | Filesystem OTA, safeboot handover (see network component) |
| GET | `/api/firmwarefilelist`, `/api/listfiles` | FSexplorer | Deprecated unversioned listings (ADR-035) |
| GET | any other path | onNotFound / handleFile | LittleFS file or 404 |

## Dependencies

### Internal Dependencies

- `OTGW-firmware.h` (settings/state), `OTGW-Core.ino` (`addCommandToQueue()`, OT state), `settingStuff.ino` (`updateSetting()`), `SATcontrol.ino` (`satHandle*()`, `satSendStatusJSON()`), `MQTTstuff.ino` (`markAllMQTTConfigPending()`, `queueMQTTRepublishAll()`, discovery verification), `sensors_ext.ino` (Dallas labels), `webServerCompat.h` (request bridge), `OTGW-ModUpdateServer-esp32.h` (`hasSpareAppOtaSlot()`, `hasSafeboot()`)

### External Dependencies

- **ESPAsyncWebServer** 3.11.2 / **AsyncTCP** 3.4.10 (request, `AsyncResponseStream`, `AsyncFileResponse`, `AsyncChunkedResponse`), **LittleFS**, **WiFi** (scan)
- No ArduinoJson (ADR-146)

## Key Behaviors

- **Async request bridge** (`webServerCompat.h`): `webBeginRequest()`, `webPushHeader()` (max `WEB_MAX_PENDING_HEADERS` = 6, silently dropped past the cap), `webSend()`, `webSendFile()`, `argCompat()`, `bodyCompat()` (request body capped at `WEB_BODY_MAX_LEN` 2560, captured by `webCaptureBody()` on both the `/api` handler and the catch-all).
- **Command queuing**: `LL=value` validated (two letters, `=`), length-limited, queued with `addCommandToQueue()`, 202 `{"status":"queued"}`.
- **Access log**: one line per request behind the REST debug flag; low-level rejections log separately.

## Notes

- Strings in PROGMEM (`F()`, `PSTR()`), `char[]` buffers with `strlcpy`/`snprintf_P` in hot paths (ADR-180).
- Admin routes registered with `HTTP_ANY` must reach `checkHttpAuth()` themselves; there is no blanket gate for them.
- `handleOTDirect` and BLE sub-routes compile out on boards without `HAS_DIRECT_OT` / `HAS_SAT_BLE`.

## References

- **ADR-035**: API versioning policy; **ADR-050**: route dispatch table; **ADR-056**: admin auth and CSRF (supersedes ADR-054); **ADR-118**: gateway override surface; **ADR-139**: ETag asset caching
- **ADR-146**: `JsonEmit`, no ArduinoJson; **ADR-147 / ADR-165 / ADR-184 / ADR-188**: concurrency and connection limits; **ADR-172**: poll rate limit; **ADR-176**: DHW water total reset; **ADR-180**: no `String` in hot paths; **ADR-191**: safeboot OTA
- Related code: `OTGW-firmware.h`, `OTGW-Core.ino`, `SATcontrol.ino`, `MQTTstuff.ino`
