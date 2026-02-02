# WebSocket Logging Guide

## Overview

The OTGW firmware WebUI includes comprehensive WebSocket logging to help debug connection, reconnection, and fallback behavior. All logs are output to the browser console with the `[WebSocket]` prefix for easy filtering.

## How to View Logs

1. Open your browser's Developer Tools (F12 or Right-click → Inspect)
2. Go to the "Console" tab
3. All WebSocket logs will be prefixed with `[WebSocket]`

### Filtering Logs

To see only WebSocket logs, use the console filter:
```
[WebSocket]
```

## Connection Tracking

The WebSocket implementation tracks the following metrics:

- **wsConnectionAttempts** - Total number of connection attempts since page load
- **wsSuccessfulConnections** - Number of successful connections
- **wsReconnectAttempts** - Number of reconnection attempts
- **wsLastConnectTime** - Timestamp of last successful connection
- **wsLastDisconnectTime** - Timestamp of last disconnect
- **wsConnectionDuration** - Duration of last connection in seconds

## Debug Helper Commands

Access these from the browser console:

### Show WebSocket Status
```javascript
otgwDebug.wsStatus()
```
Displays:
- Current connection state (CONNECTING, OPEN, CLOSING, CLOSED)
- WebSocket URL
- Buffer statistics
- **Connection statistics** (attempts, successes, reconnects)
- **Timestamps** (last connect, last disconnect)
- **Durations** (current connection, last connection)
- **Timer status** (reconnect timer, watchdog timer)

### Manually Reconnect
```javascript
otgwDebug.wsReconnect()
```
Disconnects and reconnects the WebSocket.

### Manually Disconnect
```javascript
otgwDebug.wsDisconnect()
```
Disconnects the WebSocket without reconnecting.

## What Gets Logged

### Connection Lifecycle

**Initial Connection:**
```
[WebSocket] ═══════════════════════════════════════
[WebSocket] initOTLogWebSocket called (force: false, flashMode: false)
[WebSocket] Connection stats - Attempts: 0, Successful: 0, Reconnects: 0
[WebSocket] 🔄 Connection attempt #1
[WebSocket] 🔌 CONNECTING to: ws://192.168.1.100:81/
[WebSocket] WebSocket object created, waiting for connection...
```

**Successful Connection:**
```
[WebSocket] ═══════════════════════════════════════
[WebSocket] ✅ CONNECTION ESTABLISHED
[WebSocket] Total successful connections: 1
[WebSocket] Connection time: 2026-02-02T09:22:15.123Z
[WebSocket] ReadyState: 1 (OPEN)
[WebSocket] ═══════════════════════════════════════
```

**Disconnection:**
```
[WebSocket] ═══════════════════════════════════════
[WebSocket] ❌ CONNECTION CLOSED
[WebSocket] Close event code: 1006
[WebSocket] Close reason: No reason provided
[WebSocket] Clean close: false
[WebSocket] Disconnect time: 2026-02-02T09:23:45.789Z
[WebSocket] Connection duration: 90.67 seconds
[WebSocket] ReadyState: 3 (CLOSED)
[WebSocket] ═══════════════════════════════════════
```

**Reconnection:**
```
[WebSocket] 🔄 Scheduling reconnect attempt #1 in 5 seconds...
[WebSocket] initOTLogWebSocket called (force: false, flashMode: false)
[WebSocket] Connection stats - Attempts: 1, Successful: 1, Reconnects: 1
[WebSocket] 🔄 Connection attempt #2
```

### Watchdog Timer

**Normal Operation:**
```
[WebSocket] Watchdog timer reset (timeout: 45s)
[WebSocket] Previous watchdog timer cleared
```

**Timeout:**
```
[WebSocket] ⚠️ WATCHDOG TIMEOUT - No data received for 45s
[WebSocket] Forcing reconnect due to watchdog timeout
[WebSocket] Closing existing connection (readyState: 1)
```

### Fallback Detection

