# PIC Flash Debugging Guide

**Date**: 2026-01-25  
**Issue**: PIC flashing not starting despite fixes  
**Debugging Session**: Step-by-step guide

---

## Quick Diagnosis

### Symptom
PIC flashing doesn't start at all - no progress, no activity.

### Hypothesis
The `httpServer.client().stop()` call in my fix may be **interfering with ESP8266WebServer's internal connection management**, preventing the handler from completing properly.

---

## Step-by-Step Debugging Instructions

### Phase 1: Web UI Console Monitoring

**Open Browser Developer Tools**:
1. Open web UI: `http://<device-ip>/`
2. Press F12 to open Developer Tools
3. Go to "Console" tab
4. Go to "Network" tab

**Trigger Flash and Monitor**:
1. Navigate to PIC Flash tab
2. Select a firmware file
3. Click "Flash" button
4. **Watch Console for**:
   - JavaScript errors
   - Fetch API errors
   - WebSocket connection status
   - Any timeout messages

**Expected Console Output (Working)**:
```javascript
Flash started: {status: "started"}
WebSocket connected
Progress update: {"state":"start", ...}
Progress update: {"state":"write", "flash_written":25, ...}
```

**Actual Console Output (If Broken)**:
```javascript
// Likely one of these:
TypeError: Cannot read property 'readyState' of undefined
Failed to fetch: net::ERR_CONNECTION_RESET
WebSocket connection closed unexpectedly
```

**Network Tab Analysis**:
1. Look for `/pic?action=upgrade&name=...` request
2. Check:
   - Status Code: Should be 200
   - Response Body: Should be `{"status":"started"}`
   - Connection: Check if "closed" or "keep-alive"
   - Timing: Check if request completes or hangs

**Key Questions**:
- ❓ Does the HTTP request to `/pic?action=upgrade` complete?
- ❓ What status code is returned?
- ❓ Is the response body correct?
- ❓ Does the WebSocket stay connected or disconnect?

---

### Phase 2: Telnet Debug Monitoring

**Connect to Device**:
```bash
telnet <device-ip> 23
```

**Clear Screen and Prepare**:
- Press Ctrl+L to clear
- Device outputs debug messages continuously

**Trigger Flash and Watch For**:

**Expected Output (Working)**:
```
Action: upgrade gateway.hex 
Upgrade /16F1847/gateway.hex
Executing deferred upgrade for: /16F1847/gateway.hex
Start PIC upgrade now: /16F1847/gateway.hex
Start PIC upgrade with hexfile: /16F1847/gateway.hex
PIC upgrade started, now running in background
Deferred upgrade initiated
Bootloader version 2.9
Upgrade: 0%
Upgrade: 10%
[... continues ...]
```

**Actual Output (If Broken - Scenario 1: HTTP Never Completes)**:
```
Action: upgrade gateway.hex 
Upgrade /16F1847/gateway.hex
(MISSING: Executing deferred upgrade)
(MISSING: Start PIC upgrade now)
```
→ **Diagnosis**: `pendingUpgradePath` set but `handlePendingUpgrade()` never called or HTTP handler crashed

**Actual Output (If Broken - Scenario 2: Upgrade Starts But Fails)**:
```
Action: upgrade gateway.hex 
Upgrade /16F1847/gateway.hex
Executing deferred upgrade for: /16F1847/gateway.hex
Start PIC upgrade now: /16F1847/gateway.hex
Upgrade finished: Errorcode = 3 - Could not open hex file
```
→ **Diagnosis**: File path wrong or file doesn't exist

**Actual Output (If Broken - Scenario 3: Upgrade Hangs)**:
```
Action: upgrade gateway.hex 
Upgrade /16F1847/gateway.hex
Executing deferred upgrade for: /16F1847/gateway.hex
Start PIC upgrade now: /16F1847/gateway.hex
Start PIC upgrade with hexfile: /16F1847/gateway.hex
PIC upgrade started, now running in background
Deferred upgrade initiated
(MISSING: Bootloader version)
(MISSING: Progress updates)
```
→ **Diagnosis**: State machine not progressing, `upgradeEvent()` not being called

**Key Questions**:
- ❓ Do you see "Executing deferred upgrade"?
- ❓ Do you see "Start PIC upgrade now"?
- ❓ Do you see "PIC upgrade started, now running in background"?
- ❓ Do you see "Bootloader version X.Y"?
- ❓ Do you see "Upgrade: X%" progress messages?

