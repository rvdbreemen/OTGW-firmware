# Future Enhancement Plan - Browser Memory Monitoring & Data Persistence

## Overview

This document outlines the roadmap for future enhancements to the OTGW firmware browser-side memory monitoring and data persistence features. The core implementation (Phases 1-4) is complete. This plan covers additional features and improvements that can be implemented incrementally.

## Current Status ✅

### Implemented Features (Phases 1-4)
- ✅ **Phase 1**: Enhanced memory monitoring with automatic crash prevention
- ✅ **Phase 2**: IndexedDB persistent storage with session recovery
- ✅ **Phase 3**: localStorage for UI state and backup
- ✅ **Phase 4**: Hybrid smart mode with automatic storage selection

## Future Development Roadmap

### Phase 5: Advanced User Controls
**Priority**: High  
**Estimated Effort**: Medium (2-3 weeks)  
**Status**: ✅ **COMPLETE**

#### Features
1. **Storage Mode Selector**
   - UI dropdown to manually select storage mode
   - Options: Disabled, Smart (auto), Always IndexedDB, Always localStorage
   - Save preference to device settings via API
   - Visual indicator showing current active mode

2. **Retention Policy Configuration**
   - UI selector for retention period (1, 7, 14, 30, 90 days, or Forever)
   - Default remains 7 days
   - Visual indicator showing next cleanup date
   - Manual "Clean Up Now" button

3. **Storage Usage Dashboard**
   - Detailed breakdown of storage usage by type
   - Visual charts showing:
     - IndexedDB usage (by session)
     - localStorage usage
     - File streaming disk usage (if enabled)
   - Session browser with metadata (date, size, line count)
   - Individual session deletion capability

4. **Manual Cleanup Controls**
   - "Clean Up Old Sessions" button with confirmation
   - "Clear All Stored Data" button (nuclear option)
   - "Export Before Cleanup" option
   - Progress indication during cleanup operations

#### Implementation Notes
```javascript
// New UI elements to add
<div class="storage-controls">
  <label>Storage Mode:
    <select id="storageMode">
      <option value="disabled">Disabled</option>
      <option value="smart" selected>Smart (Recommended)</option>
      <option value="indexeddb">Always IndexedDB</option>
      <option value="localstorage">Always localStorage</option>
    </select>
  </label>
  
  <label>Retention Period:
    <select id="retentionPeriod">
      <option value="1">1 Day</option>
      <option value="7" selected>7 Days</option>
      <option value="14">14 Days</option>
      <option value="30">30 Days</option>
      <option value="90">90 Days</option>
      <option value="0">Forever</option>
    </select>
  </label>
  
  <button id="btnCleanupNow">Clean Up Old Sessions</button>
  <button id="btnViewSessions">Browse Sessions</button>
</div>

<div id="storageUsageChart" class="storage-dashboard">
  <!-- Canvas for chart.js visualization -->
</div>
```

#### Dependencies
- Consider using Chart.js or similar for visualizations
- Add modal/dialog component for session browser

---

### Phase 6: Enhanced File Streaming Integration
**Priority**: High  
**Estimated Effort**: Medium (2-3 weeks)  
**Status**: Not Started

#### Features
1. **Memory-Aware Auto-Offload**
   - Automatically enable file streaming when memory reaches 70%
   - Offload oldest 50% of logs to file when threshold hit
   - Keep most recent logs in memory for fast access
   - User notification when auto-offload occurs

2. **File-Based Recovery**
   - Remember directory handle across sessions (request permission on load)
   - "Load from Previous File" option in recovery banner
   - Merge file data with IndexedDB data for complete history
   - Import log files from any source

3. **Hybrid Memory + File View**
   - "Load More from File" button to fetch older logs
   - Seamless scrolling between in-memory and file-based logs
   - Visual indicator showing data source (memory vs file)
   - Lazy loading of file chunks

4. **Multi-File Management**
   - Automatic file rotation at configurable size (e.g., 100 MB)
   - File browser showing all saved log files
   - Individual file export/import/delete
   - File compression option (gzip)