**HTTPS Proxy:**
```
[WebSocket] Display state check: {
  protocol: "https:",
  isProxied: true,
  disabled: true,
  reason: "HTTPS proxy (mixed content)"
}
[WebSocket] ⚠️ FALLBACK: HTTPS reverse proxy detected. WebSocket connections not supported.
```

**Mobile Device:**
```
[WebSocket] Display state check: {
  isPhone: true,
  disabled: true,
  reason: "Phone detected"
}
[WebSocket] ⚠️ FALLBACK: Smartphone detected. Disabling OpenTherm Monitor to save resources.
```

**Small Screen:**
```
[WebSocket] Display state check: {
  screenWidth: 640,
  isSmallScreen: true,
  disabled: true,
  reason: "Small screen"
}
[WebSocket] ⚠️ FALLBACK: Small screen detected (width: 640px). Disabling OpenTherm Monitor.
```

### Message Handling

**Keepalive Messages:**
```
[WebSocket] 💓 Keepalive message received
[WebSocket] Watchdog timer reset (timeout: 45s)
```

**Data Messages:**
```
[WebSocket] 📨 Message received (size: 45 bytes)
[WebSocket] Data: T80120100 / R80120100
[WebSocket] Adding message to log buffer
```

**JSON Messages:**
```
[WebSocket] 📨 Message received (size: 123 bytes)
[WebSocket] Data: {"type":"upgrade","progress":45}
[WebSocket] Message parsed as JSON object
```

## Common Scenarios

### Scenario 1: Connection Drops

Look for:
```
[WebSocket] ❌ CONNECTION CLOSED
[WebSocket] Close event code: 1006
[WebSocket] Clean close: false
```

Code 1006 indicates an abnormal closure (network issue, server crash, etc.)

### Scenario 2: Slow/No Data

Look for:
```
[WebSocket] ⚠️ WATCHDOG TIMEOUT - No data received for 45s
```

This indicates the server isn't sending data (OTGW may be unresponsive)

### Scenario 3: Can't Connect

Look for:
```
[WebSocket] 🔄 Connection attempt #5
[WebSocket] ❌ FAILED TO CREATE WEBSOCKET
```

Multiple failed attempts indicate server is unreachable or port 81 is blocked.

### Scenario 4: HTTPS Reverse Proxy

Look for:
```
[WebSocket] ⚠️ FALLBACK: HTTPS reverse proxy detected.
```

WebSocket (ws://) won't work over HTTPS due to mixed content blocking.

## Close Event Codes

Common WebSocket close codes you may see:

- **1000** - Normal closure
- **1001** - Going away (browser navigating away)
- **1006** - Abnormal closure (connection lost, no close frame)
- **1008** - Policy violation
- **1011** - Server error

## Tips

1. **Filter by prefix** - Use `[WebSocket]` in console filter to see only WebSocket logs
2. **Check statistics** - Run `otgwDebug.wsStatus()` to see connection health
3. **Monitor watchdog** - If you see frequent watchdog timeouts, check OTGW connection
4. **Check close codes** - Code 1006 usually means network issues
5. **Test reconnection** - Use `otgwDebug.wsReconnect()` to test reconnection logic

## Browser Compatibility

The logging works in all modern browsers:
- Chrome/Edge (full support including memory stats)
- Firefox (full support)
- Safari (full support, keepalive messages for ping/pong workaround)

## Performance Impact

The logging has minimal performance impact:
- Only logs to console (not stored in memory)
- No impact on WebSocket data throughput
- Watchdog timer runs independently
- Connection tracking uses < 1KB memory

## Disabling Verbose Logging

If you need to reduce console noise, you can modify the code to comment out specific log lines, but it's recommended to use browser console filtering instead:

1. Open Console
2. Click the filter icon
3. Select "Errors" or "Warnings" only
4. Or use text filter: `-[WebSocket]` to hide WebSocket logs

## Related Documentation

- [WebSocket Protocol](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [Close Event Codes](https://developer.mozilla.org/en-US/docs/Web/API/CloseEvent/code)
- [Browser Console Guide](https://developer.chrome.com/docs/devtools/console/)
