/*
***************************************************************************  
**  Program  : index.js, part of OTGW-firmware project
**  Version  : v1.0.0-rc4
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
const localURL = window.location.protocol + '//' + window.location.host;
const APIGW = window.location.protocol + '//' + window.location.host + '/api/';

"use strict";

let needReload = false;

console.log("Hash=" + window.location.hash);
window.onload = initMainPage;

window.onblur = function() {
  needReload = true;
};

window.onfocus = function () {
  if (needReload) {
    needReload = false;
    window.location.reload(true);
  }
};


var tid = 0;
var timeupdate = null; // Will be started when needed

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
    timeupdate = setInterval(function () { refreshDevTime(); }, 1000);
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
// OpenTherm Log WebSocket Variables and Functions
//============================================================================
let otLogWS = null;
let otLogBuffer = [];
let otLogFilteredBuffer = [];
const MAX_LOG_LINES_DEFAULT = 2000;
const MAX_LOG_LINES_CAPTURE = 1000000; 
const RENDER_LIMIT = 2000;
let maxLogLines = MAX_LOG_LINES_DEFAULT; 
let autoScroll = true;
let showTimestamps = true;
let logExpanded = false;
let searchTerm = '';
let updatePending = false;
let otLogControlsInitialized = false;
let isFlashing = false;
let currentFlashFilename = "";
let flashModeActive = false; // Track if we're on the flash page

// File Streaming Variables
let logDirectoryHandle = null;
let fileStreamHandle = null;
let fileWritableStream = null; // Deprecated, kept for cleanup just in case
let logWriteQueue = [];
let isLogWriting = false;
let isStreamingToFile = false;
let streamBytesWritten = 0;
let fileRotationTimer = null;
let currentLogDateStr = "";

//============================================================================
// Memory Monitoring and Storage Management
//============================================================================
// 500MB limit chosen as safe threshold for most browsers:
// - Allows ~2.5M log lines (avg 200 bytes each)
// - Leaves headroom for other page resources
// - Well below typical browser memory limits (2-4GB)
const MEMORY_SAFE_LIMIT = 500 * 1024 * 1024; // 500MB safe limit
const MEMORY_WARNING_THRESHOLD = 0.8; // 80% - warn user
const MEMORY_CRITICAL_THRESHOLD = 0.95; // 95% - auto-trim to prevent crash
let memoryMonitorTimer = null;
let currentMemoryStatus = 'green'; // green, yellow, red

// Storage Mode
let storageMode = 'smart'; // 'disabled', 'smart', 'indexeddb', 'localstorage'
let logDatabase = null;
let currentSessionId = null;
let autoSaveTimer = null;
const AUTO_SAVE_INTERVAL = 10000; // 10 seconds
let storageSystemInitialized = false; // Guard against multiple initializations

// Storage configuration
const STORAGE_CONFIG = {
  retentionDays: 7,
  maxIndexedDBSize: 1024 * 1024 * 1024, // 1GB
  maxLocalStorageLines: 1000,
  autoSaveEnabled: true
};

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
const WS_WATCHDOG_TIMEOUT = 10000; // 10 seconds timeout for silence

//============================================================================
function resetWSWatchdog() {
  if (wsWatchdogTimer) clearTimeout(wsWatchdogTimer);
  wsWatchdogTimer = setTimeout(function() {
      console.warn("WS Watchdog expired. No data received for " + (WS_WATCHDOG_TIMEOUT/1000) + "s. Forcing reconnect...");
      // Closing the socket will trigger onclose, which triggers the reconnect logic
      if (otLogWS) {
        otLogWS.close();
      } else {
        // If socket is somehow null but watchdog fired, force a new connection
        // We reuse the last known 'force' state implicitly by calling without args (undefined is falsy)
        // or we could store the last 'force' state in a global if critical.
        // For now, attempting standard connect is safe.
        initOTLogWebSocket(false);
      }
  }, WS_WATCHDOG_TIMEOUT);
}

//============================================================================
function initOTLogWebSocket(force) {
  // Don't connect if in flash mode
  if (flashModeActive) {
    console.log('Flash mode active - skipping WebSocket connection');
    return;
  }
  
  // Detect smartphone (iPhone or Android Phone)
  const isPhone = /iPhone|iPod/.test(navigator.userAgent) || 
                 (/Android/.test(navigator.userAgent) && /Mobile/.test(navigator.userAgent));
  
  // Also check screen width as a fallback (standard breakpoint for tablets is 768px)
  const isSmallScreen = window.innerWidth < 768;

  if ((isPhone || isSmallScreen) && !force && !isFlashing) {
    console.log("Smartphone or small screen detected. Disabling OpenTherm Monitor.");
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
  
  // Close existing connection if it exists
  if (otLogWS) {
    // Remove listeners to avoid double-triggers during manual cleanup
    otLogWS.onclose = null; 
    otLogWS.onerror = null;
    if (otLogWS.readyState === WebSocket.OPEN || otLogWS.readyState === WebSocket.CONNECTING) {
      otLogWS.close();
    }
    otLogWS = null;
  }

  console.log('Connecting to WebSocket: ' + wsURL);
  
  try {
    otLogWS = new WebSocket(wsURL);
    
    otLogWS.onopen = function() {
      console.log('OT Log WebSocket connected');
      updateWSStatus(true);
      // Clear any reconnect timer just in case
      if (wsReconnectTimer) {
        clearTimeout(wsReconnectTimer);
        wsReconnectTimer = null;
      }
      resetWSWatchdog();
    };
    
    otLogWS.onclose = function() {
      console.log('OT Log WebSocket disconnected');
      updateWSStatus(false);
      // Stop watchdog
      if (wsWatchdogTimer) {
        clearTimeout(wsWatchdogTimer);
        wsWatchdogTimer = null;
      }
      // Attempt to reconnect after 5 seconds if not already scheduled
      if (!wsReconnectTimer) {
        let delay = isFlashing ? 1000 : 5000;
        wsReconnectTimer = setTimeout(function() { initOTLogWebSocket(force); }, delay);
      }
    };
    
    otLogWS.onerror = function(error) {
      console.error('OT Log WebSocket error:', error);
      updateWSStatus(false);
      // onclose will usually follow, but we ensure cleanup
      if (otLogWS) otLogWS.close(); 
    };
    
    otLogWS.onmessage = function(event) {
      resetWSWatchdog();

      // Always log the raw incoming message
      console.log("OT Log WS received:", event.data);

      if (typeof handleFlashMessage === "function") {
        if (handleFlashMessage(event.data)) return;
      }
      
      let data = event.data;
      let isObject = false;

      try {
        if (data && typeof data === 'string' && data.startsWith('{')) {
          data = JSON.parse(data);
          isObject = true;
          console.log("OT Log WS parsed:", data);
        }
      } catch(e) {
        // ignore JSON parse error, treat as text
      }

      if (!isObject && typeof data === 'string') {
        const parsed = parseLogLine(data);
        if (parsed) data = parsed;
      }

      addLogLine(data);
    };
    
  } catch (e) {
    console.error('Failed to create WebSocket:', e);
    updateWSStatus(false);
    if (!wsReconnectTimer) {
      let delay = isFlashing ? 1000 : 5000;
      wsReconnectTimer = setTimeout(function() { initOTLogWebSocket(force); }, delay);
    }
  }
  
  // Setup UI event handlers only once
  if (!otLogControlsInitialized) {
    setupOTLogControls();
    otLogControlsInitialized = true;
  }
}

//============================================================================
function disconnectOTLogWebSocket() {
  // Clear any pending reconnect timer
  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer);
    wsReconnectTimer = null;
  }
  // Clear watchdog
  if (wsWatchdogTimer) {
     clearTimeout(wsWatchdogTimer);
     wsWatchdogTimer = null;
  }

  if (otLogWS) {
    console.log('Disconnecting OT Log WebSocket');
    // Remove event listeners to prevent auto-reconnect
    otLogWS.onclose = null;
    otLogWS.onerror = null;
    otLogWS.onmessage = null;
    
    if (otLogWS.readyState === WebSocket.OPEN || otLogWS.readyState === WebSocket.CONNECTING) {
      otLogWS.close();
    }
    otLogWS = null;
  }
  updateWSStatus(false);
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
  
  // Detect timestamp: HH:MM:SS.mmmmmm (15 chars)
  // Regex must be robust.
  const tsMatch = line.match(/^(\d{2}:\d{2}:\d{2}\.\d{6})\s/);
  if (tsMatch) {
      obj.time = tsMatch[1];
      offset = 16; // 15 chars + 1 space
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
  if (typeof OTGraph !== 'undefined') {
      OTGraph.processLine(logLine);
  }

  // Trim buffer if exceeds max
  if (otLogBuffer.length > maxLogLines) {
    otLogBuffer.shift();
  }
  
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

  const displayCount = logExpanded ? Math.min(otLogFilteredBuffer.length, RENDER_LIMIT) : Math.min(10, otLogFilteredBuffer.length);
  const startIndex = Math.max(0, otLogFilteredBuffer.length - displayCount);
  const linesToShow = otLogFilteredBuffer.slice(startIndex);

  if (logExpanded && otLogFilteredBuffer.length > RENDER_LIMIT && startIndex === otLogFilteredBuffer.length - RENDER_LIMIT) {
     // Optional: Add indicator that logs are truncated in view
     // We can just rely on the counters in the footer.
  }

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
  
  if (memUsageEl) {
    const memInfo = getMemoryUsage();
    const mb = memInfo.bufferSize / (1024 * 1024);
    memUsageEl.textContent = mb.toFixed(2);
  }
  
  // Update detailed memory display if exists
  updateMemoryDisplay();
}

//============================================================================
// Enhanced Memory Monitoring Functions
//============================================================================

/**
 * Get actual memory usage with browser API support detection
 */
