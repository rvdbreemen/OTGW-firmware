# WebUI Data Persistence

## Overview

The OTGW firmware WebUI now includes **automatic data persistence** with **progressive saving** and **dynamic memory management**. This prevents loss of collected OpenTherm logs and eliminates fixed line limits by intelligently managing browser storage and memory.

## Key Features

### 🚀 Progressive Storage
- **Saves as data arrives** - Debounced saves trigger 2 seconds after new data
- **No more waiting** - Data is protected immediately, not just every 30 seconds
- **Rate limited** - Prevents excessive writes while ensuring data safety

### 📊 Dynamic Memory Management
- **No fixed limits** - Automatically calculates optimal buffer size
- **Memory monitoring** - Tracks actual heap usage (Chrome/Edge) or estimates
- **Storage aware** - Detects localStorage quota and adjusts accordingly
- **Smart trimming** - Removes oldest entries only when necessary

### ⚡ Intelligent Modes
- **Normal mode**: Balances memory and storage (~5k-200k lines typical)
- **Capture mode**: Maximizes buffer size based on available memory
- **Auto-adjustment**: Recalculates limits every 1000 new entries

## What Gets Saved?

### 1. OpenTherm Log Buffer
- **Unlimited entries** (limited only by browser memory/storage)
- Automatically managed based on available resources
- Typical capacity: 50k-200k entries in normal mode
- Capture mode: Can exceed 1M entries with sufficient memory
- Progressive saving ensures minimal data loss

### 2. User Preferences
- Auto-scroll setting
- Timestamp display setting
- Log expanded/collapsed state
- **Capture mode setting**
- Search term
- Saved automatically with buffer data

## How It Works

### Progressive Saving (New!)
- **Debounced saves**: Triggers 2 seconds after last data update
- **Rate limiting**: Won't save more than once per second
- **Fallback timer**: Still saves every 30 seconds as backup
- **On page unload**: Final save when you close or refresh
- **Minimal latency**: Data protected almost immediately

### Dynamic Memory Management (New!)
The system continuously monitors and adapts:

1. **Detects storage quota** using Navigator.storage API
2. **Monitors heap usage** via performance.memory (Chrome/Edge)
3. **Estimates buffer size** by sampling entries
4. **Calculates optimal limit** every 1000 new entries
5. **Trims intelligently** only when limits are exceeded

#### Normal Mode
- Balances memory and storage efficiency
- Target: 100 MB maximum memory usage
- Uses 80% of available localStorage
- Typical range: 5,000 to 200,000 entries
- Ideal for continuous monitoring

#### Capture Mode
- Maximizes data collection
- Uses all available memory (up to limit)
- Can handle 1M+ entries with sufficient resources
- Ideal for diagnostic sessions or troubleshooting

### Automatic Restoration
- 🔄 **On page load** in `initMainPage()`
- ✅ **Restores log buffer** (all OpenTherm log entries)
- ⚙️ **Restores preferences** (auto-scroll, timestamps, capture mode, etc.)
- 🎨 **Updates UI** to match restored state
- 📊 **Recalculates limits** based on current system resources

## Storage and Memory Limits

### No More Fixed Limits!
The old system had fixed limits (2,000 normal / 1M capture). The new system is **fully dynamic**:

### Dynamic Calculation
```javascript
Normal Mode:
  - Memory limit: ~100 MB for log buffer
  - Storage limit: 80% of localStorage quota
  - Final limit: minimum of both
  - Typical: 50k-200k entries (~500 bytes each)

Capture Mode:
  - Memory limit: Uses all available heap
  - Storage limit: 80% of localStorage quota  
  - Final limit: memory-based (usually higher)
  - Typical: 200k-2M entries (depends on system)
```

### Automatic Adjustments
- **Every 1000 entries**: Recalculates optimal limit
- **Storage quota changes**: Adapts to browser limits
- **Memory pressure**: Reduces buffer if needed
- **No user intervention**: All automatic

### Browser Storage Quotas
Different browsers have different limits:
- **Chrome/Edge**: ~10 GB total, ~10 MB localStorage
- **Firefox**: ~10 GB total, ~10 MB localStorage
- **Safari**: ~1 GB total, ~5 MB localStorage

The system detects and adapts to your browser's limits.

### Memory Monitoring

The UI shows real-time memory usage:
- **Chrome/Edge**: Actual JS heap usage from performance.memory
- **Other browsers**: Estimated buffer size
- **Format**: "X.X / YYY MB" or "~X.X MB"
- **Tooltip**: Detailed heap and buffer information

