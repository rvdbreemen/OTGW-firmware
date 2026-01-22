# Browser-Side Memory Monitoring and Data Persistence

## Overview

This document outlines five scenarios for implementing memory monitoring and data persistence in the OTGW firmware web interface. Each scenario has different trade-offs in terms of complexity, reliability, and user experience.

## Current Implementation

- **Buffer Limit**: 2,000 lines (default) or 1,000,000 lines (capture mode)
- **Render Limit**: 2,000 lines maximum displayed at once
- **Memory Estimation**: ~200 bytes per log line (rough estimate)
- **File Streaming**: Available via File System Access API (Chrome/Edge)
- **Settings Persistence**: Stored on ESP8266 device via REST API

## Scenario 1: Enhanced Memory Monitoring with Browser Memory API

### Description
Add real-time browser memory monitoring using the Performance API and dynamic memory management.

### Implementation Details

**Memory Monitoring:**
- Use `performance.memory` API (Chrome/Edge) for actual heap usage
- Use `JSON.stringify(otLogBuffer).length` for accurate buffer size
- Calculate total JavaScript heap vs available heap
- Monitor at 1-second intervals

**Thresholds:**
- 🟢 Green: < 80% of safe limit (e.g., < 400MB)
- 🟡 Yellow: 80-95% (400-475MB)
- 🔴 Red: > 95% (> 475MB)

**Automatic Management:**
- When yellow: Warn user to enable capture mode or file streaming
- When red: Auto-trim oldest 25% of buffer
- Prevent browser crash by staying under 500MB total

**UI Elements:**
```
Memory: 45.2 MB / 500 MB 🟢 (Heap: 180 MB / 2048 MB)
```

### Pros
- Prevents browser crashes
- Real-time feedback
- No storage quota issues
- Works on all browsers (with graceful fallback)

### Cons
- `performance.memory` only available in Chrome/Edge
- Automatic trimming may lose data
- No data recovery after reload

### Browser Compatibility
- Chrome/Edge: Full support
- Firefox/Safari: Fallback to estimation only

---

## Scenario 2: IndexedDB for Persistent Log Storage

### Description
Use IndexedDB to persist all log data, enabling recovery after page reload or tab close.

### Implementation Details

**Database Schema:**
```javascript
Database: 'OTGW-Logs'
  Store: 'logs'
    - key: auto-increment ID
    - index: timestamp
    - data: {time, data, source, id, label, value}
  
  Store: 'sessions'
    - key: sessionId (date-based)
    - data: {start, end, lineCount}
  
  Store: 'metadata'
    - key: 'settings'
    - data: {maxLines, autoSave, retention}
```

**Auto-Save Strategy:**
- Save to IndexedDB every 10 seconds (batched)
- Save on page unload (beforeunload event)
- Keep last 7 days of data by default (configurable)

**Recovery:**
- On page load, check IndexedDB for recent session
- Prompt user: "Recover 15,234 log lines from previous session?"
- Load logs back into buffer with progress indicator

**Storage Management:**
- Monitor quota usage via `navigator.storage.estimate()`
- Alert when approaching quota (e.g., 80% of available)
- Auto-cleanup old sessions based on retention policy
- Manual cleanup option in UI

**UI Elements:**
```
[✓] Auto-save to browser storage (5.2 GB available)
[Recover Previous Session] - Last: 2026-01-21 23:45 (15,234 lines)
[Clean Up Old Sessions] (Using 245 MB of 5.2 GB)
```

### Pros
- Survives page reload and tab close
- Large storage capacity (typically > 1 GB)
- Can store millions of log lines
- Works offline
- Native browser API

### Cons
- More complex implementation
- Async API (requires promises)
- User can clear browser data
- Not available in private/incognito mode (some browsers)
- Slower than in-memory operations

### Browser Compatibility
- All modern browsers (Chrome, Firefox, Safari, Edge)

---

## Scenario 3: LocalStorage for Session State & Small Data

### Description
Use localStorage for UI state and a limited backup of recent log lines.

