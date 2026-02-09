/*
***************************************************************************  
**  Program  : index.js, part of OTGW-firmware project
**  Version  : v1.0.0
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
const localURL = window.location.protocol + '//' + window.location.host;
const APIGW = window.location.protocol + '//' + window.location.host + '/api/';

"use strict";


console.log(`Hash=${window.location.hash}`);
window.onload = initMainPage;

function isPageVisible() {
  return !(document.hidden || document.visibilityState === 'hidden');
}

document.addEventListener('visibilitychange', function () {
  if (!isPageVisible()) {
    // When tab is hidden, stop UI updates to save resources but KEEP WebSocket connected
    if (timeupdate) {
      clearInterval(timeupdate);
      timeupdate = null;
    }
    if (tid) {
      clearInterval(tid);
      tid = 0;
    }
    // WebSocket stays connected to continue gathering data in background
    // The watchdog timer will keep it alive and reconnect if needed
    return;
  }
  // When tab becomes visible again, resume UI updates
  if (!flashModeActive) {
    refreshDevTime();
    refreshGatewayMode(true);
    if (!timeupdate) {
      timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
    }
    // Ensure WebSocket is connected (will reconnect if needed)
    initOTLogWebSocket();
    if (!tid) {
      tid = setInterval(function () { refreshOTmonitor(); }, 1000);
    }
  }
});


var tid = 0;
var timeupdate = null; // Will be started when needed

let gatewayModeRefreshCounter = 0;
let gatewayModeRefreshInFlight = false;
const GATEWAY_MODE_REFRESH_INTERVAL = 60; // 60s max polling interval (at most once a minute)

function updateGatewayModeIndicator(value) {
  const statusEl = document.getElementById('gatewayModeStatus');
  const textEl = document.getElementById('gatewayModeText');
  if (!statusEl || !textEl) return;

  if (value === true) {
    statusEl.className = 'mode-status mode-gateway';
    textEl.textContent = 'Mode: Gateway';
  } else if (value === false) {
    statusEl.className = 'mode-status mode-monitor';
    textEl.textContent = 'Mode: Monitor';
  } else {
    statusEl.className = 'mode-status mode-unknown';
    textEl.textContent = 'Mode: Unknown';
  }
}

function parseGatewayModeValue(gatewayModeValue) {
  if (typeof gatewayModeValue !== 'string') return null;

  const normalized = gatewayModeValue.trim().toLowerCase();
  if (normalized === 'on' || normalized === '1' || normalized === 'true' || normalized === 'gateway') {
    return true;
  }
  if (normalized === 'off' || normalized === '0' || normalized === 'false' || normalized === 'monitor' || normalized === 'standalone') {
    return false;
  }
  return null;
}

function updateGatewayModeFromDevInfoEntries(entries) {
  let gatewayModeValue = null;

  for (let i = 0; i < entries.length; i++) {
    if (entries[i].name === 'gatewaymode') {
      gatewayModeValue = entries[i].value;
      break;
    }
  }

  updateGatewayModeIndicator(parseGatewayModeValue(gatewayModeValue));
}

function refreshGatewayMode(force) {
  if (flashModeActive || !isPageVisible()) return;
  
  // Throttle has priority: prevent double-triggering even with force=true
  if (gatewayModeRefreshInFlight) return;
  
  if (!force && gatewayModeRefreshCounter < GATEWAY_MODE_REFRESH_INTERVAL) {
    gatewayModeRefreshCounter++;
    return;
  }

  gatewayModeRefreshCounter = 0;
  gatewayModeRefreshInFlight = true;

  fetch(APIGW + 'v0/devinfo')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      const entries = (json && json.devinfo) ? json.devinfo : [];
      updateGatewayModeFromDevInfoEntries(entries);
    })
    .catch(error => {
      console.warn('refreshGatewayMode warning:', error);
      updateGatewayModeIndicator(null);
    })
    .finally(() => {
      gatewayModeRefreshInFlight = false;
    });
}

//============================================================================
// Flash Mode Management - stops all background activity during flashing
//============================================================================
function enterFlashMode() {
  console.log('Entering flash mode - stopping all background activity');
  flashModeActive = true;
  
  // Stop all timers
  if (timeupdate) {
    clearInterval(timeupdate);
    timeupdate = null;
  }
  if (tid) {
    clearInterval(tid);
    tid = 0;
  }
  
  // Disconnect WebSocket
  disconnectOTLogWebSocket();
  
  console.log('Flash mode active - all polling and WebSocket activity stopped');
}

function exitFlashMode() {
  console.log('Exiting flash mode - restarting background activity');
  flashModeActive = false;
  
  // Restart time update
  if (!timeupdate) {
    timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
  }
  
  // Restart WebSocket if on main page
  if (document.getElementById('displayMainPage') && 
      document.getElementById('displayMainPage').classList.contains('active')) {
    initOTLogWebSocket();
    if (!tid) {
      tid = setInterval(function () { refreshOTmonitor(); }, 1000);
    }
  }
  
  console.log('Flash mode exited - background activity resumed');
}

// Make functions globally accessible for cross-window communication
window.enterFlashMode = enterFlashMode;
window.exitFlashMode = exitFlashMode;

//============================================================================
// Browser Console Debug Helper
//============================================================================
window.otgwDebug = {
  // Display help menu
  help: function() {
    console.log('%c╔═══════════════════════════════════════════════════════════╗', 'color: #00ff00; font-weight: bold;');
    console.log('%c║         OTGW Firmware - Browser Debug Helper             ║', 'color: #00ff00; font-weight: bold;');
    console.log('%c╚═══════════════════════════════════════════════════════════╝', 'color: #00ff00; font-weight: bold;');
    console.log('');
    console.log('%c📊 Status Information:', 'color: #00aaff; font-weight: bold;');
    console.log('  otgwDebug.status()        - Show current system status');
    console.log('  otgwDebug.info()          - Show device information');
    console.log('  otgwDebug.settings()      - Show current settings');
    console.log('');
    console.log('%c🔌 WebSocket & Connections:', 'color: #00aaff; font-weight: bold;');
    console.log('  otgwDebug.wsStatus()      - Show WebSocket connection status');
    console.log('  otgwDebug.wsReconnect()   - Reconnect WebSocket');
    console.log('  otgwDebug.wsDisconnect()  - Disconnect WebSocket');
    console.log('');
    console.log('%c🔍 Data Inspection:', 'color: #00aaff; font-weight: bold;');
    console.log('  otgwDebug.otmonitor()     - Show current OT monitor data');
    console.log('  otgwDebug.logs()          - Show buffered log lines');
    console.log('  otgwDebug.clearLogs()     - Clear log buffer');
    console.log('  otgwDebug.persistence()   - Show localStorage persistence info');
    console.log('');
    console.log('%c⚙️  API Testing:', 'color: #00aaff; font-weight: bold;');
    console.log('  otgwDebug.api(endpoint)   - Test API endpoint (e.g., "v1/devinfo")');
    console.log('  otgwDebug.health()        - Check system health API');
    console.log('  otgwDebug.sendCmd(cmd)    - Send OTGW command (e.g., "PS=1")');
    console.log('');
    console.log('%c🐛 Debug Toggles:', 'color: #00aaff; font-weight: bold;');
    console.log('  otgwDebug.verbose = true  - Enable verbose console logging');
    console.log('  otgwDebug.verbose = false - Disable verbose logging');
    console.log('');
    console.log('%c💾 Data Export:', 'color: #00aaff; font-weight: bold;');
    console.log('  otgwDebug.exportLogs()    - Download current logs as file');
    console.log('  otgwDebug.exportData()    - Download current OT data as JSON');
    console.log('');
    console.log('%cType otgwDebug.help() to see this menu again', 'color: #ffaa00;');
  },

  // Verbose mode toggle
  verbose: false,

  // Show current status
  status: function() {
    console.group('📊 OTGW System Status');
    console.log('Flash Mode Active:', flashModeActive);
    console.log('Page Visible:', isPageVisible());
    console.log('Auto Refresh Timer:', tid ? 'Running' : 'Stopped');
    console.log('Time Update Timer:', timeupdate ? 'Running' : 'Stopped');
    console.log('WebSocket Status:', otLogWS ? otLogWS.readyState : 'Not initialized');
    console.groupEnd();
  },

  // Show device information
  info: async function() {
    try {
      const response = await fetch(APIGW + 'v1/devinfo');
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      console.group('📱 Device Information');
      console.table(data.devinfo || data);
      console.groupEnd();
    } catch (error) {
      console.error('Failed to fetch device info:', error);
    }
  },

  // Show current settings
  settings: async function() {
    try {
      const response = await fetch(APIGW + 'v0/settings');
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      console.group('⚙️  Current Settings');
      console.table(data.settings || data);
      console.groupEnd();
    } catch (error) {
      console.error('Failed to fetch settings:', error);
    }
  },

  // Show WebSocket status
  wsStatus: function() {
    console.group('🔌 WebSocket Status');
    if (otLogWS) {
      const states = ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'];
      console.log('State:', states[otLogWS.readyState] || 'UNKNOWN');
      console.log('URL:', otLogWS.url);
      console.log('Buffered Lines:', otLogBuffer.length);
      console.log('Auto Scroll:', autoScroll);
      console.log('Max Log Lines:', maxLogLines);
    } else {
      console.log('WebSocket: Not initialized');
    }
    
    // Show connection statistics
    console.log('');
    console.log('📊 Connection Statistics:');
    console.log('  Total Attempts:', wsConnectionAttempts);
    console.log('  Successful Connections:', wsSuccessfulConnections);
    console.log('  Reconnect Attempts:', wsReconnectAttempts);
    
    if (wsLastConnectTime) {
      console.log('  Last Connect:', wsLastConnectTime.toISOString());
      if (otLogWS && otLogWS.readyState === WebSocket.OPEN) {
        const currentDuration = (new Date() - wsLastConnectTime) / 1000;
        console.log('  Current Duration:', currentDuration.toFixed(2) + ' seconds');
      }
    }
    
    if (wsLastDisconnectTime) {
      console.log('  Last Disconnect:', wsLastDisconnectTime.toISOString());
      if (wsConnectionDuration > 0) {
        console.log('  Last Connection Duration:', wsConnectionDuration.toFixed(2) + ' seconds');
      }
    }
    
    // Show timer status
    console.log('');
    console.log('⏲️  Timer Status:');
    console.log('  Reconnect Timer:', wsReconnectTimer ? 'Scheduled' : 'Not scheduled');
    console.log('  Watchdog Timer:', wsWatchdogTimer ? 'Active' : 'Inactive');
    
    console.groupEnd();
  },

  // Reconnect WebSocket
  wsReconnect: function() {
    console.log('🔄 Reconnecting WebSocket...');
    disconnectOTLogWebSocket();
    setTimeout(() => initOTLogWebSocket(), 1000);
  },

  // Disconnect WebSocket
  wsDisconnect: function() {
    console.log('🔌 Disconnecting WebSocket...');
    disconnectOTLogWebSocket();
  },

  // Show current OT monitor data
  otmonitor: function() {
    console.group('📊 OT Monitor Data');
    if (typeof data !== 'undefined' && data) {
      console.table(data);
    } else {
      console.log('No OT monitor data available');
    }
    console.groupEnd();
  },
  
  // Show persistence info
  persistence: function() {
    if (window.otgwPersistence) {
      console.group('💾 LocalStorage Persistence Info');
      window.otgwPersistence.info();
      console.groupEnd();
    } else {
      console.log('Persistence not initialized');
    }
  },

  // Show buffered logs
  logs: function(lines = 50) {
    console.group(`📜 Last ${lines} Log Lines`);
    const recentLogs = otLogBuffer.slice(-lines);
    recentLogs.forEach((line, idx) => {
      console.log(`${otLogBuffer.length - lines + idx + 1}: ${line}`);
    });
    console.groupEnd();
  },

  // Clear log buffer
  clearLogs: function() {
    otLogBuffer = [];
    otLogFilteredBuffer = [];
    console.log('✅ Log buffers cleared');
  },

  // Test API endpoint
  api: async function(endpoint) {
    const url = APIGW + endpoint;
    console.log(`🌐 Fetching: ${url}`);
    try {
      const response = await fetch(url);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const contentType = response.headers.get('content-type');
      let data;
      if (contentType && contentType.includes('application/json')) {
        data = await response.json();
        console.group(`✅ Response from ${endpoint}`);
        console.log('Status:', response.status);
        console.log('Data:', data);
        console.groupEnd();
      } else {
        data = await response.text();
        console.group(`✅ Response from ${endpoint}`);
        console.log('Status:', response.status);
        console.log('Data:', data);
        console.groupEnd();
      }
      return data;
    } catch (error) {
      console.error('❌ API Error:', error);
    }
  },

  // Check system health
  health: async function() {
    return await this.api('v1/health');
  },

  // Send OTGW command
  sendCmd: async function(cmd) {
    console.log(`📤 Sending command: ${cmd}`);
    try {
      const response = await fetch(APIGW + `v1/otgw/command/${cmd}`, {
        method: 'POST'
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      console.log('✅ Command response:', data);
      return data;
    } catch (error) {
      console.error('❌ Command failed:', error);
    }
  },

  // Export logs to file - Compatible with otmonitor
  exportLogs: function() {
    if (!otLogBuffer || otLogBuffer.length === 0) {
      console.warn('⚠️  No logs to export');
      return;
    }

    // Helper to pad strings with spaces
    // Ensures alignment for otmonitor compatibility
    const pad = (str, len) => {
      str = String(str || '');
      // If string is longer than len, don't truncate, just append space if needed? 
      // standard padEnd behavior is fine
      return str.padEnd(len, ' ');
    };

    const lines = otLogBuffer.map(entry => {
      const d = entry.data || {};
      
      // 1. Timestamp (15 chars: HH:MM:SS.mmmmmm)
      // entry.time is typically "HH:MM:SS.mmmmmm" from parsing or generation
      let line = (entry.time || "00:00:00.000000").substring(0, 15);
      
      // Align to col 17 (otmonitor fileanal/untab format: 15 chars + 2 spaces)
      line = pad(line, 17);
      
      // 2. Raw Message (at col 17)
      // d.raw is the hex string e.g. "B4001500"
      const raw = (d.raw || "").trim();
      line += raw;
      
      // Align to col 28
      line = pad(line, 28);
      
      // 3. Type / Direction (at col 28)
      // d.dir corresponds to the message type string (e.g., Read-Ack)
      // or we can use d.source if d.dir is empty
      const type = (d.dir || d.source || "").trim();
      line += type;
      
      // Align to col 40
      line = pad(line, 40);
      
      // 4. Description and Value (Human readability)
      // This part is likely ignored by otmonitor import (which re-decodes raw)
      // but good for reading by humans.
      let desc = (d.label || "").trim();
      if (d.value !== undefined && d.value !== null && String(d.value).trim() !== "") {
        if (desc) desc += ": ";
        desc += d.value;
        if (d.unit) desc += " " + d.unit;
      }
      line += desc;
      
      return line;
    });

    const content = lines.join('\n');
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `otgw-logs-${new Date().toISOString().replace(/[:.]/g, '-')}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    console.log(`✅ Exported ${otLogBuffer.length} log lines (Compatible Format)`);
  },

  // Export current data to JSON
  exportData: function() {
    if (typeof data === 'undefined' || !data) {
      console.warn('⚠️  No data to export');
      return;
    }
    const content = JSON.stringify(data, null, 2);
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `otgw-data-${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
    a.click();
    URL.revokeObjectURL(url);
    console.log('✅ Data exported as JSON');
  }
};

// Show welcome message on load
console.log('%c🔧 OTGW Debug Helper Loaded', 'color: #00ff00; font-weight: bold; font-size: 14px;');
console.log('%cType otgwDebug.help() for available commands', 'color: #ffaa00;');

//============================================================================
// OpenTherm Log WebSocket Variables and Functions
//============================================================================
let otLogWS = null;
let otLogBuffer = [];
let otLogFilteredBuffer = [];

// Dynamic memory management - no fixed limits, monitor actual usage
const RENDER_LIMIT = 2000; // Still limit display rendering for performance
let maxLogLines = null; // Dynamic - calculated based on available memory
let captureMode = false; // User-requested unlimited mode
const TARGET_MEMORY_MB = 100; // Target max memory for log buffer (reasonable for modern browsers)
const STORAGE_SAFETY_MARGIN = 0.8; // Use 80% of available localStorage to leave room for other data
let currentMemoryUsageMB = 0;
let storageQuotaMB = 10; // Default, will be detected

let autoScroll = true;
let showTimestamps = true;
let searchTerm = '';
let updatePending = false;
let otLogControlsInitialized = false;
let isFlashing = false;
let currentFlashFilename = "";
let flashModeActive = false; // Track if we're on the flash page
let flashPollTimer = null; // Timer for polling flash status as failsafe (both ESP and PIC)
let otLogResponsiveInitialized = false;
let otLogResizeTimer = null;

// File Streaming Variables
// NOTE: File System Access API (showDirectoryPicker, getFileHandle, etc.) is only supported
// in Chrome, Edge, and Opera. Firefox and Safari do not support this API as of 2026.
// The code gracefully degrades to regular download functionality when the API is unavailable.
// See: https://developer.mozilla.org/en-US/docs/Web/API/File_System_Access_API
let logDirectoryHandle = null;
let fileStreamHandle = null;
let fileWritableStream = null; // Deprecated, kept for cleanup just in case
let logWriteQueue = [];
let isLogWriting = false;
let isStreamingToFile = false;
let streamBytesWritten = 0;
let fileRotationTimer = null;
let currentLogDateStr = "";

// Expose helper for other modules (graph.js) to save files to the same directory
window.saveBlobToLogDir = async function(filename, blob) {
    if (!logDirectoryHandle) {
        console.warn("saveBlobToLogDir: No log directory handle available.");
        return false;
    }
    try {
        const fileHandle = await logDirectoryHandle.getFileHandle(filename, { create: true });
        const writable = await fileHandle.createWritable();
        await writable.write(blob);
        await writable.close();
        console.log(`Saved ${filename} to log directory.`);
        return true;
    } catch (e) {
        console.error("Failed to save blob to log directory:", e);
        return false;
    }
};


// WebSocket configuration: must match the WebSocket port used in webSocketStuff.ino (currently hardcoded as 81 in the WebSocketsServer constructor).
const WEBSOCKET_PORT = 81;
let wsReconnectTimer = null;
let wsWatchdogTimer = null;
const WS_WATCHDOG_TIMEOUT = 45000; // 45 seconds timeout (allows for 30s keepalive + 15s margin)

// WebSocket connection tracking for detailed logging
let wsConnectionAttempts = 0; // Count of connection attempts since page load
let wsSuccessfulConnections = 0; // Count of successful connections
let wsReconnectAttempts = 0; // Count of reconnection attempts
let wsLastConnectTime = null; // Timestamp of last successful connection
let wsLastDisconnectTime = null; // Timestamp of last disconnect
let wsConnectionDuration = 0; // Duration of last connection in seconds

// LocalStorage persistence for data continuity across page reloads
let persistenceTimer = null;
let debouncedSaveTimer = null;
const PERSISTENCE_INTERVAL_MS = 30000; // Fallback save every 30 seconds
const DEBOUNCE_SAVE_MS = 2000; // Save 2 seconds after last data update
const PERSISTENCE_KEY_LOGS = 'otgw_log_buffer';
const PERSISTENCE_KEY_PREFS = 'otgw_log_prefs';
let lastSaveTimestamp = 0;

//============================================================================
// Memory and Storage Monitoring
//============================================================================

function detectStorageQuota() {
  if (!navigator.storage || !navigator.storage.estimate) {
    console.warn('Storage quota API not available, using default 10MB');
    return Promise.resolve(10 * 1024 * 1024);
  }
  
  return navigator.storage.estimate().then(estimate => {
    const quotaBytes = estimate.quota || (10 * 1024 * 1024);
    const usageBytes = estimate.usage || 0;
    const availableBytes = quotaBytes - usageBytes;
    
    console.log(`Storage quota: ${(quotaBytes / 1024 / 1024).toFixed(2)} MB`);
    console.log(`Storage used: ${(usageBytes / 1024 / 1024).toFixed(2)} MB`);
    console.log(`Storage available: ${(availableBytes / 1024 / 1024).toFixed(2)} MB`);
    
    storageQuotaMB = quotaBytes / 1024 / 1024;
    return availableBytes;
  }).catch(err => {
    console.warn('Error detecting storage quota:', err);
    return 10 * 1024 * 1024;
  });
}

function estimateMemoryUsage() {
  // Estimate memory usage of log buffer
  if (otLogBuffer.length === 0) {
    currentMemoryUsageMB = 0;
    return 0;
  }
  
  // Sample first entries for consistent, faster estimation (no random() overhead)
  const sampleSize = Math.min(50, otLogBuffer.length);
  let totalSize = 0;
  
  for (let i = 0; i < sampleSize; i++) {
    const entry = otLogBuffer[i];
    // Rough JSON size estimate
    totalSize += JSON.stringify(entry).length;
  }
  
  const avgSize = totalSize / sampleSize;
  const estimatedBytes = avgSize * otLogBuffer.length;
  currentMemoryUsageMB = estimatedBytes / 1024 / 1024;
  
  return currentMemoryUsageMB;
}

function getActualMemoryUsage() {
  // Use performance.memory API if available (Chrome/Edge)
  if (performance.memory) {
    const usedMB = performance.memory.usedJSHeapSize / 1024 / 1024;
    const totalMB = performance.memory.totalJSHeapSize / 1024 / 1024;
    const limitMB = performance.memory.jsHeapSizeLimit / 1024 / 1024;
    
    return {
      used: usedMB,
      total: totalMB,
      limit: limitMB,
      available: limitMB - usedMB
    };
  }
  
  // Fallback: estimate from buffer
  return {
    used: currentMemoryUsageMB,
    total: TARGET_MEMORY_MB,
    limit: TARGET_MEMORY_MB,
    available: TARGET_MEMORY_MB - currentMemoryUsageMB
  };
}

function calculateOptimalMaxLines() {
  if (captureMode) {
    // In capture mode, use memory limit
    const memInfo = getActualMemoryUsage();
    const availableMB = Math.min(memInfo.available, TARGET_MEMORY_MB);
    
    // Assume ~500 bytes per log entry average (conservative)
    const estimatedLines = Math.floor((availableMB * 1024 * 1024) / 500);
    
    console.log(`Capture mode: calculated max ${estimatedLines.toLocaleString()} lines (${availableMB.toFixed(1)} MB available)`);
    return Math.max(10000, estimatedLines); // At least 10k lines
  }
  
  // Normal mode: balance between memory and storage
  const memInfo = getActualMemoryUsage();
  const availableMemoryLines = Math.floor((memInfo.available * 1024 * 1024) / 500);
  
  // Also consider localStorage limit
  const availableStorageBytes = storageQuotaMB * 1024 * 1024 * STORAGE_SAFETY_MARGIN;
  const availableStorageLines = Math.floor(availableStorageBytes / 500);
  
  // Use the smaller of the two
  const calculated = Math.min(availableMemoryLines, availableStorageLines);
  const reasonable = Math.max(5000, Math.min(calculated, 200000)); // 5k to 200k range
  
  console.log(`Normal mode: max ${reasonable.toLocaleString()} lines (mem: ${availableMemoryLines.toLocaleString()}, storage: ${availableStorageLines.toLocaleString()})`);
  return reasonable;
}

function updateDynamicLimits() {
  maxLogLines = calculateOptimalMaxLines();
  
  // Update UI display
  const limitDisplay = document.getElementById('logLimitDisplay');
  if (limitDisplay) {
    if (captureMode) {
      limitDisplay.textContent = ` / ${(maxLogLines / 1000).toFixed(0)}k (capture)`;
    } else {
      limitDisplay.textContent = ` / ${(maxLogLines / 1000).toFixed(0)}k (auto)`;
    }
  }
  
  // Update memory display
  updateMemoryDisplay();
}

function updateMemoryDisplay() {
  const memUsage = document.getElementById('memUsage');
  if (!memUsage) return;
  
  estimateMemoryUsage();
  const memInfo = getActualMemoryUsage();
  
  // Always show buffer size (more relevant than total heap)
  const bufferMB = currentMemoryUsageMB.toFixed(1);
  const linesK = (otLogBuffer.length / 1000).toFixed(1);
  
  if (performance.memory) {
    // Show buffer size with actual heap info in tooltip
    memUsage.textContent = `${bufferMB} MB (${linesK}k lines)`;
    memUsage.title = `Log Buffer: ~${bufferMB} MB (${otLogBuffer.length.toLocaleString()} lines)\nTotal Heap: ${memInfo.used.toFixed(1)} MB used / ${memInfo.limit.toFixed(0)} MB limit`;
  } else {
    memUsage.textContent = `~${bufferMB} MB (${linesK}k lines)`;
    memUsage.title = `Estimated log buffer size: ${bufferMB} MB (${otLogBuffer.length.toLocaleString()} lines)`;
  }
}

// Initialize storage quota detection
detectStorageQuota().then(() => {
  updateDynamicLimits();
});

//============================================================================
// LocalStorage Persistence Functions
//============================================================================

function saveDataToLocalStorage() {
  if (!window.localStorage) {
    console.warn('localStorage not available');
    return false;
  }

  try {
    // Check available storage
    const availableBytes = storageQuotaMB * 1024 * 1024 * STORAGE_SAFETY_MARGIN;
    let logsToSave = otLogBuffer;
    
    // Estimate actual size by sampling
    let estimatedSize = 0;
    if (logsToSave.length > 0) {
      const sampleSize = Math.min(50, logsToSave.length);
      let sampleBytes = 0;
      for (let i = 0; i < sampleSize; i++) {
        const idx = Math.floor(Math.random() * logsToSave.length);
        sampleBytes += JSON.stringify(logsToSave[idx]).length;
      }
      const avgSize = sampleBytes / sampleSize;
      estimatedSize = avgSize * logsToSave.length;
    }
    
    // If too large, keep only the most recent entries that fit
    if (estimatedSize > availableBytes) {
      const avgSize = estimatedSize / logsToSave.length;
      const maxEntries = Math.floor(availableBytes / avgSize);
      logsToSave = otLogBuffer.slice(-maxEntries);
      console.log(`Truncating saved logs from ${otLogBuffer.length} to ${maxEntries} entries to fit storage (${(estimatedSize/1024/1024).toFixed(2)} MB → ${(availableBytes/1024/1024).toFixed(2)} MB)`);
    }
    
    const logData = JSON.stringify(logsToSave);
    localStorage.setItem(PERSISTENCE_KEY_LOGS, logData);
    
    // Save user preferences
    const prefs = {
      autoScroll: autoScroll,
      showTimestamps: showTimestamps,
      captureMode: captureMode,
      searchTerm: searchTerm,
      savedAt: new Date().toISOString()
    };
    localStorage.setItem(PERSISTENCE_KEY_PREFS, JSON.stringify(prefs));
    
    console.log(`Saved ${logsToSave.length} log entries to localStorage`);
    return true;
    
  } catch (e) {
    // Handle quota exceeded or other errors
    console.error('Failed to save to localStorage:', e);
    
    // If quota exceeded, try with fewer entries
    if (e.name === 'QuotaExceededError' || e.code === 22) {
      console.warn('Storage quota exceeded, attempting to save fewer entries...');
      try {
        // Try with half the entries
        const halfEntries = otLogBuffer.slice(-Math.floor(otLogBuffer.length / 2));
        const logData = JSON.stringify(halfEntries);
        localStorage.setItem(PERSISTENCE_KEY_LOGS, logData);
        console.log(`Saved ${halfEntries.length} log entries (reduced) to localStorage`);
        return true;
      } catch (e2) {
        console.error('Even reduced save failed:', e2);
        return false;
      }
    }
    return false;
  }
}

function restoreDataFromLocalStorage() {
  if (!window.localStorage) {
    console.warn('localStorage not available');
    return false;
  }

  try {
    // Restore log buffer
    const savedLogs = localStorage.getItem(PERSISTENCE_KEY_LOGS);
    if (savedLogs) {
      const logs = JSON.parse(savedLogs);
      if (Array.isArray(logs) && logs.length > 0) {
        otLogBuffer = logs;
        updateFilteredBuffer();
        scheduleDisplayUpdate();
        console.log(`Restored ${logs.length} log entries from localStorage`);
      }
    }
    
    // Restore preferences
    const savedPrefs = localStorage.getItem(PERSISTENCE_KEY_PREFS);
    if (savedPrefs) {
      const prefs = JSON.parse(savedPrefs);
      if (prefs) {
        autoScroll = prefs.autoScroll !== undefined ? prefs.autoScroll : true;
        showTimestamps = prefs.showTimestamps !== undefined ? prefs.showTimestamps : true;
        captureMode = prefs.captureMode !== undefined ? prefs.captureMode : false;
        searchTerm = prefs.searchTerm || '';
        
        // Recalculate dynamic limits based on current system
        updateDynamicLimits();
        
        console.log(`Restored preferences from localStorage (saved at ${prefs.savedAt || 'unknown'})`);
        
        // Update UI to match restored preferences (will be applied when controls initialize)
        setTimeout(() => {
          const chkAutoScroll = document.getElementById('chkAutoScroll');
          if (chkAutoScroll) chkAutoScroll.checked = autoScroll;
          
          const chkShowTimestamp = document.getElementById('chkShowTimestamp');
          if (chkShowTimestamp) chkShowTimestamp.checked = showTimestamps;
          
          const searchLog = document.getElementById('searchLog');
          if (searchLog && searchTerm) searchLog.value = searchTerm;
        }, 100);
      }
    }
    
    return true;
    
  } catch (e) {
    console.error('Failed to restore from localStorage:', e);
    // Clear corrupted data
    try {
      localStorage.removeItem(PERSISTENCE_KEY_LOGS);
      localStorage.removeItem(PERSISTENCE_KEY_PREFS);
    } catch (e2) {}
    return false;
  }
}

function clearStoredData() {
  if (!window.localStorage) return;
  try {
    localStorage.removeItem(PERSISTENCE_KEY_LOGS);
    localStorage.removeItem(PERSISTENCE_KEY_PREFS);
    console.log('Cleared stored log data from localStorage');
  } catch (e) {
    console.error('Failed to clear localStorage:', e);
  }
}

function debouncedSave() {
  // Clear existing timer
  if (debouncedSaveTimer) {
    clearTimeout(debouncedSaveTimer);
  }
  
  // Set new timer - save after 2 seconds of no new data
  debouncedSaveTimer = setTimeout(() => {
    const now = Date.now();
    // Only save if it's been at least 1 second since last save (rate limiting)
    if (now - lastSaveTimestamp >= 1000) {
      saveDataToLocalStorage();
      lastSaveTimestamp = now;
    }
  }, DEBOUNCE_SAVE_MS);
}

function startPersistenceTimer() {
  if (persistenceTimer) return; // Already running
  
  // Fallback save every 30 seconds (in case debounced saves don't trigger)
  persistenceTimer = setInterval(() => {
    if (otLogBuffer.length > 0) {
      const now = Date.now();
      // Only save if no recent save happened
      if (now - lastSaveTimestamp >= PERSISTENCE_INTERVAL_MS) {
        saveDataToLocalStorage();
        lastSaveTimestamp = now;
      }
    }
  }, PERSISTENCE_INTERVAL_MS);
  
  console.log('Started localStorage persistence (debounced saves + 30s fallback)');
}

function stopPersistenceTimer() {
  if (persistenceTimer) {
    clearInterval(persistenceTimer);
    persistenceTimer = null;
    console.log('Stopped localStorage persistence timer');
  }
}

// Save data when page is about to unload
window.addEventListener('beforeunload', function() {
  if (otLogBuffer.length > 0) {
    saveDataToLocalStorage();
  }
});

// Expose for debug console
window.otgwPersistence = {
  save: saveDataToLocalStorage,
  restore: restoreDataFromLocalStorage,
  clear: clearStoredData,
  info: function() {
    console.log('═══════════════════════════════════════');
    console.log('Log Buffer Info');
    console.log('═══════════════════════════════════════');
    console.log('Entries in buffer:', otLogBuffer.length.toLocaleString());
    console.log('Current limit:', maxLogLines ? maxLogLines.toLocaleString() : 'calculating...');
    console.log('Capture mode:', captureMode ? 'ON (memory-limited)' : 'OFF (balanced)');
    console.log('');
    
    const memInfo = getActualMemoryUsage();
    estimateMemoryUsage();
    console.log('Memory Usage');
    console.log('═══════════════════════════════════════');
    if (performance.memory) {
      console.log('JS Heap used:', memInfo.used.toFixed(1), 'MB');
      console.log('JS Heap total:', memInfo.total.toFixed(1), 'MB');
      console.log('JS Heap limit:', memInfo.limit.toFixed(1), 'MB');
      console.log('Available:', memInfo.available.toFixed(1), 'MB');
    }
    console.log('Buffer estimate:', currentMemoryUsageMB.toFixed(2), 'MB');
    console.log('');
    
    console.log('LocalStorage');
    console.log('═══════════════════════════════════════');
    console.log('Storage quota:', storageQuotaMB.toFixed(1), 'MB');
    console.log('Safety margin:', (STORAGE_SAFETY_MARGIN * 100) + '%');
    
    try {
      const logs = localStorage.getItem(PERSISTENCE_KEY_LOGS);
      const prefs = localStorage.getItem(PERSISTENCE_KEY_PREFS);
      console.log('Stored logs:', logs ? (logs.length / 1024).toFixed(2) + ' KB' : '0 KB');
      console.log('Stored prefs:', prefs ? (prefs.length / 1024).toFixed(2) + ' KB' : '0 KB');
      console.log('');
      
      if (prefs) {
        console.log('Preferences:', JSON.parse(prefs));
      }
    } catch (e) {
      console.error('Error reading storage:', e);
    }
    console.log('═══════════════════════════════════════');
  }
};

//============================================================================
function resetWSWatchdog() {
  console.log('[WebSocket] Watchdog reset (' + (WS_WATCHDOG_TIMEOUT/1000) + 's)');
  if (wsWatchdogTimer) {
    clearTimeout(wsWatchdogTimer);
    console.log('[WebSocket] Watchdog timer cleared');
  }
  wsWatchdogTimer = setTimeout(function() {
      console.warn('[WebSocket] WATCHDOG TIMEOUT - no data received');
      // Closing the socket will trigger onclose, which triggers the reconnect logic
      if (otLogWS) {
        console.warn('[WebSocket] Watchdog closing connection (readyState: ' + otLogWS.readyState + ')');
        otLogWS.close();
      } else {
        console.warn('[WebSocket] Watchdog fired with no active socket; reconnecting');
        // If socket is somehow null but watchdog fired, force a new connection
        // We reuse the last known 'force' state implicitly by calling without args (undefined is falsy)
        // or we could store the last 'force' state in a global if critical.
        // For now, attempting standard connect is safe.
        initOTLogWebSocket(false);
      }
  }, WS_WATCHDOG_TIMEOUT);
}

//============================================================================
function getOTLogDisplayState() {
  // Detect if accessed via HTTPS reverse proxy
  // WebSocket (ws://) won't work when page is served over HTTPS due to mixed content blocking
  const isProxied = window.location.protocol === 'https:';

  // Detect smartphone (iPhone or Android Phone)
  const isPhone = /iPhone|iPod/.test(navigator.userAgent) ||
                 (/Android/.test(navigator.userAgent) && /Mobile/.test(navigator.userAgent));

  // Also check screen width as a fallback (standard breakpoint for tablets is 768px)
  const isSmallScreen = window.innerWidth < 768;

  const state = {
    isProxied: isProxied,
    isPhone: isPhone,
    isSmallScreen: isSmallScreen,
    disabled: (isProxied || isPhone || isSmallScreen)
  };
  
  return state;
}

//============================================================================
function updateOTLogResponsiveState() {
  if (flashModeActive) return;

  const logSection = document.getElementById('otLogSection');
  if (!logSection) {
    return;
  }

  const displayState = getOTLogDisplayState();

  if (displayState.disabled) {
    logSection.classList.add('hidden');
    disconnectOTLogWebSocket();
    return;
  }

  if (logSection.classList.contains('hidden')) {
    logSection.classList.remove('hidden');
  }

  if (!otLogWS || otLogWS.readyState === WebSocket.CLOSED) {
    initOTLogWebSocket();
  }
}

//============================================================================
function handleOTLogResize() {
  if (otLogResizeTimer) {
    clearTimeout(otLogResizeTimer);
  }
  otLogResizeTimer = setTimeout(function() {
    updateOTLogResponsiveState();
  }, 200);
}

//============================================================================
function initOTLogWebSocket(force) {
  console.log('[WebSocket] initOTLogWebSocket(force=' + force + ', flashMode=' + flashModeActive + ')');
  // Don't connect if in flash mode
  if (flashModeActive) {
    console.log('[WebSocket] Skipping connect: flash mode active');
    return;
  }

  const displayState = getOTLogDisplayState();

  if (displayState.disabled && !force && !isFlashing) {
    console.log('[WebSocket] Skipping connect: display disabled');
    if (displayState.isProxied) {
      console.log("[WebSocket] FALLBACK: HTTPS reverse proxy detected. WebSocket connections not supported. Disabling OpenTherm Monitor.");
    } else if (displayState.isPhone) {
      console.log("[WebSocket] FALLBACK: Smartphone detected. Disabling OpenTherm Monitor to save resources.");
    } else if (displayState.isSmallScreen) {
      console.log("[WebSocket] FALLBACK: Small screen detected (width: " + window.innerWidth + "px). Disabling OpenTherm Monitor.");
    }
    const logSection = document.getElementById('otLogSection');
    if (logSection) {
      logSection.classList.add('hidden');
    }
    return; // Do not connect WebSocket
  }

  // Clear any pending reconnect timer
  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = null;
  }
  // Clear any pending watchdog
  if (wsWatchdogTimer) {
    clearTimeout(wsWatchdogTimer);
    wsWatchdogTimer = null;
  }

  const wsHost = window.location.hostname;
  const wsPort = WEBSOCKET_PORT;
  const wsURL = 'ws://' + wsHost + ':' + wsPort + '/';
  console.log('[WebSocket] Connecting to ' + wsURL);
  
  // Increment connection attempt counter
  wsConnectionAttempts++;
  console.log('[WebSocket] Connection attempt #' + wsConnectionAttempts);
  
  // Close existing connection if it exists
  if (otLogWS) {
    // Remove listeners to avoid double-triggers during manual cleanup
    otLogWS.onclose = null; 
    otLogWS.onerror = null;
    try {
      if (otLogWS.readyState === WebSocket.OPEN || otLogWS.readyState === WebSocket.CONNECTING) {
        otLogWS.close();
      }
    } catch(e) {
    }
    otLogWS = null;
  }
  
  try {
    otLogWS = new WebSocket(wsURL);
    console.log('[WebSocket] WebSocket created');
    
    otLogWS.onopen = function() {
      console.log('[WebSocket] OPEN');
      wsSuccessfulConnections++;
      wsLastConnectTime = new Date();
      console.log('[WebSocket] Connect count: ' + wsSuccessfulConnections);
      
      updateWSStatus(true);
      // Clear any reconnect timer just in case
      if (wsReconnectTimer) {
        clearTimeout(wsReconnectTimer);
        wsReconnectTimer = null;
      }
      // Reset reconnect attempt counter on successful connection
      wsReconnectAttempts = 0;
      
      resetWSWatchdog();
      
      // Start auto-save timer for data persistence
      startPersistenceTimer();
      
      // Record reconnect event in graph
      if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.recordReconnect === 'function') {
        OTGraph.recordReconnect();
      }
    };
    
    otLogWS.onclose = function(event) {
      console.log('[WebSocket] CLOSE code=' + event.code + ' clean=' + event.wasClean + ' reason=' + (event.reason || ''));
      wsLastDisconnectTime = new Date();
      
      // Calculate connection duration if we had a successful connection
      if (wsLastConnectTime) {
        wsConnectionDuration = (wsLastDisconnectTime - wsLastConnectTime) / 1000;
      }
      
      
      updateWSStatus(false);
      
      // Record disconnect event in graph
      if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.recordDisconnect === 'function') {
        OTGraph.recordDisconnect();
      }
      
      // Stop watchdog
      if (wsWatchdogTimer) {
        clearTimeout(wsWatchdogTimer);
        wsWatchdogTimer = null;
      }
      // Attempt to reconnect after 5 seconds if not already scheduled
      if (!wsReconnectTimer) {
        wsReconnectAttempts++;
        let delay = isFlashing ? 1000 : 5000;
        console.log('[WebSocket] Reconnect attempt #' + wsReconnectAttempts + ' in ' + delay + 'ms');
        wsReconnectTimer = setTimeout(function() { initOTLogWebSocket(force); }, delay);
      } else {
        console.log('[WebSocket] Reconnect already scheduled');
      }
    };
    
    otLogWS.onerror = function(error) {
      console.log('[WebSocket] ERROR', error);
      updateWSStatus(false);
      // onclose will usually follow, but we ensure cleanup
      try {
        if (otLogWS && otLogWS.readyState !== WebSocket.CLOSED) {
          otLogWS.close();
        }
      } catch(e) {
      }
    };
    
    otLogWS.onmessage = function(event) {
      console.log('[WebSocket] MESSAGE bytes=' + (event.data ? event.data.length : 0));
      resetWSWatchdog();

      if (typeof event.data === 'string') {
        if (event.data.length > 200) {
          console.log('[WebSocket] MESSAGE data (truncated): ' + event.data.substring(0, 200) + '...');
        } else {
          console.log('[WebSocket] MESSAGE data: ' + event.data);
        }
      }

      // Handle keepalive messages (don't log or add to buffer)
      if (typeof event.data === 'string' && event.data.includes('"type":"keepalive"')) {
        console.log('[WebSocket] KEEPALIVE');
        return;
      }

      if (typeof handleFlashMessage === "function") {
        if (handleFlashMessage(event.data)) {
          return;
        }
      }
      
      let data = event.data;
      let isObject = false;

      try {
        if (data && typeof data === 'string' && (data.startsWith('{') || data.startsWith('['))) {
          data = JSON.parse(data);
          isObject = true;
        }
      } catch(e) {
        // ignore JSON parse error, treat as text
      }

      if (!isObject && typeof data === 'string') {
        const parsed = parseLogLine(data);
        if (parsed) {
          data = parsed;
        }
      }
      addLogLine(data);
    };
    
  } catch (e) {
    console.log('[WebSocket] CREATE FAILED', e);
    updateWSStatus(false);
    if (!wsReconnectTimer) {
      wsReconnectAttempts++;
      let delay = isFlashing ? 1000 : 5000;
      console.log('[WebSocket] Reconnect attempt #' + wsReconnectAttempts + ' in ' + delay + 'ms (create failed)');
      wsReconnectTimer = setTimeout(function() { initOTLogWebSocket(force); }, delay);
    } else {
      console.log('[WebSocket] Reconnect already scheduled (create failed)');
    }
  }
  
  // Setup UI event handlers only once
  if (!otLogControlsInitialized) {
    setupOTLogControls();
  }
}

//============================================================================
function disconnectOTLogWebSocket() {
  console.log('[WebSocket] ═══════════════════════════════════════');
  console.log('[WebSocket] DISCONNECT requested');
  console.log('[WebSocket] Current state: ' + (otLogWS ? otLogWS.readyState : 'null'));
  
  // Clear any pending reconnect timer
  if (wsReconnectTimer) {
    console.log('[WebSocket] Clearing pending reconnect timer');
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = null;
  }
  // Clear watchdog
  if (wsWatchdogTimer) {
    console.log('[WebSocket] Clearing watchdog timer');
    clearTimeout(wsWatchdogTimer);
    wsWatchdogTimer = null;
  }

  if (otLogWS) {
    const currentState = otLogWS.readyState;
    console.log('[WebSocket] Disconnecting WebSocket (readyState: ' + currentState + ')');
    
    // Remove event listeners to prevent auto-reconnect
    otLogWS.onclose = null;
    otLogWS.onerror = null;
    otLogWS.onmessage = null;
    
    if (otLogWS.readyState === WebSocket.OPEN || otLogWS.readyState === WebSocket.CONNECTING) {
      console.log('[WebSocket] Closing active/connecting WebSocket');
      otLogWS.close();
    } else {
      console.log('[WebSocket] WebSocket already closed or closing');
    }
    otLogWS = null;
    console.log('[WebSocket] WebSocket object set to null');
  } else {
    console.log('[WebSocket] No active WebSocket to disconnect');
  }
  
  updateWSStatus(false);
  console.log('[WebSocket] Disconnect complete');
  console.log('[WebSocket] ═══════════════════════════════════════');
}

//============================================================================
function updateWSStatus(connected) {
  const statusEl = document.getElementById('wsStatus');
  const statusTextEl = document.getElementById('wsStatusText');
  
  if (!statusEl || !statusTextEl) return;
  
  if (connected) {
    statusEl.className = 'ws-status ws-connected';
    statusTextEl.textContent = 'Connected';
    // statusEl.style.color = 'green'; // Force color - removed, using CSS class
  } else {
    statusEl.className = 'ws-status ws-disconnected';
    statusTextEl.textContent = 'Disconnected';
    // statusEl.style.color = 'red'; // Force color - removed, using CSS class
  }
}

//============================================================================
function formatLogLine(logLine) {
  if (!logLine) return "";
  
  // Construct display line from the incoming JSON fields.
  const pad = (str, len) => (str + "").padEnd(len, ' ');
  const padStart = (str, len) => (str + "").padStart(len, ' ');

  let raw = (typeof logLine.raw === 'string' && logLine.raw) ? logLine.raw.trim() : '';
  if (!raw) raw = '00000000';
  if (raw.length > 9) raw = raw.slice(0, 9);
  const valid = (typeof logLine.valid === 'string' && logLine.valid.length) ? logLine.valid[0] : ' ';
  const id = (logLine.id !== undefined && logLine.id !== null) ? String(logLine.id) : "0";
  const label = (typeof logLine.label === 'string' && logLine.label.trim() !== '') ? logLine.label : '';
  const source = (typeof logLine.source === 'string') ? logLine.source : '';
  const dir = (typeof logLine.dir === 'string' && logLine.dir.trim() !== '') ? logLine.dir : '';

  let value = '';
  if (logLine.value !== undefined && logLine.value !== null && String(logLine.value) !== '') {
    value = String(logLine.value);
  } else if (typeof logLine.val === 'number') {
    value = String(logLine.val);
  }

  // Add unit if present
  const unit = (typeof logLine.unit === 'string' && logLine.unit.trim() !== '') ? logLine.unit : '';

  // Required display format:
  // HH:MM:SS.mmmmmm Source(18) Raw(9) ID(3) Direction(16) Valid(1) Label = Value
  const rawWidth = (raw.length > 8) ? 9 : 8;
  
  // Build the log line with all components
  let text = pad(source, 18) + " " + padStart(raw, rawWidth) + " " + padStart(id, 3);
  
  // Direction field (always 16 characters wide, even if empty)
  text += " " + pad(dir, 16);
  
  text += " " + valid;

  if (label) {
    text += " " + label;
    if (value) {
      text += " = " + value;
      if (unit) text += " " + unit;
    }
  } else if (value) {
    text += " " + value;
    if (unit) text += " " + unit;
  }

  return text;
}

function parseLogLine(line) {
  if (typeof line !== 'string') return null;

  // Expected format (New with high-res timestamp):
  // HH:MM:SS.mmmmmm Source(18) Raw(9) ID(3) Type(16) Validity(1) Label = Value
  
  const obj = {
    time: "",
    source: "",
    raw: "",
    id: 0,
    dir: "",
    valid: " ",
    label: "",
    value: "",
    unit: ""
  };
  
  let offset = 0;
  
  // Detect timestamp: HH:MM:SS.mmmmmm (15 chars) or short HH:MM:SS.mmm (12 chars)
  // Regex must be robust.
  const tsMatch = line.match(/^(\d{2}:\d{2}:\d{2}\.\d{3,6})\s/);
  if (tsMatch) {
      obj.time = tsMatch[1];
      offset = tsMatch[0].length; // Use actual length of match
  } else {
      // Fallback timestamp
      obj.time = new Date().toLocaleTimeString('en-GB', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' }) + "." + (new Date().getMilliseconds() + "").padStart(3, '0');
  }
  
  // Adjust base offsets based on offset
  const oSource = 0 + offset;
  const oRaw    = 19 + offset; // Source(18) + Space(1) = 19
  const oId     = 29 + offset; // Raw(9) + Space(1) = 10. 19+10=29
  const oType   = 33 + offset; // ID(3) + Space(1) = 4. 29+4=33
  const oValid  = 49 + offset; // Type(16). 33+16=49
  const oPayload= 51 + offset; // Valid(1) + Space(1) = 2. 49+2=51 

  try {
     // Safety check on length (Source + Raw + ID must exist)
     if (line.length < 32 + offset) return null; 
     
     obj.source = line.substring(oSource, oSource + 18).trim();
     obj.raw = line.substring(oRaw, oRaw + 9).trim(); // 9 chars for Raw
     obj.id = parseInt(line.substring(oId, oId + 3).trim(), 10);
     if (isNaN(obj.id)) obj.id = 0;
     
     if (line.length >= oType) {
        obj.dir = line.substring(oType, oType + 16).trim();
        
        if (line.length > oValid) {
           let v = line.substring(oValid, oValid + 1);
           // Updated validity characters: P=Parity, -=Skipped, " "=Invalid, >=Valid
           if (['P','-',' ','>'].includes(v)) obj.valid = v;
        }
        
        // Payload starts after the validity character
        // Firmware does not output a space after the validity character, so payload starts immediately at oValid + 1 (which is oPayload)
        
        if (line.length >= oPayload) {
           // Payload
           let payload = line.substring(oPayload).trim();
           if (payload.includes('=')) {
              let parts = payload.split('=');
              obj.label = parts[0].trim();
              
              let valPart = parts.slice(1).join('=').trim();
              obj.value = valPart;
           } else {
              if (payload.length > 0) obj.label = payload;
           }
        }
     }
  } catch (e) {
     console.error("Error parsing log line:", e);
     return null;
  }
  
  return obj;
}

function addLogLine(logLine) {
  if (!logLine) return;
  // Enforce object only (JSON logging)
  if (typeof logLine !== 'object') return;
  
  // JSON format: at minimum {time, source, id, label, value} with optional `val` and `data`.
  let timestamp = logLine.time || "00:00:00.000000";
  
  const logEntry = {
    time: timestamp,
    data: logLine
  };

  // Write to file stream if enabled
  if (isStreamingToFile) {
    writeToStream(logEntry);
  }
  
  // Add to buffer
  otLogBuffer.push(logEntry);
  
  // Process for Statistics
  if (typeof processStatsLine === 'function') {
      processStatsLine(logLine);
  }
  
  // Process for Graph
  if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.processLine === 'function') {
      OTGraph.processLine(logLine);
  }

  // Check and update dynamic limits periodically (every 1000 lines)
  if (maxLogLines === null || otLogBuffer.length % 1000 === 0) {
    updateDynamicLimits();
  }

  // Trim buffer if exceeds calculated max
  if (maxLogLines !== null && otLogBuffer.length > maxLogLines) {
    const toRemove = otLogBuffer.length - maxLogLines;
    otLogBuffer.splice(0, toRemove);
    console.log(`Trimmed ${toRemove} old log entries (limit: ${maxLogLines.toLocaleString()})`);
  }
  
  // Trigger debounced save (progressive storage)
  debouncedSave();
  
  // Update filtered buffer
  updateFilteredBuffer();
  
  // Schedule display update (throttled)
  scheduleDisplayUpdate();
}

//============================================================================
function scheduleDisplayUpdate() {
  // Use requestAnimationFrame to throttle updates
  // This batches multiple rapid updates into a single render
  if (!updatePending) {
    updatePending = true;
    requestAnimationFrame(() => {
      updatePending = false;
      updateLogDisplay();
      updateLogCounters();
    });
  }
}

//============================================================================
function updateFilteredBuffer() {
  if (!searchTerm || searchTerm.trim() === '') {
    otLogFilteredBuffer = otLogBuffer;
  } else {
    const term = searchTerm.toLowerCase();
    otLogFilteredBuffer = otLogBuffer.filter(entry => 
      formatLogLine(entry.data).toLowerCase().includes(term)
    );
  }
}

//============================================================================
// Flag to ensure we only render at most once per animation frame
let logRenderScheduled = false;

// Internal function that performs the actual DOM update
function renderLogDisplay() {
  const container = document.getElementById('otLogContent');
  if (!container) {
    console.error("otLogContent element not found!");
    return;
  }

  // Always show up to RENDER_LIMIT lines
  const displayCount = Math.min(otLogFilteredBuffer.length, RENDER_LIMIT);
  const startIndex = Math.max(0, otLogFilteredBuffer.length - displayCount);
  const linesToShow = otLogFilteredBuffer.slice(startIndex);

  // Build HTML
  let html = '';
  linesToShow.forEach(entry => {
    const text = formatLogLine(entry.data);
    const line = showTimestamps ? `${entry.time} ${text}` : text;
    html += escapeHtml(line) + '\n';
  });

  container.innerHTML = html;

  // Auto-scroll to bottom if enabled
  if (autoScroll) {
    container.scrollTop = container.scrollHeight;
  }
}

// Public function: schedule a render, coalescing multiple updates
function updateLogDisplay() {
  if (logRenderScheduled) {
    return;
  }

  logRenderScheduled = true;

  window.requestAnimationFrame(function () {
    logRenderScheduled = false;
    renderLogDisplay();
  });
}

//============================================================================
function updateLogCounters() {
  const logLineCountEl = document.getElementById('logLineCount');
  const logFilteredCountEl = document.getElementById('logFilteredCount');
  const logLimitDisplayEl = document.getElementById('logLimitDisplay');
  const memUsageEl = document.getElementById('memUsage');

  if (logLineCountEl) {
    logLineCountEl.textContent = otLogBuffer.length;
  }
  
  if (logLimitDisplayEl) {
     logLimitDisplayEl.textContent = ' / ' + maxLogLines;
  }

  if (logFilteredCountEl) {
    logFilteredCountEl.textContent = otLogFilteredBuffer.length;
  }
  
  // Update memory display
  updateMemoryDisplay();
}

//============================================================================
function setupOTLogControls() {
  // Only setup event listeners once to prevent duplicates
  if (otLogControlsInitialized) {
    return;
  }
  
  // Auto-scroll checkbox
  document.getElementById('chkAutoScroll').addEventListener('change', function() {
    autoScroll = this.checked;
    if (typeof saveUISetting === 'function') saveUISetting('#uiAutoScroll', autoScroll);
  });

  // Toggle Capture Mode
  const chkCapture = document.getElementById('chkCaptureMode');
  if (chkCapture) {
    chkCapture.addEventListener('change', function(e) {
      captureMode = e.target.checked;
      updateDynamicLimits();
      
      if (!captureMode && otLogBuffer.length > maxLogLines) {
        // Trim buffer when disabling capture mode
        otLogBuffer.splice(0, otLogBuffer.length - maxLogLines);
        updateFilteredBuffer();
        updateLogDisplay();
      }
      
      updateLogCounters();
      if (typeof saveUISetting === 'function') saveUISetting('#uiCaptureMode', e.target.checked);
      console.log(`Capture mode ${captureMode ? 'enabled' : 'disabled'}, max lines: ${maxLogLines.toLocaleString()}`);
    });
  }

  // Toggle Stream Mode
  const chkStream = document.getElementById('chkStreamToFile');
  if (chkStream) {
    if (!('showDirectoryPicker' in window)) {
        // API not supported (requires Secure Context: HTTPS or Localhost)
        // Instead of hiding, disable and explain
        const lbl = document.getElementById('lblStreamToFile');
        if (lbl) {
            lbl.title = "Not Available: Requires HTTPS or Localhost (Browser Security)";
            lbl.style.opacity = "0.5";
            lbl.style.cursor = "not-allowed";
        }
        chkStream.disabled = true;
    } else {
       chkStream.addEventListener('change', async function(e) {
         if (e.target.checked) {
           const success = await startFileStreaming();
           if (!success) {
             e.target.checked = false; // Revert if cancelled or failed
           }
         } else {
           stopFileStreaming();
         }
       });
    }
  }
  
  // Clear log button
  document.getElementById('btnClearLog').addEventListener('click', function(e) {
    e.preventDefault();
    const count = otLogBuffer.length.toLocaleString();
    if (confirm(`Clear ${count} log entries from memory and browser storage?\n\nThis cannot be undone.`)) {
      otLogBuffer = [];
      otLogFilteredBuffer = [];
      updateLogDisplay();
      updateLogCounters();
      // Clear localStorage as well
      clearStoredData();
    }
  });
  
  // Download log
  document.getElementById('btnDownloadLog').addEventListener('click', function(e) {
    e.preventDefault();
    try {
      downloadLog(false);
    } catch (err) {
      console.error('Failed to download log:', err);
    }
  });

  // Auto Download Log
  const chkAutoDL = document.getElementById('chkAutoDownloadLog');
  if (chkAutoDL) {
    chkAutoDL.addEventListener('change', function(e) {
      toggleAutoDownloadLog(e.target.checked);
      if (typeof saveUISetting === 'function') saveUISetting('ui_autodownloadlog', e.target.checked);
    });
  }
  
  // Search functionality
  document.getElementById('searchLog').addEventListener('input', function(e) {
    searchTerm = e.target.value;
    updateFilteredBuffer();
    updateLogDisplay();
    updateLogCounters();
  });
  
  // Toggle timestamps
  document.getElementById('chkShowTimestamp').addEventListener('change', function(e) {
    showTimestamps = e.target.checked;
    updateLogDisplay();
    if (typeof saveUISetting === 'function') saveUISetting('ui_timestamps', e.target.checked);
  });
  
  // Manual scroll detection (disable auto-scroll checkbox if user scrolls up)
  let manualScrollTimeout = null;
  document.getElementById('otLogContent').addEventListener('scroll', function(e) {
    // Debounce scroll handling to avoid excessive DOM reads/writes
    if (manualScrollTimeout !== null) {
      clearTimeout(manualScrollTimeout);
    }
    
    const container = e.target;
    manualScrollTimeout = setTimeout(function() {
      const isAtBottom = container.scrollHeight - container.scrollTop <= container.clientHeight + 50;
      
      if (!isAtBottom && autoScroll) {
        // User scrolled up, disable auto-scroll
        autoScroll = false;
        const chk = document.getElementById('chkAutoScroll');
        if (chk) chk.checked = false;
      }
    }, 100);
  });
  
  // Mark as initialized after all listeners are successfully registered
  otLogControlsInitialized = true;
  updateLogCounters();
}

//============================================================================
// File Streaming Implementation (File System Access API)
//============================================================================
let logFlushTimer = null;

async function startFileStreaming() {
  // Browser Support Check
  if (!('showDirectoryPicker' in window)) {
    let msg = "File streaming is not supported by your browser.\n\n";
    if (!window.isSecureContext) {
      msg += "REASON: This feature requires a Secure Context (HTTPS).\n";
      msg += "Since OTGW uses HTTP, you must enable the 'Insecure origins treated as secure' flag in your browser for this IP address.";
    } else {
      msg += "Please use Google Chrome, Microsoft Edge, or Opera on Desktop.";
    }
    alert(msg);
    stopFileStreaming();
    return false;
  }

  try {
    // Prompt user to select directory
    if (!logDirectoryHandle) {
        logDirectoryHandle = await window.showDirectoryPicker({
            id: 'otmonitor-logs',
            mode: 'readwrite',
            startIn: 'documents'
        });
    }

    isStreamingToFile = true;
    
    // Start the rotation check timer (every minute)
    if (fileRotationTimer) clearInterval(fileRotationTimer);
    fileRotationTimer = setInterval(checkFileRotation, 60000);

    // Start flush timer (10s) to limit disk writes
    if (logFlushTimer) clearInterval(logFlushTimer);
    logFlushTimer = setInterval(processLogQueue, 10000);

    // Initial file open
    return await rotateLogFile();
  } catch (err) {
    if (err.name !== 'AbortError') {
       console.error("Error setting up file stream:", err);
       alert("Failed to setup file streaming: " + err.message);
    }
    stopFileStreaming();
    return false;
  }
}

function getTodayDateString() {
    const today = new Date();
    const dd = String(today.getDate()).padStart(2, '0');
    const mm = String(today.getMonth() + 1).padStart(2, '0');
    const yyyy = today.getFullYear();
    return `${dd}-${mm}-${yyyy}`;
}

async function rotateLogFile() {
    if (!logDirectoryHandle) return false;

    const dateStr = getTodayDateString();
    // If the date hasn't changed and we have a handle, do nothing
    if (currentLogDateStr === dateStr && fileStreamHandle) return true;

    try {
        currentLogDateStr = dateStr;
        const filename = `ot-monitor-${dateStr}.log`;

        // Get file handle (create if not exists)
        fileStreamHandle = await logDirectoryHandle.getFileHandle(filename, { create: true });
        
        console.log(`Rotated log file to: ${filename}`);
        
        // Update UI
        const displayEl = document.getElementById('logFileDisplay');
        const filenameEl = document.getElementById('currentLogFile');
        if (displayEl) displayEl.style.display = 'inline';
        if (filenameEl) filenameEl.textContent = filename;

        // Mark session start (Queue it)
        enqueueLogLine(`\n# Start of logging`);
        
        return true;
    } catch (e) {
        console.error("Error rotating log file:", e);
        stopFileStreaming();
        alert("Error creating log file for new day: " + e.message);
        return false;
    }
}

async function checkFileRotation() {
    if (!isStreamingToFile) return;
    const dateStr = getTodayDateString();
    if (currentLogDateStr !== dateStr) {
        console.log("Midnight detected, rotating log file...");
        enqueueLogLine(`# End of log for ${currentLogDateStr}`);
        
        // Best effort flush before rotation
        if (!isLogWriting) {
           await processLogQueue();
        } else {
           let checks = 0;
           while(isLogWriting && checks < 10) {
               await new Promise(r => setTimeout(r, 100));
               checks++;
           }
           await processLogQueue();
        }

        rotateLogFile();
    }
}

function stopFileStreaming() {
  isStreamingToFile = false;
  if (fileRotationTimer) {
      clearInterval(fileRotationTimer);
      fileRotationTimer = null;
  }
  if (logFlushTimer) {
      clearInterval(logFlushTimer);
      logFlushTimer = null;
  }
  
  // Flush remaining queue
  processLogQueue();
  
  // Hide UI
  const displayEl = document.getElementById('logFileDisplay');
  if (displayEl) displayEl.style.display = 'none';

  console.log("File streaming stopped.");
}

function enqueueLogLine(text) {
    if (!text) return;
    if (!text.endsWith('\n')) text += '\n';
    logWriteQueue.push(text);
}

async function processLogQueue() {
    if (isLogWriting || logWriteQueue.length === 0 || !fileStreamHandle) return;
    isLogWriting = true;

    try {
        // Drain queue
        const linesToWrite = [...logWriteQueue];
        logWriteQueue = []; 
        
        if (linesToWrite.length === 0) {
            isLogWriting = false;
            return;
        }

        // Open stream (This operation locks the file for the duration)
        const writable = await fileStreamHandle.createWritable({ keepExistingData: true });
        
        // Seek to end to ensure appending
        const file = await fileStreamHandle.getFile();
        if (file.size > 0) {
            await writable.seek(file.size);
        }
        
        // Write the batch of lines
        const blob = new Blob([linesToWrite.join('')], { type: 'text/plain' });
        await writable.write(blob);
        
        // Close to flush to disk immediately
        await writable.close();

    } catch (err) {
        console.error("Error writing log queue:", err);
        // If error occurs (e.g. permission revoked), stop streaming
        stopFileStreaming();
        const chk = document.getElementById('chkStreamToFile');
        if (chk) chk.checked = false;
        alert("File stream write failed: " + err.message);
    } finally {
        isLogWriting = false;
        // Check if more lines were added while we were writing
        if (logWriteQueue.length > 0) {
            // Process next batch immediately (microtask)
            processLogQueue();
        }
    }
}

async function writeToStream(entry) {
  if (!isStreamingToFile) return;
  try {
    const text = formatLogLine(entry.data);
    const line = showTimestamps ? `${entry.time} ${text}` : text;
    enqueueLogLine(line);
    streamBytesWritten += line.length + 1;
  } catch (err) {
    console.error("Error preparing stream write:", err);
  }
}

//============================================================================
function downloadLog(isAuto = false) {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
  // Different prefix for auto
  const prefix = isAuto ? 'otgw-log-auto-' : 'otgw-log-';
  const filename = `${prefix}${timestamp}.txt`;
  
  let content = '# OTGW Log Export\n';
  content += `# Exported: ${new Date().toLocaleString()}\n`;
  content += `# Total Lines: ${otLogBuffer.length}\n`;
  content += '#' + '='.repeat(70) + '\n\n';
  
  otLogBuffer.forEach(entry => {
    const text = formatLogLine(entry.data);
    const line = showTimestamps ? `${entry.time} ${text}` : text;
    content += line + '\n';
  });
  
  const blob = new Blob([content], { type: 'text/plain' });

  // Try to save to FileSystem Handle first (if available and function exists)
  if (isAuto && window.saveBlobToLogDir) {
        window.saveBlobToLogDir(filename, blob).then(success => {
            if (success) {
                console.log("Auto-saved log to disk: " + filename);
            } else {
                // Fallback
                 forceDownloadBlob(blob, filename);
            }
        });
  } else {
      forceDownloadBlob(blob, filename);
  }
}

function forceDownloadBlob(blob, filename) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// Auto Download Log Logic
let autoDownloadLogTimer = null;
function toggleAutoDownloadLog(enabled) {
    if (autoDownloadLogTimer) {
        clearInterval(autoDownloadLogTimer);
        autoDownloadLogTimer = null;
    }
    
    if (enabled) {
        console.log("Auto-Download Log enabled (every 15 minutes)");
        autoDownloadLogTimer = setInterval(() => {
            downloadLog(true);
        }, 15 * 60 * 1000); 
    }
}


//============================================================================
// Optimize HTML escaping with static map (faster than DOM element creation)
const escapeHtml = (function() {
  const escapeMap = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#39;'
  };
  const escapeRegex = /[&<>"']/g;
  
  return function(text) {
    if (!text) return '';
    return String(text).replace(escapeRegex, char => escapeMap[char]);
  };
})();

//============================================================================  
function loadUISettings() {
  console.log("loadUISettings() ..");
  fetch(APIGW + "v0/settings")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      let data = json.settings;
      for (let i in data) {
        let name = data[i].name;
        let val = data[i].value;
        
        // Map backend settings to UI elements
        if (name === "#uiAutoScroll") {
           let newVal = strToBool(val);
           autoScroll = newVal;
           const chk = document.getElementById('chkAutoScroll');
           if (chk) chk.checked = autoScroll;
        } else if (name === "#uiAutoDownloadLog") {
           let el = document.getElementById("chkAutoDownloadLog");
           if(el) {
             let isChecked = strToBool(val);
             el.checked = isChecked;
             toggleAutoDownloadLog(isChecked);
           }
        } else if (name === "#uiShowTimestamp") {
           let el = document.getElementById("chkShowTimestamp");
           // For this one, we also need to update global variable if needed, 
           // but the event listener handles it on change. 
           // Ideally we should trigger the logic.
           if(el) {
             el.checked = strToBool(val);
             showTimestamps = el.checked;
             updateLogDisplay(); 
           }
        } else if (name === "#uiCaptureMode") {
           let el = document.getElementById("chkCaptureMode");
           if(el) {
             el.checked = strToBool(val);
             captureMode = el.checked;
             updateDynamicLimits();
           }
        } else if (name === "#uiStreamToFile") {
           // We generally default this to false on reload as we lose the file handle
           // But if we wanted to persist it we'd need to re-prompt user.
           // For now, let's just leave it unchecked or respect it? 
           // Browser security prevents auto-reopen without user gesture usually.
           // So we skip restoring this one to true.
        } else if (name === "#uiAutoScreenshot") {
           let el = document.getElementById("chkAutoScreenshot");
           if(el) {
             el.checked = strToBool(val);
             // Graph might not be loaded yet or needs to be notified
             if (window.OTGraph) window.OTGraph.toggleAutoScreenshot(el.checked);
           }
        } else if (name === "#uiAutoExport") {
           let el = document.getElementById("chkAutoExport");
           if(el) {
             el.checked = strToBool(val);
             if (window.OTGraph) window.OTGraph.toggleAutoExport(el.checked);
           }
        }
      }
    })
    .catch(error => console.error("Error loading UI settings:", error));
}

function saveUISetting(field, value) {
  // Wrapper to ensure we use the backend API
  // field should start with # if it's a hidden setting
  console.log("Saving UI Setting [" + field + "] = " + value);
  sendPostSetting(field, value);
}

//============================================================================  
function initMainPage() {
  console.log("initMainPage()");
  
  loadUISettings();
  
  // Check if we're in flash mode (from sessionStorage)
  try {
    if (sessionStorage.getItem('flashMode') === 'true') {
      console.log('Flash mode detected from sessionStorage');
      flashModeActive = true;
    }
  } catch(e) { /* ignore */ }

  Array.from(document.getElementsByClassName('FSexplorer')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        console.log("newTab: goFSexplorer");
        location.href = "/FSexplorer";
      })
    }
  );
  Array.from(document.getElementsByClassName('btnSaveSettings')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        saveSettings();
        toggleHidden('adv_dropdown', true);
        toggleHidden('btnSaveSettings', true);
      });
    }
  );
  Array.from(document.getElementsByClassName('tabDeviceInfo')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        deviceinfoPage();
        toggleHidden('adv_dropdown', true);
        toggleHidden('btnSaveSettings', true);
      });
    }
  );
  Array.from(document.getElementsByClassName('tabPICflash')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        firmwarePage();
        toggleHidden('adv_dropdown', true);
        toggleHidden('btnSaveSettings', true);
      });
    }
  );
  Array.from(document.getElementsByClassName('basicSettings')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        settingsPage();
        toggleHidden('adv_dropdown', true);
        setVisible('btnSaveSettings', false);
      });
    }
  );
  Array.from(document.getElementsByClassName('adminSettings')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () { toggleHidden('adv_dropdown', false); });
    }
  );
  Array.from(document.getElementsByClassName('home')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        console.log("newTab: goBack");
        showMainPage();
        toggleHidden('btnSaveSettings', true);
      });
    }
  );


  // Restore data from localStorage to prevent loss on page reload
  restoreDataFromLocalStorage();
  
  setupPersistentUIListeners();
  loadPersistentUI();

  applyTheme();

  if (!otLogResponsiveInitialized) {
    window.addEventListener('resize', handleOTLogResize);
    otLogResponsiveInitialized = true;
  }

  updateOTLogResponsiveState();

  if (typeof OTGraph !== 'undefined') {
      OTGraph.init();
  }

  // Start time updates if not in flash mode
  if (!flashModeActive && !timeupdate) {
    refreshDevTime();
    refreshGatewayMode(true);
    timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
  }

  if (window.location.hash == "#tabPICflash") {
    firmwarePage();
  } else {
    showMainPage();
  }
} // initMainPage()

