# ADR-023: File System Explorer HTTP Architecture

**Status:** Accepted  
**Date:** 2019-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware uses LittleFS for storing configuration files, web UI assets, and sensor data. During development and troubleshooting, access to the filesystem is essential:
- Inspect configuration files
- Upload new web UI files
- Download logs or sensor data
- Backup/restore configurations
- Update firmware via upload
- Debug filesystem issues

**Requirements:**
- Browse filesystem contents
- Upload files to LittleFS
- Download files from LittleFS
- Delete files
- View file metadata (size, timestamp)
- Integration with OTA update mechanism
- Works via standard web browser (no special tools)

## Decision

**Implement HTTP-based file system explorer with streaming upload/download and integrated firmware update capability.**

**Architecture:**
- **Protocol:** HTTP (GET for download, POST for upload/delete)
- **UI:** HTML form-based interface served from LittleFS
- **Fallback:** Serve FSexplorer.html if index.html missing
- **Streaming:** Upload and download use streaming (no full-file buffering)
- **Security:** Path validation prevents directory traversal
- **Integration:** Shares HTTP server with REST API and Web UI
- **Firmware updates:** Integrated OTA update via `/update` endpoint

**Key endpoints:**
- `GET /FSexplorer.html` - File browser UI
- `GET /api/v1/files` - List files (JSON)
- `POST /api/v1/files/upload` - Upload file
- `POST /api/v1/files/delete` - Delete file
- `GET /api/v1/files/download?path=X` - Download file
- `POST /update` - OTA firmware update (multipart)

## Alternatives Considered

### Alternative 1: FTP Server
**Pros:**
- Standard protocol
- Widely supported (FileZilla, etc.)
- Binary mode support
- Directory operations

**Cons:**
- Additional server overhead (~8KB)
- Separate port (21 + data ports)
- More complex than HTTP
- Binary FTP has firewall issues
- Not browser-accessible

**Why not chosen:** HTTP is simpler and works in browsers. FTP adds unnecessary complexity.

### Alternative 2: WebDAV
**Pros:**
- Standard protocol (RFC 4918)
- Can mount as network drive
- Full filesystem operations
- Industry standard

**Cons:**
- Complex protocol
- Large implementation overhead
- Overkill for simple file access
- Not all browsers support well
- Authentication complexity

**Why not chosen:** Too complex for ESP8266. HTTP forms are sufficient.

### Alternative 3: TFTP (Trivial FTP)
**Pros:**
- Very simple protocol
- Lightweight
- Standard for embedded systems

**Cons:**
- UDP-based (unreliable)
- No authentication
- No directory listing
- Requires separate client
- Not browser-accessible

**Why not chosen:** Cannot use in browser. HTTP more versatile.

### Alternative 4: Custom Binary Protocol over WebSocket
**Pros:**
- Efficient binary transfer
- Real-time progress
- Bidirectional

**Cons:**
- Custom protocol (no standards)
- Requires custom client code
- More complex than HTTP forms
- Difficult to debug

**Why not chosen:** HTTP multipart is standard and works everywhere.

### Alternative 5: Serial File Transfer (XMODEM, YMODEM)
**Pros:**
- No network dependency
- Direct access via USB

**Cons:**
- Requires physical access
- Slow (115200 baud)
- Serial port used for PIC communication
- Not practical for deployed devices

**Why not chosen:** Remote access via network is essential.

## Consequences

### Positive
- **Browser-based:** Works in any web browser, no special software
- **Integrated:** Uses existing HTTP server, no additional port
- **Streaming:** Large files don't exhaust RAM
- **Standard:** HTTP multipart/form-data is well-understood
- **Debug-friendly:** Can inspect filesystem remotely
- **Backup:** Users can download all files for backup
- **Update:** Integrated firmware OTA update
- **Fallback UI:** Auto-serves file explorer if main UI missing

### Negative
- **No authentication:** Anyone on network can access files
  - Accepted: Local network trust model (see ADR-003)