#### Implementation Notes
```javascript
// Auto-offload when memory high
const MEMORY_OFFLOAD_THRESHOLD = 0.7; // 70%

async function checkMemoryAndOffload() {
  const mem = getMemoryUsage();
  const percent = mem.heapUsed / mem.heapLimit;
  
  if (percent > MEMORY_OFFLOAD_THRESHOLD && !isStreamingToFile) {
    const userConsent = await confirm(
      'Memory usage is high. Enable file streaming to offload old logs?'
    );
    
    if (userConsent) {
      await startFileStreaming();
      await offloadOldLogsToFile();
    }
  }
}

async function offloadOldLogsToFile() {
  const cutoff = Math.floor(otLogBuffer.length / 2);
  const toOffload = otLogBuffer.splice(0, cutoff);
  
  // Write to file
  for (const log of toOffload) {
    await writeToStream(log);
  }
  
  console.log(`Offloaded ${toOffload.length} logs to file`);
}
```

#### Dependencies
- Requires File System Access API (Chrome/Edge only)
- Fallback behavior for unsupported browsers

---

### Phase 7: Data Compression
**Priority**: Medium  
**Estimated Effort**: Low (1 week)  
**Status**: Not Started

#### Features
1. **localStorage Compression**
   - Use LZ-string library for compressing log data
   - Compress before saving to localStorage
   - Decompress on load
   - Increase effective storage capacity 3-5x

2. **IndexedDB Compression** (Optional)
   - Compress large log entries before storing
   - Trade-off: CPU time vs storage space
   - Configurable per user preference
   - Monitor compression ratio and adjust

3. **File Compression**
   - Save log files as gzip by default
   - Option to export as plain JSON or compressed
   - Automatic decompression on import

#### Implementation Notes
```javascript
// Using LZ-string for compression
import LZString from 'lz-string';

function saveRecentLogsToLocalStorage() {
  const backup = {
    timestamp: Date.now(),
    device: window.location.hostname,
    logs: otLogBuffer.slice(-1000)
  };
  
  try {
    // Compress before saving
    const compressed = LZString.compress(JSON.stringify(backup));
    localStorage.setItem('otgw_logs_backup', compressed);
    
    const ratio = compressed.length / JSON.stringify(backup).length;
    console.log(`Compression ratio: ${(ratio * 100).toFixed(1)}%`);
  } catch (e) {
    console.error('Error saving compressed backup:', e);
  }
}

function loadRecentLogsFromLocalStorage() {
  try {
    const compressed = localStorage.getItem('otgw_logs_backup');
    if (!compressed) return null;
    
    // Decompress
    const json = LZString.decompress(compressed);
    const backup = JSON.parse(json);
    
    // Age check...
    return backup.logs;
  } catch (e) {
    console.error('Error loading compressed backup:', e);
    return null;
  }
}
```

#### Dependencies
- LZ-string library (~3KB minified)
- Add to project via CDN or npm

---

### Phase 8: Export/Import Functionality
**Priority**: Medium  
**Estimated Effort**: Medium (1-2 weeks)  
**Status**: ✅ Partially Complete (JSON/CSV Export Implemented)

#### Features
1. **Multiple Export Formats**
   - ✅ JSON (structured, full fidelity) - COMPLETE
     - Includes metadata (device, timestamp, version)
     - Parsed OpenTherm fields (msgType, dataId, value)
     - ISO and local timestamps
   - ✅ CSV (tabular, for Excel/analysis) - COMPLETE
     - Headers: Timestamp, TimestampISO, MessageType, DataID, Value, Raw, Formatted
     - Proper CSV escaping for special characters
   - ✅ Plain text (human-readable) - COMPLETE (existing)
   - ⏳ SQLite database (advanced users) - TODO
   - ⏳ OpenTherm Monitor compatible format - TODO

