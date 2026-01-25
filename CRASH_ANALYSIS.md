# ESP Crash Analysis - PIC Flash Button Click

**Date**: 2026-01-25  
**Issue**: Clicking PIC flash button causes ESP to reboot/crash  
**Reporter**: @rvdbreemen

---

## Symptom

**User Report**: "When I just clicked the flash button on the PIC tab, for the gateway. Instead of starting the flash proces, it just rebooted the ESP / crashed."

**Browser Console Log**:
- Normal operation showing WebSocket connected
- OT Log messages flowing normally
- Then sudden silence (indicating hard reset/crash)
- No JavaScript errors before crash
- GET to `/api/v1/settings` returned 404 after reboot

---

## Root Cause Analysis

### What Happened

**Accidental Code Reversion**: During the git rebase in commit 38717c6, ALL code changes were accidentally reverted back to RC3 state:
- ❌ Lost `httpServer.client().flush()` call
- ❌ Lost all debug logging
- ❌ Lost `yield()` calls in hex file reading
- ❌ Lost PIC flash mode logging

**Current State Before Fix**: Code was identical to RC3 (3ea1d59)

### Why It Crashes

**Theory 1: Missing flush() Causes State Corruption** ⚠️ HIGH PROBABILITY
- RC3 code worked when environment was stable
- Without `flush()`, HTTP response may not be fully transmitted
- ESP tries to process pending upgrade while HTTP transaction incomplete
- Server's internal state gets corrupted
- Watchdog timer expires or exception occurs
- ESP reboots

**Theory 2: Race Condition in Pending Upgrade** ⚠️ MEDIUM PROBABILITY
- `pendingUpgradePath` set without flush
- Main loop immediately tries to process it
- HTTP server still using resources
- Conflict causes exception
- ESP crashes

**Theory 3: Heap Exhaustion** ⚠️ LOW PROBABILITY
- Without debug logging, can't see heap state
- Hex file reading without `yield()` could cause heap fragmentation
- Out of memory exception
- ESP crashes

**Theory 4: Watchdog Timer** ⚠️ MEDIUM PROBABILITY
- Without `yield()` calls during hex file read
- WiFi stack starves
- Watchdog timer not fed properly
- ESP resets

---

## The Fix

### Restore Code to c6e64d9 State

**What's Being Restored**:

1. **HTTP Response Flush**:
   ```cpp
   httpServer.send_P(200, PSTR("application/json"), PSTR("{\"status\":\"started\"}"));
   httpServer.client().flush();  // Ensure response buffer is sent to client
   ```
   **Why**: Ensures HTTP transaction completes before upgrade processing begins

2. **Debug Logging**:
   ```cpp
   DebugTf(PSTR("Pending upgrade queued: [%s]\r\n"), pendingUpgradePath.c_str());
   DebugTln(F("PIC upgrade started, now running in background"));
   DebugTf(PSTR("Flash flags before start: isESPFlashing=%d, isPICFlashing=%d\r\n"), ...);
   ```
   **Why**: Enables crash diagnosis via telnet monitoring

3. **WiFi Stack Yielding**:
   ```cpp
   // In OTGWSerial.cpp readHexFile()
   if (linecnt % 10 == 0) {
       yield();  // Allow WiFi and other background tasks to run
   }
   ```
   **Why**: Prevents WiFi stack starvation and watchdog timer issues

4. **PIC Flash Mode Confirmation**:
   ```cpp
   // In OTGW-firmware.ino doBackgroundTasks()
   static bool picFlashLogged = false;
   if (!picFlashLogged) {
       DebugTln(F("doBackgroundTasks: PIC flash mode active, calling handleOTGW"));
       picFlashLogged = true;
   }
   ```
   **Why**: Confirms background task handling during flash

### Why This Should Prevent Crashes

1. **Ordered Execution**:
   - `flush()` ensures HTTP completes first
   - Then `pendingUpgradePath` is set
   - Clean separation prevents race conditions

2. **WiFi Stack Stability**:
   - `yield()` calls keep WiFi stack alive
   - Prevents watchdog timer issues
   - Maintains WebSocket connection

