# ADR-025: Safari WebSocket Connection Management During Firmware Upload

**Status:** Accepted  
**Date:** 2026-01-29  
**Supersedes:** N/A

## Context

Users reported that the Web UI progress bar stopped working during firmware uploads in Safari, while the same code worked correctly in Chrome and Firefox. Analysis revealed that Safari would drop WebSocket connections during large file uploads, leaving users without progress feedback.

**Problem manifestation:**
- WebSocket connected successfully before upload
- User clicks "Flash Firmware" and starts upload
- WebSocket silently disconnected mid-upload
- No close or error events fired
- Log messages visible before and after flash, but not during
- Progress bar frozen or showing 0%

**Root cause analysis:**
- Safari has ~6 total connection limit per domain
- Effective limit for persistent connections: **~3 connections**
- Large XHR upload (1-2 MB firmware file) competes for connection slots
- Safari prioritizes active data transfer (XHR) over idle persistent connections (WebSocket)
- WebSocket dropped without firing proper close/error events
- Same issue occurs with iCloud Private Relay enabled (Safari 26+ regression)

**Requirements:**
- Progress feedback must work reliably in Safari during uploads
- Solution must work in Chrome and Firefox without regression
- No server-side backend changes (maintain simplicity)
- Must handle Safari connection pool exhaustion gracefully

## Decision

**Proactively close WebSocket before upload starts, rely entirely on HTTP polling during flash operations.**

**Implementation:**
```javascript
// Track WebSocket instance globally
var wsInstance = null;

// Close before upload to prevent Safari from dropping it
function closeWebSocketForUpload() {
  if (wsInstance && wsInstance.readyState !== WebSocket.CLOSED) {
    console.log('Safari: Closing WebSocket before upload to avoid resource contention');
    wsInstance.close();
    wsInstance = null;
  }
}

// Before starting upload
closeWebSocketForUpload();
if (!pollActive) {
  console.log('Safari: Activating polling before upload');
  flashPollingActivated = true;
  startPolling();
}
```

**Key design principles:**
- **Proactive management:** Close WebSocket before it gets dropped
- **Explicit lifecycle:** Track instance with `wsInstance` variable
- **Polling fallback:** Activate HTTP polling immediately when WebSocket closes
- **Universal solution:** Works in all browsers, not Safari-specific hacks

## Alternatives Considered

### Alternative 1: Keep WebSocket Open + Polling Redundancy
**Approach:** Run both WebSocket and polling simultaneously during upload.

**Pros:**
- WebSocket might survive in Chrome/Firefox
- Polling provides backup if WebSocket fails
- No need to explicitly close WebSocket

**Cons:**
- Wastes connection slot in Safari (WebSocket still gets dropped)
- Increases server load (duplicate progress mechanisms)
- More complex code path with two parallel systems
- Doesn't solve the root cause

**Why not chosen:** Doesn't prevent Safari from dropping WebSocket; just adds complexity without fixing the problem.

### Alternative 2: Detect Browser and Apply Safari-Specific Logic
**Approach:** Check `navigator.userAgent` for Safari, apply close logic only for Safari.

**Pros:**
- Chrome/Firefox keep WebSocket open
- Targeted fix for Safari only

**Cons:**
- User-agent detection is fragile and unreliable
- Safari can be spoofed or change UA string
- Adds browser-specific code branches
- Violates progressive enhancement principles
- Doesn't handle Safari-like browsers (WebKit-based)

**Why not chosen:** Browser detection is an anti-pattern. Better to use a universal solution that works everywhere.

### Alternative 3: Increase Connection Limits (Server-Side)
**Approach:** Configure server to allow more concurrent connections.

**Pros:**
- Might prevent Safari from dropping WebSocket

**Cons:**
- **Not possible:** Connection limit is browser-imposed, not server-imposed
- Cannot be changed via HTTP headers or server configuration
- Would require user to change browser settings (not practical)
- Doesn't address the fundamental resource contention

**Why not chosen:** Not technically feasible. Browser connection limits are hard-coded and cannot be changed from server.

### Alternative 4: WebSocket Connection Timeout + Automatic Reconnect
**Approach:** Detect when WebSocket is unresponsive, reconnect automatically.

**Pros:**
- Handles silent disconnections
- Works for any failure mode
- Automatic recovery

**Cons:**
- Adds latency (timeout detection delay)
- Complex reconnection logic
- Doesn't prevent the initial drop
- Multiple reconnect attempts waste resources
- Still leaves progress gap during detection/reconnect

**Why not chosen:** Reactive solution that adds complexity. Proactive close is simpler and more reliable.

## Consequences