### Implementation Details

**What to Store:**
- UI state: filter, search term, scroll position, expanded state
- Last 1,000 log lines (compressed if possible)
- Session metadata: timestamp, device IP, firmware version

**Storage Strategy:**
```javascript
localStorage.setItem('otgw_ui_state', JSON.stringify({
  autoScroll: true,
  showTimestamps: true,
  searchTerm: '',
  scrollPos: 0,
  expanded: false
}));

localStorage.setItem('otgw_recent_logs', JSON.stringify({
  timestamp: Date.now(),
  device: '192.168.1.100',
  logs: otLogBuffer.slice(-1000)
}));
```

**Recovery:**
- Always restore UI state on load
- Offer to restore recent logs if < 5 minutes old
- Clear old data automatically

**Quota Management:**
- localStorage typically limited to 5-10 MB
- Compress data using simple run-length encoding
- Prioritize most recent logs
- Handle QuotaExceededError gracefully

**UI Elements:**
```
[✓] Remember UI preferences
[✓] Backup last 1000 lines (expires in 5 min)
```

### Pros
- Simple synchronous API
- Fast access
- Good for UI state
- Works in all browsers
- Persistent across sessions

### Cons
- Small storage limit (5-10 MB)
- Can only store ~10,000-50,000 log lines
- Synchronous (blocks main thread)
- No structured queries
- User can clear browser data

### Browser Compatibility
- All browsers (universal support)

---

## Scenario 4: Hybrid Approach (RECOMMENDED)

### Description
Combine localStorage, IndexedDB, and sessionStorage for optimal UX and reliability.

### Implementation Details

**Three-Tier Storage:**

1. **sessionStorage** (Temporary)
   - Current session UI state
   - Survives page refresh within same tab
   - Lost when tab closes
   - Use for: scroll position, temporary filters

2. **localStorage** (Persistent State)
   - User preferences (auto-scroll, timestamps, theme)
   - Last known device IP
   - Survives tab close and browser restart
   - Use for: settings that should persist

3. **IndexedDB** (Large Data)
   - All log lines (configurable limit)
   - Historical sessions
   - Survives tab close and browser restart
   - Use for: log buffer, long-term storage

**Smart Mode Algorithm:**
```javascript
if (otLogBuffer.length > 10000) {
  // Use IndexedDB for large datasets
  enableAutoSave('indexeddb');
} else if (otLogBuffer.length > 1000) {
  // Use localStorage for medium datasets
  enableAutoSave('localstorage');
} else {
  // In-memory only for small datasets
  enableAutoSave('none');
}
```

**Recovery Flow:**
```
1. Page loads
2. Restore UI state from localStorage (instant)
3. Check sessionStorage for current tab data
4. Check IndexedDB for recent session (< 1 hour old)
5. If found, show banner: "Recover previous session? [Yes] [No]"
6. If yes, load from IndexedDB with progress bar
7. Resume WebSocket and continue live updates
```

**Storage Monitoring:**
```javascript
async function checkStorageStatus() {
  // Check IndexedDB quota
  const estimate = await navigator.storage.estimate();
  const percent = (estimate.usage / estimate.quota) * 100;
  
  // Check localStorage size
  const localSize = JSON.stringify(localStorage).length;
  const localLimit = 5 * 1024 * 1024; // 5MB typical
  
  return {
    indexedDB: { used: estimate.usage, total: estimate.quota, percent },
    localStorage: { used: localSize, limit: localLimit }
  };
}
```

**UI Elements:**
```
Storage Status:
  Browser DB: 245 MB / 5.2 GB (4.7%) 🟢
  Quick Cache: 2.1 MB / 5 MB (42%) 🟢
  
[✓] Auto-save logs (Smart Mode)
[▼] Advanced Storage Options
    ( ) Disabled
    (•) Smart Mode (Recommended)
    ( ) Always use IndexedDB
    ( ) Always use LocalStorage only
    
Retention: [7 days ▼] [Clean Up Now]

[Recover Previous Session] (18,456 lines from 23:45)
```

