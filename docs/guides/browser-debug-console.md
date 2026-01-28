# OTGW Firmware - Browser Debug Console

## Overview

The OTGW firmware provides a comprehensive debug helper accessible directly from your browser's JavaScript console. This tool offers similar functionality to the telnet debug interface but works entirely within your web browser, making it ideal for quick diagnostics, testing, and troubleshooting.

## Accessing the Debug Console

1. Open the OTGW Web UI in your browser
2. Press **F12** (or **Ctrl+Shift+I** on Windows/Linux, **Cmd+Option+I** on Mac) to open Developer Tools
3. Click on the **Console** tab
4. You'll see a welcome message:
   ```
   🔧 OTGW Debug Helper Loaded
   Type otgwDebug.help() for available commands
   ```

## Getting Started

Type the following in the console to see all available commands:

```javascript
otgwDebug.help()
```

This displays a formatted help menu with all available debug functions.

## Available Commands

### 📊 Status Information

#### `otgwDebug.status()`
Shows current system status including:
- Flash mode state
- Page visibility
- Timer status (auto-refresh and time update)
- WebSocket connection state

**Example:**
```javascript
otgwDebug.status()
// Output:
// 📊 OTGW System Status
//   Flash Mode Active: false
//   Page Visible: true
//   Auto Refresh Timer: Running
//   Time Update Timer: Running
//   WebSocket Status: 1
```

#### `otgwDebug.info()`
Fetches and displays device information from the API in a formatted table.

**Example:**
```javascript
otgwDebug.info()
// Shows device information like hostname, IP address, firmware version, etc.
```

#### `otgwDebug.settings()`
Fetches and displays all current settings in a formatted table.

**Example:**
```javascript
otgwDebug.settings()
// Shows all OTGW settings like MQTT broker, hostname, GPIO pins, etc.
```

---

### 🔌 WebSocket & Connections

#### `otgwDebug.wsStatus()`
Shows detailed WebSocket connection information including:
- Connection state (CONNECTING, OPEN, CLOSING, CLOSED)
- WebSocket URL
- Number of buffered log lines
- Auto-scroll setting
- Maximum log lines setting

**Example:**
```javascript
otgwDebug.wsStatus()
// Output:
// 🔌 WebSocket Status
//   State: OPEN
//   URL: ws://192.168.1.100/otlog
//   Buffered Lines: 1523
//   Auto Scroll: true
//   Max Log Lines: 2000
```

#### `otgwDebug.wsReconnect()`
Disconnects and reconnects the WebSocket connection. Useful when the connection becomes stale or unresponsive.

**Example:**
```javascript
otgwDebug.wsReconnect()
// Output: 🔄 Reconnecting WebSocket...
```

#### `otgwDebug.wsDisconnect()`
Disconnects the WebSocket connection without reconnecting.

**Example:**
```javascript
otgwDebug.wsDisconnect()
// Output: 🔌 Disconnecting WebSocket...
```

---

### 🔍 Data Inspection

#### `otgwDebug.otmonitor()`
Displays the current OpenTherm monitor data in a formatted table. Shows all active OpenTherm message IDs and their values.

**Example:**
```javascript
otgwDebug.otmonitor()
// Shows table with all OT monitor values (temperatures, status flags, etc.)
```

#### `otgwDebug.logs(lines)`
Shows the most recent log lines from the buffer. Default is 50 lines, but you can specify any number.

**Examples:**
```javascript
otgwDebug.logs()      // Shows last 50 lines
otgwDebug.logs(100)   // Shows last 100 lines
otgwDebug.logs(10)    // Shows last 10 lines
```

#### `otgwDebug.clearLogs()`
Clears all buffered log lines from memory.

**Example:**
```javascript
otgwDebug.clearLogs()
// Output: ✅ Log buffers cleared
```

---

### ⚙️ API Testing

#### `otgwDebug.api(endpoint)`
Tests any API endpoint and displays the response. Automatically handles JSON and text responses.

