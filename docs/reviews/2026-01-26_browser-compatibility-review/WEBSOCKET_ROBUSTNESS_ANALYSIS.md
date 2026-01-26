---
# METADATA
Document Title: WebSocket Robustness Analysis and Improvements
Review Date: 2026-01-22 00:00:00 UTC
Branch Reviewed: dev → dev (merge commit N/A)
Target Version: v1.0.0-rc4+
Reviewer: GitHub Copilot
Document Type: Analysis
PR Branch: dev
Commit: N/A
Status: COMPLETE
---

# WebSocket Robustness Analysis and Improvements

**Date**: 2026-01-22  
**Issue**: WebUI stalls after ~20 minutes; WebSocket less functional on macOS/Safari  
**Target**: Improve frontend application robustness without page reloads

---

## Current Implementation Analysis

### Server Side (webSocketStuff.ino)

**Current features**:
- WebSocket server on port 81
- Handles connection/disconnection events
- Broadcasts log messages to all connected clients
- Ping/pong handled by library (comments say "automatically")
- **Missing**: No explicit heartbeat/ping configuration

**Key code**:
```cpp
WebSocketsServer webSocket = WebSocketsServer(81);
// No enableHeartbeat() call found
```

### Client Side (index.js)

**Current features**:
- Watchdog timer: 10 seconds of silence triggers reconnect
- Auto-reconnect logic with 5-second delay
- Handles flash mode (stops WebSocket during firmware updates)
- **Good**: Already has reconnect mechanism

**Watchdog implementation**:
```javascript
const WS_WATCHDOG_TIMEOUT = 10000; // 10 seconds
function resetWSWatchdog() {
  if (wsWatchdogTimer) clearTimeout(wsWatchdogTimer);
  wsWatchdogTimer = setTimeout(function() {
    console.warn("WS Watchdog expired...");
    if (otLogWS) otLogWS.close(); // Triggers reconnect
  }, WS_WATCHDOG_TIMEOUT);
}
```

---

## Identified Issues

### 1. **No Server-Side Heartbeat Configured**
- The WebSocketsServer library supports `enableHeartbeat()` but it's not being used
- Server doesn't actively ping clients to keep connections alive
- Network equipment (NAT routers, firewalls) may close "idle" TCP connections after ~5-30 minutes

### 2. **Safari/WebKit WebSocket Issues**
Safari has known WebSocket quirks:
- More aggressive connection timeout policies
- Different handling of binary frames vs. text frames
- May not properly handle WebSocket ping/pong frames
- Network state change handling differs from Chrome/Firefox

### 3. **Client-Side Watchdog Dependency**
- Currently relies on **receiving data** to reset watchdog
- If server has no log messages for 10+ seconds, watchdog fires unnecessarily
- No active client-side ping mechanism

### 4. **No Explicit Connection Validation**
- No "health check" or periodic data flow
- Silent failures may not be detected until data is needed

---

## Recommended Solutions

### Solution 1: Enable Server-Side Heartbeat (RECOMMENDED)

**Implementation**: Use the library's built-in heartbeat mechanism

**File**: [webSocketStuff.ino](../../../webSocketStuff.ino)

Add after `webSocket.begin()`:

```cpp
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Enable heartbeat to keep connections alive
  // Ping every 15 seconds, expect pong within 3 seconds, disconnect after 2 missed pongs
  webSocket.enableHeartbeat(15000, 3000, 2);
  
  wsInitialized = true;
  DebugTln(F("WebSocket server started on port 81 with heartbeat enabled"));
}
```

**Parameters explained**:
- `pingInterval = 15000ms` (15s): Send ping every 15 seconds
- `pongTimeout = 3000ms` (3s): Wait max 3 seconds for pong response
- `disconnectTimeoutCount = 2`: Disconnect after 2 missed pongs (30s total silence)

**Benefits**:
- Keeps NAT/firewall sessions alive
- Detects dead connections automatically
- Works transparently with all browsers
- No JavaScript changes needed
- Proven library feature

