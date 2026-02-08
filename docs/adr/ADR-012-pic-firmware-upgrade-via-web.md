# ADR-012: PIC Firmware Upgrade via Web UI

**Status:** Accepted  
**Date:** 2019-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)  
**Critical Fix:** Release Candidate v1.0.0-rc4 (Binary data parsing safety)

## Context

The OTGW hardware consists of two microcontrollers:
1. **PIC microcontroller:** Handles OpenTherm protocol communication with the boiler
2. **ESP8266:** Provides network connectivity (this firmware)

The PIC firmware (written by Schelte Bron) receives updates to fix bugs and add features. Users need a safe way to update the PIC firmware.

**Existing method (OTmonitor):**
- OTmonitor desktop application can flash PIC over serial
- Works well over USB cable
- Schelte Bron warns: **"DO NOT flash PIC over WiFi using OTmonitor"**
- Risk: Network interruption during flash can brick the PIC

**Requirements:**
- Safe PIC firmware updates without bricking risk
- No USB cable needed (WiFi-only update)
- Progress feedback during upload
- Validation of firmware file before flashing
- Recovery if update fails

## Decision

**Implement PIC firmware flashing directly in the ESP8266 Web UI with WebSocket progress streaming.**

**Architecture:**
- **Upload:** Hex file via Web UI multipart form
- **Validation:** Parse hex file, verify format, check banner
- **Reset:** ESP8266 controls PIC reset via GPIO14
- **Bootloader:** Put PIC into bootloader mode via special serial commands
- **Programming:** ESP8266 sends hex file records to PIC bootloader
- **Progress:** Real-time updates via WebSocket
- **Safety:** Binary data parsing using `memcmp_P()` (not string functions)

**Workflow:**
1. User uploads `.hex` file via Web UI
2. ESP8266 validates file format
3. PIC reset into bootloader mode (GPIO14 control)
4. ESP8266 programs PIC via serial
5. Progress streamed to browser via WebSocket
6. PIC reboots with new firmware
7. ESP8266 verifies PIC is responding

## Alternatives Considered

### Alternative 1: Continue Using OTmonitor Over WiFi
**Pros:**
- No firmware development needed
- Schelte Bron's official tool
- Well-tested

**Cons:**
- **Schelte warns against WiFi flashing** (can brick device)
- Requires Windows PC
- Network interruption = bricked PIC
- No recovery mechanism
- Users report bricked devices from this method

**Why not chosen:** Unsafe. Risk of bricking the PIC is too high with network-based OTmonitor flashing.

### Alternative 2: USB Cable Required
**Pros:**
- Most reliable connection
- No network interruption risk
- Simple implementation

**Cons:**
- Requires physical access to device
- May be mounted in hard-to-reach location
- Defeats purpose of WiFi capability
- Poor user experience

**Why not chosen:** Physical access requirement negates the benefit of network connectivity.

### Alternative 3: Over-The-Air (OTA) to ESP8266, Then to PIC
**Pros:**
- Single upload for both MCUs
- Automated process

**Cons:**
- Complex coordination
- Firmware size bloat (both firmwares in one file)
- Difficult rollback
- Higher risk (two updates in one operation)

**Why not chosen:** Too complex. Separate updates are safer and more flexible.

### Alternative 4: External Programmer (ICSP)
**Pros:**
- Can always recover from brick
- Professional solution
- Most reliable

**Cons:**
- Requires special hardware (PICkit, etc.)
- Requires disassembly
- Not end-user friendly
- Expensive

**Why not chosen:** Not a user-facing solution. Should be last resort only.

## Consequences

### Positive
- **WiFi flashing:** Users can update without physical access
- **Safe:** ESP8266 controls timing, no network interruption issues
- **Progress feedback:** User sees real-time upload status
- **Validation:** Hex file checked before flashing
- **GPIO control:** ESP8266 can reset PIC reliably
- **Recovery:** Can retry if flash fails
- **No bricking:** Much safer than OTmonitor over WiFi

### Negative
- **Complexity:** ESP8266 firmware must parse hex files
  - Mitigation: Well-tested hex parser in `OTGWSerial` library
- **Binary data bugs:** String functions on binary data cause crashes
  - Fixed: v1.0.0-rc4 switched to `memcmp_P()` for binary comparison
- **Upload size:** Hex files can be large (~100KB)
  - Mitigation: Streaming upload, no need to buffer entire file
- **Failure recovery:** If flash fails, PIC may not boot
  - Mitigation: PIC bootloader remains intact, can retry

### Risks & Mitigation
- **Buffer overrun:** Reading past end of hex file (Exception 2 crash)
  - **Fixed:** v1.0.0-rc4 uses `memcmp_P()` instead of `strncmp_P()` for banner search
  - **Mitigation:** Bounds checking on all buffer operations
