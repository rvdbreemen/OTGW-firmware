# WebSocket Robustness - Visual Guide

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Browser (Client)                         │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │               WebSocket Client (port 81)                    │ │
│  │                                                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Watchdog Timer: 45 seconds                          │  │ │
│  │  │  ✓ Resets on any message received                    │  │ │
│  │  │  ✓ Fires if no data for 45s → triggers reconnect     │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  │                                                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Keepalive Handler                                    │  │ │
│  │  │  ✓ Receives {"type":"keepalive"} every 30s           │  │ │
│  │  │  ✓ Resets watchdog timer                             │  │ │
│  │  │  ✓ Doesn't add to log buffer                         │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  │                                                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Auto-Reconnect Logic                                 │  │ │
│  │  │  ✓ Triggers on disconnect or watchdog timeout        │  │ │
│  │  │  ✓ Waits 5 seconds before retry                      │  │ │
│  │  │  ✓ Clears all timers on connect                      │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ WebSocket (ws://host:81/)
                              │ PING/PONG + JSON messages
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      ESP8266 (Server)                            │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │          WebSocketsServer (port 81)                         │ │
│  │                                                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Heartbeat (Protocol Level)                          │  │ │
│  │  │  ✓ Sends PING every 15 seconds                       │  │ │
│  │  │  ✓ Expects PONG within 3 seconds                     │  │ │
│  │  │  ✓ Disconnects after 2 missed PONGs (30s total)      │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  │                                                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Application Keepalive                                │  │ │
│  │  │  ✓ Broadcasts {"type":"keepalive"} every 30s         │  │ │
│  │  │  ✓ Sent to all connected clients                     │  │ │
│  │  │  ✓ Minimal overhead (~20 bytes)                      │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  │                                                             │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │  Connection Management                                │  │ │
│  │  │  ✓ Max 3 simultaneous clients                        │  │ │
│  │  │  ✓ Auto-cleanup of dead connections                  │  │ │
│  │  │  ✓ Heap health checks                                │  │ │
│  │  └──────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Timeline of Normal Operation

```
Time    Server Action              Network               Client Action
────────────────────────────────────────────────────────────────────────
0s      Connect accepted    ────────────────────────►  WebSocket.onopen()
        Heartbeat enabled                              Watchdog: 45s

15s     PING ───────────────────────────────────────►  (browser handles)
        
15.1s                       ◄─────────────────────────  PONG (automatic)

30s     Broadcast keepalive ─────────────────────────► Receive keepalive
        {"type":"keepalive"}                           Watchdog: reset to 45s

45s     PING ───────────────────────────────────────►  (browser handles)

45.1s                       ◄─────────────────────────  PONG (automatic)

60s     Broadcast keepalive ─────────────────────────► Receive keepalive
        {"type":"keepalive"}                           Watchdog: reset to 45s

75s     PING ───────────────────────────────────────►  (browser handles)

75.1s                       ◄─────────────────────────  PONG (automatic)

90s     Broadcast keepalive ─────────────────────────► Receive keepalive
        {"type":"keepalive"}                           Watchdog: reset to 45s

... (continues indefinitely)
```

## Timeline of Connection Failure & Recovery

### Scenario: Server Stops Responding

```
Time    Server State               Network               Client State
────────────────────────────────────────────────────────────────────────
0s      Normal operation    ─────────────────────────► Normal operation
        Last keepalive sent                            Watchdog: 45s

15s     (server hung)       ─ ─ ─ ─ X (no PING)       Watchdog: 30s

30s     (server hung)       ─ ─ ─ ─ X (no keepalive)  Watchdog: 15s

45s     (server hung)       ─ ─ ─ ─ X                 WATCHDOG TIMEOUT!
                                                       Close connection
                                                       
45.1s                                                  Wait 5 seconds...

50s                         ◄─────────────────────────  New connection attempt

50.1s   Accept connection   ─────────────────────────► WebSocket.onopen()
        Heartbeat restart                              Watchdog: reset to 45s

65s     PING ───────────────────────────────────────►  (browser handles)

65.1s                       ◄─────────────────────────  PONG (automatic)

80s     Broadcast keepalive ─────────────────────────► Receive keepalive
        {"type":"keepalive"}                           Watchdog: reset to 45s

... (normal operation resumes)
```

### Scenario: Network Disruption (NAT Timeout)

```
Time    Server State               Network               Client State
────────────────────────────────────────────────────────────────────────
0s      Normal operation    ─────────────────────────► Normal operation

15s     PING ───────────────────►  X  (NAT dropped)    (never received)

18s     (no PONG received)                             (still connected)

30s     PING ───────────────────►  X  (NAT dropped)    (never received)

33s     (no PONG, 2nd miss)                            (still connected)
        DISCONNECT client!
        
33.1s                       ◄─────────────────────────  Receive close frame
                                                       WebSocket.onclose()
                                                       Wait 5 seconds...

38s                         ◄─────────────────────────  New connection attempt

38.1s   Accept connection   ─────────────────────────► WebSocket.onopen()
        Heartbeat restart                              Watchdog: reset to 45s

... (normal operation resumes)
```

## Two-Layer Defense Visualization

```
                    ┌─────────────────────────────┐
                    │   WebSocket Connection      │
                    └─────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
         ┌──────────▼─────────┐  ┌───────▼────────┐
         │  Protocol Layer    │  │ Application    │
         │  (RFC 6455)        │  │ Layer          │
         └────────────────────┘  └────────────────┘
                    │                     │
         ┌──────────▼─────────┐  ┌───────▼────────┐
         │  PING/PONG         │  │ JSON Keepalive │
         │  • Every 15s       │  │ • Every 30s    │
         │  • 3s timeout      │  │ • "keepalive"  │
         │  • Disconnect      │  │ • Reset timer  │
         │    after 2 miss    │  │                │
         └────────────────────┘  └────────────────┘
                    │                     │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼─────────┐
                    │  Combined Result:  │
                    │                    │
                    │  • NAT keepalive   │
                    │  • Dead detection  │
                    │  • Safari compat   │
                    │  • Auto-recovery   │
                    └────────────────────┘
```

## Failure Detection Comparison

### Before (10s watchdog only)

```
┌─────────────────────────────────────────────────────┐
│ Time to Detect Failure:                             │
│                                                      │
│  Server down:        10 seconds ⚠️                   │
│  NAT timeout:        10+ seconds ⚠️                  │
│  Network drop:       10+ seconds ⚠️                  │
│  Silent failure:     NEVER ❌                        │
│                                                      │
│ False Positives:     HIGH ⚠️                         │
│  (triggers on legitimate idle periods)              │
└─────────────────────────────────────────────────────┘
```

### After (Heartbeat + Keepalive + 45s watchdog)

```
┌─────────────────────────────────────────────────────┐
│ Time to Detect Failure:                             │
│                                                      │
│  Server down:        30-45 seconds ✅                │
│  NAT timeout:        30 seconds (2 missed PINGs) ✅  │
│  Network drop:       30-45 seconds ✅                │
│  Silent failure:     45 seconds ✅                   │
│                                                      │
│ False Positives:     NONE ✅                         │
│  (keepalive messages prevent timeout)               │
└─────────────────────────────────────────────────────┘
```

## Browser Compatibility Matrix

```
┌──────────────┬──────────────┬──────────────┬─────────────┐
│ Browser      │ PING/PONG    │ Keepalive    │ Combined    │
├──────────────┼──────────────┼──────────────┼─────────────┤
│ Chrome       │ ✅ Perfect    │ ✅ Perfect    │ ✅ Excellent │
│ Firefox      │ ✅ Perfect    │ ✅ Perfect    │ ✅ Excellent │
│ Safari       │ ⚠️ Quirky     │ ✅ Perfect    │ ✅ Good      │
│ Edge         │ ✅ Perfect    │ ✅ Perfect    │ ✅ Excellent │
│ Mobile       │ ⚠️ Varies     │ ✅ Works      │ ✅ Good      │
└──────────────┴──────────────┴──────────────┴─────────────┘

Legend:
  ✅ Perfect: Full RFC 6455 compliance, no issues
  ⚠️ Quirky: Some timing issues, works with workaround
  ✅ Good: Reliable with both mechanisms combined
```

## Memory & Performance Impact

```
┌─────────────────────────────────────────────────────┐
│              Server (ESP8266)                        │
├─────────────────────────────────────────────────────┤
│ RAM:         +24 bytes (keepalive timer)             │
│ Flash:       +~200 bytes (new code)                  │
│ CPU:         <0.1% additional load                   │
│ Network:     +20 bytes every 30s per client          │
├─────────────────────────────────────────────────────┤
│              Client (Browser)                        │
├─────────────────────────────────────────────────────┤
│ RAM:         No change (timer reset only)            │
│ CPU:         Negligible                              │
│ Network:     Receives +20 bytes every 30s            │
└─────────────────────────────────────────────────────┘

Total Network Overhead (1 hour, 1 client):
  120 keepalive messages × 20 bytes = 2,400 bytes
  240 PING frames × ~few bytes = ~500 bytes
  ────────────────────────────────────────────
  Total: ~3 KB/hour (negligible)
```

## State Machine

```
                     ┌─────────────┐
                     │   INITIAL   │
                     └──────┬──────┘
                            │
                            │ initOTLogWebSocket()
                            ▼
                     ┌─────────────┐
                ┌────│ CONNECTING  │
                │    └──────┬──────┘
                │           │
    onclose() ──┘           │ onopen()
                            ▼
                     ┌─────────────┐
                     │  CONNECTED  │◄─────┐
                     └──────┬──────┘      │
                            │             │
        ┌───────────────────┼─────────────┤
        │                   │             │
        │ Keepalive         │ OTGW data   │ Keepalive
        │ received          │ received    │ received
        │                   │             │
        └───────────────────┼─────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        │ Watchdog timeout  │ onclose()         │ Server PING
        │ (45s no data)     │                   │ missed (30s)
        ▼                   ▼                   ▼
  ┌──────────┐       ┌──────────┐       ┌──────────┐
  │ TIMEOUT  │       │ CLOSED   │       │ TIMEOUT  │
  └────┬─────┘       └────┬─────┘       └────┬─────┘
       │                  │                   │
       │ Close socket     │ Clear timers      │ Server disconnect
       └──────────────────┼───────────────────┘
                          │
                          │ Wait 5 seconds
                          ▼
                   ┌─────────────┐
                   │ RECONNECTING│
                   └──────┬──────┘
                          │
                          │ Retry connection
                          │
                          └───────► (back to CONNECTING)
```

## Configuration Tuning Guide

```
┌─────────────────────────────────────────────────────────────┐
│                  Timeout Configuration                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Server PING interval:     15 seconds (default)              │
│  ├─ Decrease to 10s:       More aggressive NAT keepalive    │
│  └─ Increase to 20s:       Lower overhead                   │
│                                                              │
│  Server PONG timeout:      3 seconds (default)               │
│  ├─ Decrease to 2s:        Faster dead connection detection │
│  └─ Increase to 5s:        Tolerance for slow networks      │
│                                                              │
│  Missed PONG count:        2 (default = 30s total)           │
│  ├─ Decrease to 1:         Faster disconnect (15s total)    │
│  └─ Increase to 3:         More tolerance (45s total)       │
│                                                              │
│  Keepalive interval:       30 seconds (default)              │
│  ├─ Decrease to 15s:       Safari compatibility boost       │
│  └─ Increase to 60s:       Lower overhead                   │
│                                                              │
│  Client watchdog:          45 seconds (default)              │
│  ├─ Decrease to 30s:       Faster failure detection         │
│  └─ Increase to 60s:       Fewer false positives            │
│                                                              │
│  Reconnect delay:          5 seconds (default)               │
│  ├─ Decrease to 1s:        Faster recovery                  │
│  └─ Increase to 10s:       Reduce server load on failure    │
│                                                              │
└─────────────────────────────────────────────────────────────┘

Recommended for Safari:
  - Keepalive: 20s (more aggressive)
  - Watchdog: 35s (shorter)
  - PING: 10s (more aggressive)

Recommended for slow networks:
  - Watchdog: 60s (more tolerant)
  - PONG timeout: 5s (more tolerant)
  - Missed PONG: 3 (more tolerant)
```

---

**Visual Guide Version**: 1.0  
**Created**: 2026-01-22  
**For**: OTGW-firmware v1.0.0-rc4+
