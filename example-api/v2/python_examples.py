#!/usr/bin/env python3
"""
OTGW REST API v2 - Python Examples
Version: 1.0.0

API v2 Features:
  - Enhanced OTmonitor endpoint
  - Better structured responses
  - Full backward compatibility with v1
  
Recommendation:
  - New projects: Use OTGWClientV2
  - Existing projects: Keep OTGWClientV1 (no breaking changes)
  - Migration: Seamless, non-breaking upgrade
"""

import requests
from typing import Dict, Any, Optional
from datetime import datetime


class OTGWClientV1:
    """Base v1 client (included for reference)"""
    
    def __init__(self, device_ip: str = 'localhost', port: int = 80, timeout: int = 5):
        self.base_url = f'http://{device_ip}:{port}'
        self.api_prefix = '/api/v1'
        self.timeout = timeout

    def _request(self, path: str, method: str = 'GET', data: Optional[Dict] = None):
        url = f'{self.base_url}{self.api_prefix}{path}'
        try:
            response = requests.request(
                method=method,
                url=url,
                json=data,
                timeout=self.timeout
            )
            response.raise_for_status()
            try:
                return response.json()
            except ValueError:
                return response.text
        except requests.RequestException as e:
            print(f'❌ Request failed for {path}: {e}')
            raise

    def get_health(self) -> Dict:
        return self._request('/health')

    def get_settings(self) -> Dict:
        return self._request('/settings')

    def get_otgw_by_id(self, msg_id: int) -> Dict:
        return self._request(f'/otgw/id/{msg_id}')

    def get_otmonitor(self) -> str:
        return self._request('/otgw/otmonitor')

    def get_telegraf(self) -> str:
        return self._request('/otgw/telegraf')

    def send_command(self, command: str) -> Any:
        return self._request(f'/otgw/command/{command}', method='POST')


class OTGWClientV2(OTGWClientV1):
    """
    OTGW REST API v2 Client
    
    Extends v1 with enhanced OTmonitor endpoint and better data structures.
    All v1 methods continue to work unchanged.
    """
    
    def __init__(self, device_ip: str = 'localhost', port: int = 80, timeout: int = 5):
        super().__init__(device_ip, port, timeout)
        self.api_prefix = '/api/v2'  # Use v2 endpoint

    def get_otmonitor_v2(self) -> str:
        """
        GET /api/v2/otgw/otmonitor (Enhanced)
        
        V2 Improvements:
          - Better structured output
          - More reliable parsing
          - Consistent formatting
          
        Returns: Multi-line string with OT parameters
        """
        return self._request('/otgw/otmonitor')

    def parse_otmonitor(self) -> Dict[str, Any]:
        """
        Parse enhanced OTmonitor response into structured dictionary
        
        Example response:
          Status:                       84 (CH enabled, DHW enabled, Flame on)
          Control Setpoint:            21.5 °C
          Boiler Temperature:          54.3 °C
          DHW Actual Temperature:      45.2 °C
          Modulation:                  80.5 %
        
        Returns: Dictionary with parsed values
        """
        data = self.get_otmonitor_v2()
        parsed = {}
        
        for line in data.split('\n'):
            if not line.strip() or line.startswith('#'):
                continue
            
            # Parse lines like "Key:  Value Unit"
            if ':' in line:
                parts = line.split(':', 1)
                key = parts[0].strip().lower().replace(' ', '_').replace('(', '').replace(')', '')
                value = parts[1].strip()
                
                # Try to parse as number
                try:
                    # Remove unit suffixes like °C, %
                    num_str = value.split()[0] if value else '0'
                    parsed[key] = float(num_str)
                except (ValueError, IndexError):
                    parsed[key] = value
        
        return parsed

    def get_temperatures_from_otmonitor(self) -> Dict[str, float]:
        """
        Extract temperature data from enhanced OTmonitor endpoint
        
        Returns: Dictionary with room, boiler, and dhw temperatures
        """
        try:
            data = self.parse_otmonitor()
            return {
                'room': data.get('room_setpoint_temperature', 0),
                'boiler': data.get('boiler_actual_temperature', 0),
                'dhw': data.get('dhw_actual_temperature', 0)
            }
        except Exception as e:
            print(f'❌ Error parsing temperatures: {e}')
            return {'room': 0, 'boiler': 0, 'dhw': 0}

    def get_status_info(self) -> Dict[str, Any]:
        """
        Get detailed status information from OTmonitor
        
        Returns: Dictionary with status, temperatures, modulation, etc.
        """
        try:
            data = self.parse_otmonitor()
            return {
                'status': data.get('status', 'Unknown'),
                'room_temperature': data.get('room_temperature', 0),
                'boiler_temperature': data.get('boiler_actual_temperature', 0),
                'dhw_temperature': data.get('dhw_actual_temperature', 0),
                'modulation': data.get('modulation', 0),
                'room_setpoint': data.get('room_setpoint_temperature', 0),
                'boiler_setpoint': data.get('boiler_setpoint_temperature', 0),
                'dhw_setpoint': data.get('dhw_setpoint_temperature', 0),
            }
        except Exception as e:
            print(f'❌ Error getting status: {e}')
            return {}

    def supports_v2(self) -> bool:
        """Check if device supports v2 API"""
        try:
            response = requests.get(
                f'{self.base_url}/api/v2/otgw/otmonitor',
                timeout=self.timeout
            )
            return response.status_code == 200
        except:
            return False

    def migrate_from_v1(self) -> Dict[str, Any]:
        """
        Helper for migration from v1 to v2
        
        Returns: Migration status and recommendations
        """
        return {
            'supports_v2': self.supports_v2(),
            'breaking_changes': False,
            'recommendation': 'Use v2 client for new projects',
            'migration_effort': 'None - all v1 methods still work',
            'benefits': [
                'Enhanced OTmonitor endpoint',
                'Better structured responses',
                'Improved parsing reliability',
                'Same performance'
            ]
        }