### Pros
- Best of all worlds
- Optimal performance for each use case
- Maximum reliability
- Graceful degradation
- User control over behavior

### Cons
- Most complex implementation
- Requires careful testing
- More code to maintain

### Browser Compatibility
- Full features: Chrome, Firefox, Safari, Edge (all modern browsers)
- Graceful fallback for older browsers

---

## Scenario 5: Enhanced File System Access API with Memory Management

### Description
Extend the existing file streaming feature to manage memory by offloading old logs to files.

### Implementation Details

**Memory-Aware Streaming:**
```javascript
const MEMORY_THRESHOLD = 400 * 1024 * 1024; // 400 MB

if (estimatedMemoryUsage > MEMORY_THRESHOLD && !isStreamingToFile) {
  // Prompt user to enable file streaming
  promptEnableFileStreaming();
}

// Or auto-offload old logs
if (estimatedMemoryUsage > MEMORY_THRESHOLD && isStreamingToFile) {
  // Keep only last 10,000 lines in memory
  // Older lines already in file
  otLogBuffer = otLogBuffer.slice(-10000);
}
```

**Recovery from Files:**
- Directory handle can be re-requested on page load
- User grants permission again
- Read today's log file back into buffer
- Continue streaming

**Hybrid Memory + File:**
```
┌─────────────────────────────────────┐
│ Browser Memory (Last 10k lines)     │ ← Fast access for UI
├─────────────────────────────────────┤
│ Local File (Older lines)            │ ← Offloaded to disk
└─────────────────────────────────────┘
```

**UI Elements:**
```
Memory: 45 MB / 500 MB 🟢
File: otmonitor-22-01-2026.json (125 MB)

[✓] Stream to file
[✓] Auto-offload when memory > 400 MB
[Load More from File...] ← Reads older lines into buffer
```

### Pros
- Unlimited storage (disk space)
- Existing implementation to build on
- User has files for external tools
- Memory stays bounded
- No browser quota issues

### Cons
- Requires user permission each session
- Chrome/Edge desktop only
- Cannot auto-resume file handle
- More complex UI flow
- Files scattered across sessions

### Browser Compatibility
- Chrome/Edge desktop: Full support
- Other browsers: Not available (show alternative)

---

## Comparison Matrix

| Feature | Scenario 1 | Scenario 2 | Scenario 3 | Scenario 4 | Scenario 5 |
|---------|-----------|-----------|-----------|-----------|-----------|
| Prevents crashes | ✅ | ⚠️ | ⚠️ | ✅ | ✅ |
| Survives reload | ❌ | ✅ | ✅ | ✅ | ⚠️ |
| Survives tab close | ❌ | ✅ | ✅ | ✅ | ⚠️ |
| Large data (GB+) | ❌ | ✅ | ❌ | ✅ | ✅ |
| Browser compat | ✅ | ✅ | ✅ | ✅ | ⚠️ |
| Implementation | Simple | Medium | Simple | Complex | Medium |
| User control | Medium | High | Low | High | High |
| Performance | ✅ | ⚠️ | ✅ | ✅ | ⚠️ |

Legend: ✅ Excellent | ⚠️ Limited | ❌ No support

---

## Recommendation: Scenario 4 (Hybrid Approach)

The **Hybrid Approach** is recommended because it:

1. **Prevents Browser Crashes**: Memory monitoring from Scenario 1
2. **Enables Recovery**: IndexedDB persistence from Scenario 2
3. **Fast UI State**: localStorage from Scenario 3
4. **Works Everywhere**: Graceful fallback for all browsers
5. **User Control**: Smart defaults with advanced options

### Implementation Priority

**Phase 1 (Critical):**
- Enhanced memory monitoring (Scenario 1)
- localStorage for UI state (Scenario 3)
- Basic recovery prompt

**Phase 2 (Important):**
- IndexedDB implementation (Scenario 2)
- Smart mode selection
- Retention policies