function showMainPage() {
  console.log("showMainPage()");
  clearInterval(tid);
  
  // Exit flash mode if it was active
  if (flashModeActive) {
    exitFlashMode();
  }
  
  refreshDevTime();
  refreshGatewayMode(true);
  
  document.getElementById("displayMainPage").classList.add('active');
  document.getElementById("displaySettingsPage").classList.remove('active');
  document.getElementById("displayDeviceInfo").classList.remove('active');
  document.getElementById("displayPICflash").classList.remove('active');
  
  refreshDevInfo();
  refreshOTmonitor();
  
  if (!flashModeActive) {
    tid = setInterval(function () { refreshOTmonitor(); }, 1000);
    // Initialize WebSocket for OT log streaming
    initOTLogWebSocket();
  }
}

function firmwarePage() {
  initOTLogWebSocket();
  clearInterval(tid);
  refreshDevTime();
  document.getElementById("displayMainPage").classList.remove('active');
  document.getElementById("displaySettingsPage").classList.remove('active');
  document.getElementById("displayDeviceInfo").classList.remove('active');
  var firmwarePage = document.getElementById("displayPICflash");
  refreshFirmware();
  document.getElementById("displayPICflash").classList.add('active');
} // deviceinfoPage()

function deviceinfoPage() {
  disconnectOTLogWebSocket();
  clearInterval(tid);
  refreshDevTime();
  document.getElementById("displayMainPage").classList.remove('active');
  document.getElementById("displaySettingsPage").classList.remove('active');
  document.getElementById("displayPICflash").classList.remove('active');
  var deviceinfoPage = document.getElementById("deviceinfoPage");
  refreshDeviceInfo();
  document.getElementById("displayDeviceInfo").classList.add('active');

} // deviceinfoPage()

