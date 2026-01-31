/**
 * OTGW REST API v1 - JavaScript Examples
 * Version: 1.0.0
 * 
 * This module provides JavaScript examples for the OTGW REST API v1.
 * Works in both Node.js and modern browsers.
 * 
 * Usage:
 *   // Node.js
 *   const OTGW = require('./javascript_examples.js');
 *   const client = new OTGW.OTGWClientV1('192.168.1.100');
 *   
 *   // Browser
 *   <script src="javascript_examples.js"></script>
 *   const client = new OTGWClientV1('192.168.1.100');
 */

class OTGWClientV1 {
  constructor(deviceIp = 'localhost', port = 80) {
    this.baseUrl = `http://${deviceIp}:${port}`;
    this.apiPrefix = '/api/v1';
    this.timeout = 5000;
  }

  /**
   * Make HTTP request with error handling
   */
  async request(path, options = {}) {
    const url = `${this.baseUrl}${this.apiPrefix}${path}`;
    const fetchOptions = {
      timeout: this.timeout,
      ...options
    };

    try {
      const response = await fetch(url, fetchOptions);
      
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const contentType = response.headers.get('content-type');
      if (contentType && contentType.includes('application/json')) {
        return await response.json();
      }
      
      return await response.text();
    } catch (error) {
      console.error(`Request failed for ${path}:`, error.message);
      throw error;
    }
  }

  /**
   * GET /api/v1/health
   * Check device health status
   */
  async getHealth() {
    return this.request('/health');
  }

  /**
   * GET /api/v1/flashstatus
   * Get unified flash status for ESP and PIC
   */
  async getFlashStatus() {
    return this.request('/flashstatus');
  }

  /**
   * GET /api/v1/pic/flashstatus
   * Poll PIC flash status during upgrade
   */
  async getPICFlashStatus() {
    return this.request('/pic/flashstatus');
  }

  /**
   * GET /api/v1/settings
   * Retrieve all device settings
   */
  async getSettings() {
    return this.request('/settings');
  }

  /**
   * POST /api/v1/settings
   * Update device settings
   */
  async updateSettings(settings) {
    return this.request('/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(settings)
    });
  }

  /**
   * GET /api/v1/otgw/telegraf
   * Export OpenTherm data in Telegraf/InfluxDB format
   */
  async getTelegraf() {
    return this.request('/otgw/telegraf');
  }

  /**
   * GET /api/v1/otgw/otmonitor
   * Export OpenTherm data in OTmonitor format
   */
  async getOTmonitor() {
    return this.request('/otgw/otmonitor');
  }

  /**
   * GET /api/v1/otgw/id/{msgid}
   * Get OpenTherm value by message ID
   */
  async getOTGWById(msgId) {
    return this.request(`/otgw/id/${msgId}`);
  }

  /**
   * GET /api/v1/otgw/label/{label}
   * Get OpenTherm value by label
   */
  async getOTGWByLabel(label) {
    return this.request(`/otgw/label/${label}`);
  }

  /**
   * POST /api/v1/otgw/command/{command}
   * Send OTGW command to boiler
   * 
   * Examples:
   *   CS=1     - Enable Central Heating
   *   CS=0     - Disable Central Heating
   *   TT=22.0  - Set room temperature
   *   TR=50.0  - Set DHW setpoint
   */
  async sendCommand(command) {
    return this.request(`/otgw/command/${command}`, {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' }
    });
  }

  /**
   * POST /api/v1/otgw/autoconfigure
   * Trigger Home Assistant MQTT Auto Discovery
   */
  async autoConfigure() {
    return this.request('/otgw/autoconfigure', {
      method: 'POST'
    });
  }

  /**
   * Monitor device health with periodic polling
   */
  async monitorHealth(intervalSeconds = 30, onUpdate) {
    setInterval(async () => {
      try {
        const health = await this.getHealth();
        console.log(`[${new Date().toLocaleTimeString()}] Health:`, health);
        
        if (onUpdate) {
          onUpdate(health);
        }

        // Alert on problems
        if (health.status !== 'UP' || !health.otgw_connected) {
          console.warn('⚠️ Device health warning:', health);
        }
      } catch (error) {
        console.error('Health check failed:', error.message);
      }
    }, intervalSeconds * 1000);
  }

  /**
   * Get current room and boiler temperatures
   */
  async getTemperatures() {
    const [roomTemp, boilerTemp, dhwTemp] = await Promise.all([
      this.getOTGWById(16),
      this.getOTGWById(17),
      this.getOTGWById(18)
    ]);

    return {
      room: roomTemp.value,
      boiler: boilerTemp.value,
      dhw: dhwTemp.value
    };
  }

  /**
   * Set room temperature via temporary override
   */
  async setRoomTemperature(temp) {
    return this.sendCommand(`TT=${temp.toFixed(1)}`);
  }

  /**
   * Enable Central Heating
   */
  async enableHeating() {
    return this.sendCommand('CS=1');
  }

  /**
   * Disable Central Heating
   */
  async disableHeating() {
    return this.sendCommand('CS=0');
  }

  /**
   * Enable DHW (Domestic Hot Water)
   */
  async enableDHW() {
    return this.sendCommand('SW=1');
  }

  /**
   * Disable DHW
   */
  async disableDHW() {
    return this.sendCommand('SW=0');
  }

  /**
   * Check if device is healthy
   */
  async isHealthy() {
    try {
      const health = await this.getHealth();
      return health.status === 'UP' && health.otgw_connected && health.picavailable;
    } catch {
      return false;
    }
  }
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

/**
 * Example 1: Basic device status
 */