2. **Export Options** ✅ Basic Complete
   - ✅ Export current buffer (all formats)
   - ✅ Dropdown menu for format selection in UI
   - ⏳ Export specific session from IndexedDB - TODO
   - ⏳ Export date range - TODO
   - ⏳ Export filtered logs only - TODO
   - ⏳ Export with or without metadata - TODO

3. **Import Functionality** ⏳ TODO
   - Import JSON/CSV files
   - Append or replace current buffer
   - Validate format before import
   - Preview before final import
   - Merge with existing data

4. **Batch Operations** ⏳ TODO
   - Export all sessions at once
   - Import multiple files
   - Automated backups (daily/weekly)
   - Cloud backup integration (future)

#### Implementation Notes
```javascript
function exportLogsAsFormat(format, logs) {
  let content, mimeType, extension;
  
  switch (format) {
    case 'json':
      content = JSON.stringify(logs, null, 2);
      mimeType = 'application/json';
      extension = 'json';
      break;
      
    case 'csv':
      content = convertToCSV(logs);
      mimeType = 'text/csv';
      extension = 'csv';
      break;
      
    case 'txt':
      content = logs.map(l => formatLogLine(l.data)).join('\n');
      mimeType = 'text/plain';
      extension = 'txt';
      break;
      
    case 'sqlite':
      content = convertToSQLite(logs);
      mimeType = 'application/x-sqlite3';
      extension = 'db';
      break;
  }
  
  const blob = new Blob([content], { type: mimeType });
  const filename = `otgw-logs-${getDateString()}.${extension}`;
  
  // Trigger download
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = filename;
  a.click();
  URL.revokeObjectURL(a.href);
}

function convertToCSV(logs) {
  const headers = ['Timestamp', 'Source', 'ID', 'Label', 'Value', 'Data'];
  const rows = logs.map(log => [
    log.time,
    log.data.source || '',
    log.data.id || '',
    log.data.label || '',
    log.data.value || '',
    JSON.stringify(log.data)
  ]);
  
  return [headers, ...rows]
    .map(row => row.map(escapeCSV).join(','))
    .join('\n');
}
```

#### Dependencies
- None (pure JavaScript)
- Optional: sql.js for SQLite export

---

### Phase 9: Data Encryption
**Priority**: Low  
**Estimated Effort**: Medium (2 weeks)  
**Status**: Not Started

#### Features
1. **Optional Encryption**
   - User-configurable encryption toggle
   - Password-protected log storage
   - Encrypt IndexedDB data at rest
   - Encrypt localStorage backup
   - Encrypt exported files

2. **Encryption Methods**
   - Use Web Crypto API (AES-GCM)
   - Key derivation from user password (PBKDF2)
   - Salt stored with encrypted data
   - No plaintext storage of key

3. **User Experience**
   - Password prompt on first access
   - "Remember password" option (sessionStorage)
   - Password change functionality
   - Export with or without encryption

#### Implementation Notes
```javascript
// Encryption using Web Crypto API
async function encryptData(data, password) {
  const enc = new TextEncoder();
  const keyMaterial = await crypto.subtle.importKey(
    'raw',
    enc.encode(password),
    'PBKDF2',
    false,
    ['deriveBits', 'deriveKey']
  );
  
  const salt = crypto.getRandomValues(new Uint8Array(16));
  const key = await crypto.subtle.deriveKey(
    {
      name: 'PBKDF2',
      salt: salt,
      iterations: 100000,
      hash: 'SHA-256'
    },
    keyMaterial,
    { name: 'AES-GCM', length: 256 },
    false,
    ['encrypt']
  );
  
  const iv = crypto.getRandomValues(new Uint8Array(12));
  const encrypted = await crypto.subtle.encrypt(
    { name: 'AES-GCM', iv: iv },
    key,
    enc.encode(JSON.stringify(data))
  );
  
  return {
    salt: Array.from(salt),
    iv: Array.from(iv),
    data: Array.from(new Uint8Array(encrypted))
  };
}

async function decryptData(encryptedObj, password) {
  // Reverse of encryption process
  // ...
}
```