### Positive
- **Reliable progress in Safari:** Users always see progress bar updates
- **Universal solution:** Works identically in Chrome, Firefox, Safari, Edge
- **Eliminates silent failures:** No more mystery WebSocket drops
- **Simple implementation:** Clear, explicit lifecycle management
- **No server changes:** Client-only solution maintains backend simplicity
- **Works with iCloud Private Relay:** Avoids Safari 26+ WebSocket handshake bug

### Negative
- **WebSocket not used during upload:** Lose real-time streaming during flash
  - **Accepted:** HTTP polling provides adequate progress feedback (500ms intervals)
- **Additional 20 bytes RAM:** wsInstance tracking variable
  - **Accepted:** Negligible memory cost for major reliability improvement

### Risks & Mitigation
- **Polling may fail:** Network errors during polling could break progress
  - **Mitigation:** Existing error handling with retries and exponential backoff
  - **Mitigation:** XHR.upload.onprogress provides upload phase progress without polling
- **WebSocket doesn't reconnect:** After flash, need to re-establish WebSocket
  - **Mitigation:** Already implemented auto-reconnect with exponential backoff
- **Race condition:** WebSocket close and upload start timing
  - **Mitigation:** Synchronous close before XHR send() call

## Implementation Notes

**File modified:** `updateServerHtml.h`

**WebSocket lifecycle:**
```javascript
// Setup: Track instance
function startWebSocket(uri) {
  wsInstance = new WebSocket(uri);
  wsInstance.onopen = function() { /* ... */ };
  wsInstance.onmessage = function() { /* ... */ };
  wsInstance.onerror = function() { wsInstance = null; };
  wsInstance.onclose = function() { wsInstance = null; };
}

// Cleanup: Explicit close
function closeWebSocketForUpload() {
  if (wsInstance && wsInstance.readyState !== WebSocket.CLOSED) {
    wsInstance.close();
    wsInstance = null;
  }
}
```

**Progress architecture:**
- **Upload phase (0-100%):** `XHR.upload.onprogress` (browser native)
- **Flash phase (write to flash):** HTTP polling to `/status` (500ms intervals)
- **WebSocket:** Disconnected during entire process, reconnects after reboot

**Multi-layer fallback system:**
1. **WebSocket Primary** - Real-time updates via ws://device:81/
2. **Auto-Reconnect** - Exponential backoff (1s → 2s → 4s → 8s → 10s max)
3. **Adaptive Watchdog** - Activates polling after 5s silence during flash
4. **Proactive Polling** - Safari fix: Close WebSocket, activate polling before upload
5. **Dual-Mode** - Both WebSocket and polling can be active (future-proof)

## Browser Compatibility

| Browser | Before Fix | After Fix | Status |
|---------|-----------|----------|--------|
| **Safari (macOS)** | ❌ Broken | ✅ Works | Fixed |
| **Safari (iOS)** | ❌ Broken | ✅ Works | Fixed |
| Chrome | ✅ Works | ✅ Works | No regression |
| Firefox | ✅ Works | ✅ Works | No regression |
| Edge | ✅ Works | ✅ Works | No regression |

**iCloud Private Relay support:**
- Safari 26+ has WebSocket handshake bug when iCloud Private Relay is enabled
- This solution bypasses the issue by not using WebSocket during upload
- Polling works correctly through iCloud Private Relay

## Testing

**Safari console output (successful):**
```
WebSocket connected successfully
[User clicks "Flash Firmware"]
Safari: Closing WebSocket before upload to avoid resource contention
Safari: Activating polling before upload
Upload progress: 524288 / 1048576 bytes (XHR native)
Poll #1 - Status check (HTTP polling)
Poll #2 - Status check
Flash write progress: 45%
Flash complete
WebSocket reconnected (after reboot)
```

**Network traffic analysis:**
- Before upload: 1 WebSocket connection on port 81
- During upload: 0 WebSocket, 1 XHR upload, periodic GET /status (polling)
- After reboot: WebSocket reconnects automatically

## Related Decisions
- **ADR-005:** WebSocket for Real-Time Streaming (original WebSocket architecture)
- **ADR-010:** Multiple Concurrent Network Services (port allocation, connection management)
- **ADR-023:** File System Explorer HTTP Architecture (firmware upload mechanism)

## References
- **Pull Request:** #394 (Safari WebSocket resource contention fix)
- **Implementation:** `updateServerHtml.h` lines 280-290 (closeWebSocketForUpload)
- **Documentation:** `docs/SAFARI_FLASH_FIX.md`
- **Safari bug reports:** WebKit Bug Tracker (connection pool limits, iCloud Private Relay)
- **Research:** Safari connection limits documented ~6 total, ~3 persistent
- **Testing:** Verified on Safari 26 (macOS), Safari 26 (iOS), Chrome 120, Firefox 121