function getMemoryUsage() {
  let result = {
    bufferSize: 0,
    heapUsed: 0,
    heapTotal: 0,
    heapLimit: 0,
    supported: false
  };
  
  // Calculate actual buffer size
  try {
    result.bufferSize = JSON.stringify(otLogBuffer).length;
  } catch (e) {
    // Fallback to estimation if stringify fails
    result.bufferSize = otLogBuffer.length * 200;
  }
  
  // Use performance.memory API if available (Chrome/Edge only, non-standard)
  if (typeof performance !== 'undefined' && performance.memory) {
    result.heapUsed = performance.memory.usedJSHeapSize;
    result.heapTotal = performance.memory.totalJSHeapSize;
    result.heapLimit = performance.memory.jsHeapSizeLimit;
    result.supported = true;
  } else {
    // Fallback estimation for other browsers (Firefox, Safari)
    result.heapUsed = result.bufferSize;
    result.heapTotal = result.bufferSize * 2;
    result.heapLimit = MEMORY_SAFE_LIMIT;
    result.supported = false;
  }
  
  return result;
}

/**
 * Update memory display in the UI
 */
function updateMemoryDisplay() {
  const memDetailedEl = document.getElementById('memUsageDetailed');
  if (!memDetailedEl) return;
  
  const mem = getMemoryUsage();
  const bufferMB = mem.bufferSize / (1024 * 1024);
  const heapMB = mem.heapUsed / (1024 * 1024);
  const limitMB = mem.heapLimit / (1024 * 1024);
  const percent = (mem.heapUsed / mem.heapLimit) * 100;
  
  // Determine status indicator
  let indicator = '🟢';
  let status = 'green';
  if (percent >= MEMORY_CRITICAL_THRESHOLD * 100) {
    indicator = '🔴';
    status = 'red';
  } else if (percent >= MEMORY_WARNING_THRESHOLD * 100) {
    indicator = '🟡';
    status = 'yellow';
  }
  
  // Update status if changed
  if (status !== currentMemoryStatus) {
    currentMemoryStatus = status;
    handleMemoryStatusChange(status, percent);
  }
  
  // Build display text
  if (mem.supported) {
    memDetailedEl.innerHTML = `Buffer: ${bufferMB.toFixed(1)} MB | ` +
                               `Heap: ${heapMB.toFixed(1)} / ${limitMB.toFixed(0)} MB ` +
                               `(${percent.toFixed(1)}%) ${indicator}`;
  } else {
    memDetailedEl.innerHTML = `Buffer: ${bufferMB.toFixed(1)} MB ${indicator}`;
  }
  
  // Update CSS class for styling
  memDetailedEl.className = 'mem-status mem-' + status;
}

/**
 * Handle memory status changes (warnings, auto-trim)
 */
function handleMemoryStatusChange(status, percent) {
  console.log(`Memory status changed to ${status} (${percent.toFixed(1)}%)`);
  
  if (status === 'yellow') {
    // Warning: approaching limit
    console.warn('Memory usage is high. Consider enabling capture mode or file streaming.');
    showMemoryWarning('warning');
  } else if (status === 'red') {
    // Critical: auto-trim to prevent crash
    console.error('Memory usage critical! Auto-trimming buffer...');
    showMemoryWarning('critical');
    
    if (!isStreamingToFile) {
      // Remove oldest 25% of buffer
      const toRemove = Math.floor(otLogBuffer.length * 0.25);
      otLogBuffer.splice(0, toRemove);
      updateFilteredBuffer();
      updateLogDisplay();
      updateLogCounters();
      console.warn(`Removed ${toRemove} oldest log lines to prevent crash`);
    }
  }
}

/**
 * Show memory warning banner
 */