#### Security Considerations
- ⚠️ Password is only as strong as user chooses
- ⚠️ Lost password = lost data (no recovery)
- ⚠️ Performance impact on large datasets
- ⚠️ Consider if encryption is necessary for local network use

---

### Phase 10: Cloud Sync (Optional)
**Priority**: Low  
**Estimated Effort**: High (3-4 weeks)  
**Status**: Not Started

#### Features
1. **Cloud Storage Integration**
   - Google Drive integration
   - Dropbox integration
   - OneDrive integration
   - Generic WebDAV support

2. **Sync Behavior**
   - Optional background sync
   - Manual sync button
   - Conflict resolution
   - Bandwidth-aware (don't sync on metered connections)

3. **Multi-Device Support**
   - Same account across devices
   - Last-write-wins conflict resolution
   - Device-specific settings
   - Cloud-based session recovery

#### Implementation Notes
```javascript
// Example: Google Drive integration
async function syncToGoogleDrive() {
  // Requires OAuth2 authentication
  const auth = await authenticateWithGoogle();
  
  // Export current state
  const data = {
    sessions: await getAllSessions(),
    settings: STORAGE_CONFIG,
    timestamp: Date.now()
  };
  
  // Upload to Drive
  const response = await fetch('https://www.googleapis.com/upload/drive/v3/files', {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${auth.access_token}`,
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      name: `otgw-backup-${Date.now()}.json`,
      mimeType: 'application/json',
      parents: ['otgw-backups']
    })
  });
  
  console.log('Synced to Google Drive');
}
```

#### Considerations
- ⚠️ Requires OAuth setup and API keys
- ⚠️ Privacy implications of cloud storage
- ⚠️ Network latency and reliability
- ⚠️ Cost considerations (API quotas)
- ⚠️ May not be needed for local network use case

---

### Phase 11: Advanced Search & Analytics
**Priority**: Medium  
**Estimated Effort**: Medium (2-3 weeks)  
**Status**: Not Started

#### Features
1. **Full-Text Search Index**
   - Index all log messages in IndexedDB
   - Fast search across millions of logs
   - Regular expression support
   - Field-specific search (source, ID, label, value)

2. **Search UI**
   - Advanced search modal
   - Query builder interface
   - Search history
   - Saved searches/filters

3. **Historical Analytics**
   - Statistics over time
   - Message frequency analysis
   - Value trend charts
   - Anomaly detection
   - Export analytics reports

4. **Visualization**
   - Interactive charts (Chart.js or D3.js)
   - Time-series graphs
   - Heatmaps
   - Correlation analysis

#### Implementation Notes
```javascript
// Create search index in IndexedDB
async function buildSearchIndex() {
  const tx = logDatabase.db.transaction(['logs'], 'readonly');
  const store = tx.objectStore('logs');
  const allLogs = await store.getAll();
  
  const index = {};
  allLogs.forEach((log, id) => {
    const text = formatLogLine(log.data).toLowerCase();
    const words = text.split(/\s+/);
    
    words.forEach(word => {
      if (!index[word]) index[word] = [];
      index[word].push(id);
    });
  });
  
  // Store index
  await logDatabase.saveMetadata('search_index', index);
}