**Examples:**
```javascript
otgwDebug.api("v1/devinfo")
otgwDebug.api("v2/otgw/otmonitor")
otgwDebug.api("v1/health")
otgwDebug.api("v0/settings")
```

**Output:**
```
🌐 Fetching: http://192.168.1.100/api/v1/devinfo
✅ Response from v1/devinfo
  Status: 200
  Data: {devinfo: Array(15)}
```

#### `otgwDebug.health()`
Quick shortcut to check system health status. Returns system status, PIC availability, and other vital signs.

**Example:**
```javascript
otgwDebug.health()
// Returns: {status: "UP", picavailable: true, ...}
```

#### `otgwDebug.sendCmd(command)`
Sends an OTGW command to the device via the REST API. Use the same command format as in OTmonitor or telnet.

**Examples:**
```javascript
otgwDebug.sendCmd("PS=1")     // Print Summary
otgwDebug.sendCmd("PR=A")     // Print Report
otgwDebug.sendCmd("TT=20.5")  // Set temporary temperature override
otgwDebug.sendCmd("TC=21")    // Set constant temperature override
otgwDebug.sendCmd("SC=70")    // Set DHW setpoint to 70°C
```

**Output:**
```
📤 Sending command: PS=1
✅ Command response: {status: "OK", value: "..."}
```

---

### 💾 Data Export

#### `otgwDebug.exportLogs()`
Downloads all buffered log lines as a text file. The filename includes a timestamp.

**Example:**
```javascript
otgwDebug.exportLogs()
// Downloads: otgw-logs-2026-01-26T10-30-45-123Z.txt
// Output: ✅ Exported 1523 log lines
```

#### `otgwDebug.exportData()`
Downloads the current OpenTherm monitor data as a JSON file. Useful for analysis or sharing.

**Example:**
```javascript
otgwDebug.exportData()
// Downloads: otgw-data-2026-01-26T10-30-45-123Z.json
// Output: ✅ Data exported as JSON
```

---

### 🐛 Debug Toggles

#### `otgwDebug.verbose`
Property to enable/disable verbose logging. Currently prepared for future use.

**Example:**
```javascript
otgwDebug.verbose = true   // Enable verbose mode
otgwDebug.verbose = false  // Disable verbose mode
```

---

## Common Use Cases

### Troubleshooting Connection Issues

```javascript
// Check overall system status
otgwDebug.status()

// Check WebSocket specifically
otgwDebug.wsStatus()

// If WebSocket is stuck, reconnect it
otgwDebug.wsReconnect()

// Verify device is responding
otgwDebug.health()
```

### Testing OTGW Commands

```javascript
// Send a Print Summary command
otgwDebug.sendCmd("PS=1")

// Set a temporary temperature override
otgwDebug.sendCmd("TT=21.5")

// Check the result in the logs
otgwDebug.logs(20)
```

### Monitoring OpenTherm Data

```javascript
// View current OT monitor data
otgwDebug.otmonitor()

// Export data for analysis
otgwDebug.exportData()

// View recent log activity
otgwDebug.logs(50)
```

### API Endpoint Testing

```javascript
// Test different API versions
otgwDebug.api("v0/devinfo")
otgwDebug.api("v1/devinfo")
otgwDebug.api("v2/otgw/otmonitor")

// Check settings
otgwDebug.api("v0/settings")
```

### Capturing Diagnostic Data

```javascript
// Export current logs
otgwDebug.exportLogs()

// Export OT data
otgwDebug.exportData()

// Get device info
otgwDebug.info()

// Get settings
otgwDebug.settings()
```

---

## Comparison with Telnet Debug

The browser console debug helper provides similar functionality to the telnet debug interface (port 23) but with some key differences:

| Feature | Telnet Debug | Browser Console |
|---------|--------------|-----------------|
| Access | Requires telnet client | Built into browser |
| Help Menu | `h` key | `otgwDebug.help()` |
| Status | Single character commands | Named functions |
| Data Export | Copy/paste | Direct file download |
| API Testing | Manual curl/wget | Built-in functions |
| Log Viewing | Real-time stream | Buffered with filtering |
| WebSocket Control | N/A | Full control |
| Learning Curve | Minimal (single keys) | Moderate (function names) |
| Scriptable | Limited | Fully scriptable |

