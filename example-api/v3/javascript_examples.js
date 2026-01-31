/**
 * OpenTherm Gateway REST API v3 - JavaScript Examples
 * 
 * These examples demonstrate how to use the OTGW API from JavaScript/Node.js
 * 
 * Browser Usage:
 *   - Include this file in an HTML page
 *   - Call functions from the browser console
 *   - Examples: getSystemInfo(), updateMQTTConfig(), sendOTGWCommand()
 * 
 * Node.js Usage:
 *   - Requires node-fetch: npm install node-fetch
 *   - Load with: const examples = require('./javascript_examples.js');
 *   - Call functions: examples.getSystemInfo()
 */

const OTGW_URL = 'http://otgw.local/api/v3';

/**
 * Helper function to make API requests
 */
async function makeRequest(method, endpoint, body = null, headers = {}) {
  const url = `${OTGW_URL}${endpoint}`;
  const options = {
    method,
    headers: {
      'Content-Type': 'application/json',
      ...headers
    }
  };

  if (body) {
    options.body = JSON.stringify(body);
  }

  try {
    const response = await fetch(url, options);
    const data = await response.json();

    // Handle non-2xx responses
    if (!response.ok) {
      console.error(`API Error (${response.status}):`, data);
      throw new Error(`${data.error}: ${data.message}`);
    }

    return { status: response.status, headers: response.headers, data };
  } catch (error) {
    console.error('Request failed:', error);
    throw error;
  }
}

// ============================================================================
// API DISCOVERY
// ============================================================================

/**
 * Get API root with HATEOAS links
 */
async function getAPIRoot() {
  console.log('Getting API Root...');
  const result = await makeRequest('GET', '/');
  console.log('API Root:', result.data);
  return result.data;
}

/**
 * Discover all available resources through HATEOAS
 */
async function discoverAPI() {
  const root = await getAPIRoot();
  console.log('Available resources:');
  Object.entries(root._links).forEach(([key, link]) => {
    console.log(`  - ${key}: ${link.href} (${link.title || ''})`);
  });
  return root._links;
}

// ============================================================================
// SYSTEM INFORMATION
// ============================================================================

/**
 * Get device information
 */
async function getSystemInfo() {
  console.log('Getting system info...');
  const result = await makeRequest('GET', '/system/info');
  console.log('System Info:', result.data);
  return result.data;
}

/**
 * Get system health status
 */
async function getHealthStatus() {
  console.log('Getting health status...');
  const result = await makeRequest('GET', '/system/health');
  console.log('Health Status:', result.data);
  return result.data;
}

/**
 * Get current system time and timezone
 */
async function getSystemTime() {
  console.log('Getting system time...');
  const result = await makeRequest('GET', '/system/time');
  console.log('System Time:', result.data);
  return result.data;
}

/**
 * Get network configuration
 */
async function getNetworkInfo() {
  console.log('Getting network info...');
  const result = await makeRequest('GET', '/system/network');
  console.log('Network Info:', result.data);
  return result.data;
}

/**
 * Get system statistics and resource usage
 */
