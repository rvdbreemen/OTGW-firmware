# ADR-038: OpenTherm Message Data Flow Pipeline

**Status:** Accepted  
**Date:** 2026-02-16  
**Updated:** 2026-02-16 (Initial documentation of existing pattern)

## Context

The core purpose of the OTGW-firmware is to receive OpenTherm messages from the PIC controller and distribute them to multiple consumers: MQTT (Home Assistant), WebSocket (Web UI graph/log), REST API (on-demand queries), OTmonitor (TCP bridge), and telnet debug. This **fan-out data flow** is the central architectural pattern of the firmware.

Understanding the complete pipeline is essential because:
1. **Performance:** Every OT message triggers processing across all consumers — bottlenecks in one consumer can delay others
2. **Memory:** Each consumer has different memory requirements (MQTT buffers, WebSocket frames, REST response builders)
3. **Reliability:** Consumer failures (e.g., MQTT disconnect) must not affect other consumers
4. **Ordering:** Some consumers expect messages in order (OTmonitor), others are tolerant (REST API)

**Message volume:** The OpenTherm protocol typically generates 1-4 messages per second from the boiler/thermostat, with each message being a 9-byte raw frame that expands to a ~30-byte text representation.

## Decision

**Implement a synchronous fan-out pipeline where `handleOTGW()` reads serial data and dispatches complete lines to `processOT()`, which updates global state and pushes to all consumers in a single call.**

### Pipeline Architecture

```
PIC Controller (Serial @ 9600 baud)
       │
       ▼
┌─────────────────────────────────────────────────────┐
│ handleOTGW()  —  Serial ↔ Network Bridge            │
│                                                      │
│  Serial.read() ──┬──► OTGWstream.write()            │
│                  │    (raw byte forwarding            │
│                  │     to TCP port 25238)             │
│                  │                                    │
│                  └──► Line buffer (256 bytes)         │
│                       on CR/LF:                       │
│                       processOT(buf, len) ──────────┐│
│                                                      ││
│  OTGWstream.read() ──► OTGWSerial.write()           ││
│  (port 25238 → serial, with GW=R/PS=1/PS=0          ││
│   command interception)                              ││
└──────────────────────────────────────────────────────┘│
                                                        │
       ┌────────────────────────────────────────────────┘
       ▼
┌─────────────────────────────────────────────────────┐
│ processOT(buf, len)  —  Message Parser & Dispatcher  │
│                                                      │
│  1. Parse message type prefix (T/B/R/A/E)           │
│  2. Decode 32-bit OT frame (ID, HB, LB)            │
│  3. Update OTcurrentSystemState (100+ fields)       │
│  4. Type-specific processing by MsgID:              │
│     ├─► Status bits → sendMQTTData() per bit        │
│     ├─► Temperatures → sendMQTTData()               │
│     ├─► Setpoints → sendMQTTData()                  │
│     └─► Faults/Diagnostics → sendMQTTData()         │
│  5. doAutoConfigureMsgid() → MQTT auto-discovery    │
│  6. Format log line → ot_log_buffer                 │
│  7. sendLogToWebSocket(ot_log_buffer)               │
│  8. msglastupdated[id] = epoch (REST query support) │
└─────────────────────────────────────────────────────┘
```

### Consumer Details

| Consumer | Transport | Trigger | Memory Impact | Failure Mode |
|----------|-----------|---------|---------------|-------------|
| **OTmonitor** | TCP port 25238 | Every byte (raw forwarding) | ~128 bytes write buffer | Silent drop if disconnected |
| **MQTT** | PubSubClient | Per parsed message | ~1200 bytes publish buffer | `canPublishMQTT()` backpressure (ADR-030) |
| **WebSocket** | Port 81 | Per log line | ~700 bytes per client (max 3) | Silently skipped if no clients |
| **REST API** | HTTP port 80 | On-demand (pull) | Reads from `OTcurrentSystemState` | Not affected by message flow |
| **Telnet Debug** | Port 23 | When debug flags enabled | ~256 bytes log buffer | Silently skipped if not connected |

### Bidirectional Flow

The pipeline is bidirectional. Commands flow **inward** from network to PIC:

```
MQTT command ──┐
REST API cmd ──┼──► addOTWGcmdtoqueue() ──► handleOTGWqueue()
Web UI cmd ────┘          │                      │
Boot commands ─┘          ▼                      ▼
                   cmdqueue[20]            OTGWSerial.write()
                   (deduplication)              │
                                                ▼
                                         PIC Controller

OTmonitor ──────► OTGWstream.read() ──► OTGWSerial.write()
(port 25238)      (direct pass-through with GW=R/PS=1/PS=0 interception)
```