**Phase 3 (Nice-to-have):**
- Enhanced file streaming (Scenario 5)
- Advanced storage analytics
- Import/export features

---

## Code Examples

### Memory Monitoring

```javascript
// Get actual memory usage (Chrome/Edge only)
function getMemoryUsage() {
  if (performance.memory) {
    return {
      used: performance.memory.usedJSHeapSize,
      total: performance.memory.totalJSHeapSize,
      limit: performance.memory.jsHeapSizeLimit
    };
  }
  // Fallback: estimate from buffer size
  const bufferSize = JSON.stringify(otLogBuffer).length;
  return {
    used: bufferSize,
    total: bufferSize * 2, // estimate
    limit: 500 * 1024 * 1024 // 500MB guess
  };
}

// Monitor and display
function updateMemoryDisplay() {
  const mem = getMemoryUsage();
  const percent = (mem.used / mem.limit) * 100;
  
  const el = document.getElementById('memUsageDetailed');
  const indicator = percent < 80 ? '🟢' : percent < 95 ? '🟡' : '🔴';
  
  el.innerHTML = `${(mem.used / 1024 / 1024).toFixed(1)} MB / ` +
                 `${(mem.limit / 1024 / 1024).toFixed(0)} MB ${indicator}`;
  
  // Auto-trim if in danger zone
  if (percent > 95 && !isStreamingToFile) {
    const toRemove = Math.floor(otLogBuffer.length * 0.25);
    otLogBuffer.splice(0, toRemove);
    console.warn(`Memory critical: removed ${toRemove} old log lines`);
  }
}
```

### IndexedDB Wrapper

```javascript
class LogDatabase {
  constructor() {
    this.db = null;
    this.dbName = 'OTGW-Logs';
    this.version = 1;
  }
  
  async init() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, this.version);
      
      request.onerror = () => reject(request.error);
      request.onsuccess = () => {
        this.db = request.result;
        resolve(this.db);
      };
      
      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        
        // Create object stores
        if (!db.objectStoreNames.contains('logs')) {
          const store = db.createObjectStore('logs', { autoIncrement: true });
          store.createIndex('timestamp', 'time', { unique: false });
          store.createIndex('sessionId', 'sessionId', { unique: false });
        }
        
        if (!db.objectStoreNames.contains('sessions')) {
          db.createObjectStore('sessions', { keyPath: 'id' });
        }
      };
    });
  }
  
  async saveLogs(logs, sessionId) {
    const tx = this.db.transaction(['logs'], 'readwrite');
    const store = tx.objectStore('logs');
    
    for (const log of logs) {
      store.add({ ...log, sessionId });
    }
    
    return new Promise((resolve, reject) => {
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(tx.error);
    });
  }
  
  async getRecentSession() {
    const tx = this.db.transaction(['sessions'], 'readonly');
    const store = tx.objectStore('sessions');
    const sessions = await this.getAllFromStore(store);
    
    // Return most recent session < 1 hour old
    const oneHourAgo = Date.now() - 3600000;
    return sessions
      .filter(s => s.timestamp > oneHourAgo)
      .sort((a, b) => b.timestamp - a.timestamp)[0];
  }
  
  async loadLogsForSession(sessionId) {
    const tx = this.db.transaction(['logs'], 'readonly');
    const store = tx.objectStore('logs');
    const index = store.index('sessionId');
    const request = index.getAll(sessionId);
    
    return new Promise((resolve, reject) => {
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }
  
  async cleanOldSessions(retentionDays = 7) {
    const cutoff = Date.now() - (retentionDays * 24 * 60 * 60 * 1000);
    const tx = this.db.transaction(['sessions', 'logs'], 'readwrite');
    
    // Delete old sessions and their logs
    // ... implementation ...
  }
  
  async getAllFromStore(store) {
    return new Promise((resolve, reject) => {
      const request = store.getAll();
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }
}

// Usage
const logDB = new LogDatabase();
await logDB.init();
await logDB.saveLogs(otLogBuffer, currentSessionId);
```

### localStorage State Management

