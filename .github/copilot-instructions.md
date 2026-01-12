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

### Security Practices

- Validate all user inputs (REST API, MQTT commands, Web UI)
- Check buffer bounds before copying data
- Sanitize URL parameters and redirects
- Never expose passwords in plain text (use masking in Web UI)

### Debug Output

- Use the Debug macros for telnet output: `DebugTln()`, `DebugTf()`, `Debugln()`, `Debugf()`
- **Never** write to Serial after OTGW initialization (Serial is for PIC communication only)
- Setup phase can use `SetupDebugTln()` family of macros
- Use `F()` macro for string literals to save RAM: `DebugTln(F("Message"))`

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

- API versioning: `/api/v0/` (legacy) and `/api/v1/` (current)
- OTGW commands: POST/PUT to `/api/v1/otgw/command/{command}`
- Commands use the same queue as MQTT commands

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

## Important Constraints

- **Never** send debug output to Serial after OTGW initialization
- **Never** flash PIC firmware over WiFi using OTmonitor (can brick the PIC)
- **Always** use the command queue for OTGW commands
- **Always** validate buffer sizes before string operations
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