function settingsPage() {
  disconnectOTLogWebSocket();
  clearInterval(tid);
  refreshDevTime();
  document.getElementById("displayMainPage").classList.remove('active');
  document.getElementById("displayDeviceInfo").classList.remove('active');
  document.getElementById("displayPICflash").classList.remove('active');
  var settingsPage = document.getElementById("settingsPage");
  refreshSettings();
  document.getElementById("displaySettingsPage").classList.add('active');

} // settingsPage()

function toggleHidden(className, hideOnly) {
  Array.from(document.getElementsByClassName(className)).forEach(
    function (el, idx, arr) {
      if (!el.classList.contains("hidden")) {
        el.classList.add("hidden");
      } else if (!hideOnly) {
        el.classList.remove("hidden");
      }
    }
  );
}

function setVisible(className, visible) {
  Array.from(document.getElementsByClassName(className)).forEach(
    function (el) {
      if (visible) {
        el.classList.remove("hidden");
      } else {
        el.classList.add("hidden");
      }
    }
  );
}

//============================================================================  
function refreshDevTime() {
  //console.log("Refresh api/v0/devtime ..");
  fetch(APIGW + "v0/devtime")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      for (let i in json.devtime) {
        if (json.devtime[i].name == "dateTime") {
          //console.log("Got new time ["+json.devtime[i].value+"]");
          const timeEl = document.getElementById('theTime');
          if (timeEl) timeEl.textContent = json.devtime[i].value;
        }
        if (json.devtime[i].name == "message") {
          const msgEl = document.getElementById('message');
          if (msgEl) {
            const msgText = json.devtime[i].value || '';
            msgEl.textContent = msgText;
            
            // Add warning class if message contains version mismatch warning
            if (msgText.toLowerCase().includes('littlefs') || 
                msgText.toLowerCase().includes('version') ||
                msgText.toLowerCase().includes('flash your')) {
              msgEl.classList.add('version-warning');
            } else {
              msgEl.classList.remove('version-warning');
            }
            
            // Hide element if no message
            if (msgText === '') {
              msgEl.style.display = 'none';
            } else {
              msgEl.style.display = 'block';
            }
          }
        }
      }
    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });

} // refreshDevTime()
//============================================================================      
// Global variable to store available firmware files info
let availableFirmwareFiles = [];

