/*
***************************************************************************  
**  Program  : index.js, part of OTGW-firmware project
**  Version  : v2.0.0-alpha.3
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
const localURL = window.location.protocol + '//' + window.location.host;
const APIGW = window.location.protocol + '//' + window.location.host + '/api/';
const MOBILE_BREAKPOINT_PX = 768;
const PS_MODE_NOTICE_TEXT = 'PS=1 mode active: showing decoded field summaries. Raw OT frames not available.';
const WEBKIT_SCROLLBAR_STYLE_ID = 'otgw-webkit-scrollbar-style';

"use strict";
// ============================================================================
// Utility Functions for Safe Operations
// ============================================================================

/**
 * Safely parse JSON with validation and error handling
 * @param {string} text - The JSON string to parse
 * @returns {object|null} Parsed object or null on error
 */
function safeJSONParse(text) {
  if (!text || typeof text !== 'string') {
    console.warn('safeJSONParse: Invalid input (not a string)');
    return null;
  }
  
  // Quick validation - JSON should start with { or [
  const trimmed = text.trim();
  if (!trimmed.startsWith('{') && !trimmed.startsWith('[')) {
    console.warn('safeJSONParse: Input does not look like JSON');
    return null;
  }
  
  try {
    return JSON.parse(text);
  } catch (e) {
    console.error('safeJSONParse: Parse error:', e.message);
    return null;
  }
}

/**
 * Safely get element by ID with optional warning
 * @param {string} id - Element ID
 * @param {boolean} warnIfMissing - Whether to log warning if not found
 * @returns {Element|null} The element or null
 */
function safeGetElementById(id, warnIfMissing = false) {
  const element = document.getElementById(id);
  if (!element && warnIfMissing) {
    console.warn(`Element not found: #${id}`);
  }
  return element;
}

function ensureWebkitScrollbarStyles() {
  if (!('WebkitAppearance' in document.documentElement.style)) {
    return;
  }

  if (document.getElementById(WEBKIT_SCROLLBAR_STYLE_ID)) {
    return;
  }

  const style = document.createElement('style');
  style.id = WEBKIT_SCROLLBAR_STYLE_ID;
  style.textContent = [
    '.ot-log-content::-webkit-scrollbar { width: 10px; height: 10px; }',
    '.ot-log-content::-webkit-scrollbar-track { background: #1e1e1e; }',
    '.ot-log-content::-webkit-scrollbar-thumb { background: #555; border-radius: 5px; }',
    '.ot-log-content::-webkit-scrollbar-thumb:hover { background: #777; }'
  ].join('\n');
  document.head.appendChild(style);
}

// ============================================================================
// Helper to detect Dallas sensor entries via explicit type or legacy address format.
function isDallasAddress(entry) {
  if (entry == null) return false;
  if (entry.type === 'dallas') return true;

  var name = (typeof entry === 'string') ? entry : entry.name;
  if (typeof name !== 'string') return false;

  // Legacy and standard hex address formats (8/9/16 chars).
  var len = name.length;
  return (len === 8 || len === 9 || len === 16) &&
         /^[0-9A-Fa-f]+$/.test(name);
}
// Global cache for Dallas sensor labels (loaded from /dallas_labels.ini)
var dallasLabelsCache = {};

// Function to fetch Dallas sensor labels from backend
function fetchDallasLabels() {
  return fetch(APIGW + 'v2/sensors/labels')
    .then(function(response) {
      if (!response.ok) {
        console.warn('Failed to fetch Dallas labels:', response.status);
        return {};
      }
      var contentType = response.headers.get('content-type') || '';
      if (contentType.indexOf('application/json') === -1) {
        return {};
      }
      return response.json();
    })
    .then(function(labels) {
      dallasLabelsCache = labels || {};
      console.log('Dallas labels loaded:', Object.keys(dallasLabelsCache).length, 'labels');
      if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.refreshSensorLabels === 'function') {
        OTGraph.refreshSensorLabels(dallasLabelsCache);
      }
      // Update existing panel labels for sensor addresses already rendered
      for (var addr in dallasLabelsCache) {
        if (!dallasLabelsCache.hasOwnProperty(addr)) continue;
        var el = document.getElementById('otmon_' + addr);
        if (el && el.parentNode) {
          var labelSpan = el.parentNode.querySelector('.sensor-label-text');
          if (labelSpan && labelSpan.textContent !== dallasLabelsCache[addr]) {
            labelSpan.textContent = dallasLabelsCache[addr];
          }
        }
      }
      return dallasLabelsCache;
    })
    .catch(function(error) {
      console.warn('Error fetching Dallas labels:', error);
      dallasLabelsCache = {};
      return dallasLabelsCache;
    });
}

console.log(`Hash=${window.location.hash}`);
window.onload = initMainPage;

let mainPageCompatWarningShown = false;
let otLogCompatWarningShown = false;
let picSettingsRefreshTimer = null;
let picAvailable = false;  // Unknown until /api/v2/device/info confirms PIC is present
let otCommandInterfaceAvailable = false;

const PIC_SETTINGS_REFRESH_INTERVAL_MS = 3000;
const PIC_SETTINGS_CACHE_MAX_AGE_MS = 7 * 24 * 60 * 60 * 1000;

function isPageVisible() {
  return !(document.hidden || document.visibilityState === 'hidden');
}

function isMainPageActive() {
  var mainPage = document.getElementById('displayMainPage');
  return !!(mainPage && mainPage.classList.contains('active'));
}

function getPICSettingsStorageKey() {
  return 'otgwPicSettings:' + window.location.hostname;
}

function getPICSettingsCache() {
  if (!window.localStorage) {
    return null;
  }

  try {
    var raw = localStorage.getItem(getPICSettingsStorageKey());
    if (!raw) {
      return null;
    }

    var parsed = JSON.parse(raw);
    if (!parsed || typeof parsed !== 'object' || !parsed.settings || typeof parsed.settings !== 'object') {
      return null;
    }

    return parsed;
  } catch (e) {
    console.warn('Failed to read PIC settings cache:', e);
    return null;
  }
}

function savePICSettingToCache(key, value, timestampMs) {
  if (!window.localStorage) {
    return;
  }

  try {
    var cache = getPICSettingsCache() || { host: window.location.hostname, settings: {} };
    cache.host = window.location.hostname;
    cache.settings[key] = {
      value: value,
      timestamp: timestampMs
    };
    localStorage.setItem(getPICSettingsStorageKey(), JSON.stringify(cache));
  } catch (e) {
    console.warn('Failed to save PIC settings cache:', e);
  }
}

function getPICSettingFromCache(key) {
  var cache = getPICSettingsCache();
  if (!cache || !cache.settings || !cache.settings[key]) {
    return null;
  }

  var entry = cache.settings[key];
  if (!entry || typeof entry.value !== 'string' || typeof entry.timestamp !== 'number') {
    return null;
  }

  if ((Date.now() - entry.timestamp) > PIC_SETTINGS_CACHE_MAX_AGE_MS) {
    return null;
  }

  return entry;
}

function isPICSettingDiscovered(value) {
  return typeof value === 'string' && value !== '' && value !== '--';
}

function startPICsettingsRefreshTimer() {
  if (picSettingsRefreshTimer || !isPageVisible()) {
    return;
  }

  var container = document.getElementById('picSettingsSection');
  var picPage = document.getElementById('displayPICflash');
  var picPageActive = !!(picPage && picPage.classList.contains('active'));

  if (!container || !picPageActive) {
    return;
  }

  picSettingsRefreshTimer = setInterval(function() {
    if (!isPageVisible()) {
      return;
    }

    var currentContainer = document.getElementById('picSettingsSection');
    var currentPicPage = document.getElementById('displayPICflash');
    var currentPicPageActive = !!(currentPicPage && currentPicPage.classList.contains('active'));

    if (!currentContainer || !currentPicPageActive) {
      stopPICsettingsRefreshTimer();
      return;
    }

    refreshPICsettings();
  }, PIC_SETTINGS_REFRESH_INTERVAL_MS);
}

function stopPICsettingsRefreshTimer() {
  if (picSettingsRefreshTimer) {
    clearInterval(picSettingsRefreshTimer);
    picSettingsRefreshTimer = null;
  }
}

function startOTmonitorPolling() {
  if (!isMainPageActive()) {
    return;
  }
  if (!tid) {
    tid = setInterval(function () { refreshOTmonitor(); }, 1000);
  }
}

function stopOTmonitorPolling() {
  if (tid) {
    clearInterval(tid);
    tid = 0;
  }
}

function startTimeUpdates() {
  if (!timeupdate) {
    timeupdate = setInterval(function () { refreshDevTime(); refreshGatewayMode(false); }, 1000);
  }
}

function stopTimeUpdates() {
  if (timeupdate) {
    clearInterval(timeupdate);
    timeupdate = null;
  }
}

function setActivePageSection(activeId) {
  ['displayMainPage', 'displaySettingsPage', 'displayDeviceInfo', 'displayPICflash', 'displayWebhookPage', 'displaySATPage', 'displaySATSettingsPage'].forEach(function(id) {
    var section = document.getElementById(id);
    if (!section) return;
    if (id === activeId) section.classList.add('active');
    else section.classList.remove('active');
  });

  if (activeId !== 'displayPICflash') {
    stopPICsettingsRefreshTimer();
  }
  if (activeId !== 'displaySATPage' && typeof SAT !== 'undefined') {
    SAT.stop();
  }
}

document.addEventListener('visibilitychange', function () {
  if (!isPageVisible()) {
    // When a tab is merely hidden, stop polling load but keep the OT log socket alive.
    // Reload/navigation teardown is handled by pagehide/beforeunload.
    stopTimeUpdates();
    stopOTmonitorPolling();
    stopOTDStatusPolling();
    stopPICsettingsRefreshTimer();
    return;
  }
  // When tab becomes visible again, resume UI updates
  if (!flashModeActive) {
    refreshDevTime();
    refreshGatewayMode(true);
    startTimeUpdates();
    // Give any just-closed reload/navigation socket a brief head start before reconnecting.
    scheduleOTLogWebSocketInit(false, 250);
    startOTmonitorPolling();
    if (isMainPageActive()) startOTDStatusPolling();
    startPICsettingsRefreshTimer();
  }
});


var tid = 0;
var timeupdate = null; // Will be started when needed
var lastSensorSimulationState = null;
var graphRedrawTimer = null;

let gatewayModeRefreshCounter = 0;
let gatewayModeRefreshInFlight = false; // Prevents double-triggering even when force=true
let gatewayModeLastKnown = null; // "gateway" | "monitor"
const GATEWAY_MODE_REFRESH_INTERVAL = 60; // 60s max polling interval (at most once a minute)

function updateGatewayModeIndicator(value) {
  const statusEl = document.getElementById('gatewayModeStatus');
  const textEl = document.getElementById('gatewayModeText');
  if (!statusEl || !textEl) return;

  if (value === 'gateway') {
    statusEl.className = 'mode-status mode-gateway';
    textEl.textContent = 'Gateway';
  } else if (value === 'monitor') {
    statusEl.className = 'mode-status mode-monitor';
    textEl.textContent = 'Monitor';
  } else if (value === 'unavailable') {
    statusEl.className = 'mode-status mode-unknown';
    textEl.textContent = 'Unavailable';
  } else {
    statusEl.className = 'mode-status mode-unknown';
    textEl.textContent = 'Detecting...';
  }
}

// Network mode indicator (WiFi / Ethernet icon in header)
var currentNetworkMode = 'WiFi';

function updateNetworkIndicator(mode, apFallback, quality) {
  if (!mode && !apFallback) return;
  if (apFallback) mode = 'AP';
  currentNetworkMode = mode;
  var container = document.getElementById('netStatus');
  var wifiIcon = document.getElementById('netIconWifi');
  var ethIcon = document.getElementById('netIconEth');
  var textEl = document.getElementById('netStatusText');
  if (!container || !wifiIcon || !ethIcon) return;

  var isEth = (mode === 'Ethernet');
  var isAP  = (mode === 'AP');
  var cssClass = isEth ? 'net-ethernet' : (isAP ? 'net-ap' : 'net-wifi');
  container.className = 'headercolumn net-status ' + cssClass;
  var netLabel = isAP ? 'AP Fallback mode (no WiFi)' : ('Network: ' + mode);
  container.title = netLabel;
  container.setAttribute('aria-label', 'Network status: ' + (isAP ? 'AP Fallback' : mode));
  wifiIcon.classList.toggle('hidden', isEth);
  ethIcon.classList.toggle('hidden', !isEth);
  if (textEl) textEl.textContent = isAP ? 'AP MODE' : mode;

  // Signal bars: shown for WiFi only; neutral for Ethernet/AP
  var barsEl = document.getElementById('netSignalBars');
  if (barsEl) {
    var showBars = !isEth;  // hide for Ethernet (cable icon is enough)
    barsEl.classList.toggle('hidden', !showBars);
    if (showBars) {
      var q = (quality !== undefined && quality !== null) ? quality : -1;
      var tier = isAP ? 'none' : (q >= 70 ? 'good' : (q >= 40 ? 'medium' : 'poor'));
      var activeBars = isAP ? 0 : (q >= 75 ? 4 : (q >= 50 ? 3 : (q >= 25 ? 2 : 1)));
      barsEl.setAttribute('data-quality', tier);
      barsEl.title = isAP ? 'AP mode' : ('Signal: ' + (q >= 0 ? q + '%' : 'unknown'));
      var bars = barsEl.querySelectorAll('.sig-bar');
      for (var i = 0; i < bars.length; i++) {
        bars[i].classList.toggle('active', i < activeBars);
      }
    }
  }
}

function parseGatewayModeValue(modeValue) {
  if (typeof modeValue === 'boolean') return modeValue ? 'gateway' : 'monitor';
  if (typeof modeValue !== 'string') return null;

  const normalized = modeValue.trim().toLowerCase();
  if (normalized === 'gateway' || normalized === 'on' || normalized === '1' || normalized === 'true') {
    return 'gateway';
  }
  if (normalized === 'monitor' || normalized === 'off' || normalized === '0' || normalized === 'false') {
    return 'monitor';
  }
  if (normalized === 'detecting') {
    return 'detecting';
  }
  if (normalized === 'n/a') {
    return 'unavailable';
  }
  return null;
}

function formatGatewayModeDisplayValue(modeValue) {
  const parsedMode = parseGatewayModeValue(modeValue);
  if (parsedMode === 'gateway') return 'Gateway';
  if (parsedMode === 'monitor') return 'Monitor';
  if (parsedMode === 'detecting') return 'Detecting...';
  if (parsedMode === 'unavailable') return 'N/A';
  return modeValue;
}

function formatDeviceInfoLabel(key) {
  return translateToHuman(key);
}

const deviceInfoValueFormatters = {
  otgwmode: formatGatewayModeDisplayValue
};

function formatDeviceInfoValue(key, value) {
  if (typeof key !== 'string') return value;

  const normalizedKey = key.trim().toLowerCase();
  const formatter = deviceInfoValueFormatters[normalizedKey];
  if (typeof formatter === 'function') {
    return formatter(value);
  }
  return value;
}

// Apply a parsed gateway mode value to the indicator.
// 'gateway'/'monitor' → update last-known and indicator.
// 'detecting'         → show detecting, keep last-known unchanged.
// null               → fall back to last-known, or show detecting if nothing known yet.
function applyParsedGatewayMode(parsedMode) {
  if (parsedMode === 'gateway' || parsedMode === 'monitor') {
    gatewayModeLastKnown = parsedMode;
    updateGatewayModeIndicator(parsedMode);
  } else if (parsedMode === 'detecting') {
    updateGatewayModeIndicator('detecting');
  } else if (gatewayModeLastKnown) {
    updateGatewayModeIndicator(gatewayModeLastKnown);
  } else {
    updateGatewayModeIndicator('detecting');
  }
}

