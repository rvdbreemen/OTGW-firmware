# ADR-007: Timer-Based Task Scheduling

**Status:** Accepted  
**Date:** 2018-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)  
**Enhanced:** 2020-01-01 (49-day rollover protection)

## Context

The ESP8266 is a single-core processor running a cooperative multitasking environment. The firmware must handle multiple periodic tasks without blocking:
- WiFi connection monitoring
- MQTT publishing (every 30 seconds for slow-changing values)
- NTP time synchronization (every 30 minutes)
- Watchdog feeding (every 3 seconds max)
- Sensor readings (every 5 seconds)
- LED status updates (every second)
- Settings auto-save (every 5 minutes if changed)

**Constraints:**
- No threading or preemptive multitasking
- `millis()` wraps after 49.7 days (32-bit unsigned overflow)
- `loop()` must never block for more than a few milliseconds
- Watchdog timer requires regular feeding (3-second timeout)

**Anti-patterns to avoid:**
- `delay()` blocking calls
- Busy-wait loops
- Missed timer ticks causing "spiral of death"

## Decision

**Implement a timer-based task scheduling system using safe macros that handle millisecond rollover correctly.**

**Implementation:**
- **Timer library:** Custom `safeTimers.h` macros (not external library)
- **Timer types:** Three strategies for different use cases
- **Resolution:** Millisecond and second timers
- **Rollover safety:** All comparisons handle 49-day wrap
- **Execution model:** Check timers in `loop()`, execute when due

**Timer types:**
```cpp
// Type 1: Skip missed ticks (default)
DECLARE_TIMER_SEC(timerName, interval);
if (DUE(timerName)) {
  // Execute task
  // Next execution: now + interval
}

// Type 2: Catch up missed ticks
DECLARE_TIMER_SEC(timerName, interval, CATCH_UP_MISSED_TICKS);
if (DUE(timerName)) {
  // Execute task
  // Next execution: last_execution + interval (may trigger immediately again)
}

// Type 3: Skip missed ticks with sync (prevent spiral of death)
DECLARE_TIMER_SEC(timerName, interval, SKIP_MISSED_TICKS_WITH_SYNC);
if (DUE(timerName)) {
  // Execute task
  // Next execution: now + interval, but synced to prevent drift
}
```

**Rollover-safe comparison:**
```cpp
// Safe for 49-day millis() rollover
#define DUE(timer) ((int32_t)(millis() - timer) >= 0)
```

## Alternatives Considered

### Alternative 1: delay() Blocking
**Pros:**
- Simple to understand
- Direct sequential flow

**Cons:**
- Blocks entire system
- Watchdog timeout causes reset
- Unresponsive to network/user input
- Cannot handle multiple tasks
- WiFi disconnects during delays

**Why not chosen:** Completely unsuitable for networked device. Would cause constant watchdog resets.

### Alternative 2: FreeRTOS (ESP8266 RTOS SDK)
**Pros:**
- True preemptive multitasking
- Task priorities
- Industry-standard RTOS

**Cons:**
- Different SDK (not Arduino framework)
- Requires complete rewrite
- Higher memory overhead
- More complex debugging
- Breaks Arduino library compatibility
- Steeper learning curve

**Why not chosen:** Would require abandoning Arduino framework and rewriting entire codebase. Not compatible with community contribution model.

### Alternative 3: Ticker Library (ESP8266 Arduino Core)
**Pros:**
- Built into ESP8266 core
- Hardware timer-based
- Precise timing

**Cons:**
- Callbacks execute in interrupt context (ISR)
- Cannot use most Arduino functions in callbacks
- Serial, WiFi, File operations forbidden in ISR
- Easy to cause crashes
- Difficult debugging

**Why not chosen:** Too restrictive. Most tasks need to use WiFi, Serial, or File operations which are forbidden in ISR context.

### Alternative 4: SimpleTimer Library
**Pros:**
- Third-party library
- Timer management abstraction
- Multiple timers support

**Cons:**
- External dependency
- Added complexity
- May not handle 49-day rollover correctly
- Overkill for simple needs
- Additional memory overhead

**Why not chosen:** Simple macros provide same functionality without external dependency.

### Alternative 5: TaskScheduler Library
**Pros:**
- Feature-rich task scheduling
- Dependencies between tasks
- Task priorities

**Cons:**
- Large library overhead
- Memory usage
- Complexity exceeds requirements
- Learning curve