---

### Phase 3: Detailed Flow Analysis

**Check Each Step**:

1. **HTTP Request Received**:
   - Telnet: `Action: upgrade gateway.hex`
   - Telnet: `Upgrade /16F1847/gateway.hex`
   - ✅ If YES → HTTP handler called correctly

2. **HTTP Response Sent**:
   - Browser Network: Status 200, Body `{"status":"started"}`
   - ✅ If YES → Response generated correctly

3. **Pending Upgrade Queued**:
   - Check if any errors after "Upgrade /..." message
   - ✅ If NO ERRORS → `pendingUpgradePath` should be set

4. **Pending Upgrade Executed**:
   - Telnet: `Executing deferred upgrade for: /16F1847/gateway.hex`
   - ✅ If YES → Main loop called `handlePendingUpgrade()`

5. **Upgrade Started**:
   - Telnet: `Start PIC upgrade now: /16F1847/gateway.hex`
   - ✅ If YES → `upgradepicnow()` called

6. **Hex File Read**:
   - Telnet: `Start PIC upgrade with hexfile: /16F1847/gateway.hex`
   - Telnet: `PIC upgrade started, now running in background`
   - ✅ If YES → `fwupgradestart()` completed, hex file read OK

7. **State Machine Started**:
   - Telnet: `Bootloader version X.Y`
   - ✅ If YES → PIC responded, state machine working

8. **Progress Updates**:
   - Telnet: `Upgrade: X%`
   - Browser: WebSocket messages with progress
   - ✅ If YES → Callbacks working, flash progressing

**Failure Point Diagnosis**:
- Fails at step 1-2: HTTP handler issue
- Fails at step 3: `pendingUpgradePath` not set
- Fails at step 4: Main loop not calling `handlePendingUpgrade()`
- Fails at step 5-6: File path or `busy()` check issue
- Fails at step 7: PIC not responding or state machine broken
- Fails at step 8: Callbacks not registered or `upgradeEvent()` not called

---

## Common Issues and Solutions

### Issue 1: "Executing deferred upgrade" Never Appears

**Possible Causes**:
1. HTTP handler crashed before setting `pendingUpgradePath`
2. `isFlashing()` returns true, blocking `handlePendingUpgrade()`
3. Main loop not running

**Debug Steps**:
1. Add log IMMEDIATELY after `pendingUpgradePath =` line
2. Check if `isESPFlashing` or `isPICFlashing` already true
3. Add log at start of `handlePendingUpgrade()`

**Fix**:
- If `client().stop()` crashes handler → Remove it
- If flags stuck true → Reset flags before upgrade
- If loop not running → Check for infinite loops elsewhere

### Issue 2: "Could Not Open Hex File"

**Possible Causes**:
1. File path wrong (`/{deviceid}/{filename}` vs `/{filename}`)
2. File doesn't exist in LittleFS
3. LittleFS not mounted

**Debug Steps**:
1. Check `sPICdeviceid` value: should be "16F88" or "16F1847"
2. List LittleFS files via FSexplorer
3. Verify file exists at exact path

**Fix**:
- Upload hex file to correct path
- Check file permissions
- Remount LittleFS if needed

### Issue 3: Hangs After "PIC upgrade started"

**Possible Causes**:
1. `upgradeEvent()` not being called
2. PIC not responding (hardware issue)
3. Serial port stuck

**Debug Steps**:
1. Add log in `handleOTGW()` at entry
2. Add log in `OTGWSerial::available()` at entry
3. Check if `isPICFlashing` is true
4. Check if `handleOTGW()` is being called

**Fix**:
- Ensure `doBackgroundTasks()` runs every loop
- Verify `handleOTGW()` called when `isPICFlashing == true`
- Check hardware connections

### Issue 4: WebSocket Disconnects

**Possible Causes**:
1. `yield()` not called enough during hex read
2. WiFi stack starved
3. Browser timeout

**Debug Steps**:
1. Monitor WiFi signal strength
2. Check heap free memory
3. Increase `yield()` frequency

**Fix**:
- More frequent `yield()` calls
- Reduce hex file size
- Split into smaller chunks

---

## Critical Code Flow to Verify

### Main Loop
```cpp
void loop() {
    if (!isFlashing()) {
        // ... other tasks
        handlePendingUpgrade();  // ← Must be called when NOT flashing
    }
    doBackgroundTasks();  // ← Always called
}
```

**Verification**:
- Is `handlePendingUpgrade()` being called?
- Is `isFlashing()` false when it should be?