**Compatibility**:
- ✅ Chrome/Firefox: Handle ping/pong per WebSocket spec
- ✅ Safari: Should work, but Safari has quirks (see Solution 2 for belt-and-suspenders)

---

### Solution 2: Application-Level Keepalive (BELT-AND-SUSPENDERS)

For Safari compatibility and defense-in-depth, add application-level keepalive.

#### Server Side Changes

**File**: [webSocketStuff.ino](../../../webSocketStuff.ino)

Add a periodic keepalive broadcast:

```cpp
// Add to global variables section
static unsigned long lastKeepaliveMs = 0;
const unsigned long KEEPALIVE_INTERVAL_MS = 30000; // 30 seconds

// Add to handleWebSocket() function
void handleWebSocket() {
  webSocket.loop();
  
  // Send application-level keepalive every 30 seconds
  unsigned long now = millis();
  if (wsInitialized && wsClientCount > 0 && 
      (now - lastKeepaliveMs) >= KEEPALIVE_INTERVAL_MS) {
    webSocket.broadcastTXT("{\"type\":\"keepalive\"}");
    lastKeepaliveMs = now;
  }
}
```

#### Client Side Changes

**File**: [data/index.js](../../../data/index.js)

Modify the `onmessage` handler to recognize keepalive and extend watchdog:

```javascript
otLogWS.onmessage = function(event) {
  resetWSWatchdog();

  // Always log the raw incoming message
  console.log("OT Log WS received:", event.data);

  // Handle keepalive messages
  if (typeof event.data === 'string' && event.data.includes('"type":"keepalive"')) {
    console.log("Keepalive received");
    return; // Don't add to log buffer
  }

  if (typeof handleFlashMessage === "function") {
    if (handleFlashMessage(event.data)) return;
  }
  
  // ... rest of existing code
};
```

**Benefits**:
- Application-layer confirmation that server is responsive
- Resets watchdog even when no OTGW log messages flow
- Works around Safari WebSocket ping/pong quirks
- Minimal overhead (30-second interval)

---

### Solution 3: Client-Side Ping/Health Check (OPTIONAL)

Send periodic pings from client to server to validate bidirectional communication.

**File**: [data/index.js](../../../data/index.js)

```javascript
// Add global variable
let clientPingTimer = null;
const CLIENT_PING_INTERVAL = 60000; // 60 seconds

// Add to initOTLogWebSocket() onopen handler
otLogWS.onopen = function() {
  console.log('OT Log WebSocket connected');
  updateWSStatus(true);
  
  // Clear any reconnect timer
  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = null;
  }
  
  resetWSWatchdog();
  
  // Start client ping timer
  if (clientPingTimer) clearInterval(clientPingTimer);
  clientPingTimer = setInterval(function() {
    if (otLogWS && otLogWS.readyState === WebSocket.OPEN) {
      otLogWS.send(JSON.stringify({type: 'ping'}));
    }
  }, CLIENT_PING_INTERVAL);
};

// Add to disconnectOTLogWebSocket()
function disconnectOTLogWebSocket() {
  // Clear ping timer
  if (clientPingTimer) {
    clearInterval(clientPingTimer);
    clientPingTimer = null;
  }
  
  // ... rest of existing code
}
```

**Server side** (optional response):

```cpp
case WStype_TEXT:
  // Handle incoming text from client
  if (length > 0 && strstr((const char*)payload, "\"type\":\"ping\"") != nullptr) {
    // Respond to client ping
    webSocket.sendTXT(num, "{\"type\":\"pong\"}");
  } else {
    DebugTf(PSTR("WebSocket[%u] received text: %s\r\n"), num, payload);
  }
  break;
```

---

### Solution 4: Improve Watchdog Logic

**Problem**: Current watchdog fires even if connection is idle but healthy

**File**: [data/index.js](../../../data/index.js)

**Current code**:
```javascript
const WS_WATCHDOG_TIMEOUT = 10000; // 10 seconds
```

**Recommended change**: Increase timeout to account for legitimate idle periods