function refreshFirmware() {
  console.log("refreshFirmware() .. " + APIGW + "firmwarefilelist");
  
  let picInfo = { type: "Unknown", version: "Unknown", available: "Unknown", device: "Unknown" };

  fetch(APIGW + "v0/devinfo")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
       if (json.devinfo) {
         const data = json.devinfo;
         for (let i in data) {
           if (data[i].name === "picfwtype") picInfo.type = data[i].value;
           if (data[i].name === "picfwversion") picInfo.version = data[i].value;
           if (data[i].name === "picavailable") picInfo.available = data[i].value;
           if (data[i].name === "picdeviceid") picInfo.device = data[i].value;
         }
       }
       return fetch(APIGW + "firmwarefilelist");
    })
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(files => {
      console.log("parsed ... data is [" + JSON.stringify(files) + "]");
      availableFirmwareFiles = files; // Store for later use in flash success message

      let displayPICpage = document.getElementById('displayPICpage');

      while (displayPICpage.lastChild) {
        displayPICpage.lastChild.remove();
      }

      // --- PIC Information Section ---
      let infoDiv = document.createElement("div");
      infoDiv.setAttribute("class", "pic-info-header firmware-info-div");
      
      // Build info section using DOM methods for safety
      const deviceLabel = document.createElement("b");
      deviceLabel.textContent = "PIC Device:";
      infoDiv.appendChild(deviceLabel);
      infoDiv.appendChild(document.createTextNode(" " + picInfo.device));
      infoDiv.appendChild(document.createElement("br"));

      const typeLabel = document.createElement("b");
      typeLabel.textContent = "PIC Type:";
      infoDiv.appendChild(typeLabel);
      infoDiv.appendChild(document.createTextNode(" "));
      const typeSpan = document.createElement("span");
      typeSpan.id = "pic_type_display";
      typeSpan.textContent = picInfo.type;
      infoDiv.appendChild(typeSpan);
      infoDiv.appendChild(document.createElement("br"));

      const versionLabel = document.createElement("b");
      versionLabel.textContent = "PIC Firmware Version:";
      infoDiv.appendChild(versionLabel);
      infoDiv.appendChild(document.createTextNode(" "));
      const versionSpan = document.createElement("span");
      versionSpan.id = "pic_version_display";
      versionSpan.textContent = picInfo.version;
      infoDiv.appendChild(versionSpan);
      displayPICpage.appendChild(infoDiv);

      let tableDiv = document.createElement("div");
      tableDiv.setAttribute("class", "pictable");

      var rowDiv = document.createElement("div");
      rowDiv.setAttribute("class", "picrow firmware-row-bold");
      rowDiv.setAttribute("id", "firmwarename");
      //--- field Name ---
      var fldDiv = document.createElement("div");
      fldDiv.setAttribute("class", "piccolumn1");
      fldDiv.textContent = "Firmware name"
      rowDiv.appendChild(fldDiv);
      //--- version on screen ---
      var valDiv = document.createElement("div");
      valDiv.setAttribute("class", "piccolumn2");
      valDiv.textContent = "Version"
      rowDiv.appendChild(valDiv);
      //--- size on screen ---
      var sizDiv = document.createElement("div");
      sizDiv.setAttribute("class", "piccolumn3");
      sizDiv.textContent = "Size"
      rowDiv.appendChild(sizDiv);
      //--- refresh icon ---
      var btn = document.createElement("div");
      btn.setAttribute("class", "piccolumn4");
      rowDiv.appendChild(btn);
      //--- flash to pic icon---
      var btn = document.createElement("div");
      rowDiv.appendChild(btn);
      tableDiv.appendChild(rowDiv);

      for (let i in files) {
        console.log("[" + files[i].name + "]=>[" + files[i].version + "]=>[" + files[i].size + "]");

        // var displayPICflash = document.getElementById('displayPICflash');          
        var rowDiv = document.createElement("div");
        rowDiv.setAttribute("class", "picrow");
        rowDiv.setAttribute("id", "firmware_" + files[i].name);
        // rowDiv.style.background = "lightblue";
        //--- field Name ---
        var fldDiv = document.createElement("div");
        fldDiv.setAttribute("class", "piccolumn1");
        fldDiv.textContent = files[i].name;
        rowDiv.appendChild(fldDiv);
        //--- version on screen ---
        var valDiv = document.createElement("div");
        valDiv.setAttribute("class", "piccolumn2");
        valDiv.textContent = files[i].version;
        rowDiv.appendChild(valDiv);
        //--- size on screen ---
        var sizDiv = document.createElement("div");
        sizDiv.setAttribute("class", "piccolumn3");
        sizDiv.textContent = files[i].size;
        rowDiv.appendChild(sizDiv);
        //--- refresh icon ---
        var btn = document.createElement("div");
        btn.setAttribute("class", "piccolumn4");
        var a = document.createElement('a');
        // a.title = "Update";
        a.href = localURL + '/pic?action=refresh&name=' + files[i].name + '&version=' + files[i].version;
        var img = document.createElement('img');
        img.src = localURL + '/update.png';
        img.title = "Update firmware from web";
        img.className = 'firmware-icon';
        img.setAttribute = ("alt", "Update");
        a.appendChild(img);
        btn.appendChild(a);
        rowDiv.appendChild(btn);
        //--- flash to pic icon---
        var btn = document.createElement("div");
        btn.setAttribute("class", "piccolumn5");
        var a = document.createElement('a');
        // a.href = localURL + '/pic?action=upgrade&name=' + files[i].name + '&version=' + files[i].version;
        a.href = "#";
        let fileName = files[i].name; // Capture for closure
        a.onclick = function(e) { e.preventDefault(); startFlash(fileName); };
        
        var img = document.createElement('img');
        img.src = localURL + '/system_update.png'
        img.title = "Install firmware onto pic";
        img.className = 'firmware-icon';
        img.setAttribute = ("alt", "Install");
        a.appendChild(img);
        btn.appendChild(a);
        rowDiv.appendChild(btn);
        tableDiv.appendChild(rowDiv);
      }
      displayPICpage.appendChild(tableDiv);
      
      // --- Flash Progress Bar Section ---
      let progressDiv = document.createElement("div");
      progressDiv.id = "flashProgressSection";
      progressDiv.className = "firmware-progress";
      // progressDiv.setAttribute("class", "otmontable"); 
      
      let barWrapper = document.createElement("div");
      barWrapper.setAttribute("class", "flashProgresBarWrapper");

      let barContainer = document.createElement("div");
      barContainer.setAttribute("class", "flashProgressBarContainer");
      
      let barFill = document.createElement("div");
      barFill.id = "flashProgressBar";
      
      let percentageText = document.createElement("div");
      percentageText.id = "flashPercentageText";
      percentageText.textContent = "Ready to flash";
      
      barContainer.appendChild(barFill);
      barContainer.appendChild(percentageText);
      barWrapper.appendChild(barContainer);

      progressDiv.appendChild(barWrapper);
      
      displayPICpage.appendChild(progressDiv);

    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });


}


