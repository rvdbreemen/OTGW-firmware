# GitHub Copilot Instructions for OTGW-firmware

## Project Overview

This is the ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW). It provides network connectivity (Web UI, MQTT, REST API, and TCP serial socket) for the OpenTherm Gateway hardware, with a focus on reliable Home Assistant integration.

## Architecture Decision Records (ADRs)

**IMPORTANT:** This project maintains Architecture Decision Records (ADRs) as docs-as-code that document key architectural choices. Before making changes that affect architecture, consult the relevant ADRs:

- **Platform & Architecture:** See `docs/adr/` directory for complete ADR index
- **Key decisions documented:** ESP8266 platform, modular .ino files, HTTP-only (no HTTPS), static buffers, PROGMEM strings, WebSocket streaming (OpenTherm messages only), MQTT integration, timer-based scheduling, LittleFS persistence, hardware watchdog, PIC firmware upgrade, Arduino framework, build system, NTP/timezone, command queue, WiFiManager, ArduinoJson, simplified OTA flash (XHR-based, see ADR-029)
- **ADR Index:** `docs/adr/README.md` provides navigation and decision summaries
- **ADR Skill:** `.github/skills/adr/SKILL.md` provides comprehensive ADR creation guidance

### When to Create ADRs

Create an ADR when a change affects:
- **Architecture**: Service/module structure, deployment topology, integration patterns, platform choices
- **NFRs (Non-Functional Requirements)**: Security, availability, performance, privacy/compliance, resilience
- **Interfaces/Contracts**: API contracts, breaking changes, major coupling/decoupling decisions
- **Dependencies**: New or replaced frameworks/libraries/tooling with broad impact
- **Build/Tooling**: Build system, CI/CD, development processes with architectural impact

Do NOT create ADRs for:
- Pure refactors without architectural impact
- Small dependency bumps without impact on architecture/NFRs
- Bug fixes that don't change architecture
- Minor feature additions within existing patterns

### ADR Lifecycle and Immutability

**Critical:** Accepted ADRs are **immutable**. Never edit an accepted ADR to change its meaning.

- **Proposed** → Draft, reviewable, can be revised
- **Accepted** → Decision stands, implementation follows/runs
- **Deprecated** → Decision is no longer recommended; kept for historical context until fully superseded
- **Superseded** → Replaced by newer decision (mark old ADR with "Superseded by ADR-XXX")

**If you need to reverse a decision:**
1. Create a NEW ADR that supersedes the old one
2. Mark the old ADR status as "Superseded by ADR-XXX"
3. In the new ADR, explicitly state "Supersedes: ADR-XXX"

### ADR Content: Focus on "Why"

ADRs must focus on **rationale and trade-offs** ("why"), not just implementation ("how"):
- Context (problem statement/forces/constraints)
- Decision (chosen approach with rationale)
- Alternatives Considered (2-3 options with pros/cons and rejection reasons)
- Consequences (positive impacts, negative impacts, risks with mitigation)
- References (link to relevant code, issues, PRs, documentation)

**Keep ADRs short** (1-2 screens); link to detailed design docs when needed.

### PR and Code Review Integration

