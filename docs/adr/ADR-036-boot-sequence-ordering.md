# ADR-036: Boot Sequence Initialization Ordering

**Status:** Accepted  
**Date:** 2026-02-16  
**Updated:** 2026-02-16 (Initial documentation of existing pattern)

## Context

The OTGW-firmware `setup()` function initializes ~20 subsystems in a specific order. Several of these subsystems have **non-obvious ordering dependencies** that, if violated, cause failures ranging from silent misconfiguration to hard crashes.

**Problem scenarios observed or anticipated:**
- NTP configured **after** WiFi → DHCP cannot override NTP server (ESP8266 SDK limitation)
- MQTT started **before** webserver → port conflict or initialization race
- PIC reset **after** OTGWstream start → serial bridge receives stale/corrupt data
- Watchdog enabled **during** WiFi connect → watchdog fires on slow WiFi (240s timeout)
- Sensors/S0 initialized **before** MQTT → auto-discovery messages lost

**Constraints:**
- ESP8266 single-core: no parallelism, strict sequential initialization
- WiFi connection can take 1-240 seconds (captive portal timeout)
- External I2C hardware watchdog cannot be fed during blocking operations
- PIC serial port must be probed before any OTGW communication
- LittleFS must be mounted before settings are read (all services depend on settings)

## Decision

**Maintain a strictly ordered boot sequence in `setup()` with documented dependency rationale for each phase.**

The boot sequence is organized into 5 phases:

### Phase 1: Hardware Detection (no network, no filesystem)
```
1. WatchDogEnabled(0)     — Disable watchdog during setup
2. detectPIC()            — Probe PIC firmware via serial (must be first serial use)
3. randomSeed(RANDOM_REG32) — Hardware RNG seed
4. setLed(LED1/LED2, ON)  — Visual boot indicator
```
**Rationale:** Hardware must be detected before any configuration or network operations. Watchdog must be off because WiFi connect can block for minutes.

### Phase 2: Filesystem & Configuration (no network yet)
```
5. LittleFS.begin()       — Mount filesystem
6. readSettings(true)     — Load all settings from /settings.ini
7. checklittlefshash()    — Compare filesystem hash with firmware hash
```
**Rationale:** All subsequent operations depend on settings (hostname, MQTT broker, NTP server, GPIO pins). Settings must be loaded before any network service starts.

### Phase 3: Network Connectivity
```
8.  startNTP()            — Configure NTP **BEFORE** WiFi (DHCP override)
9.  startWiFi(hostname, 240) — Connect to WiFi (up to 240s timeout)
10. startTelnet()         — Debug port 23 (available immediately after WiFi)
11. startMDNS(hostname)   — mDNS responder for hostname.local resolution
12. startLLMNR(hostname)  — LLMNR for Windows hostname resolution
```
**Critical dependency:** NTP must be configured before WiFi connects. The ESP8266 SDK's `configTime()` sets up SNTP which can be overridden by DHCP option 42 (NTP server). If WiFi connects first, the DHCP NTP override is missed.

### Phase 4: Application Services
```
13. setupFSexplorer()     — Register filesystem HTTP routes
14. startWebserver()      — HTTP server on port 80
15. startWebSocket()      — WebSocket server on port 81
16. startMQTT()           — MQTT client (after webserver)
```
**Critical dependency:** MQTT must start after the webserver. The MQTT state machine assumes HTTP is available for health reporting and Web UI feedback. Starting MQTT first can cause connection attempts before the HTTP status endpoint exists.

### Phase 5: OTGW Hardware & Peripherals
```
17. initWatchDog()        — Enable I2C hardware watchdog
18. resetOTGW()           — Reset PIC controller via GPIO
19. startOTGWstream()     — TCP bridge on port 25238
20. initOutputs()         — GPIO output pins
21. WatchDogEnabled(1)    — Turn on watchdog feeding
22. sendOTGWbootcmd()     — Send configured boot commands to PIC
23. initS0Count()         — S0 pulse counter ISR
24. initSensors()         — Dallas DS18B20 temperature sensors
```
**Critical dependencies:**
- `resetOTGW()` before `startOTGWstream()`: The PIC must be in a known state before the serial bridge starts forwarding data to network clients.
- `sendOTGWbootcmd()` after `WatchDogEnabled(1)`: Boot commands go through the command queue which requires the watchdog to be active for normal operation.
- `initSensors()` last: Dallas sensors use MQTT for auto-discovery, so MQTT must be connected first.

