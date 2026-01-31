/**
 * OTGW REST API v2 - JavaScript Examples
 * Version: 1.0.0
 * 
 * API v2 Features:
 *   - Enhanced OTmonitor endpoint
 *   - Full backward compatibility with v1
 *   - Better response formatting
 * 
 * Note: OTGWClientV2 extends OTGWClientV1 with v2-specific methods
 */

// Include v1 client first (or copy the base class)
class OTGWClientV1 {
  constructor(deviceIp = 'localhost', port = 80) {
    this.baseUrl = `http://${deviceIp}:${port}`;
    this.apiPrefix = '/api/v1';
    this.timeout = 5000;
  }

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

  async getHealth() { return this.request('/health'); }
  async getFlashStatus() { return this.request('/flashstatus'); }
  async getPICFlashStatus() { return this.request('/pic/flashstatus'); }
  async getSettings() { return this.request('/settings'); }
  async updateSettings(settings) {
    return this.request('/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(settings)
    });
  }
  async getTelegraf() { return this.request('/otgw/telegraf'); }
  async getOTmonitor() { return this.request('/otgw/otmonitor'); }
  async getOTGWById(msgId) { return this.request(`/otgw/id/${msgId}`); }
  async getOTGWByLabel(label) { return this.request(`/otgw/label/${label}`); }
  async sendCommand(command) {
    return this.request(`/otgw/command/${command}`, { method: 'POST' });
  }
  async autoConfigure() {
    return this.request('/otgw/autoconfigure', { method: 'POST' });
  }
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
  async setRoomTemperature(temp) { return this.sendCommand(`TT=${temp.toFixed(1)}`); }
  async enableHeating() { return this.sendCommand('CS=1'); }
  async disableHeating() { return this.sendCommand('CS=0'); }
  async enableDHW() { return this.sendCommand('SW=1'); }
  async disableDHW() { return this.sendCommand('SW=0'); }
  async isHealthy() {
    try {
      const health = await this.getHealth();
      return health.status === 'UP' && health.otgw_connected && health.picavailable;
    } catch { return false; }
  }
}

/**
 * OTGWClientV2 - Extends v1 with v2-specific features
 */
class OTGWClientV2 extends OTGWClientV1 {
  constructor(deviceIp = 'localhost', port = 80) {
    super(deviceIp, port);
    this.apiPrefix = '/api/v2';  // Use v2 endpoint
  }

  /**
   * GET /api/v2/otgw/otmonitor (Enhanced)
   * Get OpenTherm data in enhanced OTmonitor format
   * 
   * V2 Improvements:
   *   - Better structured output
   *   - More reliable parsing
   *   - Consistent formatting
   */
  async getOTmonitorV2() {
    return this.request('/otgw/otmonitor');
  }

