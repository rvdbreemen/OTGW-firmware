# Action Checklist - Critical Code Review Fixes

**Date**: 2026-02-04  
**Version**: 1.0.0-rc7  
**Priority**: CRITICAL  

This document provides step-by-step instructions for implementing the recommended fixes from the critical code review.

---

## 🔴 CRITICAL PRIORITY (Must Fix Before Release)

### Issue #1: Binary Data Comparison Bug

**File**: `src/libraries/OTGWSerial/OTGWSerial.cpp`  
**Lines**: 304-307  
**Time Estimate**: 10 minutes  
**Risk**: Low

#### Steps:

1. **Open file**: `src/libraries/OTGWSerial/OTGWSerial.cpp`

2. **Locate code** (around line 300):
```cpp
// Look for the new firmware version
version = nullptr;
unsigned short ptr = 0;

while (ptr < info.datasize) {
    char *s = strstr_P((char *)datamem + ptr, banner1);
    if (s == nullptr) {
        ptr += strnlen((char *)datamem + ptr,
          info.datasize - ptr) + 1;
    } else {
        s += sizeof(banner1) - 1;
        version = s;
        Dprintf("Version: %s\n", version);
        if (firmware == FIRMWARE_OTGW && *fwversion) {
            weight += 4 * WEIGHT_DATAREAD;
        }
        break;
    }
}
```

3. **Replace with**:
```cpp
// Look for the new firmware version
version = nullptr;
size_t banner1_len = strlen_P(banner1);

// Safe binary search using memcmp_P (matches versionStuff.ino pattern)
for (unsigned short ptr = 0; ptr <= (info.datasize - banner1_len); ptr++) {
    if (memcmp_P((char *)datamem + ptr, banner1, banner1_len) == 0) {
        version = (char *)datamem + ptr + banner1_len;
        Dprintf("Version: %s\n", version);
        if (firmware == FIRMWARE_OTGW && *fwversion) {
            // Reading out the EEPROM settings takes 4 reads of 64 bytes
            weight += 4 * WEIGHT_DATAREAD;
        }
        break;
    }
}
```

4. **Save file**

5. **Build firmware**:
```bash
python build.py --firmware
```

6. **Test**:
   - Upload all PIC firmware types (gateway.hex, diagnose.hex for both PICs)
   - Monitor telnet debug output for version detection
   - Verify no Exception (2) crashes

---

### Issue #2: Memory Exhaustion in File Streaming

**File**: `FSexplorer.ino`  
**Lines**: 104-120  
**Time Estimate**: 30 minutes  
**Risk**: Low

#### Steps:

1. **Open file**: `FSexplorer.ino`

2. **Locate code** (around line 94):
```cpp
if (versionMismatch) {
    // Version mismatch: disable caching and inject version hash for cache-busting
    httpServer.sendHeader(F("Cache-Control"), F("no-store, no-cache, must-revalidate"));
    httpServer.sendHeader(F("Pragma"), F("no-cache"));
    
    // Stream file line-by-line to avoid loading entire file into RAM (11KB+)
    // This prevents memory exhaustion when version mismatch occurs
    httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    httpServer.send(200, F("text/html; charset=UTF-8"), F(""));
    
    while (f.available()) {
      String line = f.readStringUntil('\n');
      // ... String operations ...
    }
```