**Key difference:** Commands from MQTT/REST/WebUI/boot go through the command queue (ADR-016) with deduplication and throttling. Commands from OTmonitor (port 25238) bypass the queue and go directly to serial, with only `GW=R`, `PS=1`, and `PS=0` being intercepted for special handling.

### Synchronous Fan-Out Properties

- **No message queuing between consumers:** All consumers process in the same call stack as `processOT()`
- **Consumer independence:** Each consumer checks its own availability before acting (MQTT checks `canPublishMQTT()`, WebSocket checks client count, etc.)
- **Global state as buffer:** `OTcurrentSystemState` serves as a shared state object that REST API reads from asynchronously — it's always up-to-date from the last processed message
- **No back-pressure on serial:** The PIC sends at a fixed rate (9600 baud). If consumers are slow, serial bytes still get read and forwarded to OTGWstream immediately. Only the parsed processing may lag.

## Alternatives Considered

### Alternative 1: Message Queue with Async Consumers
**Pros:**
- Decoupled consumers
- Consumers process at their own rate
- Buffer for slow consumers

**Cons:**
- Queue memory overhead (ESP8266 has ~40KB RAM)
- Queue management complexity
- Still single-core — no true parallelism
- Risk of queue overflow under load

**Why not chosen:** Memory constraints make per-consumer queues impractical. Synchronous fan-out is simpler and works because all consumers are fast (no blocking I/O in consumer callbacks).

### Alternative 2: Event/Callback System
**Pros:**
- Clean separation of concerns
- Easy to add/remove consumers
- Testable in isolation

**Cons:**
- Callback registration overhead
- Function pointer tables consume RAM
- More complex to debug on ESP8266
- Over-engineering for 5 fixed consumers

**Why not chosen:** The consumer list is fixed and small (5 consumers). Direct function calls are simpler and faster than a callback dispatch system.

### Alternative 3: Publish-Subscribe Bus (Internal)
**Pros:**
- Loose coupling
- Topic-based filtering
- Conceptually clean

**Cons:**
- Significant memory overhead
- Topic matching adds latency per message
- Complex for embedded system
- Duplicates MQTT pattern internally

**Why not chosen:** This would be an internal MQTT — redundant since external MQTT already serves the subscribe role for remote consumers.

## Consequences

### Positive
- **Low latency:** Serial → all consumers in a single pass (microseconds)
- **Simple debugging:** Linear call stack, easy to trace with telnet debug
- **Memory efficient:** No intermediate queues between consumers
- **Reliable ordering:** All consumers see messages in the same order
- **Transparent bridge:** OTmonitor sees raw serial data without parsing delay

### Negative
- **Tight coupling:** Adding a new consumer requires modifying `processOT()` or `handleOTGW()`
  - Accepted: Consumer list changes rarely (years between additions)
- **No consumer isolation:** A crash in one consumer affects all
  - Mitigation: Each consumer has its own error handling
  - Mitigation: Critical consumers (OTGWstream) process raw bytes before parsing
- **Blocking risk:** A slow consumer delays all subsequent consumers
  - Mitigation: All consumers are non-blocking (MQTT uses async client, WebSocket is fire-and-forget)
  - Mitigation: `canPublishMQTT()` can skip MQTT entirely under memory pressure

### Risks & Mitigation
- **Serial buffer overflow:** processOT() takes too long
  - **Mitigation:** 256-byte read buffer provides ~26ms buffer at 9600 baud
  - **Mitigation:** Overflow counter tracked and reported via MQTT
  - **Mitigation:** On overflow, bytes are discarded until next line terminator (resync)
- **MQTT backpressure blocks pipeline:** MQTT publish is slow
  - **Mitigation:** `canPublishMQTT()` returns false under memory pressure (ADR-030)
  - **Mitigation:** MQTT publish is non-blocking (PubSubClient queues internally)

## Related Decisions
- ADR-005: WebSocket for Real-Time Streaming (WebSocket consumer)
- ADR-006: MQTT Integration Pattern (MQTT consumer with backpressure)
- ADR-010: Multiple Concurrent Network Services (all consumers on different ports)
- ADR-016: OpenTherm Command Queue (inbound command path)
- ADR-030: Heap Memory Monitoring (backpressure integration)
- ADR-031: Two-Microcontroller Coordination (serial bridge architecture)

## References
- Implementation: `OTGW-Core.ino` handleOTGW(), processOT()
- Serial bridge: `OTGW-Core.ino` (OTGWstream read/write)
- MQTT publishing: `MQTTstuff.ino` sendMQTTData()
- WebSocket: `webSocketStuff.ino` sendLogToWebSocket()
- State object: `OTGW-Core.h` OTcurrentSystemState struct
- Heap backpressure: `helperStuff.ino` canPublishMQTT()
