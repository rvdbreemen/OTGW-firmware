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
- **Communication:** ESP8266 sends I2C "heartbeat" signals
- **Timeout:** ~3 seconds (if not fed, watchdog triggers reset)
- **GPIO pins:** GPIO5 (D1/SCL) and GPIO4 (D2/SDA) for I2C
- **Feeding location:** Called from main `loop()` every iteration
- **Independence:** Watchdog operates independently of ESP8266 software

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

**Watchdog feeding function:**
```cpp
void feedWatchDog() {
  // Send I2C heartbeat to watchdog chip at address 0x26
  Wire.beginTransmission(0x26);
  Wire.write(0x00);  // Heartbeat command
  Wire.endTransmission();
}
```

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
  Wire.begin();  // SDA=GPIO4, SCL=GPIO5
  
  // Feed watchdog during long setup operations
  feedWatchDog();
  
  // Initialize WiFi (can take >3 seconds)
  startWiFi();
  feedWatchDog();  // Feed again during boot
  
  // Rest of setup...
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

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (ensures loop completes quickly)
- ADR-001: ESP8266 Platform Selection (need for external watchdog)

## References
- Hardware schematics: `hardware/` directory (I2C watchdog circuit)
- Watchdog feeding: `helperStuff.ino` (feedWatchDog function)
- Main loop: `OTGW-firmware.ino` (loop function)
- I2C configuration: ESP8266 Wire library documentation
