# ADR-005: WebSocket for Real-Time Streaming

**Status:** Accepted  
**Date:** 2019-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware needed to provide real-time visibility into OpenTherm message traffic for debugging and monitoring. Users needed to see OpenTherm messages as they arrive, similar to the OTmonitor desktop application.

**Requirements:**
- Display OpenTherm messages in real-time in the Web UI
- Show message timestamp, direction, type, and content
- Support multiple simultaneous viewers
- Minimize server load and bandwidth
- Work in modern web browsers without plugins

**Existing limitations:**
- HTTP polling creates excessive load (request overhead per poll)
- Server-Sent Events (SSE) are one-way only
- TCP socket requires non-browser client (like OTmonitor)
- REST API provides only current state, not message stream

## Decision

**Use WebSocket protocol on a dedicated port (81) for real-time OpenTherm message streaming.**

**Implementation details:**
- **Protocol:** `ws://` (not `wss://` - see ADR-003)
- **Port:** 81 (separate from HTTP on port 80)
- **Library:** `WebSocketsServer` from Links2004
- **Message format:** Text-based with timestamp prefix
- **Client limit:** Maximum 3 concurrent connections (heap-aware)
- **Buffer size:** 256 bytes per client (reduced from 512 in v1.0.0)
- **No authentication:** Local network trust model (see ADR-003)

**Message format:**
```
HH:MM:SS.mmmmmm <direction> <hex message>
Example: 14:23:45.123456 >> T80200000
```

**Adaptive throttling (v1.0.0):**
- Normal: 20 messages/second max
- HEAP_LOW: 5 messages/second
- HEAP_CRITICAL: Block new messages

## Alternatives Considered

### Alternative 1: HTTP Long Polling
**Pros:**
- Works with any HTTP client
- No special browser support needed
- Simple to implement

**Cons:**
- High overhead (HTTP headers per message)
- Latency from poll interval
- Connection timeout management complexity
- Scalability issues with multiple clients
- Wastes bandwidth on headers

**Why not chosen:** Too much overhead for real-time streaming. HTTP headers would consume more bandwidth than the actual messages.

### Alternative 2: Server-Sent Events (SSE)
**Pros:**
- Standard HTML5 feature
- Built-in reconnection
- Simpler than WebSocket
- Works over HTTP

**Cons:**
- One-way only (server to client)
- Requires keeping HTTP connection open
- Browser connection limits (6 per domain)
- Less efficient than WebSocket
- No binary support

**Why not chosen:** While SSE would work for streaming, WebSocket provides better efficiency and could support future two-way communication if needed.

### Alternative 3: MQTT Subscription
**Pros:**
- Already have MQTT implementation
- QoS levels for reliability
- Standard protocol

**Cons:**
- Requires MQTT broker
- Overkill for browser clients
- Additional dependency for users
- Not suitable for Web UI integration
- Latency through broker

**Why not chosen:** Adds unnecessary complexity for browser clients. MQTT is for integration, not UI.

### Alternative 4: Embed in HTTP Response (Chunked Transfer)
**Pros:**
- Uses existing HTTP server
- No additional port
- Standard HTTP feature

**Cons:**
- Difficult to implement correctly
- Browser may buffer chunks
- Connection reuse issues
- Not designed for bidirectional communication
- Complex error handling

**Why not chosen:** Abuse of HTTP protocol. WebSocket is purpose-built for this use case.

## Consequences

### Positive
- **Real-time updates:** Messages appear instantly in browser (<100ms latency)
- **Low overhead:** WebSocket frames are tiny compared to HTTP headers
- **Efficient:** Binary protocol with minimal framing overhead
- **Bidirectional:** Could support commands from UI in future (currently unused)
- **Standard protocol:** Works in all modern browsers
- **Dedicated port:** Doesn't interfere with HTTP/REST API traffic
- **Multiple clients:** Up to 3 simultaneous viewers supported

### Negative
- **Additional port:** Requires port 81 open (firewall configuration)
  - Mitigation: Documentation includes port requirements
- **No authentication:** Anyone on network can connect
  - Accepted: Local network trust model (see ADR-003)
- **Browser compatibility:** Requires WebSocket support (IE 10+, all modern browsers)
  - Accepted: Target users have modern browsers
- **Memory per client:** 256 bytes per connection × 3 = 768 bytes
  - Mitigated: Reduced from 512 bytes (was 1,536 bytes total)
- **Connection management:** Must handle disconnections gracefully
  - Implemented: Automatic cleanup on disconnect

### Risks & Mitigation
- **Memory exhaustion:** Too many clients could exhaust heap
  - **Mitigation:** Hard limit of 3 clients, heap-aware connection rejection
  - **Mitigation:** Adaptive throttling reduces message rate when HEAP_LOW
- **Message flood:** High OT traffic could overwhelm clients
  - **Mitigation:** Rate limiting (20 msg/s normal, 5 msg/s HEAP_LOW)
  - **Mitigation:** Drop counter tracks throttled messages
- **Connection leaks:** Clients disconnect without cleanup
  - **Mitigation:** Timeout detection, automatic cleanup
- **Firewall issues:** Port 81 may be blocked
  - **Documentation:** List required ports in README

## Implementation Notes

**Browser compatibility requirements (v1.0.0):**
```javascript
// MANDATORY: Check WebSocket state before sending
if (webSocket && webSocket.readyState === WebSocket.OPEN) {
  webSocket.send(message);
}

// MANDATORY: Handle connection errors
webSocket.onerror = function(error) {
  console.error('WebSocket error:', error);
  // Reconnection logic
};
```

**Server-side patterns:**
```cpp
// Broadcast to all connected clients
void broadcastOTMessage(const char* message) {
  if (ESP.getFreeHeap() < HEAP_CRITICAL) {
    return; // Block sends when heap critical
  }
  webSocket.broadcastTXT(message);
}

// Heap-aware client acceptance
void onWebSocketEvent(...) {
  if (type == WStype_CONNECTED) {
    if (webSocket.connectedClients() >= 3) {
      // Reject: Too many clients
    } else if (ESP.getFreeHeap() < HEAP_LOW) {
      // Reject: Insufficient heap
    } else {
      // Accept connection
    }
  }
}
```

**Message timestamp precision:**
- Uses `micros()` for microsecond resolution
- Format: `HH:MM:SS.mmmmmm` (6 decimal places)
- Timezone-aware via AceTime library

## Browser Compatibility

**Tested browsers:**
- Chrome 16+ ✅
- Firefox 11+ ✅
- Safari 6+ ✅
- Edge (all versions) ✅

**Not supported:**
- Internet Explorer 9 and below ❌

## Related Decisions
- ADR-003: HTTP-Only Network Architecture (explains ws:// vs wss://)
- ADR-004: Static Buffer Allocation Strategy (buffer sizing)
- ADR-010: Multiple Concurrent Network Services (port allocation)
- ADR-025: Safari WebSocket Connection Management (Safari-specific connection pool handling during uploads)

## References
- WebSocket library: https://github.com/Links2004/arduinoWebSockets
- Implementation: `webSocketStuff.ino`
- Web UI integration: `data/index.html` (WebSocket client code)
- Browser compatibility: `docs/BROWSER_COMPATIBILITY_AUDIT_2026.md`
- Heap protection: README.md (v1.0.0 features)
- MDN WebSocket API: https://developer.mozilla.org/en-US/docs/Web/API/WebSocket