3. **Replace the while loop** (lines 104-121) with:
```cpp
    // Use fixed buffers to avoid String allocations and heap fragmentation
    char lineBuf[512];  // Fixed buffer for line reading
    char outputBuf[600]; // Output buffer with room for version hash injection
    const char* fsHashCStr = fsHash.c_str();
    
    while (f.available()) {
      size_t len = 0;
      
      // Read line into fixed buffer (char-by-char to avoid String)
      while (f.available() && len < (sizeof(lineBuf) - 1)) {
        char c = f.read();
        if (c == '\n') break;
        lineBuf[len++] = c;
      }
      lineBuf[len] = '\0';
      
      // Check for script tags that need version injection
      const char* jsPattern = "src=\"./index.js\"";
      const char* graphPattern = "src=\"./graph.js\"";
      char* found;
      
      if ((found = strstr(lineBuf, jsPattern)) != nullptr) {
        // Build output with index.js version injection
        size_t prefixLen = found - lineBuf;
        snprintf_P(outputBuf, sizeof(outputBuf), 
                   PSTR("%.*ssrc=\"./index.js?v=%s\"%s"),
                   (int)prefixLen, lineBuf, fsHashCStr, 
                   found + strlen(jsPattern));
        httpServer.sendContent(outputBuf);
      } else if ((found = strstr(lineBuf, graphPattern)) != nullptr) {
        // Build output with graph.js version injection
        size_t prefixLen = found - lineBuf;
        snprintf_P(outputBuf, sizeof(outputBuf),
                   PSTR("%.*ssrc=\"./graph.js?v=%s\"%s"),
                   (int)prefixLen, lineBuf, fsHashCStr,
                   found + strlen(graphPattern));
        httpServer.sendContent(outputBuf);
      } else {
        // No replacement needed, send line as-is
        httpServer.sendContent(lineBuf);
      }
      
      // Add newline back (readStringUntil consumed it)
      if (f.available() || len > 0) {
        httpServer.sendContent_P(PSTR("\n"));
      }
    }
    httpServer.sendContent(F("")); // End of chunked stream
```

4. **Save file**

5. **Build firmware**:
```bash
python build.py --firmware
```

6. **Test**:
   - Create version mismatch (update firmware, not filesystem)
   - Open browser to device IP
   - Monitor heap: Add debug line before/after serving
   - Test with 3-5 concurrent browser connections
   - Verify no watchdog resets

---

## 🟡 HIGH PRIORITY (Security Hardening)

### Issue #3: XSS Vulnerability in Hash Injection

**File**: `helperStuff.ino` (or wherever `getFilesystemHash()` is defined)  
**Time Estimate**: 15 minutes  
**Risk**: Low

#### Steps:

1. **Locate** `getFilesystemHash()` function (likely in `helperStuff.ino`)

2. **Find the function**:
```cpp
String getFilesystemHash() {
  static String _githash = "";
  
  if (_githash.length() > 0) return _githash;
  
  File f = LittleFS.open("/version.hash", "r");
  if (f && f.available()) {
    _githash = f.readStringUntil('\n');
    _githash.trim();
    f.close();
  }
  return _githash;
}
```

3. **Replace with validated version**:
```cpp
String getFilesystemHash() {
  static String _githash = "";
  
  if (_githash.length() > 0) return _githash;
  
  File f = LittleFS.open("/version.hash", "r");
  if (f && f.available()) {
    String raw = f.readStringUntil('\n');
    raw.trim();
    f.close();
    
    // SECURITY: Validate hash is hexadecimal (git short hash format)
    // Expected format: 7-40 characters, 0-9 and a-f only
    if (raw.length() >= 7 && raw.length() <= 40) {
      bool valid = true;
      for (size_t i = 0; i < raw.length(); i++) {
        char c = raw.charAt(i);
        if (!((c >= '0' && c <= '9') || 
              (c >= 'a' && c <= 'f') || 
              (c >= 'A' && c <= 'F'))) {
          valid = false;
          break;
        }
      }
      
      if (valid) {
        _githash = raw;
        DebugTf(PSTR("Filesystem hash validated: %s\r\n"), _githash.c_str());
      } else {
        DebugTln(F("WARNING: Invalid version.hash format (non-hex characters) - ignoring"));
      }
    } else {
      DebugTf(PSTR("WARNING: Invalid version.hash length (%d chars) - ignoring\r\n"), 
              raw.length());
    }
  }
  return _githash;
}
```

4. **Save file**

5. **Build firmware**:
```bash
python build.py --firmware
```

6. **Test**:
   - Normal case: Valid git hash (e.g., "9bb465a")
   - Attack case: Create `/data/version.hash` with malicious content:
     ```
     "><script>alert(1)</script>
     ```
   - Verify hash is rejected and not injected into HTML
   - Check telnet debug for warning message

---

## 🟠 MEDIUM PRIORITY (Performance & Code Quality)

### Issue #4: JavaScript Context Injection

**File**: `FSexplorer.ino`  
**Lines**: 591, 594  
**Time Estimate**: 10 minutes  
**Risk**: Low

#### Steps:

1. **Open file**: `FSexplorer.ino`