function showMemoryWarning(level) {
  const bannerId = 'memoryWarningBanner';
  let banner = document.getElementById(bannerId);
  
  if (!banner) {
    banner = document.createElement('div');
    banner.id = bannerId;
    banner.className = 'warning-banner';
    
    const logSection = document.getElementById('otLogSection');
    if (logSection) {
      logSection.insertBefore(banner, logSection.firstChild);
    }
  }
  
  if (level === 'warning') {
    banner.className = 'warning-banner warning';
    banner.innerHTML = `
      ⚠️ Memory usage is high. 
      <button onclick="enableCaptureMode()" class="btn-inline">Enable Capture Mode</button> or 
      <button onclick="startFileStreamingPrompt()" class="btn-inline">Stream to File</button>
      <button onclick="dismissMemoryWarning()" class="btn-close">✖</button>
    `;
  } else if (level === 'critical') {
    banner.className = 'warning-banner critical';
    banner.innerHTML = `
      🔴 Memory critical! Oldest logs were removed to prevent crash. 
      <button onclick="startFileStreamingPrompt()" class="btn-inline">Enable File Streaming</button>
      <button onclick="dismissMemoryWarning()" class="btn-close">✖</button>
    `;
  }
  
  banner.style.display = 'block';
}

/**
 * Dismiss memory warning banner
 */
function dismissMemoryWarning() {
  const banner = document.getElementById('memoryWarningBanner');
  if (banner) {
    banner.style.display = 'none';
  }
}

/**
 * Enable capture mode programmatically
 */
function enableCaptureMode() {
  const chk = document.getElementById('chkCaptureMode');
  if (chk && !chk.checked) {
    chk.checked = true;
    chk.dispatchEvent(new Event('change'));
  }
  dismissMemoryWarning();
}

/**
 * Prompt to start file streaming
 */
function startFileStreamingPrompt() {
  const chk = document.getElementById('chkStreamToFile');
  if (chk && !chk.checked) {
    chk.checked = true;
    chk.dispatchEvent(new Event('change'));
  }
  dismissMemoryWarning();
}

/**
 * Start memory monitoring
 */
function startMemoryMonitoring() {
  if (memoryMonitorTimer) {
    clearInterval(memoryMonitorTimer);
  }
  
  // Update every 5 seconds
  memoryMonitorTimer = setInterval(() => {
    updateMemoryDisplay();
  }, 5000);
  
  // Initial update
  updateMemoryDisplay();
}

/**
 * Stop memory monitoring
 */
function stopMemoryMonitoring() {
  if (memoryMonitorTimer) {
    clearInterval(memoryMonitorTimer);
    memoryMonitorTimer = null;
  }
}

//============================================================================
// Storage Management - IndexedDB
//============================================================================

/**
 * IndexedDB wrapper for log storage
 */
class LogDatabase {
  constructor() {
    this.db = null;
    this.dbName = 'OTGW-Logs';
    this.version = 1;
  }
  
  async init() {
    return new Promise((resolve, reject) => {
      // Safari Private Browsing may throw SecurityError when opening IndexedDB
      let request;
      try {
        request = indexedDB.open(this.dbName, this.version);
      } catch (e) {
        console.warn('IndexedDB.open() failed (Safari Private Browsing?):', e);
        reject(new Error('IndexedDB unavailable - possibly in Private Browsing mode'));
        return;
      }
      
      request.onerror = () => {
        console.error('IndexedDB error:', request.error);
        reject(request.error);
      };
      
      // Safari may also silently block the request
      request.onblocked = () => {
        console.warn('IndexedDB blocked - close other tabs/windows');
      };
      
      request.onsuccess = () => {
        this.db = request.result;
        console.log('IndexedDB initialized');
        resolve(this.db);
      };
      
      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        
        // Create logs object store
        if (!db.objectStoreNames.contains('logs')) {
          const logsStore = db.createObjectStore('logs', { autoIncrement: true });
          logsStore.createIndex('timestamp', 'time', { unique: false });
          logsStore.createIndex('sessionId', 'sessionId', { unique: false });
        }
        
        // Create sessions object store
        if (!db.objectStoreNames.contains('sessions')) {
          const sessionsStore = db.createObjectStore('sessions', { keyPath: 'id' });
          sessionsStore.createIndex('timestamp', 'timestamp', { unique: false });
        }
        
        // Create metadata object store
        if (!db.objectStoreNames.contains('metadata')) {
          db.createObjectStore('metadata', { keyPath: 'key' });
        }
      };
    });
  }
  
  async saveLogs(logs, sessionId) {
    if (!this.db) {
      console.warn('Database not initialized');
      return false;
    }
    
    try {
      const tx = this.db.transaction(['logs'], 'readwrite');
      const store = tx.objectStore('logs');
      
      for (const log of logs) {
        store.add({ ...log, sessionId });
      }
      
      return new Promise((resolve, reject) => {
        tx.oncomplete = () => resolve(true);
        tx.onerror = () => reject(tx.error);
      });
    } catch (e) {
      console.error('Error saving logs:', e);
      return false;
    }
  }
  
  async saveSession(session) {
    if (!this.db) return false;
    
    try {
      const tx = this.db.transaction(['sessions'], 'readwrite');
      const store = tx.objectStore('sessions');
      store.put(session);
      
      return new Promise((resolve, reject) => {
        tx.oncomplete = () => resolve(true);
        tx.onerror = () => reject(tx.error);
      });
    } catch (e) {
      console.error('Error saving session:', e);
      return false;
    }
  }
  
  async getRecentSession(maxAgeMs = 3600000) {
    if (!this.db) return null;
    
    try {
      const tx = this.db.transaction(['sessions'], 'readonly');
      const store = tx.objectStore('sessions');
      const index = store.index('timestamp');
      const cutoff = Date.now() - maxAgeMs;
      const range = IDBKeyRange.lowerBound(cutoff);
      
      return new Promise((resolve, reject) => {
        const request = index.openCursor(range, 'prev');
        request.onsuccess = (event) => {
          const cursor = event.target.result;
          resolve(cursor ? cursor.value : null);
        };
        request.onerror = () => reject(request.error);
      });
    } catch (e) {
      console.error('Error getting recent session:', e);
      return null;
    }
  }
  
  async loadLogsForSession(sessionId, limit = null) {
    if (!this.db) return [];
    
    try {
      const tx = this.db.transaction(['logs'], 'readonly');
      const store = tx.objectStore('logs');
      const index = store.index('sessionId');
      const range = IDBKeyRange.only(sessionId);
      
      return new Promise((resolve, reject) => {
        const request = index.getAll(range, limit);
        request.onsuccess = () => resolve(request.result || []);
        request.onerror = () => reject(request.error);
      });
    } catch (e) {
      console.error('Error loading logs:', e);
      return [];
    }
  }
  
  async cleanOldSessions(retentionDays = 7) {
    if (!this.db) return false;
    
    try {
      const cutoff = Date.now() - (retentionDays * 24 * 60 * 60 * 1000);
      
      const tx = this.db.transaction(['sessions', 'logs'], 'readwrite');
      const sessionsStore = tx.objectStore('sessions');
      const logsStore = tx.objectStore('logs');
      
      // Find old sessions
      const sessionsIndex = sessionsStore.index('timestamp');
      const range = IDBKeyRange.upperBound(cutoff);
      
      return new Promise((resolve, reject) => {
        const sessionIds = [];
        const sessionRequest = sessionsIndex.openCursor(range);
        
        sessionRequest.onsuccess = (event) => {
          const cursor = event.target.result;
          if (cursor) {
            sessionIds.push(cursor.value.id);
            cursor.delete(); // Delete old session
            cursor.continue();
          } else {
            // Now delete logs for these sessions
            const logsIndex = logsStore.index('sessionId');
            sessionIds.forEach(id => {
              const logRange = IDBKeyRange.only(id);
              const logRequest = logsIndex.openCursor(logRange);
              logRequest.onsuccess = (e) => {
                const logCursor = e.target.result;
                if (logCursor) {
                  logCursor.delete();
                  logCursor.continue();
                }
              };
            });
          }
        };
        
        tx.oncomplete = () => {
          console.log(`Cleaned up ${sessionIds.length} old sessions`);
          resolve(true);
        };
        tx.onerror = () => reject(tx.error);
      });
    } catch (e) {
      console.error('Error cleaning old sessions:', e);
      return false;
    }
  }
  
  async getStorageSize() {
    if (!this.db) return 0;
    
    try {
      const tx = this.db.transaction(['logs', 'sessions'], 'readonly');
      const logsStore = tx.objectStore('logs');
      const sessionsStore = tx.objectStore('sessions');
      
      return new Promise((resolve, reject) => {
        let totalSize = 0;
        
        const logsRequest = logsStore.count();
        logsRequest.onsuccess = () => {
          totalSize += logsRequest.result * 500; // Estimate 500 bytes per log
          
          const sessionsRequest = sessionsStore.count();
          sessionsRequest.onsuccess = () => {
            totalSize += sessionsRequest.result * 200; // Estimate 200 bytes per session
            resolve(totalSize);
          };
          sessionsRequest.onerror = () => reject(sessionsRequest.error);
        };
        logsRequest.onerror = () => reject(logsRequest.error);
      });
    } catch (e) {
      console.error('Error getting storage size:', e);
      return 0;
    }
  }
}