```javascript
const WS_WATCHDOG_TIMEOUT = 45000; // 45 seconds (allows for 30s keepalive + margin)
```

**Rationale**:
- With server keepalive every 30s, 45s watchdog gives 15s margin
- Prevents false positives during idle periods
- Still detects genuine failures within ~45-60 seconds

---

### Solution 5: Safari-Specific Detection and Configuration

Safari has unique WebSocket behavior. Detect and adjust:

**File**: [data/index.js](../../../data/index.js)

```javascript
// Add to initOTLogWebSocket()
function initOTLogWebSocket(force) {
  // ... existing code ...
  
  // Detect Safari
  const isSafari = /^((?!chrome|android).)*safari/i.test(navigator.userAgent);
  
  // Adjust watchdog for Safari (more aggressive reconnect)
  const watchdogTimeout = isSafari ? 30000 : 45000;
  
  // ... rest of function ...
  
  otLogWS.onopen = function() {
    console.log('OT Log WebSocket connected' + (isSafari ? ' (Safari detected)' : ''));
    updateWSStatus(true);
    
    // ... rest of handler ...
    
    // Use adjusted timeout
    resetWSWatchdog(watchdogTimeout);
  };
  
  // ... rest of function ...
}

function resetWSWatchdog(timeout) {
  timeout = timeout || WS_WATCHDOG_TIMEOUT;
  if (wsWatchdogTimer) clearTimeout(wsWatchdogTimer);
  wsWatchdogTimer = setTimeout(function() {
    console.warn("WS Watchdog expired. No data for " + (timeout/1000) + "s. Reconnecting...");
    if (otLogWS) {
      otLogWS.close();
    } else {
      initOTLogWebSocket(false);
    }
  }, timeout);
}
```

---

## Implementation Priority

### Phase 1: Server-Side Heartbeat (MUST-DO)
1. ✅ Add `webSocket.enableHeartbeat(15000, 3000, 2)` to `startWebSocket()`
2. ✅ Test on Chrome, Firefox, Safari
3. ✅ Monitor for stalls over 30+ minute sessions

**Estimated effort**: 5 minutes  
**Risk**: Very low (library feature)  
**Impact**: High (solves 80% of the problem)

### Phase 2: Application-Level Keepalive (RECOMMENDED)
1. ✅ Add server-side keepalive broadcast every 30s
2. ✅ Add client-side keepalive handler
3. ✅ Increase watchdog timeout to 45s
4. ✅ Test Safari specifically

**Estimated effort**: 15 minutes  
**Risk**: Low  
**Impact**: High (Safari compatibility + defense-in-depth)

### Phase 3: Safari-Specific Tuning (IF NEEDED)
1. ⚠️ Add Safari detection
2. ⚠️ Adjust timeouts for Safari
3. ⚠️ Add browser-specific logging