//============================================================================  
function refreshDevInfo() {
  const devNameEl = document.getElementById('devName');
  if (devNameEl) devNameEl.textContent = "";
  fetch(APIGW + "v0/devinfo")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      console.log("parsed .., data is [" + JSON.stringify(json) + "]");
      data = json.devinfo;
      let hostname = "";
      let ipaddress = "";
      let version = "";
      for (let i in data) {
        if (data[i].name == "fwversion") {
          version = data[i].value;
        } else if (data[i].name == 'hostname') {
          hostname = data[i].value;
        } else if (data[i].name == 'ipaddress') {
          ipaddress = data[i].value;
        }
      }

      updateGatewayModeFromDevInfoEntries(data);

      const versionEl = document.getElementById('devVersion');
      if (versionEl) versionEl.textContent = version;

      const devNameEl = document.getElementById('devName');
      if (devNameEl) {
        devNameEl.textContent = hostname + (ipaddress ? " (" + ipaddress + ")" : "");
      }
    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });
} // refreshDevInfo()

//============================================================================  
function refreshOTmonitor() {
  if (flashModeActive || !isPageVisible()) return;

  data = {};
  fetch(APIGW + "v2/otgw/otmonitor")  //api/v2/otgw/otmonitor
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      data = json.otmonitor;

      let otMonPage = document.getElementById('mainPage');
      let otMonTable = document.querySelector(".otmontable");
      
      // If table doesn't exist, create it (and clear waiting/existing content)
      if (!otMonTable) {
        otMonPage.innerHTML = ""; 
        otMonTable = document.createElement("div");
        otMonTable.setAttribute("class", "otmontable");
        otMonPage.appendChild(otMonTable);
      }

      for (let i in data) {
        // Support for new Map-based JSON (less redundant):
        // If data is an object map, 'i' is the key (name).
        // If data is an array, 'i' is the index, and data[i] has a 'name' property.
        if (!data[i].name) data[i].name = i;

        //console.log("["+data[i].name+"]=>["+data[i].value+"]");

        if ((document.getElementById("otmon_" + data[i].name)) == null) { // if element does not exists yet, then build page
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "otmonrow");
          //rowDiv.setAttribute("id", "otmon_"+data[i].name);
          // rowDiv.style.background = "lightblue";
          if (data[i].epoch == 0) rowDiv.classList.add('hidden-row');
          var epoch = document.createElement("INPUT");
          epoch.setAttribute("type", "hidden");
          epoch.setAttribute("id", "otmon_epoch_" + data[i].name);
          epoch.name = data[i].name;
          epoch.value = data[i].epoch;
          rowDiv.appendChild(epoch);
          //--- field Name ---
          var fldDiv = document.createElement("div");
          fldDiv.setAttribute("class", "otmoncolumn1");
          fldDiv.textContent = translateToHuman(data[i].name);
          rowDiv.appendChild(fldDiv);
          //--- Value ---
          var valDiv = document.createElement("div");
          valDiv.setAttribute("class", "otmoncolumn2");
          valDiv.setAttribute("id", "otmon_" + data[i].name);
          if (data[i].value === "On") valDiv.innerHTML = "<span class='state-on'></span>";
          else if (data[i].value === "Off") valDiv.innerHTML = "<span class='state-off'></span>";
          else valDiv.textContent = data[i].value;
          rowDiv.appendChild(valDiv);
          //--- Unit  ---
          var unitDiv = document.createElement("div");
          unitDiv.setAttribute("class", "otmoncolumn3");
          unitDiv.textContent = data[i].unit;
          rowDiv.appendChild(unitDiv);
          otMonTable.appendChild(rowDiv);
        }
        else { //if the element exists, then update the value
          var update = document.getElementById("otmon_" + data[i].name);
          if (update.parentNode) {
            if (data[i].epoch == 0) {
              update.parentNode.classList.add('hidden-row');
            } else {
              update.parentNode.classList.remove('hidden-row');
            }
          }
          var epoch = document.getElementById("otmon_epoch_" + data[i].name);
          // if ((Number(epoch.value)==0) && (Number(data[i].epoch)>0)) {
          //   //console.log ("unhide based on epoch");
          //   //setTimeout(function () { update.style.visibility = 'visible';}, 0);
          //   needReload = true;
          // } 
          epoch.value = data[i].epoch;
          if (data[i].value === "On") update.innerHTML = "<span class='state-on'></span>";
          else if (data[i].value === "Off") update.innerHTML = "<span class='state-off'></span>";
          else update.textContent = data[i].value;
          //if (update.style.visibility == 'visible') update.textContent = data[i].value;

        }
      }
    })
    .catch(function (error) {
      if (flashModeActive || !isPageVisible()) return;
      var msg = (error && error.message) ? error.message : 'Load failed';
      if (msg.indexOf('Load failed') !== -1 || msg.indexOf('Failed to fetch') !== -1 || msg.indexOf('NetworkError') !== -1) {
        console.warn("refreshOTmonitor warning:", error);
      } else {
        console.error("refreshOTmonitor error:", error);
      }
      var waiting = document.getElementById('waiting');
      if (waiting) {
        waiting.textContent = 'Error: ' + msg + ' (Retrying...)';
        waiting.className = 'waiting-error';
      }
    });

} // refreshOTmonitor()