## Manual Control

You can manually control data persistence and view detailed statistics via the browser console:

```javascript
// Show detailed statistics (enhanced output)
otgwPersistence.info()
// Output shows:
// - Buffer entries and current limit
// - Capture mode status
// - Memory usage (heap if available, estimate otherwise)
// - LocalStorage quota and usage
// - Saved data sizes
// - User preferences

// Save current data immediately
otgwPersistence.save()

// Restore saved data
otgwPersistence.restore()

// Clear all saved data
otgwPersistence.clear()
```

### Example Output
```
══════════════════════════════════════
Log Buffer Info
══════════════════════════════════════
Entries in buffer: 47,823
Current limit: 125,000 (auto)
Capture mode: OFF (balanced)

Memory Usage
══════════════════════════════════════
JS Heap used: 87.4 MB
JS Heap total: 112.1 MB  
JS Heap limit: 2048.0 MB
Available: 1960.6 MB
Buffer estimate: 23.91 MB

LocalStorage
══════════════════════════════════════
Storage quota: 10.0 MB
Safety margin: 80%
Stored logs: 2847.32 KB
Stored prefs: 0.15 KB

Preferences: {autoScroll: true, ...}
══════════════════════════════════════
```

## Debug Console Integration

The persistence system is integrated with the debug helper:

```javascript
// Show persistence info
otgwDebug.persistence()

// Access via help menu
otgwDebug.help()
```

## Clear Log Behavior

When you click "Clear" on the log buffer:
- Clears the in-memory log buffer
- **Also clears** saved data from localStorage
- Prevents old data from reappearing on next page load

You'll be prompted: _"Clear all log messages from buffer? This will also clear saved data from browser storage."_

## Browser Compatibility

The persistence feature works in all modern browsers that support:
- localStorage API (Chrome 4+, Firefox 3.5+, Safari 4+, Edge all versions)
- JSON serialization
- beforeunload event
- Optional: Navigator.storage API (for accurate quota detection)
- Optional: performance.memory API (Chrome/Edge for heap monitoring)

**Fully compatible** with the WebUI's browser support requirements.

## Privacy & Security

- **Local only**: All data stays in your browser's localStorage
- **No transmission**: Nothing is sent to external servers
- **Per-domain**: Data is isolated to the OTGW device's domain/IP
- **User control**: You can clear data anytime via browser settings or the clear button

### Clearing localStorage manually:
- **Chrome**: DevTools → Application → Storage → Local Storage
- **Firefox**: DevTools → Storage → Local Storage
- **Safari**: DevTools → Storage → Local Storage

## Error Handling

The system gracefully handles:
- **Quota exceeded**: Automatically reduces data size and retries
- **Corrupted data**: Clears bad data and starts fresh
- **No localStorage**: Falls back to normal operation (no persistence)
- **Parse errors**: Logs error and clears corrupted storage
- **Memory pressure**: Automatically trims buffer when limits reached

## Benefits

1. **No data loss** on page refresh
2. **Progressive saving** - data protected as it arrives
3. **No artificial limits** - uses available resources optimally
4. **Intelligent** - manages quota and memory automatically
5. **Fast** - debounced saves prevent excessive writes
6. **Responsive** - adapts to browser capabilities
7. **Transparent** - shows real-time memory and storage usage
8. **Efficient** - minimal performance impact

## Limitations

- **Browser-specific**: Data saved in Chrome won't appear in Firefox
- **Device-specific**: Data is tied to the device's IP/hostname
- **Not a backup**: Use Download Log for permanent backups
- **Memory dependent**: Very large datasets depend on available RAM
- **Private browsing**: May not work in incognito/private mode
- **Storage quota**: Each browser has different localStorage limits

## Performance Characteristics

### Write Performance
- **Debounced**: Saves triggered 2s after last update
- **Rate limited**: Maximum 1 save per second
- **Non-blocking**: Uses setTimeout for async behavior
- **Efficient**: Only saves when data actually changes

### Memory Performance
- **Lazy calculation**: Limits recalculated every 1000 entries
- **Sampling**: Estimates size from 50-100 random entries
- **Smart trimming**: Batch removes oldest entries when needed
- **Display throttling**: UI updates limited to 2000 visible lines

### Storage Performance
- **Quota detection**: Cached after first check
- **Compressed storage**: JSON serialization is efficient
- **Safety margin**: Uses only 80% of quota to prevent errors
- **Graceful degradation**: Automatically reduces size if quota exceeded

