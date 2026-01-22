# Memory Monitoring & Data Persistence Demo

## Overview

This document demonstrates the enhanced memory monitoring and data persistence features implemented in the OTGW firmware web interface.

## Features Implemented

### 1. Real-Time Memory Monitoring

**Location**: OpenTherm Monitor footer

**Display Elements**:
- Basic memory usage: `Mem: 45.2 MB`
- Detailed memory status: `Buffer: 45.1 MB | Heap: 180.5 / 2048 MB (8.8%) 🟢`
- Storage quota: `Storage: 245.3 MB / 5.2 GB (4.7%) 🟢`

**Status Indicators**:
- 🟢 Green: < 80% of safe limit (healthy)
- 🟡 Yellow: 80-95% (warning)
- 🔴 Red: > 95% (critical)

**Browser Support**:
- Chrome/Edge: Full support with `performance.memory` API
- Firefox/Safari: Estimation-based (buffer size calculation)

### 2. Automatic Memory Management

**Warning Thresholds**:
- **80% (Yellow)**: Warning banner displayed
  ```
  ⚠️ Memory usage is high.
  [Enable Capture Mode] or [Stream to File] [✖]
  ```

- **95% (Red)**: Critical banner + auto-trim
  ```
  🔴 Memory critical! Oldest logs were removed to prevent crash.
  [Enable File Streaming] [✖]
  ```

**Auto-Trim Behavior**:
- Removes oldest 25% of buffer when memory reaches 95%
- Only triggers if file streaming is NOT enabled
- Prevents browser crash/freeze

### 3. IndexedDB Persistent Storage

**Database Schema**:
```
Database: OTGW-Logs
├── logs (auto-increment ID)
│   ├── sessionId (index)
│   ├── time (index)
│   └── data (log entry)
├── sessions (session metadata)
│   ├── id (key)
│   ├── timestamp (index)
│   ├── lineCount
│   └── device
└── metadata (configuration)
    └── settings
```

**Features**:
- Auto-save every 10 seconds
- Configurable retention (default: 7 days)
- Can store millions of log lines (GB+)
- Survives page reload and tab close

### 4. Session Recovery

**Recovery Banner**:
```
ℹ️ Found previous session from 23:45 with 15,234 log lines.
[Recover] [Ignore]
```

**Recovery Behavior**:
- Checks for sessions < 1 hour old on page load
- Prompts user to recover
- Shows progress during recovery
- Restores all log lines and UI state

### 5. localStorage State Persistence

**Saved State**:
- Auto-scroll enabled/disabled
- Timestamps shown/hidden
- Search filter text
- Log expanded/collapsed
- Capture mode enabled/disabled
- Storage mode preference

**Backup**:
- Last 1000 log lines saved as backup
- 5-minute freshness check
- Quota-aware (graceful fallback to 500 lines)

### 6. Smart Storage Mode

**Auto-Selection Algorithm**:
```javascript
if (lines > 10,000) {
  // Use IndexedDB for large datasets
  mode = 'indexeddb';
} else if (lines > 1,000) {
  // Use localStorage for medium datasets
  mode = 'localstorage';
} else {
  // In-memory only for small datasets
  mode = 'none';
}
```

**Manual Modes** (future enhancement):
- Disabled: No persistence
- Smart: Auto-select (recommended)
- IndexedDB: Always use IndexedDB
- localStorage: Always use localStorage

## User Experience Flow

### Scenario 1: Normal Usage
1. User opens page → UI state restored from localStorage
2. Logs stream in via WebSocket
3. Auto-save to IndexedDB every 10 seconds
4. Memory monitored every 5 seconds
5. Page unload → State saved

### Scenario 2: Page Reload
1. User reloads page
2. UI state restored instantly (localStorage)
3. Recovery banner appears if recent session found
4. User clicks "Recover" → Logs loaded from IndexedDB
5. WebSocket reconnects → Live updates resume

### Scenario 3: Tab Closed and Reopened
1. User closes tab (state saved to IndexedDB + localStorage)
2. User reopens same device URL (within 1 hour)
3. Recovery banner appears
4. User recovers session → All data restored

### Scenario 4: High Memory Usage
1. Logs accumulate to 400MB (80% threshold)
2. Yellow warning banner appears
3. User enables "Capture Mode" or "Stream to File"
4. Memory stays bounded

### Scenario 5: Critical Memory
1. Memory reaches 475MB (95% threshold)
2. Red critical banner appears
3. System auto-trims 25% of oldest logs
4. Browser stays responsive

### Scenario 6: Browser Quota Check
1. Storage usage displayed in footer
2. Color-coded indicator (green/yellow/red)
3. User can monitor quota consumption
4. Cleanup triggered if approaching limit

## Technical Details

### Memory Estimation
```javascript
// Accurate buffer size
const bufferSize = JSON.stringify(otLogBuffer).length;

// With performance.memory API (Chrome/Edge)
const heapUsed = performance.memory.usedJSHeapSize;
const heapLimit = performance.memory.jsHeapSizeLimit;

// Fallback for other browsers
const estimated = otLogBuffer.length * 200; // bytes
```

