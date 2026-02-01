# Quick Reference: File Serving on ESP8266

**Based on Bug Fix**: Commit 2e93554 (Memory Management Improvement)

## File Size Decision Tree

```
Is the file > 2KB?
├─ NO → Can use readString() if needed
└─ YES → MUST use streaming
    ├─ Need to modify content?
    │   ├─ NO → Use httpServer.streamFile()
    │   └─ YES → Use chunked transfer encoding
    └─ File > 10KB?
        └─ YES → CRITICAL - Always stream (can crash if loaded)
```

## Pattern: Direct Streaming (Unmodified Files)

```cpp
File f = LittleFS.open("/file.html", "r");
if (!f) {
  httpServer.send(404, F("text/plain"), F("File not found"));
  return;
}
httpServer.streamFile(f, F("text/html; charset=UTF-8"));
f.close();
```

**Memory**: Minimal (~few hundred bytes)  
**Use When**: File doesn't need modification

## Pattern: Chunked Streaming (Modified Files)

```cpp
File f = LittleFS.open("/file.html", "r");
if (!f) {
  httpServer.send(404, F("text/plain"), F("File not found"));
  return;
}

httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
httpServer.send(200, F("text/html; charset=UTF-8"), F(""));

while (f.available()) {
  String line = f.readStringUntil('\n');
  
  // Modify line if needed
  if (line.indexOf(F("search_term")) >= 0) {
    line.replace(F("old"), "new");
  }
  
  httpServer.sendContent(line);
  if (f.available() || line.length() > 0) {
    httpServer.sendContent(F("\n"));
  }
}
httpServer.sendContent(F("")); // End stream
f.close();
```

**Memory**: ~100-500 bytes per line  
**Use When**: File needs content modification

## Pattern: Deduplicate Routes with Lambda

```cpp
// GOOD - Define once, use multiple times
auto sendIndex = []() {
  // Streaming implementation here
};

httpServer.on("/", sendIndex);
httpServer.on("/index", sendIndex);
httpServer.on("/index.html", sendIndex);
```

## Pattern: Cache Expensive Operations

```cpp
String getFileHash() {
  static String _hash = ""; // Persistent cache
  
  if (_hash.length() > 0) return _hash; // Return cached
  
  // Load only once
  File f = LittleFS.open("/version.hash", "r");
  if (f && f.available()) {
    _hash = f.readStringUntil('\n');
    _hash.trim();
    f.close();
  }
  return _hash;
}
```

## Memory Guidelines

| File Size | Guideline | Risk Level |
|-----------|-----------|------------|
| < 1KB     | Can use `readString()` if needed | ⚠️ Low |
| 1-5KB     | Prefer streaming | ⚠️ Medium |
| > 5KB     | MUST use streaming | 🔴 High |
| > 10KB    | CRITICAL - Always stream | 🔴 Critical |

## Why This Matters

**ESP8266 Memory**:
- Total RAM: ~80KB
- Available: ~40KB (after core libraries)

**Example**: Loading 11KB file
- File: 11KB
- Modifications: +11KB (String reallocations)
- **Peak Usage**: >22KB (>50% of available RAM!)

**Concurrent Requests**: 2-3 simultaneous = Memory exhaustion → Crash

## Common Mistakes to Avoid

### ❌ DON'T: Load entire file
```cpp
String html = f.readString();  // 11KB allocation
html.replace("foo", "bar");    // 11KB+ reallocation
httpServer.send(200, type, html);
```

### ✅ DO: Stream line-by-line
```cpp
while (f.available()) {
  String line = f.readStringUntil('\n');  // ~100 bytes
  if (line.indexOf(F("foo")) >= 0) {
    line.replace(F("foo"), "bar");
  }
  httpServer.sendContent(line + "\n");
}
```

### ❌ DON'T: Duplicate code
```cpp
httpServer.on("/", []() { /* same code */ });
httpServer.on("/index", []() { /* same code */ });
httpServer.on("/index.html", []() { /* same code */ });
```

### ✅ DO: Use lambda
```cpp
auto handler = []() { /* code once */ };
httpServer.on("/", handler);
httpServer.on("/index", handler);
httpServer.on("/index.html", handler);
```

### ❌ DON'T: Read file repeatedly
```cpp
String getHash() {
  File f = LittleFS.open("/hash", "r");  // File I/O every call!
  String h = f.readStringUntil('\n');
  f.close();
  return h;
}
```

### ✅ DO: Cache result
```cpp
String getHash() {
  static String _h = "";  // Static cache
  if (_h.length() > 0) return _h;
  
  File f = LittleFS.open("/hash", "r");  // File I/O once
  _h = f.readStringUntil('\n');
  f.close();
  return _h;
}
```

## Performance Tips

1. **Use `indexOf()` before `replace()`** - Avoid unnecessary allocations
2. **Limit String scope** - Let variables go out of scope quickly
3. **Prefer char buffers** - For fixed-size strings
4. **Always use `F()` macro** - Keep string literals in PROGMEM

## For More Information

- **Detailed Assessment**: `docs/reviews/2026-02-01_memory-management-bug-fix/BUG_FIX_ASSESSMENT.md`
- **Copilot Instructions**: `.github/copilot-instructions.md` (lines 193-320)
- **Real Bug Fix**: Commit 2e935543b9381566d77545559bffdde98475a3e7

---

**Keep this reference handy when implementing file serving in OTGW firmware!**
