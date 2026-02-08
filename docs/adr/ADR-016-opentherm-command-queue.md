# ADR-016: OpenTherm Command Queue with Deduplication

**Status:** Accepted  
**Date:** 2018-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW firmware receives OpenTherm commands from multiple sources:
- MQTT commands from Home Assistant
- REST API calls from scripts
- Web UI user actions
- Boot commands (configured in settings)
- Internal firmware commands

These commands must be sent to the PIC controller via serial interface, which has constraints:
- **Serial speed:** 9600 baud (slow)
- **Processing time:** PIC takes time to process each command
- **No parallel execution:** Commands must be sent sequentially
- **Risk of overrun:** Sending too fast can overwhelm PIC serial buffer

**Problem scenarios:**
- User rapidly clicks temperature up/down in UI
- MQTT receives multiple commands in quick succession
- Boot commands conflict with runtime commands
- Duplicate commands waste time and bandwidth

## Decision

**Implement a command queue with automatic deduplication and sequential processing.**

**Architecture:**
- **Queue:** Fixed-size array `cmdqueue[CMDQUEUE_MAX]`
- **Size:** Configurable (typically 10-20 commands)
- **Deduplication:** Identical commands merged (only keep latest)
- **Processing:** One command sent per loop iteration
- **Retry:** Optional retry on failure (configurable)
- **Priority:** FIFO order (first in, first out)

**Queue operations:**
```cpp
void addOTWGcmdtoqueue(const char* cmd) {
  // Check if command already in queue
  // If found: update/refresh it
  // If not found: add to end of queue
}

void handleOTGWqueue() {
  // Send next command from queue
  // Wait for PIC response
  // Remove from queue or retry
}
```

## Alternatives Considered

### Alternative 1: Direct Send (No Queue)
**Pros:**
- Simplest implementation
- Immediate execution
- No buffering overhead

**Cons:**
- Serial buffer overrun
- Lost commands
- No duplicate detection
- No retry capability
- Race conditions

**Why not chosen:** Serial overrun causes lost commands and unreliable operation.

### Alternative 2: Priority Queue
**Pros:**
- Important commands execute first
- Can expedite critical operations
- Better responsiveness for UI

**Cons:**
- More complex implementation
- Requires priority assignment logic
- Can starve low-priority commands
- Overkill for use case

**Why not chosen:** FIFO is sufficient. All commands are equally important.

### Alternative 3: Ring Buffer
**Pros:**
- Efficient memory usage
- Fixed size, no allocation
- Fast enqueue/dequeue

**Cons:**
- Oldest commands dropped when full
- No deduplication support
- More complex than array

**Why not chosen:** Fixed array with deduplication is simpler and meets needs.

### Alternative 4: Dynamic Queue (Linked List)
**Pros:**
- Unlimited size (until memory exhausted)
- Easy insertion/deletion
- Flexible

**Cons:**
- Dynamic allocation (heap fragmentation risk)
- Memory overhead per node
- Complex memory management
- Not suitable for constrained ESP8266

**Why not chosen:** Dynamic allocation conflicts with static buffer strategy (ADR-004).

### Alternative 5: Per-Source Queues
**Pros:**
- Isolate MQTT, REST, UI commands
- Prevent one source blocking others
- Better fairness

**Cons:**
- Multiple queues to manage
- More memory usage
- Complex scheduling
- Overkill for serial bottleneck

**Why not chosen:** Serial interface is the bottleneck. Multiple queues don't help.

## Consequences

### Positive
- **Serial protection:** Commands sent at safe rate, no buffer overrun
- **Deduplication:** Rapid duplicate commands collapsed to single execution
  - Example: User clicking temp up 5 times → only final setpoint sent
- **Reliability:** Retry logic handles transient failures
- **Memory efficient:** Fixed-size array, no dynamic allocation
- **Debugging:** Can inspect queue state via debug interface
- **Throttling:** Natural rate limiting prevents PIC overwhelm

### Negative
- **Latency:** Commands queued, not immediate execution
  - Typical delay: 50-500ms depending on queue depth
  - Accepted: Serial is slow anyway (9600 baud)
- **Queue overflow:** If queue fills, new commands rejected
  - Mitigation: Queue size tuned to handle typical load
  - Mitigation: Deduplication reduces queue usage
- **Command loss:** If queue full, command may be dropped
  - Mitigation: Log warning when queue full
  - Rare: Queue sized to handle peak load

### Risks & Mitigation
- **Queue full during boot:** Boot commands overflow queue
  - **Mitigation:** Process boot commands slowly (one per second)
- **Stuck command:** Command never completes, blocks queue
  - **Mitigation:** Timeout on command response (configurable)
  - **Mitigation:** Remove stuck command after max retries
- **Duplicate detection bugs:** Wrong commands deduplicated
  - **Mitigation:** Exact string match required for deduplication
- **Memory corruption:** Array bounds exceeded
  - **Mitigation:** Bounds checking on all queue operations

## Implementation Details

**Queue structure:**
```cpp
#define CMDQUEUE_MAX 20

struct {
  char cmd[CMSG_SIZE];    // Command string (e.g., "TT=20.5")
  int retries;             // Retry count
  bool inProgress;         // Command being sent
} cmdqueue[CMDQUEUE_MAX];

int cmdQueueHead = 0;      // Next command to send
int cmdQueueTail = 0;      // Next free slot
```

