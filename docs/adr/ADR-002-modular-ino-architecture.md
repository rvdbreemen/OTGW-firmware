# ADR-002: Modular .ino File Architecture

**Status:** Accepted  
**Date:** 2018-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware needed to manage multiple complex subsystems (MQTT, REST API, WebSocket, file system, settings, sensors, etc.) within a single Arduino sketch. The codebase was growing beyond what could be reasonably maintained in a single monolithic `.ino` file.

Requirements:
- Organize code by functional domain for maintainability
- Allow multiple developers to work on different features simultaneously
- Keep Arduino IDE compatibility while supporting modern build tools
- Enable selective compilation for debugging
- Maintain global state access across modules

## Decision

Adopt a **modular .ino file architecture** where the firmware is split into 14+ separate `.ino` files, each responsible for a specific functional domain.

**File organization:**
```
OTGW-firmware.ino          # Entry point, setup(), loop(), task scheduling
OTGW-Core.ino             # OpenTherm protocol implementation
MQTTstuff.ino             # MQTT client and Home Assistant integration
restAPI.ino               # HTTP REST API endpoints
webSocketStuff.ino        # WebSocket server for live streaming
FSexplorer.ino            # File system browser and management
jsonStuff.ino             # JSON serialization helpers
settingStuff.ino          # Configuration persistence
handleDebug.ino           # Telnet debug interface
networkStuff.h            # WiFi, NTP, MDNS (header-only)
helperStuff.ino           # Utility functions
versionStuff.ino          # Version management and Git info
s0PulseCount.ino          # S0 pulse counter logic
sensors_ext.ino           # Dallas temperature sensors
outputs_ext.ino           # GPIO output control
```

**Build approach:**
- Arduino build system automatically concatenates all `.ino` files
- Files compiled in alphabetical order
- All files share the same global scope (C++ translation unit)
- Header files (`.h`) provide declarations and inline functions

## Alternatives Considered

### Alternative 1: Monolithic Single .ino File
**Pros:**
- Simplest build configuration
- No file organization overhead
- Clear compile order

**Cons:**
- Unmanageable for large codebases (10,000+ lines)
- Difficult to navigate and maintain
- Merge conflicts when multiple developers work simultaneously
- Hard to debug specific subsystems
- Poor code organization

**Why not chosen:** Not scalable. The firmware has grown to 15,000+ lines of code across all modules.

### Alternative 2: C++ Class-Based Library Structure
**Pros:**
- True C++ modularity with namespaces
- Better encapsulation
- Reusable across projects
- Standard C++ project structure

**Cons:**
- Breaks Arduino IDE compatibility
- Requires significant refactoring of existing code
- More complex for Arduino community developers
- Global state management becomes more complex
- Loses Arduino framework convenience patterns

**Why not chosen:** Arduino ecosystem compatibility is important for community contributions. The `.ino` pattern is familiar to Arduino developers.

### Alternative 3: PlatformIO with src/ Directory
**Pros:**
- Modern C++ project structure
- Better dependency management
- IDE-agnostic
- Professional tooling

**Cons:**
- Requires all contributors to use PlatformIO
- Arduino IDE users excluded
- Migration effort for existing codebase
- Breaks familiar Arduino patterns

**Why not chosen:** Maintaining Arduino IDE compatibility was a priority for the community.

### Alternative 4: Header-Only Library Pattern
**Pros:**
- No link-time overhead
- Template-friendly
- Easy to include

**Cons:**
- Increases compile time significantly
- Template bloat with limited RAM
- Debug symbols become large
- Not suitable for large implementations

**Why not chosen:** Compile-time overhead and RAM constraints make this impractical for ESP8266.

## Consequences

### Positive
- **Maintainability:** Each file has clear responsibility (~200-800 lines each)
- **Collaboration:** Developers can work on different files with minimal conflicts
- **Navigation:** Easy to locate functionality by domain
- **Debugging:** Can focus on specific subsystems
- **Arduino IDE compatible:** Works with both Arduino IDE and arduino-cli
- **Selective compilation:** Can comment out modules for testing
- **Clear dependencies:** Header files make dependencies explicit

### Negative
- **Global scope pollution:** All `.ino` files share the same namespace
  - Mitigation: Careful naming conventions, prefix module-specific globals
- **Compile order dependencies:** Alphabetical order can cause forward declaration issues
  - Mitigation: Use header files for declarations, define order with `#include` in main file
- **Hidden dependencies:** Function calls between modules not explicitly visible
  - Mitigation: Document cross-module dependencies in file headers
- **No encapsulation:** Any function can call any other function
  - Accepted: Trade-off for Arduino compatibility and simplicity

### Risks & Mitigation
- **Name collisions:** Multiple modules might define similar functions
  - **Mitigation:** Prefix module-specific functions (e.g., `MQTT_`, `REST_`, `WS_`)
- **Circular dependencies:** Modules calling each other can create cycles
  - **Mitigation:** Keep data flow unidirectional where possible, use global state as mediator
- **Build order issues:** Functions called before declaration
  - **Mitigation:** Forward declarations in header files

## Implementation Notes

**Naming conventions:**
- Main file (`OTGW-firmware.ino`) contains `setup()` and `loop()`
- Module files named by domain: `<feature>Stuff.ino` or `<feature>_ext.ino`
- Header files provide shared definitions: `.h` suffix
- Global variables prefixed by module (e.g., `settingHostname`, `mqttClient`)

**Module communication patterns:**
1. **Global state:** Shared variables in `OTGW-firmware.h`
2. **Event callbacks:** Functions called from main loop
3. **Helper functions:** Exported via module-specific functions
4. **Data structures:** Defined in header files (e.g., `OTGW-Core.h`)

**File size guidelines:**
- Keep modules under 1,000 lines
- Extract large feature sets into separate files
- Use helper functions to avoid duplication

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (requires modular organization)
- ADR-008: LittleFS for Configuration Persistence (settings module)

## References
- Arduino build process: https://arduino.github.io/arduino-cli/latest/sketch-build-process/
- File organization: Repository root directory structure
- Build system: `Makefile`, `build.py`
- Coding conventions: `.github/copilot-instructions.md`
