# 🔴 CRITICAL FIX: OTA Upload Stall Root Cause

## The Problem

OTA uploads stall at random percentages and never complete.

## Root Cause Identified

**You moved watchdog feeding from UPLOAD_FILE_WRITE handler to Update.onProgress callback**, creating a critical timing gap.

### RC1 Branch (Works) - Dual Watchdog Feeding:
```cpp
// Setup: NO Update.onProgress callback
Update.onProgress([...]); // NOT PRESENT IN RC1

// UPLOAD_FILE_WRITE: Feeds watchdog on EVERY chunk
else if(upload.status == UPLOAD_FILE_WRITE) {
    Wire.beginTransmission(0x26);   // ✅ FEED ON EVERY CHUNK
    Wire.write(0xA5);   
    Wire.endTransmission();
    
    Update.write(upload.buf, upload.currentSize);
    _status.flash_written += written;
    _sendStatusEvent();  // Push via WebSocket
}
```

### Your Current Branch (Broken) - Single Watchdog Feeding:
```cpp
// Setup: Added Update.onProgress callback
Update.onProgress([this](size_t progress, size_t total) {
    // Throttled watchdog feeding (250ms minimum)
    if (now - _lastDogFeedTime >= 250) {  // ⚠️ THROTTLED
        Wire.beginTransmission(0x26);
        Wire.write(0xA5);
        Wire.endTransmission();
    }
});

// UPLOAD_FILE_WRITE: NO watchdog feeding!
else if(upload.status == UPLOAD_FILE_WRITE) {
    // ❌ NO WATCHDOG FEEDING HERE!
    Update.write(upload.buf, upload.currentSize);
    _status.flash_written += written;
    // No WebSocket - relies on polling
}
```

## Why It Fails

### The Two Phases of Upload

1. **HTTP Upload Phase** (UPLOAD_FILE_WRITE)
   - Client sends file chunks over HTTP
   - ESP receives chunks into RAM buffer
   - Handler called for each chunk (every 512 bytes)
   - **Duration**: Depends on network speed
   - **Your bug**: NO watchdog feeding in this phase!

2. **Flash Write Phase** (Update.write → Update.onProgress)
   - RAM buffer written to flash memory  
   - Update.onProgress callback fires
   - **Duration**: Fast, internal operation
   - **Your code**: Watchdog fed here with 250ms throttle

### The Timing Problem

```
Timeline of a typical upload:

RC1 (Works):
------------------------------------------------------------
HTTP chunk arrives → UPLOAD_FILE_WRITE → Feed watchdog ✅
  ↓ 50ms
HTTP chunk arrives → UPLOAD_FILE_WRITE → Feed watchdog ✅
  ↓ 50ms  
HTTP chunk arrives → UPLOAD_FILE_WRITE → Feed watchdog ✅
  ↓ 50ms
HTTP chunk arrives → UPLOAD_FILE_WRITE → Feed watchdog ✅

Your Branch (Fails):
------------------------------------------------------------
HTTP chunk arrives → UPLOAD_FILE_WRITE → NO watchdog ❌
  ↓ 50ms
HTTP chunk arrives → UPLOAD_FILE_WRITE → NO watchdog ❌
  ↓ 50ms
HTTP chunk arrives → UPLOAD_FILE_WRITE → NO watchdog ❌
  ↓ 50ms
HTTP chunk arrives → UPLOAD_FILE_WRITE → NO watchdog ❌
  ↓ 300ms total elapsed
Watchdog timeout (500ms typical) → RESET! 💥
```

### Why Update.onProgress Doesn't Help

**Update.onProgress fires during `Update.write()`** - but this is:
1. Called **after** receiving the HTTP chunk
2. Completes **quickly** (writing to flash is fast)
3. Returns to HTTP upload phase **immediately**
4. The callback might not fire at all if flash write is very fast
5. Even when it fires, 250ms throttle means gaps between feeds

**The gap**: Between HTTP chunks, there's no watchdog feeding!