```javascript
// Save UI state
function saveUIState() {
  const state = {
    autoScroll,
    showTimestamps,
    logExpanded,
    searchTerm,
    captureMode: maxLogLines === MAX_LOG_LINES_CAPTURE,
    lastUpdate: Date.now()
  };
  
  try {
    localStorage.setItem('otgw_ui_state', JSON.stringify(state));
  } catch (e) {
    console.warn('Failed to save UI state:', e);
  }
}

// Restore UI state
function restoreUIState() {
  try {
    const json = localStorage.getItem('otgw_ui_state');
    if (!json) return;
    
    const state = JSON.parse(json);
    
    // Apply state
    autoScroll = state.autoScroll ?? true;
    showTimestamps = state.showTimestamps ?? true;
    logExpanded = state.logExpanded ?? false;
    searchTerm = state.searchTerm ?? '';
    
    // Update UI elements
    const btnAutoScroll = document.getElementById('btnAutoScroll');
    if (btnAutoScroll) {
      btnAutoScroll.classList.toggle('btn-active', autoScroll);
    }
    
    const chkTimestamp = document.getElementById('chkShowTimestamp');
    if (chkTimestamp) {
      chkTimestamp.checked = showTimestamps;
    }
    
    // ... etc
  } catch (e) {
    console.warn('Failed to restore UI state:', e);
  }
}

// Save recent logs as backup
function saveRecentLogsBackup() {
  const backup = {
    timestamp: Date.now(),
    device: window.location.hostname,
    logs: otLogBuffer.slice(-1000) // Last 1000 lines
  };
  
  try {
    localStorage.setItem('otgw_logs_backup', JSON.stringify(backup));
  } catch (e) {
    if (e.name === 'QuotaExceededError') {
      // Try with fewer lines
      backup.logs = otLogBuffer.slice(-500);
      try {
        localStorage.setItem('otgw_logs_backup', JSON.stringify(backup));
      } catch (e2) {
        console.warn('Cannot save logs backup: quota exceeded');
      }
    }
  }
}
```

---

## Storage Quota API

```javascript
// Check storage quota (all browsers)
async function checkStorageQuota() {
  if ('storage' in navigator && 'estimate' in navigator.storage) {
    const estimate = await navigator.storage.estimate();
    return {
      usage: estimate.usage,
      quota: estimate.quota,
      percent: (estimate.usage / estimate.quota) * 100,
      available: estimate.quota - estimate.usage
    };
  }
  return null;
}

// Request persistent storage (optional)
async function requestPersistentStorage() {
  if ('storage' in navigator && 'persist' in navigator.storage) {
    const isPersisted = await navigator.storage.persist();
    console.log(`Persistent storage granted: ${isPersisted}`);
    return isPersisted;
  }
  return false;
}
```

---

## Testing Checklist

- [ ] Test memory monitoring with 10k, 100k, 500k, 1M lines
- [ ] Test IndexedDB save/restore across page reloads
- [ ] Test localStorage with quota exceeded scenarios
- [ ] Test recovery after tab close (IndexedDB)
- [ ] Test recovery after browser restart (IndexedDB)
- [ ] Test on Chrome, Firefox, Safari, Edge
- [ ] Test on mobile browsers
- [ ] Test in incognito/private mode
- [ ] Test with browser data clearing
- [ ] Test concurrent tabs (data sync)
- [ ] Test storage cleanup (retention policy)
- [ ] Measure performance impact (save frequency)
- [ ] Test graceful degradation (API not available)

---

## Future Enhancements

1. **Cloud Sync** (optional): Sync to user's cloud storage (Google Drive, Dropbox)
2. **Compression**: Use LZ-string or similar for localStorage efficiency
3. **Encryption**: Encrypt logs in IndexedDB (if sensitive data)
4. **Multi-device**: Sync state across devices (WebRTC or server)
5. **Export Formats**: CSV, JSON, SQLite export options
6. **Search Index**: Full-text search index in IndexedDB
7. **Statistics**: Historical statistics stored in IndexedDB

