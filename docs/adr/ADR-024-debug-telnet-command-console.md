# ADR-024: Debug Telnet Command Console

**Status:** Accepted  
**Date:** 2018-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

During development and troubleshooting, developers need:
- Real-time visibility into firmware operation
- Ability to trigger actions without Web UI
- Hardware-level testing (GPIO, LED, relay)
- Debug output without serial cable
- Runtime configuration inspection
- Service state monitoring (WiFi, MQTT, sensors)

**Constraints:**
- Serial port reserved for PIC communication
- Cannot use Serial.print() after OTGW initialization
- Need remote access (no physical connection)
- Must not interfere with normal operation

**Debug requirements:**
- View all debug messages
- Toggle debug levels per module
- Execute diagnostic commands
- Test hardware (LEDs, GPIO)
- Force service reconnections
- Trigger MQTT discovery

## Decision

**Implement character-driven telnet command console using TelnetStream library with menu-based command system.**

**Architecture:**
- **Protocol:** Raw telnet (port 23)
- **Library:** TelnetStream (wraps WiFiServer)
- **Command style:** Single-character commands (like Unix utilities)
- **Menu:** 'h' displays help menu
- **Concurrent clients:** 1 client at a time
- **Debug macros:** DebugTln(), DebugTf() redirect to telnet
- **Integration:** Called from main loop, non-blocking

**Command categories:**
1. **System info:** Status, uptime, heap, WiFi
2. **Debug toggles:** Enable/disable per-module logging
3. **Hardware tests:** Blink LED, control GPIO
4. **Service control:** Reconnect WiFi/MQTT, force updates
5. **Diagnostics:** Query PIC version, sensor readings

## Alternatives Considered

### Alternative 1: HTTP-Based Debug Console
**Pros:**
- Browser-accessible
- Rich UI possible
- Standard protocol

**Cons:**
- Stateless (cannot stream debug output)
- Requires polling (inefficient)
- More complex to implement
- HTTP overhead for simple commands

**Why not chosen:** Telnet provides real-time streaming output. HTTP requires polling.

### Alternative 2: WebSocket Debug Console
**Pros:**
- Real-time bidirectional
- Browser-accessible
- Modern protocol

**Cons:**
- Port 81 already used for OT messages
- Adds complexity
- Browser required
- Mixing debug and OT data problematic

**Why not chosen:** Telnet is simpler and doesn't conflict with existing WebSocket use.

### Alternative 3: Serial Console (USB)
**Pros:**
- Direct connection
- No network dependency
- Standard Arduino pattern

**Cons:**
- **Serial port used for PIC communication**
- Requires physical access
- Cannot debug deployed devices
- Cable required

**Why not chosen:** Serial port is reserved for OpenTherm Gateway PIC. Cannot use for debug.

### Alternative 4: MQTT Debug Topic
**Pros:**
- Uses existing MQTT connection
- Can log to Home Assistant
- No additional port

**Cons:**
- Cannot send commands (subscribe delays)
- Broker dependency
- Not interactive
- Message rate limits

**Why not chosen:** Need interactive command execution, not just logging.

### Alternative 5: UDP Syslog
**Pros:**
- Standard logging protocol
- Low overhead
- Centralized logging

**Cons:**
- UDP unreliable (messages lost)
- Cannot send commands
- Requires syslog server
- Not interactive

**Why not chosen:** Need interactive console, not just one-way logging.

## Consequences

### Positive
- **Real-time output:** Debug messages appear immediately
- **Remote access:** No USB cable needed
- **Interactive:** Execute commands, see results
- **Simple protocol:** Any telnet client works
- **Hardware testing:** Direct GPIO control for diagnostics
- **Module-specific debug:** Toggle logging per subsystem
- **No Serial conflicts:** Leaves Serial available for PIC
- **Standard port:** Port 23 is well-known telnet port

### Negative
- **No authentication:** Anyone on network can connect
  - Accepted: Local network trust model (see ADR-003)