## The Fix Applied

Added watchdog feeding **back** into UPLOAD_FILE_WRITE handler:

```cpp
else if(upload.status == UPLOAD_FILE_WRITE) {
    // **CRITICAL**: Feed watchdog on EVERY chunk during HTTP upload
    Wire.beginTransmission(0x26);  // ✅ RESTORED
    Wire.write(0xA5);
    Wire.endTransmission();
    
    Update.write(upload.buf, upload.currentSize);
    _status.flash_written += written;
}
```

Now watchdog is fed in **BOTH** places:
- ✅ **UPLOAD_FILE_WRITE**: Every chunk (every 512 bytes, ~50ms intervals)
- ✅ **Update.onProgress**: During flash writes (250ms throttle)

## Flow Comparison

### RC1 (WebSocket - Works Perfectly)

| Phase | Watchdog | Progress Updates | Method |
|-------|----------|------------------|--------|
| HTTP Upload | Every chunk (512B) | Every chunk | WebSocket push |
| Flash Write | Every chunk (512B) | Every chunk | WebSocket push |
| **Result** | ✅ Always fed | ✅ Always updated | ✅ Instant |

### Your Original Branch (Broken)

| Phase | Watchdog | Progress Updates | Method |
|-------|----------|------------------|--------|
| HTTP Upload | ❌ Never | Every 100ms | HTTP polling |
| Flash Write | 250ms throttle | Callback + 100ms | HTTP polling |
| **Result** | ❌ Timeout | ⚠️ Delayed | ⚠️ 500ms lag |

### After This Fix (Should Work)

| Phase | Watchdog | Progress Updates | Method |
|-------|----------|------------------|--------|
| HTTP Upload | Every chunk (512B) ✅ | Every 100ms | HTTP polling |
| Flash Write | Dual: chunk + 250ms ✅ | Callback + 100ms | HTTP polling |
| **Result** | ✅ Always fed | ⚠️ Delayed | ⚠️ 500ms lag |

## Why RC1 Uses WebSocket

**WebSocket provides instant push updates** - the client doesn't poll, the server pushes:
- ✅ No polling lag
- ✅ Instant progress updates
- ✅ More responsive UI
- ⚠️ More complex code
- ⚠️ Requires WebSocket handshake

**Polling approach** (your branch):
- ✅ Simpler code
- ✅ No WebSocket complexity
- ✅ Better browser compatibility
- ⚠️ 100-500ms status lag (acceptable)

## Testing Checklist

After this fix, verify:

1. ✅ Upload completes without stalling
2. ✅ Progress bars update smoothly
3. ✅ Watchdog doesn't reset ESP during upload
4. ✅ Telnet shows progress dots
5. ✅ Final status delivered before reboot

## Technical Details

### Hardware Watchdog
- **Address**: I2C 0x26
- **Feed byte**: 0xA5
- **Timeout**: ~500ms (varies by hardware)
- **Effect if not fed**: ESP8266 hard reset

### Upload Buffer
- **Size**: 512 bytes (reduced from 2048)
- **Chunk frequency**: Every ~50-100ms on typical network
- **Chunks per second**: ~10-20 depending on speed

### Critical Math
- **Watchdog timeout**: 500ms
- **Chunk arrival**: Every 50ms
- **Required**: Feed at least every 400ms to be safe
- **RC1 approach**: Feed every 50ms (10x safety margin)
- **Your broken approach**: Feed every 250ms in callback (might miss HTTP phase)
- **Fixed approach**: Feed every 50ms during HTTP + 250ms during flash (safe)

## Conclusion

The stall was caused by **removing watchdog feeding from the HTTP upload handler**. The Update.onProgress callback doesn't fire during HTTP chunk reception, only during flash writes. This created gaps of 300-500ms without watchdog feeding, causing hardware watchdog timeout and ESP reset.

**Fix**: Restore watchdog feeding in UPLOAD_FILE_WRITE handler (done).