3. **Diagnostic Capability**:
   - Debug logs show exact point of failure
   - Can identify if crash happens in HTTP handler, main loop, or upgrade code
   - Telnet monitoring provides real-time crash diagnosis

---

## Expected Behavior After Fix

### Telnet Output (If Working):
```
Action: upgrade gateway.hex
Upgrade /16F1847/gateway.hex
Pending upgrade queued: [/16F1847/gateway.hex]
handlePendingUpgrade: No pending upgrade
Executing deferred upgrade for: /16F1847/gateway.hex
Flash flags before start: isESPFlashing=0, isPICFlashing=0
Start PIC upgrade now: /16F1847/gateway.hex
Start PIC upgrade with hexfile: /16F1847/gateway.hex
doBackgroundTasks: PIC flash mode active, calling handleOTGW
PIC upgrade started, now running in background
Deferred upgrade initiated
Bootloader version 2.9
Upgrade: 0%
```

### Telnet Output (If Still Crashing):
Will show exactly WHERE the crash occurs:
- If crash after "Pending upgrade queued" → Problem in pendingUpgradePath processing
- If crash after "Executing deferred upgrade" → Problem in upgradepicnow()
- If crash after "Start PIC upgrade now" → Problem in fwupgradestart()
- If crash during hex file read → Problem in readHexFile()
- If no output at all → Crash in HTTP handler before logging

---

## Testing Instructions

### Monitor for Crash Location

**Setup**:
1. `telnet <device-ip> 23`
2. Browser F12 → Console tab
3. Clear both

**Trigger Flash**:
1. Click "Flash" button on PIC tab
2. Watch BOTH telnet and browser console simultaneously

**Observe**:
- Last message in telnet before crash = crash location
- Browser console: Look for WebSocket disconnect
- Note: ESP will reboot, telnet will disconnect, WebSocket will close

### If Crash Still Occurs

**Report**:
1. Last telnet message before crash
2. Browser console output
3. Time between click and crash (immediate vs delayed)
4. Does ESP always crash or intermittent?

### If It Works

**Verify**:
- ✅ Flash proceeds normally
- ✅ WebSocket stays connected
- ✅ Progress updates appear
- ✅ No ESP reboot

---

## Additional Defensive Measures (If Still Crashes)

If crash persists after restoring code, add these:

### 1. Watchdog Feed in HTTP Handler
```cpp
if (action == F("upgrade")) {
    feedWatchDog();  // Prevent watchdog reset
    DebugTf(PSTR("Upgrade /%s/%s\r\n"), sPICdeviceid, filename.c_str());
    // ... rest of code
}
```

### 2. Exception Handler Around Upgrade
```cpp
void handlePendingUpgrade() {
    if (pendingUpgradePath != F("")) {
        DebugTf(PSTR("Executing deferred upgrade for: %s\r\n"), pendingUpgradePath.c_str());
        
        // Feed watchdog before potentially long operation
        feedWatchDog();
        
        // Try-catch equivalent for ESP
        upgradepicnow(pendingUpgradePath.c_str());
        
        pendingUpgradePath = "";
    }
}
```

### 3. Heap Check Before Upgrade
```cpp
if (ESP.getFreeHeap() < 8192) {
    DebugTf(PSTR("ERROR: Low heap before upgrade: %d bytes\r\n"), ESP.getFreeHeap());
    httpServer.send_P(500, PSTR("text/plain"), PSTR("Low memory"));
    return;
}
```

### 4. Delay After HTTP Response
```cpp
httpServer.send_P(200, PSTR("application/json"), PSTR("{\"status\":\"started\"}"));
httpServer.client().flush();
delay(100);  // Give HTTP stack time to complete
pendingUpgradePath = "/" + String(sPICdeviceid) + "/" + filename;
```

---

## Summary

**Problem**: Accidental code reversion during git rebase removed all fixes, code crashed

**Fix**: Restore code to c6e64d9 state with:
- `flush()` call for clean HTTP completion
- Debug logging for crash diagnosis  
- `yield()` calls for WiFi stability
- PIC flash mode confirmation

**Next**: Test and monitor telnet to identify crash location if it still occurs

**Status**: Code restored, ready for testing