function searchLogs(query) {
  const index = await logDatabase.loadMetadata('search_index');
  const words = query.toLowerCase().split(/\s+/);
  
  // Find logs matching all words (AND query)
  const results = words.reduce((acc, word) => {
    const ids = index[word] || [];
    return acc.length === 0 ? ids : acc.filter(id => ids.includes(id));
  }, []);
  
  return results.map(id => otLogBuffer[id]);
}
```

---

### Phase 12: Performance Optimizations
**Priority**: Medium  
**Estimated Effort**: Ongoing  
**Status**: Not Started

#### Areas for Optimization

1. **Virtual Scrolling**
   - Render only visible log lines
   - Dramatically improve performance with large buffers
   - Libraries: react-window, virtual-scroller

2. **Web Workers**
   - Move IndexedDB operations to worker thread
   - Non-blocking UI during saves
   - Background search indexing

3. **Batch Operations**
   - Batch DOM updates
   - Batch IndexedDB writes
   - Debounced search

4. **Memory Pooling**
   - Reuse log entry objects
   - Reduce garbage collection pressure
   - Object pooling for frequently created objects

5. **Code Splitting**
   - Lazy load IndexedDB code
   - Lazy load search features
   - Reduce initial page load time

---

## Implementation Guidelines

### Development Workflow
1. Create feature branch from main
2. Implement phase incrementally
3. Write tests for new functionality
4. Update documentation
5. Submit PR for review
6. Merge after approval

### Testing Requirements
- Unit tests for new functions
- Integration tests for storage operations
- Browser compatibility testing
- Performance benchmarking
- User acceptance testing

### Documentation Updates
- Update BROWSER_MEMORY_SCENARIOS.md with new scenarios
- Update user guide with new features
- Add inline code documentation
- Update configuration examples

### Backward Compatibility
- Maintain compatibility with existing data
- Provide migration paths for schema changes
- Don't break existing functionality
- Feature flags for experimental features

---

## Priority Matrix

| Phase | Priority | Effort | Value | Status |
|-------|----------|--------|-------|--------|
| 5 - User Controls | High | Medium | High | Not Started |
| 6 - File Streaming | High | Medium | High | Not Started |
| 7 - Compression | Medium | Low | Medium | Not Started |
| 8 - Export/Import | Medium | Medium | High | Not Started |
| 9 - Encryption | Low | Medium | Low | Not Started |
| 10 - Cloud Sync | Low | High | Low | Not Started |
| 11 - Search/Analytics | Medium | Medium | Medium | Not Started |
| 12 - Performance | Medium | Ongoing | High | Not Started |

---

## Success Metrics

### User Adoption
- % of users enabling persistent storage
- % of users using recovery feature
- User feedback and satisfaction

### Performance
- Page load time impact
- Memory usage over time
- IndexedDB operation latency
- Search performance

### Reliability
- Data recovery success rate
- Browser crash incidents
- Storage quota issues
- Data integrity checks

---

## Resources Needed

### Development
- Frontend developer time
- Testing time
- Code review time

### Infrastructure
- Test devices (various browsers)
- Performance testing tools
- Analytics for user behavior

### Documentation
- Technical writing
- User guides
- API documentation
- Video tutorials (optional)

---

## Risk Assessment

### Technical Risks
- Browser API changes/deprecation
- Storage quota variations across browsers
- Performance with very large datasets
- Browser compatibility issues

### Mitigation Strategies
- Regular browser compatibility testing
- Progressive enhancement approach
- Feature detection and graceful fallback
- Performance monitoring and optimization
- User feedback collection

---

## Timeline Estimate

**Aggressive (full-time development):**
- Phase 5: 2-3 weeks
- Phase 6: 2-3 weeks
- Phase 7: 1 week
- Phase 8: 1-2 weeks
- Total: ~2-3 months

**Realistic (part-time development):**
- Phase 5: 1-2 months
- Phase 6: 1-2 months
- Phase 7: 2 weeks
- Phase 8: 3-4 weeks
- Total: ~4-6 months

**Conservative (as time permits):**
- Implement features as needed based on user feedback
- Prioritize highest value items first
- Timeline: 6-12 months

---

## Conclusion

This plan provides a roadmap for enhancing the OTGW firmware browser memory monitoring and data persistence features. The phases are designed to be implemented incrementally, with each adding significant value independently.

**Recommended Next Steps:**
1. Gather user feedback on current implementation
2. Prioritize features based on user needs
3. Start with Phase 5 (User Controls) for immediate value
4. Implement Phase 6 (File Streaming) for power users
5. Add other features based on demand

**Key Principle:** Build features incrementally based on real user needs rather than implementing everything at once.

---

*Last Updated: 2026-01-22*  
*Status: Living document - update as priorities change*