function refreshGatewayMode(force) {
  // In-flight check MUST occur before force check to ensure throttle has priority
  if (gatewayModeRefreshInFlight) return;
  
  if (flashModeActive || !isPageVisible()) return;
  
  if (!force && gatewayModeRefreshCounter < GATEWAY_MODE_REFRESH_INTERVAL) {
    gatewayModeRefreshCounter++;
    return;
  }

  gatewayModeRefreshCounter = 0;
  gatewayModeRefreshInFlight = true;

  fetch(APIGW + 'v2/device/info')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      const device = (json && json.device) ? json.device : {};
      applyParsedGatewayMode(parseGatewayModeValue(device.otgwmode));
      applyOTGWSimulationState(device.otgwsimulation);
    })
    .catch(error => {
      console.warn('refreshGatewayMode warning:', error);
      if (gatewayModeLastKnown) {
        updateGatewayModeIndicator(gatewayModeLastKnown);
      } else {
        updateGatewayModeIndicator('unavailable');
      }
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
  stopTimeUpdates();
  stopOTmonitorPolling();
  
  // Disconnect WebSocket
  disconnectOTLogWebSocket();
  
  console.log('Flash mode active - all polling and WebSocket activity stopped');
}

function exitFlashMode() {
  console.log('Exiting flash mode - restarting background activity');
  flashModeActive = false;
  
  // Restart time update
  startTimeUpdates();
  
  // Restart WebSocket if on main page
  if (isMainPageActive()) {
    initOTLogWebSocket();
    startOTmonitorPolling();
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
    console.log('  otgwDebug.api(endpoint)   - Test API endpoint (e.g., "v2/device/info")');
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
      const response = await fetch(APIGW + 'v2/device/info');
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      console.group('📱 Device Information');
      console.table(data.device || data);
      console.groupEnd();
    } catch (error) {
      console.error('Failed to fetch device info:', error);
    }
  },

  // Show current settings
  settings: async function() {
    try {
      const response = await fetch(APIGW + 'v2/settings');
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
    return await this.api('v2/health');
  },

  // Send OTGW command
  sendCmd: async function(cmd) {
    console.log(`📤 Sending command: ${cmd}`);
    try {
      const response = await fetch(APIGW + `v2/otgw/command/${cmd}`, {
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

// Wire otgwDebug.verbose accessor to the DEBUG_WS flag so the advertised help
// command actually works. Reads return the current flag; writes coerce to bool
// and log the transition once for confirmation.
Object.defineProperty(window.otgwDebug, 'verbose', {
  get: () => DEBUG_WS,
  set: (v) => { DEBUG_WS = !!v; console.log('[WebSocket] verbose logging =', DEBUG_WS); },
  enumerable: true,
  configurable: true,
});

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
let frozenLogStartIndex = null; // Freeze visible slice when auto-scroll is disabled
let lastRenderedStartIndex = 0;
let lastRenderedLogText = null;
let scrollToBottomScheduled = false;
let showTimestamps = true;
let searchTerm = '';
let updatePending = false;
let otLogControlsInitialized = false;
let isFlashing = false;
let currentFlashFilename = "";
let flashModeActive = false; // Track if we're on the flash page
let isPSmode = false; // Track PS=1 (Print Summary) mode from OTGW PIC
let statusMessageText = ''; // Device status message from /v2/device/time
let currentFreeHeap = null;    // Free heap bytes from /v2/device/time
let currentMaxFreeBlock = null; // Max free block bytes from /v2/device/time
let sensorSimulationActive = false; // Mirror of otmonitor.sensorsimulation for footer notice
let otgwSimulationActive = false; // Mirror of device.otgwsimulation for status badge
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
let wsConnectDelayTimer = null;
const WS_WATCHDOG_TIMEOUT = 45000; // 45 seconds timeout (allows for 30s keepalive + 15s margin)
// Per-message WebSocket console logs are gated behind this flag. It is
// file-scoped; toggle from the browser console via `otgwDebug.verbose = true`
// (wired via an accessor near the otgwDebug helper). Keeps the default console
// clean so warnings/errors are not drowned out by a torrent of MESSAGE/KEEPALIVE/
// Watchdog/Trim lines at steady state.
let DEBUG_WS = false;

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
  // Cap normal-mode buffer at 10k lines for 1.4.2: higher values stall the render hotpath
  // on restore and drag the WS watchdog down with them. Capture mode still scales with memory.
  const reasonable = Math.max(5000, Math.min(calculated, 10000)); // 5k to 10k range
  
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
      const logs = safeJSONParse(savedLogs);
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
      const prefs = safeJSONParse(savedPrefs);
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

function stopScheduledOTLogWebSocketInit() {
  if (wsConnectDelayTimer) {
    clearTimeout(wsConnectDelayTimer);
    wsConnectDelayTimer = null;
  }
}

function scheduleOTLogWebSocketInit(force, delayMs) {
  stopScheduledOTLogWebSocketInit();

  if (delayMs > 0) {
    wsConnectDelayTimer = setTimeout(function() {
      wsConnectDelayTimer = null;
      initOTLogWebSocket(force);
    }, delayMs);
    return;
  }

  initOTLogWebSocket(force);
}

function shutdownPageNetworking(reason) {
  console.log('[Lifecycle] Shutting down page networking: ' + reason);
  stopTimeUpdates();
  stopOTmonitorPolling();
  stopOTDStatusPolling();
  stopPICsettingsRefreshTimer();
  stopScheduledOTLogWebSocketInit();
  disconnectOTLogWebSocket();
}

function persistOTLogBufferForUnload() {
  if (otLogBuffer.length > 0) {
    saveDataToLocalStorage();
  }
}

window.addEventListener('pagehide', function(event) {
  persistOTLogBufferForUnload();
  shutdownPageNetworking(event && event.persisted ? 'pagehide (bfcache)' : 'pagehide');
});

// Save data and explicitly close live OT-log sockets before a full page unload/reload.
window.addEventListener('beforeunload', function() {
  persistOTLogBufferForUnload();
  shutdownPageNetworking('beforeunload');
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
  // Reset+cleared logs removed -- they fired on every inbound message and drowned
  // out real signals. Only the WATCHDOG TIMEOUT path below still logs (console.warn).
  if (wsWatchdogTimer) {
    clearTimeout(wsWatchdogTimer);
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
  const isSmallScreen = window.innerWidth <= MOBILE_BREAKPOINT_PX;

  const state = {
    isProxied: isProxied,
    isPhone: isPhone,
    isSmallScreen: isSmallScreen,
    isPSmode: isPSmode,
    sectionDisabled: (isProxied || isPhone || isSmallScreen),
    wsDisabled: (isProxied || isPhone || isSmallScreen)
  };
  
  return state;
}

//============================================================================
function updateOTLogModeNotice(displayState) {
  const noticeEl = document.getElementById('otLogModeNotice');
  if (!noticeEl) {
    return;
  }

  if (displayState && displayState.isPSmode) {
    noticeEl.textContent = PS_MODE_NOTICE_TEXT;
    noticeEl.classList.remove('hidden');
    return;
  }

  noticeEl.textContent = '';
  noticeEl.classList.add('hidden');
}

//============================================================================
function setOTLogCommandsOnly(enabled) {
  const logSection = document.getElementById('otLogSection');
  if (!logSection) {
    return;
  }

  if (enabled) {
    logSection.classList.add('commands-only');
    logSection.classList.remove('hidden');

    const logPanel = document.getElementById('Log');
    if (logPanel) {
      logPanel.classList.add('active');
    }

    const tabLinks = document.getElementsByClassName('tab-link');
    for (let i = 0; i < tabLinks.length; i++) {
      tabLinks[i].classList.remove('active');
    }

    currentTab = 'Log';
    return;
  }

  logSection.classList.remove('commands-only');
}

//============================================================================
function updateOTLogResponsiveState() {
  if (flashModeActive) return;

  const logSection = document.getElementById('otLogSection');
  if (!logSection) {
    return;
  }

  const displayState = getOTLogDisplayState();
  updateOTLogModeNotice(displayState);

  if (displayState.sectionDisabled) {
    setOTLogCommandsOnly(true);
    disconnectOTLogWebSocket();
    return;
  }

  setOTLogCommandsOnly(false);

  if (logSection.classList.contains('hidden')) {
    logSection.classList.remove('hidden');
  }

  if (displayState.wsDisabled) {
    disconnectOTLogWebSocket();
    return;
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

  if (displayState.wsDisabled && !force && !isFlashing) {
    console.log('[WebSocket] Skipping connect: display disabled');
    if (displayState.isProxied) {
      console.log("[WebSocket] FALLBACK: HTTPS reverse proxy detected. WebSocket connections not supported. Disabling OpenTherm Monitor.");
    } else if (displayState.isPhone) {
      console.log("[WebSocket] FALLBACK: Smartphone detected. Disabling OpenTherm Monitor to save resources.");
    } else if (displayState.isSmallScreen) {
      console.log("[WebSocket] FALLBACK: Small screen detected (width: " + window.innerWidth + "px). Disabling OpenTherm Monitor.");
    }
    updateOTLogModeNotice(displayState);
    const logSection = document.getElementById('otLogSection');
    if (logSection && displayState.sectionDisabled) {
      setOTLogCommandsOnly(true);
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
      // Per-message trace gated behind DEBUG_WS (set true in console to enable).
      // MESSAGE bytes= removed as redundant with MESSAGE data:.
      resetWSWatchdog();

      if (DEBUG_WS && typeof event.data === 'string') {
        if (event.data.length > 200) {
          console.log('[WebSocket] MESSAGE data (truncated): ' + event.data.substring(0, 200) + '...');
        } else {
          console.log('[WebSocket] MESSAGE data: ' + event.data);
        }
      }

      // Handle keepalive messages (don't log or add to buffer)
      if (typeof event.data === 'string' && event.data.includes('"type":"keepalive"')) {
        if (DEBUG_WS) console.log('[WebSocket] KEEPALIVE');
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
  
  var tip = 'Live WebSocket connection to the gateway for real-time OpenTherm message streaming';
  if (connected) {
    statusEl.className = 'ws-status ws-connected';
    statusTextEl.textContent = 'Connected';
  } else {
    statusEl.className = 'ws-status ws-disconnected';
    statusTextEl.textContent = 'Disconnected';
  }
  statusEl.title = tip;
  statusTextEl.title = tip;
}

function parseSimulationValue(rawValue) {
  if (typeof rawValue === 'boolean') return rawValue;
  if (typeof rawValue === 'string') {
    const normalized = rawValue.trim().toLowerCase();
    if (normalized === 'true' || normalized === 'on' || normalized === '1') return true;
    if (normalized === 'false' || normalized === 'off' || normalized === '0') return false;
  }
  return null;
}

// Hide or show PIC-only and OT-command-capable UI elements based on hardware.
// Called once after the first /api/v2/device/info response.
function applyPICAvailability(available, otCommandAvailable) {
  picAvailable = !!available;
  // otCommandAvailable is now a string: "PIC", "OT-Direct", or "None" (or legacy bool)
  otCommandInterfaceAvailable = (otCommandAvailable === "PIC" || otCommandAvailable === "OT-Direct" || otCommandAvailable === true || otCommandAvailable === "true");
  // Static HTML elements marked with class "pic-only"
  Array.from(document.getElementsByClassName('pic-only')).forEach(function(el) {
    if (picAvailable) el.classList.remove('hidden');
    else el.classList.add('hidden');
  });
  Array.from(document.getElementsByClassName('ot-command-capable')).forEach(function(el) {
    if (otCommandInterfaceAvailable) el.classList.remove('hidden');
    else el.classList.add('hidden');
  });
  // Dynamic settings rows (created by refreshSettings)
  var picSettingKeys = ['otgwcommandenable', 'otgwcommands'];
  picSettingKeys.forEach(function(key) {
    var row = document.getElementById('D_' + key);
    if (row) {
      if (otCommandInterfaceAvailable) row.classList.remove('hidden');
      else row.classList.add('hidden');
    }
  });
  // Dynamic device info rows (created by refreshDeviceInfo)
  var picDevInfoKeys = ['picavailable', 'picfwversion', 'picdeviceid', 'picfwtype'];
  picDevInfoKeys.forEach(function(key) {
    var row = document.getElementById('devinfo_' + key);
    if (row) {
      if (picAvailable) row.classList.remove('hidden');
      else row.classList.add('hidden');
    }
  });
}

// Hide or show OT-direct-only UI elements based on hardware.
// Parallel to applyPICAvailability() for OTGW32 builds.
var otDirectAvailable = false;

function applyOTDirectAvailability(available) {
  otDirectAvailable = !!available;
  Array.from(document.getElementsByClassName('otdirect-only')).forEach(function(el) {
    if (otDirectAvailable) el.classList.remove('hidden');
    else el.classList.add('hidden');
  });
  // Restore collapsed/expanded state of the OT-Direct status panel from localStorage
  // once the panel is visible. Safe to call when otDirectAvailable=false (no-ops if
  // the elements aren't in the DOM tree).
  if (otDirectAvailable) restoreOTDPanelCollapse();
  // Dynamic device info rows
  var otdDevInfoKeys = ['otdirectavailable', 'otdmode', 'otdbypass', 'otdmonitor', 'otdmaster', 'otdstepup', 'otdthermostat', 'otdsetback', 'otdschedtotal', 'otdschedactive', 'otdscheddisabled', 'otdoverrides'];
  otdDevInfoKeys.forEach(function(key) {
    var row = document.getElementById('devinfo_' + key);
    if (row) {
      if (otDirectAvailable) row.classList.remove('hidden');
      else row.classList.add('hidden');
    }
  });
}

// ---------------------------------------------------------------------------
// OT-Direct status widget — polls /api/v2/otdirect/status every 5 seconds
// ---------------------------------------------------------------------------
var _otdStatusTimer = null;

function startOTDStatusPolling() {
  if (_otdStatusTimer || !otDirectAvailable) return;
  refreshOTDStatus();
  _otdStatusTimer = setInterval(refreshOTDStatus, 5000);
}

function stopOTDStatusPolling() {
  if (_otdStatusTimer) { clearInterval(_otdStatusTimer); _otdStatusTimer = null; }
}

function refreshOTDStatus() {
  if (!otDirectAvailable) return;
  fetch(APIGW + 'v2/otdirect/status')
    .then(function(r) { return r.ok ? r.json() : null; })
    .then(function(json) {
      if (!json || !json.otdirect_status) return;
      var s = json.otdirect_status;
      var el, dot;
      function setDot(dotId, on) {
        var d = document.getElementById(dotId);
        if (d) d.className = 'sat-indicator ' + (on ? 'sat-indicator-on' : 'sat-indicator-off');
      }
      el = document.getElementById('otd-st-mode');
      if (el) el.textContent = (s.mode || '--').toUpperCase();
      el = document.getElementById('otd-st-online');
      if (el) el.textContent = s.ot_online ? 'Online' : 'Offline';
      setDot('otd-st-online-dot', s.ot_online);
      el = document.getElementById('otd-st-therm');
      if (el) el.textContent = s.thermostat_connected ? 'Connected' : 'None';
      setDot('otd-st-therm-dot', s.thermostat_connected);
      el = document.getElementById('otd-st-boiler');
      if (el) el.textContent = s.boiler ? 'Active' : 'Idle';
      setDot('otd-st-boiler-dot', s.boiler);
      el = document.getElementById('otd-st-setback');
      if (el) el.textContent = s.setback_active ? 'Active' : 'Off';
      setDot('otd-st-setback-dot', s.setback_active);
      el = document.getElementById('otd-st-stepup');
      if (el) el.textContent = s.stepup ? 'On' : 'Off';
      setDot('otd-st-stepup-dot', s.stepup);
      el = document.getElementById('otd-st-sched');
      if (el) el.textContent = (s.schedule_active || 0) + '/' + (s.schedule_total || 0);
      el = document.getElementById('otd-st-ovr');
      if (el) el.textContent = String(s.overrides_active || 0);
    })
    .catch(function() {});
}

// ---------------------------------------------------------------------------
// OT-Direct override management
// ---------------------------------------------------------------------------
function toggleOTDOverrides() {
  var sec = document.getElementById('otd-ovr-section');
  var arrow = document.getElementById('otd-ovr-arrow');
  if (!sec) return;
  var visible = sec.style.display !== 'none';
  sec.style.display = visible ? 'none' : 'block';
  if (arrow) arrow.textContent = visible ? '▼' : '▲';
  var toggleEl = document.querySelector('[aria-controls="otd-ovr-section"]');
  if (toggleEl) toggleEl.setAttribute('aria-expanded', visible ? 'false' : 'true');
  if (!visible) refreshOTDOverrides();
}

// Collapse / expand the entire OT-Direct status panel (TASK-433 follow-up).
// State persisted in localStorage so the user's preference survives reloads.
function toggleOTDPanel() {
  var body = document.getElementById('otd-panel-body');
  var arrow = document.getElementById('otd-panel-arrow');
  if (!body) return;
  var visible = body.style.display !== 'none';
  body.style.display = visible ? 'none' : '';
  if (arrow) arrow.textContent = visible ? '▶' : '▼';
  var toggleEl = document.querySelector('[aria-controls="otd-panel-body"]');
  if (toggleEl) toggleEl.setAttribute('aria-expanded', visible ? 'false' : 'true');
  try { localStorage.setItem('otd-panel-collapsed', visible ? '1' : '0'); } catch (_) {}
}

function restoreOTDPanelCollapse() {
  try {
    if (localStorage.getItem('otd-panel-collapsed') === '1') {
      var body = document.getElementById('otd-panel-body');
      var arrow = document.getElementById('otd-panel-arrow');
      var toggleEl = document.querySelector('[aria-controls="otd-panel-body"]');
      if (body) body.style.display = 'none';
      if (arrow) arrow.textContent = '▶';
      if (toggleEl) toggleEl.setAttribute('aria-expanded', 'false');
    }
  } catch (_) {}
}

function refreshOTDOverrides() {
  fetch(APIGW + 'v2/otdirect/overrides')
    .then(function(r) { return r.ok ? r.json() : null; })
    .then(function(json) {
      if (!json || !json.overrides) return;
      renderOTDOverrides(json.overrides);
    })
    .catch(function() {});
}

function renderOTDOverrides(ov) {
  var list = document.getElementById('otd-ovr-list');
  if (!list) return;
  var html = '';
  var hasAny = false;

  if (ov.write && ov.write.length) {
    ov.write.forEach(function(e) {
      hasAny = true;
      html += '<div class="otd-ovr-row"><span>Write</span><span>MsgID ' + e.msgid + '</span><span>0x' + ('0000' + e.value.toString(16)).slice(-4).toUpperCase() + '</span></div>';
    });
  }
  if (ov.response && ov.response.length) {
    ov.response.forEach(function(e) {
      hasAny = true;
      html += '<div class="otd-ovr-row"><span>SR</span><span>MsgID ' + e.msgid + '</span><span>0x' + ('0000' + e.value.toString(16)).slice(-4).toUpperCase() + '</span>';
      html += '<button onclick="clearOTDOverride(\'cr\',' + e.msgid + ')">Clear</button></div>';
    });
  }
  if (ov.modify && ov.modify.length) {
    ov.modify.forEach(function(e) {
      hasAny = true;
      html += '<div class="otd-ovr-row"><span>RM</span><span>MsgID ' + e.msgid + '</span><span>0x' + ('0000' + e.value.toString(16)).slice(-4).toUpperCase() + '</span>';
      html += '<button onclick="clearOTDOverride(\'cm\',' + e.msgid + ')">Clear</button></div>';
    });
  }
  if (ov.unknown && ov.unknown.length) {
    ov.unknown.forEach(function(id) {
      hasAny = true;
      html += '<div class="otd-ovr-row"><span>UI</span><span>MsgID ' + id + '</span><span>(Unknown ID)</span>';
      html += '<button onclick="clearOTDOverride(\'ki\',' + id + ')">Clear</button></div>';
    });
  }

  list.innerHTML = hasAny ? html : '<div class="otd-ovr-empty">No active overrides</div>';
}

function sendOTDOverride() {
  var action = document.getElementById('otd-ovr-action');
  var msgid = document.getElementById('otd-ovr-msgid');
  var value = document.getElementById('otd-ovr-value');
  if (!action || !msgid) return;
  var a = action.value;
  var m = msgid.value;
  if (m === '') return;
  var url = APIGW + 'v2/otdirect/overrides?action=' + a + '&msgid=' + m;
  if ((a === 'sr' || a === 'rm') && value && value.value !== '') {
    url += '&value=' + value.value;
  }
  fetch(url, { method: 'POST', mode: 'cors' })
    .then(function(r) { return r.ok ? r.json() : null; })
    .then(function(json) {
      if (json && json.overrides) renderOTDOverrides(json.overrides);
      if (msgid) msgid.value = '';
      if (value) value.value = '';
    })
    .catch(function(err) { console.log('OTD override error: ' + err.message); });
}

function clearOTDOverride(action, msgid) {
  fetch(APIGW + 'v2/otdirect/overrides?action=' + action + '&msgid=' + msgid, { method: 'POST', mode: 'cors' })
    .then(function(r) { return r.ok ? r.json() : null; })
    .then(function(json) {
      if (json && json.overrides) renderOTDOverrides(json.overrides);
    })
    .catch(function() {});
}

function applyOTGWSimulationState(rawValue) {
  const parsedValue = parseSimulationValue(rawValue);
  if (parsedValue === null) return;
  otgwSimulationActive = parsedValue;
  updateSimulationBadge();
}

function updateSimulationBadge() {
  const badgeEl = document.getElementById('simulationBadge');
  if (!badgeEl) return;

  const activeModes = [];
  if (otgwSimulationActive) activeModes.push('OT replay');
  if (sensorSimulationActive) activeModes.push('Dallas sensors');

  if (activeModes.length === 0) {
    badgeEl.classList.add('hidden');
    badgeEl.removeAttribute('title');
    badgeEl.setAttribute('aria-hidden', 'true');
    return;
  }

  badgeEl.textContent = 'SIMULATION';
  badgeEl.title = 'Simulation active: ' + activeModes.join(' + ');
  badgeEl.setAttribute('aria-label', badgeEl.title);
  badgeEl.setAttribute('aria-hidden', 'false');
  badgeEl.classList.remove('hidden');
}

//============================================================================
function formatLogLine(logLine) {
  if (!logLine) return "";

  // Event lines from sendEventToWebSocket (sent cmds, responses, errors, system events)
  if (logLine.isEvent) {
    const pfx = (typeof logLine.prefix === 'string') ? logLine.prefix : ' ';
    const content = (typeof logLine.label === 'string') ? logLine.label : '';
    return pfx + ' ' + content;
  }
  
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
  
  // Detect event prefix lines produced by sendEventToWebSocket:
  // Format: HH:MM:SS.mmmmmm {prefix} {content}  where prefix is >, <, !, or *
  const rest = line.substring(offset);
  const eventMatch = rest.match(/^([><!*]) (.*)/);
  if (eventMatch) {
    return {
      time: obj.time,
      isEvent: true,
      prefix: eventMatch[1],
      // Fields below kept for object shape consistency (processStatsLine/OTGraph use id/valid)
      source: '', raw: '', id: null, dir: '', valid: ' ',
      label: eventMatch[2].trim(), value: '', unit: ''
    };
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
    if (DEBUG_WS) console.log(`Trimmed ${toRemove} old log entries (limit: ${maxLogLines.toLocaleString()})`);
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

function getMainPageContainer() {
  var mainPage = document.getElementById('mainPage');
  if (mainPage) {
    return mainPage;
  }

  var displayMainPage = document.getElementById('displayMainPage');
  if (!displayMainPage) {
    return null;
  }

  mainPage = document.createElement('div');
  mainPage.id = 'mainPage';

  var waiting = document.getElementById('waiting');
  if (waiting) {
    mainPage.appendChild(waiting);
  } else {
    var placeholder = document.createElement('div');
    placeholder.id = 'waiting';
    placeholder.textContent = 'Wait for it...';
    mainPage.appendChild(placeholder);
  }

  var otLogSection = document.getElementById('otLogSection');
  if (otLogSection && otLogSection.parentNode === displayMainPage) {
    displayMainPage.insertBefore(mainPage, otLogSection);
  } else {
    var navShell = displayMainPage.querySelector('.page-nav-shell');
    if (navShell && navShell.parentNode === displayMainPage) {
      displayMainPage.insertBefore(mainPage, navShell.nextSibling);
    } else {
      displayMainPage.appendChild(mainPage);
    }
  }

  if (!mainPageCompatWarningShown) {
    console.warn('mainPage element missing; recreated compatible container');
    mainPageCompatWarningShown = true;
  }

  return mainPage;
}

function getOTLogContentElement() {
  var container = document.getElementById('otLogContent');
  if (container) {
    return container;
  }

  var logPanel = document.getElementById('Log');
  if (!logPanel) {
    return null;
  }

  var logContainer = document.getElementById('otLogContainer');
  if (!logContainer) {
    logContainer = document.createElement('div');
    logContainer.id = 'otLogContainer';
    logContainer.className = 'ot-log-container';

    var logFooter = logPanel.querySelector('.ot-log-footer');
    if (logFooter) {
      logPanel.insertBefore(logContainer, logFooter);
    } else {
      logPanel.appendChild(logContainer);
    }
  }

  container = document.createElement('div');
  container.id = 'otLogContent';
  container.className = 'ot-log-content';
  container.setAttribute('role', 'log');
  container.setAttribute('aria-live', 'polite');
  logContainer.appendChild(container);

  if (!otLogCompatWarningShown) {
    console.warn('otLogContent element missing; recreated compatible container');
    otLogCompatWarningShown = true;
  }

  return container;
}

// Internal function that performs the actual DOM update
function renderLogDisplay() {
  const container = getOTLogContentElement();
  if (!container) {
    return;
  }

  // Always show up to RENDER_LIMIT lines
  const displayCount = Math.min(otLogFilteredBuffer.length, RENDER_LIMIT);
  const maxStartIndex = Math.max(0, otLogFilteredBuffer.length - displayCount);
  let startIndex = maxStartIndex;

  if (autoScroll) {
    frozenLogStartIndex = null;
  } else {
    if (frozenLogStartIndex === null) {
      frozenLogStartIndex = maxStartIndex;
    }
    if (frozenLogStartIndex < 0) frozenLogStartIndex = 0;
    if (frozenLogStartIndex > maxStartIndex) frozenLogStartIndex = maxStartIndex;
    startIndex = frozenLogStartIndex;
  }

  const linesToShow = otLogFilteredBuffer.slice(startIndex, startIndex + displayCount);
  lastRenderedStartIndex = startIndex;

  // Build plain text: container CSS is white-space: pre, so '\n' becomes a line break.
  // textContent skips the HTML parser and per-line escaping entirely.
  const text = linesToShow.map(entry => {
    const body = formatLogLine(entry.data);
    return showTimestamps ? `${entry.time} ${body}` : body;
  }).join('\n');

  if (text !== lastRenderedLogText) {
    container.textContent = text;
    lastRenderedLogText = text;
  }

  // Defer scroll-to-bottom to the next frame so scrollHeight is read after paint,
  // not synchronously in the same frame as the textContent write (avoids forced reflow).
  if (autoScroll && !scrollToBottomScheduled) {
    scrollToBottomScheduled = true;
    requestAnimationFrame(() => {
      scrollToBottomScheduled = false;
      if (autoScroll) {
        container.scrollTop = container.scrollHeight;
      }
    });
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

function clearLogBuffer() {
  otLogBuffer = [];
  otLogFilteredBuffer = [];
  try { if (window.localStorage) localStorage.removeItem(PERSISTENCE_KEY_LOGS); } catch(e) {}
  updateLogDisplay();
  updateLogCounters();
}

//============================================================================
function setupOTLogControls() {
  // Only setup event listeners once to prevent duplicates
  if (otLogControlsInitialized) {
    return;
  }
  
  // Auto-scroll checkbox
  const chkAutoScroll = document.getElementById('chkAutoScroll');
  if (chkAutoScroll) {
    chkAutoScroll.addEventListener('change', function() {
      autoScroll = this.checked;
      if (autoScroll) {
        frozenLogStartIndex = null;
        updateLogDisplay();
      } else {
        frozenLogStartIndex = lastRenderedStartIndex;
      }
      if (typeof saveUISetting === 'function') saveUISetting('ui_autoscroll', autoScroll);
    });
  }

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
      if (typeof saveUISetting === 'function') saveUISetting('ui_capture', e.target.checked);
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
  const btnClearLog = document.getElementById('btnClearLog');
  if (btnClearLog) {
    btnClearLog.addEventListener('click', function(e) {
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
  }
  
  // Download log
  const btnDownloadLog = document.getElementById('btnDownloadLog');
  if (btnDownloadLog) {
    btnDownloadLog.addEventListener('click', function(e) {
      e.preventDefault();
      try {
        downloadLog(false);
      } catch (err) {
        console.error('Failed to download log:', err);
      }
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
  const searchLog = document.getElementById('searchLog');
  if (searchLog) {
    searchLog.addEventListener('input', function(e) {
      searchTerm = e.target.value;
      updateFilteredBuffer();
      updateLogDisplay();
      updateLogCounters();
    });
  }
  
  // Toggle timestamps
  const chkShowTimestamp = document.getElementById('chkShowTimestamp');
  if (chkShowTimestamp) {
    chkShowTimestamp.addEventListener('change', function(e) {
      showTimestamps = e.target.checked;
      updateLogDisplay();
      if (typeof saveUISetting === 'function') saveUISetting('ui_timestamps', e.target.checked);
    });
  }
  
  // Manual scroll detection (disable auto-scroll checkbox if user scrolls up)
  let manualScrollTimeout = null;
  const otLogContent = getOTLogContentElement();
  if (otLogContent) {
    otLogContent.addEventListener('scroll', function(e) {
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
          frozenLogStartIndex = lastRenderedStartIndex;
          const chk = document.getElementById('chkAutoScroll');
          if (chk) chk.checked = false;
        }
      }, 100);
    });
  }
  
  // Mark as initialized after all listeners are successfully registered
  otLogControlsInitialized = true;
  updateLogCounters();

  // Command input bar - send one-shot OTGW commands
  const otCmdInput = document.getElementById('otCmdInput');
  const btnSendCmd = document.getElementById('btnSendCmd');
  if (btnSendCmd && otCmdInput) {
    btnSendCmd.addEventListener('click', function() {
      sendOTGWcommand(otCmdInput.value);
    });
    otCmdInput.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') {
        e.preventDefault();
        sendOTGWcommand(otCmdInput.value);
      }
    });
  }
}

//============================================================================
// Send a one-shot command to the OTGW PIC via the REST API
//============================================================================
let statusClearTimer = null;

function normalizeOTGWcommand(cmd) {
  var trimmedCmd = (cmd || '').trim();
  if (!trimmedCmd) return '';

  var normalizedCmd = trimmedCmd.replace(/\s*=\s*/g, '=');
  var spacedCommandMatch = normalizedCmd.match(/^([A-Za-z]{2})\s+(.+)$/);
  if (spacedCommandMatch) {
    normalizedCmd = spacedCommandMatch[1] + '=' + spacedCommandMatch[2].trim();
  }

  var compactMatch = normalizedCmd.match(/^([A-Za-z]{2})=(.+)$/);
  if (compactMatch) {
    return compactMatch[1].toUpperCase() + '=' + compactMatch[2].trim();
  }

  return normalizedCmd;
}

function sendOTGWcommand(cmd) {
  var trimmedCmd = (cmd || '').trim();
  var normalizedCmd = normalizeOTGWcommand(trimmedCmd);
  var statusEl = document.getElementById('otCmdStatus');

  function clearStatus(delay) {
    clearTimeout(statusClearTimer);
    statusClearTimer = setTimeout(function() {
      var el = document.getElementById('otCmdStatus');
      if (el) { el.textContent = ''; el.className = 'ot-cmd-status'; }
    }, delay);
  }

  if (!trimmedCmd) {
    if (statusEl) {
      statusEl.textContent = 'Enter a command first';
      statusEl.className = 'ot-cmd-status ot-cmd-error';
      clearStatus(2000);
    }
    return;
  }

  if (!/^[A-Z]{2}=.+$/.test(normalizedCmd)) {
    if (statusEl) {
      statusEl.textContent = 'Use XX=value, e.g. PS=1 or TT=20.5';
      statusEl.className = 'ot-cmd-status ot-cmd-error';
      clearStatus(3500);
    }
    return;
  }

  fetch(`${APIGW}v2/otgw/commands`, {
    method: 'POST',
    headers: { 'content-type': 'application/json; charset=UTF-8' },
    body: JSON.stringify({ command: normalizedCmd })
  })
  .then(function(response) {
    if (!response.ok) {
      return response.text()
        .catch(function(textErr) {
          console.error('Failed to read error response body:', textErr);
          return '';
        })
        .then(function(text) {
          throw new Error('HTTP ' + response.status + (text ? ': ' + text.trim() : ''));
        });
    }
    return response.json();
  })
  .then(function(data) {
    if (statusEl) {
      statusEl.textContent = 'Queued: ' + normalizedCmd;
      statusEl.className = 'ot-cmd-status ot-cmd-ok';
      clearStatus(3000);
    }
    var inputEl = document.getElementById('otCmdInput');
    if (inputEl) inputEl.value = '';
  })
  .catch(function(err) {
    console.error('Command failed:', err);
    if (statusEl) {
      statusEl.textContent = 'Error: ' + err.message;
      statusEl.className = 'ot-cmd-status ot-cmd-error';
    }
  });
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
        if (displayEl) displayEl.classList.remove('hidden');
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
  if (displayEl) displayEl.classList.add('hidden');

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
  // Deprecated wrapper kept for compatibility with older call sites.
  // The canonical loader is loadPersistentUI() using ui_* setting names.
  loadPersistentUI();
}

function saveUISetting(field, value) {
  // Wrapper to ensure we use the backend API with canonical ui_* setting names.
  console.log("Saving UI Setting [" + field + "] = " + value);
  sendPostSetting(field, value);
}

function renderSharedPageNavShell() {
  var template = document.getElementById('pageNavTemplate');
  if (!template) return;

  Array.from(document.getElementsByClassName('page-nav-shell')).forEach(function (slot) {
    if (!slot || slot.dataset.rendered === '1') return;

    if (template.content && typeof template.content.cloneNode === 'function') {
      slot.appendChild(template.content.cloneNode(true));
    } else {
      // Fallback for older browsers: clone via a temporary container.
      var wrapper = document.createElement('div');
      wrapper.innerHTML = template.innerHTML;
      while (wrapper.firstChild) {
        slot.appendChild(wrapper.firstChild);
      }
    }

    slot.dataset.rendered = '1';
  });
}

//============================================================================
function updateThemeToggle() {
  var isDark = (localStorage.getItem('otgw-theme') || localStorage.getItem('theme')) === 'dark';
  var icon  = isDark ? '\u2600\uFE0E' : '\u263D\uFE0E';   // ☀︎ sun  or  ☽︎ moon (text presentation, not emoji)
  var title = isDark ? 'Switch to light theme' : 'Switch to dark theme';
  document.querySelectorAll('.theme-toggle-btn').forEach(function(btn) {
    btn.textContent = icon;
    btn.title       = title;
  });
  // TASK-435 Patch A: mirror the theme to body.dark + html[data-theme] so
  // the new design-system rules in components.css (which target body.dark)
  // pick up the same theme as the legacy stylesheet swap. Idempotent.
  document.body.classList.toggle('dark', isDark);
  document.documentElement.setAttribute('data-theme', isDark ? 'dark' : 'light');
}

//============================================================================
function initMainPage() {
  console.log("initMainPage()");
  ensureWebkitScrollbarStyles();

  // Check if we're in flash mode (from sessionStorage)
  try {
    if (sessionStorage.getItem('flashMode') === 'true') {
      console.log('Flash mode detected from sessionStorage');
      flashModeActive = true;
    }
  } catch(e) { /* ignore */ }

  renderSharedPageNavShell();
  updateThemeToggle();

  // theme-toggle.js is the canonical toggle; listen for its event to handle
  // side-effects: chart re-theme, server persistence, settings checkbox sync.
  document.addEventListener('theme:changed', function(e) {
    var isDark = (e.detail && e.detail.mode === 'dark');
    // Keep 'theme' key in sync so applyTheme() and the pre-paint script can
    // read it regardless of which key was written first.
    try { localStorage.setItem('theme', isDark ? 'dark' : 'light'); } catch (_) {}
    // TASK-462: re-register otgw-light/otgw-dark with the freshly-read CSS
    // vars BEFORE charts re-init, so echarts.init(c, 'otgw-dark') resolves
    // to the correct token-driven theme rather than ECharts default.
    if (typeof window.registerOtgwThemes === 'function') window.registerOtgwThemes();
    if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.setTheme === 'function') {
      OTGraph.setTheme(isDark ? 'dark' : 'light');
    }
    if (typeof SAT !== 'undefined' && SAT && typeof SAT.setTheme === 'function') {
      SAT.setTheme(isDark ? 'dark' : 'light');
    }
    var cb = document.getElementById('darktheme');
    if (cb) cb.checked = isDark;
    fetch(APIGW + 'v2/settings', {
      method: 'POST', mode: 'cors',
      headers: { 'content-type': 'application/json; charset=UTF-8' },
      body: JSON.stringify({ name: 'darktheme', value: String(isDark) })
    }).catch(function(err) { console.warn('Theme save failed:', err.message); });
  });

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
        if (document.getElementById("displayWebhookPage").classList.contains('active')) {
          saveWebhookSettings();
        } else {
          saveSettings();
        }
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
  Array.from(document.getElementsByClassName('tabWebhook')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        webhookPage();
        toggleHidden('adv_dropdown', true);
        toggleHidden('btnSaveSettings', true);
      });
    }
  );
  Array.from(document.getElementsByClassName('tabSAT')).forEach(
    function (el, idx, arr) {
      el.addEventListener('click', function () {
        satPage();
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

  function startMainPage() {
    if (window.location.hash == "#tabPICflash") {
      // Must resolve PIC availability before routing to the PIC flash page.
      // picAvailable defaults to false; fetch device info first.
      fetch(APIGW + 'v2/device/info')
        .then(function(r) { return r.ok ? r.json() : Promise.reject(r.statusText); })
        .then(function(json) {
          var d = json.device || {};
          applyPICAvailability(d.picavailable, d.otcommandinterface);
          applyOTDirectAvailability(d.otdirectavailable);
          if (picAvailable) {
            firmwarePage();
          } else {
            showMainPage();
          }
        })
        .catch(function() { showMainPage(); });
    } else {
      showMainPage();
    }
  }

  // Fetch Dallas sensor labels on page load before initial render
  fetchDallasLabels()
    .then(function() {
      console.log('Dallas labels cache initialized');
    })
    .catch(function() {
      console.warn('Dallas labels cache init failed, continuing');
    })
    .then(function() {
      startMainPage();
    });

  // Start time updates and populate navbar (version, hostname) immediately,
  // in parallel with the Dallas labels fetch below, so the header is not
  // blank for the full 2-RTT chain (labels → device/info) on slow devices.
  if (!flashModeActive && !timeupdate) {
    refreshDevTime();
    startTimeUpdates();
  }
  refreshDevInfo();

  // Check filesystem/firmware hash match once on page load
  checkFSMismatch();

  // startMainPage() is called after labels are loaded (or failed)
} // initMainPage()

//============================================================================
function checkFSMismatch() {
  fetch(APIGW + 'v2/filesystem/hash-check')
    .then(function(r) { return r.ok ? r.json() : Promise.reject(new Error('HTTP ' + r.status)); })
    .then(function(data) {
      var fc = data && data.filesystem_check;
      var banner = document.getElementById('fs-mismatch-banner');
      if (!banner || !fc) return;
      if (!fc.match) {
        // Build the banner content with a link to the flash utility
        while (banner.firstChild) banner.removeChild(banner.firstChild);
        var icon = document.createTextNode('\u26a0\ufe0f ');
        banner.appendChild(icon);
        var msg = document.createTextNode(
          'Firmware/filesystem mismatch (firmware: ' + fc.fw_hash +
          ', filesystem: ' + (fc.fs_hash || 'unknown') + '). '
        );
        banner.appendChild(msg);
        var link = document.createElement('a');
        link.href = '/update';
        link.textContent = 'Flash the matching LittleFS to fix this.';
        banner.appendChild(link);
        banner.classList.remove('hidden');
      }
    })
    .catch(function() {}); // silently ignore — device may not be reachable yet
}

function showMainPage() {
  console.log("showMainPage()");
  stopOTmonitorPolling();
  stopOTDStatusPolling();
  if (typeof SAT !== 'undefined') SAT.stop();

  // Exit flash mode if it was active
  if (flashModeActive) {
    exitFlashMode();
  }

  refreshDevTime();

  setActivePageSection('displayMainPage');

  refreshDevInfo();
  refreshOTmonitor();
  
  if (!flashModeActive) {
    startOTmonitorPolling();
    startOTDStatusPolling();
    // Delay reconnect slightly so a rapid reload can retire the previous page's socket first.
    scheduleOTLogWebSocketInit(false, 250);
  }
}

function firmwarePage() {
  stopOTDStatusPolling();
  initOTLogWebSocket();
  stopOTmonitorPolling();
  refreshDevTime();
  refreshFirmware();
  setActivePageSection('displayPICflash');
} // firmwarePage()

function deviceinfoPage() {
  disconnectOTLogWebSocket();
  stopOTmonitorPolling();
  stopOTDStatusPolling();
  refreshDevTime();
  refreshDeviceInfo();
  refreshCrashLogInfo();
  setActivePageSection('displayDeviceInfo');

} // deviceinfoPage()

function settingsPage() {
  disconnectOTLogWebSocket();
  stopOTmonitorPolling();
  stopOTDStatusPolling();
  refreshDevTime();
  setActivePageSection('displaySettingsPage');
  var msgEl = document.getElementById("settingMessage");
  if (msgEl) { msgEl.textContent = "Loading settings\u2026"; msgEl.className = "loading"; }
  refreshSettings();

} // settingsPage()

function webhookPage() {
  disconnectOTLogWebSocket();
  stopOTDStatusPolling();
  clearInterval(tid);
  refreshDevTime();
  document.getElementById("displayMainPage").classList.remove('active');
  document.getElementById("displayDeviceInfo").classList.remove('active');
  document.getElementById("displayPICflash").classList.remove('active');
  document.getElementById("displaySettingsPage").classList.remove('active');
  refreshWebhookPage();
  document.getElementById("displayWebhookPage").classList.add('active');

} // webhookPage()

function satPage() {
  disconnectOTLogWebSocket();
  stopOTmonitorPolling();
  stopOTDStatusPolling();
  refreshDevTime();
  setActivePageSection('displaySATPage');
  if (typeof SAT !== 'undefined') {
    SAT.start();
  }
} // satPage()

function satSettingsPage() {
  disconnectOTLogWebSocket();
  stopOTmonitorPolling();
  stopOTDStatusPolling();
  refreshDevTime();
  setActivePageSection('displaySATSettingsPage');
  refreshSATSettings();
} // satSettingsPage()

// ============================================================================
// SAT Settings Page — load, render, save
// ============================================================================

// Definition of all SAT settings grouped in 12 categories.
// Fields: key (API setting name), label, type ('b'|'f'|'i'|'s'|'select'),
//         unit (optional string), min, max, step (for numbers),
//         options (for select: [[value, label], ...])
// Keys use the lowercase form returned by GET /api/v2/settings.
// updateSetting() on the firmware accepts case-insensitive names so saving back works fine.
var SAT_SETTINGS_GROUPS = [
  {
    id: 'sat-grp-core',
    title: 'Core Control',
    fields: [
      { key: 'satenabled',     label: 'SAT Enabled',           type: 'b' },
      { key: 'satsystem',      label: 'Heating System',        type: 'select',
        options: [[0,'Gas / Radiators'],[1,'Heat Pump'],[2,'Hybrid'],[3,'Underfloor']] },
      { key: 'satmanufacturer',label: 'Manufacturer',          type: 'select',
        options: [[0,'Generic'],[1,'Intergas'],[2,'Remeha'],[3,'Nefit'],[4,'Vaillant'],[5,'Bosch'],[6,'Worcester'],[7,'Baxi'],[8,'Ideal'],[9,'Atag']] },
      { key: 'satcoefficient', label: 'Heating Curve',         type: 'f', min: 0.1, max: 5.0, step: 0.1 },
      { key: 'satdeadband',    label: 'Deadband',              type: 'f', unit: '\u00B0C', min: 0.05, max: 2.0, step: 0.05 },
      { key: 'satinterval',    label: 'Control Interval',      type: 'i', unit: 's', min: 30, max: 600, step: 10 },
      { key: 'satheatingmode', label: 'Heating Mode',          type: 'select',
        options: [[0,'Comfort'],[1,'Eco']] },
      { key: 'sattargettemp',  label: 'Target Temperature',    type: 'f', unit: '\u00B0C', min: 5.0, max: 30.0, step: 0.5 },
      { key: 'sattempstep',    label: 'Temp Adjust Step',      type: 'f', unit: '\u00B0C', min: 0.1, max: 1.0, step: 0.1 }
    ]
  },
  {
    id: 'sat-grp-pid',
    title: 'PID Tuning',
    fields: [
      { key: 'satpwmautoswitch',  label: 'Auto Gains',         type: 'b' },
      { key: 'satautogains',      label: 'Auto Gains Value',   type: 'f', min: 0.1, max: 10.0, step: 0.1 },
      { key: 'satovershootmargin',label: 'Overshoot Margin',   type: 'f', unit: '\u00B0C', min: 0.5, max: 5.0, step: 0.1 }
    ]
  },
  {
    id: 'sat-grp-pwm',
    title: 'PWM',
    fields: [
      { key: 'satforcepwm',       label: 'Force PWM',          type: 'b' },
      { key: 'satcyclesperhour',  label: 'Cycles per Hour',    type: 'i', min: 2, max: 6, step: 1 },
      { key: 'satmaxmodulation',  label: 'Max Rel. Modulation',type: 'i', unit: '%', min: 0, max: 100, step: 1 }
    ]
  },
  {
    id: 'sat-grp-tempsrc',
    title: 'Temperature Sources',
    fields: [
      { key: 'satexternaltemp',   label: 'Use External Temp',  type: 'b' },
      { key: 'satsensormaxage',   label: 'Sensor Max Age',     type: 'i', unit: 's', min: 60, max: 86400, step: 60 },
      { key: 'satbleenable',      label: 'BLE Enable',         type: 'b' },
      // TASK-508: legacy field. Beheerd via het BLE Sensoren paneel
      // hierboven (zelf-ontdekking + selecteren). Readonly om
      // dubbele bewerking te voorkomen.
      { key: 'satblemac',         label: 'BLE MAC Address',    type: 's', maxlen: 18, size: 20, readonly: true },
      { key: 'satbleinterval',    label: 'BLE Interval',       type: 'i', unit: 's', min: 10, max: 300, step: 5 }
    ]
  },
  {
    id: 'sat-grp-presets',
    title: 'Presets',
    fields: [
      { key: 'satpresetcomfort',  label: 'Comfort',            type: 'f', unit: '\u00B0C', min: 15.0, max: 28.0, step: 0.5 },
      { key: 'satpreseteco',      label: 'Eco',                type: 'f', unit: '\u00B0C', min: 10.0, max: 22.0, step: 0.5 },
      { key: 'satpresetaway',     label: 'Away',               type: 'f', unit: '\u00B0C', min: 5.0,  max: 18.0, step: 0.5 },
      { key: 'satpresetsleep',    label: 'Sleep',              type: 'f', unit: '\u00B0C', min: 5.0,  max: 25.0, step: 0.5 },
      { key: 'satpresetactivity', label: 'Activity',           type: 'f', unit: '\u00B0C', min: 5.0,  max: 20.0, step: 0.5 },
      { key: 'satpresethome',     label: 'Home',               type: 'f', unit: '\u00B0C', min: 10.0, max: 25.0, step: 0.5 }
    ]
  },
  {
    id: 'sat-grp-dhw',
    title: 'DHW (Domestic Hot Water)',
    fields: [
      { key: 'satdhwenabled',     label: 'DHW Enabled',        type: 'b' },
      { key: 'satdhwsetpoint',    label: 'DHW Setpoint',       type: 'f', unit: '\u00B0C', min: 0.0, max: 60.0, step: 0.5 }
    ]
  },
  {
    id: 'sat-grp-pressure',
    title: 'Pressure',
    fields: [
      { key: 'satminpressure',    label: 'Min Pressure',       type: 'f', unit: 'bar', min: 0.0, max: 3.0, step: 0.1 },
      { key: 'satmaxpressure',    label: 'Max Pressure',       type: 'f', unit: 'bar', min: 1.0, max: 4.0, step: 0.1 },
      { key: 'satmaxpressdrop',   label: 'Max Pressure Drop',  type: 'f', unit: 'bar', min: 0.05, max: 2.0, step: 0.05 }
    ]
  },
  {
    id: 'sat-grp-smart',
    title: 'Smart Features',
    fields: [
      { key: 'satsolargain',       label: 'Solar Gain Enable',      type: 'b' },
      { key: 'satsolarminrise',    label: 'Solar Min Rise Rate',    type: 'f', unit: '\u00B0C/hr', min: 0.1, max: 5.0, step: 0.1 },
      { key: 'satsolaroffset',     label: 'Solar Setpoint Offset',  type: 'f', unit: '\u00B0C', min: 0.5, max: 10.0, step: 0.5 },
      { key: 'satsolarminelev',    label: 'Solar Min Elevation',    type: 'f', unit: '\u00B0', min: -10.0, max: 45.0, step: 1.0 },
      { key: 'satsolarfreezeint',  label: 'Solar Freeze Integral',  type: 'b' },
      { key: 'satsummersimmer',    label: 'Summer Simmer Enable',   type: 'b' },
      { key: 'satsummerthreshold', label: 'Summer Threshold',       type: 'f', unit: '\u00B0C', min: 5.0, max: 35.0, step: 0.5 },
      { key: 'satsummerminhours',  label: 'Summer Min Hours',       type: 'i', unit: 'h', min: 1, max: 48, step: 1 },
      { key: 'satcomfortadjust',   label: 'Thermal Comfort Enable', type: 'b' },
      { key: 'satcomforthumidity', label: 'Comfort Humidity',       type: 'f', unit: '%', min: 10.0, max: 90.0, step: 1.0 },
      { key: 'satcomfortmaxoffset',label: 'Comfort Max Offset',     type: 'f', unit: '\u00B0C', min: 0.0, max: 3.0, step: 0.1 },
      { key: 'satmultiarea',       label: 'Multi-Area Enable',      type: 'b' },
      { key: 'satmultiareacount',  label: 'Multi-Area Count',       type: 'i', min: 0, max: 4, step: 1 },
      { key: 'satareaweight0',     label: 'Area 0 Weight',          type: 'f', min: 0.0, max: 10.0, step: 0.1 },
      { key: 'satareaweight1',     label: 'Area 1 Weight',          type: 'f', min: 0.0, max: 10.0, step: 0.1 },
      { key: 'satareaweight2',     label: 'Area 2 Weight',          type: 'f', min: 0.0, max: 10.0, step: 0.1 },
      { key: 'satareaweight3',     label: 'Area 3 Weight',          type: 'f', min: 0.0, max: 10.0, step: 0.1 },
      { key: 'satthermalcoeff',    label: 'Thermal Coefficient',    type: 'f', min: 0.005, max: 0.3, step: 0.005 }
    ]
  },
  {
    id: 'sat-grp-safety',
    title: 'Safety',
    fields: [
      { key: 'satwindowdetect',  label: 'Window Detection',       type: 'b' },
      { key: 'satwindowminsec',  label: 'Window Min Open Time',   type: 'i', unit: 's', min: 10, max: 600, step: 10 },
      { key: 'satpushsetpoint',  label: 'Push Setpoint',          type: 'b' },
      { key: 'satovpenabled',    label: 'OVP Enabled',            type: 'b' },
      { key: 'satovpvalue',      label: 'OVP Value',              type: 'f', unit: '\u00B0C', min: 0.0, max: 90.0, step: 0.5 }
    ]
  },
  {
    id: 'sat-grp-energy',
    title: 'Energy',
    fields: [
      { key: 'satboilercapacity', label: 'Boiler Capacity', type: 'f', unit: 'kW', min: 1.0, max: 100.0, step: 0.5 }
    ]
  },
  {
    id: 'sat-grp-weather',
    title: 'Weather',
    fields: [
      { key: 'satweatherenable',   label: 'Weather Enable',         type: 'b' },
      { key: 'satweatherlat',      label: 'Latitude',               type: 'f', min: -90.0, max: 90.0, step: 0.0001 },
      { key: 'satweatherlon',      label: 'Longitude',              type: 'f', min: -180.0, max: 180.0, step: 0.0001 },
      // TASK-511: hard floor at 900 s (15 min) per OWM free-tier rate-limit (1000 calls/day).
      { key: 'satweatherinterval', label: 'Fetch Interval',         type: 'i', unit: 's', min: 900, max: 3600, step: 60 },
      { key: 'satweatherapikey',   label: 'OWM API Key',            type: 's', maxlen: 64, size: 40, hidden: true }
    ]
  },
  {
    id: 'sat-grp-sync',
    title: 'Sync',
    fields: [
      { key: 'satpresetsync',       label: 'Preset Sync Enable', type: 'b' },
      { key: 'satpresetsynctopic',  label: 'Preset Sync Topic',  type: 's', maxlen: 64, size: 40 }
    ]
  },
  {
    id: 'sat-grp-advanced',
    title: 'Advanced',
    fields: [
      { key: 'satsimulation',    label: 'Simulation Mode',       type: 'b' },
      { key: 'satsimheatrate',   label: 'Sim Heat Rate',         type: 'f', unit: '\u00B0C/min', min: 0.01, max: 5.0, step: 0.01 },
      { key: 'satsimcoolrate',   label: 'Sim Cool Rate',         type: 'f', unit: '\u00B0C/min', min: 0.01, max: 5.0, step: 0.01 },
      { key: 'satautotune',      label: 'Auto-Tune',             type: 'b' },
      { key: 'satautotunerate',  label: 'Auto-Tune Rate',        type: 'f', min: 0.005, max: 0.1, step: 0.005 },
      { key: 'saterrormon',      label: 'Error Monitoring',      type: 'b' },
      { key: 'satflameoffset',   label: 'Flame Off Offset',      type: 'f', unit: '\u00B0C', min: 0.0, max: 5.0, step: 0.1 },
      { key: 'satflowoffset',    label: 'Flow Offset',           type: 'f', unit: '\u00B0C', min: 0.5, max: 10.0, step: 0.1 },
      { key: 'satmodsupoffset',  label: 'Mod Suppression Offset',type: 'f', unit: '%', min: 0.0, max: 5.0, step: 0.1 },
      { key: 'satmodsupdelay',   label: 'Mod Suppression Delay', type: 'f', unit: 's', min: 0.0, max: 120.0, step: 1.0 },
      { key: 'satvalveoffset',   label: 'Valve Offset',          type: 'f', min: -1.0, max: 1.0, step: 0.1 }
    ]
  }
];

function toggleSATSettingsGroup(headerId) {
  var header = document.getElementById(headerId);
  if (!header) return;
  var body = header.nextElementSibling;
  if (!body) return;
  var collapsed = body.style.display === 'none';
  body.style.display = collapsed ? '' : 'none';
  var arrow = header.querySelector('.sat-settings-arrow');
  if (arrow) arrow.innerHTML = collapsed ? '&#9660;' : '&#9654;';
  var toggleBtn = header.querySelector('.sat-settings-group-toggle');
  if (toggleBtn) toggleBtn.setAttribute('aria-expanded', collapsed ? 'true' : 'false');
}

function refreshSATSettings() {
  var page = document.getElementById('satSettingsContent');
  var msgEl = document.getElementById('satSettingsMessage');
  if (!page) return;
  if (msgEl) { msgEl.textContent = 'Loading\u2026'; msgEl.className = 'loading'; }

  fetch(APIGW + 'v2/settings')
    .then(function(response) {
      if (!response.ok) throw new Error('HTTP ' + response.status);
      return response.json();
    })
    .then(function(json) {
      var data = json.settings || {};
      if (msgEl) { msgEl.textContent = ''; msgEl.className = ''; }

      // Build the groups if not yet rendered
      if (!page.hasChildNodes()) {
        // TASK-508: BLE sensors panel goes ABOVE the generic groups so
        // self-discovery + selection is the first thing the user sees.
        buildBleSensorsPanel(page);
        buildSATSettingsGroups(page, data);
      } else {
        // Just update values
        populateSATSettingsValues(data);
      }
      // TASK-508: refresh roster on every poll (5s cadence via fetchSATSettings).
      refreshBleRoster();
    })
    .catch(function(error) {
      if (msgEl) { msgEl.textContent = 'Error loading settings: ' + error.message; msgEl.className = 'error'; }
    });
}

// TASK-508: BLE Sensors discovery panel \u2014 injected into #satSettingsContent
// before the generic settings groups. Reuses .sat-settings-group* and
// .sat-grid / .sat-row / .sat-label / .sat-value / .sat-btn classes per
// ADR-091 design-system drift gate. No new CSS classes introduced.
// All DOM nodes built via createElement + textContent for XSS safety \u2014
// no innerHTML on user-controlled fields.
function buildBleSensorsPanel(page) {
  var grpDiv = document.createElement('div');
  grpDiv.className = 'sat-settings-group';
  grpDiv.id = 'sat-grp-ble-sensors';

  var header = document.createElement('div');
  header.className = 'sat-settings-group-header';
  header.id = 'sat-grp-ble-sensors-header';

  var toggleBtn = document.createElement('button');
  toggleBtn.type = 'button';
  toggleBtn.className = 'sat-settings-group-toggle';
  toggleBtn.setAttribute('aria-expanded', 'true');
  toggleBtn.setAttribute('aria-controls', 'sat-grp-ble-sensors-body');
  var arrow = document.createElement('span');
  arrow.className = 'sat-settings-arrow';
  arrow.textContent = '\u25bc';   // \u25bc
  toggleBtn.appendChild(arrow);
  toggleBtn.appendChild(document.createTextNode(' BLE Sensoren'));
  toggleBtn.addEventListener('click', function() {
    toggleSATSettingsGroup('sat-grp-ble-sensors-header');
  }, false);
  header.appendChild(toggleBtn);

  var body = document.createElement('div');
  body.className = 'sat-settings-group-body';
  body.id = 'sat-grp-ble-sensors-body';

  var grid = document.createElement('div');
  grid.className = 'sat-grid';

  // Active-sensor row
  var activeRow = document.createElement('div');
  activeRow.className = 'sat-row';
  var activeLab = document.createElement('span');
  activeLab.className = 'sat-label';
  activeLab.textContent = 'Actieve sensor';
  var activeVal = document.createElement('span');
  activeVal.className = 'sat-value';
  activeVal.id = 'ble-active';
  activeVal.textContent = '--';
  activeRow.appendChild(activeLab);
  activeRow.appendChild(activeVal);

  // Roster list container (rows added by renderBleRoster)
  var listDiv = document.createElement('div');
  listDiv.id = 'ble-roster-list';

  // Roster-full warning row (hidden by default)
  var warnRow = document.createElement('div');
  warnRow.className = 'sat-row';
  warnRow.id = 'ble-roster-warning';
  warnRow.hidden = true;
  var warnLab = document.createElement('span');
  warnLab.className = 'sat-label';
  warnLab.textContent = 'Waarschuwing';
  var warnVal = document.createElement('span');
  warnVal.className = 'sat-value';
  warnVal.textContent = 'Roster vol (8/8). Vergeet onbenutte sensors om nieuwe toe te voegen.';
  warnRow.appendChild(warnLab);
  warnRow.appendChild(warnVal);

  grid.appendChild(activeRow);
  grid.appendChild(listDiv);
  grid.appendChild(warnRow);
  body.appendChild(grid);

  grpDiv.appendChild(header);
  grpDiv.appendChild(body);
  page.appendChild(grpDiv);
}

// TASK-508: pull /api/v2/sat/ble/discovery and render the roster.
// Tolerant of 404 (BLE not enabled / firmware without endpoint).
function refreshBleRoster() {
  fetch(APIGW + 'v2/sat/ble/discovery')
    .then(function(r) {
      if (r.status === 404 || r.status === 503) return null;
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json();
    })
    .then(function(j) {
      if (!j) return;
      renderBleRoster(j);
    })
    .catch(function(e) { console.warn('[BLE] discovery fetch:', e); });
}

// MAC strict-validate: 17 chars, AA:BB:CC:DD:EE:FF only. Defense-in-depth
// before injecting MACs into element ids and event handlers.
function bleIsValidMac(mac) {
  return typeof mac === 'string' && /^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$/.test(mac);
}

// Build one roster slot row pair. Returns a DocumentFragment.
function bleRowFor(s) {
  var mac = s.mac.toUpperCase();
  var temp  = (typeof s.temp === 'number') ? s.temp.toFixed(2) : '--';
  var rssi  = (typeof s.rssi === 'number') ? s.rssi : '--';
  var ageStr;
  if (s.valid && typeof s.age_ms === 'number') {
    ageStr = (s.age_ms < 60000)
             ? Math.round(s.age_ms / 1000) + 's oud'
             : Math.round(s.age_ms / 60000) + 'm oud';
  } else {
    ageStr = 'stale';
  }
  var frag = document.createDocumentFragment();

  // Header row: MAC + temp/age/rssi
  var row1 = document.createElement('div');
  row1.className = 'sat-row';
  var lab1 = document.createElement('span');
  lab1.className = 'sat-label';
  lab1.textContent = mac + (s.selected ? ' \u2014 actief' : '');
  var val1 = document.createElement('span');
  val1.className = 'sat-value';
  val1.textContent = temp + '\u00b0C / ' + ageStr + ' / ' + rssi + 'dBm';
  row1.appendChild(lab1);
  row1.appendChild(val1);

  // Action row: label input + 3 buttons
  var row2 = document.createElement('div');
  row2.className = 'sat-row';
  var lab2 = document.createElement('span');
  lab2.className = 'sat-label';
  lab2.textContent = 'Naam';
  var val2 = document.createElement('span');
  val2.className = 'sat-value';

  var input = document.createElement('input');
  input.type = 'text';
  input.id = 'ble-label-' + mac;
  input.value = s.label || '';
  input.maxLength = 23;
  input.placeholder = '(geen naam)';

  var btnSave = document.createElement('button');
  btnSave.type = 'button';
  btnSave.className = 'sat-btn';
  btnSave.textContent = 'Opslaan';
  btnSave.addEventListener('click', function() { bleSaveLabel(mac); }, false);

  var btnSelect = document.createElement('button');
  btnSelect.type = 'button';
  btnSelect.className = 'sat-btn';
  btnSelect.textContent = 'Selecteer';
  if (s.selected) btnSelect.disabled = true;
  btnSelect.addEventListener('click', function() { bleSelect(mac); }, false);

  var btnForget = document.createElement('button');
  btnForget.type = 'button';
  btnForget.className = 'sat-btn';
  btnForget.textContent = 'Vergeet';
  btnForget.addEventListener('click', function() { bleForget(mac); }, false);

  val2.appendChild(input);
  val2.appendChild(document.createTextNode(' '));
  val2.appendChild(btnSave);
  val2.appendChild(document.createTextNode(' '));
  val2.appendChild(btnSelect);
  val2.appendChild(document.createTextNode(' '));
  val2.appendChild(btnForget);

  row2.appendChild(lab2);
  row2.appendChild(val2);

  frag.appendChild(row1);
  frag.appendChild(row2);
  return frag;
}

function renderBleRoster(j) {
  var listEl   = document.getElementById('ble-roster-list');
  var activeEl = document.getElementById('ble-active');
  var warnEl   = document.getElementById('ble-roster-warning');
  if (!listEl) return;

  var sensors = (j && j.sensors) || [];

  // Active-sensor display
  if (activeEl) {
    var sel = sensors.find(function(s) { return s.selected; });
    if (sel) {
      var label = sel.label || sel.mac;
      var temp = (typeof sel.temp === 'number') ? sel.temp.toFixed(2) + '\u00b0C' : '--';
      activeEl.textContent = label + ' (' + temp + ')';
    } else if (j.selected_mac) {
      activeEl.textContent = j.selected_mac + ' (geen verse data)';
    } else {
      activeEl.textContent = '(geen geselecteerd \u2014 kies hieronder)';
    }
  }

  if (warnEl) warnEl.hidden = !j.roster_full;

  // Wipe and rebuild the list. removeChild loop is safer than innerHTML.
  while (listEl.firstChild) listEl.removeChild(listEl.firstChild);

  if (sensors.length === 0) {
    var emptyRow = document.createElement('div');
    emptyRow.className = 'sat-row';
    var emptyVal = document.createElement('span');
    emptyVal.className = 'sat-value';
    emptyVal.textContent = 'Geen sensoren ontdekt. Wacht ~30-60s op de eerstvolgende broadcast.';
    emptyRow.appendChild(emptyVal);
    listEl.appendChild(emptyRow);
  } else {
    sensors.forEach(function(s) {
      if (!bleIsValidMac(s.mac)) return;       // skip malformed entries
      listEl.appendChild(bleRowFor(s));
    });
  }
}

// TASK-508: roster action handlers. All POST JSON {mac[, label]}.
function blePost(action, body) {
  return fetch(APIGW + 'v2/sat/ble/' + action, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  }).then(function(r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  });
}

function bleSelect(mac) {
  if (!bleIsValidMac(mac)) return;
  blePost('select', { mac: mac })
    .then(function() { refreshBleRoster(); })
    .catch(function(e) { alert('Selectie mislukt: ' + e.message); });
}

function bleSaveLabel(mac) {
  if (!bleIsValidMac(mac)) return;
  var inp = document.getElementById('ble-label-' + mac);
  var val = inp ? inp.value : '';
  blePost('label', { mac: mac, label: val })
    .then(function() { refreshBleRoster(); })
    .catch(function(e) { alert('Label opslaan mislukt: ' + e.message); });
}

function bleForget(mac) {
  if (!bleIsValidMac(mac)) return;
  if (!confirm('Sensor ' + mac + ' verwijderen uit roster?')) return;
  blePost('forget', { mac: mac })
    .then(function() { refreshBleRoster(); })
    .catch(function(e) { alert('Verwijderen mislukt: ' + e.message); });
}

function buildSATSettingsGroups(page, data) {
  SAT_SETTINGS_GROUPS.forEach(function(group) {
    // Group container
    var grpDiv = document.createElement('div');
    grpDiv.className = 'sat-settings-group';

    // Collapsible header: layout row containing a toggle button and a save button as siblings
    var header = document.createElement('div');
    header.className = 'sat-settings-group-header';
    header.id = group.id + '-header';

    var bodyId = group.id + '-body';

    // Toggle button (keyboard accessible, syncs aria-expanded)
    var toggleBtn = document.createElement('button');
    toggleBtn.type = 'button';
    toggleBtn.className = 'sat-settings-group-toggle';
    toggleBtn.setAttribute('aria-expanded', 'true');
    toggleBtn.setAttribute('aria-controls', bodyId);
    var arrow = document.createElement('span');
    arrow.className = 'sat-settings-arrow';
    arrow.innerHTML = '&#9660;';
    toggleBtn.appendChild(arrow);
    toggleBtn.appendChild(document.createTextNode(' ' + group.title));
    (function(hid) {
      toggleBtn.addEventListener('click', function() { toggleSATSettingsGroup(hid); }, false);
    })(group.id + '-header');
    header.appendChild(toggleBtn);

    // Save group button (sibling of toggle, not nested)
    var saveBtn = document.createElement('button');
    saveBtn.type = 'button';
    saveBtn.className = 'sat-settings-save-btn';
    saveBtn.textContent = 'Save';
    (function(gid) {
      saveBtn.addEventListener('click', function() { saveSATSettingsGroup(gid); }, false);
    })(group.id);
    header.appendChild(saveBtn);

    grpDiv.appendChild(header);

    // Body
    var body = document.createElement('div');
    body.className = 'sat-settings-group-body';
    body.id = group.id + '-body';

    group.fields.forEach(function(field) {
      var val = data[field.key] ? data[field.key].value : '';
      var rowDiv = document.createElement('div');
      rowDiv.className = 'settingDiv';
      rowDiv.id = 'SATD_' + field.key;

      var labelEl = document.createElement('label');
      labelEl.className = 'settings-field-container';
      labelEl.setAttribute('for', 'SAT_' + field.key);
      labelEl.textContent = field.label;
      rowDiv.appendChild(labelEl);

      var inputDiv = document.createElement('div');
      inputDiv.className = 'settings-input-container';

      var input;
      if (field.type === 'select') {
        input = document.createElement('select');
        input.id = 'SAT_' + field.key;
        field.options.forEach(function(opt) {
          var o = document.createElement('option');
          o.value = opt[0];
          o.textContent = opt[1];
          if (String(opt[0]) === String(val)) o.selected = true;
          input.appendChild(o);
        });
      } else if (field.type === 'b') {
        input = document.createElement('input');
        input.type = 'checkbox';
        input.id = 'SAT_' + field.key;
        input.checked = (val === true || val === 'true' || val === '1' || val === 'on');
      } else if (field.type === 's') {
        input = document.createElement('input');
        input.type = 'text';
        input.id = 'SAT_' + field.key;
        if (field.maxlen) input.maxLength = field.maxlen;
        if (field.size) input.size = field.size;
        if (field.readonly) input.readOnly = true;   // TASK-508
        input.value = val;
      } else {
        // 'f' or 'i'
        input = document.createElement('input');
        input.type = 'number';
        input.id = 'SAT_' + field.key;
        if (field.min !== undefined) input.min = field.min;
        if (field.max !== undefined) input.max = field.max;
        if (field.step !== undefined) input.step = field.step;
        input.value = val;
      }
      input.className = 'sat-setting-input';
      (function(inputEl) {
        function markDirty() { inputEl.className = 'sat-setting-input sat-setting-changed'; }
        inputEl.addEventListener('change', markDirty, false);
        inputEl.addEventListener('keydown', markDirty, false);
      })(input);
      inputDiv.appendChild(input);

      // Unit label
      if (field.unit) {
        var unitSpan = document.createElement('span');
        unitSpan.className = 'sat-setting-unit';
        unitSpan.textContent = ' ' + field.unit;
        inputDiv.appendChild(unitSpan);
      }

      rowDiv.appendChild(inputDiv);
      body.appendChild(rowDiv);
    });

    grpDiv.appendChild(body);
    page.appendChild(grpDiv);
  });
}

function populateSATSettingsValues(data) {
  SAT_SETTINGS_GROUPS.forEach(function(group) {
    group.fields.forEach(function(field) {
      var el = document.getElementById('SAT_' + field.key);
      if (!el) return;
      var val = data[field.key] ? data[field.key].value : '';
      if (field.type === 'b') {
        el.checked = strToBool(val);
      } else if (field.type === 'select') {
        el.value = String(val);
      } else {
        el.value = val;
      }
      el.className = 'sat-setting-input';
    });
  });
}

function saveSATSettingsGroup(groupId) {
  var group = null;
  for (var i = 0; i < SAT_SETTINGS_GROUPS.length; i++) {
    if (SAT_SETTINGS_GROUPS[i].id === groupId) { group = SAT_SETTINGS_GROUPS[i]; break; }
  }
  if (!group) return;

  var msgEl = document.getElementById('satSettingsMessage');
  var changed = [];
  group.fields.forEach(function(field) {
    var el = document.getElementById('SAT_' + field.key);
    if (!el) return;
    if (el.className.indexOf('sat-setting-changed') === -1) return;
    var value;
    if (field.type === 'b') {
      value = el.checked ? 'true' : 'false';
    } else {
      value = el.value;
    }
    changed.push({ key: field.key, value: value });
  });

  if (changed.length === 0) {
    if (msgEl) { msgEl.textContent = 'No changes to save.'; setTimeout(function() { if (msgEl) msgEl.textContent = ''; }, 2000); }
    return;
  }

  if (msgEl) msgEl.textContent = 'Saving\u2026';

  var promises = changed.map(function(item) {
    return fetch(APIGW + 'v2/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json; charset=UTF-8' },
      body: JSON.stringify({ name: item.key, value: item.value }),
      mode: 'cors'
    }).then(function(resp) {
      if (!resp.ok) throw new Error(item.key + ': HTTP ' + resp.status);
      var el = document.getElementById('SAT_' + item.key);
      if (el) el.className = 'sat-setting-input';
      return resp;
    });
  });

  Promise.all(promises)
    .then(function() {
      if (msgEl) { msgEl.textContent = 'Saved successfully.'; setTimeout(function() { if (msgEl) msgEl.textContent = ''; }, 2500); }
    })
    .catch(function(err) {
      if (msgEl) { msgEl.textContent = 'Save error: ' + err.message; }
    });
}

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

function renderBottomMessage() {
  const msgEl = document.getElementById('message');
  if (!msgEl) return;

  let msgText = (typeof statusMessageText === 'string') ? statusMessageText : '';

  if (isPSmode) {
    msgText = 'PS=1 mode; showing decoded field summaries.';
  } else {
    // Suppress LittleFS mismatch here — already shown in the top banner
    if (typeof msgText === 'string' && (msgText.toLowerCase().includes('littlefs') ||
        msgText.toLowerCase().includes('flash your'))) {
      msgText = '';
    }
    if (typeof msgText === 'string' && msgText.toLowerCase().startsWith('sensorsimulation')) {
      msgText = '';
    }
    if (sensorSimulationActive) {
      msgText = msgText ? (msgText + ' | Sensor simulation active.') : 'Sensor simulation active.';
    }
  }

  msgEl.textContent = msgText;
  if (isPSmode) msgEl.classList.add('ps-mode-watermark');
  else msgEl.classList.remove('ps-mode-watermark');
  msgEl.classList.remove('version-warning');

  msgEl.style.display = (msgText === '') ? 'none' : 'block';
}

//============================================================================
function updateHeapDisplay() {
  const el = document.getElementById('heap-info');
  if (!el || currentFreeHeap === null) return;
  el.textContent = `Heap: (${currentFreeHeap} / ${currentMaxFreeBlock})`;
}

//============================================================================
function refreshDevTime() {
  //console.log("Refresh api/v2/device/time ..");
  fetch(APIGW + "v2/device/time")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      const devtime = json.devtime || {};
      const hasPsmode = (devtime.psmode !== undefined);
      const newPSmode = hasPsmode ? (devtime.psmode === true || devtime.psmode === 'true') : isPSmode;
      
      if (devtime.dateTime) {
        const timeEl = document.getElementById('theTime');
        if (timeEl) timeEl.textContent = devtime.dateTime;
      }
      statusMessageText = devtime.message || '';
      if (typeof SAT !== 'undefined' && SAT && typeof SAT.renderStatusBanner === 'function') {
        SAT.renderStatusBanner();
      }
      if (devtime.freeheap !== undefined)    currentFreeHeap = devtime.freeheap;
      if (devtime.maxfreeblock !== undefined) currentMaxFreeBlock = devtime.maxfreeblock;
      updateHeapDisplay();
      if (devtime.networkmode || devtime.apfallback) updateNetworkIndicator(devtime.networkmode, devtime.apfallback, devtime.wifiquality);

      if (hasPsmode) {
        if (newPSmode !== isPSmode) {
          isPSmode = newPSmode;
          console.log('[PS mode] PS=1 mode changed to: ' + isPSmode);
          applyPSmodeState();
        }
      }

      if (devtime.otgwsimulation !== undefined) {
        applyOTGWSimulationState(devtime.otgwsimulation);
      }
      renderBottomMessage();
    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });

} // refreshDevTime()

//============================================================================
// Apply PS=1 mode state to the UI
// When PS=1 is active: keep OT monitor panel/tabs visible, pause WebSocket log stream,
// and continue monitor polling so parsed PS data remains visible.
function applyPSmodeState() {
  if (isPSmode) {
    console.log('[PS mode] Entering PS=1 mode - keeping OT monitor visible and pausing live log stream');
    updateOTLogResponsiveState();
    if (isMainPageActive()) {
      refreshOTmonitor();
      startOTmonitorPolling();
    }
  } else {
    console.log('[PS mode] Exiting PS=1 mode - re-enabling OT monitor live stream');
    // Re-evaluate display state (may still be disabled by smartphone/proxy/screen size)
    updateOTLogResponsiveState();
    // Restart OT monitor polling if on main page
    if (isMainPageActive()) {
      startOTmonitorPolling();
    }
  }
} // applyPSmodeState()

//============================================================================      
// Global variable to store available firmware files info
let availableFirmwareFiles = [];

function refreshFirmware() {
  console.log("refreshFirmware() .. " + APIGW + "v2/firmware/files");
  
  let picInfo = { type: "Unknown", version: "Unknown", available: "Unknown", device: "Unknown" };

  fetch(APIGW + "v2/device/info")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
       if (json.device) {
         const d = json.device;
         if (d.picfwtype !== undefined) picInfo.type = d.picfwtype;
         if (d.picfwversion !== undefined) picInfo.version = d.picfwversion;
         if (d.picavailable !== undefined) picInfo.available = String(d.picavailable);
         if (d.picdeviceid !== undefined) picInfo.device = d.picdeviceid;
       }
       return fetch(APIGW + "v2/firmware/files");
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

      // Update-check banner — populated asynchronously after the page renders
      let bannerDiv = document.createElement("div");
      bannerDiv.id = "pic-update-banner";
      bannerDiv.className = "pic-update-banner checking";
      bannerDiv.textContent = "Checking for update\u2026";
      displayPICpage.appendChild(bannerDiv);

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
        valDiv.id = "firmware_version_" + files[i].name;
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
        a.href = "#";
        let refreshImg = document.createElement('img');
        refreshImg.src = localURL + '/update.png';
        refreshImg.title = "Update firmware from web";
        refreshImg.className = 'firmware-icon';
        refreshImg.setAttribute("alt", "Update");
        let refreshName = files[i].name;
        let refreshVersion = files[i].version;
        a.onclick = function(e) {
          e.preventDefault();
          refreshImg.style.opacity = '0.5';
          refreshImg.style.cursor = 'wait';
          fetch(localURL + '/pic?action=refresh&name=' + refreshName + '&version=' + refreshVersion)
            .then(function() { return fetch(APIGW + "v2/firmware/files"); })
            .then(function(r) { return r.json(); })
            .then(function(updatedFiles) {
              var entry = updatedFiles.find(function(f) { return f.name === refreshName; });
              if (entry) {
                var versionEl = document.getElementById('firmware_version_' + refreshName);
                if (versionEl) versionEl.textContent = entry.version;
              }
              refreshImg.style.opacity = '';
              refreshImg.style.cursor = '';
            })
            .catch(function(err) {
              console.error('Refresh failed:', err);
              refreshImg.style.opacity = '';
              refreshImg.style.cursor = '';
            });
        };
        a.appendChild(refreshImg);
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

      // --- Gateway Settings section (PR= polled values) ---
      var picSettingsSection = document.createElement('div');
      picSettingsSection.id = 'picSettingsSection';
      picSettingsSection.className = 'pic-settings-section';
      displayPICpage.appendChild(picSettingsSection);

      // Populate gateway settings now that the container exists
      refreshPICsettings();

      // Fire off the on-demand update check — makes an outbound call so may take a moment
      fetch(APIGW + "v2/pic/update-check")
        .then(function(r) { return r.ok ? r.json() : Promise.reject(new Error('HTTP ' + r.status)); })
        .then(function(data) {
          var pu = data && data.pic_update;
          var banner = document.getElementById('pic-update-banner');
          if (!banner || !pu) return;
          if (pu.update_available) {
            banner.className = 'pic-update-banner update-available';
            banner.textContent = 'New PIC firmware available: ' + pu.current + ' \u2192 ' + pu.latest + '. Click \u21ba to download, then \u2193 to flash.';
            var verSpan = document.getElementById('pic_version_display');
            if (verSpan) verSpan.className = 'pic-version-outdated';
          } else if (pu.latest) {
            banner.className = 'pic-update-banner up-to-date';
            banner.textContent = 'PIC firmware is up to date (' + pu.current + ')';
          } else {
            banner.className = 'pic-update-banner checking';
            banner.textContent = 'Could not check for updates';
          }
        })
        .catch(function() {
          var banner = document.getElementById('pic-update-banner');
          if (banner) {
            banner.className = 'pic-update-banner checking';
            banner.textContent = 'Could not check for updates';
          }
        });

    })
    .catch(function (error) {
      var p = document.createElement('p');
      p.appendChild(
        document.createTextNode('Error: ' + error.message)
      );
    });


}


//============================================================================
// Gateway Settings panel — populated from GET /api/v2/pic/settings
// Appends a read-only table to displayPICpage showing all 15 PR= cached values.
function mapPICCode(code, table, fallbackPrefix) {
  if (typeof code !== 'string' || code.length === 0) return '';

  if (Object.prototype.hasOwnProperty.call(table, code)) {
    return table[code];
  }

  return fallbackPrefix ? (fallbackPrefix + code) : code;
}

function formatPICSetpointOverride(value) {
  if (typeof value !== 'string' || value.length === 0) return '';

  if (value === 'N' || value === '0') {
    return 'No override';
  }

  if (value.length > 1) {
    var mode = value.charAt(0);
    var temp = value.slice(1);
    if (mode === 'T') return 'Temporary override to ' + temp + ' °C';
    if (mode === 'C') return 'Constant override to ' + temp + ' °C';
  }

  return value;
}

function formatPICDhwOverride(value) {
  return mapPICCode(value, {
    '0': 'Off',
    '1': 'On',
    'A': 'Auto',
    'P': 'Push once'
  });
}

function formatPICGpioFunctions(value) {
  if (typeof value !== 'string' || value.length === 0) return '';

  var gpioMap = {
    '0': 'Input',
    '1': 'Ground',
    '2': 'Vcc',
    '3': 'LED E',
    '4': 'LED F',
    '5': 'Home',
    '6': 'Away',
    '7': 'DS18x20 sensor',
    '8': 'DHW block'
  };
  var labels = ['A', 'B'];
  var parts = [];

  for (var i = 0; i < value.length && i < labels.length; i++) {
    parts.push(labels[i] + ': ' + mapPICCode(value.charAt(i), gpioMap));
  }

  return parts.length ? parts.join('\n') : value;
}

function formatPICGpioStates(value) {
  if (typeof value !== 'string' || value.length === 0) return '';

  var labels = ['A', 'B'];
  var parts = [];

  for (var i = 0; i < value.length && i < labels.length; i++) {
    parts.push(labels[i] + ': ' + (value.charAt(i) === '1' ? 'High' : 'Low'));
  }

  return parts.length ? parts.join('\n') : value;
}

function formatPICLedFunctions(value) {
  if (typeof value !== 'string' || value.length === 0) return '';

  var ledMap = {
    'R': 'Receive',
    'X': 'Transmit',
    'T': 'Master traffic',
    'B': 'Slave traffic',
    'O': 'Remote override active',
    'F': 'Flame',
    'H': 'Central heating',
    'W': 'Hot water',
    'C': 'Comfort mode',
    'E': 'Transmission error',
    'M': 'Maintenance required',
    'P': 'Raised power mode'
  };
  var labels = ['A', 'B', 'C', 'D', 'E', 'F'];
  var parts = [];

  for (var i = 0; i < value.length && i < labels.length; i++) {
    parts.push(labels[i] + ': ' + mapPICCode(value.charAt(i), ledMap));
  }

  return parts.length ? parts.join('\n') : value;
}

function formatPICTweaks(value) {
  if (typeof value !== 'string' || value.length === 0) return '';

  var parts = [];

  if (value.length >= 1) {
    parts.push('Ignore transitions: ' + (value.charAt(0) === '1' ? 'On' : 'Off'));
  }
  if (value.length >= 2) {
    parts.push('Override in high byte: ' + (value.charAt(1) === '1' ? 'On' : 'Off'));
  }

  return parts.length ? parts.join('\n') : value;
}

function formatPICTempSensor(value) {
  return mapPICCode(value, {
    'O': 'Outside temperature',
    'R': 'Return water temperature'
  });
}

function formatPICSmartPower(value) {
  return mapPICCode(value, {
    'L': 'Low power',
    'M': 'Medium power',
    'H': 'High power',
    'N': 'Normal power'
  });
}

function formatPICThermostatDetect(value) {
  return mapPICCode(value, {
    'C': 'Forced: Remeha Celcia 20',
    'I': 'Forced: Remeha iSense',
    'S': 'Forced: Standard thermostat',
    'D': 'Auto-detect (default)',
    'A': 'Auto-detect'
  });
}

function formatPICResetCause(value) {
  return mapPICCode(value, {
    'B': 'Brownout',
    'C': 'By command (GW=R)',
    'E': 'External reset',
    'L': 'Stuck in loop',
    'O': 'Stack overflow',
    'P': 'Power-on',
    'S': 'Serial BREAK',
    'U': 'Stack underflow',
    'W': 'Watchdog timer'
  });
}

function formatPICVoltageRef(value) {
  if (typeof value !== 'string' || value.length === 0) return '';

  var level = parseInt(value, 10);
  if (isNaN(level) || level < 0 || level > 9) return 'Level ' + value;

  // Voltage per level for each PIC variant (from OTGW firmware docs)
  var f88  = [0.625, 0.833, 1.042, 1.250, 1.458, 1.667, 1.875, 2.083, 2.292, 2.500];
  var f1847 = [0.832, 0.960, 1.088, 1.216, 1.344, 1.472, 1.600, 1.728, 1.856, 1.984];

  return 'Level ' + level
    + ' (F88: ' + f88[level].toFixed(3) + 'V'
    + ' / F1847: ' + f1847[level].toFixed(3) + 'V)';
}

function formatPICSettingValue(key, value) {
  switch (key) {
    case 'setpoint_override':
      return formatPICSetpointOverride(value);
    case 'dhw_override':
      return formatPICDhwOverride(value);
    case 'gpio':
      return formatPICGpioFunctions(value);
    case 'gpio_states':
      return formatPICGpioStates(value);
    case 'led':
      return formatPICLedFunctions(value);
    case 'tweaks':
      return formatPICTweaks(value);
    case 'temp_sensor':
      return formatPICTempSensor(value);
    case 'smart_power':
      return formatPICSmartPower(value);
    case 'thermostat_detect':
      return formatPICThermostatDetect(value);
    case 'reset_cause':
      return formatPICResetCause(value);
    case 'voltage_ref':
      return formatPICVoltageRef(value);
    default:
      return value;
  }
}

function setPICValueWithBreaks(el, text, unit) {
  var suffix = unit ? '\u00a0' + unit : '';
  var lines = text.split('\n');
  for (var i = 0; i < lines.length; i++) {
    if (i > 0) el.appendChild(document.createElement('br'));
    el.appendChild(document.createTextNode(lines[i]));
  }
  if (suffix) el.appendChild(document.createTextNode(suffix));
}

function refreshPICsettings() {
  var container = document.getElementById('picSettingsSection');
  if (!container) return;

  // Grouped display: [ label, json_key, unit_hint ]
  var groups = [
    {
      title: 'Active state',
      rows: [
        ['Setpoint override',     'setpoint_override',   ''],
        ['Setback temperature',   'setback',             '\u00b0C'],
        ['DHW override',          'dhw_override',        '']
      ]
    },
    {
      title: 'Hardware config',
      rows: [
        ['GPIO functions',        'gpio',                ''],
        ['GPIO states',           'gpio_states',         ''],
        ['LED functions',         'led',                 ''],
        ['Tweaks',                'tweaks',              ''],
        ['Temp sensor',           'temp_sensor',         ''],
        ['Smart power',           'smart_power',         ''],
        ['Thermostat detect',     'thermostat_detect',   '']
      ]
    },
    {
      title: 'Diagnostics',
      rows: [
        ['Build date',            'builddate',           ''],
        ['Clock speed',           'clock_mhz',           ''],
        ['Reset cause',           'reset_cause',         ''],
        ['Standalone interval',   'standalone_interval', 's'],
        ['Voltage reference',     'voltage_ref',         '']
      ]
    }
  ];

  fetch(APIGW + 'v2/pic/settings')
    .then(function(r) {
      if (!r.ok) { throw new Error('HTTP ' + r.status); }
      return r.json();
    })
    .then(function(json) {
      var s = (json && json.pic_settings) ? json.pic_settings : {};
      var now = Date.now();

      // Clear previous content
      while (container.lastChild) { container.lastChild.remove(); }

      var heading = document.createElement('div');
      heading.className = 'pic-settings-section-heading';
      heading.textContent = 'Gateway Settings';
      container.appendChild(heading);

      var table = document.createElement('div');
      table.className = 'pictable';

      groups.forEach(function(group) {
        // Group header row
        var groupRow = document.createElement('div');
        groupRow.className = 'picrow';
        var groupCell = document.createElement('div');
        groupCell.className = 'piccolumn1 pic-settings-group-cell';
        groupCell.setAttribute('colspan', '2');
        groupCell.style.fontStyle = 'italic';
        groupCell.style.color = '';
        groupCell.textContent = group.title;
        var emptyCell = document.createElement('div');
        emptyCell.className = 'piccolumn2';
        emptyCell.textContent = '';
        groupRow.appendChild(groupCell);
        groupRow.appendChild(emptyCell);
        table.appendChild(groupRow);

        group.rows.forEach(function(rowDef) {
          var label = rowDef[0];
          var key   = rowDef[1];
          var unit  = rowDef[2];
          var apiVal = (typeof s[key] === 'string') ? s[key] : '';
          var val = null;

          var valState = 'unknown'; // 'live' | 'cached' | 'unknown'
          if (isPICSettingDiscovered(apiVal)) {
            val = apiVal;
            valState = 'live';
            savePICSettingToCache(key, apiVal, now);
          } else {
            var cachedEntry = getPICSettingFromCache(key);
            if (cachedEntry) {
              val = cachedEntry.value;
              valState = 'cached';
            }
          }

          var displayVal = val !== null ? formatPICSettingValue(key, val) : null;

          var row = document.createElement('div');
          row.className = 'picrow';

          var c1 = document.createElement('div');
          c1.className = 'piccolumn1';
          c1.textContent = label;
          row.appendChild(c1);

          var c2 = document.createElement('div');
          c2.className = 'piccolumn2';
          if (valState === 'live') {
            c2.className += ' pic-val-live';
            setPICValueWithBreaks(c2, displayVal, unit);
          } else if (valState === 'cached') {
            c2.className += ' pic-val-cached';
            c2.title = 'Cached value (not yet polled this session)';
            setPICValueWithBreaks(c2, displayVal, unit);
          } else {
            c2.className += ' pic-val-unknown';
            c2.textContent = '\u2014';
          }
          row.appendChild(c2);

          table.appendChild(row);
        });
      });

      container.appendChild(table);

      var footer = document.createElement('div');
      footer.className = 'pic-settings-refresh';
      var note = document.createElement('span');
      note.innerHTML = 'Settings read on demand (one PR= every 3\u00a0s, full cycle \u223c45\u00a0s). '
        + '<span class="pic-val-live">Green</span>\u00a0=\u00a0live, '
        + '<span class="pic-val-cached">orange</span>\u00a0=\u00a0cached (up to 7\u00a0days), '
        + 'gray\u00a0=\u00a0not yet discovered.';
      footer.appendChild(note);
      container.appendChild(footer);

      startPICsettingsRefreshTimer();
    })
    .catch(function(err) {
      while (container.lastChild) { container.lastChild.remove(); }
      var heading = document.createElement('div');
      heading.className = 'pic-settings-section-heading';
      heading.textContent = 'Gateway Settings';
      container.appendChild(heading);
      var msg = document.createElement('div');
      msg.style.padding = '8px 10px';
      msg.textContent = 'Could not load PIC settings: ' + err.message;
      container.appendChild(msg);
      stopPICsettingsRefreshTimer();
    });
} // refreshPICsettings()


//============================================================================  
function refreshDevInfo() {
  const devNameEl = document.getElementById('devName');
  if (devNameEl) devNameEl.textContent = "";
  fetch(APIGW + "v2/device/info")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      console.log("parsed .., data is [" + JSON.stringify(json) + "]");
      const device = json.device || {};
      const hostname = device.hostname || "";
      const ipaddress = device.ipaddress || "";
      const version = device.fwversion || "";

      applyParsedGatewayMode(parseGatewayModeValue(device.otgwmode));
      applyOTGWSimulationState(device.otgwsimulation);
      applyPICAvailability(device.picavailable, device.otcommandinterface);
      applyOTDirectAvailability(device.otdirectavailable);
      updateNetworkIndicator(device.networkmode, device.apfallback, device.wifiquality);

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
      if (!gatewayModeLastKnown) {
        updateGatewayModeIndicator('unavailable');
      }
    });
} // refreshDevInfo()

//============================================================================  
function refreshOTmonitor() {
  if (flashModeActive || !isPageVisible() || !isMainPageActive()) return;

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

      // React to Dallas sensor simulation toggle to refresh labels on demand
      var simState = null;
      if (data && data.sensorsimulation && typeof data.sensorsimulation.value !== 'undefined') {
        simState = data.sensorsimulation.value;
      }

      simState = parseSimulationValue(simState);

      if (simState === true && lastSensorSimulationState !== true) {
        fetchDallasLabels()
          .then(function() {
            if (typeof OTGraph !== 'undefined' && OTGraph) {
              if (typeof OTGraph.refreshSensorLabels === 'function') {
                OTGraph.refreshSensorLabels(dallasLabelsCache);
              }
              if (typeof OTGraph.detectAndRegisterSensors === 'function') {
                OTGraph.detectAndRegisterSensors(data);
              }
              if (typeof OTGraph.updateChart === 'function') {
                if (graphRedrawTimer) {
                  clearTimeout(graphRedrawTimer);
                }
                graphRedrawTimer = setTimeout(function() {
                  OTGraph.updateChart();
                  graphRedrawTimer = null;
                }, 250);
              }
            }
          });
      }

      if (typeof simState === 'boolean') {
        const simChanged = (sensorSimulationActive !== simState);
        sensorSimulationActive = simState;
        lastSensorSimulationState = simState;
        if (simChanged) renderBottomMessage();
        updateSimulationBadge();
      }

      // Detect and register temperature sensors for the graph
      if (typeof OTGraph !== 'undefined' && OTGraph.running) {
        OTGraph.detectAndRegisterSensors(data);
        if (dallasLabelsCache && Object.keys(dallasLabelsCache).length > 0 &&
            typeof OTGraph.refreshSensorLabels === 'function') {
          OTGraph.refreshSensorLabels(dallasLabelsCache);
        }
        // Process sensor data with current timestamp
        OTGraph.processSensorData(data, new Date());
      }

      if (!isMainPageActive()) {
        return;
      }

      let otMonPage = getMainPageContainer();
      if (!otMonPage) {
        return;
      }
      let otMonTable = document.querySelector(".otmontable");
      
      // If table doesn't exist, create it (and clear waiting/existing content)
      if (!otMonTable) {
        otMonPage.innerHTML = ""; 
        otMonTable = document.createElement("div");
        otMonTable.setAttribute("class", "otmontable");
        otMonPage.appendChild(otMonTable);
      }

      for (let i in data) {
        // Map-based JSON: 'i' is the key (name), data[i] is the entry object
        const entry = data[i];
        if (!entry || typeof entry !== 'object') continue;
        if (!entry.name) entry.name = i;

        // Hide debug-only sensor simulation line from the main data table (map and array formats).
        if (i === 'sensorsimulation' || entry.name === 'sensorsimulation') {
          continue;
        }

        //console.log("["+data[i].name+"]=>["+data[i].value+"]");

        // Patch 03: bool rows get an is-bool class and a data-state attribute
        // so otmonitor.css can render them as a colored dot via the CSS
        // tokens. Names that look like a fault/alarm/safety flag also get
        // is-fault, which flips the "on" color from --status-ok to
        // --status-error.
        var isBool = (entry.value === "On" || entry.value === "Off");
        var lname = (entry.name || "").toLowerCase();
        var isFault = isBool && (lname.indexOf("fault") !== -1 ||
                                 lname.indexOf("alarm") !== -1 ||
                                 lname.indexOf("safety") !== -1);

        if ((document.getElementById("otmon_" + entry.name)) == null) { // if element does not exists yet, then build page
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "otmonrow");
          if (isBool) {
            rowDiv.classList.add("is-bool");
            rowDiv.setAttribute("data-state", entry.value === "On" ? "on" : "off");
          }
          if (isFault) rowDiv.classList.add("is-fault");
          if (entry.epoch == 0) rowDiv.classList.add('no-data-row');
          var epoch = document.createElement("INPUT");
          epoch.setAttribute("type", "hidden");
          epoch.setAttribute("id", "otmon_epoch_" + entry.name);
          epoch.name = entry.name;
          epoch.value = entry.epoch;
          rowDiv.appendChild(epoch);
          //--- field Name (Patch 03: was otmoncolumn1) ---
          var fldDiv = document.createElement("div");
          fldDiv.setAttribute("class", "label otmoncolumn1");

          var displayName;
          if (isDallasAddress(entry)) {
            displayName = entry.name;
            // Check for custom label in cache
            if (dallasLabelsCache[entry.name]) {
              displayName = dallasLabelsCache[entry.name];
            }

            // Add click handler to allow editing label
            fldDiv.classList.add('editable-label');
            fldDiv.title = 'Click to edit label (Address: ' + entry.name + ')';
            fldDiv.onclick = (function(addr, node) {
              return function(evt) { openInlineSensorLabelEditor(addr, node, evt); };
            })(entry.name, fldDiv);

            // Create label text and edit icon
            var labelText = document.createElement('span');
            labelText.setAttribute('class', 'sensor-label-text');
            labelText.textContent = displayName;
            fldDiv.appendChild(labelText);

            var editIcon = document.createElement('span');
            editIcon.setAttribute('class', 'sensor-edit-icon');
            editIcon.textContent = ' ✏️';
            fldDiv.appendChild(editIcon);
          } else {
            displayName = translateToHuman(entry.name);
            fldDiv.textContent = displayName;
          }

          rowDiv.appendChild(fldDiv);
          //--- Value cell ---
          // TASK-435 Patch E follow-up: components.css styles the legacy
          // <span class="state-on">/<span class="state-off"> markers via
          // currentColor; the otmonitor.css that drove the data-state CSS
          // dot was dropped in Patch E. Reverting the value rendering to
          // the inline-span approach so booleans still display a solid /
          // open square. The is-bool / data-state / is-fault classes set
          // on the row are kept (they cost nothing and the is-fault
          // heuristic is still useful for future styling).
          var valDiv = document.createElement("div");
          valDiv.setAttribute("class", "value otmoncolumn2");
          valDiv.setAttribute("id", "otmon_" + entry.name);
          if (entry.epoch != 0) {
            if (entry.value === "On") valDiv.innerHTML = "<span class='state-on'></span>";
            else if (entry.value === "Off") valDiv.innerHTML = "<span class='state-off'></span>";
            else valDiv.textContent = entry.value;
          }
          rowDiv.appendChild(valDiv);
          //--- Unit ---
          var unitDiv = document.createElement("div");
          unitDiv.setAttribute("class", "unit otmoncolumn3");
          unitDiv.textContent = entry.unit;
          rowDiv.appendChild(unitDiv);
          otMonTable.appendChild(rowDiv);
        }
        else { //if the element exists, then update the value
          var update = document.getElementById("otmon_" + entry.name);
          if (update.parentNode) {
            if (entry.epoch == 0) {
              update.parentNode.classList.add('no-data-row');
            } else {
              update.parentNode.classList.remove('no-data-row');
            }
            // Refresh is-bool / data-state on existing rows so the metadata
            // stays in sync. Visual rendering happens inside .value via
            // the state-on / state-off span; see create path above.
            if (isBool) {
              update.parentNode.classList.add("is-bool");
              update.parentNode.setAttribute("data-state", entry.value === "On" ? "on" : "off");
              if (isFault) update.parentNode.classList.add("is-fault");
            }
          }
          var epoch = document.getElementById("otmon_epoch_" + entry.name);
          epoch.value = entry.epoch;
          if (entry.epoch != 0) {
            if (entry.value === "On") update.innerHTML = "<span class='state-on'></span>";
            else if (entry.value === "Off") update.innerHTML = "<span class='state-off'></span>";
            else update.textContent = entry.value;
          } else {
            update.textContent = '';
          }
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
  fetch(APIGW + "v2/device/info")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      //console.log("parsed .., data is ["+ JSON.stringify(json)+"]");
      const device = json.device || {};
      applyOTGWSimulationState(device.otgwsimulation);
      applyPICAvailability(device.picavailable, device.otcommandinterface);
      applyOTDirectAvailability(device.otdirectavailable);
      for (let key in device) {
        if (key === 'otgwsimulation') continue;
        console.log("[" + key + "]=>[" + device[key] + "]");
        const displayLabel = formatDeviceInfoLabel(key);
        const displayValue = formatDeviceInfoValue(key, device[key]);
        var deviceinfoPage = document.getElementById('deviceinfoPage');
        if ((document.getElementById("devinfo_" + key)) == null) { // if element does not exists yet, then build page
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "devinforow");
          rowDiv.setAttribute("id", "devinfo_" + key);
          //--- field Name ---
          var fldDiv = document.createElement("div");
          fldDiv.setAttribute("class", "devinfocolumn1");
          fldDiv.textContent = displayLabel;
          var tooltipText = translateTooltip(key);
          if (tooltipText) {
            fldDiv.setAttribute("title", tooltipText);
          }
          rowDiv.appendChild(fldDiv);
          //--- value on screen ---
          var valDiv = document.createElement("div");
          valDiv.setAttribute("class", "devinfocolumn2");
          valDiv.textContent = displayValue;
          rowDiv.appendChild(valDiv);
          deviceinfoPage.appendChild(rowDiv);
        } else {
          const existingRow = document.getElementById("devinfo_" + key);
          const labelEl = existingRow ? existingRow.querySelector('.devinfocolumn1') : null;
          const valueEl = existingRow ? existingRow.querySelector('.devinfocolumn2') : null;
          if (labelEl) labelEl.textContent = displayLabel;
          if (valueEl) valueEl.textContent = displayValue;
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

function renderCrashLogInfo(crashlog) {
  const panel = document.getElementById('deviceinfoCrashLog');
  if (!panel) return;

  const hasCrashLog = !!(crashlog && crashlog.available);
  if (!hasCrashLog) {
    panel.classList.add('hidden');
    panel.innerHTML = '';
    return;
  }

  panel.classList.remove('hidden');
  panel.innerHTML = '';

  const title = document.createElement('div');
  title.className = 'crashlog-title';
  title.textContent = 'Stored Crash / Reboot Diagnostics';
  panel.appendChild(title);

  const intro = document.createElement('div');
  intro.className = 'crashlog-intro';
  intro.textContent = 'Latest abnormal reboot entry found in the stored reboot log.';
  panel.appendChild(intro);

  const summaryLabel = document.createElement('div');
  summaryLabel.className = 'crashlog-label';
  summaryLabel.textContent = 'Summary';
  panel.appendChild(summaryLabel);

  const summaryPre = document.createElement('pre');
  summaryPre.className = 'crashlog-pre';
  summaryPre.textContent = crashlog.summary || '';
  panel.appendChild(summaryPre);

  if (crashlog.details) {
    const detailsLabel = document.createElement('div');
    detailsLabel.className = 'crashlog-label';
    detailsLabel.textContent = 'Details';
    panel.appendChild(detailsLabel);

    const detailsPre = document.createElement('pre');
    detailsPre.className = 'crashlog-pre';
    detailsPre.textContent = crashlog.details;
    panel.appendChild(detailsPre);
  }
}

function refreshCrashLogInfo() {
  fetch(APIGW + 'v2/device/crashlog')
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      renderCrashLogInfo(json.crashlog || {});
    })
    .catch(error => {
      console.warn('refreshCrashLogInfo error:', error);
      renderCrashLogInfo(null);
    });
}

//============================================================================  
const hiddenSettings = [
  "ui_autoscroll",
  "ui_timestamps",
  "ui_capture",
  "ui_autoscreenshot", 
  "ui_autodownloadlog",
  "ui_autoexport",
  "ui_graphtimewindow",
  "webhookenable",
  "webhookurlon",
  "webhookurloff",
  "webhooktriggerbit",
  "webhookpayload",
  "webhookcontenttype"
];

const httpPasswordPlaceholderValues = ["notthepassword", "notthispassword"];
const httpPasswordSavePlaceholder = "notthispassword";
const httpPasswordPlaceholderPrefix = "password=";
const passwordPlaceholderFields = ["httppasswd", "mqttpasswd"];

function isHttpPasswordPlaceholder(value) {
  return getHttpPasswordPlaceholderLength(value) !== null;
}

function getHttpPasswordPlaceholderLength(value) {
  if (typeof value !== 'string') {
    return null;
  }

  if (httpPasswordPlaceholderValues.indexOf(value) >= 0) {
    return 0;
  }

  if (value.indexOf(httpPasswordPlaceholderPrefix) !== 0) {
    return null;
  }

  const lengthText = value.substring(httpPasswordPlaceholderPrefix.length);
  if (!/^\d+$/.test(lengthText)) {
    return null;
  }

  return parseInt(lengthText, 10);
}

function isPasswordPlaceholderField(field) {
  return passwordPlaceholderFields.indexOf(field) >= 0;
}

function getOriginalPasswordPrefill(field) {
  if (!field || !data[field]) {
    return "";
  }

  const currentValue = data[field].value;
  const currentLength = getHttpPasswordPlaceholderLength(currentValue);
  if (currentLength === null || currentLength <= 0) {
    return "";
  }

  return currentValue;
}

// Settings group taxonomy. Maps key prefixes to a UI group. First match
// wins. Group display order follows the firmware's setting iteration order
// (settingStuff.ino), since each group container is created lazily on the
// first setting that lands in it.
var SETTINGS_GROUPS = [
  { id: 'system',   title: 'System',            prefixes: ['hostname', 'httppasswd', 'DeviceManufacturer', 'DeviceModel'] },
  { id: 'mqtt',     title: 'MQTT',              prefixes: ['MQTT'] },
  { id: 'ntp',      title: 'Time / NTP',        prefixes: ['NTP'] },
  { id: 'behavior', title: 'Behavior',          prefixes: ['LEDblink', 'darktheme', 'nightlyrestart'] },
  { id: 'ui',       title: 'User Interface',    prefixes: ['ui_'] },
  { id: 'sensors',  title: 'GPIO Sensors',      prefixes: ['GPIOSENSORS'] },
  { id: 's0',       title: 'S0 Pulse Counter',  prefixes: ['S0COUNTER'] },
  { id: 'otgw',     title: 'OpenTherm Gateway', prefixes: ['OTGW'] },
  { id: 'outputs',  title: 'GPIO Outputs',      prefixes: ['GPIOOUTPUTS'] },
  { id: 'webhook',  title: 'Webhook',           prefixes: ['Webhook'] }
];

function getSettingsGroupId(key) {
  for (var g = 0; g < SETTINGS_GROUPS.length; g++) {
    var prefixes = SETTINGS_GROUPS[g].prefixes;
    for (var p = 0; p < prefixes.length; p++) {
      if (key.indexOf(prefixes[p]) === 0) return SETTINGS_GROUPS[g].id;
    }
  }
  return 'other';
}

function getOrCreateSettingsGroup(parentEl, groupId) {
  var existing = parentEl.querySelector('section.settings-group[data-group-id="' + groupId + '"] .settings-group-body');
  if (existing) return existing;

  var title = 'Other';
  for (var i = 0; i < SETTINGS_GROUPS.length; i++) {
    if (SETTINGS_GROUPS[i].id === groupId) { title = SETTINGS_GROUPS[i].title; break; }
  }

  var section = document.createElement('section');
  section.className = 'ds-card settings-group';
  section.setAttribute('data-group-id', groupId);

  var heading = document.createElement('h3');
  heading.className = 'settings-group-title';
  heading.textContent = title;
  section.appendChild(heading);

  var body = document.createElement('div');
  body.className = 'settings-group-body';
  section.appendChild(body);

  parentEl.appendChild(section);
  return body;
}

function refreshSettings() {
  console.log("refreshSettings() ..");
  data = {};
  fetch(APIGW + "v2/settings")
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
      if (msgEl) { msgEl.textContent = ""; msgEl.className = ""; }
      for (const key in data) {
        if (!Object.prototype.hasOwnProperty.call(data, key)) continue;
        const s = data[key]; // s.value, s.type, s.maxlen, s.max, s.min
        console.log("[" + key + "]=>[" + s.value + "]");
        // Skip hidden settings
        if (key.startsWith('#') || hiddenSettings.includes(key)) continue;

        var settings = document.getElementById('settingsPage');
        if ((document.getElementById("D_" + key)) == null) {
          var rowDiv = document.createElement("div");
          rowDiv.setAttribute("class", "settingDiv");
          //----rowDiv.setAttribute("id", "settingR_"+key);
          rowDiv.setAttribute("id", "D_" + key);
          // rowDiv.setAttribute("style", "text-align: right;");
          // rowDiv.style.marginLeft = "10px";
          // rowDiv.style.marginRight = "10px";
          // rowDiv.style.minWidth = "850px";
          // rowDiv.style.border = "thick solid lightblue";
          // rowDiv.style.background = "lightblue";
          //--- field Name ---
          var fldLabel = document.createElement("label");
          fldLabel.className = 'settings-field-container';
          fldLabel.setAttribute("for", key);
          fldLabel.textContent = translateToHuman(key);
          const tooltipText = translateTooltip(key);
          if (tooltipText) {
            fldLabel.setAttribute("title", tooltipText);
          }
          rowDiv.appendChild(fldLabel);
          //--- input ---
          var inputDiv = document.createElement("div");
          inputDiv.className = 'settings-input-container';

          // Dropdown options for select-type fields
          var selectOptions = {
            "otdmode": [
              [0, "Bypass (thermostat direct to boiler)"],
              [1, "Gateway (full override processing)"],
              [2, "Monitor (transparent pass-through)"],
              [3, "Master / Standalone (no thermostat)"],
              [4, "Loopback Test (simulated boiler)"]
            ]
          };

          var sInput;
          const fieldName = key;

          if (selectOptions[key]) {
            // Render as <select> dropdown
            sInput = document.createElement("select");
            sInput.setAttribute("id", key);
            var opts = selectOptions[key];
            for (var oi = 0; oi < opts.length; oi++) {
              var opt = document.createElement("option");
              opt.value = opts[oi][0];
              opt.textContent = opts[oi][1];
              if (String(opts[oi][0]) === String(s.value)) opt.selected = true;
              sInput.appendChild(opt);
            }
            sInput.setAttribute("data-prev-value", String(s.value));
          } else {
            sInput = document.createElement("input");
            sInput.setAttribute("id", key);
            if (s.type == "b") {
              sInput.setAttribute("type", "checkbox");
              sInput.checked = strToBool(s.value);
            }
            else if (s.type == "s") {
              sInput.setAttribute("type", "text");
              sInput.setAttribute("maxlength", s.maxlen);
              sInput.setAttribute("size", (s.maxlen > 20 ? 20 : s.maxlen));
            }
            else if (s.type == "p") {
              sInput.setAttribute("type", "password");
              sInput.setAttribute("maxlength", s.maxlen);
              sInput.setAttribute("size", (s.maxlen > 20 ? 20 : s.maxlen));
            }
            else if (s.type == "f") {
              sInput.setAttribute("type", "number");
              sInput.max = s.max;
              sInput.min = s.min;
              sInput.step = (s.min + s.max) / 1000;
            }
            else if (s.type == "i") {
              sInput.setAttribute("type", "number");
              sInput.setAttribute("size", 10);
              sInput.max = s.max;
              sInput.min = s.min;
              sInput.step = 1;
            }
            else if (s.type == "r") {
              sInput.setAttribute("type", "text");
              sInput.setAttribute("maxlength", s.maxlen);
              sInput.setAttribute("size", (s.maxlen > 20 ? 20 : s.maxlen));
              sInput.setAttribute("disabled", "disabled");
              sInput.className = "input-readonly";
            }
            if (isPasswordPlaceholderField(key) && isHttpPasswordPlaceholder(s.value)) {
              sInput.setAttribute("value", getHttpPasswordPlaceholderLength(s.value) > 0 ? s.value : "");
            } else {
              sInput.setAttribute("value", s.value);
            }
          }
          if (tooltipText) {
            sInput.setAttribute("title", tooltipText);
          }
          // TASK-435 follow-up: build the Reset WiFi button up front but
          // DO NOT append it to inputDiv here — append happens AFTER sInput
          // further down so the button renders to the RIGHT of the SSID
          // input (was rendering to the left because the original code
          // appended this button before sInput went in).
          var resetWifiBtn = null;
          if (key === "ssid") {
            resetWifiBtn = document.createElement("button");
            resetWifiBtn.type = "button";
            resetWifiBtn.textContent = "Reset WiFi";
            resetWifiBtn.className = "btn-wifi-reset";
            resetWifiBtn.title = "Clear stored Wi-Fi credentials and reboot the device in Access Point mode";
            resetWifiBtn.addEventListener('click', resetWiFiSettingsUI);
          }
          if (s.type !== "r") {
          sInput.addEventListener('change',
            function () {
              if (fieldName === "otdmode") {
                var modeWarnings = {
                  "0": "Bypass mode routes the thermostat directly to the boiler via relay. OT processing, overrides, and logging will stop. Continue?",
                  "2": "Monitor mode disables all overrides and modifications. Frames pass through unmodified. Continue?",
                  "3": "Master/Standalone mode assumes no thermostat is connected. The OTGW32 becomes the sole OT master. Continue?",
                  "4": "Loopback test mode uses simulated boiler data. No real boiler communication will occur. Continue?"
                };
                var warning = modeWarnings[this.value];
                if (warning && !confirm(warning)) {
                  this.value = this.getAttribute("data-prev-value");
                  return;
                }
                this.setAttribute("data-prev-value", this.value);
              }
              var inputEl = document.getElementById(fieldName);
              if (inputEl) {
                inputEl.className = "input-changed";
              }
              if (fieldName == "darktheme") {
                 localStorage.setItem('theme', this.checked ? 'dark' : 'light');
                 updateThemeToggle();
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
          } // end if (s.type !== "r")
          inputDiv.appendChild(sInput);
          // TASK-435 follow-up: Reset WiFi button rendered AFTER the SSID
          // input so it appears to the right of the field instead of in
          // front of it.
          if (resetWifiBtn) inputDiv.appendChild(resetWifiBtn);

          rowDiv.appendChild(inputDiv);
          var groupBody = getOrCreateSettingsGroup(settings, getSettingsGroupId(key));
          groupBody.appendChild(rowDiv);
        }
        else {
          //----document.getElementById("setFld_"+key).style.background = "white";
          const inputEl = document.getElementById(key);
          if (inputEl) {
            if (s.type !== "r") {
              inputEl.className = "input-normal";
            }
            //----document.getElementById("setFld_"+key).value = s.value;
            // document.getElementById(key).value = s.value;
            // FIX If checkbox change checked iso value
            if (s.type == "b")
              inputEl.checked = strToBool(s.value);
            else if (isPasswordPlaceholderField(key) && isHttpPasswordPlaceholder(s.value))
              inputEl.value = getHttpPasswordPlaceholderLength(s.value) > 0 ? s.value : "";
            else inputEl.value = s.value;
            if (inputEl.hasAttribute("data-prev-value")) {
              inputEl.setAttribute("data-prev-value", String(s.value));
            }
          }
        }
      }
      //console.log("-->done..");
      // Hide PIC-related settings rows when no PIC is detected
      applyPICAvailability(picAvailable, otCommandInterfaceAvailable);
      applyOTDirectAvailability(otDirectAvailable);
    })
    .catch(function (error) {
      var msgEl = document.getElementById("settingMessage");
      if (msgEl) { msgEl.textContent = "Error loading settings: " + error.message; msgEl.className = "error"; }
    });

} // refreshSettings()


//============================================================================
function testWebhookUI(stateOn) {
  var resultEl = document.getElementById("webhookTestResult");
  if (resultEl) resultEl.textContent = "Sending...";
  fetch(APIGW + "v2/webhook/test?state=" + (stateOn ? "on" : "off"), {
    method: "POST"
  })
    .then(function(response) {
      if (!response.ok) {
        throw new Error("HTTP " + response.status);
      }
      return response.json();
    })
    .then(function() {
      if (resultEl) resultEl.textContent = "Sent (" + (stateOn ? "ON" : "OFF") + ")";
      setTimeout(function() { if (resultEl) resultEl.textContent = ""; }, 3000);
    })
    .catch(function(error) {
      if (resultEl) resultEl.textContent = "Error: " + error.message;
    });
}

//============================================================================
function refreshWebhookPage() {
  var page = document.getElementById("webhookPage");
  if (!page) return;
  while (page.firstChild) page.removeChild(page.firstChild);

  fetch(APIGW + "v2/settings")
    .then(function(response) {
      if (!response.ok) throw new Error("HTTP " + response.status);
      return response.json();
    })
    .then(function(json) {
      var wh = {};
      ["webhookenable", "webhookurlon", "webhookurloff", "webhooktriggerbit", "webhookpayload", "webhookcontenttype"].forEach(function(key) {
        if (json.settings && json.settings[key] !== undefined) {
          wh[key] = json.settings[key];
        }
      });

      var fields = [
        { key: "webhookenable",    label: "Webhook Enabled",        type: "b" },
        { key: "webhookurlon",     label: "URL (ON state)",          type: "s", maxlen: 100, size: 60, placeholder: "http://homeassistant.local:8123/api/webhook/otgw_boiler" },
        { key: "webhookurloff",    label: "URL (OFF state)",         type: "s", maxlen: 100, size: 60, placeholder: "http://homeassistant.local:8123/api/webhook/otgw_boiler" },
        { key: "webhooktriggerbit",label: "Trigger Bit (0-15)",      type: "i", min: 0, max: 15 },
        { key: "webhookpayload",    label: "Payload Template",        type: "s", maxlen: 200, size: 86, placeholder: '{"state":"{state}","tboiler":{tboiler},"tr":{tr},"relmod":{relmod},"flame":{flameon}}' },
        { key: "webhookcontenttype",label: "Content-Type (POST)",     type: "s", maxlen: 31,  size: 20 }
      ];

      fields.forEach(function(f) {
        var s = wh[f.key];
        if (!s) return;

        var rowDiv = document.createElement("div");
        rowDiv.className = "settingDiv";

        var labelDiv = document.createElement("div");
        labelDiv.className = "settings-field-container";
        labelDiv.style.marginRight = "10px";
        labelDiv.textContent = f.label;
        rowDiv.appendChild(labelDiv);

        var inputDiv = document.createElement("div");
        inputDiv.style.textAlign = "left";

        var input = document.createElement("input");
        input.id = "WH_" + f.key;
        input.className = "input-normal";
        if (f.type === "b") {
          input.type = "checkbox";
          input.checked = strToBool(s.value);
        } else if (f.type === "s") {
          input.type = "text";
          input.maxLength = f.maxlen || 100;
          input.size = f.size || 20;
          input.value = s.value;
          if (f.placeholder) input.placeholder = f.placeholder;
        } else {
          input.type = "number";
          input.min = f.min;
          input.max = f.max;
          input.step = 1;
          input.value = s.value;
        }
        function markChanged() { input.className = "input-changed"; setVisible("btnSaveSettings", true); }
        input.addEventListener("change", markChanged, false);
        input.addEventListener("keydown", markChanged, false);

        inputDiv.appendChild(input);
        rowDiv.appendChild(inputDiv);
        page.appendChild(rowDiv);
      });

      // Test Webhook row
      var testDiv = document.createElement("div");
      testDiv.className = "settingDiv";

      var testLabelDiv = document.createElement("div");
      testLabelDiv.className = "settings-field-container";
      testLabelDiv.style.marginRight = "10px";
      testLabelDiv.textContent = "Test Webhook";
      testDiv.appendChild(testLabelDiv);

      var testBtnDiv = document.createElement("div");
      testBtnDiv.style.textAlign = "left";

      var btnOn = document.createElement("button");
      btnOn.type = "button";
      btnOn.textContent = "Test ON";
      btnOn.style.marginRight = "6px";
      btnOn.addEventListener("click", function() { testWebhookUI(true); }, false);

      var btnOff = document.createElement("button");
      btnOff.type = "button";
      btnOff.textContent = "Test OFF";
      btnOff.addEventListener("click", function() { testWebhookUI(false); }, false);

      var resultSpan = document.createElement("span");
      resultSpan.id = "webhookTestResult";
      resultSpan.style.cssText = "margin-left:10px;font-style:italic;";

      testBtnDiv.appendChild(btnOn);
      testBtnDiv.appendChild(btnOff);
      testBtnDiv.appendChild(resultSpan);
      testDiv.appendChild(testBtnDiv);
      page.appendChild(testDiv);
    })
    .catch(function(error) {
      var p = document.createElement("p");
      p.textContent = "Error loading webhook settings: " + error.message;
      page.appendChild(p);
    });
}

//============================================================================
function saveWebhookSettings() {
  var fields = ["webhookenable", "webhookurlon", "webhookurloff", "webhooktriggerbit", "webhookpayload", "webhookcontenttype"];
  var msgEl = document.getElementById("webhookMessage");
  fields.forEach(function(name) {
    var el = document.getElementById("WH_" + name);
    if (!el || el.className !== "input-changed") return;
    el.className = "input-normal";
    var value = (el.type === "checkbox") ? String(el.checked) : el.value;
    var body = JSON.stringify({ "name": name, "value": value });
    fetch(APIGW + "v2/settings", {
      headers: { "content-type": "application/json; charset=UTF-8" },
      body: body,
      method: "POST",
      mode: "cors"
    })
    .then(function(response) {
      if (response.ok) {
        if (msgEl) { msgEl.textContent = "Saved"; setTimeout(function() { if (msgEl) msgEl.textContent = ""; }, 2000); }
      } else {
        if (msgEl) msgEl.textContent = "Save failed";
      }
    })
    .catch(function(error) {
      console.log("webhook save error: " + error.message);
      if (msgEl) msgEl.textContent = "Save failed: " + error.message;
    });
  });
}

//============================================================================  
function saveSettings() {
  console.log("saveSettings() ...");

  //--- has anything changed?
  var page = document.getElementById("settingsPage");
  // Collect both <input> and <select> elements
  var inputs = page.getElementsByTagName("input");
  var selects = page.getElementsByTagName("select");
  var allFields = Array.prototype.slice.call(inputs).concat(Array.prototype.slice.call(selects));
  // Pre-count changes for the save-bar status indicator (DS:SETTINGS-SAVEBAR).
  var changeCount = 0;
  for (var ci = 0; ci < allFields.length; ci++) {
    if (allFields[ci].className === "input-changed") changeCount++;
  }
  var savebarEl = document.getElementById("settingMessage");
  if (savebarEl) {
    if (changeCount === 0) savebarEl.textContent = "";
    else savebarEl.textContent = "Saving " + changeCount + " change" + (changeCount === 1 ? "" : "s") + "...";
  }
  for (var i = 0; i < allFields.length; i++) {
    var field = allFields[i].getAttribute("id");
    console.log("InputNr[" + i + "], InputId[" + field + "]");
    const fieldEl = document.getElementById(field);
    if (!fieldEl) continue;
    var value;
    if (allFields[i].type == "checkbox") {
      value = fieldEl.checked;
    } else {
      value = fieldEl.value;
      if (isPasswordPlaceholderField(field) && value === getOriginalPasswordPrefill(field) && value !== "") {
        value = httpPasswordSavePlaceholder;
      }
    }
    console.log("==> name[" + field + "], value[" + value + "]");

    if (fieldEl.className == "input-changed") {
      //then it was changes, and needs to be saved
      fieldEl.className = "input-normal";
      console.log("Changes where made in [" + field + "][" + value + "]");

      // Update theme immediately if darktheme setting changed
      if (field === "darktheme") {
        let isDark = fieldEl.checked;
        localStorage.setItem('theme', isDark ? 'dark' : 'light');
        updateThemeToggle();
      }

      // Note: the savebar already shows "Saving N change(s)..." set by the
      // pre-count above; sendPostSetting() takes over with per-response status.
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

  fetch(APIGW + "v2/settings", other_params)
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
  if (typeof longName === 'string') {
    longName = longName.trim();
  }
  //for(var index = 0; index < (translateFields.length -1); index++) 
  for (var index = 0; index < translateFields.length; index++) {
    if (translateFields[index][0] == longName) {
      return translateFields[index][1];
    }
  };

  // Fallback to a case-insensitive lookup so table labels stay human-readable
  // even if the API key casing varies.
  if (typeof longName === 'string') {
    const normalizedName = longName.toLowerCase();
    for (var idx = 0; idx < translateFields.length; idx++) {
      const fieldKey = translateFields[idx][0];
      if (typeof fieldKey === 'string' && fieldKey.trim().toLowerCase() == normalizedName) {
        return translateFields[idx][1];
      }
    }
  }
  return longName;

} // translateToHuman()


//============================================================================
function translateTooltip(longName) {
  if (typeof longName === 'string') {
    longName = longName.trim();
  }

  for (var index = 0; index < translateTooltips.length; index++) {
    if (translateTooltips[index][0] == longName) {
      return translateTooltips[index][1];
    }
  }

  if (typeof longName === 'string') {
    const normalizedName = longName.toLowerCase();
    for (var idx = 0; idx < translateTooltips.length; idx++) {
      const fieldKey = translateTooltips[idx][0];
      if (typeof fieldKey === 'string' && fieldKey.trim().toLowerCase() == normalizedName) {
        return translateTooltips[idx][1];
      }
    }
  }
  return "";

} // translateTooltip()



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

  ["hostname", "Hostname"]
  , ["httppasswd", "Protected Endpoints Password"]
  , ["mqttbroker", "MQTT Broker Host/IP"]
  , ["mqttbrokerport", "MQTT Broker Port"]
  , ["mqttuser", "MQTT User"]
  , ["mqttpasswd", "MQTT Password"]
  , ["mqtttoptopic", "MQTT Base Topic"]
  , ["mqttuniqueid", "MQTT Unique ID"]
  , ["influxdbhostname", "InfluxDB Hostname"]
  , ["influxdbport", "InfluxDB Port (default: 8086)"]
  , ["influxdbdatabasename", "InfluxDB Database Name"]
  , ["flamestatus", "Flame Status"]
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
  , ["dhwtemperature", "Domestic Hot Water Temperature"]
  , ["dhwsetpoint", "Domestic Hot Water Setpoint"]
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
  , ["compiled", "Compiled On"]
  , ["HostName", "Hostname (.local)"]
  , ["ipaddress", "IP Address"]
  , ["macaddress", "MAC Address"]
  , ["freeheap", "Free Heap Memory (bytes)"]
  , ["maxfreeblock", "Max. Free Block (bytes)"]
  , ["hd_fragmentation_pct", "Heap Fragmentation (%)"]
  , ["hd_ws_drops", "WebSocket Drops (since boot)"]
  , ["hd_mqtt_drops", "MQTT Drops (since boot)"]
  , ["hd_enter_low", "Heap Entered LOW (count)"]
  , ["hd_enter_warning", "Heap Entered WARNING (count)"]
  , ["hd_enter_critical", "Heap Entered CRITICAL (count)"]
  , ["hd_drip_burst_skip", "Discovery Drip Skipped (active burst)"]
  , ["hd_drip_cooldown_skip", "Discovery Drip Skipped (cooldown)"]
  , ["hd_drip_slowmode", "Discovery Drip Slow-mode (count)"]
  , ["disc_published_topics", "Discovery Topics Published"]
  , ["disc_pending_ids", "Discovery IDs Pending"]
  , ["disc_verify_runs", "Discovery Verify Runs"]
  , ["disc_republish_triggered", "Discovery Republish Triggered (count)"]
  , ["disc_last_missing", "Discovery Last Missing Count"]
  , ["disc_last_orphan", "Discovery Last Orphan Count"]
  , ["disc_last_outcome", "Discovery Last Verify Outcome"]
  , ["MQTTdiscoveryAutoVerify", "MQTT Discovery Daily Auto-Verify"]
  , ["chipid", "Unique Chip ID"]
  , ["coreversion", "Arduino Core Version"]
  , ["sdkversion", "Espressif SDK Version"]
  , ["cpufreq", "CPU Speed (MHz)"]
  , ["sketchsize", "Sketch Size (bytes)"]
  , ["freesketchspace", "Sketch Free (bytes)"]
  , ["flashchipid", "Flash ID"]
  , ["flashchipsize", "Flash Chip Size (MB)"]
  , ["flashchiprealsize", "Real Flash Chip (MB)"]
  , ["littlefssize", "LittleFS Size (MB)"]
  , ["flashchipspeed", "Flash Chip Speed (MHz)"]
  , ["flashchipmode", "Flash Mode"]
  , ["boardtype", "Board Type"]
  , ["picavailable", "PIC Co-Processor"]
  , ["otcommandinterface", "OT Interface"]
  , ["otdirectavailable", "OT-Direct Active"]
  , ["otdmode", "OT-Direct Mode"]
  , ["otdbypass", "Bypass Relay"]
  , ["otdmonitor", "Monitor Mode"]
  , ["otdmaster", "Master/Standalone Mode"]
  , ["otdstepup", "24V Step-Up"]
  , ["otdthermostat", "Thermostat Connected"]
  , ["otdsetback", "Setback Active"]
  , ["otdschedtotal", "Poll Schedule Total"]
  , ["otdschedactive", "Poll Schedule Active"]
  , ["otdscheddisabled", "Poll Schedule Disabled"]
  , ["otdoverrides", "Active Overrides"]
  , ["board", "Board"]
  , ["hardwaremode", "Hardware Mode"]
  , ["networkmode", "Network Transport"]
  , ["oledpresent", "OLED Display"]
  , ["ethernetpresent", "Ethernet Hardware"]
  , ["ethernetlink", "Ethernet Link"]
  , ["ssid", "Wi-Fi Network (SSID)"]
  , ["wifirssi", "Wi-Fi Signal Strength (dBm)"]
  , ["wifiquality", "Wi-Fi Quality (%)"]
  , ["wifiquality_text", "Wi-Fi Quality"]
  , ["lastreset", "Last Reset Reason"]
  , ["mqttconnected", "MQTT Connected"]
  , ["mqttenable", "MQTT Enabled"]
  , ["mqtthaprefix", "MQTT Auto-Discovery Prefix"]
  , ["mqttharebootdetection", "MQTT Home Assistant Reboot Detection"]
  , ["ntpenable", "NTP Enabled"]
  , ["ntptimezone", "NTP Timezone"]
  , ["ntphostname", "NTP Hostname"]
  , ["ntpsendtime", "Send Time to Thermostat"]
  , ["uptime", "Uptime Since Boot"]
  , ["bootcount", "Number of Reboots"]
  , ["ledblink", "Heartbeat LED"]
  , ["darktheme", "Dark Theme"]
  , ["nightlyrestart", "Scheduled Nightly Restart"]
  , ["nightlyrestarthour", "Nightly Restart Hour (0-23)"]
  , ["gpiosensorsenabled", "GPIO Sensors Enabled"]
  , ["gpiosensorslegacyformat", "GPIO Sensors Legacy Format"]
  , ["gpiosensorsinterval", "GPIO Publish Interval (sec)"]
  , ["gpiosensorspin", "GPIO Sensor Pin (SD3 = GPIO10 => 10)"]
  , ["numberofsensors", "Number of Temperature Sensors"]
  , ["s0counterenabled", "S0 Counter Enabled"]
  , ["s0counterinterval", "S0 Counter Interval (sec)"]
  , ["s0counterpin", "S0 Counter Pin (D6 = GPIO12 => 12)"]
  , ["s0counterdebouncetime", "S0 Counter Debounce Time (ms)"]
  , ["s0counterpulsekw", "S0 Pulses per kWh"]
  , ["s0powerkw", "S0 Actual Power (kW)"]
  , ["s0intervalcount", "S0 Interval Pulses"]
  , ["s0totalcount", "S0 Total Pulses"]
  , ["mqttinterval", "MQTT Publish Interval (sec)"]
  , ["mqttotmessage", "MQTT Raw OpenTherm Messages"]
  , ["mqttseparatesources", "MQTT Separate Sources"]
  , ["legacyport25238enabled", "Legacy TCP Port 25238"]
  , ["otgwcommandenable", "Run Boot Command"]
  , ["otgwcommands", "Boot Command"]
  , ["thermostatconnected", "Thermostat Connected"]
  , ["boilerconnected", "Boiler Connected"]
  , ["otgwmode", "Gateway Mode"]
  , ["otgwconnected", "OpenTherm Active"]
  , ["gpiooutputsenabled", "GPIO Output Trigger Enabled"]
  , ["gpiooutputspin", "GPIO Output Pin"]
  , ["gpiooutputstriggerbit", "GPIO Trigger Bit (0-15)"]
  , ["webhookenable", "Webhook Enabled"]
  , ["webhookurlon", "Webhook URL (On State)"]
  , ["webhookurloff", "Webhook URL (Off State)"]
  , ["webhooktriggerbit", "Webhook Trigger Bit (0-15)"]
  , ["webhookpayload", "Webhook Payload Template"]
  , ["webhookcontenttype", "Webhook Content-Type (POST)"]
  , ["SATenabled", "SAT Enabled"]
  , ["SATsystem", "SAT Heating System (0=Auto, 1=Radiators, 2=Heat Pump, 3=Underfloor)"]
  , ["SATmanufacturer", "SAT Manufacturer (0=Auto, 1=Atag, 2=Baxi, 3=Brotge, 4=DeDietrich, 5=Ferroli, 6=Geminox, 7=Ideal, 8=Immergas, 9=Intergas, 10=Itho, 11=Nefit, 12=Radiant, 13=Remeha, 14=Sime, 15=Vaillant, 16=Viessmann, 17=Worcester, 18=Other)"]
  , ["SATtargettemp", "SAT Target Temperature"]
  , ["SATcoefficient", "SAT Heating Curve Coefficient"]
  , ["SATdeadband", "SAT PID Deadband"]
  , ["SATinterval", "SAT Control Interval (sec)"]
  , ["SATexternaltemp", "SAT Use External Temperature"]
  , ["SATpresetcomfort", "SAT Preset: Comfort"]
  , ["SATpreseteco", "SAT Preset: Eco"]
  , ["SATpresetaway", "SAT Preset: Away"]
  , ["SATpresetsleep", "SAT Preset: Sleep"]
  , ["SATpresetactivity", "SAT Preset: Activity"]
  , ["SATpwmautoswitch", "SAT PWM Auto-Switch"]
  , ["SATmaxmodulation", "SAT Max Relative Modulation (0-100%)"]
  , ["SATovpvalue", "SAT OPV Value (\u00B0C, 0=not calibrated)"]
  , ["SATovpenabled", "SAT OPV Enabled"]
  , ["SATovershootmargin", "SAT Overshoot Margin (\u00B0C)"]
  , ["SATboilercapacity", "SAT Boiler Capacity (kW)"]
  , ["SATpresetsync", "SAT Preset Sync Enable"]
  , ["SATpresetsynctopic", "SAT Preset Sync MQTT Topic"]
  , ["SATsolargain", "SAT Solar Gain Compensation"]
  , ["SATsolarminrise", "SAT Solar Min Rise Rate (\u00B0C/hr)"]
  , ["SATsolaroffset", "SAT Solar Setpoint Offset (\u00B0C)"]
  , ["SATsummersimmer", "SAT Summer Simmer Auto-Disable"]
  , ["SATsummerthreshold", "SAT Summer Threshold (\u00B0C)"]
  , ["SATsummerminhours", "SAT Summer Min Hours Above"]
  , ["SATcomfortadjust", "SAT Comfort Adjustment"]
  , ["SATcomforthumidity", "SAT Reference Humidity (%)"]
  , ["SATcomfortmaxoffset", "SAT Comfort Max Offset (\u00B0C)"]
  , ["SATmultiarea", "SAT Multi-Area Enable"]
  , ["SATmultiareacount", "SAT Multi-Area Count"]
  , ["SATareaweight0", "SAT Area 0 Weight"]
  , ["SATareaweight1", "SAT Area 1 Weight"]
  , ["SATareaweight2", "SAT Area 2 Weight"]
  , ["SATareaweight3", "SAT Area 3 Weight"]
  , ["SATthermalcoeff", "SAT Thermal Drop Coefficient"]
  , ["SATbleenable", "SAT BLE Sensor Enable"]
  , ["SATblemac", "SAT BLE Sensor MAC Address"]
  , ["SATbleinterval", "SAT BLE Publish Cadence (sec)"]
  , ["SATweatherenable", "SAT Weather Enable"]
  , ["SATweatherlat", "SAT Weather Latitude"]
  , ["SATweatherlon", "SAT Weather Longitude"]
  , ["SATweatherinterval", "SAT Weather Poll Interval (sec)"]
  , ["SATweatherapikey", "SAT OWM API Key"]
  , ["otdmode", "OT-Direct Operating Mode"]
  , ["otdautodetect", "OT-Direct Auto-Detect Mode"]
  , ["otdsetbacktemp", "OT-Direct Setback Temperature (\u00B0C)"]
  , ["otdsetbacktimeout", "OT-Direct Setback Timeout (seconds)"]
  , ["otdenableslave", "OT-Direct Enable Slave (Master Mode)"]
  , ["otdsummermode", "OT-Direct Summer Mode"]
  , ["otdfailsafe", "OT-Direct Fail-Safe"]
  , ["otdmsginterval", "OT-Direct Message Interval (ms)"]
  , ["otdhasbypassrelay", "OT-Direct Bypass Relay Present"]
  , ["ethstaticip", "Ethernet Static IP"]
  , ["ethipaddress", "Ethernet IP Address"]
  , ["ethgateway", "Ethernet Gateway"]
  , ["ethsubnet", "Ethernet Subnet Mask"]
  , ["ethdns", "Ethernet DNS Server"]

];

var translateTooltips = [

  ["hostname", "Device name on your network. Use letters, numbers and hyphens only."]
  , ["httppasswd", "Password for protected admin endpoints such as settings, maintenance actions, file management, reboot, and OTA update. Username is admin."]
  , ["HostName", "Advertised hostname. Add .local when you open the device by mDNS name."]
  , ["ssid", "Read-only name of the Wi-Fi network the gateway is connected to."]
  , ["mqttconnected", "Read-only MQTT connection state. This should show connected after broker login succeeds."]
  , ["mqttenable", "Turn MQTT publishing on when you use an MQTT broker like Home Assistant."]
  , ["mqttbroker", "Hostname or IP address of your MQTT broker."]
  , ["mqttbrokerport", "Broker port, usually 1883 for plain MQTT."]
  , ["mqttuser", "Leave empty if your MQTT broker does not require a login."]
  , ["mqttpasswd", "Password used when connecting to the configured MQTT broker with the MQTT user."]
  , ["mqtttoptopic", "Base topic used for all MQTT publish and command topics."]
  , ["mqttuniqueid", "Unique device ID used for MQTT discovery. Change only if you need a new device identity."]
  , ["mqtthaprefix", "Home Assistant discovery prefix. Keep the default unless your HA setup uses a custom prefix."]
  , ["mqttharebootdetection", "Enable this if Home Assistant should republish discovery after it restarts."]
  , ["mqttinterval", "Minimum time between repeated MQTT updates when a value does not change. Use 0 to publish every update."]
  , ["mqttotmessage", "Publish raw OpenTherm messages on MQTT for diagnostics and advanced integrations."]
  , ["mqttseparatesources", "Publish thermostat and boiler values on separate MQTT topics when available."]
  , ["legacyport25238enabled", "Enable the legacy otmonitor TCP bridge on port 25238. Leave disabled unless pyotgw, otmonitor, or another external TCP client needs it."]
  , ["ntpenable", "Use an NTP server to keep the gateway clock in sync."]
  , ["ntptimezone", "Timezone name used for local time and daylight saving changes."]
  , ["ntphostname", "Hostname of the NTP server. The default pool server is usually fine."]
  , ["ntpsendtime", "Send the current time from this gateway to the thermostat when supported."]
  , ["ledblink", "Blink the onboard LED as a heartbeat to show the firmware is running."]
  , ["darktheme", "Use the dark color theme in the web interface."]
  , ["nightlyrestart", "Restart the device once a day at the configured hour to recover heap memory. Causes a brief (~30 second) service interruption."]
  , ["nightlyrestarthour", "Local hour (0-23) when the nightly restart runs. Default is 4 (04:00). Only active when NTP is enabled and synced."]
  , ["gpiosensorsenabled", "Enable 1-Wire temperature sensors connected to the selected GPIO pin."]
  , ["gpiosensorslegacyformat", "Use the older MQTT payload format only if an existing setup depends on it."]
  , ["gpiosensorsinterval", "Seconds between GPIO sensor updates. Use a higher value to reduce MQTT traffic."]
  , ["gpiosensorspin", "GPIO pin used for the 1-Wire bus. GPIO10 is the default hardware wiring."]
  , ["numberofsensors", "Read-only count of detected temperature sensors on the configured 1-Wire bus."]
  , ["s0counterenabled", "Enable the S0 pulse counter input for energy or meter pulses."]
  , ["s0counterinterval", "Seconds between S0 counter updates and MQTT publishes."]
  , ["s0counterpin", "GPIO pin connected to the S0 pulse output of your meter."]
  , ["s0counterdebouncetime", "Ignore pulses that arrive sooner than this debounce time in milliseconds."]
  , ["s0counterpulsekw", "Number of S0 pulses per kWh reported by your meter."]
  , ["otgwcommandenable", "Run the boot command automatically after the gateway starts."]
  , ["otgwcommands", "Command sent to the OTGW at startup. Use the normal OTGW serial command format."]
  , ["thermostatconnected", "Read-only status showing whether the thermostat side is currently detected."]
  , ["boilerconnected", "Read-only status showing whether the boiler side is currently detected."]
  , ["otgwmode", "Current gateway operating mode reported by the firmware."]
  , ["otgwconnected", "Shows whether OpenTherm communication is active (thermostat and/or boiler detected on the bus)."]
  , ["gpiooutputsenabled", "Enable the GPIO output trigger feature."]
  , ["gpiooutputspin", "GPIO pin that will be switched when the selected status bit changes."]
  , ["gpiooutputstriggerbit", "Status bit number to follow. Use values 0 through 15."]
  , ["webhookenable", "Enable HTTP callbacks when the selected status bit changes."]
  , ["webhookurlon", "URL to call when the tracked bit switches on."]
  , ["webhookurloff", "URL to call when the tracked bit switches off."]
  , ["webhooktriggerbit", "Status bit number that triggers the webhook. Use values 0 through 15."]
  , ["webhookpayload", "Optional POST body. Leave empty when the receiving service does not need a payload."]
  , ["webhookcontenttype", "HTTP Content-Type header sent with POST requests, for example application/json."]
  , ["SATenabled", "Enable the SAT (Smart Autotune Thermostat) smart heating controller."]
  , ["SATsystem", "0 = Auto-detect from boiler, 1 = Radiators (max 62C, 4 cycles/hr), 2 = Heat Pump (max 40C, 2 cycles/hr, MM=100), 3 = Underfloor (max 45C, base offset 20.0)."]
  , ["SATmanufacturer", "0 = Auto-detect from OT MemberID (may be ambiguous). Select your boiler brand to enable manufacturer-specific optimizations. Quirks: Geminox=min 10% modulation, Immergas=TP command+80% cap, Ideal/Intergas/Nefit=MI=500 boot."]
  , ["SATtargettemp", "Desired room temperature in degrees Celsius (5-30)."]
  , ["SATcoefficient", "Heating curve steepness. Higher = hotter boiler water for same demand."]
  , ["SATdeadband", "PID deadband in degrees. Within this band only the I-term integrates."]
  , ["SATinterval", "Seconds between SAT control loop updates (10-300)."]
  , ["SATexternaltemp", "Use external temperature sensor via MQTT/REST instead of OT bus room temp."]
  , ["SATpresetcomfort", "Comfort preset target temperature."]
  , ["SATpreseteco", "Eco preset target temperature."]
  , ["SATpresetaway", "Away preset target temperature."]
  , ["SATpresetsleep", "Sleep preset target temperature."]
  , ["SATpresetactivity", "Activity preset target temperature. Used by window detection to lower heating when window is open."]
  , ["SATpwmautoswitch", "Automatically switch between PWM and continuous mode based on cycle analysis."]
  , ["SATmaxmodulation", "Maximum relative modulation (MM= command) sent to boiler. 0-100%. Heat pumps always use 100%. Default 100."]
  , ["SATovpvalue", "Overshoot Protection Value: maximum boiler temp at minimum modulation. Determines when PWM is needed. 0 = not yet calibrated. Run calibration to auto-detect."]
  , ["SATovpenabled", "Enable OPV-based automatic PWM switching. When boiler temp exceeds OPV and room temp is near setpoint, switch to PWM mode."]
  , ["SATovershootmargin", "Flow temperature margin above setpoint to classify as overshoot (0.5-5.0). Default 2.0."]
  , ["SATboilercapacity", "Boiler power capacity in kW (1-100). Used to calculate current power from modulation percentage. Typical values: 24 kW for combi boilers, 15-40 kW range."]
  , ["SATpresetsync", "When enabled, publish the active preset name to the configured MQTT topic on every preset change. Secondary climate entities or Home Assistant automations can subscribe to follow the same preset."]
  , ["SATpresetsynctopic", "MQTT topic to publish preset changes to (e.g. homeassistant/climate/zone2/preset). The preset name (activity, away, eco, home, comfort, sleep, none) is published as a retained message."]
  , ["SATsolargain", "Detect solar heating (sunlight through windows) and reduce setpoint to prevent overheating. Triggers when indoor temp rises while boiler modulation is low."]
  , ["SATsolarminrise", "Minimum indoor temperature rise rate in degrees per hour to detect solar gain. Default 0.5. Lower values are more sensitive."]
  , ["SATsolaroffset", "Setpoint reduction in degrees Celsius during solar gain event. Default 2.0. Higher values reduce heating more aggressively."]
  , ["SATsummersimmer", "Automatically disable heating when outdoor temperature stays above the threshold for the configured number of hours. Re-enables when temp drops below threshold minus 2 degrees."]
  , ["SATsummerthreshold", "Outdoor temperature threshold in degrees Celsius. When outdoor temp stays above this for the configured hours, summer mode activates and heating is suppressed. Default 18."]
  , ["SATsummerminhours", "Number of consecutive hours the outdoor temperature must stay above the threshold before summer mode activates. Default 6. Range 1-48."]
  , ["SATcomfortadjust", "Adjust heating setpoint based on indoor humidity. High humidity makes the same temperature feel warmer, so the setpoint is lowered. Low humidity feels cooler, so the setpoint is raised. Requires humidity input via MQTT or weather API."]
  , ["SATcomforthumidity", "Reference humidity percentage where no adjustment is applied. At this level the setpoint stays unchanged. Default 50%."]
  , ["SATcomfortmaxoffset", "Maximum temperature adjustment in degrees Celsius from the humidity comfort model. The actual offset scales linearly with the difference from the reference humidity. Default 1.0, range 0-3."]
  , ["SATmultiarea", "Enable multi-area room temperature support. When enabled, the SAT control loop uses the weighted average of up to 4 area sensors instead of a single room temperature. Each area receives its temperature via MQTT (sat/area/0-3) or REST API."]
  , ["SATmultiareacount", "Number of configured temperature areas (0-4). Only areas below this count are included in the weighted average."]
  , ["SATareaweight0", "Weight for area 0 in the weighted average calculation. Higher weight means this area has more influence on the combined room temperature. Default 1.0, range 0-10."]
  , ["SATareaweight1", "Weight for area 1 in the weighted average calculation. Default 1.0, range 0-10."]
  , ["SATareaweight2", "Weight for area 2 in the weighted average calculation. Default 1.0, range 0-10."]
  , ["SATareaweight3", "Weight for area 3 in the weighted average calculation. Default 1.0, range 0-10."]
  , ["SATautotune", "Enable automatic PID gains self-tuning. Monitors heating cycle performance (overshoot, undershoot, oscillation) and gradually adjusts the heating curve coefficient. Tuning runs once per hour after at least 6 heating cycles."]
  , ["SATautotunerate", "Adjustment rate per tuning cycle (0.5%-10%). Default 2%. Lower values make smaller, more conservative adjustments. Higher values converge faster but risk instability."]
  , ["SATthermalcoeff", "Learned thermal drop coefficient (degrees C per hour per degree indoor-outdoor delta). Automatically updated by observing how fast the building cools when the boiler is off. Used during fallback mode to estimate room temperature. Default 0.05, typical range 0.02-0.1 for well-insulated buildings."]
  , ["SATbleenable", "Enable BLE (Bluetooth Low Energy) temperature sensor scanning (ESP32 only). Supports Xiaomi LYWSD03MMC with ATC/pvvx custom firmware and BTHome v2 protocol sensors. Discovered sensors appear in the BLE Sensoren panel above where you can name them and pick one. When enabled, BLE temperature becomes the highest priority room temperature source for SAT."]
  , ["SATblemac", "(Beheerd via het BLE Sensoren paneel hierboven) MAC address of the active BLE sensor. Leave empty for auto-select-if-only-one. Format AA:BB:CC:DD:EE:FF, uppercase."]
  , ["SATbleinterval", "How often the gateway publishes BLE-sensor state (MQTT + state.sat.*) in seconds (10-300). Default 30 seconds. Since 2.0.0 the BLE radio scans continuously on ESP32 (matches OT-Thing); this setting controls publish/state-update cadence, not scan rate."]
  , ["SATweatherenable", "Enable weather data fetching from Open-Meteo API (free, no key needed). Provides outdoor temperature fallback when no OT outdoor sensor is available."]
  , ["SATweatherlat", "Latitude for weather data. Use the Detect Location button on the SAT dashboard, or enter manually (-90 to 90)."]
  , ["SATweatherlon", "Longitude for weather data. Use the Detect Location button on the SAT dashboard, or enter manually (-180 to 180)."]
  , ["SATweatherinterval", "How often to fetch weather data in seconds (900-3600). Default 900 (15 minutes). Minimum 900 enforced to honour OpenWeatherMap free-tier rate-limit (1000 calls/day)."]
  , ["SATweatherapikey", "OpenWeatherMap API key. Get a free key at openweathermap.org. Leave blank to use Open-Meteo (free, no key needed)."]
  , ["otdmode", "Gateway: full control with overrides. Monitor: transparent pass-through, observe only. Bypass: thermostat talks directly to boiler via relay. Master: standalone, no thermostat needed."]
  , ["otdautodetect", "At boot, wait 5 seconds for thermostat on the bus. If none found, automatically switch to Master mode."]
  , ["otdsetbacktemp", "When the thermostat disconnects, override the boiler setpoint to this safe temperature (1-30\u00B0C)."]
  , ["otdsetbacktimeout", "Seconds without thermostat communication before engaging setback protection (5-255s)."]
  , ["otdenableslave", "In Master mode, keep the slave interface active to respond to a thermostat if one connects. Disable for pure standalone operation with no thermostat bus activity."]
  , ["otdsummermode", "When enabled, the gateway sends CH=0 (central heating off) to the boiler regardless of what the thermostat requests. Use this in summer when you want DHW only."]
  , ["otdfailsafe", "When enabled, the gateway sends a safe low setpoint to the boiler if communication with the thermostat is lost, preventing the boiler from running uncontrolled."]
  , ["otdmsginterval", "Minimum time in milliseconds between consecutive OpenTherm messages on the bus (100-1275ms). Lower values increase polling rate; the OT spec requires at least 100ms."]
  , ["otdhasbypassrelay", "Enable when a physical bypass relay is installed on the OTGW32 board. In Bypass mode, the relay connects thermostat directly to boiler, bypassing the gateway."]
  , ["ethstaticip", "Use a static IP address for Ethernet instead of DHCP."]
  , ["ethipaddress", "Static IP address for the Ethernet interface (e.g. 192.168.1.100)."]
  , ["ethgateway", "Default gateway for the Ethernet interface."]
  , ["ethsubnet", "Subnet mask for the Ethernet interface (e.g. 255.255.255.0)."]
  , ["ethdns", "DNS server for the Ethernet interface. Leave 0.0.0.0 to use the gateway."]

];

//============================================================================
function applyTheme() {
  fetch(APIGW + "v2/settings")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      let data = json.settings;
      if (data && data["darktheme"]) {
        // Only apply server theme if the browser has no local preference yet
        var localTheme = null;
        try { localTheme = localStorage.getItem('otgw-theme') || localStorage.getItem('theme'); } catch(e) {}
        if (!localTheme) {
          let isDark = strToBool(data["darktheme"].value);
          try {
            localStorage.setItem('theme', isDark ? 'dark' : 'light');
            localStorage.setItem('otgw-theme', isDark ? 'dark' : 'light');
          } catch(e) {}
          if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.setTheme === 'function') {
              OTGraph.setTheme(isDark ? 'dark' : 'light');
          }
          updateThemeToggle();
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
        // Check diagnose first, then interface, default to gateway
        if (fname.includes("diagnose")) {
            displayType = "Diagnose";
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
    fetch(APIGW + 'v2/flash/status')
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
    clearLogBuffer();
    
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
    startOTmonitorPolling();
    startTimeUpdates();
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
    startOTmonitorPolling();
    startTimeUpdates();
}

// function pollForReboot() - Removed
// function startFlashCountdown() - Removed

function performFlash(filename) {
    currentFlashFilename = filename;
    isFlashing = true;
    toggleInteraction(false);
    // Stop polling during upgrade to prevent interference and reduce load
  stopOTmonitorPolling();
  stopTimeUpdates();

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
                startOTmonitorPolling();
                startTimeUpdates();
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
                    startOTmonitorPolling();
                    startTimeUpdates();
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
                clearLogBuffer();

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
                startOTmonitorPolling();
                startTimeUpdates();
            } else if (msg.state === 'error' || msg.state === 'abort') {
                // Error or abort
                stopFlashPolling(); // Stop failsafe polling
                isFlashing = false;
                toggleInteraction(true);
                
                let errorMsg = msg.error || "Flash failed";
                if (pctText) pctText.textContent = "Finished " + (msg.filename || currentFlashFilename) + ": " + errorMsg;
                
                if (progressBar) progressBar.classList.add('error');
                
                // Restart polling
                startOTmonitorPolling();
                startTimeUpdates();
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
        html += '<td>' + escapeHtml(r.hex) + '</td>';
        html += '<td>' + escapeHtml(String(r.id)) + '</td>';
        html += '<td>' + escapeHtml(dir) + '</td>';
        html += '<td>' + escapeHtml(r.label) + '</td>';
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
  sendPostSetting(field, value);
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
  
  fetch(apiPath + "v2/settings")
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return response.json();
    })
    .then(json => {
      if (!json || !json.settings) return;
      const settings = json.settings;
      const getVal = (name) => settings[name] ? settings[name].value : null;

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

//============================================================================
// Edit sensor label functionality - inline non-blocking editor
//============================================================================
var activeSensorLabelEditor = null;

function openInlineSensorLabelEditor(address, targetNode, evt) {
  if (!address || !targetNode) return;
  if (evt) {
    evt.preventDefault();
    evt.stopPropagation();
  }

  closeInlineSensorLabelEditor();

  var labelTextNode = targetNode.querySelector('.sensor-label-text');
  var currentLabel = address;
  var labelKey = address + '_label';
  if (dallasLabelsCache && typeof dallasLabelsCache[address] === 'string' && dallasLabelsCache[address].trim() !== '') {
    currentLabel = dallasLabelsCache[address].trim();
  } else if (typeof data !== 'undefined' && data[labelKey] && data[labelKey].value) {
    currentLabel = data[labelKey].value;
  }

  var input = document.createElement('input');
  input.type = 'text';
  input.maxLength = 16;
  input.className = 'sensor-label-inline-editor';
  input.value = currentLabel;
  input.title = 'Enter new label and press Enter to save';

  if (labelTextNode) {
    targetNode.replaceChild(input, labelTextNode);
  } else {
    targetNode.insertBefore(input, targetNode.firstChild);
  }

  activeSensorLabelEditor = {
    address: address,
    container: targetNode,
    input: input,
    originalText: currentLabel,
    saving: false  // Guard flag to prevent duplicate saves
  };

  input.focus();
  input.select();

  input.addEventListener('keydown', function(e) {
    if (e.key === 'Enter') {
      e.preventDefault();
      saveInlineSensorLabel();
    } else if (e.key === 'Escape') {
      e.preventDefault();
      closeInlineSensorLabelEditor(true);
    }
  });

  input.addEventListener('blur', function() {
    if (activeSensorLabelEditor && activeSensorLabelEditor.input === input) {
      // Only save if label changed to avoid unnecessary flash writes
      var newLabel = input.value.trim() || address;
      if (newLabel !== activeSensorLabelEditor.originalText) {
        saveInlineSensorLabel();
      } else {
        // Label unchanged, just close the editor
        closeInlineSensorLabelEditor(true);
      }
    }
  });
}

function closeInlineSensorLabelEditor(cancelOnly) {
  if (!activeSensorLabelEditor) return;

  var editor = activeSensorLabelEditor;
  var container = editor.container;
  var input = editor.input;
  var text = cancelOnly ? editor.originalText : (input ? input.value.trim() : editor.originalText);
  if (!text) text = editor.address;

  var labelText = document.createElement('span');
  labelText.setAttribute('class', 'sensor-label-text');
  labelText.textContent = text;

  if (container && input && input.parentNode === container) {
    container.replaceChild(labelText, input);
  }

  activeSensorLabelEditor = null;
}

function saveInlineSensorLabel() {
  if (!activeSensorLabelEditor) return;
  
  var editor = activeSensorLabelEditor;
  
  // Guard against duplicate saves (e.g., blur triggered during save)
  if (editor.saving) return;
  
  var input = editor.input;
  if (!input) {
    closeInlineSensorLabelEditor(true);
    return;
  }

  var newLabel = input.value.trim();
  if (newLabel.length === 0) {
    newLabel = editor.address;
  }

  if (newLabel.length > 16) {
    newLabel = newLabel.substring(0, 16);
  }

  // Set saving flag before disabling input
  editor.saving = true;
  input.disabled = true;

  // Use bulk labels endpoint with read-modify-write flow
  var labelsUrl = APIGW + 'v2/sensors/labels';

  fetch(labelsUrl)
    .then(function (response) {
      if (!response.ok) {
        throw new Error('HTTP ' + response.status);
      }
      var contentType = response.headers.get('content-type') || '';
      if (contentType.indexOf('application/json') === -1) {
        // No JSON body; start from empty labels map
        return {};
      }
      return response.json();
    })
    .then(function (labels) {
      // Modify the label for the current address
      labels[editor.address] = newLabel;

      // Write all labels back
      return fetch(labelsUrl, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(labels)
      });
    })
    .then(function(response) {
      if (!response.ok) {
        throw new Error('HTTP ' + response.status);
      }
      // Success! Update cache and close editor
      dallasLabelsCache[editor.address] = newLabel;
      if (typeof OTGraph !== 'undefined' && OTGraph && typeof OTGraph.refreshSensorLabels === 'function') {
        OTGraph.refreshSensorLabels(dallasLabelsCache);
      }
      closeInlineSensorLabelEditor();
      refreshOTmonitor();
    })
    .catch(function(error) {
      console.error('Error updating sensor label:', error);
      if (input) {
        input.disabled = false;
        input.focus();
        input.select();
      }
      editor.saving = false;
    });
}

//============================================================================
function resetWiFiSettingsUI() {
  if (confirm('Reset Wi-Fi settings?\n\nThis will clear the stored Wi-Fi credentials and reboot the device in Access Point (AP) mode.\n\nAfter the reboot, connect to the "OTGW-XXXXXX" Wi-Fi network to reconfigure.')) {
    window.location.href = localURL + '/ResetWireless';
  }
}
