# ADR-011: External Hardware Watchdog for Reliability

**Status:** Accepted  
**Date:** 2018-01-01 (Estimated - initial hardware design)  
**Updated:** 2026-01-28 (Documentation)

## Context

The ESP8266, like many embedded systems, can hang or crash due to:
- Software bugs (null pointer dereferences, infinite loops)
- WiFi stack issues
- Heap exhaustion
- Flash corruption
- Hardware glitches

For an always-on device controlling home heating, crashes are unacceptable. The device must recover automatically without user intervention.

**Requirements:**
- Detect when ESP8266 stops responding
- Automatically reset the device
- Work independently of ESP8266 firmware state
- No false positives (reset when device is actually working)
- Minimal power consumption

## Decision

**Use an external I2C hardware watchdog chip that must be fed regularly by the ESP8266, or it will force a hardware reset.**

**Implementation:**
- **Watchdog chip:** I2C device at address 0x26
- **Communication:** ESP8266 sends I2C commands via Wire library
- **Timeout:** ~3 seconds (if not fed, watchdog triggers reset)
- **GPIO pins:** GPIO5 (D1/SCL) and GPIO4 (D2/SDA) for I2C
- **Feeding interval:** Maximum every 100ms (rate-limited to reduce I2C traffic)
- **Feeding location:** Called from main `loop()` via timer-based mechanism
- **Independence:** Watchdog operates independently of ESP8266 software

**I2C Protocol Commands:**
- **Feed command (0xA5):** Reset watchdog timer, prevent reset
- **Enable/Disable (register 7):** 1 = armed to reset, 0 = turned off
- **Status query (register 17):** Read reset reason (bit 0 = watchdog reset)

**Watchdog feeding pattern:**
```cpp
void loop() {
  feedWatchDog();  // First thing in loop
  
  // All other tasks...
  // Loop must complete in <3 seconds
}
```

## Alternatives Considered

### Alternative 1: ESP8266 Internal Software Watchdog
**Pros:**
- No external hardware needed
- Built into ESP8266
- Zero cost

**Cons:**
- Can be disabled by buggy code
- May not trigger on some crash types
- Less reliable than hardware solution
- Same failure domain as main firmware

**Why not chosen:** Software watchdogs share the same failure domain. If the system is completely hung, software watchdog may not function.

### Alternative 2: No Watchdog (Manual Reset Required)
**Pros:**
- Simplest implementation
- No hardware cost
- No complexity

**Cons:**
- Requires manual intervention on hang
- Unacceptable for heating system
- Bad user experience
- System may be down for hours/days

**Why not chosen:** Manual reset is not acceptable for an always-on heating controller. Users may be away from home.

### Alternative 3: External GPIO Watchdog
**Pros:**
- Simple digital signal
- Low cost
- Easy to implement

**Cons:**
- Requires dedicated GPIO pin
- Less flexible than I2C
- Cannot configure timeout
- No status feedback

**Why not chosen:** I2C provides better flexibility and allows shared bus with other sensors.

### Alternative 4: Network-Based Watchdog (External Monitor)
**Pros:**
- Can monitor from Home Assistant
- No hardware changes needed
- Flexible monitoring rules

**Cons:**
- Requires network connectivity
- Cannot reset if network stack fails
- Adds external dependency
- Complex setup
- May not detect all failure modes

**Why not chosen:** If WiFi fails (common crash scenario), network watchdog cannot help. Hardware solution is more reliable.

## Consequences

### Positive
- **Automatic recovery:** Device resets itself on hang/crash
- **Always-on reliability:** No manual intervention needed
- **Independent operation:** Works even if ESP8266 completely frozen
- **User confidence:** Device will recover from most failures
- **Remote deployment:** Safe to deploy where physical access is limited
- **Proven reliability:** Hardware watchdogs are industry standard for critical systems

### Negative
- **Hardware cost:** Additional chip on board (~$0.50-1.00)
  - Accepted: Cost justified for reliability
- **GPIO usage:** Uses 2 GPIO pins for I2C
  - Mitigated: I2C bus can be shared with other sensors
- **Restart on any hang:** Even temporary hangs cause full reboot
  - Accepted: Full restart is safest recovery method
- **No crash analysis:** Watchdog reset loses crash state
  - Mitigated: Can disable watchdog feeding for debugging
- **Loop time constraint:** Main loop must complete in <3 seconds
  - Mitigation: All tasks designed to be non-blocking

### Risks & Mitigation
- **False positives:** Watchdog resets when device is actually working
  - **Mitigation:** 3-second timeout allows sufficient loop time
  - **Mitigation:** Non-blocking task design ensures quick loop completion
