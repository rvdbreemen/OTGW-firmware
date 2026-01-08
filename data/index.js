/*
***************************************************************************  
**  Program  : index.js, part of OTGW-firmware project
**  Version  : v1.0.0-rc3
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
      document.getElementById('displayMainPage').style.display !== 'none') {
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
const MAX_LOG_LINES = 2000; // limit client-side log buffer to reduce browser memory usage
let autoScroll = true;
let showTimestamps = true;
let logExpanded = false;
let searchTerm = '';
let updatePending = false;
let otLogControlsInitialized = false;
let isFlashing = false;
let currentFlashFilename = "";
let flashModeActive = false; // Track if we're on the flash page

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
      logSection.style.display = 'none';
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
  
  if (dir) {
    text += " " + pad(dir, 16);
  }
  
  text += valid;

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
  const oValid  = 50 + offset; // Type(16) + Space(1) = 17. 33+17=50
  const oPayload= 52 + offset; // Valid(1) + Space(1) = 2. 50+2=52 

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
           console.log("DEBUG: oPayload=" + oPayload + ", payload='" + payload + "'");
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
  if (otLogBuffer.length > MAX_LOG_LINES) {
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
    otLogFilteredBuffer = otLogBuffer.slice();
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

  const displayCount = logExpanded ? otLogFilteredBuffer.length : Math.min(10, otLogFilteredBuffer.length);
  const startIndex = Math.max(0, otLogFilteredBuffer.length - displayCount);
  const linesToShow = otLogFilteredBuffer.slice(startIndex);

  // console.log("Rendering logs. Total:", otLogFilteredBuffer.length, "Showing:", linesToShow.length); // Debug

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

  if (logLineCountEl) {
    logLineCountEl.textContent = otLogBuffer.length;
  }

  if (logFilteredCountEl) {
    logFilteredCountEl.textContent = otLogFilteredBuffer.length;
  }
}

//============================================================================
function setupOTLogControls() {
  // Only setup event listeners once to prevent duplicates
  if (otLogControlsInitialized) {
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
  });
  
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
  document.getElementById('btnDownloadLog').addEventListener('click', function() {
    downloadLog();
  });
  
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
}

//============================================================================
function downloadLog() {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
  const filename = `otgw-log-${timestamp}.txt`;
  
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
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

//============================================================================
function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

//============================================================================  
function initMainPage() {
  console.log("initMainPage()");
  
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
  
  document.getElementById("displayMainPage").style.display = "block";
  document.getElementById("displaySettingsPage").style.display = "none";
  document.getElementById("displayDeviceInfo").style.display = "none";
  document.getElementById("displayPICflash").style.display = "none";
  
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
  document.getElementById("displayMainPage").style.display = "none";
  document.getElementById("displaySettingsPage").style.display = "none";
  document.getElementById("displayDeviceInfo").style.display = "none";
  var firmwarePage = document.getElementById("displayPICflash");
  refreshFirmware();
  document.getElementById("displayPICflash").style.display = "block";
} // deviceinfoPage()

function deviceinfoPage() {
  disconnectOTLogWebSocket();
  clearInterval(tid);
  refreshDevTime();
  document.getElementById("displayMainPage").style.display = "none";
  document.getElementById("displaySettingsPage").style.display = "none";
  document.getElementById("displayPICflash").style.display = "none";
  var deviceinfoPage = document.getElementById("deviceinfoPage");
  refreshDeviceInfo();
  document.getElementById("displayDeviceInfo").style.display = "block";

} // deviceinfoPage()

function settingsPage() {
  disconnectOTLogWebSocket();
  clearInterval(tid);
  refreshDevTime();
  document.getElementById("displayMainPage").style.display = "none";
  document.getElementById("displayDeviceInfo").style.display = "none";
  document.getElementById("displayPICflash").style.display = "none";
  var settingsPage = document.getElementById("settingsPage");
  refreshSettings();
  document.getElementById("displaySettingsPage").style.display = "block";

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

      let displayPICpage = document.getElementById('displayPICpage');
      while (displayPICpage.lastChild) {
        displayPICpage.lastChild.remove();
      }

      // --- PIC Information Section ---
      let infoDiv = document.createElement("div");
      infoDiv.setAttribute("class", "pic-info-header");
      infoDiv.style.marginBottom = "15px";
      infoDiv.style.padding = "10px";
      
      let infoContent = "";
      // infoContent += "<b>PIC Status:</b> " + ((picInfo.available == "true" || picInfo.available == true) ? "Available" : "Not Available") + "<br>";
      infoContent += "<b>PIC Device:</b> " + picInfo.device + "<br>";
      infoContent += "<b>PIC Type:</b> " + picInfo.type + "<br>";
      infoContent += "<b>PIC Firmware Version:</b> " + picInfo.version;
      
      infoDiv.innerHTML = infoContent;
      displayPICpage.appendChild(infoDiv);

      let tableDiv = document.createElement("div");
      tableDiv.setAttribute("class", "pictable");

      var rowDiv = document.createElement("div");
      rowDiv.setAttribute("class", "picrow");
      rowDiv.setAttribute("id", "firmwarename");
      // rowDiv.style.background = "lightblue";
      rowDiv.style.fontWeight = "bold";
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
        img.style.width = '16px';
        img.style.height = 'auto';
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
        img.style.width = '16px';
        img.style.height = 'auto';
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
      progressDiv.style.display = "block";
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
  fetch(APIGW + "v1/otgw/otmonitor")  //api/v1/otgw/otmonitor
    .then(response => response.json())
    .then(json => {
      console.log("then(json => ..)");
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      needReload = false;
      data = json.otmonitor;

      let otMonPage = document.getElementById('mainPage');
      while (otMonPage.lastChild) {
        otMonPage.lastChild.remove();
      }
      let otMonTable = document.createElement("div");
      otMonTable.setAttribute("class", "otmontable");

      for (let i in data) {
        //console.log("["+data[i].name+"]=>["+data[i].value+"]");

        if ((document.getElementById("otmon_" + data[i].name)) == null) { // if element does not exists yet, then build page
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "otmonrow");
          //rowDiv.setAttribute("id", "otmon_"+data[i].name);
          // rowDiv.style.background = "lightblue";
          rowDiv.style.display = ((data[i].epoch == 0) ? "none" : "flex");
          // rowDiv.style.display = ((data[i].epoch==0)?"none":"table-row");
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
            update.parentNode.style.display = ((data[i].epoch == 0) ? "none" : "flex");
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
      otMonPage.appendChild(otMonTable);
      if (needReload) window.location.reload(true);
    })
    .catch(function (error) {
      console.error("refreshOTmonitor error:", error);
      var waiting = document.getElementById('waiting');
      if (waiting) {
        waiting.textContent = 'Error: ' + error.message + ' (Retrying...)';
        waiting.style.color = 'red';
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
          fldDiv.setAttribute("style", "margin-right: 10px;");
          fldDiv.style.width = "320px";
          fldDiv.style.float = 'left';
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
  document.getElementById(field).style.background = newColor;

} // setBackGround()


//============================================================================  
function getBackGround(field) {
  //console.log("getBackground("+field+")");
  return document.getElementById(field).style.background;

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
    currentFlashFilename = filename;
    isFlashing = true;
    toggleInteraction(false);
    // Stop polling during upgrade to prevent interference and reduce load
    if (tid) { clearInterval(tid); tid = 0; }
    if (timeupdate) { clearInterval(timeupdate); timeupdate = 0; }

    let progressSection = document.getElementById("flashProgressSection");
    let progressBar = document.getElementById("flashProgressBar");
    let pctText = document.getElementById("flashPercentageText");
    
    if (progressSection) progressSection.style.display = "block";
    if (progressBar) {
        progressBar.style.width = "0%";
        progressBar.style.backgroundColor = "#4CAF50";
    }
    if (pctText) pctText.innerText = "Starting upgrade for " + filename + "...";
    
    // Ensure WebSocket is connected for progress updates
    initOTLogWebSocket(true);
    
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
        if (progressBar) progressBar.style.backgroundColor = "red";
        
        // Restart polling on start failure
        if (!tid) tid = setInterval(function () { refreshOTmonitor(); }, 1000);
        if (!timeupdate) timeupdate = setInterval(function () { refreshDevTime(); }, 1000);
    });
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
            
            if (progressSection && progressSection.style.display === "none") {
                 progressSection.style.display = "block";
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
                } else if (progressBar) {
                    progressBar.style.backgroundColor = "red";
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
    if (typeCode === null || ![1, 4, 6, 7].includes(typeCode)) return;

    const dirStr = (typeof line.dir === 'string' && line.dir) ? line.dir : '';
    const dirLabel = otmDirectionLabel(typeCode, dirStr);
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