async function example_basicStatus() {
  console.log('\n=== Example 1: Basic Device Status ===\n');
  
  const client = new OTGWClientV1('192.168.1.100');
  
  try {
    const health = await client.getHealth();
    console.log('Device Health:', health);
    console.log(`  Status: ${health.status}`);
    console.log(`  OTGW Connected: ${health.otgw_connected}`);
    console.log(`  PIC Available: ${health.picavailable}`);
    console.log(`  Heap Free: ${health.heap_free} bytes`);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 2: Get current temperatures
 */
async function example_temperatures() {
  console.log('\n=== Example 2: Get Current Temperatures ===\n');
  
  const client = new OTGWClientV1('192.168.1.100');
  
  try {
    const temps = await client.getTemperatures();
    console.log('Current Temperatures:');
    console.log(`  Room: ${temps.room}°C`);
    console.log(`  Boiler: ${temps.boiler}°C`);
    console.log(`  DHW: ${temps.dhw}°C`);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 3: Control heating
 */
async function example_controlHeating() {
  console.log('\n=== Example 3: Control Heating ===\n');
  
  const client = new OTGWClientV1('192.168.1.100');
  
  try {
    // Set room temperature to 22°C
    console.log('Setting room temperature to 22°C...');
    await client.setRoomTemperature(22.0);
    console.log('✓ Temperature set');

    // Enable heating if not already
    console.log('Enabling heating...');
    await client.enableHeating();
    console.log('✓ Heating enabled');

    // Check current temperatures
    const temps = await client.getTemperatures();
    console.log('Current temperatures:', temps);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 4: Device settings management
 */
async function example_settingsManagement() {
  console.log('\n=== Example 4: Device Settings ===\n');
  
  const client = new OTGWClientV1('192.168.1.100');
  
  try {
    // Get current settings
    console.log('Fetching device settings...');
    const settings = await client.getSettings();
    console.log('Current Settings:');
    console.log(`  Hostname: ${settings.hostname}`);
    console.log(`  MQTT Broker: ${settings.mqtt_broker}:${settings.mqtt_port}`);
    console.log(`  NTP Server: ${settings.ntp_server}`);
    console.log(`  Timezone: ${settings.tz_info}`);

    // Update settings
    console.log('\nUpdating settings...');
    const updated = await client.updateSettings({
      hostname: 'OpenTherm-Gateway',
      mqtt_broker: '192.168.1.50',
      mqtt_port: 1883,
      ntp_server: 'pool.ntp.org'
    });
    console.log('✓ Settings updated:', updated);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 5: Monitoring dashboard
 */
async function example_monitoringDashboard() {
  console.log('\n=== Example 5: Monitoring Dashboard ===\n');
  
  const client = new OTGWClientV1('192.168.1.100');
  
  try {
    // Check initial health
    const health = await client.getHealth();
    const temps = await client.getTemperatures();
    
    console.log('╔════════════════════════════════════════╗');
    console.log('║   OTGW Device Monitoring Dashboard    ║');
    console.log('╠════════════════════════════════════════╣');
    console.log(`║ Status: ${health.status.padEnd(30)} ║`);
    console.log(`║ OTGW Connected: ${String(health.otgw_connected).padEnd(22)} ║`);
    console.log(`║ PIC Available: ${String(health.picavailable).padEnd(23)} ║`);
    console.log('├────────────────────────────────────────┤');
    console.log(`║ Room Temperature: ${String(temps.room + '°C').padEnd(18)} ║`);
    console.log(`║ Boiler Temperature: ${String(temps.boiler + '°C').padEnd(16)} ║`);
    console.log(`║ DHW Temperature: ${String(temps.dhw + '°C').padEnd(19)} ║`);
    console.log('├────────────────────────────────────────┤');
    console.log(`║ Heap Free: ${String(health.heap_free + ' bytes').padEnd(25)} ║`);
    console.log(`║ WiFi RSSI: ${String(health.wifi_rssi + ' dBm').padEnd(25)} ║`);
    console.log('╚════════════════════════════════════════╝');

    // Monitor in real-time
    console.log('\nStarting 30-second monitoring loop...');
    let count = 0;
    const maxIterations = 3;

    const interval = setInterval(async () => {
      count++;
      if (count > maxIterations) {
        clearInterval(interval);
        console.log('Monitoring stopped.');
        return;
      }

      try {
        const health = await client.getHealth();
        const temps = await client.getTemperatures();
        
        console.log(`[${new Date().toLocaleTimeString()}] ` +
          `Status: ${health.status} | ` +
          `Room: ${temps.room}°C | ` +
          `Boiler: ${temps.boiler}°C | ` +
          `Heap: ${health.heap_free}B`);
      } catch (error) {
        console.error('Monitoring error:', error.message);
      }
    }, 30000);

  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 6: Export data format
 */
async function example_exportFormats() {
  console.log('\n=== Example 6: Export Data Formats ===\n');
  
  const client = new OTGWClientV1('192.168.1.100');
  
  try {
    // Telegraf/InfluxDB format
    console.log('Telegraf Format (InfluxDB line protocol):');
    const telegraf = await client.getTelegraf();
    console.log(telegraf.split('\n').slice(0, 3).join('\n'));
    console.log('...\n');

    // OTmonitor format
    console.log('OTmonitor Format:');
    const otmonitor = await client.getOTmonitor();
    console.log(otmonitor.split('\n').slice(0, 5).join('\n'));
    console.log('...\n');
  } catch (error) {
    console.error('Error:', error.message);
  }
}

// Export for Node.js
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    OTGWClientV1,
    example_basicStatus,
    example_temperatures,
    example_controlHeating,
    example_settingsManagement,
    example_monitoringDashboard,
    example_exportFormats
  };
}
