# C4 Code Level: REST API & File Server Module

## Overview

- **Name**: REST API & File Server Module
- **Description**: ESP8266 Arduino firmware module providing versioned REST API (v2) and file serving from LittleFS with comprehensive JSON response handling, ETag-based caching, CSRF protection, and multi-domain API resource handlers.
- **Location**: `/src/OTGW-firmware/restAPI.ino` (2103 lines), `/src/OTGW-firmware/FSexplorer.ino` (661 lines), `/src/OTGW-firmware/jsonStuff.ino` (793 lines)
- **Language**: Arduino C++ (ESP8266)
- **Purpose**: Provides the REST API surface for the OTGW firmware, enabling web UI and external clients to query/control OpenTherm Gateway state, settings, sensors, and SAT (Smart Ambient Temperature) control; handles file serving with browser caching optimization and firmware/PIC update orchestration.

## Key Architecture

### API Design Pattern (ADR-050)

The REST API v2 uses a **centralized route dispatch table** pattern:

1. **Single entry point**: `processAPI()` at line 1100
2. **URL parsing**: URI split into tokens (max 8 words, 32 chars each)
3. **Route matching**: Dispatch table `kV2Routes[]` at line 1080 maps resource names to handler functions
4. **Handler isolation**: Each resource (health, settings, sensors, device, otgw, sat, etc.) has its own handler

This design allows adding new endpoints by simply:
1. Adding a handler function
2. Adding one entry to `kV2Routes[]`

No changes needed to the router itself.

### Security Model

- **CORS preflight**: OPTIONS method always allowed (RFC 7231)
- **HTTP Basic Auth** (ADR-054): All POST/PUT mutations require auth
- **CSRF protection** (ADR-054): Same-origin validation via Origin/Referer headers
- **Rate limiting**: Via global `checkHttpAuth()` — only one auth check per request
- **Error format**: Consistent JSON: `{"error":{"status":N,"message":"..."}}`

### File Serving & Caching Strategy

1. **ETag-based conditional GET** (HTTP 304 Not Modified)
   - `index.html` uses filesystem hash as ETag
   - Browser caches and revalidates via `If-None-Match` header
   - Unchanged FS → 304 (no re-download)
   - Updated FS → ETag changes → 200 (fresh content)

2. **Versioned asset URLs** (cache-busting)
   - JS assets: `index.js?v=<fsHash>`, `graph.js?v=<fsHash>`
   - Versioned requests get long-term cache (1 day)
   - Bare requests get no-cache header

3. **Chunked streaming** (no full buffering)
   - Large files streamed via `httpServer.streamFile()`
   - Line-by-line injection of `?v=<hash>` into HTML
   - Avoids heap fragmentation

## Code Elements

### Main API Handler

#### `void processAPI()`
- **Location**: `restAPI.ino:1100–1176`
- **Purpose**: Central dispatcher for all `/api/v2/*` requests with handler timing and access logging
- **Signature**: `void processAPI()`
- **Behavior**:
  - Records `startMs = millis()` at entry for timing measurement
  - Parses URI into tokens via `strtok_r()`
  - Validates heap (>4KB) and URI length (<50 chars)
  - Routes to resource handlers via `kV2Routes[]` dispatch table
  - Forwards deprecated v0/v1 with 410 Gone error
  - Sends 404 for unknown routes
  - Logs one-line access log to telnet debug: method, URI, response status code, handler elapsed time via `RESTDebugTf()` (v1.3.5+, commit 583dd59c). Example: `REST GET /api/v2/sensors/status => 200 v2/sensors 42ms`
- **Dependencies**: `httpServer`, `kV2Routes[]`, per-resource handlers
- **Notes**: Uses static buffers (URI[50], words[8][32], originalURI) totaling ~356 bytes to save stack; cooperative scheduler — not re-entrant with yield()

### Resource Handlers (Route Dispatch)

Each handler function signature: `void handleXXX(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method, const char* originalURI)`

#### `void handleHealth()`
- **Location**: `restAPI.ino:216–219`
- **Purpose**: GET-only endpoint for system health metrics
- **HTTP Method**: GET only
- **Route**: `/api/v2/health`
- **Response**: JSON health object via `sendHealth()`
- **Auth**: Not required

#### `void handleSettings()`
- **Location**: `restAPI.ino:242–252`
- **Purpose**: Read device settings (GET) or update settings (POST/PUT)
- **HTTP Methods**: GET, POST, PUT
- **Routes**:
  - `GET /api/v2/settings` → `sendDeviceSettings()` (sensitive data, auth required)
  - `POST/PUT /api/v2/settings` → `postSettings()` (auth required)
- **Auth**: Required for all methods
- **Notable read-only field**: `ssid` (type `"r"`) — returns the connected WiFi SSID via `WiFi.SSID()`. Not writable; exposed so the Settings page can display the current network without additional API calls. When on Ethernet, the device info endpoint returns `"ssid": "Wired"` via `sendDeviceInfoV2()`. Line 1730 shows SSID metadata: `sendJsonSettingObj(F("ssid"), ssidBuf, "r", 32)` where `"r"` marks it read-only.
- **Nightly restart settings** (v1.3.5+, commit 06c62770): Endpoints expose `nightlyrestart` (bool) and `nightlyrestarthour` (int 0-23) for scheduling automatic device restart; frontend Settings page now provides Reset WiFi button alongside settings; lines 1754-1755 in sendDeviceSettings