- **Power loss during flash:** Could corrupt PIC firmware
  - **Accepted:** Same risk as any firmware update (USB or WiFi)
  - **Recovery:** PIC bootloader survives, can reflash
- **Wrong firmware file:** User uploads incorrect hex file
  - **Mitigation:** Banner validation checks for correct PIC type
- **Serial communication failure:** ESP8266 ↔ PIC communication fails
  - **Mitigation:** Retry logic, timeout detection

## Implementation Details

## Breaking Changes (Release Candidate v1.0.0-rc4)

**Critical buffer overrun fix that prevents Exception (2) crashes:**

**Critical fix (v1.0.0-rc4):**
```cpp
// BEFORE (CRASHES - buffer overrun)
while (ptr < datasize) {
  char *s = strstr_P(datamem + ptr, banner);  // DANGEROUS on binary data!
  if (s == nullptr) {
    ptr += strnlen(datamem + ptr, datasize - ptr) + 1;  // Reads past buffer!
  }
}

// AFTER (SAFE - bounded search)
size_t bannerLen = sizeof(banner) - 1;
if (datasize >= bannerLen) {  // Prevent underflow
  for (ptr = 0; ptr <= (datasize - bannerLen); ptr++) {  // Bounded loop
    if (memcmp_P(datamem + ptr, banner, bannerLen) == 0) {  // Binary-safe
      // Found banner
      break;
    }
  }
}
```

**Why the change was critical:**
- `strstr_P()` and `strnlen()` expect null-terminated strings
- Hex file binary data has NO null terminators
- Reading past buffer looking for `\0` causes Exception (2) crash
- `memcmp_P()` compares exact byte count, no null terminator needed

**WebSocket progress updates:**
```cpp
void updateProgress(int percent, const char* message) {
  char json[128];
  snprintf_P(json, sizeof(json), 
    PSTR("{\"type\":\"progress\",\"percent\":%d,\"message\":\"%s\"}"),
    percent, message);
  webSocket.broadcastTXT(json);
}

// During flash
updateProgress(0, "Resetting PIC");
updateProgress(10, "Entering bootloader");
updateProgress(50, "Programming..."); 
updateProgress(100, "Complete");
```

**PIC reset control:**
```cpp
// GPIO14 connected to PIC reset line
#define PIC_RESET_PIN 14

void resetPIC() {
  pinMode(PIC_RESET_PIN, OUTPUT);
  digitalWrite(PIC_RESET_PIN, LOW);   // Assert reset
  delay(100);
  digitalWrite(PIC_RESET_PIN, HIGH);  // Release reset
  delay(500);                          // Wait for PIC boot
}
```

**Hex file validation:**
```cpp
bool validateHexFile(const uint8_t* data, size_t len) {
  // Check for Intel HEX format
  // Look for firmware banner
  // Verify checksum
  // Ensure correct PIC type
  
  const char banner[] PROGMEM = "OpenTherm Gateway";
  return findBannerInHex(data, len, banner);
}
```

## Web UI Integration

**Upload form:**
```html
<form method="POST" action="/api/v1/pic/flash" enctype="multipart/form-data">
  <input type="file" name="firmware" accept=".hex">
  <button type="submit">Flash PIC Firmware</button>
</form>
```

**JavaScript progress handler:**
```javascript
const ws = new WebSocket('ws://' + location.hostname + ':81/');

ws.onmessage = function(event) {
  const data = JSON.parse(event.data);
  if (data.type === 'progress') {
    updateProgressBar(data.percent);
    showMessage(data.message);
  }
};
```

## Safety Measures

1. **Pre-flash validation:** Reject invalid hex files before touching PIC
2. **Bootloader protection:** PIC bootloader never overwritten
3. **Progress tracking:** User sees exactly what's happening
4. **Timeout handling:** If programming stalls, report error
5. **Retry capability:** Can attempt flash again if it fails
6. **Verification:** Read back firmware version after flash

## Related Decisions
- ADR-005: WebSocket for Real-Time Streaming (progress updates)
- ADR-004: Static Buffer Allocation Strategy (binary data safety)

## References
- Implementation: `src/libraries/OTGWSerial/OTGWSerial.cpp` (hex file parsing)
- Version detection: `versionStuff.ino` (GetVersion function)
- Critical fix: v1.0.0-rc4 changelog (buffer overrun fix)
- Web UI: `data/index.html` (firmware upload form)
- Schelte Bron warning: https://otgw.tclcode.com/firmware.html (WiFi flashing warning)
- Binary data safety: `.github/copilot-instructions.md` (Binary Data Handling section)