2. **Locate the redirect code** (around line 585):
```cpp
httpServer.sendContent_P(PSTR("<script>setTimeout(function() { "));
httpServer.sendContent_P(PSTR("window.location.href='"));
httpServer.sendContent(safeURL);
httpServer.sendContent_P(PSTR("'; }, 3000);</script>"));
```

3. **Replace with HTTP header redirect**:
```cpp
// Use HTTP Refresh header instead of JavaScript (more secure, accessible)
String refreshHeader = "3; url=" + safeURL;
httpServer.sendHeader(F("Refresh"), refreshHeader);
```

4. **Remove the JavaScript redirect lines** (they're no longer needed)

5. **Save file**

6. **Build and test**:
```bash
python build.py --firmware
```

7. **Test**: Trigger error conditions that cause redirects

---

### Issue #5: String Concatenation in WiFi Init

**File**: `networkStuff.h`  
**Lines**: 153, 443  
**Time Estimate**: 15 minutes  
**Risk**: Low

#### Steps:

1. **Open file**: `networkStuff.h`

2. **Locate line 153** (inside `startWiFi()` or similar):
```cpp
String thisAP = String(hostname) + "-" + WiFi.macAddress();
```

3. **Replace with**:
```cpp
char thisAP[64];
snprintf_P(thisAP, sizeof(thisAP), PSTR("%s-%s"), 
           hostname, WiFi.macAddress().c_str());
```

4. **Locate line 443** (MQTT unique ID generation):
```cpp
String uniqueId = "otgw-"+(String)getMacAddress();
```

5. **Replace with**:
```cpp
char uniqueId[32];
snprintf_P(uniqueId, sizeof(uniqueId), PSTR("otgw-%s"), getMacAddress());
```

6. **Note**: If these functions return String, change return type to `const char*` and return pointer to buffer

7. **Save file**

8. **Build and test**:
```bash
python build.py --firmware
```

---

## Testing Checklist

After implementing fixes, perform these tests:

### Build Verification
- [ ] Firmware compiles without errors
- [ ] Filesystem builds successfully
- [ ] No new compiler warnings introduced

### Functional Tests
- [ ] Web UI loads correctly
- [ ] Version mismatch detection works
- [ ] JavaScript cache busting works
- [ ] WiFi connection succeeds
- [ ] MQTT connection succeeds
- [ ] PIC firmware upgrade works (all types)

### Load Tests
- [ ] Open 3-5 browser tabs simultaneously
- [ ] Monitor heap usage (should stay above 8KB free)
- [ ] No watchdog resets during normal operation
- [ ] No exceptions in telnet debug log

### Security Tests
- [ ] Create malicious version.hash file
- [ ] Verify hash validation rejects it
- [ ] Verify no XSS execution in browser
- [ ] Check browser console for errors

### Memory Tests
```cpp
// Add to main loop temporarily for testing:
static unsigned long lastHeapReport = 0;
if (millis() - lastHeapReport > 5000) {
  DebugTf(PSTR("Free heap: %d bytes\r\n"), ESP.getFreeHeap());
  lastHeapReport = millis();
}
```

Expected Results:
- Free heap should stay **above 8KB** during normal operation
- Free heap should **not continuously decrease** (memory leak check)
- No sudden drops **below 4KB** (crash risk threshold)

---

## Rollback Plan

If issues are encountered after implementing fixes:

1. **Revert to commit before fixes**:
```bash
git revert HEAD
git push
```

2. **Or revert individual file**:
```bash
git checkout HEAD~1 -- src/libraries/OTGWSerial/OTGWSerial.cpp
git commit -m "Revert OTGWSerial.cpp changes"
```

3. **Document issue** in GitHub issue tracker

---

## Success Criteria

All fixes are successful if:

✅ All builds complete without errors  
✅ All functional tests pass  
✅ Free heap stays above 8KB under load  
✅ No watchdog resets during 24-hour test  
✅ Security validation rejects malicious input  
✅ PIC firmware upgrades work for all firmware types

---

## Post-Implementation

After successful implementation:

1. **Update ADRs** if architectural decisions were made
2. **Update version** to 1.0.0-rc8 (or 1.0.0 final)
3. **Create release notes** documenting fixes
4. **Archive this review** in `docs/reviews/`
5. **Update README.md** if user-facing changes

---

*Estimated Total Implementation Time: 80 minutes*  
*Estimated Total Testing Time: 120 minutes*  
*Total Project Time: ~3-4 hours*