- **Bootloop:** If boot code fails, watchdog causes continuous resets
  - **Mitigation:** Boot code is minimal and well-tested
  - **Mitigation:** WiFi connection failure does NOT prevent watchdog feeding
- **I2C failure:** I2C bus failure could prevent watchdog feeding
  - **Accepted:** I2C failure is rare; usually indicates hardware problem
- **Configuration errors:** Wrong I2C address or pin configuration
  - **Mitigation:** Hardware design standardized, tested during manufacturing

## Implementation Details

### I2C Protocol Specification

**I2C Address:** 0x26 (EXT_WD_I2C_ADDRESS)

**Command 1: Feed Watchdog (Reset Timer)**
```cpp
Wire.beginTransmission(0x26);
Wire.write(0xA5);              // Feed command
Wire.endTransmission();
```
- **Purpose:** Reset watchdog timer to prevent reset
- **Frequency:** Called every 100ms maximum via rate-limiting timer
- **Effect:** Restarts 3-second countdown

**Command 2: Enable/Disable Watchdog**
```cpp
Wire.beginTransmission(0x26);
Wire.write(7);                 // Register 7: action register
Wire.write(stateWatchdog);     // 1 = armed, 0 = disabled
Wire.endTransmission();
```
- **Purpose:** Enable (1) or disable (0) watchdog functionality
- **Use cases:** 
  - Disable during OTA firmware updates (prevents timeout during flash)
  - Disable during WiFi reconnection attempts
  - Enable during normal operation

**Command 3: Read Reset Reason**
```cpp
// Set pointer to status register
Wire.beginTransmission(0x26);
Wire.write(0x83);              // Set pointer command
Wire.write(17);                // Register 17: status byte
Wire.endTransmission();

// Read status
Wire.requestFrom((uint8_t)0x26, (uint8_t)1);
byte status = Wire.read();
bool wasWatchdogReset = (status & 0x01);  // Bit 0 = WD reset
```
- **Purpose:** Determine if last reset was caused by watchdog
- **Usage:** Boot diagnostics, logging, reboot counter

### Rate-Limited Feeding

**Actual implementation (with timer):**
```cpp
void feedWatchDog() {
  // Feed the watchdog at most every 100ms to prevent hardware watchdog resets
  // during blocking operations while limiting I2C bus traffic
  DECLARE_TIMER_MS(timerWD, 100, SKIP_MISSED_TICKS);
  if DUE(timerWD) {
    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   // 0x26
    Wire.write(0xA5);                             // Feed command
    Wire.endTransmission();
  }
}
```
- **Rate limiting:** Prevents excessive I2C traffic
- **Timer-based:** Uses safeTimers.h timer macros
- **Interval:** 100ms maximum (30 feeds per second max, 1 feed per second sufficient)
- **Rollover safe:** DECLARE_TIMER_MS handles millis() rollover

### Macro for Emergency Feeding

**FEEDWATCHDOGNOW macro:**
```cpp
#define FEEDWATCHDOGNOW \
  Wire.beginTransmission(EXT_WD_I2C_ADDRESS); \
  Wire.write(0xA5); \
  Wire.endTransmission();
```
- **Purpose:** Immediate watchdog feed without rate limiting
- **Use cases:** Critical sections, OTA flash chunks, PIC firmware upgrade
- **Warning:** Bypasses rate limiter, use sparingly

**Main loop structure:**
```cpp
void loop() {
  // CRITICAL: Feed watchdog first
  feedWatchDog();
  
  // Then handle all tasks
  handleTimers();
  handleNetworkServices();
  handleOTGW();
  
  // Loop must complete in <3 seconds
  // All tasks are non-blocking
}
```

**Debug mode (disable watchdog):**
```cpp
#ifdef DEBUG_NO_WATCHDOG
  // Don't feed watchdog - for debugging crash scenarios
  // WARNING: Only use during development
#else
  feedWatchDog();  // Normal operation
#endif
```

**Boot sequence:**
```cpp
void setup() {
  // Initialize I2C for watchdog
  Wire.begin();  // SDA=GPIO4 (D2), SCL=GPIO5 (D1)
  
  // Enable watchdog for normal operation
  WatchDogEnabled(1);  // 1 = armed to reset
  
  // Feed watchdog during long setup operations
  feedWatchDog();
  
  // Initialize WiFi (can take >3 seconds)
  startWiFi();
  feedWatchDog();  // Feed again during boot
  
  // Check if last reset was caused by watchdog
  String resetReason = getWatchdogResetReason();
  if (resetReason.length() > 0) {
    DebugTln(resetReason);  // "Reset by External WD"
    // Log to reboot counter, send MQTT notification, etc.
  }
  
  // Rest of setup...
}
```