//============================================================================
// Storage Management - localStorage
//============================================================================

/**
 * Save UI state to localStorage
 * Safari limits: 5 MB, Chrome/Firefox: 10 MB
 */
function saveUIStateToLocalStorage() {
  // Check if localStorage is available (Safari Private Browsing may disable it)
  try {
    if (!window.localStorage) {
      console.warn('localStorage not available');
      return false;
    }
  } catch (e) {
    console.warn('localStorage access denied:', e);
    return false;
  }
  
  const state = {
    autoScroll,
    showTimestamps,
    logExpanded,
    searchTerm,
    captureMode: maxLogLines === MAX_LOG_LINES_CAPTURE,
    storageMode,
    timestamp: Date.now()
  };
  
  try {
    localStorage.setItem('otgw_ui_state', JSON.stringify(state));
    return true;
  } catch (e) {
    if (e.name === 'QuotaExceededError') {
      console.warn('localStorage quota exceeded for UI state');
    } else {
      console.error('Error saving UI state:', e);
    }
    return false;
  }
}

/**
 * Restore UI state from localStorage
 */
function restoreUIStateFromLocalStorage() {
  // Check if localStorage is available
  try {
    if (!window.localStorage) {
      console.warn('localStorage not available');
      return false;
    }
  } catch (e) {
    console.warn('localStorage access denied:', e);
    return false;
  }
  
  try {
    const json = localStorage.getItem('otgw_ui_state');
    if (!json) return false;
    
    const state = JSON.parse(json);
    
    // Restore state variables
    autoScroll = state.autoScroll ?? true;
    showTimestamps = state.showTimestamps ?? true;
    logExpanded = state.logExpanded ?? false;
    searchTerm = state.searchTerm ?? '';
    storageMode = state.storageMode ?? 'smart';
    
    // Update UI elements
    const btnAutoScroll = document.getElementById('btnAutoScroll');
    if (btnAutoScroll) {
      if (autoScroll) {
        btnAutoScroll.classList.add('btn-active');
      } else {
        btnAutoScroll.classList.remove('btn-active');
      }
    }
    
    const chkTimestamp = document.getElementById('chkShowTimestamp');
    if (chkTimestamp) {
      chkTimestamp.checked = showTimestamps;
    }
    
    const chkCapture = document.getElementById('chkCaptureMode');
    if (chkCapture && state.captureMode) {
      chkCapture.checked = true;
      maxLogLines = MAX_LOG_LINES_CAPTURE;
    }
    
    const searchInput = document.getElementById('searchLog');
    if (searchInput && searchTerm) {
      searchInput.value = searchTerm;
    }
    
    console.log('UI state restored from localStorage');
    return true;
  } catch (e) {
    console.error('Error restoring UI state:', e);
    return false;
  }
}

/**
 * Save recent logs backup to localStorage
 */
function saveRecentLogsToLocalStorage() {
  const backup = {
    timestamp: Date.now(),
    device: window.location.hostname,
    logs: otLogBuffer.slice(-STORAGE_CONFIG.maxLocalStorageLines)
  };
  
  try {
    localStorage.setItem('otgw_logs_backup', JSON.stringify(backup));
    return true;
  } catch (e) {
    if (e.name === 'QuotaExceededError') {
      // Try with fewer lines
      backup.logs = otLogBuffer.slice(-500);
      try {
        localStorage.setItem('otgw_logs_backup', JSON.stringify(backup));
        console.warn('Saved reduced backup due to quota');
        return true;
      } catch (e2) {
        console.warn('Cannot save logs backup: quota exceeded');
        return false;
      }
    } else {
      console.error('Error saving logs backup:', e);
      return false;
    }
  }
}

/**
 * Load recent logs backup from localStorage
 */
function loadRecentLogsFromLocalStorage(maxAge = 300000) {
  try {
    const json = localStorage.getItem('otgw_logs_backup');
    if (!json) return null;
    
    const backup = JSON.parse(json);
    
    // Check if backup is recent enough (default 5 minutes)
    if (Date.now() - backup.timestamp > maxAge) {
      console.log('Backup too old, ignoring');
      return null;
    }
    
    // Check if same device
    if (backup.device !== window.location.hostname) {
      console.log('Backup from different device, ignoring');
      return null;
    }
    
    return backup.logs || [];
  } catch (e) {
    console.error('Error loading logs backup:', e);
    return null;
  }
}

//============================================================================
// Storage Auto-Save and Recovery
//============================================================================

/**
 * Initialize storage system
 */