- **Plain text:** All traffic unencrypted
  - Accepted: Debug data not sensitive, local network only
- **Single client:** Only one connection at a time
  - Accepted: Debug console is single-user tool
- **Character-based:** No mouse, no cursor control
  - Accepted: Commands are simple single characters

### Risks & Mitigation
- **Accidental commands:** User presses wrong key
  - **Mitigation:** Destructive commands require confirmation
  - **Mitigation:** Commands are single characters (visible in menu)
- **Debug flood:** Too much output crashes connection
  - **Mitigation:** Debug toggles allow disabling verbose modules
  - **Mitigation:** TelnetStream buffers output
- **Connection stuck:** Client disconnects without cleanup
  - **Mitigation:** TelnetStream detects disconnection
  - **Mitigation:** Timeout after inactivity
- **Port conflict:** Port 23 used by other service
  - **Extremely rare:** Standard telnet port, unlikely conflict
  - **Mitigation:** Can disable via compile flag if needed

## Implementation Details

**TelnetStream initialization:**
```cpp
#include <TelnetStream.h>

void setup() {
  // Start telnet server on port 23
  TelnetStream.begin();
  
  // Set callback for connection events
  TelnetStream.setWelcomeMsg(
    "OTGW Debug Console\r\n"
    "Press 'h' for help\r\n"
  );
  
  DebugTln(F("Telnet console ready on port 23"));
}
```

**Command handler:**
```cpp
void handleDebug() {
  // Check if data available
  if (TelnetStream.available() > 0) {
    char cmd = TelnetStream.read();
    
    switch (cmd) {
      case 'h':
        // Help menu
        DebugTln(F("\r\n=== OTGW Debug Console ==="));
        DebugTln(F("System:"));
        DebugTln(F("  s - Show status"));
        DebugTln(F("  r - Reconnect services"));
        DebugTln(F("  R - Reboot device"));
        DebugTln(F("Hardware:"));
        DebugTln(F("  b - Blink LED"));
        DebugTln(F("  i - Initialize relay"));
        DebugTln(F("  u/o - GPIO up/on, down/off"));
        DebugTln(F("Debug Toggles:"));
        DebugTln(F("  1 - Toggle OT message debug"));
        DebugTln(F("  2 - Toggle REST API debug"));
        DebugTln(F("  3 - Toggle MQTT debug"));
        DebugTln(F("  4 - Toggle Sensor debug"));
        DebugTln(F("MQTT:"));
        DebugTln(F("  m/F - Send MQTT discovery"));
        DebugTln(F("PIC:"));
        DebugTln(F("  a - Query PIC version"));
        DebugTln(F("Settings:"));
        DebugTln(F("  q - Force read settings"));
        break;
        
      case 's':
        // Show status
        showStatus();
        break;
        
      case 'b':
        // Blink LED test
        DebugTln(F("Blinking LED..."));
        blinkLED(LED1, 5, 200);
        break;
        
      case '1':
        // Toggle OT message debug
        debugOTmsg = !debugOTmsg;
        DebugTf(PSTR("OT message debug: %s\r\n"), 
                debugOTmsg ? "ON" : "OFF");
        break;
        
      case 'm':
      case 'F':
        // Send MQTT discovery
        DebugTln(F("Sending MQTT discovery..."));
        sendMQTTDiscovery();
        break;
        
      case 'a':
        // Query PIC version
        DebugTln(F("Querying PIC firmware version..."));
        addOTWGcmdtoqueue("PR=A");
        break;
        
      case 'r':
        // Reconnect services
        DebugTln(F("Reconnecting WiFi and MQTT..."));
        reconnectWiFi();
        reconnectMQTT();
        break;
        
      case 'R':
        // Reboot
        DebugTln(F("Rebooting in 2 seconds..."));
        delay(2000);
        ESP.restart();
        break;
        
      case '\r':
      case '\n':
        // Ignore newlines
        break;
        
      default:
        DebugTf(PSTR("Unknown command: '%c' (press 'h' for help)\r\n"), cmd);
        break;
    }
  }
}
```