- **Path length limit:** 30 characters (LittleFS limitation)
  - Accepted: Sufficient for /data/* structure
- **No directories:** LittleFS is flat, all files in root
  - Accepted: LittleFS design choice
- **File limit:** Display limited to 40 files
  - Mitigation: Sufficient for typical use case
- **No compression:** Files transferred uncompressed
  - Accepted: Files are small, bandwidth not constrained

### Risks & Mitigation
- **Directory traversal:** Malicious paths like `../../../etc/passwd`
  - **Mitigation:** Validate paths, restrict to LittleFS root
  - **Mitigation:** 30-char limit prevents long traversal paths
- **Disk full:** Uploading large files exhausts filesystem
  - **Mitigation:** Check available space before write
  - **Mitigation:** Stream write allows early abort
- **Concurrent uploads:** Multiple simultaneous uploads
  - **Accepted:** HTTP server is sequential, no concurrent requests
- **Malicious upload:** User uploads malware or corrupted files
  - **Accepted:** Local network trust model
  - **Mitigation:** Filesystem separate from main firmware

## Implementation Details

**File listing (JSON API):**
```cpp
void handleFileList() {
  Dir dir = LittleFS.openDir("/");
  
  DynamicJsonDocument doc(2048);
  JsonArray files = doc.createNestedArray("files");
  
  int count = 0;
  while (dir.next() && count < 40) {  // Limit to 40 files
    JsonObject file = files.createNestedObject();
    file["name"] = dir.fileName();
    file["size"] = dir.fileSize();
    count++;
  }
  
  doc["count"] = count;
  doc["total_bytes"] = LittleFS.totalBytes();
  doc["used_bytes"] = LittleFS.usedBytes();
  
  String response;
  serializeJson(doc, response);
  
  httpServer.send(200, F("application/json"), response);
}
```

**File upload (streaming):**
```cpp
void handleFileUpload() {
  HTTPUpload& upload = httpServer.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    // Start of upload
    String filename = upload.filename;
    
    // Validate filename length
    if (filename.length() > 30) {
      DebugTln(F("Filename too long"));
      return;
    }
    
    // Validate filename (no path traversal)
    if (filename.indexOf("..") >= 0) {
      DebugTln(F("Invalid filename"));
      return;
    }
    
    // Open file for writing
    String path = "/" + filename;
    uploadFile = LittleFS.open(path, "w");
    
    if (!uploadFile) {
      DebugTln(F("Failed to open file for writing"));
      return;
    }
    
    DebugTf(PSTR("Upload started: %s\r\n"), filename.c_str());
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write chunk to file (streaming)
    if (uploadFile) {
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      
      if (written != upload.currentSize) {
        DebugTln(F("Write error"));
      }
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    // Upload complete
    if (uploadFile) {
      uploadFile.close();
      DebugTf(PSTR("Upload complete: %d bytes\r\n"), upload.totalSize);
    }
  }
}
```

**File download (streaming):**
```cpp
void handleFileDownload() {
  String path = httpServer.arg("path");
  
  // Validate path
  if (path.length() == 0 || path.indexOf("..") >= 0) {
    httpServer.send(400, F("text/plain"), F("Invalid path"));
    return;
  }
  
  // Open file
  File file = LittleFS.open(path, "r");
  if (!file) {
    httpServer.send(404, F("text/plain"), F("File not found"));
    return;
  }
  
  // Determine content type
  String contentType = F("application/octet-stream");
  if (path.endsWith(".html")) contentType = F("text/html");
  else if (path.endsWith(".css")) contentType = F("text/css");
  else if (path.endsWith(".js")) contentType = F("application/javascript");
  else if (path.endsWith(".json")) contentType = F("application/json");
  
  // Stream file to client
  size_t sent = httpServer.streamFile(file, contentType);
  file.close();
  
  DebugTf(PSTR("Downloaded %s (%d bytes)\r\n"), path.c_str(), sent);
}
```

**File deletion:**
```cpp
void handleFileDelete() {
  String path = httpServer.arg("path");
  
  // Validate path
  if (path.length() == 0 || path.indexOf("..") >= 0) {
    httpServer.send(400, F("text/plain"), F("Invalid path"));
    return;
  }
  
  // Check file exists
  if (!LittleFS.exists(path)) {
    httpServer.send(404, F("text/plain"), F("File not found"));
    return;
  }
  
  // Delete file
  if (LittleFS.remove(path)) {
    httpServer.send(200, F("text/plain"), F("File deleted"));
    DebugTf(PSTR("Deleted: %s\r\n"), path.c_str());
  } else {
    httpServer.send(500, F("text/plain"), F("Delete failed"));
  }
}
```

**Fallback routing:**
```cpp
void handleNotFound() {
  String uri = httpServer.uri();
  
  // Try to serve from LittleFS
  if (LittleFS.exists(uri)) {
    File file = LittleFS.open(uri, "r");
    httpServer.streamFile(file, getContentType(uri));
    file.close();
    return;
  }
  
  // If index.html missing, serve FSexplorer.html instead
  if (uri == "/" || uri == "/index.html") {
    if (!LittleFS.exists("/index.html") && 
        LittleFS.exists("/FSexplorer.html")) {
      File file = LittleFS.open("/FSexplorer.html", "r");
      httpServer.streamFile(file, F("text/html"));
      file.close();
      return;
    }
  }
  
  // True 404
  httpServer.send(404, F("text/plain"), F("Not found"));
}
```

## Web UI (FSexplorer.html)

**Features:**
- File list with sizes
- Upload button
- Download links
- Delete buttons
- Available space display
- Firmware update section

**HTML structure:**
```html
<div id="filesystem">
  <h2>File System Explorer</h2>
  
  <div class="stats">
    Used: <span id="used">0</span> / <span id="total">0</span> bytes
  </div>
  
  <table id="fileList">
    <tr>
      <th>Filename</th>
      <th>Size</th>
      <th>Actions</th>
    </tr>
    <!-- Files populated via JavaScript -->
  </table>
  
  <form id="uploadForm" enctype="multipart/form-data">
    <input type="file" name="file" id="fileInput">
    <button type="submit">Upload</button>
  </form>
  
  <hr>
  
  <h3>Firmware Update</h3>
  <form id="updateForm" action="/update" method="POST" 
        enctype="multipart/form-data">
    <input type="file" name="firmware" accept=".bin">
    <button type="submit">Flash Firmware</button>
  </form>
</div>
```

**JavaScript:**
```javascript
// Load file list
fetch('/api/v1/files')
  .then(response => response.json())
  .then(data => {
    const table = document.getElementById('fileList');
    
    data.files.forEach(file => {
      const row = table.insertRow();
      row.innerHTML = `
        <td>${file.name}</td>
        <td>${file.size}</td>
        <td>
          <a href="/api/v1/files/download?path=/${file.name}">Download</a>
          <button onclick="deleteFile('${file.name}')">Delete</button>
        </td>
      `;
    });
    
    document.getElementById('used').textContent = data.used_bytes;
    document.getElementById('total').textContent = data.total_bytes;
  });

// Upload file
document.getElementById('uploadForm').addEventListener('submit', (e) => {
  e.preventDefault();
  
  const formData = new FormData();
  formData.append('file', document.getElementById('fileInput').files[0]);
  
  fetch('/api/v1/files/upload', {
    method: 'POST',
    body: formData
  })
  .then(() => location.reload())
  .catch(err => alert('Upload failed: ' + err));
});
```

## Firmware Update Integration

**OTA update handler:**
```cpp
httpServer.on("/update", HTTP_POST, 
  []() {
    // Update complete
    httpServer.send(200, F("text/plain"), 
      (Update.hasError()) ? F("Update failed") : F("Update success"));
    
    ESP.restart();
  },
  []() {
    // Upload handler
    HTTPUpload& upload = httpServer.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
      DebugTf(PSTR("Update: %s\r\n"), upload.filename.c_str());
      
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      
      if (!Update.begin(maxSketchSpace)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        DebugTf(PSTR("Update success: %u bytes\r\n"), upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  }
);
```

## Security Considerations

**Path validation:**
```cpp
bool isValidPath(const String& path) {
  // No empty paths
  if (path.length() == 0) return false;
  
  // No parent directory references
  if (path.indexOf("..") >= 0) return false;
  
  // Length limit
  if (path.length() > 30) return false;
  
  // Must start with /
  if (!path.startsWith("/")) return false;
  
  return true;
}
```

**Allowed operations:**
- Read any file (download)
- Write any file (upload)
- Delete any file
- List all files

**Not allowed:**
- Directory creation (LittleFS is flat)
- Rename/move (not implemented)
- Execute code from filesystem
- Access outside LittleFS

## Related Decisions
- ADR-008: LittleFS for Configuration Persistence (filesystem choice)
- ADR-003: HTTP-Only Network Architecture (no authentication)
- ADR-010: Multiple Concurrent Network Services (shares HTTP server)

## References
- Implementation: `FSexplorer.ino`
- Web UI: `data/FSexplorer.html`
- LittleFS documentation: https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
- Update handler: `OTGW-ModUpdateServer-impl.h`