async function initStorageSystem() {
  // Guard against multiple initializations
  if (storageSystemInitialized) {
    console.log('Storage system already initialized');
    return;
  }
  
  console.log('Initializing storage system...');
  
  // Generate session ID
  currentSessionId = 'session_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
  
  // Restore UI state from localStorage
  restoreUIStateFromLocalStorage();
  
  // Check if IndexedDB is supported and available (Safari Private Browsing disables it)
  if ('indexedDB' in window) {
    try {
      logDatabase = new LogDatabase();
      await logDatabase.init();
      console.log('IndexedDB ready');
      
      // Safari ITP may clear data after 7 days - log warning
      if (navigator.userAgent.includes('Safari') && !navigator.userAgent.includes('Chrome')) {
        console.info('Note: Safari may clear stored data after 7 days of inactivity (ITP policy)');
      }
      
      // Check for recent session to recover
      if (storageMode !== 'disabled') {
        await checkForRecoverableSession();
      }
    } catch (e) {
      console.warn('IndexedDB initialization failed:', e);
      // Check if this is Safari Private Browsing
      if (e.message && e.message.includes('Private Browsing')) {
        console.warn('IndexedDB unavailable in Private Browsing mode - using localStorage only');
      }
      logDatabase = null;
    }
  }
  
  // Start auto-save if enabled
  if (STORAGE_CONFIG.autoSaveEnabled && storageMode !== 'disabled') {
    startAutoSave();
  }
  
  // Save state on page unload (only add listener once)
  window.addEventListener('beforeunload', handlePageUnload);
  
  storageSystemInitialized = true;
  console.log('Storage system initialized');
}

/**
 * Check for recoverable session
 */
async function checkForRecoverableSession() {
  if (!logDatabase) return;
  
  try {
    const recentSession = await logDatabase.getRecentSession(3600000); // 1 hour
    
    if (recentSession && recentSession.lineCount > 0) {
      showRecoveryBanner(recentSession);
    }
  } catch (e) {
    console.error('Error checking for recoverable session:', e);
  }
}

/**
 * Show recovery banner
 */
function showRecoveryBanner(session) {
  const bannerId = 'recoveryBanner';
  let banner = document.getElementById(bannerId);
  
  if (!banner) {
    banner = document.createElement('div');
    banner.id = bannerId;
    banner.className = 'recovery-banner';
    
    const logSection = document.getElementById('otLogSection');
    if (logSection) {
      logSection.insertBefore(banner, logSection.firstChild);
    }
  }
  
  const date = new Date(session.timestamp);
  const timeStr = date.toLocaleTimeString();
  
  banner.innerHTML = `
    ℹ️ Found previous session from ${timeStr} with ${session.lineCount.toLocaleString()} log lines. 
    <button onclick="recoverSession('${session.id}')" class="btn-inline">Recover</button>
    <button onclick="dismissRecoveryBanner()" class="btn-inline">Ignore</button>
  `;
  
  banner.style.display = 'block';
}

/**
 * Recover session from IndexedDB
 */
async function recoverSession(sessionId) {
  if (!logDatabase) {
    alert('Cannot recover: IndexedDB not available');
    return;
  }
  
  try {
    dismissRecoveryBanner();
    
    // Show progress
    const progressEl = document.getElementById('recoveryProgress');
    if (progressEl) {
      progressEl.style.display = 'block';
      progressEl.textContent = 'Loading logs...';
    }
    
    const logs = await logDatabase.loadLogsForSession(sessionId);
    
    if (logs && logs.length > 0) {
      // Clear current buffer
      otLogBuffer = [];
      
      // Add recovered logs
      logs.forEach(log => {
        otLogBuffer.push(log);
      });
      
      // Update display
      updateFilteredBuffer();
      updateLogDisplay();
      updateLogCounters();
      
      console.log(`Recovered ${logs.length} logs from session ${sessionId}`);
      
      if (progressEl) {
        progressEl.textContent = `Recovered ${logs.length.toLocaleString()} logs`;
        setTimeout(() => {
          progressEl.style.display = 'none';
        }, 3000);
      }
    }
  } catch (e) {
    console.error('Error recovering session:', e);
    alert('Failed to recover session: ' + e.message);
  }
}

/**
 * Dismiss recovery banner
 */
function dismissRecoveryBanner() {
  const banner = document.getElementById('recoveryBanner');
  if (banner) {
    banner.style.display = 'none';
  }
}

// Expose memory/storage functions globally for onclick handlers
// (functions are already global but this makes intent explicit)
window.enableCaptureMode = enableCaptureMode;
window.startFileStreamingPrompt = startFileStreamingPrompt;
window.dismissMemoryWarning = dismissMemoryWarning;
window.recoverSession = recoverSession;
window.dismissRecoveryBanner = dismissRecoveryBanner;

/**
 * Start auto-save timer
 */
function startAutoSave() {
  if (autoSaveTimer) {
    clearInterval(autoSaveTimer);
  }
  
  autoSaveTimer = setInterval(async () => {
    await saveCurrentState();
  }, AUTO_SAVE_INTERVAL);
  
  console.log('Auto-save enabled');
}

/**
 * Stop auto-save timer
 */
function stopAutoSave() {
  if (autoSaveTimer) {
    clearInterval(autoSaveTimer);
    autoSaveTimer = null;
  }
  console.log('Auto-save disabled');
}

/**
 * Save current state based on storage mode
 */
async function saveCurrentState() {
  // Always save UI state to localStorage
  saveUIStateToLocalStorage();
  
  if (storageMode === 'disabled' || otLogBuffer.length === 0) {
    return;
  }
  
  // Determine storage mode
  const mode = determineStorageMode();
  
  if (mode === 'indexeddb' && logDatabase) {
    // Save to IndexedDB for large datasets
    try {
      // Only save new logs since last save (simplified here: save all)
      await logDatabase.saveLogs(otLogBuffer, currentSessionId);
      
      // Update session info
      await logDatabase.saveSession({
        id: currentSessionId,
        timestamp: Date.now(),
        lineCount: otLogBuffer.length,
        device: window.location.hostname
      });
      
      console.log(`Auto-saved ${otLogBuffer.length} logs to IndexedDB`);
    } catch (e) {
      console.error('Error auto-saving to IndexedDB:', e);
    }
  } else if (mode === 'localstorage') {
    // Save to localStorage for small datasets
    saveRecentLogsToLocalStorage();
  }
}

/**
 * Determine which storage mode to use based on data size
 */
function determineStorageMode() {
  if (storageMode !== 'smart') {
    return storageMode;
  }
  
  // Smart mode logic
  if (otLogBuffer.length > 10000) {
    return 'indexeddb';
  } else if (otLogBuffer.length > 1000) {
    return 'localstorage';
  } else {
    return 'none';
  }
}

/**
 * Handle page unload - save state
 */
function handlePageUnload(event) {
  // Save UI state
  saveUIStateToLocalStorage();
  
  // Save recent logs to localStorage for quick recovery
  if (otLogBuffer.length > 0) {
    saveRecentLogsToLocalStorage();
  }
  
  // Note: Can't use async operations here reliably
}

/**
 * Check storage quota
 * Note: Safari < 17 doesn't support navigator.storage.estimate()
 */
async function checkStorageQuota() {
  if ('storage' in navigator && 'estimate' in navigator.storage) {
    try {
      const estimate = await navigator.storage.estimate();
      return {
        usage: estimate.usage || 0,
        quota: estimate.quota || 0,
        percent: estimate.quota > 0 ? (estimate.usage / estimate.quota) * 100 : 0,
        available: (estimate.quota || 0) - (estimate.usage || 0)
      };
    } catch (e) {
      console.error('Error checking storage quota:', e);
      return null;
    }
  }
  
  // Safari < 17 fallback: return estimated values
  if (navigator.userAgent.includes('Safari') && !navigator.userAgent.includes('Chrome')) {
    console.info('navigator.storage.estimate() not available (Safari < 17), using estimates');
    // Safari typically allows 50-100 MB for IndexedDB
    return {
      usage: 0, // Can't determine actual usage
      quota: 50 * 1024 * 1024, // Estimate 50MB for Safari
      percent: 0,
      available: 50 * 1024 * 1024,
      estimated: true // Flag to indicate this is a fallback
    };
  }
  
  return null;
}