**Status display:**
```cpp
void showStatus() {
  DebugTln(F("\r\n=== System Status ==="));
  
  // Uptime
  unsigned long uptime = millis() / 1000;
  DebugTf(PSTR("Uptime: %lu seconds\r\n"), uptime);
  
  // Heap
  DebugTf(PSTR("Free heap: %u bytes\r\n"), ESP.getFreeHeap());
  
  // WiFi
  DebugTf(PSTR("WiFi: %s\r\n"), WiFi.isConnected() ? "Connected" : "Disconnected");
  if (WiFi.isConnected()) {
    DebugTf(PSTR("  SSID: %s\r\n"), WiFi.SSID().c_str());
    DebugTf(PSTR("  IP: %s\r\n"), WiFi.localIP().toString().c_str());
    DebugTf(PSTR("  RSSI: %d dBm\r\n"), WiFi.RSSI());
  }
  
  // MQTT
  DebugTf(PSTR("MQTT: %s\r\n"), mqttClient.connected() ? "Connected" : "Disconnected");
  if (mqttClient.connected()) {
    DebugTf(PSTR("  Broker: %s:%d\r\n"), 
            settingMqttBroker, settingMqttPort);
  }
  
  // OpenTherm
  DebugTf(PSTR("Boiler temp: %.1f °C\r\n"), OTdata.Tboiler);
  DebugTf(PSTR("Return temp: %.1f °C\r\n"), OTdata.Tret);
  DebugTf(PSTR("CH active: %s\r\n"), OTdata.CHmode ? "Yes" : "No");
  DebugTf(PSTR("DHW active: %s\r\n"), OTdata.DHWmode ? "Yes" : "No");
  DebugTf(PSTR("Flame: %s\r\n"), OTdata.flame ? "On" : "Off");
  
  // Sensors
  if (nrSensors > 0) {
    DebugTf(PSTR("Sensors: %d detected\r\n"), nrSensors);
    for (int i = 0; i < nrSensors; i++) {
      DebugTf(PSTR("  Sensor %d: %.2f °C\r\n"), i, sensorVal[i]);
    }
  }
  
  // Debug flags
  DebugTln(F("\r\n=== Debug Flags ==="));
  DebugTf(PSTR("OT messages: %s\r\n"), debugOTmsg ? "ON" : "OFF");
  DebugTf(PSTR("REST API: %s\r\n"), debugRestAPI ? "ON" : "OFF");
  DebugTf(PSTR("MQTT: %s\r\n"), debugMQTT ? "ON" : "OFF");
  DebugTf(PSTR("Sensors: %s\r\n"), debugSensors ? "ON" : "OFF");
}
```

**Debug macros (Debug.h):**
```cpp
// Telnet debug output
#define DebugTln(x)       TelnetStream.println(x)
#define DebugTf(...)      TelnetStream.printf(__VA_ARGS__)
#define Debugln(x)        TelnetStream.println(x)
#define Debugf(...)       TelnetStream.printf(__VA_ARGS__)

// Setup phase (before telnet active)
#define SetupDebugTln(x)  Serial.println(x)
#define SetupDebugTf(...) Serial.printf(__VA_ARGS__)
```

**Main loop integration:**
```cpp
void loop() {
  feedWatchDog();
  
  // Handle telnet console
  handleDebug();
  
  // Other tasks...
  handleOTGW();
  handleTimers();
  // ...
}
```

## Hardware Test Commands

**Blink LED:**
```cpp
void blinkLED(int led, int count, int delayMs) {
  for (int i = 0; i < count; i++) {
    setLed(led, ON);
    delay(delayMs);
    setLed(led, OFF);
    delay(delayMs);
  }
}
```

**GPIO control:**
```cpp
case 'u':
  // GPIO up/on
  DebugTln(F("Set GPIO output HIGH"));
  if (settingGPIOOUTPUTSpin > 0) {
    digitalWrite(settingGPIOOUTPUTSpin, HIGH);
  }
  break;
  
case 'o':
  // GPIO down/off
  DebugTln(F("Set GPIO output LOW"));
  if (settingGPIOOUTPUTSpin > 0) {
    digitalWrite(settingGPIOOUTPUTSpin, LOW);
  }
  break;
```