## Alternatives Considered

### Alternative 1: Dependency Graph with Automatic Ordering
**Pros:**
- Self-documenting dependencies
- Automatically resolves ordering
- Cannot accidentally violate ordering

**Cons:**
- Significant code complexity (dependency resolver)
- Overkill for ~20 initialization steps
- Hard to debug on ESP8266 (limited debugging tools)
- Memory overhead for dependency graph

**Why not chosen:** The linear sequence is simple, tested, and has only a few critical ordering constraints. A dependency graph adds complexity without proportional benefit.

### Alternative 2: Parallel Initialization
**Pros:**
- Faster boot time
- Better resource utilization

**Cons:**
- ESP8266 is single-core — no true parallelism
- Cooperative multitasking during init is fragile
- Race conditions between services
- Much harder to debug

**Why not chosen:** ESP8266 has a single core. Parallel init would require cooperative yielding during setup, which conflicts with blocking operations like WiFi connect.

### Alternative 3: Lazy Initialization (Init on First Use)
**Pros:**
- Faster initial boot
- Only init what's needed
- Simpler setup()

**Cons:**
- Unpredictable initialization timing
- First request to a service is slow
- Hard to debug initialization failures
- Race conditions when multiple services first-use simultaneously

**Why not chosen:** Predictable boot behavior is more important than boot speed for a home heating controller. Users expect all services to be available after reboot.

## Consequences

### Positive
- **Predictable boot:** Every boot follows the same sequence
- **Debuggable:** Telnet available from phase 3 onwards for monitoring
- **Reliable:** Dependencies are always satisfied before dependent services start
- **Visual feedback:** LED blinks indicate boot progress
- **Fail-safe:** Watchdog disabled during potentially slow WiFi connect

### Negative
- **Boot time:** Sequential initialization takes 5-30 seconds depending on WiFi
  - Accepted: Boot speed is not critical for a heating controller
- **Fragile ordering:** Moving a line in setup() can break the system
  - Mitigation: This ADR documents the ordering rationale
  - Mitigation: Code comments reference critical dependencies
- **No partial availability:** All-or-nothing — services unavailable until setup() completes
  - Accepted: Partial availability would be confusing for users

### Risks & Mitigation
- **WiFi timeout:** WiFi connect blocks for up to 240 seconds
  - **Mitigation:** Watchdog disabled during setup
  - **Mitigation:** WiFiManager captive portal provides fallback configuration
- **PIC not responding:** detectPIC() fails on first boot
  - **Mitigation:** 60-second retry timer in main loop continues probing
  - **Mitigation:** Web UI and MQTT still functional without PIC
- **Settings corruption:** readSettings() fails
  - **Mitigation:** Default values used when settings file missing/corrupt
  - **Mitigation:** Factory reset via filesystem format

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (main loop timer design, post-boot)
- ADR-010: Multiple Concurrent Network Services (port allocation for services started here)
- ADR-011: External Hardware Watchdog (watchdog enable/disable during boot)
- ADR-015: NTP and AceTime for Time Management (NTP-before-WiFi dependency)
- ADR-017: WiFiManager for Initial Configuration (WiFi connect during boot)
- ADR-031: Two-Microcontroller Coordination (PIC detect/reset during boot)

## References
- Implementation: `OTGW-firmware.ino` setup() function
- ESP8266 DHCP NTP override: ESP8266 Arduino Core `configTime()` documentation
- WiFiManager timeout: `startWiFi()` in `networkStuff.h`
- Watchdog control: `initWatchDog()` / `WatchDogEnabled()` in `OTGW-firmware.ino`