function refreshDeviceInfo() {
  console.log("refreshDeviceInfo() ..");

  data = {};
  fetch(APIGW + "v0/devinfo")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      data = json.devinfo;
      for (let i in data) {
        console.log("[" + data[i].name + "]=>[" + data[i].value + "]");
        var deviceinfoPage = document.getElementById('deviceinfoPage');
        if ((document.getElementById("devinfo_" + data[i].name)) == null) { // if element does not exists yet, then build page
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "devinforow");
          rowDiv.setAttribute("id", "devinfo_" + data[i].name);
          // rowDiv.style.background = "lightblue";
          //--- field Name ---
          var fldDiv = document.createElement("div");
          fldDiv.setAttribute("class", "devinfocolumn1");
          fldDiv.textContent = translateToHuman(data[i].name);
          rowDiv.appendChild(fldDiv);
          //--- value on screen ---
          var valDiv = document.createElement("div");
          valDiv.setAttribute("class", "devinfocolumn2");
          valDiv.textContent = data[i].value;
          rowDiv.appendChild(valDiv);
          deviceinfoPage.appendChild(rowDiv);
        }
      }
    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });

} // refreshDeviceInfo()

//============================================================================  
const hiddenSettings = [
  "ui_autoscroll",
  "ui_timestamps",
  "ui_capture",
  "ui_autoscreenshot", 
  "ui_autodownloadlog",
  "ui_autoexport",
  "ui_graphtimewindow"
];

function refreshSettings() {
  console.log("refreshSettings() ..");
  data = {};
  fetch(APIGW + "v0/settings")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      console.log("parsed .., data is [" + JSON.stringify(json) + "]");
      data = json.settings;
      const msgEl = document.getElementById("settingMessage");
      if (msgEl) msgEl.textContent = "";
      for (let i in data) {
        console.log("[" + data[i].name + "]=>[" + data[i].value + "]");
        // Skip hidden settings
        if (data[i].name.startsWith('#') || hiddenSettings.includes(data[i].name)) continue;

        var settings = document.getElementById('settingsPage');
        if ((document.getElementById("D_" + data[i].name)) == null) {
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "settingDiv");
          //----rowDiv.setAttribute("id", "settingR_"+data[i].name);
          rowDiv.setAttribute("id", "D_" + data[i].name);
          // rowDiv.setAttribute("style", "text-align: right;");
          // rowDiv.style.marginLeft = "10px";
          // rowDiv.style.marginRight = "10px";
          // rowDiv.style.minWidth = "850px";
          // rowDiv.style.border = "thick solid lightblue";
          // rowDiv.style.background = "lightblue";
          //--- field Name ---
          var fldDiv = document.createElement("div");
          fldDiv.className = 'settings-field-container';
          fldDiv.setAttribute("style", "margin-right: 10px;");
          fldDiv.textContent = translateToHuman(data[i].name);
          rowDiv.appendChild(fldDiv);
          //--- input ---
          var inputDiv = document.createElement("div");
          inputDiv.setAttribute("style", "text-align: left;");

          var sInput = document.createElement("input");
          //----sInput.setAttribute("id", "setFld_"+data[i].name);
          sInput.setAttribute("id", data[i].name);
          if (data[i].type == "b") {
            sInput.setAttribute("type", "checkbox");
            sInput.checked = strToBool(data[i].value);
          }
          else if (data[i].type == "s") {
            sInput.setAttribute("type", "text");
            sInput.setAttribute("maxlength", data[i].maxlen);
            sInput.setAttribute("size", (data[i].maxlen > 20 ? 20 : data[i].maxlen));
          }
          else if (data[i].type == "p") {
            sInput.setAttribute("type", "password");
            sInput.setAttribute("maxlength", data[i].maxlen);
            sInput.setAttribute("size", (data[i].maxlen > 20 ? 20 : data[i].maxlen));
          }
          else if (data[i].type == "f") {
            sInput.setAttribute("type", "number");
            sInput.max = data[i].max;
            sInput.min = data[i].min;
            sInput.step = (data[i].min + data[i].max) / 1000;
          }
          else if (data[i].type == "i") {
            sInput.setAttribute("type", "number");
            sInput.setAttribute("size", 10);
            sInput.max = data[i].max;
            sInput.min = data[i].min;
            //sInput.step = (data[i].min + data[i].max) / 1000;
            sInput.step = 1;
          }
          sInput.setAttribute("value", data[i].value);
          const fieldName = data[i].name;
          sInput.addEventListener('change',
            function () { 
              var inputEl = document.getElementById(fieldName);
              if (inputEl) {
                inputEl.className = "input-changed";
              }
              if (fieldName == "darktheme") {
                 document.getElementById('theme-style').href = this.checked ? "index_dark.css" : "index.css";
              }
              setVisible('btnSaveSettings', true);
            },
            false
          );
          sInput.addEventListener('keydown',
            function () { 
              var inputEl = document.getElementById(fieldName);
              if (inputEl) {
                inputEl.className = "input-changed";
              }
              setVisible('btnSaveSettings', true);
            },
            false
          );
          inputDiv.appendChild(sInput);

          rowDiv.appendChild(inputDiv);
          settings.appendChild(rowDiv);
        }
        else {
          //----document.getElementById("setFld_"+data[i].name).style.background = "white";
          const inputEl = document.getElementById(data[i].name);
          if (inputEl) {
            inputEl.className = "input-normal";
            //----document.getElementById("setFld_"+data[i].name).value = data[i].value;
            // document.getElementById(data[i].name).value = data[i].value;
            // FIX If checkbox change checked iso value
            if (data[i].type == "b")
              inputEl.checked = strToBool(data[i].value);
            else inputEl.value = data[i].value;
          }
        }
      }
      //console.log("-->done..");
    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });

} // refreshSettings()


//============================================================================  
function saveSettings() {
  console.log("saveSettings() ...");
  let changes = false;

  //--- has anything changed?
  var page = document.getElementById("settingsPage");
  var inputs = page.getElementsByTagName("input");
  //var mRow = document.getElementById("mainPage").getElementsByTagName('div');
  for (var i = 0; i < inputs.length; i++) {
    //do something to each div like
    var field = inputs[i].getAttribute("id");
    console.log("InputNr[" + i + "], InputId[" + field + "]");
    const fieldEl = document.getElementById(field);
    if (!fieldEl) continue;
    if (inputs[i].type == "checkbox") {
      value = fieldEl.checked;
    } else {
      value = fieldEl.value;
    }
    console.log("==> name[" + field + "], value[" + value + "]");

    if (fieldEl.className == "input-changed") {
      //then it was changes, and needs to be saved
      fieldEl.className = "input-normal";
      console.log("Changes where made in [" + field + "][" + value + "]");
      
      // Update theme immediately if darktheme setting changed
      if (field === "darktheme") {
        let isDark = fieldEl.checked;
        document.getElementById('theme-style').href = isDark ? "index_dark.css" : "index.css";
        localStorage.setItem('theme', isDark ? 'dark' : 'light');
      }

      //processWithTimeout([(data.length -1), 0], 2, data, sendPostReading);
      const msgEl = document.getElementById("settingMessage");
      if (msgEl) msgEl.textContent = "Saving changes...";
      sendPostSetting(field, value);
    }
  }
} // saveSettings()


//============================================================================  
function sendPostSetting(field, value) {
  const jsonString = { "name": field, "value": value };
  console.log("sending: " + JSON.stringify(jsonString));
  const other_params = {
    headers: { "content-type": "application/json; charset=UTF-8" },
    body: JSON.stringify(jsonString),
    method: "POST",
    mode: "cors"
  };

  fetch(APIGW + "v0/settings", other_params)
    .then((response) => {
      //console.log(response.status );    //=> number 100–599
      //console.log(response.statusText); //=> String
      //console.log(response.headers);    //=> Headers
      //console.log(response.url);        //=> String
      //console.log(response.text());
      //return response.text()
      const msgEl = document.getElementById("settingMessage");
      if (response.ok) {
        if (msgEl) msgEl.textContent = "Saving changes... SUCCESS";
        setTimeout(function () {
          const msgEl = document.getElementById("settingMessage");
          if (msgEl) msgEl.textContent = "";
        }, 2000); //and clear the message
      } else {
        if (msgEl) msgEl.textContent = "Saving changes... FAILED";
      }
    }, (error) => {
      console.log("Error[" + error.message + "]"); //=> String
      return false;
    });
} // sendPostSetting()


//============================================================================  
function translateToHuman(longName) {
  //for(var index = 0; index < (translateFields.length -1); index++) 
  for (var index = 0; index < translateFields.length; index++) {
    if (translateFields[index][0] == longName) {
      return translateFields[index][1];
    }
  };
  return longName;

} // translateToHuman()