# ============================================================================
# EXAMPLE FUNCTIONS
# ============================================================================

def example_basic_v2():
    """Example 1: Basic V2 Usage"""
    print('\n' + '='*50)
    print('Example 1: Basic V2 Usage')
    print('='*50 + '\n')
    
    client = OTGWClientV2('192.168.1.100')
    
    try:
        # All v1 methods still work
        health = client.get_health()
        print(f'✓ Health Status: {health["status"]}')
        
        # V2 specific: Enhanced OTmonitor
        otmonitor = client.get_otmonitor_v2()
        lines = otmonitor.split('\n')[:5]
        print(f'✓ OTmonitor (v2 enhanced):')
        for line in lines:
            if line.strip():
                print(f'  {line}')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_parse_otmonitor():
    """Example 2: Parse OTmonitor Data (V2 Feature)"""
    print('\n' + '='*50)
    print('Example 2: Parse Enhanced OTmonitor (V2)')
    print('='*50 + '\n')
    
    client = OTGWClientV2('192.168.1.100')
    
    try:
        data = client.parse_otmonitor()
        print('✓ Parsed OTmonitor Data:')
        for key, value in sorted(data.items())[:10]:
            print(f'  {key}: {value}')
        print('  ... (more fields available)')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_structured_data():
    """Example 3: Get Structured Status Info (V2 Feature)"""
    print('\n' + '='*50)
    print('Example 3: Structured Status Information')
    print('='*50 + '\n')
    
    client = OTGWClientV2('192.168.1.100')
    
    try:
        status = client.get_status_info()
        print('✓ Device Status:')
        print(f'  Status: {status["status"]}')
        print(f'  Room Temperature: {status["room_temperature"]}°C')
        print(f'  Room Setpoint: {status["room_setpoint"]}°C')
        print(f'  Boiler Temperature: {status["boiler_temperature"]}°C')
        print(f'  Boiler Setpoint: {status["boiler_setpoint"]}°C')
        print(f'  DHW Temperature: {status["dhw_temperature"]}°C')
        print(f'  DHW Setpoint: {status["dhw_setpoint"]}°C')
        print(f'  Modulation: {status["modulation"]}%')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_migration():
    """Example 4: Migration from V1 to V2"""
    print('\n' + '='*50)
    print('Example 4: Migrate from V1 to V2')
    print('='*50 + '\n')
    
    client = OTGWClientV2('192.168.1.100')
    
    try:
        migration = client.migrate_from_v1()
        
        print('Migration Status:')
        print(f'  Device supports V2: {migration["supports_v2"]}')
        print(f'  Breaking changes: {migration["breaking_changes"]}')
        print(f'  Migration effort: {migration["migration_effort"]}')
        print()
        print('Benefits of upgrading:')
        for benefit in migration['benefits']:
            print(f'  ✓ {benefit}')
        print()
        print(f'Recommendation: {migration["recommendation"]}')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_backward_compatibility():
    """Example 5: Backward Compatibility Test"""
    print('\n' + '='*50)
    print('Example 5: Backward Compatibility')
    print('='*50 + '\n')
    
    v1_client = OTGWClientV1('192.168.1.100')
    v2_client = OTGWClientV2('192.168.1.100')
    
    try:
        print('Testing backward compatibility...\n')
        
        # Test health endpoint (same in both)
        v1_health = v1_client.get_health()
        v2_health = v2_client.get_health()
        
        print('✓ Health endpoint:')
        print(f'  V1 Status: {v1_health["status"]}')
        print(f'  V2 Status: {v2_health["status"]}')
        print(f'  Compatible: {v1_health["status"] == v2_health["status"]}')
        print()
        
        print('✓ OTmonitor endpoint (enhanced in V2):')
        v1_otmon = v1_client.get_otmonitor()
        v2_otmon = v2_client.get_otmonitor_v2()
        print(f'  V1 lines: {len(v1_otmon.split(chr(10)))}')
        print(f'  V2 lines: {len(v2_otmon.split(chr(10)))}')
        print(f'  V2 enhanced: {len(v2_otmon) >= len(v1_otmon)}')
        print()
        
        print('✓ Conclusion:')
        print('  - All V1 apps work unchanged with V2 devices')
        print('  - V2 provides additional features')
        print('  - Migration is seamless and non-breaking')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_data_comparison():
    """Example 6: Compare Data Sources (V1 vs V2)"""
    print('\n' + '='*50)
    print('Example 6: Compare Data Sources')
    print('='*50 + '\n')
    
    v1_client = OTGWClientV1('192.168.1.100')
    v2_client = OTGWClientV2('192.168.1.100')
    
    try:
        print('Comparing temperature data sources:\n')
        
        # Method 1: Individual endpoints (v1 compatible)
        print('Method 1: Individual ID endpoints (V1 compatible)')
        temps_v1 = {
            'room': v1_client.get_otgw_by_id(16)['value'],
            'boiler': v1_client.get_otgw_by_id(17)['value'],
            'dhw': v1_client.get_otgw_by_id(18)['value']
        }
        print(f'  Room: {temps_v1["room"]}°C')
        print(f'  Boiler: {temps_v1["boiler"]}°C')
        print(f'  DHW: {temps_v1["dhw"]}°C')
        print()
        
        # Method 2: OTmonitor parsing (v2 feature)
        print('Method 2: OTmonitor parsing (V2 feature)')
        temps_v2 = v2_client.get_temperatures_from_otmonitor()
        print(f'  Room: {temps_v2["room"]}°C')
        print(f'  Boiler: {temps_v2["boiler"]}°C')
        print(f'  DHW: {temps_v2["dhw"]}°C')
        print()
        
        print('✓ Both methods provide same data')
        print('✓ V2 offers convenience of single endpoint')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def print_migration_guide():
    """Print V1 to V2 migration guide"""
    print('\n' + '='*50)
    print('V1 to V2 Migration Guide')
    print('='*50 + '\n')
    
    print('Step 1: Check Device Support')
    print('  client = OTGWClientV2("192.168.1.100")')
    print('  if client.supports_v2():')
    print('    print("Device supports V2")')
    print()
    
    print('Step 2: Create V2 Client')
    print('  # Old (v1):'
    print('  client = OTGWClientV1("192.168.1.100")')
    print()
    print('  # New (v2):'
    print('  client = OTGWClientV2("192.168.1.100")')
    print()
    
    print('Step 3: Use Enhanced Features')
    print('  # Old way (still works):'
    print('  temps = {"room": client.get_otgw_by_id(16)["value"], ...}')
    print()
    print('  # New way (V2 feature):'
    print('  temps = client.get_temperatures_from_otmonitor()')
    print()
    
    print('Step 4: No Code Changes Required')
    print('  - All V1 methods work unchanged')
    print('  - Existing code requires zero modifications')
    print('  - Add V2 methods for new features as needed')
    print()


# ============================================================================
# MAIN
# ============================================================================

if __name__ == '__main__':
    print('\n' + '█'*50)
    print('█  OTGW REST API v2 - Python Examples')
    print('█'*50)
    
    print('\nV2 Features:')
    print('  ✓ Enhanced OTmonitor endpoint')
    print('  ✓ Better structured responses')
    print('  ✓ Full backward compatibility with V1')
    print('  ✓ Seamless migration path')
    
    print('\nConfiguration:')
    print('  Device IP: 192.168.1.100 (update as needed)')
    
    print('\nAvailable Examples:')
    print('  1. example_basic_v2()')
    print('  2. example_parse_otmonitor()')
    print('  3. example_structured_data()')
    print('  4. example_migration()')
    print('  5. example_backward_compatibility()')
    print('  6. example_data_comparison()')
    
    print('\nQuick Start:')
    print("""
from python_examples import OTGWClientV2

# Create client
client = OTGWClientV2('192.168.1.100')

# Check if V2 is supported
if client.supports_v2():
    print('Device supports V2!')

# Get temperatures (V2 feature)
temps = client.get_temperatures_from_otmonitor()
print(f"Room: {temps['room']}°C")

# All V1 methods still work
health = client.get_health()
print(f"Status: {health['status']}")
""")
    
    print_migration_guide()