async function getSystemStats() {
  console.log('Getting system stats...');
  const result = await makeRequest('GET', '/system/stats');
  console.log('System Stats:', result.data);
  return result.data;
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

/**
 * Get device configuration
 */
async function getDeviceConfig() {
  console.log('Getting device config...');
  const result = await makeRequest('GET', '/config/device');
  console.log('Device Config:', result.data);
  return result.data;
}

/**
 * Update device hostname
 */
async function updateHostname(newHostname) {
  console.log(`Updating hostname to: ${newHostname}`);
  const result = await makeRequest('PATCH', '/config/device', {
    hostname: newHostname
  });
  console.log('Hostname updated:', result.data);
  return result.data;
}

/**
 * Update timezone
 */
async function updateTimezone(timezone) {
  console.log(`Updating timezone to: ${timezone}`);
  const result = await makeRequest('PATCH', '/config/device', {
    ntp_timezone: timezone
  });
  console.log('Timezone updated:', result.data);
  return result.data;
}

/**
 * Get MQTT configuration
 */
async function getMQTTConfig() {
  console.log('Getting MQTT config...');
  const result = await makeRequest('GET', '/config/mqtt');
  console.log('MQTT Config:', result.data);
  return result.data;
}

/**
 * Update MQTT configuration
 */
async function updateMQTTConfig(config) {
  console.log('Updating MQTT config...', config);
  const result = await makeRequest('PATCH', '/config/mqtt', config);
  console.log('MQTT Config updated:', result.data);
  return result.data;
}

/**
 * Get feature availability
 */
async function getFeatures() {
  console.log('Getting available features...');
  const result = await makeRequest('GET', '/config/features');
  console.log('Features:', result.data);
  return result.data;
}

// ============================================================================
// OPENTHERM DATA
// ============================================================================

/**
 * Get gateway status
 */
async function getGatewayStatus() {
  console.log('Getting gateway status...');
  const result = await makeRequest('GET', '/otgw/status');
  console.log('Gateway Status:', result.data);
  return result.data;
}

/**
 * Get current OpenTherm data values
 */
async function getOTGWData() {
  console.log('Getting OTGW data...');
  const result = await makeRequest('GET', '/otgw/data');
  console.log('OTGW Data:', result.data);
  return result.data;
}

/**
 * Get all OpenTherm messages with pagination
 */
async function getOpenThermMessages(page = 0, perPage = 20) {
  console.log(`Getting OpenTherm messages (page ${page}, ${perPage} items)...`);
  const endpoint = `/otgw/messages?page=${page}&per_page=${perPage}`;
  const result = await makeRequest('GET', endpoint);
  console.log(`Messages:`, result.data.messages);
  console.log('Pagination:', {
    total: result.data.total,
    page: result.data.page,
    totalPages: result.data.total_pages
  });
  return result.data;
}

/**
 * Get messages by category (temperature, pressure, flow, status, setpoint)
 */
async function getMessagesByCategory(category) {
  console.log(`Getting ${category} messages...`);
  const result = await makeRequest('GET', `/otgw/messages?category=${category}`);
  console.log(`${category} Messages:`, result.data.messages);
  return result.data.messages;
}

/**
 * Get single message by ID
 */
async function getMessage(messageId) {
  console.log(`Getting message ${messageId}...`);
  const result = await makeRequest('GET', `/otgw/messages?id=${messageId}`);
  if (result.data.messages.length > 0) {
    console.log(`Message ${messageId}:`, result.data.messages[0]);
    return result.data.messages[0];
  } else {
    console.log(`Message ${messageId} not found`);
    return null;
  }
}

/**
 * Send command to OpenTherm Gateway
 */
async function sendOTGWCommand(command) {
  // Command format: XX=YYYY (e.g., "TT=21.5" for temperature)
  console.log(`Sending OTGW command: ${command}`);
  const result = await makeRequest('POST', '/otgw/command', { command });
  console.log('Command sent:', result.data);
  return result.data;
}

/**
 * Set temporary temperature (TT command)
 */
async function setTemperature(temperature) {
  console.log(`Setting temperature to: ${temperature}°C`);
  return sendOTGWCommand(`TT=${temperature}`);
}

/**
 * Override return temperature (TR command)
 */
async function setReturnTemperature(temperature) {
  console.log(`Setting return temperature to: ${temperature}°C`);
  return sendOTGWCommand(`TR=${temperature}`);
}

/**
 * Get OTmonitor format data
 */
async function getOTmonitorData() {
  console.log('Getting OTmonitor format data...');
  const result = await makeRequest('GET', '/otgw/monitor');
  console.log('OTmonitor Data:', result.data);
  return result.data;
}

// ============================================================================
// CACHING WITH ETAGS
// ============================================================================

// Store for ETag and data
let systemInfoCache = {
  data: null,
  etag: null,
  timestamp: null
};

/**
 * Get system info with ETag caching
 */
async function getCachedSystemInfo() {
  const headers = {};
  if (systemInfoCache.etag) {
    headers['If-None-Match'] = systemInfoCache.etag;
  }

  try {
    const response = await fetch(`${OTGW_URL}/system/info`, { headers });

    if (response.status === 304) {
      console.log('Cached data (304 Not Modified)');
      return systemInfoCache.data;
    }

    const data = await response.json();
    const etag = response.headers.get('ETag');

    // Update cache
    systemInfoCache = { data, etag, timestamp: Date.now() };
    console.log('Fresh data received, cache updated');
    return data;
  } catch (error) {
    console.error('Cache request failed:', error);
    throw error;
  }
}

/**
 * Clear cache
 */
function clearCache() {
  systemInfoCache = { data: null, etag: null, timestamp: null };
  console.log('Cache cleared');
}

// ============================================================================
// SENSORS
// ============================================================================

/**
 * Get Dallas temperature sensors
 */
async function getDallasSensors() {
  console.log('Getting Dallas sensors...');
  const result = await makeRequest('GET', '/sensors/dallas');
  console.log('Dallas Sensors:', result.data.sensors);
  return result.data.sensors;
}

/**
 * Get S0 pulse counter
 */
async function getS0Counter() {
  console.log('Getting S0 counter...');
  const result = await makeRequest('GET', '/sensors/s0');
  console.log('S0 Counter:', result.data);
  return result.data;
}

/**
 * Reset S0 pulse counter
 */
async function resetS0Counter() {
  console.log('Resetting S0 counter...');
  const result = await makeRequest('PUT', '/sensors/s0', {});
  console.log('Counter reset:', result.data);
  return result.data;
}

// ============================================================================
// PIC FIRMWARE
// ============================================================================

/**
 * Get PIC firmware information
 */
async function getPICInfo() {
  console.log('Getting PIC info...');
  const result = await makeRequest('GET', '/pic/info');
  console.log('PIC Info:', result.data);
  return result.data;
}

/**
 * Get PIC flash status
 */
async function getPICFlashStatus() {
  console.log('Getting PIC flash status...');
  const result = await makeRequest('GET', '/pic/flash');
  console.log('PIC Flash Status:', result.data);
  return result.data;
}

// ============================================================================
// DATA EXPORT
// ============================================================================

/**
 * Export settings as JSON
 */
async function exportSettings() {
  console.log('Exporting settings...');
  const result = await makeRequest('GET', '/export/settings');
  console.log('Settings:', result.data);
  return result.data;
}

/**
 * Get Telegraf metrics format
 */
async function exportTelegraf() {
  console.log('Getting Telegraf metrics...');
  const response = await fetch(`${OTGW_URL}/export/telegraf`);
  const data = await response.text();
  console.log('Telegraf Data:', data);
  return data;
}

/**
 * Get OTmonitor export format
 */
async function exportOTmonitor() {
  console.log('Getting OTmonitor export...');
  const result = await makeRequest('GET', '/export/otmonitor');
  console.log('OTmonitor Export:', result.data);
  return result.data;
}

/**
 * Get recent debug logs
 */
async function getLogs() {
  console.log('Getting recent logs...');
  const response = await fetch(`${OTGW_URL}/export/logs`);
  const data = await response.text();
  console.log('Logs:', data);
  return data;
}

// ============================================================================
// ERROR HANDLING EXAMPLES
// ============================================================================

/**
 * Example: Handle API errors gracefully
 */
async function exampleErrorHandling() {
  try {
    // Try to access non-existent resource
    await makeRequest('GET', '/resource/notfound');
  } catch (error) {
    console.error('Caught error:', error.message);
    // Handle error (e.g., show user message, retry, etc.)
  }
}

/**
 * Example: Validate command format
 */
async function exampleCommandValidation() {
  const command = 'TT=21.5'; // Format: XX=YYYY

  // Validate format
  if (!/^[A-Z]{2}=.*/.test(command)) {
    console.error('Invalid command format. Expected: XX=YYYY');
    return;
  }

  try {
    const result = await sendOTGWCommand(command);
    console.log('Command sent successfully:', result);
  } catch (error) {
    console.error('Failed to send command:', error);
  }
}

// ============================================================================
// MONITORING & AUTOMATION
// ============================================================================

/**
 * Monitor device health every 30 seconds
 */
let healthMonitorInterval = null;

function startHealthMonitor(interval = 30000) {
  console.log(`Starting health monitor (interval: ${interval}ms)`);
  healthMonitorInterval = setInterval(async () => {
    try {
      const health = await getHealthStatus();
      console.log(`[${new Date().toLocaleTimeString()}] Status: ${health.status}, Heap: ${health.heap_free} bytes`);
    } catch (error) {
      console.error('Health check failed:', error);
    }
  }, interval);
}

function stopHealthMonitor() {
  if (healthMonitorInterval) {
    clearInterval(healthMonitorInterval);
    healthMonitorInterval = null;
    console.log('Health monitor stopped');
  }
}

/**
 * Monitor temperature changes
 */
let tempMonitorInterval = null;
let lastTemp = null;

function startTemperatureMonitor(interval = 60000) {
  console.log(`Starting temperature monitor (interval: ${interval}ms)`);
  tempMonitorInterval = setInterval(async () => {
    try {
      const data = await getOTGWData();
      const currentTemp = parseFloat(data.boiler_temp);
      
      if (lastTemp !== null && Math.abs(currentTemp - lastTemp) > 1.0) {
        console.log(`[${new Date().toLocaleTimeString()}] Temperature changed: ${lastTemp}°C → ${currentTemp}°C`);
      }
      lastTemp = currentTemp;
    } catch (error) {
      console.error('Temperature check failed:', error);
    }
  }, interval);
}

function stopTemperatureMonitor() {
  if (tempMonitorInterval) {
    clearInterval(tempMonitorInterval);
    tempMonitorInterval = null;
    lastTemp = null;
    console.log('Temperature monitor stopped');
  }
}

// ============================================================================
// HELPER UTILITIES
// ============================================================================

/**
 * Pretty-print JSON
 */
function prettyPrint(obj) {
  console.log(JSON.stringify(obj, null, 2));
}

/**
 * Run all discovery examples
 */
async function runDiscoveryExamples() {
  console.log('=== Running Discovery Examples ===');
  try {
    await discoverAPI();
    await getSystemInfo();
    await getHealthStatus();
    await getNetworkInfo();
  } catch (error) {
    console.error('Examples failed:', error);
  }
}

/**
 * Run all data retrieval examples
 */
async function runDataExamples() {
  console.log('=== Running Data Examples ===');
  try {
    await getGatewayStatus();
    await getOTGWData();
    await getOpenThermMessages(0, 10);
    await getMessagesByCategory('temperature');
  } catch (error) {
    console.error('Examples failed:', error);
  }
}

// ============================================================================
// EXPORT FOR NODE.JS
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    // Discovery
    getAPIRoot,
    discoverAPI,
    
    // System
    getSystemInfo,
    getHealthStatus,
    getSystemTime,
    getNetworkInfo,
    getSystemStats,
    
    // Configuration
    getDeviceConfig,
    updateHostname,
    updateTimezone,
    getMQTTConfig,
    updateMQTTConfig,
    getFeatures,
    
    // OpenTherm
    getGatewayStatus,
    getOTGWData,
    getOpenThermMessages,
    getMessagesByCategory,
    getMessage,
    sendOTGWCommand,
    setTemperature,
    setReturnTemperature,
    getOTmonitorData,
    
    // Caching
    getCachedSystemInfo,
    clearCache,
    
    // Sensors
    getDallasSensors,
    getS0Counter,
    resetS0Counter,
    
    // PIC
    getPICInfo,
    getPICFlashStatus,
    
    // Export
    exportSettings,
    exportTelegraf,
    exportOTmonitor,
    getLogs,
    
    // Monitoring
    startHealthMonitor,
    stopHealthMonitor,
    startTemperatureMonitor,
    stopTemperatureMonitor,
    
    // Utilities
    prettyPrint,
    runDiscoveryExamples,
    runDataExamples
  };
}