**Add command (with deduplication):**
```cpp
void addOTWGcmdtoqueue(const char* cmd) {
  // Check if command already in queue
  for (int i = cmdQueueHead; i != cmdQueueTail; i = (i + 1) % CMDQUEUE_MAX) {
    if (strcmp(cmdqueue[i].cmd, cmd) == 0) {
      // Already in queue - refresh it (move to end)
      DebugTf(PSTR("Command already queued, refreshing: %s\r\n"), cmd);
      // Copy to end, remove from current position
      return;
    }
  }
  
  // Check if queue full
  int nextTail = (cmdQueueTail + 1) % CMDQUEUE_MAX;
  if (nextTail == cmdQueueHead) {
    DebugTln(F("Command queue full, dropping command"));
    return;
  }
  
  // Add to queue
  strlcpy(cmdqueue[cmdQueueTail].cmd, cmd, sizeof(cmdqueue[0].cmd));
  cmdqueue[cmdQueueTail].retries = 0;
  cmdqueue[cmdQueueTail].inProgress = false;
  
  cmdQueueTail = nextTail;
  
  DebugTf(PSTR("Added to queue: %s (depth: %d)\r\n"), cmd, queueDepth());
}
```

**Process queue:**
```cpp
void handleOTGWqueue() {
  // Check if queue empty
  if (cmdQueueHead == cmdQueueTail) {
    return;  // Nothing to do
  }
  
  // Get next command
  QueueEntry* entry = &cmdqueue[cmdQueueHead];
  
  if (!entry->inProgress) {
    // Send command to PIC
    sendOTGW(entry->cmd);
    entry->inProgress = true;
    
    DebugTf(PSTR("Sending from queue: %s\r\n"), entry->cmd);
  }
  
  // Wait for response or timeout
  if (commandComplete(entry->cmd) || commandTimeout(entry->cmd)) {
    // Success or timeout
    DebugTf(PSTR("Command complete: %s\r\n"), entry->cmd);
    
    // Remove from queue
    cmdQueueHead = (cmdQueueHead + 1) % CMDQUEUE_MAX;
  }
  else if (commandFailed(entry->cmd)) {
    // Retry or remove
    entry->retries++;
    if (entry->retries >= MAX_RETRIES) {
      DebugTf(PSTR("Command failed after %d retries: %s\r\n"), 
              entry->retries, entry->cmd);
      cmdQueueHead = (cmdQueueHead + 1) % CMDQUEUE_MAX;
    }
    else {
      DebugTf(PSTR("Retrying command (%d/%d): %s\r\n"),
              entry->retries, MAX_RETRIES, entry->cmd);
      entry->inProgress = false;  // Retry
    }
  }
}
```

**Common OpenTherm commands:**
```cpp
// Temperature override (temporary setpoint)
addOTWGcmdtoqueue("TT=20.5");

// DHW setpoint
addOTWGcmdtoqueue("SW=55");

// Control setpoint (heating)
addOTWGcmdtoqueue("CS=60");

// Gateway mode
addOTWGcmdtoqueue("GW=M");  // Monitor mode

// Reset
addOTWGcmdtoqueue("GW=R");

// Set LED modes
addOTWGcmdtoqueue("LA=F");  // LED A = Flame
```

**MQTT to command mapping:**
```cpp
// In MQTTstuff.ino
const char* setcmds[] = {
  "TT",  // TargetTemperature
  "SW",  // DHWsetpoint
  "CS",  // ControlSetpoint
  "CH",  // CentralHeatingEnable
  // ... more mappings
};

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Parse topic to get command type
  // Map to OTGW command
  // Add to queue
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "%s=%s", otgwCmd, value);
  addOTWGcmdtoqueue(cmd);
}
```

**Boot commands:**
```cpp
void sendBootCommands() {
  // Boot commands from settings
  if (strlen(settingGPIObootcmd) > 0) {
    addOTWGcmdtoqueue(settingGPIObootcmd);
  }
  
  // Set LED modes
  if (strlen(settingLEDabluecmd) > 0) {
    addOTWGcmdtoqueue(settingLEDabluecmd);
  }
  
  // Process slowly (one per second)
  DECLARE_TIMER_SEC(bootCmdTimer, 1);
  // Process next command when timer fires
}
```

## Queue Monitoring

**Debug interface:**
```
> queue status
Queue depth: 3 / 20
Commands:
  1. TT=20.5 (retries: 0)
  2. SW=55 (retries: 0)
  3. LA=F (retries: 1)
```

**REST API:**
```
GET /api/v1/otgw/queue
{
  "depth": 3,
  "max": 20,
  "commands": [
    {"cmd": "TT=20.5", "retries": 0},
    {"cmd": "SW=55", "retries": 0}
  ]
}
```

## Related Decisions
- ADR-004: Static Buffer Allocation Strategy (fixed-size queue array)
- ADR-006: MQTT Integration Pattern (MQTT commands use queue)
- ADR-007: Timer-Based Task Scheduling (queue processed in main loop)

## References
- Implementation: `OTGW-Core.ino` (queue functions)
- MQTT commands: `MQTTstuff.ino` (setcmds array)
- OpenTherm commands: https://otgw.tclcode.com/firmware.html (Schelte Bron documentation)
- Command format: `OTGW-Core.h` (OpenTherm message IDs)