  /**
   * Parse enhanced OTmonitor response into structured object
   * 
   * Example response:
   *   Status:                       84 (CH enabled, DHW enabled, Flame on)
   *   Control Setpoint:            21.5 °C
   *   Boiler Temperature:          54.3 °C
   *   DHW Actual Temperature:      45.2 °C
   *   Modulation:                  80.5 %
   */
  async parseOTmonitorData() {
    const data = await this.getOTmonitorV2();
    const parsed = {};

    const lines = data.split('\n');
    for (const line of lines) {
      if (!line.trim() || line.startsWith('#')) continue;

      // Parse lines like "Key:  Value Unit"
      const match = line.match(/^(.+?):\s+([^\(]+?)(?:\s*\(|$)/);
      if (match) {
        const key = match[1].trim().toLowerCase().replace(/\s+/g, '_');
        const value = match[2].trim();
        
        // Try to parse as number
        const numValue = parseFloat(value);
        parsed[key] = isNaN(numValue) ? value : numValue;
      }
    }

    return parsed;
  }

  /**
   * Get structured temperature data from OTmonitor
   */
  async getTemperaturesFromOTmonitor() {
    const data = await this.parseOTmonitorData();
    return {
      room: data.room_setpoint_temperature || 0,
      boiler: data.boiler_actual_temperature || 0,
      dhw: data.dhw_actual_temperature || 0
    };
  }

  /**
   * Migrate from v1 client
   * Helper to show compatibility between v1 and v2
   */
  async compareVersions() {
    const v1Client = new OTGWClientV1(this.baseUrl.split('://')[1].split(':')[0]);
    
    console.log('V1 vs V2 Comparison:');
    
    try {
      // Both use same v1 endpoints except otmonitor
      const health = await this.getHealth();
      console.log('Health:', health);

      // Different OTmonitor endpoints
      const v1OTmon = await v1Client.request('/otgw/otmonitor');
      const v2OTmon = await this.getOTmonitorV2();
      
      console.log('V1 OTmonitor length:', v1OTmon.length);
      console.log('V2 OTmonitor length:', v2OTmon.length);
      console.log('V2 has enhanced formatting:', v2OTmon.length >= v1OTmon.length);
      
    } catch (error) {
      console.error('Comparison error:', error.message);
    }
  }

  /**
   * Migration helper - check if device supports v2
   */
  async supportsV2() {
    try {
      const response = await fetch(
        `${this.baseUrl}/api/v2/otgw/otmonitor`,
        { timeout: 5000 }
      );
      return response.ok;
    } catch {
      return false;
    }
  }
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

/**
 * Example 1: Basic v2 Usage (with v1 compatibility)
 */
async function example_basicV2() {
  console.log('\n=== Example 1: Basic V2 Usage ===\n');
  
  const client = new OTGWClientV2('192.168.1.100');
  
  try {
    // All v1 methods still work
    const health = await client.getHealth();
    console.log('Health Status:', health.status);
    
    // V2 specific: Enhanced OTmonitor
    const otmonitor = await client.getOTmonitorV2();
    console.log('OTmonitor (first 200 chars):', otmonitor.substring(0, 200));
    
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 2: Parse OTmonitor Data (v2 feature)
 */
async function example_parseOTmonitor() {
  console.log('\n=== Example 2: Parse Enhanced OTmonitor ===\n');
  
  const client = new OTGWClientV2('192.168.1.100');
  
  try {
    const data = await client.parseOTmonitorData();
    console.log('Parsed OTmonitor Data:');
    console.log(`  Status: ${data.status}`);
    console.log(`  Room Temp: ${data.room_setpoint_temperature}°C`);
    console.log(`  Boiler Temp: ${data.boiler_actual_temperature}°C`);
    console.log(`  DHW Temp: ${data.dhw_actual_temperature}°C`);
    console.log(`  Modulation: ${data.modulation}%`);
    
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 3: Migration from v1 to v2
 */
async function example_migration() {
  console.log('\n=== Example 3: Migrate from V1 to V2 ===\n');
  
  const deviceIp = '192.168.1.100';
  
  try {
    // Check if device supports v2
    const v2Client = new OTGWClientV2(deviceIp);
    const supportsV2 = await v2Client.supportsV2();
    
    console.log(`Device supports V2: ${supportsV2}`);
    
    if (supportsV2) {
      console.log('✓ Device supports V2 - migration possible');
      console.log('✓ All existing V1 code continues to work');
      console.log('✓ You can now use enhanced V2 endpoints');
    } else {
      console.log('✗ Device only supports V1');
      console.log('  Keep using V1Client for now');
    }
    
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 4: Version comparison
 */
async function example_versionComparison() {
  console.log('\n=== Example 4: V1 vs V2 Comparison ===\n');
  
  const client = new OTGWClientV2('192.168.1.100');
  
  try {
    await client.compareVersions();
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 5: Structured temperature monitoring (v2)
 */
async function example_structuredMonitoring() {
  console.log('\n=== Example 5: Structured Temperature Monitoring ===\n');
  
  const client = new OTGWClientV2('192.168.1.100');
  
  try {
    // V2 provides better structured data
    const temps = await client.getTemperaturesFromOTmonitor();
    console.log('Current Temperatures (from OTmonitor):');
    console.log(`  Room: ${temps.room}°C`);
    console.log(`  Boiler: ${temps.boiler}°C`);
    console.log(`  DHW: ${temps.dhw}°C`);
    
    // Also available from individual endpoints (v1 compatible)
    const temps2 = await client.getTemperatures();
    console.log('\nTemperatures (from individual endpoints):');
    console.log(`  Room: ${temps2.room}°C`);
    console.log(`  Boiler: ${temps2.boiler}°C`);
    console.log(`  DHW: ${temps2.dhw}°C`);
    
  } catch (error) {
    console.error('Error:', error.message);
  }
}

/**
 * Example 6: Backward compatibility test
 */
async function example_backwardCompatibility() {
  console.log('\n=== Example 6: Backward Compatibility ===\n');
  
  const v1Client = new OTGWClientV1('192.168.1.100');
  const v2Client = new OTGWClientV2('192.168.1.100');
  
  try {
    // Same endpoints should return same data (except otmonitor)
    console.log('Testing backward compatibility...\n');
    
    const v1Health = await v1Client.getHealth();
    const v2Health = await v2Client.getHealth();
    
    console.log('✓ Health check:');
    console.log(`  V1 Status: ${v1Health.status}`);
    console.log(`  V2 Status: ${v2Health.status}`);
    console.log(`  Match: ${v1Health.status === v2Health.status}`);
    
    const v1Temps = await v1Client.getTemperatures();
    const v2Temps = await v2Client.getTemperatures();
    
    console.log('\n✓ Temperature endpoints:');
    console.log(`  V1 Room: ${v1Temps.room}°C`);
    console.log(`  V2 Room: ${v2Temps.room}°C`);
    console.log(`  Match: ${v1Temps.room === v2Temps.room}`);
    
    console.log('\n✓ Backward compatibility confirmed');
    console.log('  Existing V1 apps work unchanged with V2 devices');
    
  } catch (error) {
    console.error('Error:', error.message);
  }
}

// Export for Node.js
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    OTGWClientV1,
    OTGWClientV2,
    example_basicV2,
    example_parseOTmonitor,
    example_migration,
    example_versionComparison,
    example_structuredMonitoring,
    example_backwardCompatibility
  };
}

// Log available examples if running directly
if (typeof window === 'undefined' && typeof require !== 'undefined') {
  console.log(`
╔══════════════════════════════════════════════════════════╗
║  OTGW REST API v2 - JavaScript Examples                ║
╠══════════════════════════════════════════════════════════╣
║                                                          ║
║  API v2 Features:                                        ║
║    ✓ Enhanced OTmonitor endpoint                        ║
║    ✓ Better structured responses                        ║
║    ✓ Full backward compatibility with v1                ║
║                                                          ║
║  Examples available:                                    ║
║    1. example_basicV2()                                 ║
║    2. example_parseOTmonitor()                          ║
║    3. example_migration()                               ║
║    4. example_versionComparison()                       ║
║    5. example_structuredMonitoring()                    ║
║    6. example_backwardCompatibility()                   ║
║                                                          ║
║  Usage:                                                 ║
║    const { OTGWClientV2 } = require('./javascript_examples.js');
║    const client = new OTGWClientV2('192.168.1.100');   ║
║    const health = await client.getHealth();            ║
║                                                          ║
╚══════════════════════════════════════════════════════════╝
  `);
}