### Background Tasks During PIC Flash
```cpp
void doBackgroundTasks() {
    if (isPICFlashing) {
        handleOTGW();  // ← MUST be called for upgrade to progress
        handleWebSocket();
        httpServer.handleClient();
    }
}
```

**Verification**:
- Is `handleOTGW()` being called every loop when `isPICFlashing == true`?
- Does `handleOTGW()` call `OTGWSerial.available()`?

### Upgrade Event Processing
```cpp
int OTGWSerial::available() {
    if (upgradeEvent()) return 0;  // ← Processes upgrade if active
    return HardwareSerial::available();
}
```

**Verification**:
- Is `available()` being called from `handleOTGW()`?
- Does `upgradeEvent()` return true when upgrade active?

---

## Debugging Commands

### Check LittleFS Files
```bash
# Via FSexplorer web UI:
http://<device-ip>/FSexplorer.html

# Look for:
/16F88/gateway.hex
/16F1847/gateway.hex
```

### Check Variables Via Debug
Add these to telnet output:
```cpp
DebugTf(PSTR("isESPFlashing: %d, isPICFlashing: %d\r\n"), isESPFlashing, isPICFlashing);
DebugTf(PSTR("pendingUpgradePath: [%s]\r\n"), pendingUpgradePath.c_str());
DebugTf(PSTR("sPICdeviceid: [%s]\r\n"), sPICdeviceid);
```

### Check WebSocket State
In browser console:
```javascript
console.log("WebSocket state:", otLogWS ? otLogWS.readyState : "null");
// 0 = CONNECTING, 1 = OPEN, 2 = CLOSING, 3 = CLOSED
```

---

## Test Procedure

1. **Connect Telnet**: `telnet <device-ip> 23`
2. **Open Browser Console**: F12 → Console tab
3. **Open Network Tab**: F12 → Network tab
4. **Clear Both**: Telnet Ctrl+L, Browser console "Clear"
5. **Trigger Flash**: Click "Flash" in web UI
6. **Observe Both**: Watch telnet AND browser simultaneously
7. **Compare**: Expected vs Actual output
8. **Identify**: Where does flow diverge?
9. **Report**: Share both telnet log AND browser console/network output

---

## Expected Successful Flow (Complete)

### Telnet Output:
```
Action: upgrade gateway.hex 5.0
Upgrade /16F1847/gateway.hex
Executing deferred upgrade for: /16F1847/gateway.hex
Start PIC upgrade now: /16F1847/gateway.hex
Start PIC upgrade with hexfile: /16F1847/gateway.hex
PIC upgrade started, now running in background
Deferred upgrade initiated
Bootloader version 2.9
Upgrade: 0%
Upgrade: 5%
Upgrade: 10%
[... every 5% ...]
Upgrade: 95%
Upgrade: 100%
Upgrade finished: Errorcode = 0 - PIC upgrade was successful
```

### Browser Console:
```javascript
Connecting to event stream...
Starting upgrade for gateway.hex...
Flash started: {status: "started"}
Flashing gateway.hex started...
```

### Browser Network:
```
Request URL: http://192.168.1.100/pic?action=upgrade&name=gateway.hex
Status: 200 OK
Response: {"status":"started"}
Timing: < 100ms
```

### Browser WebSocket:
```
WebSocket opened
{"state":"start","flash_written":0,"flash_total":100,"filename":"gateway.hex","error":""}
{"state":"write","flash_written":10,"flash_total":100,"filename":"gateway.hex"}
[... continues ...]
{"state":"end","flash_written":100,"flash_total":100,"filename":"gateway.hex","error":""}
```

---

## Next Steps Based on Findings

**If HTTP request fails**:
→ Issue with `client().stop()` call - REVERT that change

**If "Executing deferred upgrade" missing**:
→ Issue with `pendingUpgradePath` or main loop - CHECK flags

**If hex file can't open**:
→ Issue with file path - VERIFY sPICdeviceid and file location

**If hangs after starting**:
→ Issue with state machine - CHECK `handleOTGW()` being called

**If WebSocket disconnects**:
→ Issue with `yield()` frequency - INCREASE frequency

---

**Please run this debugging session and share**:
1. Complete telnet output from trigger to failure
2. Browser console output (errors, logs)
3. Browser Network tab for /pic request (status, timing, response)
4. Browser WebSocket messages (if any)

This will tell us EXACTLY where the flow breaks!
