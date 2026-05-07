# WebSocket Quick Reference

## TL;DR: Who Initiates the Connection?

**The browser (client) initiates the connection TO the ESP8266 (server).**

```
Browser                          ESP8266
=======                          =======
                                 Boot & start WebSocket server on port 81
                                 ↓
                                 [LISTENING on port 81]
User opens http://esp-ip/       ↓
  ↓                              ↓
Loads index.html & index.js      ↓
  ↓                              ↓
JavaScript executes              ↓
  ↓                              ↓
new WebSocket("ws://esp-ip:81/") ↓
  |----------------------------->|
  |     Connection Request       |
  |<-----------------------------|
  |     Connection Accepted      |
  |                              |
  |<=============================|
  |   OpenTherm messages stream  |
```

## Key Points

1. **ESP8266 = SERVER (Listener)**
   - Runs WebSocket server on port 81
   - Waits for browsers to connect
   - Does NOT initiate outbound connections

2. **Browser = CLIENT (Initiator)**
   - Creates WebSocket connection when page loads (with a 250 ms debounce on page show and tab-visibility restore)
   - Connects TO the ESP8266
   - Explicitly closes the socket on `pagehide` and `beforeunload` to avoid overlap with the next page's connection
   - Reconnects automatically if disconnected

3. **Protocol**
   - Plain WebSocket (`ws://`), not encrypted (`wss://`)
   - Port: 81 (separate from HTTP on port 80)
   - No authentication (local network trust model)

4. **Data Flow**
   - ESP8266 → Browser: OpenTherm messages, keepalives, flash progress
   - Browser → ESP8266: Pong responses (automatic)
   - Primarily one-way: Server broadcasts to clients

## Files

- **Server:** `webSocketStuff.ino` (ESP8266 code)
- **Client:** `data/index.js` (Browser JavaScript)
- **Detailed docs:** `docs/api/WEBSOCKET_FLOW.md`

## Common Misconceptions

❌ **WRONG:** "The ESP8266 connects to an external WebSocket server"
✅ **CORRECT:** "The ESP8266 IS the WebSocket server, browsers connect to it"

❌ **WRONG:** "The ESP8266 receives connections from the internet"
✅ **CORRECT:** "The ESP8266 accepts connections from browsers on the local network"

❌ **WRONG:** "WebSocket uses the same port as HTTP (port 80)"
✅ **CORRECT:** "WebSocket uses a separate port (81), HTTP uses port 80"

## Reload-Storm Mitigation (v1.5.0)

Rapid browser reloads used to produce overlapping connect/disconnect events. Two changes address this:

- **Client:** `scheduleOTLogWebSocketInit(false, 250)` replaces the direct `initOTLogWebSocket()` call on page show and tab-visibility restore. The 250 ms delay lets the previous page's socket close before a new one opens. `pagehide` and `beforeunload` cancel the pending timer and close the current socket immediately.
- **Server:** A 5-second sliding window counts lifecycle events. When 3+ events accumulate, a single burst-summary line is written to the telnet debug log. This is diagnostics only; no protocol behavior changes.

If you see `WebSocket burst window=5000ms` in your telnet log during normal operation, a client is reconnecting unusually fast.

## Architecture Decision Records

- **ADR-005:** WebSocket for Real-Time Streaming
- **ADR-003:** HTTP-Only Network Architecture (explains `ws://` vs `wss://`)
- **ADR-010:** Multiple Concurrent Network Services (port allocation)

## See Also

- Full documentation: `docs/api/WEBSOCKET_FLOW.md`
- Browser compatibility: `docs/reviews/2026-01-26_browser-compatibility-review/`
- WebSocket library: https://github.com/Links2004/arduinoWebSockets