**For all PRs:**
- If a PR changes architecture/NFRs/interfaces/dependencies/tooling, **link to the relevant ADR** in the PR description
- If no ADR exists and the change is architecturally significant, **create a new ADR as part of the same PR**
- If the code would reverse an existing ADR, **create a new superseding ADR** (don't rewrite history)

**During code review:**
- Verify architecturally significant changes have linked ADRs
- If ADR is missing, request one and explain which decision should be captured
- If code violates an accepted ADR, request alignment or a superseding ADR

**Legacy non-compliance:**
- Existing code may not comply with newer ADRs
- Call it out and propose remediation (incremental cleanup or tech-debt tasks)
- Don't let legacy non-compliance block new ADR adoption

### ADR Compliance

- Follow patterns established in ADRs (e.g., static buffers, PROGMEM, no HTTPS)
- Don't violate architectural decisions without discussing alternatives
- If architectural decisions change, create new ADR that supersedes the old one (immutability)
- Reference ADR numbers in code comments, reviews, and pull requests
- Use ADR skill (`.github/skills/adr/SKILL.md`) for creating comprehensive ADRs

## Technology Stack

- **Platform**: ESP8266 (NodeMCU / Wemos D1 mini)
- **Language**: Arduino C/C++ (.ino files)
- **Core Library**: ESP8266 Arduino Core 2.7.4
- **Filesystem**: LittleFS
- **Build System**: Makefile with arduino-cli
- **Key Libraries**:
  - WiFiManager - WiFi configuration and connection management
  - ArduinoJson - JSON parsing and serialization
  - PubSubClient - MQTT client
  - TelnetStream - Debug telnet server
  - AceTime - Time and timezone handling
  - OneWire/DallasTemperature - Dallas temperature sensors

## Architecture

- **Main firmware**: OTGW-firmware.ino (setup and main loop)
- **Modular .ino files**: Each module handles a specific feature (MQTT, REST API, settings, etc.)
- **Communication**: Serial interface to OpenTherm Gateway PIC controller
- **Integration**: MQTT for Home Assistant Auto Discovery, REST API, TCP socket for OTmonitor

## Architecture Decision Records (ADRs)

When making decisions for refactors, feature additions, or bug fixes, always review existing ADRs first so you understand prior context and constraints.

### ADR Location

- Store ADRs under `docs/adr/`.
- If the folder does not exist yet, create it when adding the first ADR.

### ADR Format (MANDATORY)

Use this template for new decisions and updates to existing ones:

```
# ADR-YYYYMMDD-Short-Title

## Status
Proposed | Accepted | Deprecated | Superseded

## Context
- What problem are we solving?
- What constraints apply (hardware, memory, security, compatibility)?
- What alternatives were considered?

## Decision
- The chosen approach and rationale.
- Why alternatives were not chosen.

## Consequences
- Expected benefits.
- Trade-offs, risks, or migration notes.

## Related
- Links to relevant code paths, issues, or PRs.
- Links to prior ADRs if this supersedes or depends on them.
```

### ADR Workflow (MANDATORY)

- **Before** implementing: read relevant ADRs to align with existing decisions.
- **During** planning: if a change materially alters architecture, protocols, data flow, or external behavior, write a new ADR.
- **After** implementation: ensure ADR status is updated (e.g., Proposed → Accepted) and reference the change.

## Network Architecture and Security

- **Target Environment**: Local network use only (not internet-exposed)
- **HTTP Only**: This codebase uses HTTP only, never HTTPS
- **WebSocket Protocol**: Always uses `ws://` protocol, never `wss://`
- **No TLS/SSL**: The ESP8266 firmware does not implement TLS/SSL encryption
- **Reverse Proxy**: While the REST API and basic Web UI can work behind a reverse proxy with HTTPS, WebSocket-based features (live OT message log) assume plain HTTP and may not function correctly via HTTPS reverse proxy
- **Security Model**: Device should be accessed only on trusted local networks; use VPN for remote access if needed

**CRITICAL**: Never add HTTPS or WSS (WebSocket Secure) protocol detection or support to this codebase. All network communication uses unencrypted HTTP and WS protocols.

## Coding Conventions

### General Guidelines

- Use Arduino/ESP8266 conventions and patterns
- Follow existing code style in the repository
- Prefer bounded buffers over `String` class to reduce heap fragmentation
- Always feed the watchdog in long-running operations
- Use telnet (port 23) for debug output, NOT Serial (Serial is reserved for OTGW PIC communication)

### Naming Conventions

- **Variables**: camelCase (e.g., `settingHostname`, `lastReset`)
- **Constants**: UPPER_CASE for defines (e.g., `ON`, `OFF`)
- **Functions**: camelCase (e.g., `startWiFi`, `readSettings`)
- **Global settings**: Prefix with `setting` (e.g., `settingMqttBroker`)

### Memory Management

- **Critical**: Avoid `String` class in performance-critical or frequently-called code
- Use `char` buffers with bounds checking for string operations
- Be mindful of heap fragmentation - this is an ESP8266 with limited RAM
- Prefer stack allocation for small, temporary buffers

#### PROGMEM Usage (CRITICAL - ESP8266 Has Limited RAM)

**MANDATORY**: All string literals MUST use `PROGMEM` to keep them in flash memory instead of RAM.

- **Always use `F()` macro** for string literals passed to functions that support it:
  ```cpp
  DebugTln(F("Message"));           // GOOD
  httpServer.send(200, F("text/html"), content);  // GOOD
  ```

- **Always use `PSTR()` macro** for string literals with printf-style functions:
  ```cpp
  DebugTf(PSTR("Value: %d\r\n"), value);  // GOOD
  snprintf_P(buffer, size, PSTR("Format: %s"), str);  // GOOD
  ```

- **Always use `PROGMEM` keyword** for string constants and arrays:
  ```cpp
  const char myString[] PROGMEM = "Long string";  // GOOD
  const char* const table[] PROGMEM = {str1, str2};  // GOOD
  
  // BAD - wastes RAM:
  const char myString[] = "Long string";
  const char Header[] = "HTTP/1.1 303 OK\r\n...";
  ```

- **Use `_P` variants for string comparisons**:
  - Use `strcmp_P()`, `strcasecmp_P()` for comparing null-terminated strings with PROGMEM literals.
  - **CRITICAL**: Use `memcmp_P()` for comparing binary data (not `strncmp_P()` or `strstr_P()`).
  - Wrap the distinct string literal in `PSTR()`.
  
  Example:
  ```cpp
  // GOOD - for null-terminated strings:
  if (strcmp_P(str, PSTR("value")) == 0) { ... }
  if (strcasecmp_P(field, PSTR("Hostname")) == 0) { ... }
  
  // GOOD - for binary data (hex files, raw buffers):
  if (memcmp_P(datamem + ptr, banner, bannerLen) == 0) { ... }
  
  // BAD - loads literal into RAM:
  if (strcmp(str, "value") == 0) { ... }
  
  // BAD - strncmp_P/strstr_P on binary data causes crashes:
  if (strncmp_P(datamem + ptr, banner, len) == 0) { ... }  // DANGEROUS!
  if (strstr_P(datamem + ptr, banner) != NULL) { ... }      // DANGEROUS!
  ```

- **Create function overloads** when existing functions don't support PROGMEM types:
  - If a function only accepts `const char*`, create an overload accepting `const __FlashStringHelper*` or `PGM_P`
  - Use `pgm_read_byte()` and related functions to read from PROGMEM
  - **Never** copy PROGMEM strings to RAM buffers just to call a function - create the overload instead
  
  Example:
  ```cpp
  // Original function
  void sendData(const char* data) { /* ... */ }
  
  // Add PROGMEM overload
  void sendData(const __FlashStringHelper* data) {
    PGM_P p = reinterpret_cast<PGM_P>(data);
    // Read from PROGMEM and process
  }
  ```

- **Refactor existing code** to use PROGMEM when you encounter RAM-based string literals
- The ESP8266 has only ~80KB of RAM total, with ~40KB typically available after core libraries
- Every byte of string literal in RAM is a byte unavailable for runtime operations
- This is **non-negotiable** - PROGMEM usage is critical for firmware stability

#### File Serving and Streaming (CRITICAL - Prevents Memory Exhaustion)

**MANDATORY**: Never load large files entirely into RAM. Use streaming for files >2KB.

**Critical Rules**:

1. **NEVER use `File.readString()` for files >2KB**
   - `index.html` is ~11KB - loading it causes memory exhaustion
   - Each String allocation fragments the heap
   - Multiple concurrent requests can crash the device

2. **ALWAYS use streaming for unmodified files**:
   ```cpp
   // GOOD - Direct streaming (minimal memory)
   File f = LittleFS.open("/index.html", "r");
   if (!f) {
     httpServer.send(404, F("text/plain"), F("File not found"));
     return;
   }
   httpServer.streamFile(f, F("text/html; charset=UTF-8"));
   f.close();
   ```

3. **Use chunked transfer encoding for files requiring modification**:
   ```cpp
   // GOOD - Stream with line-by-line modifications
   File f = LittleFS.open("/index.html", "r");
   if (!f) {
     httpServer.send(404, F("text/plain"), F("File not found"));
     return;
   }
   
   httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
   httpServer.send(200, F("text/html; charset=UTF-8"), F(""));
   
   while (f.available()) {
     String line = f.readStringUntil('\n');
     
     // Modify line if needed (use indexOf() before replace() for efficiency)
     if (line.indexOf(F("src=\"./index.js\"")) >= 0) {
       line.replace(F("src=\"./index.js\""), "src=\"./index.js?v=" + version);
     }
     
     httpServer.sendContent(line);
     if (f.available() || line.length() > 0) {
       httpServer.sendContent(F("\n"));
     }
   }
   httpServer.sendContent(F("")); // End chunked stream
   f.close();
   ```

4. **Eliminate code duplication with lambdas**:
   ```cpp
   // GOOD - Single handler for multiple routes
   auto sendIndex = []() {
     // Implementation here
   };
   
   httpServer.on("/", sendIndex);
   httpServer.on("/index", sendIndex);
   httpServer.on("/index.html", sendIndex);
   
   // BAD - Duplicate code for each route
   httpServer.on("/", []() { /* duplicate code */ });
   httpServer.on("/index", []() { /* duplicate code */ });
   httpServer.on("/index.html", []() { /* duplicate code */ });
   ```

5. **Cache expensive operations**:
   ```cpp
   // GOOD - Static caching for read-once data
   String getFilesystemHash() {
     static String _githash = ""; // Cached value
     
     if (_githash.length() > 0) return _githash; // Return cached
     
     // Read from file only on first call
     File f = LittleFS.open("/version.hash", "r");
     if (f && f.available()) {
       _githash = f.readStringUntil('\n');
       _githash.trim();
       f.close();
     }
     return _githash;
   }
   
   // BAD - Reading file on every call
   String getFilesystemHash() {
     File f = LittleFS.open("/version.hash", "r");
     String hash = f.readStringUntil('\n');
     f.close();
     return hash;
   }
   ```

6. **Memory impact guidelines**:
   - **<1KB files**: Can use `readString()` if absolutely necessary
   - **1-5KB files**: Use streaming if possible; `readString()` only if no alternative
   - **>5KB files**: MUST use streaming, NEVER load into memory
   - **>10KB files**: CRITICAL - Always stream, can cause crash if loaded

7. **Check for String operations that increase memory**:
   ```cpp
   // BAD - Multiple allocations and reallocations
   String html = f.readString();  // Allocation 1: 11KB
   html.replace("old", "new");    // Allocation 2: 11KB + growth
   html.replace("foo", "bar");    // Allocation 3: 11KB + growth
   httpServer.send(200, type, html); // All in memory at once
   
   // GOOD - Minimal allocations via streaming
   while (f.available()) {
     String line = f.readStringUntil('\n'); // Allocation: ~100-500 bytes
     if (line.indexOf(F("old")) >= 0) {
       line.replace(F("old"), "new");
     }
     httpServer.sendContent(line + "\n");   // Line sent and freed
   }
   ```

**Why This Matters**:
- ESP8266 has ~40KB available RAM after core libraries
- Loading 11KB file + modifications = >22KB peak memory usage (>50% of available RAM)
- Multiple concurrent requests can easily exhaust memory
- String operations cause heap fragmentation
- Memory exhaustion leads to crashes and watchdog resets

**Reference**: See `docs/reviews/2026-02-01_memory-management-bug-fix/BUG_FIX_ASSESSMENT.md` for detailed analysis of a real bug caused by violating these rules.

### Binary Data Handling (CRITICAL - Prevents Exception Crashes)

**MANDATORY**: When working with binary data (hex files, raw buffers), use proper comparison functions.

- **Never use `strncmp_P()`, `strstr_P()`, or `strnlen()` on binary data**
  - These functions expect null-terminated strings and may read beyond buffer boundaries
  - Binary data from hex files does NOT have null terminators
  - This causes Exception (2) crashes when reading protected memory

- **Always use `memcmp_P()` for binary data comparison with PROGMEM**:
  ```cpp
  // CORRECT - sliding window search in binary data:
  size_t bannerLen = sizeof(banner) - 1;
  if (datasize >= bannerLen) {
      for (ptr = 0; ptr <= (datasize - bannerLen); ptr++) {
          if (memcmp_P(datamem + ptr, banner, bannerLen) == 0) {
              // Found banner in binary data
              break;
          }
      }
  }
  
  // WRONG - causes buffer overruns and crashes:
  while (ptr < datasize) {
      char *s = strstr_P(datamem + ptr, banner);  // DANGEROUS!
      if (s == nullptr) {
          ptr += strnlen(datamem + ptr, datasize - ptr) + 1;  // DANGEROUS!
      }
  }
  ```

- **Key files with binary data handling**:
  - `versionStuff.ino` - GetVersion() parses hex files
  - `src/libraries/OTGWSerial/OTGWSerial.cpp` - readHexFile() parses hex files
  - Both must use `memcmp_P()` for banner searching, not `strncmp_P()` or `strstr_P()`

- **Always add underflow protection**:
  ```cpp
  // Check buffer is large enough before loop
  if (datasize >= bannerLen) {
      for (ptr = 0; ptr <= (datasize - bannerLen); ptr++) {
          // Safe: ptr + bannerLen will never exceed datasize
      }
  }
  ```

### Security Practices

- Validate all user inputs (REST API, MQTT commands, Web UI)
- Check buffer bounds before copying data
- Sanitize URL parameters and redirects
- Never expose passwords in plain text (use masking in Web UI)
- **Use `memcmp_P()` for binary data** (see Binary Data Handling section above)

### Debug Output

- Use the Debug macros for telnet output: `DebugTln()`, `DebugTf()`, `Debugln()`, `Debugf()`
- **Never** write to Serial after OTGW initialization (Serial is for PIC communication only)
- Setup phase can use `SetupDebugTln()` family of macros
- **MANDATORY**: Always use `F()` macro for string literals: `DebugTln(F("Message"))`
- **MANDATORY**: Always use `PSTR()` macro for formatted strings: `DebugTf(PSTR("Value: %d"), val)`
- Never use plain string literals without `F()` or `PSTR()` - see Memory Management section

### Browser Compatibility (MANDATORY for Frontend Code)

**CRITICAL**: All frontend JavaScript MUST be compatible with Chrome, Firefox, and Safari (latest versions and 2 versions back).

#### Required Compatibility Checks

Before implementing or modifying frontend features, verify browser support:

1. **WebSocket API**
   - ✅ Fully supported: Chrome 16+, Firefox 11+, Safari 6+
   - Always check `readyState` before sending (OPEN = 1, CONNECTING = 0)
   - Handle connection/disconnection gracefully

2. **Fetch API**
   - ✅ Fully supported: Chrome 42+, Firefox 39+, Safari 10.1+
   - **MANDATORY**: Always include error handling with `.catch()`
   - **MANDATORY**: Check `response.ok` before processing (HTTP errors don't throw)
   - **MANDATORY**: Validate content-type before calling `response.json()`

3. **JSON APIs**
   - ✅ Fully supported: Chrome 3+, Firefox 3.5+, Safari 4+
   - **MANDATORY**: Wrap `JSON.parse()` in try-catch blocks
   - **MANDATORY**: Validate input is JSON before parsing
   - Example:
     ```javascript
     try {
       if (data && data.startsWith('{')) {
         const json = JSON.parse(data);
         // Process json
       }
     } catch (e) {
       console.error('JSON parse error:', e);
     }
     ```

4. **DOM Manipulation**
   - **MANDATORY**: Check element exists before accessing properties
   - Use `querySelector()` / `querySelectorAll()` (modern, well-supported)
   - Example:
     ```javascript
     const element = document.getElementById('myId');
     if (element) {
       element.innerText = 'Hello';
     }
     ```

#### Forbidden Patterns (Browser-Specific or Incompatible)

**NEVER** use these patterns:

- ❌ Browser-specific prefixes (`-webkit-`, `-moz-`, etc.) without fallbacks
- ❌ Relying on fetch to throw on HTTP errors (it doesn't - check `response.ok`)
- ❌ Assuming JSON without validation (always check content-type and format)
- ❌ Missing error handlers on async operations (always add `.catch()`)
- ❌ Using experimental APIs without checking support
- ❌ Vendor-specific JavaScript extensions
- ❌ Missing null/undefined checks on DOM elements

#### Best Practices (MANDATORY)

1. **Error Handling**
   ```javascript
   // GOOD - Complete error handling
   fetch('/api/v1/data')
     .then(response => {
       if (!response.ok) {
         throw new Error(`HTTP ${response.status}`);
       }
       return response.json();
     })
     .then(data => {
       // Process data
     })
     .catch(error => {
       console.error('Fetch error:', error);
       // Handle error gracefully
     });
   
   // BAD - Missing error handling
   fetch('/api/v1/data')
     .then(response => response.json())
     .then(data => {
       // Process data
     });
   ```

2. **WebSocket State Management**
   ```javascript
   // GOOD - Check state before sending
   if (webSocket && webSocket.readyState === WebSocket.OPEN) {
     webSocket.send(message);
   }
   
   // BAD - No state check
   webSocket.send(message);
   ```

3. **JSON Parsing Safety**
   ```javascript
   // GOOD - Validated parsing
   function parseJSON(data) {
     if (!data || typeof data !== 'string') return null;
     if (!data.startsWith('{') && !data.startsWith('[')) return null;
     
     try {
       return JSON.parse(data);
     } catch (e) {
       console.error('JSON parse error:', e);
       return null;
     }
   }
   
   // BAD - Unprotected parsing
   const json = JSON.parse(data);
   ```

4. **DOM Safety**
   ```javascript
   // GOOD - Existence check
   const element = document.getElementById('myId');
   if (element) {
     element.innerText = 'Value';
   }
   
   // BAD - Assuming element exists
   document.getElementById('myId').innerText = 'Value';  // May throw
   ```

#### Testing Requirements

- **MANDATORY**: Test all frontend changes in Chrome, Firefox, and Safari
- **MANDATORY**: Check browser console for errors during testing
- **MANDATORY**: Verify WebSocket connections work in all browsers
- **MANDATORY**: Test error scenarios (network failures, invalid responses)
- Use browser DevTools to verify:
  - No JavaScript errors in console
  - Network requests complete successfully
  - WebSocket connections establish and maintain
  - JSON parsing succeeds

#### Reference Resources

- **MDN Web Docs**: Authoritative source for browser compatibility
- **Can I Use**: https://caniuse.com for feature support tables
- Follow ECMAScript 5 (ES5) or later standards
- Avoid bleeding-edge features without checking support

### OpenTherm Protocol

- Message IDs are defined in `OTGW-Core.h`
- Commands sent to OTGW PIC use a command queue (see `addOTWGcmdtoqueue`)
- OTGW commands are two-letter codes (e.g., `TT` for temporary temperature override, `SW` for DHW setpoint)
- Always validate OpenTherm message format before processing

### MQTT

- Topic structure: `<mqtt-prefix>/value/<node-id>/<sensor>` for publishing
- Command structure: `<mqtt-prefix>/set/<node-id>/<command>` for subscriptions
- Support Home Assistant MQTT Auto Discovery
- Commands map to OTGW commands via the `setcmds` array in `MQTTstuff.ino`

### REST API

- API versioning: `/api/v0/` (legacy), `/api/v1/` (standard), and `/api/v2/` (optimized)
- OTGW commands: POST/PUT to `/api/v1/otgw/command/{command}`
- Check system health: `/api/v1/health` (Returns JSON map with health metrics)
  - Response format: `{"health": {"status": "UP", "uptime": "...", "heap": 12345, ...}}`
  - Access values via map: `data.health.status`, `data.health.heap`, etc.
  - **Map format** - use simple object property access
- Device time: `/api/v0/devtime` (Returns JSON array with time data)
  - Response format: `{"devtime": [{"name": "dateTime", "value": "..."}, {"name": "epoch", "value": 123}, ...]}`
  - **Array format** - use `.find()` or array iteration
- Commands use the same queue as MQTT commands
- **Reboot Verification**: WebUI must check `/api/v1/health` to confirm the device is back online.
  - Validate with: `data.health && data.health.status === 'UP'`

### Build and Test

- **Build locally**: `python build.py` or `make -j$(nproc)`
  - Build firmware only: `python build.py --firmware`
  - Build filesystem only: `python build.py --filesystem`
  - Clean build: `python build.py --clean`
  - Build script auto-installs arduino-cli if missing
- **Flash firmware**: `python flash_esp.py` (downloads and flashes latest release)
  - Flash from local build: `python flash_esp.py --build`
  - See [FLASH_GUIDE.md](../docs/FLASH_GUIDE.md) for detailed instructions
- **Evaluate code quality**: `python evaluate.py`
  - Quick check: `python evaluate.py --quick`
  - Generate report: `python evaluate.py --report`
  - See [EVALUATION.md](../docs/EVALUATION.md) for evaluation framework details
- **Build artifacts**: Located in `build/` directory with versioned filenames
- **CI/CD**: Uses GitHub Actions with the same Makefile
- **Always test on actual hardware when possible** (ESP8266 behavior can differ from simulation)

## Common Patterns

### Settings Management

- Settings are stored in LittleFS as JSON files
- Read settings with `readSettings()`
- Write settings with `writeSettings()`
- Settings structure is defined in `OTGW-firmware.h`

### Command Queue

- OTGW commands are queued to prevent overrunning the serial buffer
- Use `addOTWGcmdtoqueue(command)` to queue commands
- Never send commands directly to Serials

### Timer Management

- Use the `DECLARE_TIMER_SEC()` and `DECLARE_TIMER_MS()` macros
- Check timers with `DUE(timer)` macro
- Timers are defined in `safeTimers.h`

### GPIO and Hardware

- LED control: `setLed(LED1, ON)` or `setLed(LED2, OFF)`
- GPIO outputs can follow OpenTherm status bits (configurable)
- Dallas sensors on configurable GPIO (default GPIO10)
- S0 pulse counter on configurable GPIO

## Documentation

- User-facing documentation lives in the wiki: https://github.com/rvdbreemen/OTGW-firmware/wiki
- Build instructions: `docs/BUILD.md`
- Flash instructions: `docs/FLASH_GUIDE.md`
- Update README.md for significant feature changes
- Keep release notes format consistent (see README.md)
- **OpenTherm Protocol**:
  - Documentation is found in the `Specification` folder
  - Version 2.2: `Specification/Opentherm Protocol v2.2.pdf`
  - Version 4.2: `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
  - Other files in the `Specification` folder provide additional information
- **PIC Firmware & Hardware**:
  - Schelte Bron's website: https://otgw.tclcode.com/index.html#intro
  - Firmware commands: https://otgw.tclcode.com/firmware.html
  - Support Forum: https://otgw.tclcode.com/support/forum
  - **Critical source** for PIC firmware behavior and serial port communication documentation.
  - Consult this for any issues related to the PIC controller.

## Code Review and Analysis Documentation

When performing code reviews or analysis work, always preserve the documentation for historical reference.

### Review Documentation Structure

Store all review and analysis documentation in:
```
docs/reviews/YYYY-MM-DD_<review-name>/
```

Example: `docs/reviews/2026-01-17_dev-rc4-analysis/`

### Required Metadata Header

**MANDATORY**: All review documents MUST include a metadata header at the top:

```markdown
---
# METADATA
Document Title: [Descriptive title]
Review Date: YYYY-MM-DD HH:MM:SS UTC
Branch Reviewed: [source] → [target] (merge commit [hash])
Target Version: [version number]
Reviewer: [GitHub Copilot Advanced Agent / other]
Document Type: [type]
PR Branch: [branch name]
Commit: [commit hash]
Status: [COMPLETE/IN PROGRESS/etc.]
---
```

### Document Types to Create

1. **Main Review Document** (`*_REVIEW.md`)
   - Complete technical analysis
   - Issue-by-issue breakdown
   - Code examples and explanations
   - Security and memory safety assessment
   - Testing recommendations

2. **Executive Summary** (`REVIEW_SUMMARY.md`)
   - High-level overview for decision-makers
   - Priority matrix
   - Risk assessment
   - Quality metrics
   - Merge recommendations

3. **Action Checklist** (`ACTION_CHECKLIST.md`)
   - Step-by-step implementation guide
   - Copy/paste code snippets
   - Verification commands
   - Success criteria

4. **Fix Documentation** (e.g., `HIGH_PRIORITY_FIXES.md`)
   - Detailed fix suggestions
   - Before/after code examples
   - Implementation options

5. **Review Index** (`REVIEW_INDEX.md`)
   - Navigation hub
   - Document selector by audience
   - Quick reference guide

6. **Archive README** (`README.md`)
   - Overview of the review
   - Summary of issues found and fixed
   - Timeline of events
   - How to use the archive

### Workflow for Code Reviews

1. **Perform Analysis**
   - Analyze code changes thoroughly
   - Identify issues by priority (Critical, High, Medium, Low)
   - Document findings in structured format

2. **Create Documentation**
   - Create review directory: `docs/reviews/YYYY-MM-DD_<review-name>/`
   - Generate all required documents with metadata headers
   - Include version, branch, date/time in all documents

3. **Apply Fixes** (if applicable)
   - Implement fixes with minimal code changes
   - Document each fix with rationale
   - Update review documents to reflect fixes applied

4. **Archive Documentation**
   - Move all review documents to the archive directory
   - Remove temporary/working documents from repository root
   - Create comprehensive README.md for the archive
   - Update main README.md if needed

5. **Preservation**
   - Never delete review archives
   - Archives serve as historical reference
   - Future reviews should reference previous ones when relevant

### Example Structure

```
docs/reviews/
├── 2026-01-17_dev-rc4-analysis/
│   ├── README.md                    # Archive overview
│   ├── DEV_RC4_BRANCH_REVIEW.md    # Complete analysis
│   ├── REVIEW_SUMMARY.md           # Executive summary
│   ├── ACTION_CHECKLIST.md         # Implementation steps
│   ├── HIGH_PRIORITY_FIXES.md      # Detailed fixes
│   └── REVIEW_INDEX.md             # Navigation
└── YYYY-MM-DD_<next-review>/
    └── ...
```

### Benefits

- **Historical Context**: Future developers can understand past decisions
- **Reproducibility**: Review methodology can be replicated
- **Learning**: Patterns of issues can be identified over time
- **Accountability**: Clear record of what was reviewed and fixed
- **Knowledge Transfer**: New team members can learn from past reviews

### Tools and References

- Use evaluation framework: `python evaluate.py` (see docs/EVALUATION.md)
- Follow coding standards documented in this file
- Reference previous reviews for consistency
- Link to related PRs and commits in documentation

## Important Constraints

- **Never** send debug output to Serial after OTGW initialization
- **Never** flash PIC firmware over WiFi using OTmonitor (can brick the PIC)
- **Always** use the command queue for OTGW commands
- **Always** validate buffer sizes before string operations
- **Always** use `PROGMEM` (`F()` or `PSTR()`) for string literals - ESP8266 RAM is severely limited
- **Always** test MQTT and Home Assistant integration for relevant changes
- **Always** consider backwards compatibility with existing configurations
- Target audience: Local network use only (not internet-exposed)

## Home Assistant Integration

- Primary integration method: MQTT Auto Discovery
- Alternative: Home Assistant OpenTherm Gateway integration via TCP socket (port 25238)
- All sensors should support MQTT discovery when applicable
- Use unique IDs for MQTT discovery (configurable via settings)
- Climate entity uses temporary temperature override (`TT` command)

## Testing Guidance

- Test on both NodeMCU and Wemos D1 mini if possible
- Verify MQTT publish/subscribe functionality
- Test Web UI on multiple browsers
- Verify serial communication with actual OTGW hardware
- Check memory usage and heap fragmentation during operation
- Test OTA updates

## Dependencies and Security

- Avoid adding new dependencies unless absolutely necessary
- Check library compatibility with ESP8266 Arduino Core 2.7.4
- Review security implications of any network-facing changes
- Consider impact on limited ESP8266 resources (RAM, flash)

## Contribution Workflow

- Follow existing code organization (modular .ino files)
- Add version bumps to `version.h` for releases
- Update release notes in README.md for user-facing changes
- **Run evaluation before submitting**: `python evaluate.py` to check code quality
- **Test builds**: `python build.py` before submitting
- Ensure changes work with the NodoShop OTGW hardware

## Development Tools

### Build System (`build.py`)

The Python build script automates the entire build process:
- Auto-installs arduino-cli if not present
- Updates version information from git
- Builds firmware and filesystem
- Creates versioned artifacts in `build/` directory
- Options: `--firmware`, `--filesystem`, `--clean`, `--no-rename`

### Flash Tool (`flash_esp.py`)

Automated firmware flashing for ESP8266:
- Default: Downloads and flashes latest GitHub release
- `--build`: Builds from source and flashes
- Handles both firmware and filesystem images
- Platform-independent (Windows, Mac, Linux)

### Evaluation Framework (`evaluate.py`)

Comprehensive code quality analysis tool:
- **Categories**: Code structure, coding standards, memory patterns, build system, dependencies, documentation, security, git health, filesystem data
- **Usage patterns**:
  - Full evaluation: `python evaluate.py`
  - Quick check: `python evaluate.py --quick` (essentials only)
  - Generate JSON report: `python evaluate.py --report`
  - Verbose output: `python evaluate.py --verbose`
- **Key checks**:
  - Detects improper `Serial.print()` usage (should use Debug macros)
  - Flags excessive `String` class usage (heap fragmentation risk)
  - Validates OpenTherm command format in MQTT mappings
  - Checks for buffer overflow vulnerabilities
  - Verifies header guards in .h files
  - Validates build system health
- **Exit codes**: Non-zero if any FAIL results (CI/CD integration)
- See [docs/EVALUATION.md](../docs/EVALUATION.md) for detailed documentation