## Debug Levels

**Global flags:**
```cpp
bool debugOTmsg = false;     // OpenTherm message logging
bool debugRestAPI = false;   // REST API request logging
bool debugMQTT = false;      // MQTT publish/subscribe logging
bool debugSensors = false;   // Sensor reading logging
```

**Conditional logging:**
```cpp
if (debugOTmsg) {
  DebugTf(PSTR("OT >> %s\r\n"), otMessage);
}

if (debugMQTT) {
  DebugTf(PSTR("MQTT publish: %s = %s\r\n"), topic, payload);
}
```

## Usage Examples

**Connect:**
```bash
$ telnet 192.168.1.100 23
Connected to OTGW
OTGW Debug Console
Press 'h' for help
```

**Get status:**
```
> s
=== System Status ===
Uptime: 12345 seconds
Free heap: 23456 bytes
WiFi: Connected
  SSID: MyNetwork
  IP: 192.168.1.100
  RSSI: -45 dBm
MQTT: Connected
  Broker: 192.168.1.10:1883
Boiler temp: 65.2 °C
...
```

**Enable OT debug:**
```
> 1
OT message debug: ON

OT >> T10100000
OT << B10100000
OT >> T00010000
...
```

**Trigger MQTT discovery:**
```
> m
Sending MQTT discovery...
Published: homeassistant/sensor/otgw_123456_boiler_temp/config
Published: homeassistant/sensor/otgw_123456_return_temp/config
...
```

## Security Model

**Assumptions (development/debug builds):**
- Device on trusted local network
- No malicious users on network
- Physical security of network

**Not protected against (when telnet debug console is enabled):**
- Eavesdropping (telnet is plain text)
- Unauthorized access (no authentication on raw TelnetStream)
- Command abuse within predefined command set (no per-command authorization)
- Heating disruption via relay control, MQTT commands, or device reboot
- Service disruption via reconnect commands or PIC command injection

**Security Risks:**
- **Powerful commands exposed:** `showStatus()`, service reconnects, MQTT discovery, GPIO/relay control, PIC command injection (`addOTWGcmdtoqueue("PR=A")`), device reboot
- **Network-wide access:** Any host on local network can connect on port 23
- **Browser exploitation:** Malicious web pages could potentially connect via telnet-capable clients
- **No audit logging:** Command execution not logged or tracked

**Accepted risks (development/debug builds only):**
- Debug output may contain sensitive info (WiFi password not shown)
- Commands can disrupt operation (reboot, reconnect)
- Anyone on the same network segment can execute debug commands
- Potential for heating control disruption or integration pivoting

**Production requirements:**
- **RECOMMENDED:** Default production firmware should disable the debug telnet console entirely, **OR**
- **ALTERNATIVE:** If enabled in production, it **MUST** be:
  - Gated behind strong authentication (password/token)
  - Configurable to use non-standard port (not port 23)
  - Clearly documented as a security risk in user documentation
  - Disabled by default with explicit opt-in required
- **CRITICAL:** Production builds **MUST NOT** ship with an unauthenticated, always-on telnet console on port 23

**Why acceptable for development:**
- Consistent with local network trust model (ADR-003) for development environments
- Debug console is a developer/diagnostic tool
- Clear separation between development/debug behavior and production security posture
- Can be disabled via compile-time flag or runtime configuration

## Related Decisions
- ADR-003: HTTP-Only Network Architecture (local network trust model)
- ADR-010: Multiple Concurrent Network Services (telnet port 23)
- ADR-007: Timer-Based Task Scheduling (handleDebug in main loop)

## References
- Implementation: `handleDebug.ino`
- Debug macros: `Debug.h`
- TelnetStream library: https://github.com/jandrassy/TelnetStream
- Telnet protocol: RFC 854
