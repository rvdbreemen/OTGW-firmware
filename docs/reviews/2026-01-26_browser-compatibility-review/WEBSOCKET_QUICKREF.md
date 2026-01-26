---
# METADATA
Document Title: WebSocket Robustness - Quick Reference
Review Date: 2026-01-22 00:00:00 UTC
Branch Reviewed: dev → dev (merge commit N/A)
Target Version: v1.0.0-rc4+
Reviewer: GitHub Copilot
Document Type: Quick Reference
PR Branch: dev
Commit: N/A
Status: COMPLETE
---

# WebSocket Robustness - Quick Reference

## What Was Fixed

**Problem**: WebUI stalls after ~20 minutes; Safari/macOS compatibility issues

**Solution**: Added two-layer keepalive mechanism

## Changes Made

### ✅ Server Side (`webSocketStuff.ino`)

1. **WebSocket Heartbeat** - Line ~139
   ```cpp
   webSocket.enableHeartbeat(15000, 3000, 2);
   ```
   - Sends PING every 15 seconds
   - Expects PONG within 3 seconds
   - Disconnects after 2 missed PONGs

2. **Application Keepalive** - Line ~148
   ```cpp
   // Broadcasts {"type":"keepalive"} every 30 seconds
   webSocket.broadcastTXT("{\"type\":\"keepalive\"}");
   ```

### ✅ Client Side (`data/index.js`)

1. **Longer Watchdog** - Line ~140
   ```javascript
   const WS_WATCHDOG_TIMEOUT = 45000; // Increased from 10s to 45s
   ```

2. **Keepalive Handler** - Line ~250
   ```javascript
   // Ignores keepalive messages (doesn't add to log)
   if (event.data.includes('"type":"keepalive"')) return;
   ```

## How to Test

### Quick Test (5 minutes)
```bash
1. Flash firmware
2. Open WebUI in browser
3. Open browser console (F12)
4. Look for: "OT Log WS keepalive received" every ~30s
```

### Full Test (60+ minutes)
```bash
1. Open WebUI
2. Leave running for 1+ hour
3. Verify no stalls or disconnections
4. Check data capture continues
```

### Safari Test
```bash
1. Open in Safari (macOS)
2. Monitor for 30+ minutes
3. Verify stable connection
```

## Expected Console Output

**Normal**:
```
OT Log WebSocket connected
OT Log WS keepalive received
OT Log WS keepalive received
...
```

**Reconnection** (should be rare):
```
OT Log WebSocket disconnected
OT Log WebSocket connected
```

## Troubleshooting

**Still stalling?**
- Check browser console for errors
- Verify keepalive messages arriving
- Try reducing keepalive to 15s (edit KEEPALIVE_INTERVAL_MS)

**Safari not connecting?**
- Check Safari Web Inspector console
- Verify port 81 accessible
- Try with Web Inspector open (timing changes)

**Too many reconnections?**
- Increase watchdog to 60s (edit WS_WATCHDOG_TIMEOUT)
- Check WiFi signal strength
- Monitor router logs

## Performance Impact

**Negligible**:
- 20 bytes every 30 seconds per client
- ~2.4 KB per hour total overhead
- No noticeable CPU/memory impact

## Rollback

To disable if issues occur:

**Server** (`webSocketStuff.ino`):
```cpp
// Comment out:
// webSocket.enableHeartbeat(15000, 3000, 2);
```

**Client** (`data/index.js`):
```javascript
// Change back:
const WS_WATCHDOG_TIMEOUT = 10000;
```

## Technical Summary

**Two-Layer Defense**:
1. **WebSocket Protocol Layer**: PING/PONG (RFC 6455)
2. **Application Layer**: JSON keepalive messages

**Why Both?**
- Protocol layer: Prevents NAT timeout, works on all browsers
- Application layer: Works around Safari bugs, provides visible confirmation

**Timeout Hierarchy**:
- Server PING: every 15s
- Application keepalive: every 30s
- Client watchdog: 45s timeout

## Documentation

- Full analysis: [WEBSOCKET_ROBUSTNESS_ANALYSIS.md](WEBSOCKET_ROBUSTNESS_ANALYSIS.md)
- Implementation details: [WEBSOCKET_IMPROVEMENTS_SUMMARY.md](WEBSOCKET_IMPROVEMENTS_SUMMARY.md)
- This guide: [WEBSOCKET_QUICKREF.md](WEBSOCKET_QUICKREF.md)

## Success Criteria

✅ No stalls during 2+ hour sessions  
✅ Safari compatibility  
✅ Auto-recovery without page reload  
✅ Minimal overhead

---

**Status**: IMPLEMENTED  
**Version**: v1.0.0-rc4+  
**Date**: 2026-01-22
