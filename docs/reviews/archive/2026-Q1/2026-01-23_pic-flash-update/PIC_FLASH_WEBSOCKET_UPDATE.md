# PIC Flash WebSocket Message Format Update

**Date**: 2026-01-23  
**Author**: GitHub Copilot Advanced Agent  
**PR**: Optimize PIC flashing WebSocket JSON messages

## Summary

Updated PIC firmware flash WebSocket messages to use the same format as ESP firmware flash, ensuring proper frontend handling and display of progress.

## Problem

The original PIC flash callbacks sent messages in a different format than the ESP flash:
- **PIC (old)**: `{"percent":50}` and `{"result":0,"errors":0,"retries":0}`
- **ESP**: `{"state":"write","flash_written":X,"flash_total":Y,"filename":"...","error":""}`

The frontend's `handleFlashMessage()` function only processes messages with a `state` property, so the old PIC messages were silently ignored.

## Solution

Updated PIC flash callbacks to send messages in the same format as ESP flash:

### Changes Made

1. **Added filename tracking** (`OTGW-firmware.h`):
   ```cpp
   char currentPICFlashFile[65] = ""; // Track current PIC flash filename
   ```

2. **Store filename at start** (`fwupgradestart()`):
   ```cpp
   const char *filename = strrchr(hexfile, '/');
   if (filename) filename++; else filename = hexfile;
   strlcpy(currentPICFlashFile, filename, sizeof(currentPICFlashFile));
   ```

3. **Send progress messages** (`fwupgradestep(pct)`):
   ```cpp
   const char *state = (pct == 0) ? "start" : "write";
   snprintf_P(buffer, sizeof(buffer), 
     PSTR("{\"state\":\"%s\",\"flash_written\":%d,\"flash_total\":100,\"filename\":\"%s\",\"error\":\"\"}"),
     state, pct, currentPICFlashFile);
   sendWebSocketJSON(buffer);
   ```

4. **Send completion messages** (`fwupgradedone(result)`):
   ```cpp
   const char *state = (result == OTGWError::OTGW_ERROR_NONE) ? "end" : "error";
   snprintf_P(buffer, sizeof(buffer), 
     PSTR("{\"state\":\"%s\",\"flash_written\":100,\"flash_total\":100,\"filename\":\"%s\",\"error\":\"%s\"}"),
     state, currentPICFlashFile, errorupgrade);
   sendWebSocketJSON(buffer);
   currentPICFlashFile[0] = '\0'; // Clear after completion
   ```

## Message Flow

### Successful Flash
1. **Start**: `{"state":"start","flash_written":0,"flash_total":100,"filename":"gateway.hex","error":""}`
2. **Progress**: `{"state":"write","flash_written":25,"flash_total":100,"filename":"gateway.hex","error":""}`
3. **Progress**: `{"state":"write","flash_written":50,"flash_total":100,"filename":"gateway.hex","error":""}`
4. **Progress**: `{"state":"write","flash_written":75,"flash_total":100,"filename":"gateway.hex","error":""}`
5. **Complete**: `{"state":"end","flash_written":100,"flash_total":100,"filename":"gateway.hex","error":""}`

### Failed Flash
1. **Start**: `{"state":"start","flash_written":0,"flash_total":100,"filename":"gateway.hex","error":""}`
2. **Progress**: `{"state":"write","flash_written":30,"flash_total":100,"filename":"gateway.hex","error":""}`
3. **Error**: `{"state":"error","flash_written":100,"flash_total":100,"filename":"gateway.hex","error":"Too many retries"}`

## Frontend Handling

The `handleFlashMessage()` function in `data/index.js` now properly handles PIC flash messages:

```javascript
function handleFlashMessage(data) {
    let msg = JSON.parse(data);
    
    if (msg.hasOwnProperty('state')) {  // Now matches!
        let percent = Math.round((msg.flash_written * 100) / msg.flash_total);
        progressBar.style.width = percent + "%";
        
        if (msg.state === 'write' || msg.state === 'start') {
            pctText.innerText = "Flashing " + msg.filename + " : " + percent + "%";
        }
        
        if (msg.state === 'end') {
            // Success handling
        } else if (msg.state === 'error') {
            // Error handling with msg.error
        }
    }
}
```

## Benefits

✓ **Consistent format**: ESP and PIC flash use same message structure  
✓ **Frontend compatibility**: Messages properly handled by existing code  
✓ **Progress display**: Users see real-time PIC flash progress  
✓ **Error reporting**: Error messages displayed in UI  
✓ **Minimal code**: Reuses existing frontend handling logic  

## Memory Usage

- **Added**: 65 bytes for `currentPICFlashFile`
- **Per message**: 192-256 bytes stack allocation (temporary)
- **Total impact**: Minimal - single global variable

## Testing

Verify:
1. PIC flash shows progress bar advancing
2. Filename displayed during flash
3. Success message on completion
4. Error message on failure
5. WebSocket console shows proper JSON format

## Files Modified

- `OTGW-firmware.h` - Added `currentPICFlashFile` variable
- `OTGW-Core.ino` - Updated `fwupgradestart()`, `fwupgradestep()`, `fwupgradedone()`