#### `void handleSensors()`
- **Location**: `restAPI.ino:254–317` (implemented via `sendSensorStatus()` starting at line 254)
- **Purpose**: Read current sensor readings and manage Dallas temperature sensor labels
- **HTTP Methods**: GET, POST, PUT
- **Routes**:
  - `GET /api/v2/sensors` or `GET /api/v2/sensors/status` → `sendSensorStatus()` at line 254 (current Dallas + S0 readings; new endpoint added commit 583dd59c)
  - `GET /api/v2/sensors/labels` → `getDallasLabels()`
  - `POST/PUT /api/v2/sensors/labels` → `updateAllDallasLabels()`
- **Auth**: Not specified (defaults to no auth for GET)

#### `void handleDevice()`
- **Location**: `restAPI.ino:247–259`
- **Purpose**: Device metadata endpoints
- **HTTP Method**: GET only
- **Routes**:
  - `GET /api/v2/device/info` → `sendDeviceInfoV2()`
  - `GET /api/v2/device/time` → `sendDeviceTimeV2()`
  - `GET /api/v2/device/crashlog` → `sendDeviceCrashLog()`
- **Auth**: Not required

#### `void handleFlash()`
- **Location**: `restAPI.ino:261–268`
- **Purpose**: ESP8266 flash memory status
- **HTTP Method**: GET only
- **Route**: `GET /api/v2/flash/status` → `sendFlashStatus()`
- **Auth**: Not required

#### `void handlePic()`
- **Location**: `restAPI.ino:270–282`
- **Purpose**: PIC (Programmable Interface Controller) firmware metadata
- **HTTP Method**: GET only
- **Routes**:
  - `GET /api/v2/pic/flash-status`
  - `GET /api/v2/pic/update-check`
  - `GET /api/v2/pic/settings`
- **Precondition**: PIC must be detected (`isPICEnabled()`)
- **Auth**: Not required

#### `void handleOTDirect()` (ESP32 / OTGW32 only)
- **Location**: `restAPI.ino:285–410` (guarded by `#if HAS_DIRECT_OT`)
- **Purpose**: Direct OpenTherm control (OTGW32 hardware feature)
- **HTTP Methods**: GET, POST, PUT
- **Routes**:
  - `GET /api/v2/otdirect/status`
  - `POST /api/v2/otdirect/mode?mode=gateway|monitor|bypass|master|loopback`
  - `GET /api/v2/otdirect/settings`
  - `POST /api/v2/otdirect/settings?key=value...`
  - `GET /api/v2/otdirect/overrides` — list all active overrides
  - `POST /api/v2/otdirect/overrides?action=sr&msgid=X&value=HHHH` (set stored response)
  - `POST /api/v2/otdirect/overrides?action=cr&msgid=X` (clear stored response)
  - `POST /api/v2/otdirect/overrides?action=rm&msgid=X&value=HHHH` (set response modifier)
  - `POST /api/v2/otdirect/overrides?action=cm&msgid=X` (clear response modifier)
  - `POST /api/v2/otdirect/overrides?action=ui|ki&msgid=X` (mark unknown/known ID)
- **Auth**: Not explicitly required, but POST/PUT get auth via central check
- **Precondition**: OTGW32 hardware must be present
- **Notes**: Settings mapping includes room compensation, heating curve, PID parameters

#### `void handleFirmware()`
- **Location**: `restAPI.ino:413–420`
- **Purpose**: Firmware file inventory for OTA updates
- **HTTP Method**: GET only
- **Route**: `GET /api/v2/firmware/files` → `apifirmwarefilelist()`
- **Auth**: Not required

#### `void handleFilesystem()`
- **Location**: `restAPI.ino:422–431`
- **Purpose**: LittleFS file listing and hash validation
- **HTTP Method**: GET only
- **Routes**:
  - `GET /api/v2/filesystem/files`
  - `GET /api/v2/filesystem/hash-check`
- **Auth**: Not required

#### `void handleSimulate()`
- **Location**: `restAPI.ino:433–458`
- **Purpose**: OTGW message simulation control (debug feature)
- **HTTP Methods**: GET, POST, PUT
- **Routes**:
  - `GET /api/v2/simulate` → `sendSimulationStatus()`
  - `POST /api/v2/simulate/start`
  - `POST /api/v2/simulate/stop`
- **Auth**: Not required
- **State**: Controls `state.debug.bOTGWSimulation`

#### `void handleOtgw()`
- **Location**: `restAPI.ino:460–505`
- **Purpose**: OpenTherm Gateway state and command submission
- **HTTP Methods**: GET, POST, PUT
- **Routes**:
  - `GET /api/v2/otgw/otmonitor` or `/api/v2/otgw/telegraf` → `sendOTmonitorV2()` (Telegraf-compatible output)
  - `GET /api/v2/otgw/messages/{id}` → `sendOTValue(id)` (get by message ID)
  - `GET /api/v2/otgw/id/{id}` (alias for messages)
  - `POST /api/v2/otgw/commands` — command in body (JSON or raw)
  - `POST /api/v2/otgw/command/{cmd}` (legacy, prefer /commands)
  - `POST /api/v2/otgw/discovery` or `/api/v2/otgw/autoconfigure` → triggers MQTT auto-configure
  - `GET /api/v2/otgw/label/{label}` → `sendOTLabel(label)`
- **Auth**: POST/PUT require auth (via central check)
- **Notes**: Commands queued via `addCommandToQueue()`, validated for format

#### `void handleWebhook()`
- **Location**: `restAPI.ino:507–527`
- **Purpose**: Webhook testing/validation
- **HTTP Methods**: POST, PUT
- **Route**: `POST /api/v2/webhook/test?state=on|off|1|0` → `testWebhook(bool)`
- **Auth**: Not required