/**
 * Update storage display in UI
 */
async function updateStorageDisplay() {
  const storageEl = document.getElementById('storageStatus');
  if (!storageEl) return;
  
  const quota = await checkStorageQuota();
  
  if (quota) {
    const usedMB = quota.usage / (1024 * 1024);
    const totalGB = quota.quota / (1024 * 1024 * 1024);
    const percent = quota.percent;
    
    let indicator = '🟢';
    if (percent > 90) indicator = '🔴';
    else if (percent > 75) indicator = '🟡';
    
    // Add '~' prefix for Safari < 17 estimates
    const prefix = quota.estimated ? '~' : '';
    
    storageEl.innerHTML = `Storage: ${prefix}${usedMB.toFixed(1)} MB / ${totalGB.toFixed(1)} GB (${percent.toFixed(1)}%) ${indicator}`;
  } else {
    storageEl.innerHTML = 'Storage: Not available';
  }
}

//============================================================================
function setupOTLogControls() {
  // Only setup event listeners once to prevent duplicates
  if (otLogControlsInitialized) {
    // Even if initialized, check sync of capture mode just in case (e.g. tab switch)
    syncCaptureMode();
    return;
  }
  
  // Toggle expand/collapse
  document.getElementById('btnToggleLog').addEventListener('click', function() {
    logExpanded = !logExpanded;
    const container = document.getElementById('otLogContainer');
    const btn = document.getElementById('btnToggleLog');
    
    if (logExpanded) {
      container.classList.remove('collapsed');
      btn.textContent = '▼ Show Less';
    } else {
      container.classList.add('collapsed');
      btn.textContent = '▲ Show More';
    }
    
    updateLogDisplay();
  });
  
  // Toggle auto-scroll
  document.getElementById('btnAutoScroll').addEventListener('click', function() {
    autoScroll = !autoScroll;
    const btn = document.getElementById('btnAutoScroll');
    
    if (autoScroll) {
      btn.classList.add('btn-active');
    } else {
      btn.classList.remove('btn-active');
    }
    if (typeof saveUISetting === 'function') saveUISetting('ui_autoscroll', autoScroll);
  });

  // Toggle Capture Mode
  const chkCapture = document.getElementById('chkCaptureMode');
  if (chkCapture) {
    chkCapture.addEventListener('change', function(e) {
      if (e.target.checked) {
        maxLogLines = MAX_LOG_LINES_CAPTURE;
      } else {
        maxLogLines = MAX_LOG_LINES_DEFAULT;
        if (otLogBuffer.length > maxLogLines) {
           otLogBuffer.splice(0, otLogBuffer.length - maxLogLines);
           updateFilteredBuffer();
           updateLogDisplay();
        }
      }
      updateLogCounters();
      if (typeof saveUISetting === 'function') saveUISetting('ui_capture', e.target.checked);
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
  
  // Clear log
  document.getElementById('btnClearLog').addEventListener('click', function() {
    if (confirm('Clear all log messages from buffer?')) {
      otLogBuffer = [];
      otLogFilteredBuffer = [];
      updateLogDisplay();
      updateLogCounters();
    }
  });
  
  // Download log
  // Download button with dropdown menu
  const btnDownloadLog = document.getElementById('btnDownloadLog');
  const downloadMenu = document.querySelector('.download-menu');
  
  if (btnDownloadLog && downloadMenu) {
    // Toggle dropdown
    btnDownloadLog.addEventListener('click', function(e) {
      e.stopPropagation();
      downloadMenu.classList.toggle('hidden');
    });
    
    // Close dropdown when clicking outside
    document.addEventListener('click', function() {
      if (downloadMenu && !downloadMenu.classList.contains('hidden')) {
        downloadMenu.classList.add('hidden');
      }
    });
    
    // Handle format selection
    document.querySelectorAll('.download-menu-item').forEach(item => {
      item.addEventListener('click', function(e) {
        e.stopPropagation();
        const format = this.getAttribute('data-format');
        downloadMenu.classList.add('hidden');
        downloadLog(false, format);
      });
    });
  }


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
  
  // Manual scroll detection (disable auto-scroll if user scrolls up)
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
        document.getElementById('btnAutoScroll').classList.remove('btn-active');
      }
    }, 100);
  });
  
  // Mark as initialized after all listeners are successfully registered
  otLogControlsInitialized = true;
  
  // Sync initial state
  syncCaptureMode();
  updateLogCounters();
}