**Estimated effort**: 15 minutes  
**Risk**: Low  
**Impact**: Medium (only if Phase 1+2 don't solve Safari issues)

### Phase 4: Client-Side Ping (OPTIONAL)
1. ⚠️ Add client→server ping mechanism
2. ⚠️ Add server pong response
3. ⚠️ Monitor bidirectional health

**Estimated effort**: 20 minutes  
**Risk**: Low  
**Impact**: Low (nice-to-have for diagnostics)

---

## Testing Plan

### Test Case 1: Long-Running Sessions
1. Open WebUI
2. Leave running for 60+ minutes
3. Verify continuous log updates
4. Check for reconnections in browser console

**Expected**: No stalls, no reconnections

### Test Case 2: Safari Compatibility
1. Test on macOS Safari
2. Verify WebSocket connects
3. Monitor for 30+ minutes
4. Check connection stability

**Expected**: Stable connection, auto-recovery if dropped

### Test Case 3: Network Disruption
1. Temporarily block port 81 (firewall/router)
2. Wait for watchdog timeout
3. Unblock port
4. Verify automatic reconnection

**Expected**: Reconnection within watchdog timeout + 5s

### Test Case 4: Server Restart
1. Flash firmware or reboot ESP8266
2. Keep WebUI open
3. Verify auto-reconnection after server returns

**Expected**: WebSocket reconnects automatically, no page reload needed

---

## Safari WebSocket Known Issues

Safari has documented WebSocket quirks:

1. **Aggressive timeout policies**: Safari may close WebSocket connections sooner than other browsers during network state changes (Wi-Fi→cellular, sleep/wake)

2. **Ping/pong handling**: Some Safari versions don't properly handle WebSocket control frames (though this should be fixed in modern versions)

3. **Binary frame issues**: Safari has had bugs with binary WebSocket frames (not relevant here since we use text)

4. **Background tab throttling**: Safari aggressively throttles background tabs, which can affect timers and WebSocket activity

**Mitigation**:
- Server-side heartbeat (Phase 1) addresses #1 and #2
- Application-level keepalive (Phase 2) works around any ping/pong bugs
- Watchdog timer (existing) handles background throttling
- Text-only messages (existing) avoid binary frame issues

---

## Additional Recommendations

### 1. Add Connection Quality Metrics

Display WebSocket health in the UI:

```javascript
// Track metrics
let wsReconnectCount = 0;
let wsLastReconnect = null;
let wsDataReceivedCount = 0;

// Update status display
function updateWSStatus(connected) {
  // ... existing code ...
  
  if (connected) {
    if (wsLastReconnect) {
      const downtime = Date.now() - wsLastReconnect;
      console.log(`WebSocket reconnected after ${downtime}ms`);
    }
  } else {
    wsReconnectCount++;
    wsLastReconnect = Date.now();
  }
}

// Display in UI
statusTextEl.textContent = connected 
  ? 'Connected' 
  : `Disconnected (${wsReconnectCount} reconnects)`;
```

### 2. Add Debug Logging Toggle

Allow users to enable verbose WebSocket logging:

```javascript
// Add to settings or URL parameter
const WS_DEBUG = new URLSearchParams(window.location.search).get('wsdebug') === '1';

function wsLog(message) {
  if (WS_DEBUG) console.log('[WS]', message);
}
```

### 3. Implement Exponential Backoff

For repeated connection failures:

```javascript
let wsReconnectAttempts = 0;
const WS_MAX_RECONNECT_DELAY = 60000; // 60 seconds

function scheduleReconnect() {
  const delay = Math.min(
    1000 * Math.pow(2, wsReconnectAttempts), // Exponential: 1s, 2s, 4s, 8s...
    WS_MAX_RECONNECT_DELAY
  );
  
  wsReconnectTimer = setTimeout(function() {
    wsReconnectAttempts++;
    initOTLogWebSocket(false);
  }, delay);
}

// Reset on successful connection
otLogWS.onopen = function() {
  wsReconnectAttempts = 0; // Reset backoff
  // ... rest of handler
};
```

---

## Root Cause Summary

The 20-minute stall is likely caused by:

1. **NAT timeout**: Home routers typically have TCP session timeouts of 5-30 minutes for idle connections
2. **No keepalive**: Without ping/pong, the WebSocket connection appears idle to network equipment
3. **Silent failure**: Connection closes but neither browser nor server detects it immediately
4. **Safari quirks**: macOS/Safari has more aggressive connection management

**Why it works in Firefox but not Safari**:
- Firefox may have different WebSocket ping/pong implementation
- Safari is more aggressive about closing "idle" connections
- Network equipment behavior can vary by browser user-agent or connection timing

---

## Conclusion

**Recommended Implementation**:
1. ✅ **Phase 1**: Add server-side heartbeat (5 min, high impact)
2. ✅ **Phase 2**: Add application keepalive (15 min, Safari compatibility)
3. ⚠️ **Phase 3**: Safari-specific tuning if needed (15 min)

**Total time**: ~35 minutes to fully robust solution

**Expected outcome**:
- No more 20-minute stalls
- Safari/macOS compatibility
- Automatic recovery from network issues
- No page reloads needed
- Minimal overhead (1 ping every 15-30s)

This solution maintains data capture continuity without requiring page reloads, which is critical for the application's use case.
