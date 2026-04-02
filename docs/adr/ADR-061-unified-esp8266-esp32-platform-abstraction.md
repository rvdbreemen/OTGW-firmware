# ADR-061: Unified ESP8266/ESP32 Platform Abstraction

**Status:** Accepted
**Date:** 2026-04-02
**Supersedes:** ADR-001 (ESP8266 Platform Selection)

## Context

The NodoShop OpenTherm Gateway is expanding to support an ESP32-based board variant alongside the existing ESP8266 (Wemos D1 mini) hardware. The ESP32 variant offers more RAM (~320KB usable), a faster dual-core CPU, and a more modern SDK, while the ESP8266 remains the production platform for existing deployments.

The firmware codebase (~15 source files, ~5000 lines of application code) contained direct calls to ESP8266-specific APIs throughout: `ESP.getFreeHeap()`, `WiFi.hostname()`, `LittleFS.info()`, `Dir` iteration, `ESP.rtcUserMemoryRead/Write()`, `wifi_station_dhcpc_stop/start()`, and others. These APIs differ between platforms in function names, signatures, return types, or behavior.

### Key constraints:
- Arduino build model: all `.ino` files are concatenated into a single translation unit
- ESP8266 has only ~40KB usable RAM and a 4KB CONT stack; abstractions must be zero-cost
- The ESP32 board hardware is not yet finalized; pin definitions are provisional
- Both platforms must produce correct, independently flashable firmware from the same source tree
- The existing ESP8266 build must not regress in size, performance, or behavior

## Decision

Introduce a **compile-time platform abstraction layer** using three headers:

1. **`platform.h`** - Main entry point; selects the correct platform header via `#ifdef ESP8266`/`ESP32`. Also contains cross-platform utilities (e.g., `PlatformDir` class for directory iteration).
2. **`platform_esp8266.h`** - ESP8266-specific inline shim functions (~35 functions).
3. **`platform_esp32.h`** - ESP32-specific inline shim functions (~35 functions).

Additionally:

4. **`boards.h`** - Board-specific pin definitions selected by `BOARD_NODOSHOP_ESP8266` / `BOARD_NODOSHOP_ESP32` build flags (with auto-detect fallback).
5. **`OTGW-ModUpdateServer-esp32.h`** - ESP32 OTA update server, functionally equivalent to the ESP8266 template-based implementation.

### Design principles:
- **All platform differences are localized** to `platform.h`, `platform_esp8266.h`, `platform_esp32.h`, and `boards.h`. Application code never uses `#ifdef ESP8266`/`ESP32` directly.
- **Inline functions** ensure zero overhead on both platforms (the Arduino single-TU model means each function has exactly one call site per binary).
- **Naming convention:** `platform*()` prefix for all shim functions (e.g., `platformFreeHeap()`, `platformSetHostname()`, `platformRestart()`).
- **Type aliases:** `OTGWWebServer` (maps to `ESP8266WebServer` or `WebServer`), `OTGWUpdateServer`.
- **Feature flags:** `HAS_LLMNR`, `MDNS_NEEDS_UPDATE` control platform-specific code paths via `#if` rather than `#ifdef ESP8266`.
- **Legacy pin aliases** (`LED1`, `BUTTON`, `PICRST`, etc.) are preserved as `#define` macros mapping to the new `PIN_*` constants from `boards.h`.

### Build system:
- **`build.py`** (arduino-cli): extended with `--target esp8266|esp32|all` and target definitions table.
- **`platformio.ini`**: new dual-environment configuration (`[env:esp8266]`, `[env:esp32]`).
- Both build systems produce target-suffixed artifacts (e.g., `OTGW-firmware-esp8266-1.4.0.ino.bin`).

## Alternatives Considered

### Alternative 1: Separate firmware branches per platform
**Pros:** Simple, no abstraction overhead, each platform can diverge freely.
**Cons:** Feature parity requires cherry-picking across branches; bug fixes must be applied twice; divergence is inevitable.
**Why not chosen:** Maintenance burden grows linearly with the number of platforms.

### Alternative 2: Runtime polymorphism (virtual functions / function pointers)
**Pros:** Clean OOP design; easy to add platforms.
**Cons:** Virtual call overhead on every platform function call; vtable consumes RAM; ESP8266 with 40KB RAM cannot afford the indirection; no benefit since the platform is known at compile time.
**Why not chosen:** Incompatible with ESP8266 resource constraints.

### Alternative 3: C preprocessor macros for all platform differences
**Pros:** Zero overhead; familiar pattern.
**Cons:** No type safety; poor IDE support; debugging is harder; macro hygiene issues in a large codebase.
**Why not chosen:** Inline functions provide the same zero overhead with better type safety and debuggability.

## Consequences

### Benefits
- Single source tree produces firmware for both platforms
- Platform differences are isolated to 4 header files; application code is platform-agnostic
- Zero runtime overhead from the abstraction (inline functions, compile-time selection)
- Adding a third platform requires only a new `platform_*.h` and `boards.h` section
- Legacy ESP8266 builds are unaffected (identical compiled output)

### Trade-offs
- ~700 lines of new platform header code to maintain
- ESP32 RTC user memory is emulated via NVS (flash-backed, not SRAM); callers must be aware of write frequency limits
- ESP32 `PlatformDir::fileName()` required normalization (ESP32 returns full path, ESP8266 returns basename)
- Some ESP32 features lack full parity (serial error detection returns false; no OTA rollback partition with `huge_app` scheme)

### Risks
- ESP32 board pin definitions are provisional until hardware is finalized
- Dual build system (arduino-cli + PlatformIO) requires keeping both in sync

## Related

- ADR-001: ESP8266 Platform Selection (superseded by this ADR)
- ADR-004: Static Buffer Allocation (applies to both platforms)
- ADR-044: Header/Implementation Separation
- ADR-051: Dual-Struct Settings/State Architecture
- Files: `platform.h`, `platform_esp8266.h`, `platform_esp32.h`, `boards.h`
- Files: `OTGW-ModUpdateServer-esp32.h`, `platformio.ini`, `build.py`