//============================================================================  
function setBackGround(field, newColor) {
  //console.log("setBackground("+field+", "+newColor+")");
  const element = document.getElementById(field);
  if (element) {
    element.dataset.customBg = newColor;
    element.style.background = newColor; // Keep for now if used functionally
  }
} // setBackGround()


//============================================================================  
function getBackGround(field) {
  //console.log("getBackground("+field+")");
  const element = document.getElementById(field);
  return element ? (element.dataset.customBg || element.style.background) : '';
} // getBackGround()


//============================================================================  
function round(value, precision) {
  var multiplier = Math.pow(10, precision || 0);
  return Math.round(value * multiplier) / multiplier;
}


//============================================================================  
function printAllVals(obj) {
  for (let k in obj) {
    if (typeof obj[k] === "object") {
      printAllVals(obj[k])
    } else {
      // base case, stop recurring
      console.log(obj[k]);
    }
  }
} // printAllVals()
//============================================================================  
function strToBool(s) {
  // will match one and only one of the string 'true','1', or 'on' rerardless
  // of capitalization and regardless off surrounding white-space.
  regex = /^\s*(true|1|on)\s*$/i

  return regex.test(s);
}


var translateFields = [

  ["hostname", "HostName"]
  , ["mqttbroker", "MQTT Broker IP/URL"]
  , ["mqttbrokerport", "MQTT Broker Port"]
  , ["mqttuser", "MQTT User"]
  , ["mqttpasswd", "Password MQTT User"]
  , ["mqtttoptopic", "MQTT Top Topic"]
  , ["mqttuniqueid", "MQTT Uniqueid"]
  , ["influxdbhostname", "InfluxDB hostname"]
  , ["influxdbport", "InfluxDB port (default: 8086)"]
  , ["influxdbdatabasename", "InfluxDB database name"]
  , ["flamestatus", "Flame status"]
  , ["chenable", "Central Heating Enabled"]
  , ["chmodus", "Central Heating Status"]
  , ["ch2enable", "Central Heating 2 Enabled"]
  , ["ch2modus", "Central Heating 2 Status"]
  , ["dhwenable", "Domestic Hot Water Enabled"]
  , ["dhwmode", "Domestic Hot Water Status"]
  , ["diagnosticindicator", "Diagnostic Indicator"]
  , ["faultindicator", "Fault Indicator"]
  , ["outsidetemperature", "Outside Temperature"]
  , ["roomtemperature", "Room Temperature"]
  , ["roomsetpoint", "Room Temperature Setpoint"]
  , ["remoteroomsetpoint", "Remote Room Temperature Setpoint"]
  , ["relmodlvl", "Relative Modulation Level"]
  , ["maxrelmodlvl", "Max. Rel. Modulation Level"]
  , ["chwaterpressure", "Central Heating Water Pressure"]
  , ["boilertemperature", "Boiler Temperature"]
  , ["returnwatertemperature", "Return Water Temperature"]
  , ["controlsetpoint", "Control Setpoint"]
  , ["maxchwatersetpoint", "Max. CH Water Setpoint"]
  , ["dhwtemperature", "Domestic Hotwater Temperature"]
  , ["dhwsetpoint", "Domestic Hotwater Setpoint"]
  , ["oemfaultcode", "OEM Fault Code"]
  , ["oemdiagnosticcode", "OEM Diagnostic Code"]
  , ["coolingmodus", "Cooling Enabled"]
  , ["coolingactive", "Cooling Status"]
  , ["otcactive", "Outside Temp Compensation"]
  , ["servicerequest", "Service Request"]
  , ["lockoutreset", "Lockout Reset"]
  , ["lowwaterpressure", "Low Water Pressure"]
  , ["gasflamefault", "Gas/Flame Fault"]
  , ["airtemp", "Air Temperature"]
  , ["waterovertemperature", "Water Over-Temperature"]
  , ["author", "Developer"]
  , ["fwversion", "Firmware Version"]
  , ["picavailable", "PIC Available"]
  , ["picfwversion", "PIC Firmware Version"]
  , ["picdeviceid", "PIC Device ID"]
  , ["picfwtype", "PIC Firmware Type"]
  , ["compiled", "Compiled on (date/time)"]
  , ["HostName", "Hostname (add .local)"]
  , ["ipaddress", "IP address"]
  , ["macaddress", "MAC address"]
  , ["freeheap", "Free Heap Mem (bytes)"]
  , ["maxfreeblock", "Max. Free Mem (bytes)"]
  , ["chipid", "Unique Chip ID"]
  , ["coreversion", "Arduino Core Version"]
  , ["sdkversion", "Espressif SDK Version"]
  , ["cpufreq", "CPU speed (MHz)"]
  , ["sketchsize", "Sketch Size (bytes)"]
  , ["freesketchspace", "Sketch Free (bytes)"]
  , ["flashchipid", "Flash ID"]
  , ["flashchipsize", "Flash Chip Size (MB)"]
  , ["flashchiprealsize", "Real Flash Chip (MB)"]
  , ["littlefssize", "LittleFS size (MB)"]
  , ["flashchipspeed", "Flash Chip Speed (MHz)"]
  , ["flashchipmode", "Flash Mode"]
  , ["boardtype", "Board Type"]
  , ["ssid", "Wifi Network (SSID)"]
  , ["wifirssi", "Wifi RX Power (dBm)"]
  , ["wifiquality", "Wifi Quality (%)"]
  , ["wifiqualitytldr", "Wifi Quality"]
  , ["lastreset", "Last Reset Reason"]
  , ["mqttconnected", "MQTT Connected"]
  , ["mqttenable", "MQTT Enable"]
  , ["mqtthaprefix", "MQTT Auto Discovery prefix"]
  , ["mqttharebootdetection", "MQTT Home Assistant reboot detection"]
  , ["ntpenable", "NTP Enable"]
  , ["ntptimezone", "NTP Timezone"]
  , ["ntphostname", "NTP hostname"]
  , ["ntpsendtime", "Send time to Thermostat"]
  , ["uptime", "Uptime Since Boot"]
  , ["bootcount", "Nr. Reboots"]
  , ["ledblink", "Heartbeat LED (on/off)"]
  , ["darktheme", "Dark Theme (on/off)"]
  , ["gpiosensorsenabled", "GPIO Sensors Enabled"]
  , ["gpiosensorslegacyformat", "GPIO Sensors Legacy Format"]
  , ["gpiosensorsinterval", "GPIO Publish Interval (sec)"]
  , ["gpiosensorspin", "GPIO pin # (SD3 = GPIO10 => 10)"]
  , ["numberofsensors", "Number of temperature sensors"]
  , ["s0counterenabled", "S0 Counter Enabled"]
  , ["s0counterinterval", "S0 Counter Interval (sec)"]
  , ["s0counterpin", "S0 Counter pin # (D6 = GPIO12 => 12)"]
  , ["s0counterdebouncetime", "S0 Counter debouncetime (mS)"]
  , ["s0counterpulsekw", "S0 pulses per kW"]
  , ["s0powerkw", "S0 actual power (kW)"]
  , ["s0intervalcount", "S0 interval pulses"]
  , ["s0totalcount", "S0 total pulses"]
  , ["mqttotmessage", "MQTT OT msg Enable"]
  , ["otgwcommandenable", "Boot Command Enabled"]
  , ["otgwcommands", "Boot Command"]
  , ["thermostatconnected", "Thermostat Connected"]
  , ["boilerconnected", "Boiler Connected"]
  , ["gatewaymode", "Gateway/Standalone"]
  , ["otgwconnected", "HA Integration"]
  , ["gpiooutputsenabled", "GPIO Output Enabled"]
  , ["gpiooutputspin", "GPIO pin # to switch on/off"]
  , ["gpiooutputstriggerbit", "Bit X (master/slave) to trigger on (0-15)"]
  
];

//============================================================================
function applyTheme() {
  fetch(APIGW + "v0/settings")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      let data = json.settings;
      for (let i in data) {
        if (data[i].name == "darktheme") {
           let isDark = strToBool(data[i].value);
           document.getElementById('theme-style').href = isDark ? "index_dark.css" : "index.css";
           localStorage.setItem('theme', isDark ? 'dark' : 'light');
           if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.setTheme === 'function') {
               OTGraph.setTheme(isDark ? 'dark' : 'light');
           }
        }
      }
    })
    .catch(error => console.log(error));
}

//============================================================================
// PIC Flash Functions
//============================================================================
// let currentFlashFilename = ""; // Moved to top

function toggleInteraction(enabled) {
    if (enabled) {
        document.body.classList.remove('disable-interaction');
    } else {
        document.body.classList.add('disable-interaction');
    }
}

function startFlash(filename) {
    performFlash(filename);
}

// Helper function to determine firmware type and version from filename
function parseFirmwareInfo(filename) {
    let displayType = "Gateway";
    let version = "Unknown";
    
    // Determine type from filename (check in specific order to avoid false positives)
    if (filename) {
        let fname = filename.toLowerCase();
        // Check diagnostic first, then interface, default to gateway
        if (fname.includes("diagnostic") || fname.includes("diag.hex")) {
            displayType = "Diagnostic";
        } else if (fname.includes("interface") || fname.includes("inter.hex")) {
            displayType = "Interface";
        }
        // else remains "Gateway"
    }
    
    // Look up version from available firmware files list
    if (typeof availableFirmwareFiles !== 'undefined' && filename) {
        let f = availableFirmwareFiles.find(x => x.name === filename);
        if (f && f.version) version = f.version;
    }
    
    return { type: displayType, version: version };
}

// Failsafe polling mechanism for flash status (both ESP and PIC)
function startFlashPolling() {
    console.log("Starting flash status polling (every 5s)");
    if (flashPollTimer) {
        clearInterval(flashPollTimer);
    }
    flashPollTimer = setInterval(pollFlashStatus, 5000);
}

function stopFlashPolling() {
    console.log("Stopping flash status polling");
    if (flashPollTimer) {
        clearInterval(flashPollTimer);
        flashPollTimer = null;
    }
}

function pollFlashStatus() {
    // Use unified endpoint that works for both ESP and PIC flash
    fetch(APIGW + 'v1/flashstatus')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            return response.json();
        })
        .then(json => {
            if (!json.flashstatus) return;
            
            const status = json.flashstatus;
            console.log("Flash status poll:", status);
            
            // If not flashing at all, stop polling
            if (!status.flashing) {
                stopFlashPolling();
                return;
            }
            
            let progressBar = document.getElementById("flashProgressBar");
            let pctText = document.getElementById("flashPercentageText");
            
            // Handle PIC flash progress
            if (status.pic_flashing && status.pic_progress >= 0 && status.pic_progress <= 100) {
                if (progressBar) progressBar.style.width = status.pic_progress + "%";
                if (pctText) pctText.textContent = "Flashing " + (status.pic_filename || currentFlashFilename) + " : " + status.pic_progress + "%";
                
                // Check for completion
                if (status.pic_progress === 100) {
                    handleFlashCompletion(status.pic_filename, status.pic_error);
                } else if (status.pic_progress === -1) {
                    handleFlashError(status.pic_filename, status.pic_error);
                }
            }
            // ESP flash doesn't provide detailed progress in this API
            // It relies on WebSocket messages from ModUpdateServer
        })
        .catch(error => {
            console.error("Flash status poll error:", error);
            // Keep polling - might be temporary network issue
        });
}

function handleFlashCompletion(filename, error) {
    stopFlashPolling();
    isFlashing = false;
    toggleInteraction(true);
    
    let progressBar = document.getElementById("flashProgressBar");
    let pctText = document.getElementById("flashPercentageText");
    let progressSection = document.getElementById("flashProgressSection");
    
    if (progressBar) {
        progressBar.style.width = "100%";
        if (progressBar.classList.contains('error')) progressBar.classList.remove('error');
    }
    
    // Parse firmware info from filename
    const fwInfo = parseFirmwareInfo(filename || currentFlashFilename);
    
    // Update UI with TARGET version
    let elType = document.getElementById('pic_type_display');
    let elVer = document.getElementById('pic_version_display');
    if (elType) elType.textContent = fwInfo.type;
    if (elVer) elVer.textContent = fwInfo.version;
    
    if (pctText) pctText.textContent = "Successfully flashed to " + fwInfo.type + " " + fwInfo.version;
    
    // Refresh firmware info
    setTimeout(() => refreshFirmware(), 2000);
    
    // Reset progress bar after 10 seconds
    setTimeout(function() {
        if (progressSection) progressSection.classList.remove('active');
        if (progressBar) progressBar.style.width = "0%";
        if (pctText) pctText.textContent = "Ready to flash";
    }, 10000);
    
    // Restart polling
    if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
    if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
}

function handleFlashError(filename, error) {
    stopFlashPolling();
    isFlashing = false;
    toggleInteraction(true);
    
    let progressBar = document.getElementById("flashProgressBar");
    let pctText = document.getElementById("flashPercentageText");
    
    if (progressBar) progressBar.classList.add('error');
    if (pctText) pctText.textContent = "Flash failed: " + (error || "Unknown error");
    
    // Restart polling
    if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
    if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
}

// function pollForReboot() - Removed
// function startFlashCountdown() - Removed

function performFlash(filename) {
    currentFlashFilename = filename;
    isFlashing = true;
    toggleInteraction(false);
    // Stop polling during upgrade to prevent interference and reduce load
    if (tid) { clearInterval(tid); tid = 0; }
    if (timeupdate) { clearInterval(timeupdate); timeupdate = 0; }

    let progressSection = document.getElementById("flashProgressSection");
    let progressBar = document.getElementById("flashProgressBar");
    let pctText = document.getElementById("flashPercentageText");
    
    if (progressSection) progressSection.classList.add('active');
    if (progressBar) {
        progressBar.style.width = "0%";
        progressBar.classList.remove('error');
    }
    
    if (pctText) pctText.textContent = "Connecting to event stream...";
    
    // Ensure WebSocket is connected for progress updates
    initOTLogWebSocket(true);
    
    // Start failsafe polling every 5 seconds
    startFlashPolling();

    // Wait for WebSocket to be OPEN before sending flash command
    let attempts = 0;
    const waitForWS = setInterval(() => {
        attempts++;
        // ReadyState 1 is OPEN
        if ((otLogWS && otLogWS.readyState === 1) || attempts > 50) { // 5s timeout
             clearInterval(waitForWS);

             if (!otLogWS || otLogWS.readyState !== 1) {
                console.error("Flash aborted: WebSocket timeout");
                if (pctText) pctText.textContent = "Error: Connection timed out. Cannot track progress.";
                if (progressBar) progressBar.classList.add('error');
                isFlashing = false;
                toggleInteraction(true);
                // Restart polling
                if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
                if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
                return;
             }

             if (pctText) pctText.textContent = "Starting upgrade for " + filename + "...";
             
             fetch(localURL + '/pic?action=upgrade&name=' + filename)
                .then(response => {
                   if (!response.ok) {
                      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                   }
                   const contentType = response.headers.get("content-type");
                   if (contentType && contentType.indexOf("application/json") !== -1) {
                     return response.json();
                   } else {
                     return {status: "started (legacy)"}; 
                   }
                })
                .then(data => {
                    console.log("Flash started:", data);
                    if (pctText) pctText.textContent = "Flashing " + filename + " started...";
                })
                .catch(error => {
                    console.error("Flash error:", error);
                    isFlashing = false;
                    toggleInteraction(true);
                    if (pctText) pctText.textContent = "Error starting flash: " + error.message;
                    if (progressBar) progressBar.classList.add('error');
                    
                    // Stop failsafe polling
                    stopFlashPolling();
                    
                    // Restart polling on start failure
                    if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
                    if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
                });
        }
    }, 100);
}

