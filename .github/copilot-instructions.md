# GitHub Copilot Instructions for OTGW-firmware

## Project Overview

This is the ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW). It provides network connectivity (Web UI, MQTT, REST API, and TCP serial socket) for the OpenTherm Gateway hardware, with a focus on reliable Home Assistant integration.

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
- Check system health: `/api/v1/health` (Returns `status: UP` and system vital signs)
- Commands use the same queue as MQTT commands
- **Reboot Verification**: WebUI must check `/api/v1/health` to confirm the device is back online.
  - Expected response includes `status: UP` and `picavailable: true`.

### Build and Test

- **Build locally**: `python build.py` or `make -j$(nproc)`
  - Build firmware only: `python build.py --firmware`
  - Build filesystem only: `python build.py --filesystem`
  - Clean build: `python build.py --clean`
  - Build script auto-installs arduino-cli if missing
- **Flash firmware**: `python flash_esp.py` (downloads and flashes latest release)
  - Flash from local build: `python flash_esp.py --build`
  - See [FLASH_GUIDE.md](FLASH_GUIDE.md) for detailed instructions
- **Evaluate code quality**: `python evaluate.py`
  - Quick check: `python evaluate.py --quick`
  - Generate report: `python evaluate.py --report`
  - See [EVALUATION.md](EVALUATION.md) for evaluation framework details
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
- Build instructions: `BUILD.md`
- Flash instructions: `FLASH_GUIDE.md`
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

- Use evaluation framework: `python evaluate.py` (see EVALUATION.md)
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
- See [EVALUATION.md](EVALUATION.md) for detailed documentation