**When to use Telnet Debug:**
- Quick, keyboard-only interaction
- Real-time streaming of all debug output
- Toggling debug flags (OTmsg, RestAPI, MQTT, Sensors)
- Hardware testing (LED blink, GPIO control)
- PIC reset and firmware commands

**When to use Browser Console:**
- API endpoint testing
- Data export and analysis
- WebSocket troubleshooting
- Settings and info inspection
- JavaScript-based automation
- No telnet client available

---

## Advanced Usage

### Scripting and Automation

You can create custom scripts in the browser console:

```javascript
// Poll health status every 5 seconds
let healthMonitor = setInterval(async () => {
  let health = await otgwDebug.health();
  console.log('Health check:', health.status);
}, 5000);

// Stop monitoring
clearInterval(healthMonitor);
```

```javascript
// Export logs and data together
async function exportAll() {
  otgwDebug.exportLogs();
  await new Promise(resolve => setTimeout(resolve, 1000));
  otgwDebug.exportData();
  console.log('✅ All data exported');
}
exportAll();
```

### Continuous Monitoring

```javascript
// Monitor WebSocket state
let wsMonitor = setInterval(() => {
  if (otLogWS && otLogWS.readyState !== WebSocket.OPEN) {
    console.warn('⚠️  WebSocket not open, attempting reconnect...');
    otgwDebug.wsReconnect();
  }
}, 10000);

// Stop monitoring
clearInterval(wsMonitor);
```

---

## Tips and Tricks

1. **Quick Access**: Create a browser bookmark with `javascript:otgwDebug.help()` to quickly show the help menu

2. **Command History**: Use the **Up Arrow** key in the console to recall previous commands

3. **Tab Completion**: Type `otgwDebug.` and press **Tab** to see all available functions

4. **Copy Output**: Right-click on any console output and select "Copy" to copy data

5. **Filter Logs**: Use browser console's built-in filter box to search log output

6. **Persist Logs**: Enable "Preserve log" in console settings to keep logs across page refreshes

7. **Mobile Access**: The debug console works on mobile browsers too (though the interface is smaller)

8. **Multiple Tabs**: Open multiple browser tabs to the OTGW UI - each has its own debug console instance

---

## Troubleshooting

### Debug helper not available

**Problem**: `otgwDebug is not defined`

**Solution**: 
- Ensure you're on a page with the OTGW Web UI loaded
- Refresh the page (Ctrl+F5 for hard refresh)
- Check that index.js loaded successfully in the Network tab

### API calls fail

**Problem**: API calls return errors or timeout

**Solution**:
```javascript
// Check device is reachable
otgwDebug.health()

// Check network in DevTools Network tab
// Verify API URL is correct
console.log(APIGW)
```

### WebSocket won't connect

**Problem**: WebSocket stays in CONNECTING or CLOSED state

**Solution**:
```javascript
// Check current state
otgwDebug.wsStatus()

// Try reconnecting
otgwDebug.wsReconnect()

// If that fails, check if flash mode is active
console.log('Flash mode:', flashModeActive)
```

### Export functions don't work

**Problem**: Export functions don't download files

**Solution**:
- Check browser pop-up blocker settings
- Ensure browser allows downloads
- Try manually: `console.save(otgwDebug.logs(), 'logs.txt')`

---

## See Also

- [Telnet Debug Interface](../handleDebug.ino) - Server-side debug menu
- [REST API Documentation](../README.md#rest-api) - API endpoint reference
- [MQTT Commands](../README.md#mqtt) - MQTT command reference
- [WebSocket Implementation](../webSocketStuff.ino) - WebSocket source code

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-26 | Initial browser debug console implementation |

---

*This documentation is part of the OTGW-firmware project.*  
*For more information, visit: https://github.com/rvdbreemen/OTGW-firmware*