**Why not chosen:** Too complex for requirements. Simple timer checks are sufficient.

## Consequences

### Positive
- **Non-blocking:** All tasks execute quickly, yield control back to `loop()`
- **Responsive:** Network and user input processed without delay
- **Watchdog compliant:** Loop completes quickly, watchdog fed regularly
- **Flexible intervals:** Easy to add new periodic tasks
- **Rollover safe:** Works correctly after 49+ days uptime
- **Zero dependencies:** No external libraries required
- **Minimal overhead:** Just integer comparison per timer check
- **Debuggable:** Clear execution order, no hidden behavior

### Negative
- **Manual management:** Developer must remember to check timers
  - Mitigation: Established pattern in `loop()` function
- **No task priorities:** All tasks checked every loop iteration
  - Accepted: Tasks are infrequent enough that checking is cheap
- **Execution time limits:** Tasks must complete quickly (<100ms)
  - Mitigation: Long-running operations split into state machines
- **Jitter:** Timer execution can vary by loop time (~1-10ms)
  - Accepted: Sub-second precision not required for most tasks

### Risks & Mitigation
- **Missed deadlines:** If loop takes too long, timers may be late
  - **Mitigation:** Monitor loop execution time, log slow loops
  - **Mitigation:** Heap-aware throttling reduces work when system stressed
- **Timer drift:** Execution time affects next interval
  - **Mitigation:** `SKIP_MISSED_TICKS_WITH_SYNC` mode prevents accumulation
- **Rollover bugs:** Incorrect time comparison can fail after 49 days
  - **Mitigation:** All comparisons use `(int32_t)(millis() - timer)` pattern
  - **Testing:** Fixed 7+ rollover bugs in v1.0.0 development

## Implementation Patterns

**Standard periodic task:**
```cpp
// In global scope
DECLARE_TIMER_SEC(publishTimer, 30);  // Every 30 seconds

// In loop()
void loop() {
  if (DUE(publishTimer)) {
    publishMQTTValues();
  }
  // Other tasks...
}
```

**Minute-change detection:**
```cpp
// Execute exactly once per minute change
DECLARE_TIMER_MIN(minuteChanged, 1);
if (DUE(minuteChanged)) {
  // Minute changed (e.g., 14:32 -> 14:33)
  doMinuteStuff();
}
```

**Conditional execution:**
```cpp
// Only execute if conditions met
DECLARE_TIMER_SEC(heapMonitor, 60);
if (DUE(heapMonitor)) {
  if (settingHeapMonitorEnabled) {
    printHeapStats();
  }
}
```

**Catch-up mode (for precise intervals):**
```cpp
// NTP sync must happen at precise intervals
DECLARE_TIMER_MIN(ntpSync, 30, CATCH_UP_MISSED_TICKS);
if (DUE(ntpSync)) {
  syncNTPTime();
  // If missed by 5 minutes, will execute 5 times to catch up
}
```

**Rollover-safe patterns:**
```cpp
// BAD - Fails after 49 days
if (millis() > timer) { }

// GOOD - Works after rollover
if ((int32_t)(millis() - timer) >= 0) { }

// BEST - Use DUE() macro
if (DUE(timer)) { }
```

## Common Timer Intervals

**Firmware usage:**
```cpp
DECLARE_TIMER_SEC(publish1sec, 1);      // LED updates, status
DECLARE_TIMER_SEC(publish5sec, 5);      // Sensor reads
DECLARE_TIMER_SEC(publish30sec, 30);    // MQTT publishes
DECLARE_TIMER_SEC(publish60sec, 60);    // Heap monitoring
DECLARE_TIMER_MIN(publish5min, 5);      // Settings auto-save
DECLARE_TIMER_MIN(ntpTimer, 30);        // NTP resync
```

## Watchdog Feeding

**Critical pattern:**
```cpp
void loop() {
  // Feed watchdog at start of every loop
  feedWatchDog();
  
  // Process timers and tasks
  handleTimerTasks();
  
  // Never call delay() or block
  // Loop must complete in <3 seconds
}
```

## Related Decisions
- ADR-001: ESP8266 Platform Selection (single-core constraint)
- ADR-002: Modular .ino File Architecture (timer declarations per module)

## References
- Timer implementation: `safeTimers.h`
- Main loop structure: `OTGW-firmware.ino` (loop function)
- 49-day rollover fixes: Git history, v1.0.0 changelog
- Watchdog feeding: `helperStuff.ino`