### Storage Quota Check
```javascript
const estimate = await navigator.storage.estimate();
const usage = estimate.usage; // bytes used
const quota = estimate.quota; // bytes available
const percent = (usage / quota) * 100;
```

### Session Management
```javascript
// Create session
currentSessionId = 'session_' + Date.now() + '_' + randomId();

// Save session metadata
await logDatabase.saveSession({
  id: currentSessionId,
  timestamp: Date.now(),
  lineCount: otLogBuffer.length,
  device: window.location.hostname
});

// Save logs
await logDatabase.saveLogs(otLogBuffer, currentSessionId);

// Recover recent session
const session = await logDatabase.getRecentSession(3600000); // 1 hour
if (session) {
  const logs = await logDatabase.loadLogsForSession(session.id);
  // Restore logs to buffer
}
```

### Cleanup Strategy
```javascript
// Automatic cleanup of sessions > 7 days old
const retentionDays = 7;
const cutoff = Date.now() - (retentionDays * 24 * 60 * 60 * 1000);

// Delete old sessions and their logs
await logDatabase.cleanOldSessions(retentionDays);
```

## Browser Compatibility

| Feature | Chrome/Edge | Firefox | Safari |
|---------|------------|---------|--------|
| Memory monitoring | Full (API) | Estimated | Estimated |
| IndexedDB | ✅ | ✅ | ✅ |
| localStorage | ✅ | ✅ | ✅ |
| Storage quota | ✅ | ✅ | ✅ |
| Session recovery | ✅ | ✅ | ✅ |
| Auto-save | ✅ | ✅ | ✅ |

## Performance Impact

- **Memory monitoring**: Negligible (5-second intervals)
- **Auto-save**: Low (10-second batched writes)
- **IndexedDB writes**: Async, non-blocking
- **localStorage writes**: Sync, but small data only
- **Recovery**: One-time cost on page load

## Configuration Options

All configuration is in `index.js`:

```javascript
// Memory thresholds
const MEMORY_SAFE_LIMIT = 500 * 1024 * 1024; // 500MB
const MEMORY_WARNING_THRESHOLD = 0.8; // 80%
const MEMORY_CRITICAL_THRESHOLD = 0.95; // 95%

// Storage configuration
const STORAGE_CONFIG = {
  retentionDays: 7,
  maxIndexedDBSize: 1024 * 1024 * 1024, // 1GB
  maxLocalStorageLines: 1000,
  autoSaveEnabled: true
};

// Auto-save interval
const AUTO_SAVE_INTERVAL = 10000; // 10 seconds
```

## Future Enhancements

### Phase 5: User Controls
- Storage mode selector in UI
- Retention policy configuration
- Manual cleanup button
- Storage usage breakdown
- Export/import functionality

### Phase 6: Enhanced File Streaming
- Auto-offload old logs to file when memory high
- File-based recovery on page load
- "Load More from File" button

### Phase 7: Advanced Features
- Compression (LZ-string)
- Encryption (for sensitive data)
- Cloud sync (optional)
- Multi-device sync
- Search indexing

## Troubleshooting

### IndexedDB Not Working
- Check browser console for errors
- Verify not in incognito/private mode
- Check if site has storage permission
- Try clearing browser data and reload

### localStorage Quota Exceeded
- System automatically falls back to fewer lines
- Check browser storage settings
- Consider enabling IndexedDB mode

### Session Not Recovering
- Check if session is < 1 hour old
- Verify same device hostname
- Check browser console for errors
- Try manual recovery from Settings

### Memory Still Growing
- Enable "Stream to File" for unlimited storage
- Reduce buffer size via capture mode
- Check for memory leaks in browser DevTools

## Testing Recommendations

1. **Small dataset** (< 1000 lines): Test in-memory only
2. **Medium dataset** (1000-10000 lines): Test localStorage
3. **Large dataset** (> 10000 lines): Test IndexedDB
4. **Very large** (> 100k lines): Test file streaming
5. **Memory stress**: Let logs accumulate to trigger warnings
6. **Recovery**: Reload page, close tab, restart browser
7. **Quota**: Fill storage to test quota monitoring
8. **Cross-browser**: Test on Chrome, Firefox, Safari, Edge

## Summary

This implementation provides **5 comprehensive scenarios** for memory monitoring and data persistence:

1. ✅ **Enhanced Memory Monitoring** - Prevents crashes
2. ✅ **IndexedDB Persistent Storage** - Unlimited data, survives tab close
3. ✅ **localStorage State** - Fast UI preferences and backup
4. ✅ **Hybrid Smart Mode** - Auto-selects best storage
5. ⏳ **Enhanced File Streaming** - Existing feature, future enhancement

The **Hybrid Approach (Scenario 4)** is implemented and recommended for production use.