function syncCaptureMode() {
  const chkCapture = document.getElementById('chkCaptureMode');
  if (chkCapture) {
     if (chkCapture.checked) {
        maxLogLines = MAX_LOG_LINES_CAPTURE;
     } else {
        maxLogLines = MAX_LOG_LINES_DEFAULT;
     }
  }
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
/**
 * Download log in specified format (txt, json, or csv)
 */
function downloadLog(isAuto = false, format = 'txt') {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
  const prefix = isAuto ? 'otgw-log-auto-' : 'otgw-log-';
  const filename = `${prefix}${timestamp}.${format}`;
  
  let content, mimeType;
  
  if (format === 'json') {
    content = exportLogsAsJSON();
    mimeType = 'application/json';
  } else if (format === 'csv') {
    content = exportLogsAsCSV();
    mimeType = 'text/csv';
  } else {
    // Default text format
    content = exportLogsAsText();
    mimeType = 'text/plain';
  }
  
  const blob = new Blob([content], { type: mimeType });

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

/**
 * Export logs as plain text
 */
function exportLogsAsText() {
  let content = '# OTGW Log Export\n';
  content += `# Exported: ${new Date().toLocaleString()}\n`;
  content += `# Total Lines: ${otLogBuffer.length}\n`;
  content += '#' + '='.repeat(70) + '\n\n';
  
  otLogBuffer.forEach(entry => {
    const text = formatLogLine(entry.data);
    const line = showTimestamps ? `${entry.time} ${text}` : text;
    content += line + '\n';
  });
  
  return content;
}

/**
 * Export logs as JSON
 */
function exportLogsAsJSON() {
  const exportData = {
    metadata: {
      exported: new Date().toISOString(),
      exportedLocal: new Date().toLocaleString(),
      totalLines: otLogBuffer.length,
      device: window.location.hostname || 'unknown',
      version: document.getElementById('devVersion')?.textContent || 'unknown'
    },
    logs: otLogBuffer.map(entry => ({
      timestamp: entry.time,
      timestampISO: new Date(entry.time).toISOString(),
      raw: entry.data,
      formatted: formatLogLine(entry.data),
      // Parse OpenTherm message if possible
      msgType: entry.data.length >= 1 ? entry.data[0] : null,
      dataId: entry.data.length >= 2 ? entry.data.slice(1, 3) : null,
      value: entry.data.length >= 4 ? entry.data.slice(3) : null
    }))
  };
  
  return JSON.stringify(exportData, null, 2);
}

/**
 * Export logs as CSV
 */
function exportLogsAsCSV() {
  // CSV header
  let csv = 'Timestamp,TimestampISO,MessageType,DataID,Value,Raw,Formatted\n';
  
  // Add each log entry
  otLogBuffer.forEach(entry => {
    const formatted = formatLogLine(entry.data);
    const timestampISO = new Date(entry.time).toISOString();
    const msgType = entry.data.length >= 1 ? entry.data[0] : '';
    const dataId = entry.data.length >= 2 ? entry.data.slice(1, 3) : '';
    const value = entry.data.length >= 4 ? entry.data.slice(3) : '';
    
    // Escape quotes in fields and wrap in quotes if needed
    const escapeCsvField = (field) => {
      field = String(field);
      if (field.includes(',') || field.includes('"') || field.includes('\n')) {
        return '"' + field.replace(/"/g, '""') + '"';
      }
      return field;
    };
    
    csv += `${escapeCsvField(entry.time)},${escapeCsvField(timestampISO)},${escapeCsvField(msgType)},${escapeCsvField(dataId)},${escapeCsvField(value)},${escapeCsvField(entry.data)},${escapeCsvField(formatted)}\n`;
  });
  
  return csv;
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
function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

//============================================================================  
function loadUISettings() {
  console.log("loadUISettings() ..");
  fetch(APIGW + "v0/settings")
    .then(response => response.json())
    .then(json => {
      let data = json.settings;
      for (let i in data) {
        let name = data[i].name;
        let val = data[i].value;
        
        // Map backend settings to UI elements
        if (name === "#uiAutoScroll") {
           // AutoScroll isn't a checkbox but a button state and global var
           let newVal = strToBool(val);
           autoScroll = newVal;
           const btn = document.getElementById('btnAutoScroll');
           if (btn) {
             if (autoScroll) btn.classList.add('btn-active');
             else btn.classList.remove('btn-active');
           }
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
             if (el.checked) maxLogLines = MAX_LOG_LINES_CAPTURE;
             else maxLogLines = MAX_LOG_LINES_DEFAULT;
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

  needReload = false;
  
  setupPersistentUIListeners();
  loadPersistentUI();

  applyTheme();

  if (typeof OTGraph !== 'undefined') {
      OTGraph.init();
  }

  // Start time updates if not in flash mode
  if (!flashModeActive && !timeupdate) {
    timeupdate = setInterval(function () { refreshDevTime(); }, 1000);
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
    // Initialize storage system and memory monitoring
    initStorageSystem().catch(e => console.error('Storage init error:', e));
    startMemoryMonitoring();
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
    .then(response => response.json())
    .then(json => {
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      for (let i in json.devtime) {
        if (json.devtime[i].name == "dateTime") {
          //console.log("Got new time ["+json.devtime[i].value+"]");
          document.getElementById('theTime').innerHTML = json.devtime[i].value;
        }
        if (json.devtime[i].name == "message") document.getElementById('message').innerHTML = json.devtime[i].value;
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
    .then(response => response.json())
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
    .then(response => response.json())
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
      
      let infoContent = "";
      // infoContent += "<b>PIC Status:</b> " + ((picInfo.available == "true" || picInfo.available == true) ? "Available" : "Not Available") + "<br>";
      infoContent += "<b>PIC Device:</b> " + picInfo.device + "<br>";
      infoContent += "<b>PIC Type:</b> <span id='pic_type_display'>" + picInfo.type + "</span><br>";
      infoContent += "<b>PIC Firmware Version:</b> <span id='pic_version_display'>" + picInfo.version + "</span>";
      
      infoDiv.innerHTML = infoContent;
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
      percentageText.innerText = "Ready to flash";
      
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
  document.getElementById('devName').innerHTML = "";
  fetch(APIGW + "v0/devinfo")
    .then(response => response.json())
    .then(json => {
      console.log("parsed .., data is [" + JSON.stringify(json) + "]");
      data = json.devinfo;
      for (let i in data) {
        if (data[i].name == "fwversion") {
          document.getElementById('devVersion').innerHTML = json.devinfo[i].value;

        } else if (data[i].name == 'hostname') {
          document.getElementById('devName').innerHTML += data[i].value + " ";

        } else if (data[i].name == 'ipaddress') {
          document.getElementById('devName').innerHTML += " (" + data[i].value + ") ";
        }
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
  console.log("refreshOTmonitor() ..");

  data = {};
  fetch(APIGW + "v2/otgw/otmonitor")  //api/v2/otgw/otmonitor
    .then(response => response.json())
    .then(json => {
      console.log("then(json => ..)");
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      needReload = false;
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
      if (needReload) window.location.reload(true);
    })
    .catch(function (error) {
      console.error("refreshOTmonitor error:", error);
      var waiting = document.getElementById('waiting');
      if (waiting) {
        waiting.textContent = 'Error: ' + error.message + ' (Retrying...)';
        waiting.className = 'waiting-error';
      }
    });

} // refreshOTmonitor()


function refreshDeviceInfo() {
  console.log("refreshDeviceInfo() ..");

  data = {};
  fetch(APIGW + "v0/devinfo")
    .then(response => response.json())
    .then(json => {
      console.log("then(json => ..)");
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
    .then(response => response.json())
    .then(json => {
      console.log("then(json => ..)");
      console.log("parsed .., data is [" + JSON.stringify(json) + "]");
      data = json.settings;
      document.getElementById("settingMessage").innerHTML = "";
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
          sInput.addEventListener('change',
            function () { 
              document.getElementById(data[i].name).className = "input-changed"; 
              if (data[i].name == "darktheme") {
                 document.getElementById('theme-style').href = this.checked ? "index_dark.css" : "index.css";
              }
              setVisible('btnSaveSettings', true);
            },
            false
          );
          sInput.addEventListener('keydown',
            function () { 
              document.getElementById(data[i].name).className = "input-changed"; 
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
          document.getElementById(data[i].name).className = "input-normal";
          //----document.getElementById("setFld_"+data[i].name).value = data[i].value;
          // document.getElementById(data[i].name).value = data[i].value;
          // FIX If checkbox change checked iso value
          if (data[i].type == "b")
            document.getElementById(data[i].name).checked = strToBool(data[i].value);
          else document.getElementById(data[i].name).value = data[i].value;

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
    if (inputs[i].type == "checkbox") {
      value = document.getElementById(field).checked;
    } else {
      value = document.getElementById(field).value;
    }
    console.log("==> name[" + field + "], value[" + value + "]");

    if (document.getElementById(field).className == "input-changed") {
      //then it was changes, and needs to be saved
      document.getElementById(field).className = "input-normal";
      console.log("Changes where made in [" + field + "][" + value + "]");
      
      // Update theme immediately if darktheme setting changed
      if (field === "darktheme") {
        let isDark = document.getElementById(field).checked;
        document.getElementById('theme-style').href = isDark ? "index_dark.css" : "index.css";
        localStorage.setItem('theme', isDark ? 'dark' : 'light');
      }

      //processWithTimeout([(data.length -1), 0], 2, data, sendPostReading);
      document.getElementById("settingMessage").innerHTML = "Saving changes...";
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
      if (response.ok) {
        document.getElementById("settingMessage").innerHTML = "Saving changes... SUCCESS";
        setTimeout(function () { document.getElementById("settingMessage").innerHTML = ""; }, 2000); //and clear the message
      } else {
        document.getElementById("settingMessage").innerHTML = "Saving changes... FAILED";
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
    .then(response => response.json())
    .then(json => {
      let data = json.settings;
      for (let i in data) {
        if (data[i].name == "darktheme") {
           let isDark = strToBool(data[i].value);
           document.getElementById('theme-style').href = isDark ? "index_dark.css" : "index.css";
           localStorage.setItem('theme', isDark ? 'dark' : 'light');
           if (typeof OTGraph !== 'undefined' && OTGraph.setTheme) {
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
    
    if (pctText) pctText.innerText = "Connecting to event stream...";
    
    // Ensure WebSocket is connected for progress updates
    initOTLogWebSocket(true);

    // Wait for WebSocket to be OPEN before sending flash command
    let attempts = 0;
    const waitForWS = setInterval(() => {
        attempts++;
        // ReadyState 1 is OPEN
        if ((otLogWS && otLogWS.readyState === 1) || attempts > 50) { // 5s timeout
             clearInterval(waitForWS);
             
             if (pctText) pctText.innerText = "Starting upgrade for " + filename + "...";
             
             fetch(localURL + '/pic?action=upgrade&name=' + filename)
                .then(response => {
                   if (response.headers.get("content-type").indexOf("application/json") !== -1) {
                     return response.json();
                   } else {
                     return {status: "started (legacy)"}; 
                   }
                })
                .then(data => {
                    console.log("Flash started:", data);
                    if (pctText) pctText.innerText = "Flashing " + filename + " started...";
                })
                .catch(error => {
                    console.error("Flash error:", error);
                    isFlashing = false;
                    toggleInteraction(true);
                    if (pctText) pctText.innerText = "Error starting flash: " + error.message;
                    if (progressBar) progressBar.classList.add('error');
                    
                    // Restart polling on start failure
                    if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
                    if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); }, 1000);
                });
        }
    }, 100);
}

function handleFlashMessage(data) {
    try {
        if (!data || !data.startsWith('{')) return false; // Not JSON
        
        // Try parsing as JSON
        let msg = JSON.parse(data);
        
        // Check if it looks like a flash message
        if (msg.hasOwnProperty('percent') || msg.hasOwnProperty('result')) {
            let progressBar = document.getElementById("flashProgressBar");
            let pctText = document.getElementById("flashPercentageText");
            let progressSection = document.getElementById("flashProgressSection");
            
            if (progressSection && !progressSection.classList.contains('active')) {
                 progressSection.classList.add('active');
            }
            
            if (msg.hasOwnProperty('percent')) {
                if (progressBar) progressBar.style.width = msg.percent + "%";
                if (pctText) pctText.innerText = "Flashing " + currentFlashFilename + " : " + msg.percent + "%";
            }
            
            if (msg.hasOwnProperty('result')) {
                // Done
                let resultText = (msg.result === 0) ? "Success!" : "Failed (Error " + msg.result + ")";
                isFlashing = false;
                toggleInteraction(true);
                
                if (pctText) {
                    pctText.innerText = "Finished " + currentFlashFilename + ": " + resultText;
                }
                
                if (msg.result === 0 && progressBar) {
                    progressBar.style.width = "100%";
                    if (progressBar.classList.contains('error')) progressBar.classList.remove('error');

                    // Look up version for immediate feedback
                    let flashedVer = "Unknown";
                    // Attempt to find version from available global list
                    if (typeof availableFirmwareFiles !== 'undefined') {
                        let f = availableFirmwareFiles.find(x => x.name === currentFlashFilename);
                        if (f && f.version) flashedVer = f.version;
                    }

                    // Infer Type from version or filename roughly if needed, or just say Gateway (most common)
                    // But 'flashedVer' usually contains the full banner e.g. "OpenTherm Gateway 4.3"
                    // If it is just version "4.3", we might want to say "Gateway 4.3"
                    // The backend scanner puts just the version number in .version usually (e.g. "4.3") or "OpenTherm Gateway 4.3"??
                    // apifirmwarefilelist: version = f.readStringUntil('\n'); or GetVersion extracts it.
                    // GetVersion extracts string after "OpenTherm Gateway " offset. So it returns "4.3" or similar.
                    // Let's assume Type is "Gateway" (or Diagnostic) based on filename or just display the version.

                    let displayType = "Gateway"; 
                    if (currentFlashFilename.toLowerCase().includes("diag")) displayType = "Diagnostic";
                    if (currentFlashFilename.toLowerCase().includes("inter")) displayType = "Interface";

                    // Update UI immediately with TARGET version (optimistic)
                    let elType = document.getElementById('pic_type_display');
                    let elVer = document.getElementById('pic_version_display');
                    if (elType) elType.innerText = displayType;
                    if (elVer) elVer.innerText = flashedVer;
                    
                    if (pctText) pctText.innerText = "Successfully flashed to " + displayType + " " + flashedVer;
                    
                    // Trigger actual hardware refresh in background
                    setTimeout(() => refreshFirmware(), 2000); // Give PIC 2s to boot

                    // Reset progress bar after 10 seconds
                    setTimeout(function() {
                        if (progressSection) progressSection.classList.remove('active');
                        if (progressBar) progressBar.style.width = "0%";
                        if (pctText) pctText.innerText = "Ready to flash";
                    }, 10000);

                } else if (progressBar) {
                    progressBar.classList.add('error');
                }
                
                // Restart polling
                if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
                if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); }, 1000);

                // Refresh firmware list after success to show new version?
                if (msg.result === 0) {
                    setTimeout(refreshFirmware, 3000);
                }
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
        
        entry.intervalSum += diff;
        entry.intervalCount++;
        
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
    .then(response => response.json())
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
         const btn = document.getElementById('btnAutoScroll');
         if (btn) {
           if (newVal) btn.classList.add('btn-active');
           else btn.classList.remove('btn-active');
         }
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
              if (typeof syncCaptureMode === 'function') syncCaptureMode();
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
              if (typeof OTGraph !== 'undefined' && OTGraph.toggleAutoExport) {
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
             if (typeof OTGraph !== 'undefined' && OTGraph.toggleAutoScreenshot) {
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
               if (typeof OTGraph !== 'undefined' && OTGraph.setTimeWindow) {
                  OTGraph.setTimeWindow(parseInt(timeVal));
              }
          }
      }

    })
    .catch(err => console.error("Error loading persistent settings:", err));
}