**Watchdog control during critical operations:**
```cpp
// OTA Firmware Update - disable watchdog during flash
void performOTAUpdate() {
  WatchDogEnabled(0);  // Disable: flash can take >3 seconds per chunk
  
  // Perform flash write (blocking operation)
  // But still feed periodically via FEEDWATCHDOGNOW in upload handler
  
  WatchDogEnabled(1);  // Re-enable after flash complete
}

// WiFi Reconnection - disable during connection attempts
void reconnectWiFi() {
  WatchDogEnabled(0);  // Disable: WiFi connection can take >3 seconds
  
  WiFi.reconnect();
  // Wait for connection...
  
  WatchDogEnabled(1);  // Re-enable after connection established
}
```

## Watchdog Behavior

**Normal operation:**
1. Loop executes (typically 10-50ms)
2. Watchdog fed
3. Watchdog timer resets to 3 seconds
4. Repeat

**Crash/hang scenario:**
1. Loop hangs (infinite loop, crash, deadlock)
2. Watchdog not fed
3. After 3 seconds, watchdog triggers reset
4. ESP8266 hard reset (equivalent to power cycle)
5. Device boots fresh, resumes normal operation

**Recovery time:**
- Watchdog timeout: 3 seconds
- Boot time: ~5-10 seconds
- Total recovery: ~8-13 seconds

## Special Considerations

### OTA Firmware Updates

**Challenge:** Flash write operations can block for 10-20 seconds per chunk, exceeding watchdog timeout.

**Solution (see ADR-029):**
1. **Disable watchdog** at start of OTA update: `WatchDogEnabled(0)`
2. **Feed on every chunk** using FEEDWATCHDOGNOW macro (bypasses rate limiting)
3. **Re-enable after flash complete:** `WatchDogEnabled(1)`

**Implementation:**
```cpp
// In OTGW-ModUpdateServer-impl.h
if (upload.status == UPLOAD_FILE_WRITE) {
  // Feed watchdog on every chunk to prevent timeout during flash
  Wire.beginTransmission(0x26);
  Wire.write(0xA5);
  Wire.endTransmission();
  
  // Perform blocking flash write (can take 10+ seconds)
  Update.write(upload.buf, upload.currentSize);
}
```

**Why this approach:**
- Flash operations are critical and cannot be interrupted
- Feeding on every chunk provides safety during long writes
- Disabling entirely would lose watchdog protection during upload phase
- Combination of disable + periodic feeding provides best reliability

### WiFi Reconnection

**Challenge:** WiFi connection attempts can take several seconds.

**Solution:**
- Disable watchdog during reconnection attempts
- Re-enable once connection established or attempt abandoned
- Prevents unnecessary resets during normal network issues

### Heap Emergency Recovery

**Integration with ADR-030:**
- Watchdog provides hardware-level recovery for severe crashes
- Heap monitoring (ADR-030) provides software-level graceful degradation
- Two-layer defense: graceful (heap throttling) → forceful (watchdog reset)

## Related Decisions
- **ADR-007:** Timer-Based Task Scheduling (ensures loop completes quickly, timer-based feeding)
- **ADR-001:** ESP8266 Platform Selection (memory constraints necessitate external watchdog)
- **ADR-029:** Simple XHR-Based OTA Flash (watchdog disabled during flash, fed on chunks)
- **ADR-030:** Heap Memory Monitoring and Emergency Recovery (software-level recovery complements hardware watchdog)
- **ADR-012:** PIC Firmware Upgrade via Web UI (watchdog handling during PIC flash)

## References

### Implementation Files
- **I2C protocol constants:** `src/OTGW-firmware/OTGW-Core.ino` (lines 29, 40)
  - `EXT_WD_I2C_ADDRESS` definition (0x26)
  - `FEEDWATCHDOGNOW` macro definition
- **Watchdog functions:** `src/OTGW-firmware/OTGW-Core.ino` (lines 335-380)
  - `getWatchdogResetReason()` - Read status register
  - `WatchDogEnabled(byte state)` - Enable/disable watchdog
  - `feedWatchDog()` - Rate-limited feeding with timer
- **OTA integration:** `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h` (line 206)
  - Watchdog feeding during flash chunks
- **Main loop:** `src/OTGW-firmware/OTGW-firmware.ino`
  - feedWatchDog() called in main loop

### Documentation
- **Hardware schematics:** `hardware/` directory (I2C watchdog circuit)
- **ESP8266 Wire library:** I2C communication documentation
- **Timer macros:** `src/OTGW-firmware/safeTimers.h` (DECLARE_TIMER_MS)

### Related ADRs
- **ADR-029:** OTA flash implementation with watchdog handling
- **ADR-030:** Heap monitoring as complementary software-level recovery
- **ADR-007:** Timer-based architecture that enables rate-limited feeding