#### `void handleSAT()`
- **Location**: `restAPI.ino:644–821`
- **Purpose**: Smart Ambient Temperature (SAT) control and diagnostics
- **HTTP Methods**: GET, POST, PUT
- **Routes**:
  - `GET /api/v2/sat` or `/api/v2/sat/status` — full state (or with `?detail=full` for extended health)
  - `POST /api/v2/sat/target` — set target temperature (body: "21.0" or `{"value":"21.0"}`)
  - `POST /api/v2/sat/externaltemp` — push indoor temperature
  - `POST /api/v2/sat/externaloutdoor` — push outdoor temperature
  - `POST /api/v2/sat/humidity` — push indoor humidity (0–100%)
  - `POST /api/v2/sat/area/<0-3>` — push area temperature (multi-area)
  - `POST /api/v2/sat/flush` — flush short-lived data (PID integral + cycle window)
  - `POST /api/v2/sat/window` — set window open/closed state
  - `POST /api/v2/sat/preset` — apply preset
  - `POST /api/v2/sat/settings/<name>` — update SAT settings (mirrors MQTT sat/* commands)
  - `POST /api/v2/sat/reset_integral` — reset PID integral
- **Auth**: Required (`checkHttpAuth()` at start)
- **Extended diagnostics** (`?detail=full`):
  - Sync diagnostics (setpoint, modulation, CH mismatch)
  - Pressure metrics (smoothed pressure, drop rate, alarm)
  - Cycle diagnostics (kind, duration, count, fraction CH/DHW)
  - Error statistics (mean, stddev, sample count)
  - Health booleans (flame, device, cycle health)
  - Auto-tune & OVP calibration metrics

### JSON Response Helpers

#### `void sendStartJsonMap(const char *objName)`
- **Location**: `jsonStuff.ino:221–236`
- **Purpose**: Begin chunked JSON map response with optional object wrapper
- **Signature**: `void sendStartJsonMap(const char *objName)`
- **Behavior**:
  - Sets `CONTENT_LENGTH_UNKNOWN` for chunked transfer
  - Sends 200 OK with `application/json` mime type
  - Outputs `{` or `{"objName":{` based on parameter
  - Initializes `iIdentlevel = 1`, `bFirst = true`
- **Notes**: Entry and exit are paired; must call `sendEndJsonMap()` to close

#### `void sendEndJsonMap(const char *objName)`
- **Location**: `jsonStuff.ino:326–334`
- **Purpose**: Close JSON map response and end chunked transfer
- **Signature**: `void sendEndJsonMap(const char *objName)`
- **Behavior**: Outputs `}` or `}}` and empty chunk (end marker)

#### `void sendJsonMapEntry(const char *cName, T value)`
- **Location**: `jsonStuff.ino:241–325` (overloaded for bool, int32_t, uint32_t, float, const char*, String)
- **Purpose**: Stream one key-value pair into JSON map
- **Signature**: Multiple overloads:
  - `void sendJsonMapEntry(const char *cName, bool bValue)`
  - `void sendJsonMapEntry(const char *cName, int32_t iValue)`
  - `void sendJsonMapEntry(const char *cName, uint32_t uValue)`
  - `void sendJsonMapEntry(const char *cName, float fValue)` (3 decimal places)
  - `void sendJsonMapEntry(const char *cName, const char *cValue)` (with JSON escaping)
  - `void sendJsonMapEntry(const char *cName, String sValue)`
- **Behavior**:
  - Handles comma separator (via `sendBeforenext()`)
  - Adds indentation (via `sendIdent()`)
  - Streams value chunk by chunk (no intermediate buffering)
- **Notes**: String values auto-escaped; overloads for `__FlashStringHelper*` (PROGMEM) available

#### `void sendJsonOTmonMapEntry(const char *cName, T value, const char *cUnit, time_t epoch)`
- **Location**: `jsonStuff.ino:336–414`
- **Purpose**: Stream OT monitor map entry with value, unit, and timestamp
- **Signature**: Overloaded for const char*, int32_t, uint32_t, float, bool
- **Format**: `"name": {"value": V, "unit": "U", "epoch": T}`
- **Notes**: Supports Dallas temperature sensor format via `sendJsonOTmonMapEntryDallasTemp()`

#### `void sendJsonSettingObj(const char *cName, T value, const char *sType, ...)`
- **Location**: `jsonStuff.ino:446–513`
- **Purpose**: Stream a settings object with type metadata
- **Overloads**:
  - Integer: `(const char *cName, int iValue, const char *iType, int minValue, int maxValue)`
  - String: `(const char *cName, const char *cValue, const char *sType, int maxLen)`
  - Float: `(const char *cName, const char *cValue, const char *sType, int minValue, int maxValue)` (value as pre-formatted string)
  - Boolean: `(const char *cName, bool bValue, const char *sType)`
- **Format**: `"name": {"value": V, "type": "T", "min": M, "max": M}` or similar
- **Notes**: Used for `/api/v2/settings` endpoint

### Security & Validation Helpers

#### `bool checkHttpAuth()`
- **Location**: `restAPI.ino:89–105`
- **Purpose**: Validate HTTP Basic Auth credentials and CSRF origin
- **Signature**: `bool checkHttpAuth()`
- **Behavior**:
  - Returns true if auth disabled (empty password)
  - Allows HTTP_OPTIONS (CORS preflight)
  - Calls `httpServer.authenticate("admin", settings.sHTTPpasswd)`
  - Validates CSRF via `isSameOriginRequest()`
  - Sends 401 or 403 if validation fails
- **Dependencies**: `settings.sHTTPpasswd[]`

#### `static bool isSameOriginRequest()`
- **Location**: `restAPI.ino:54–83`
- **Purpose**: Validate CSRF by checking Origin/Referer against Host header
- **Signature**: `static bool isSameOriginRequest()`
- **Behavior**:
  - Extracts host from Origin or Referer header
  - Compares against request Host header
  - Permissive: no header → allow (non-browser clients)
- **Notes**: Prevents cross-origin POST/PUT mutations (ADR-054)

#### `void sendApiError(int httpCode, const __FlashStringHelper* message)`
- **Location**: `restAPI.ino:35–42`
- **Purpose**: Send standardized error JSON response (ADR-035)
- **Signature**: `void sendApiError(int httpCode, const __FlashStringHelper* message)`
- **Format**: `{"error":{"status":N,"message":"..."}}`
- **Behavior**: Adds CORS Origin header, sends with appropriate HTTP status

#### `void sendApiMethodNotAllowed(const __FlashStringHelper* allowedMethods)`
- **Location**: `restAPI.ino:45–48`
- **Purpose**: 405 Method Not Allowed with RFC 7231 Allow header
- **Signature**: `void sendApiMethodNotAllowed(const __FlashStringHelper* allowedMethods)`
- **Example**: `sendApiMethodNotAllowed(F("GET, POST"))`

### Command Submission

#### `static void handleCommandSubmit(const char* cmdStr)`
- **Location**: `restAPI.ino:164–190`
- **Purpose**: Validate and queue OpenTherm Gateway command
- **Signature**: `static void handleCommandSubmit(const char* cmdStr)`
- **Validation**:
  - Checks command interface available
  - Validates format: `LL=value` (two alphabetic letters, equals sign, value)
  - Max length: `kMaxCmdLen` (command queue slot limit)
  - Rejects non-alphabetic prefixes (prevent injection)
- **Response**: 202 Accepted (async): `{"status":"queued"}`
- **Queuing**: Via `addCommandToQueue()`

#### `static const char* satExtractPostValue(const char* body, char* buf, size_t bufSize)`
- **Location**: `restAPI.ino:531–552`
- **Purpose**: Extract numeric value from POST body (raw or JSON)
- **Signature**: `static const char* satExtractPostValue(const char* body, char* buf, size_t bufSize)`
- **Behavior**:
  - Tries to parse JSON `"value": <number>` first
  - Falls back to treating body as raw value
  - Copies result to caller-supplied buffer
  - Returns pointer into buffer (null-terminated)
- **Usage**: SAT endpoints (target, externaltemp, etc.)

### File Serving (FSexplorer)

#### `void startWebserver()`
- **Location**: `FSexplorer.ino:67–232`
- **Purpose**: Configure HTTP server routes and file serving
- **Behavior**:
  - Checks for `/index.html`; if missing, serves FSexplorer UI
  - Configures ETag-based conditional GET for index.html
  - Injects `?v=<fsHash>` into script URLs (cache-busting)
  - Sets up CSS/JS caching headers (1 day for versioned, no-cache for bare)
  - Registers `/api` catch-all → `processAPI()`
  - Enables CORS header collection (If-None-Match)
- **Key Feature**: Line-by-line HTML streaming with on-the-fly URL injection (avoids buffer overflow)

#### `void setupFSexplorer()`
- **Location**: `FSexplorer.ino:234–288`
- **Purpose**: Register LittleFS file explorer handlers and deprecate old API endpoints
- **Behavior**:
  - Serves `FSexplorer.html` if found (fallback to helper form)
  - Registers deprecated endpoints (v0/v1 API):
    - `/api/firmwarefilelist` → `apifirmwarefilelist()`
    - `/api/listfiles` → `apilistfiles()`
  - Registers file upload: `POST /upload` → `handleFileUpload()`
  - Registers reboot: `GET /ReBoot` → `reBootESP()`
  - Registers WiFi reset: `GET /ResetWireless` → `resetWirelessButton()`
  - Sets up `onNotFound()` handler for `/api/*` and file routing
- **Notes**: Old endpoints were scheduled for removal in v1.3.0 (ADR-035) but remain in v1.4.0 for backward compatibility

#### `void apifirmwarefilelist()`
- **Location**: `FSexplorer.ino:291–379`
- **Purpose**: Stream firmware file inventory as chunked JSON array
- **Signature**: `void apifirmwarefilelist()`
- **Response**: `[{"name":"file.hex","version":"X.Y.Z","size":1234},...]`
- **Behavior**:
  - Iterates directory `/` + device ID
  - Extracts `.hex` files, reads `.ver` companion files
  - Calls `GetVersion(hexfile)` to extract version from hex
  - Streams entries incrementally (150-byte buffer per entry)
  - Respects 40-file limit (`MAX_FILES_IN_LIST`)
  - Feeds watchdog during iteration
- **Notes**: Uses chunked transfer (streaming response without pre-buffering entire array)

#### `void apilistfiles()`
- **Location**: `FSexplorer.ino:398–483`
- **Purpose**: Stream LittleFS file listing with delete support
- **Signature**: `void apilistfiles()`
- **Query Parameters**:
  - `path=<dir>` — directory to list (default "/")
  - `delete=<path>` — delete file (requires auth)
- **Response**: `[{"name":"file.ext","size":1234,"type":"file|dir"},...,{"usedBytes":X,"totalBytes":Y,"freeBytes":Z,"truncated":false}]`
- **Behavior**:
  - Streams file entries (max 40 files)
  - Skips hidden files (names starting with '.')
  - Adds storage info as final entry (raw bytes — frontend formats)
  - Feeds watchdog per iteration
  - Delete requires `checkHttpAuth()`
- **Notes**: Sorting and size formatting delegated to frontend

#### `bool handleFile(String&& path)`
- **Location**: `FSexplorer.ino:487–501`
- **Purpose**: Serve static file from LittleFS with MIME type detection
- **Signature**: `bool handleFile(String&& path)`
- **Behavior**:
  - Handles delete query parameter (requires auth)
  - Appends `index.html` for directory paths
  - Streams file via `httpServer.streamFile()`
  - Auto-detects MIME type via `contentType(path)`
  - Returns true if successful, false if file not found
- **Notes**: Rvalue reference prevents dangling pointer

#### `void handleFileUpload()`
- **Location**: `FSexplorer.ino:505–548`
- **Purpose**: Handle multipart file uploads to LittleFS
- **Signature**: `void handleFileUpload()`
- **State Machine**:
  - `UPLOAD_FILE_START`: Auth check, open file for writing
  - `UPLOAD_FILE_WRITE`: Write chunks to file
  - `UPLOAD_FILE_END`: Close file, send 303 redirect (if auth passed)
- **Auth**: Checked at START; if fails, file is never opened (write prevented)
- **Notes**: Uses static `fsUploadFile` handle; max filename 30 chars; auto-truncates long names

#### `const String &contentType(String& filename)`
- **Location**: `FSexplorer.ino:569–586`
- **Purpose**: Map file extension to MIME type
- **Signature**: `const String &contentType(String& filename)`
- **Supported Types**:
  - `.html`, `.htm` → `text/html; charset=UTF-8`
  - `.css` → `text/css`
  - `.js` → `application/javascript`
  - `.json` → `application/json`
  - `.png`, `.gif`, `.jpg`, `.ico` → appropriate `image/*`
  - `.xml` → `text/xml`
  - `.pdf`, `.zip`, `.gz` → appropriate types
  - Default: `text/plain`
- **Notes**: Modifies string in-place; returns reference to modified string

### JSON Parsing & Serialization

#### `bool extractJsonField(const char* json, const __FlashStringHelper* key, char* result, size_t resultSize)`
- **Location**: `jsonStuff.ino:650–700`
- **Purpose**: Minimal streaming JSON field extraction for flat objects
- **Signature**: `bool extractJsonField(const char* json, const __FlashStringHelper* key, char* result, size_t resultSize)`
- **Supports**:
  - Quoted strings: `{"key":"value"}` → extracts "value"
  - Unquoted literals: `{"key":true}` or `{"key":42}` → extracts "true" or "42"
- **Behavior**:
  - Builds search pattern `"keyname"` using `cMsg` scratch buffer
  - Finds colon, skips whitespace, extracts value until quote/comma/brace
  - Handles backslash-escaped characters (via `unescapeJsonChar()`)
  - Returns true if field found and populated
- **Notes**: Uses `cMsg` as scratch; safe because `String::indexOf()` doesn't yield

#### `bool readJsonStringPair(File& f, char* key, size_t keySize, char* val, size_t valSize)`
- **Location**: `jsonStuff.ino:710–752`
- **Purpose**: Stream-read one "key":"value" pair from flat JSON file
- **Signature**: `bool readJsonStringPair(File& f, char* key, size_t keySize, char* val, size_t valSize)`
- **Behavior**:
  - Skips whitespace, commas, opening brace
  - Reads key until unescaped quote
  - Skips colon and value marker quote
  - Reads value until unescaped quote
  - Returns false at `}` (end of object) or on read error
- **Usage**: `ensureSensorDefaultLabels()` streams `dallas_labels.ini` without full file load
- **Escaping**: Handles `\"`, `\\`, etc. via `unescapeJsonChar()`

#### `void writeJsonStringPair(File& f, const char* key, const char* val, bool addComma)`
- **Location**: `jsonStuff.ino:759–768`
- **Purpose**: Write one "key":"value" pair to JSON file
- **Signature**: `void writeJsonStringPair(File& f, const char* key, const char* val, bool addComma)`
- **Behavior**:
  - Optionally prepends comma (for all but first pair)
  - Escapes key and value via `writeEscapedJsonStringContent()`
  - Outputs `"key":"value"`
- **Usage**: Rebuild `dallas_labels.ini` after updates

### Chunked JSON Streaming Helpers

#### `void escapeJsonStringTo(const char* src, char* dest, size_t destSize)`
- **Location**: `jsonStuff.ino:14–53`
- **Purpose**: In-place buffer escaping of JSON special characters
- **Signature**: `void escapeJsonStringTo(const char* src, char* dest, size_t destSize)`
- **Escapes**: `"`, `\`, `\b`, `\f`, `\n`, `\r`, `\t`, control chars (as `\uXXXX`)
- **Notes**: Outputs to caller-supplied buffer; returns early if buffer exhausted

#### `static void sendEscapedJsonStringContent(const char* src)`
- **Location**: `jsonStuff.ino:55–104`
- **Purpose**: Stream escaped JSON string without surrounding quotes
- **Signature**: `static void sendEscapedJsonStringContent(const char* src)`
- **Behavior**: Chunks output (24-byte buffer) to avoid large intermediate allocations
- **Usage**: String values in `sendJsonMapEntry()` and `sendJsonSettingObj()`

#### `static void writeEscapedJsonStringContent(File& f, const char* src)`
- **Location**: `jsonStuff.ino:106–155`
- **Purpose**: Write escaped JSON string to file
- **Signature**: `static void writeEscapedJsonStringContent(File& f, const char* src)`
- **Behavior**: Same as `sendEscapedJsonStringContent()` but writes to file handle
- **Usage**: `writeJsonStringPair()` for LittleFS file updates

### Data Structures

#### `struct ApiRoute`
- **Location**: `restAPI.ino` (defined before dispatch table)
- **Definition**:
  ```cpp
  struct ApiRoute {
    PGM_P              segment;     // Route name in PROGMEM (e.g., "health", "settings")
    ApiResourceHandler handler;     // Function pointer to handler
  };
  ```
- **Dispatch Table**: `kV2Routes[]` at line 1080-1097 maps resource names to handlers. Authoritative endpoint list: add new endpoints by adding one entry to this table and implementing the handler function. Sentinel entry `{ nullptr, nullptr }` marks table end.
- **Purpose**: Dispatch table entry mapping route to handler
- **Notes**: Sentinel entry has `segment = nullptr`; no changes needed to `processAPI()` router when adding endpoints

#### `struct FSInfo`
- **Location**: ESP8266 platform header
- **Used in**: `apilistfiles()` (line 467)
- **Fields**: `totalBytes`, `usedBytes`
- **Purpose**: Filesystem statistics for storage info endpoint

## API Endpoints Reference

### v2 API Routes (Complete)

| Method | Endpoint | Handler | Auth | Purpose |
|--------|----------|---------|------|---------|
| GET | `/api/v2/health` | handleHealth | No | System health metrics |
| GET | `/api/v2/settings` | handleSettings | Yes | Read device settings (sensitive data; includes WiFi SSID, nightly restart time) |
| POST/PUT | `/api/v2/settings` | handleSettings | Yes | Update device settings (supports all setting keys; nightly restart settings exposed v1.3.5+) |
| GET | `/api/v2/sensors` | handleSensors | No | Current sensor readings (Dallas + S0) |
| GET | `/api/v2/sensors/status` | handleSensors | No | Alias for /sensors |
| GET | `/api/v2/sensors/labels` | handleSensors | No | Dallas temperature labels |
| POST/PUT | `/api/v2/sensors/labels` | handleSensors | No | Update labels |
| GET | `/api/v2/device/info` | handleDevice | No | Device metadata |
| GET | `/api/v2/device/time` | handleDevice | No | Device clock state |
| GET | `/api/v2/device/crashlog` | handleDevice | No | Crash log (if available) |
| GET | `/api/v2/flash/status` | handleFlash | No | ESP8266 flash info |
| GET | `/api/v2/pic/flash-status` | handlePic | No | PIC flash status |
| GET | `/api/v2/pic/update-check` | handlePic | No | PIC firmware update check |
| GET | `/api/v2/pic/settings` | handlePic | No | PIC settings |
| GET | `/api/v2/otdirect/status` | handleOTDirect | No | OT-direct mode/status (OTGW32) |
| POST/PUT | `/api/v2/otdirect/mode?mode=...` | handleOTDirect | Yes | Set OT-direct mode |
| GET | `/api/v2/otdirect/settings` | handleOTDirect | No | OT-direct settings |
| POST/PUT | `/api/v2/otdirect/settings?...` | handleOTDirect | Yes | Update OT-direct settings |
| GET | `/api/v2/otdirect/overrides` | handleOTDirect | No | List OT message overrides |
| POST/PUT | `/api/v2/otdirect/overrides?action=sr&msgid=X&value=HHHH` | handleOTDirect | Yes | Set stored response |
| POST/PUT | `/api/v2/otdirect/overrides?action=cr&msgid=X` | handleOTDirect | Yes | Clear stored response |
| POST/PUT | `/api/v2/otdirect/overrides?action=rm&msgid=X&value=HHHH` | handleOTDirect | Yes | Set response modifier |
| POST/PUT | `/api/v2/otdirect/overrides?action=cm&msgid=X` | handleOTDirect | Yes | Clear response modifier |
| POST/PUT | `/api/v2/otdirect/overrides?action=ui&msgid=X` | handleOTDirect | Yes | Mark ID unknown |
| POST/PUT | `/api/v2/otdirect/overrides?action=ki&msgid=X` | handleOTDirect | Yes | Mark ID known |
| GET | `/api/v2/firmware/files` | handleFirmware | No | Firmware file inventory |
| GET | `/api/v2/filesystem/files` | handleFilesystem | No | LittleFS directory listing |
| GET | `/api/v2/filesystem/hash-check` | handleFilesystem | No | Filesystem hash (for ETag) |
| GET | `/api/v2/simulate` | handleSimulate | No | OTGW simulation status |
| POST/PUT | `/api/v2/simulate/start` | handleSimulate | No | Enable simulation |
| POST/PUT | `/api/v2/simulate/stop` | handleSimulate | No | Disable simulation |
| GET | `/api/v2/otgw/otmonitor` | handleOtgw | No | OT monitor values (Telegraf format) |
| GET | `/api/v2/otgw/telegraf` | handleOtgw | No | Telegraf-compatible OT values |
| GET | `/api/v2/otgw/messages/{id}` | handleOtgw | No | Get OT message value by ID |
| GET | `/api/v2/otgw/id/{id}` | handleOtgw | No | Alias for messages |
| GET | `/api/v2/otgw/label/{label}` | handleOtgw | No | Get OT message by label |
| POST/PUT | `/api/v2/otgw/commands` | handleOtgw | Yes | Queue OTGW command (body) |
| POST/PUT | `/api/v2/otgw/command/{cmd}` | handleOtgw | Yes | Queue OTGW command (legacy) |
| POST/PUT | `/api/v2/otgw/discovery` | handleOtgw | Yes | Trigger MQTT auto-configure |
| POST/PUT | `/api/v2/otgw/autoconfigure` | handleOtgw | Yes | Alias for discovery |
| POST/PUT | `/api/v2/webhook/test?state=on\|off\|1\|0` | handleWebhook | No | Test webhook |
| GET | `/api/v2/sat` | handleSAT | Yes | SAT status (or `?detail=full` for extended) |
| GET | `/api/v2/sat/status` | handleSAT | Yes | Same as above |
| POST/PUT | `/api/v2/sat/target` | handleSAT | Yes | Set target temperature |
| POST/PUT | `/api/v2/sat/externaltemp` | handleSAT | Yes | Push indoor temperature |
| POST/PUT | `/api/v2/sat/externaloutdoor` | handleSAT | Yes | Push outdoor temperature |
| POST/PUT | `/api/v2/sat/humidity` | handleSAT | Yes | Push humidity (0–100%) |
| POST/PUT | `/api/v2/sat/area/<0-3>` | handleSAT | Yes | Push area temperature |
| POST/PUT | `/api/v2/sat/flush` | handleSAT | Yes | Flush short-lived SAT data |
| POST/PUT | `/api/v2/sat/window` | handleSAT | Yes | Set window open/closed |
| POST/PUT | `/api/v2/sat/preset` | handleSAT | Yes | Apply preset |
| POST/PUT | `/api/v2/sat/settings/<name>` | handleSAT | Yes | Update SAT setting |
| POST | `/api/v2/sat/reset_integral` | handleSAT | Yes | Reset PID integral |
| OPTIONS | `/api/v2/*` | sendApiOptions | N/A | CORS preflight |

### File Serving Routes

| Method | Path | Handler | Purpose |
|--------|------|---------|---------|
| GET | `/` | sendIndex | Serve index.html with ETag caching |
| GET | `/index` | sendIndex | Alias for / |
| GET | `/index.html` | sendIndex | Alias for / |
| GET | `/index.css` | (lambda) | Serve CSS (1-day cache) |
| GET | `/index.js` | (lambda) | Serve JS (versioned or no-cache) |
| GET | `/graph.js` | (lambda) | Serve graph library |
| GET | `/FSexplorer.png` | serveStatic | Serve icon |
| POST | `/upload` | handleFileUpload | File upload to LittleFS (multipart) |
| GET | `/ReBoot` | reBootESP | Reboot device |
| GET | `/ResetWireless` | resetWirelessButton | Reset WiFi settings |
| GET | `/FSexplorer.html` | (lambda) | Fallback file explorer |
| GET | `/FSexplorer` | (lambda) | Alias for FSexplorer.html |

## Dependencies

### Internal Dependencies

- **Core modules**:
  - `OTGW-firmware.h` — main header, global state definitions
  - `OTGW-Core.ino` — OpenTherm message processing, `doAutoConfigure()`
  - `settingStuff.ino` — settings persistence, `updateSetting()`
  - `SATcontrol.ino` — SAT state machine, `satSendStatusJSON()`, `satSendHealthJSON()`
  - `OTmonitor.ino` — OT message history, `getMsgLastUpdated()`, `getOTGWValue()`
  - `MQTTstuff.ino` — MQTT integration, `doAutoConfigure()`
  - `DallasStuff.ino` — Dallas sensor management, `getDallasLabels()`, `ensureSensorDefaultLabels()`

- **Global functions**:
  - `sendHealth()` — health metrics endpoint
  - `sendDeviceSettings()` — settings in v2 format
  - `postSettings()` — settings update handler
  - `sendDeviceInfoV2()` — device metadata
  - `sendDeviceTimeV2()` — NTP/clock state
  - `sendDeviceCrashLog()` — crash dump if available
  - `sendFlashStatus()` — flash memory info
  - `sendPICFlashStatus()`, `sendPICUpdateCheck()`, `sendPICsettings()` — PIC info
  - `sendOTDirectStatus()` — OT-direct status
  - `addCommandToQueue()` — command queuing
  - `testWebhook()` — webhook validation
  - `satHandleTargetTemp()`, `satHandleExternalTemp()`, etc. — SAT input handlers
  - `satFlushShortLivedData()` — SAT flush operation
  - `getDallasLabels()`, `updateAllDallasLabels()` — sensor label management
  - `reBootESP()` — system reboot
  - `resetWiFiSettings()` — WiFi reset
  - `doRedirect()` — HTML redirect with countdown

### External Dependencies

- **ESP8266 Core**:
  - `ESP8266WebServer` (via `httpServer` global) — HTTP server, routing, auth, streaming
  - `LittleFS` (via `LittleFS` global) — filesystem operations
  - `time.h` — `time_t`, `time()` for timestamps
  - `string.h` — C string utilities (`strtok_r`, `strstr`, `strcmp_P`, etc.)
  - `ctype.h` — character classification (`isdigit`, `isxdigit`, `isalpha`)

- **Macros & Helpers**:
  - `PROGMEM`, `PSTR()`, `F()` — Flash memory string management
  - `snprintf_P()`, `strlcpy()`, `strcmp_P()` — Safe formatted output and comparisons
  - `CBOOLEAN()`, `CONOFF()` — Boolean-to-string conversion
  - `CSTR()` — Null-safe C string macro
  - `DebugTf()`, `DebugTln()` — Telnet debug output
  - `feedWatchDog()` — Watchdog timer reset

## Key Behaviors

### ETag Caching Strategy

1. **Index.html caching**:
   - Browser requests `/index.html`
   - Server computes `fsHash` (filesystem version marker)
   - Sends ETag header: `"<fsHash>"`
   - Browser stores in cache and sends `If-None-Match: "<fsHash>"` on next visit
   - Server compares: if unchanged → `304 Not Modified` (zero-byte response)
   - If FS upgraded → ETag changes → server sends `200 + full HTML`

2. **JS asset versioning**:
   - HTML contains `<script src="./index.js?v=<fsHash>"></script>`
   - Injection happens line-by-line during HTML stream (avoids buffering full file)
   - Versioned requests (matching fsHash) get long-term cache (1 day)
   - Unversioned requests get no-cache (force revalidation)

### Chunked JSON Streaming

All JSON responses (maps, arrays, OT monitor) are sent via chunked transfer encoding to avoid buffering entire responses:

1. `httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN)` — enable chunked mode
2. `httpServer.send(200, "application/json", "")` — start response
3. `httpServer.sendContent()` — stream chunks as data is generated
4. `httpServer.sendContent("")` — empty chunk marks end of transfer

This allows streaming 10KB+ responses without allocating 10KB buffer.

### CSRF & Origin Validation

- **Permissive for non-browser clients**: No Origin/Referer header → allow
- **Strict for browsers**: Origin/Referer must match request Host header
- **Allow CORS preflight**: OPTIONS method always allowed (no auth)
- **Centralized POST/PUT auth**: All mutations checked in `processAPI()` before dispatch

### Command Queuing & Validation

Commands are validated before queuing:

1. **Format validation**: Must be `LL=value` (two letters, equals, value)
2. **Alphabetic prefix check**: Prevents injection of non-command bytes
3. **Length check**: Must fit in command queue slot
4. **Async response**: 202 Accepted with `{"status":"queued"}`
5. **Queuing**: Via `addCommandToQueue()`, executed in background

### Handler Timing & Access Logging (v1.3.5+)

Each request generates a one-line telnet debug log when `state.debug.bRestAPI` flag is set (key 7):

- **Format**: `REST <METHOD> <URI> => <STATUS> <HANDLER_NAME> <ELAPSED_MS>ms`
- **Example**: `REST GET /api/v2/sensors => 200 v2/sensors 15ms`
- **Timing**: Measured from `processAPI()` entry to handler exit (includes routing overhead)
- **Non-API routes**: Logged with suffix `non-api`, deprecated v0/v1 with `deprecated`, routing failures with `not found`
- **Low-level errors**: Heap check and URI length errors logged separately before routing
- **Purpose**: One-line access log for performance diagnostics and request auditing without full request body logging

### SAT Extended Diagnostics

`GET /api/v2/sat/status?detail=full` returns:

- **Synchronization health**: setpoint mismatch, modulation reliability, CH sync
- **Pressure diagnostics**: smoothed pressure, drop rate, low-water alarm
- **Cycle analysis**: cycle kind, duration, count, CH/DHW fraction
- **Error statistics**: mean error, standard deviation, sample count, heating curve recommendation
- **Flame & device health**: boolean indicators (flame on, device ready, cycle health)
- **Auto-tune metrics**: score, cycles completed, OVP calibration phase

## Stack & Memory Considerations

- **processAPI() stack usage**: ~356 bytes (static buffers to save stack)
- **Static buffer sizes**:
  - `URI[50]` — max 50-char API path
  - `words[8][32]` — max 8 tokens, 32 chars each
  - `iIdentlevel`, `bFirst` — JSON state (global)
- **Chunked streaming**: No 10KB+ intermediate buffers
- **PROGMEM strings**: All string literals in PROGMEM to save RAM (OTGW principle)

## Notes

- **No ArduinoJson library** (ADR-001): JSON is built manually with `snprintf_P` and streamed via `httpServer.sendContent()`
- **Backward compatibility**: v0/v1 endpoints return 410 Gone; unversioned legacy endpoints (`/api/firmwarefilelist`, `/api/listfiles`) remain in v1.4.0 for backward compatibility (scheduled removal per ADR-035)
- **OTGW32 hardware detection**: OTDirect endpoints guarded by `#if HAS_DIRECT_OT` (OTGW32 exclusive)
- **Cooperative scheduling**: Static buffers in `processAPI()` avoid re-entrancy issues with `feedWatchDog()` yields
- **File upload security**: Auth check prevents file write if credentials invalid (no partial uploads)
- **Tornado-safe**: No `String` class in hot paths; `strlcpy()` and `snprintf_P()` prevent buffer overflows
- **Adding endpoints**: Modify `kV2Routes[]` dispatch table (line 1080-1097) only; implement handler function using pattern of existing handlers; no router changes needed
- **Handler timing**: All requests logged with response code and elapsed milliseconds when REST debug flag enabled (telnet debug key 7), supporting performance diagnostics and request auditing
- **Nightly restart settings** (v1.3.5+): `nightlyrestart` and `nightlyrestarthour` exposed via `/api/v2/settings` (lines 1754-1755); WiFi SSID and Reset WiFi button added to Settings API

## References

- **ADR-035**: API versioning policy; v0/v1 removal roadmap
- **ADR-050**: v2 API route dispatch table design
- **ADR-054**: CSRF protection and auth integration
- **Related code**: 
  - `/src/OTGW-firmware/OTGW-firmware.h` (settings, state structures)
  - `/src/OTGW-firmware/OTGW-Core.ino` (command execution)
  - `/src/OTGW-firmware/SATcontrol.ino` (SAT state machine)
  - `/src/OTGW-firmware/MQTTstuff.ino` (auto-configure)
