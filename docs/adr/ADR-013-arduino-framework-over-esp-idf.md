# ADR-013: Arduino Framework Over ESP-IDF

**Status:** Accepted  
**Date:** 2016-01-01 (Initial development)  
**Updated:** 2026-01-28 (Documentation)

## Context

The ESP8266 can be programmed using two main frameworks:
1. **Arduino framework:** High-level C++ library built on top of ESP8266 SDK
2. **ESP8266 RTOS SDK (ESP-IDF):** Low-level SDK from Espressif

The choice of framework affects:
- Development velocity
- Community support
- Library availability
- Learning curve
- Code portability
- Performance characteristics

**Project requirements:**
- Rapid development and iteration
- Community contributions
- Rich ecosystem of libraries (WiFi, HTTP, MQTT, JSON, etc.)
- Good documentation and examples
- Accessible to hobbyist developers

## Decision

**Use the Arduino framework (ESP8266 Arduino Core) for all firmware development.**

**Key characteristics:**
- **Language:** Arduino C/C++ (.ino files)
- **Core version:** ESP8266 Arduino Core 2.7.4+
- **Build tool:** arduino-cli (command-line compilation)
- **IDE support:** Arduino IDE, VS Code, PlatformIO (all supported)
- **Library ecosystem:** Full access to Arduino libraries

**Framework features used:**
- WiFi stack (ESP8266WiFi library)
- Web server (ESP8266WebServer)
- File system (LittleFS)
- OTA updates
- I2C (Wire library)
- Time (configTime, AceTime)

## Alternatives Considered

### Alternative 1: ESP8266 RTOS SDK (ESP-IDF)
**Pros:**
- Direct access to ESP8266 hardware
- More control over WiFi stack
- Lower-level optimization possible
- Official Espressif SDK
- Better for complex multithreading

**Cons:**
- Steeper learning curve
- Smaller community
- Fewer high-level libraries
- More verbose code
- Complex build system
- Harder for hobbyists to contribute

**Why not chosen:** Development velocity and community accessibility are more important than low-level control for this project.

### Alternative 2: MicroPython
**Pros:**
- Python language (easier for beginners)
- Interactive REPL
- Rapid prototyping
- Dynamic typing

**Cons:**
- Performance overhead (interpreter)
- Memory overhead (too large for ESP8266's 40KB RAM)
- Limited library support
- Not suitable for real-time requirements
- Flash size limitations

**Why not chosen:** Memory constraints make Python unsuitable. ESP8266 has too little RAM for a Python interpreter plus application code.

### Alternative 3: NodeMCU (Lua)
**Pros:**
- Scripting language
- Fast iteration
- Small footprint

**Cons:**
- Niche community
- Limited libraries
- Performance issues
- Debugging difficulty
- Less mature than Arduino ecosystem

**Why not chosen:** Arduino ecosystem is much larger and better supported.

### Alternative 4: Bare Metal C
**Pros:**
- Maximum performance
- Full control
- Smallest code size

**Cons:**
- Must implement everything from scratch
- WiFi stack complexity
- No library ecosystem
- Very high development time
- Difficult to maintain
- Hard for community contributions

**Why not chosen:** Reinventing WiFi/HTTP/MQTT stacks is not feasible. Library ecosystem is essential.

## Consequences

### Positive
- **Fast development:** High-level APIs accelerate development
- **Rich ecosystem:** 7,000+ Arduino libraries available
- **Community support:** Large Arduino community, many examples
- **Easy onboarding:** Arduino is familiar to many developers
- **Code portability:** Code can work on other Arduino-compatible boards
- **IDE support:** Works with Arduino IDE, VS Code, PlatformIO
- **Library quality:** Well-tested libraries (WiFiManager, PubSubClient, ArduinoJson)

### Negative
- **Performance overhead:** Arduino abstraction layer adds some overhead
  - Accepted: Not performance-critical for network bridge application
- **Memory overhead:** Arduino core uses ~40KB RAM (out of 80KB total)
  - Mitigation: Careful memory management required (see ADR-004, ADR-009)
- **Less control:** Cannot tweak low-level WiFi parameters
  - Accepted: Default WiFi behavior is sufficient
- **Framework limitations:** Must work within Arduino patterns
  - Examples: Global scope for .ino files, setup/loop structure
- **Version dependencies:** Tied to ESP8266 Arduino Core release cycle
  - Accepted: Core is actively maintained

### Risks & Mitigation
- **Arduino core bugs:** Framework bugs affect firmware
  - **Mitigation:** Pin to stable core version (2.7.4), test before upgrading
- **Library compatibility:** Some libraries may not work with ESP8266
  - **Mitigation:** Careful library selection, test on hardware
- **Memory constraints:** Arduino overhead leaves less RAM for application
  - **Mitigation:** PROGMEM, static buffers, heap monitoring (ADR-004, ADR-009)
- **Breaking changes:** Future Arduino core updates may break code
  - **Mitigation:** Pin to working version, test upgrades thoroughly

## Arduino Ecosystem Benefits

**Libraries used (examples):**
```cpp
#include <ESP8266WiFi.h>          // WiFi networking
#include <ESP8266WebServer.h>     // HTTP server
#include <PubSubClient.h>         // MQTT client
#include <ArduinoJson.h>          // JSON parsing
#include <LittleFS.h>             // File system
#include <Wire.h>                 // I2C communication
#include <OneWire.h>              // Dallas sensors
#include <WebSocketsServer.h>     // WebSocket server
#include <TelnetStream.h>         // Debug telnet
```

**Without Arduino framework, each of these would require:**
- Implementing from scratch OR
- Finding ESP-IDF compatible library OR
- Porting from other platforms

**Development time saved:** Estimated 6-12 months of development

## Arduino Patterns Used

**Sketch structure:**
```cpp
void setup() {
  // Initialization code runs once
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  httpServer.begin();
}

void loop() {
  // Runs continuously
  httpServer.handleClient();
  mqttClient.loop();
  // Other tasks...
}
```

**Global scope (all .ino files share namespace):**
```cpp
// OTGW-firmware.h
extern ESP8266WebServer httpServer;
extern PubSubClient mqttClient;

// restAPI.ino - can access httpServer directly
void handleAPI() {
  httpServer.send(200, "text/plain", "OK");
}
```

**Arduino functions available:**
```cpp
millis()           // Milliseconds since boot
micros()           // Microseconds since boot
delay()            // Blocking delay (avoided - see ADR-007)
pinMode()          // GPIO configuration
digitalWrite()     // GPIO output
analogRead()       // ADC reading
```

## Build System Integration

**arduino-cli compilation:**
```bash
arduino-cli compile \
  --fqbn esp8266:esp8266:nodemcuv2 \
  --build-property "compiler.cpp.extra_flags=-DUSE_LITTLEFS" \
  OTGW-firmware.ino
```

**Automatic dependency resolution:**
- Arduino library manager downloads dependencies
- Specified in `Makefile` or `build.py`
- No manual library installation needed

## Related Decisions
- ADR-001: ESP8266 Platform Selection (platform + framework)
- ADR-002: Modular .ino File Architecture (Arduino sketch pattern)
- ADR-007: Timer-Based Task Scheduling (setup/loop pattern)

## References
- ESP8266 Arduino Core: https://github.com/esp8266/Arduino
- Arduino language reference: https://www.arduino.cc/reference/en/
- Build system: `Makefile`, `build.py`
- ESP-IDF comparison: https://docs.espressif.com/projects/esp8266-rtos-sdk/
- Library list: Repository README.md