function handleFlashMessage(data) {
    try {
        if (!data || !data.startsWith('{')) return false; // Not JSON
        
        // Try parsing as JSON
        let msg = JSON.parse(data);
        
        // Check if it looks like a flash message with 'state' property (matches OTGW-ModUpdateServer-impl.h format)
        if (msg.hasOwnProperty('state')) {
            let progressBar = document.getElementById("flashProgressBar");
            let pctText = document.getElementById("flashPercentageText");
            let progressSection = document.getElementById("flashProgressSection");
            
            if (progressSection && !progressSection.classList.contains('active')) {
                 progressSection.classList.add('active');
            }
            
            // Calculate percentage from flash_written and flash_total
            if (msg.flash_total > 0 && msg.flash_written != null) {
                let percent = Math.round((msg.flash_written * 100) / msg.flash_total);
                if (progressBar) progressBar.style.width = percent + "%";
                
                // Update text based on state
                if (msg.state === 'write' || msg.state === 'start') {
                    if (pctText) pctText.textContent = "Flashing " + (msg.filename || currentFlashFilename) + " : " + percent + "%";
                }
            }
            
            // Handle completion states
            if (msg.state === 'end') {
                // Success
                stopFlashPolling(); // Stop failsafe polling
                isFlashing = false;
                toggleInteraction(true);
                
                if (progressBar) {
                    progressBar.style.width = "100%";
                    if (progressBar.classList.contains('error')) progressBar.classList.remove('error');
                }

                // Parse firmware info from filename
                const fwInfo = parseFirmwareInfo(msg.filename || currentFlashFilename);

                // Update UI immediately with TARGET version (optimistic)
                let elType = document.getElementById('pic_type_display');
                let elVer = document.getElementById('pic_version_display');
                if (elType) elType.textContent = fwInfo.type;
                if (elVer) elVer.textContent = fwInfo.version;
                
                if (pctText) pctText.textContent = "Successfully flashed to " + fwInfo.type + " " + fwInfo.version;
                
                // Trigger actual hardware refresh in background
                setTimeout(() => refreshFirmware(), 2000); // Give PIC 2s to boot

                // Reset progress bar after 10 seconds
                setTimeout(function() {
                    if (progressSection) progressSection.classList.remove('active');
                    if (progressBar) progressBar.style.width = "0%";
                    if (pctText) pctText.textContent = "Ready to flash";
                }, 10000);
                
                // Restart polling
                if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
                if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
            } else if (msg.state === 'error' || msg.state === 'abort') {
                // Error or abort
                stopFlashPolling(); // Stop failsafe polling
                isFlashing = false;
                toggleInteraction(true);
                
                let errorMsg = msg.error || "Flash failed";
                if (pctText) pctText.textContent = "Finished " + (msg.filename || currentFlashFilename) + ": " + errorMsg;
                
                if (progressBar) progressBar.classList.add('error');
                
                // Restart polling
                if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
                if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
            }
            
            return true; // It was a flash message
        }
    } catch (e) {
        // Not JSON or error parsing
        return false;
    }
    return false;
}

/*
***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************
*/

/*
***************************************************************************
** Statistics Tab Functions
***************************************************************************
*/
var statsBuffer = {};
var statsSortCol = 1; // Default sort by Dec ID
var statsSortAsc = true;
var currentTab = 'Log';

// OTmonitor-6.6 compatibility helpers
// OTmonitor derives message uniqueness from (type & 7, msgid) and only shows
// types {1,4,6,7} as {Write,Read,Invalid,Unk}.
function otmGetTypeNibbleChar(raw) {
  if (typeof raw !== 'string' || !raw) return null;
  const s = raw.trim();
  if (!s) return null;
  
  // OTGW log frames are typically formatted as "<SRC><8-hex>", e.g. "B40000000".
  // Standard OT messages are 32-bit (8 hex chars). If length is 9, assume first char is Source prefix.
  // This handles cases where Source is 'B' (Boiler) or 'A' (Answer) which are also valid hex chars.
  if (s.length === 9 && /[0-9A-Fa-f]/.test(s.charAt(1))) {
    return s.charAt(1);
  }

  // In that case, the message type nibble is the first hex char after the SRC letter.
  if (s.length >= 2 && !/[0-9A-Fa-f]/.test(s.charAt(0)) && /[0-9A-Fa-f]/.test(s.charAt(1))) {
    return s.charAt(1);
  }
  // If there's no SRC prefix (or it's already hex), the first char should be the nibble.
  if (/[0-9A-Fa-f]/.test(s.charAt(0))) {
    return s.charAt(0);
  }
  return null;
}

function otmGetTypeFromRaw(raw) {
  const nibbleChar = otmGetTypeNibbleChar(raw);
  if (!nibbleChar) return null;
  const nibble = parseInt(nibbleChar, 16);
  if (isNaN(nibble)) return null;
  return (nibble & 7);
}

function otmTypeFromDirString(dir) {
  if (typeof dir !== 'string' || !dir) return null;
  switch (dir) {
    case 'Read-Data': return 0;
    case 'Write-Data': return 1;
    case 'Inv-Data': return 2;
    case 'Reserved': return 3;
    case 'Read-Ack': return 4;
    case 'Write-Ack': return 5;
    case 'Data-Inv': return 6;
    case 'Unk-DataId': return 7;
    default: return null;
  }
}

function otmDirectionLabel(typeCode, fallbackDir) {
  // OTmonitor's TV trace labels only these types.
  // For our UI Statistics table, keep those labels where applicable, but do
  // not drop other types (otherwise the table can become empty when the stream
  // contains mostly requests, e.g. Read-Data type 0).
  switch (typeCode) {
    case 4: return 'Read';
    case 1: return 'Write';
    case 6: return 'Invalid';
    case 7: return 'Unk';
    default:
      if (typeof fallbackDir === 'string' && fallbackDir) return fallbackDir;
      if (typeCode === null || typeCode === undefined) return '';
      return String(typeCode);
  }
}

function openLogTab(evt, tabName) {
  var i, tabcontent, tablinks;
  tabcontent = document.getElementsByClassName('tab-content');
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].classList.remove('active');
  }
  tablinks = document.getElementsByClassName('tab-link');
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].classList.remove('active');
  }
  document.getElementById(tabName).classList.add('active');
  evt.currentTarget.classList.add('active');
  currentTab = tabName;
  if (currentTab === 'Statistics') {
      updateStatisticsDisplay();
  } else if (currentTab === 'Graph' && typeof OTGraph !== 'undefined') {
      // Ensure the chart resizes when the tab becomes visible
      if (OTGraph.resize) OTGraph.resize();
  }
}

function processStatsLine(line) {
  // Statistics works purely off JSON objects.
    if (!line || typeof line !== 'object') return;

    if (line.id === undefined || line.id === null) return;
    const id = parseInt(line.id, 10);
    if (isNaN(id)) return;

    // OTmonitor uniqueness key: (type,msgid)
    // Prefer deriving type from raw message (first hex nibble after OTGW SRC prefix),
    // fallback to dir string.
    const raw = (typeof line.raw === 'string' && line.raw) ? line.raw : '';
    let typeCode = otmGetTypeFromRaw(raw);
    if (typeCode === null) {
      const dirStr = (typeof line.dir === 'string' && line.dir) ? line.dir : '';
      typeCode = otmTypeFromDirString(dirStr);
    }
    
    // OTmonitor filter logic: Only these types carry unique data points to track statistics.
    // 0 (Read-Data) is request. 5 (Write-Ack) is ack.
    // Exception: ID 0 (Status) carries Master Status in Type 0 (Read-Data) and Slave Status in Type 4 (Read-Ack).
    // Both are meaningful unique messages for ID 0.
    const isStatusMsg = (id === 0);
    const isValidType = [1, 4, 6, 7].includes(typeCode) || (isStatusMsg && typeCode === 0);
    
    if (typeCode === null || !isValidType) return;

    const dirStr = (typeof line.dir === 'string' && line.dir) ? line.dir : '';
    let dirLabel = otmDirectionLabel(typeCode, dirStr);

    // If we allowed Type 0 for Status, ensure it has a label (typically it falls back to '0' or dirStr)
    // Map Type 0 to 'Read' to match OTmonitor style, or 'Read-Data' if preferred. 
    // OTmonitor maps Type 4 to 'Read'. Type 1 to 'Write'. 
    if (typeCode === 0 && (!dirLabel || dirLabel === '0')) dirLabel = 'Read';

    if (!dirLabel) return;

    let label = (typeof line.label === 'string' && line.label.trim() !== '') ? line.label : 'Unknown';
    let value = '';
    if (line.value !== undefined && line.value !== null) {
      value = String(line.value);
    } else if (line.val !== undefined && line.val !== null) {
      value = String(line.val);
    }

    const now = Date.now();
    const key = typeCode + ',' + id;
    
    if (!statsBuffer[key]) {
        statsBuffer[key] = {
            id: id,
      hex: id.toString(16).toUpperCase().padStart(2, '0'),
      typeCode: typeCode,
      type: dirLabel,
        dir: dirLabel,
            label: label,
            value: value,
            count: 0,
            lastTime: now, 
            intervalSum: 0,
            intervalCount: 0
        };
    } else {
        const entry = statsBuffer[key];
        const diff = (now - entry.lastTime) / 1000.0; // seconds
        
        // Only accumulate interval if this is not the first message (entry.count > 0)
        // This prevents counting the buffer-creation-to-first-message interval
        // Also validate diff is reasonable (> 0 and < 1 hour) to handle clock skew
        if (entry.count > 0 && diff > 0 && diff < 3600) {
            entry.intervalSum += diff;
            entry.intervalCount++;
        }
        
        entry.lastTime = now;
        entry.value = value;
        entry.type = dirLabel;
        if (label && label !== 'Unknown') entry.label = label;
    }
    statsBuffer[key].count++;
    
    // If stats tab is active, schedule update
    if (currentTab === 'Statistics') {
         scheduleStatsUpdate();
    }
}

var statsUpdatePending = false;
function scheduleStatsUpdate() {
    if (!statsUpdatePending) {
        statsUpdatePending = true;
        requestAnimationFrame(function() {
            statsUpdatePending = false;
            updateStatisticsDisplay();
        });
    }
}

function updateStatisticsDisplay() {
    if (currentTab !== 'Statistics') return;
    
    var tbody = document.querySelector('#otStatsTable tbody');
    if (!tbody) return;

    var rows = Object.values(statsBuffer);
    
    // Sort
    rows.sort(function(a, b) {
        var valA, valB;
        switch(statsSortCol) {
            case 0: valA = parseInt(a.hex, 16); valB = parseInt(b.hex, 16); break;
            case 1: valA = a.id; valB = b.id; break;
            case 2: valA = a.dir || ''; valB = b.dir || ''; break; // Sort by Direction
            case 3: valA = a.label; valB = b.label; break;
            case 4: valA = (a.intervalCount > 0 ? a.intervalSum / a.intervalCount : 0); 
                    valB = (b.intervalCount > 0 ? b.intervalSum / b.intervalCount : 0); break;
            case 5: valA = a.value; valB = b.value; break;
            default: valA = a.id; valB = b.id;
        }
        
        if (typeof valA === 'string') valA = valA.toLowerCase();
        if (typeof valB === 'string') valB = valB.toLowerCase();
        
        if (valA < valB) return statsSortAsc ? -1 : 1;
        if (valA > valB) return statsSortAsc ? 1 : -1;
        return 0;
    });

    var html = '';
    rows.forEach(function(r) {
        var avgInterval = (r.intervalCount > 0) ? (r.intervalSum / r.intervalCount).toFixed(1) : '-';
        
        // Direction is now stored in r.dir
        var dir = r.dir || 'Unk';

        html += '<tr>';
        html += '<td>' + r.hex + '</td>';
        html += '<td>' + r.id + '</td>';
        html += '<td>' + dir + '</td>';
        html += '<td>' + r.label + '</td>';
        html += '<td>' + avgInterval + '</td>';
        html += '<td>' + escapeHtml(r.value) + '</td>';
        html += '</tr>';
    });
    
    tbody.innerHTML = html;
    
    var countEl = document.getElementById('statsCount');
    if (countEl) countEl.textContent = rows.length;
}

function sortStats(col) {
    if (statsSortCol === col) {
        statsSortAsc = !statsSortAsc;
    } else {
        statsSortCol = col;
        statsSortAsc = true;
    }
    updateStatisticsDisplay();
}

//============================================================================
function saveUISetting(field, value) {
  // UI element settings should be hidden settings, marked by "#" in the API call
  // Only checkboxes and dropdowns trigger this
  const apiField = field.startsWith('#') ? field : '#' + field;
  sendPostSetting(apiField, value);
}
window.saveUISetting = saveUISetting;

function setupPersistentUIListeners() {
  // Graph Tab - Auto Screenshot
  const chkAutoScreenshot = document.getElementById('chkAutoScreenshot');
  if (chkAutoScreenshot) {
    chkAutoScreenshot.addEventListener('change', function(e) {
      saveUISetting('ui_autoscreenshot', e.target.checked);
    });
  }

  // Graph Tab - Auto Export
  const chkAutoExport = document.getElementById('chkAutoExport');
  if (chkAutoExport) {
    chkAutoExport.addEventListener('change', function(e) {
      saveUISetting('ui_autoexport', e.target.checked);
    });
  }
  
  // Graph Tab - Time Window
  const graphTimeWindow = document.getElementById('graphTimeWindow');
  if (graphTimeWindow) {
    graphTimeWindow.addEventListener('change', function(e) {
      saveUISetting('ui_graphtimewindow', e.target.value);
    });
  }
}

function loadPersistentUI() {
  console.log("Loading persistent UI settings...");
  const apiPath = (typeof APIGW !== 'undefined') ? APIGW : (window.location.protocol + '//' + window.location.host + '/api/');
  
  fetch(apiPath + "v1/settings")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      if (!json || !json.settings) return;
      const settings = json.settings;
      const getVal = (name) => {
        const s = settings.find(s => s.name === name);
        return s ? s.value : null;
      };

      // Auto Scroll
      const autoScrollVal = getVal("ui_autoscroll");
      if (autoScrollVal !== null) {
         const newVal = (autoScrollVal === true || autoScrollVal === "true");
         if (typeof autoScroll !== 'undefined') autoScroll = newVal;
         const chk = document.getElementById('chkAutoScroll');
         if (chk) chk.checked = newVal;
      }

      // Timestamps
      const tsVal = getVal("ui_timestamps");
      if (tsVal !== null) {
          const chk = document.getElementById("chkShowTimestamp");
          if (chk) {
             chk.checked = (tsVal === true || tsVal === "true");
             if (typeof showTimestamps !== 'undefined') showTimestamps = chk.checked;
             if (typeof updateLogDisplay === 'function') updateLogDisplay();
          }
      }

      // Capture
      const capVal = getVal("ui_capture");
      if (capVal !== null) {
          const chk = document.getElementById("chkCaptureMode");
          if (chk) {
              chk.checked = (capVal === true || capVal === "true");
              captureMode = chk.checked;
              if (typeof updateDynamicLimits === 'function') updateDynamicLimits();
          }
      }

      // Auto Download Log
      const dlVal = getVal("ui_autodownloadlog");
      if (dlVal !== null) {
          const chk = document.getElementById("chkAutoDownloadLog");
          if (chk) {
              chk.checked = (dlVal === true || dlVal === "true");
              if (typeof toggleAutoDownloadLog === 'function') toggleAutoDownloadLog(chk.checked);
          }
      }

      // Auto Export (Graph Tab)
      const exportVal = getVal("ui_autoexport");
      if (exportVal !== null) {
          const chk = document.getElementById("chkAutoExport");
          if (chk) {
              chk.checked = (exportVal === true || exportVal === "true");
              if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.toggleAutoExport === 'function') {
                  OTGraph.toggleAutoExport(chk.checked);
              }
          }
      }

      // Auto Screenshot
      const shotVal = getVal("ui_autoscreenshot");
      if (shotVal !== null) {
          const chk = document.getElementById("chkAutoScreenshot");
          if (chk) {
             chk.checked = (shotVal === true || shotVal === "true");
             if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.toggleAutoScreenshot === 'function') {
                 OTGraph.toggleAutoScreenshot(chk.checked);
             }
          }
      }

      // Time Window
      const timeVal = getVal("ui_graphtimewindow");
      if (timeVal !== null && timeVal > 0) {
          const sel = document.getElementById("graphTimeWindow");
          if (sel) {
              sel.value = timeVal;
               if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.setTimeWindow === 'function') {
                  OTGraph.setTimeWindow(parseInt(timeVal));
              }
          }
      }

    })
    .catch(err => console.error("Error loading persistent settings:", err));
}