## Recommendations

### For Different Use Cases

**Continuous Monitoring (Normal Mode)**
- Leave capture mode OFF
- System manages limits automatically
- Typically handles hours of data
- Progressive saves protect recent data

**Diagnostic Sessions (Capture Mode)**  
- Enable capture mode checkbox
- Collects maximum data possible
- Monitor memory usage display
- Download log when finished

**Long-term Collection**
- Use **Download Log** to save files periodically
- Use **Stream to File** for continuous logging to disk
- Use **Auto Download** (15 min intervals) for automatic backups
- Progressive persistence handles session interruptions

**Session Continuity (Built-in)**
- Progressive persistence automatically protects data
- Handles page reloads, crashes, accidental refreshes
- No configuration needed - works out of the box
- Keeps workspace intact across sessions

## Troubleshooting

**Data not persisting?**
1. Check if browser supports localStorage (`otgwPersistence.info()`)
2. Verify you're not in private/incognito mode
3. Check browser console for errors
4. Try manually saving: `otgwPersistence.save()`

**Storage full errors?**
1. System auto-adjusts limits dynamically
2. Check available storage: `otgwPersistence.info()`
3. Manually clear old data: `otgwPersistence.clear()`
4. Download and clear logs regularly
5. Check browser storage settings

**Running out of memory?**
1. Check memory usage in UI footer
2. View details: `otgwPersistence.info()`
3. Disable capture mode if enabled
4. System will auto-trim when limit reached
5. Download logs and refresh page

**Want more capacity?**
1. Enable capture mode for maximum collection
2. Close other tabs to free memory
3. Use Chrome/Edge for better memory monitoring
4. Check `otgwPersistence.info()` for current limits

**Corrupted data on restore?**
1. System automatically clears corrupted data
2. Logs error to console
3. Starts fresh with clean state

## Technical Details

### Storage Keys
- `otgw_log_buffer`: Stores the log buffer array (dynamic size)
- `otgw_log_prefs`: Stores user preferences object (~150 bytes)

### Dynamic Limit Algorithm
```javascript
function calculateOptimalMaxLines() {
  if (captureMode) {
    // Use available memory, target 100 MB
    availableMB = min(heapAvailable, 100)
    return floor(availableMB * 1024 * 1024 / 500) // ~500 bytes/entry
  } else {
    // Balance memory and storage
    memoryLines = floor(heapAvailable * 1024 * 1024 / 500)
    storageLines = floor(storageQuota * 0.8 * 1024 * 1024 / 500)
    calculated = min(memoryLines, storageLines)
    return clamp(calculated, 5000, 200000) // Reasonable range
  }
}
```

### Progressive Save Logic
```javascript
// Triggered on every new log entry
function debouncedSave() {
  clearTimeout(timer)
  timer = setTimeout(() => {
    if (timeSinceLastSave >= 1000ms) {
      saveDataToLocalStorage()
    }
  }, 2000ms) // 2 second debounce
}
```

### Memory Monitoring
- **Chrome/Edge**: Uses `performance.memory` API for actual heap data
- **Other browsers**: Estimates from JSON.stringify() sampling
- **Update frequency**: Every 1000 entries or on demand
- **Display**: Real-time in UI footer

### Data Format
```javascript
// Log buffer (array of log entries)
[
  {
    time: "12:34:56.789000",
    data: { /* parsed log entry */ }
  },
  // ... more entries
]

// Preferences (object)
{
  autoScroll: true,
  showTimestamps: true,
  logExpanded: false,
  captureMode: false,  // New: tracks capture mode state
  searchTerm: "",
  savedAt: "2026-01-27T12:34:56.789Z"
}
```

### Browser API Usage
- **localStorage**: Data persistence (all browsers)
- **navigator.storage.estimate()**: Quota detection (modern browsers)
- **performance.memory**: Heap monitoring (Chrome/Edge only)
- **requestAnimationFrame**: Display throttling (all browsers)
- **setTimeout**: Debouncing and rate limiting (all browsers)

## Future Enhancements

Potential future additions:
- **Compression**: LZ-string or similar for better storage efficiency
- **IndexedDB**: For 10s-100s of MBs of storage  
- **Cross-tab sync**: Share buffer across multiple tabs
- **Export/import**: Save/restore complete sessions
- **Cloud sync**: Optional backup to cloud storage
- **Adaptive sampling**: Reduce data rate under memory pressure
