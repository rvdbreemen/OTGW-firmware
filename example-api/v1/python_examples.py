#!/usr/bin/env python3
"""
OTGW REST API v1 - Python Examples
Version: 1.0.0

This module provides Python examples for the OTGW REST API v1.
Compatible with Python 3.6+

Usage:
    python3 python_examples.py
    
    Or import as module:
    from python_examples import OTGWClientV1
    client = OTGWClientV1('192.168.1.100')
"""

import json
import time
import requests
from typing import Dict, Any, Optional
from datetime import datetime


class OTGWClientV1:
    """OTGW REST API v1 Client for Python"""
    
    def __init__(self, device_ip: str = 'localhost', port: int = 80, timeout: int = 5):
        """
        Initialize OTGW API client
        
        Args:
            device_ip: IP address of OTGW device
            port: HTTP port (default 80)
            timeout: Request timeout in seconds
        """
        self.base_url = f'http://{device_ip}:{port}'
        self.api_prefix = '/api/v1'
        self.timeout = timeout
        self.session = requests.Session()

    def _request(self, path: str, method: str = 'GET', 
                 data: Optional[Dict] = None, **kwargs) -> Any:
        """
        Make HTTP request with error handling
        
        Args:
            path: API endpoint path
            method: HTTP method (GET, POST, etc.)
            data: JSON data for POST/PUT requests
            **kwargs: Additional requests arguments
            
        Returns:
            Response JSON or text
        """
        url = f'{self.base_url}{self.api_prefix}{path}'
        
        try:
            headers = kwargs.pop('headers', {})
            
            response = self.session.request(
                method=method,
                url=url,
                headers=headers,
                json=data,
                timeout=self.timeout,
                **kwargs
            )
            
            response.raise_for_status()
            
            # Try to parse as JSON, fall back to text
            try:
                return response.json()
            except ValueError:
                return response.text
                
        except requests.RequestException as e:
            print(f'❌ Request failed for {path}: {e}')
            raise

    def get_health(self) -> Dict:
        """GET /api/v1/health - Check device health"""
        return self._request('/health')

    def get_flash_status(self) -> Dict:
        """GET /api/v1/flashstatus - Get unified flash status"""
        return self._request('/flashstatus')

    def get_pic_flash_status(self) -> Dict:
        """GET /api/v1/pic/flashstatus - Poll PIC flash status"""
        return self._request('/pic/flashstatus')

    def get_settings(self) -> Dict:
        """GET /api/v1/settings - Get device settings"""
        return self._request('/settings')

    def update_settings(self, settings: Dict) -> Any:
        """POST /api/v1/settings - Update device settings"""
        return self._request('/settings', method='POST', data=settings)

    def get_telegraf(self) -> str:
        """GET /api/v1/otgw/telegraf - Export Telegraf/InfluxDB format"""
        return self._request('/otgw/telegraf')

    def get_otmonitor(self) -> str:
        """GET /api/v1/otgw/otmonitor - Export OTmonitor format"""
        return self._request('/otgw/otmonitor')

    def get_otgw_by_id(self, msg_id: int) -> Dict:
        """GET /api/v1/otgw/id/{msgid} - Get OT value by message ID"""
        return self._request(f'/otgw/id/{msg_id}')

    def get_otgw_by_label(self, label: str) -> Dict:
        """GET /api/v1/otgw/label/{label} - Get OT value by label"""
        return self._request(f'/otgw/label/{label}')

    def send_command(self, command: str) -> Any:
        """POST /api/v1/otgw/command/{command} - Send OTGW command"""
        return self._request(f'/otgw/command/{command}', method='POST')

    def auto_configure(self) -> Any:
        """POST /api/v1/otgw/autoconfigure - Trigger HA auto-discovery"""
        return self._request('/otgw/autoconfigure', method='POST')

    def get_temperatures(self) -> Dict[str, float]:
        """Get current room, boiler, and DHW temperatures"""
        # Message IDs: 16=Room Setpoint, 17=Boiler Temp, 18=DHW Setpoint
        room = self.get_otgw_by_id(16)['value']
        boiler = self.get_otgw_by_id(17)['value']
        dhw = self.get_otgw_by_id(18)['value']
        
        return {
            'room': room,
            'boiler': boiler,
            'dhw': dhw
        }

    def set_room_temperature(self, temp: float) -> Any:
        """Set room temperature via temporary override (TT command)"""
        return self.send_command(f'TT={temp:.1f}')

    def enable_heating(self) -> Any:
        """Enable Central Heating (CS=1)"""
        return self.send_command('CS=1')

    def disable_heating(self) -> Any:
        """Disable Central Heating (CS=0)"""
        return self.send_command('CS=0')

    def enable_dhw(self) -> Any:
        """Enable DHW (SW=1)"""
        return self.send_command('SW=1')

    def disable_dhw(self) -> Any:
        """Disable DHW (SW=0)"""
        return self.send_command('SW=0')

    def is_healthy(self) -> bool:
        """Check if device is healthy"""
        try:
            health = self.get_health()
            return (health.get('status') == 'UP' and 
                    health.get('otgw_connected') and 
                    health.get('picavailable'))
        except:
            return False

    def monitor_health(self, interval: int = 30, duration: Optional[int] = None):
        """
        Monitor device health periodically
        
        Args:
            interval: Check interval in seconds
            duration: Total monitoring duration in seconds (None = infinite)
        """
        start_time = time.time()
        
        try:
            while True:
                if duration and (time.time() - start_time) > duration:
                    break
                    
                try:
                    health = self.get_health()
                    temps = self.get_temperatures()
                    
                    timestamp = datetime.now().strftime('%H:%M:%S')
                    print(f'[{timestamp}] Health: {health["status"]} | '
                          f'Room: {temps["room"]}°C | '
                          f'Boiler: {temps["boiler"]}°C | '
                          f'Heap: {health["heap_free"]}B')
                    
                    # Warn on issues
                    if health['status'] != 'UP' or not health['otgw_connected']:
                        print(f'⚠️ Warning: Device health issue detected!')
                        
                except Exception as e:
                    print(f'❌ Monitoring error: {e}')
                
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print('\n✓ Monitoring stopped.')


# ============================================================================
# EXAMPLE FUNCTIONS
# ============================================================================

def example_basic_status():
    """Example 1: Get basic device status"""
    print('\n' + '='*50)
    print('Example 1: Basic Device Status')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    try:
        health = client.get_health()
        print('✓ Device Health:')
        print(f'  Status: {health["status"]}')
        print(f'  OTGW Connected: {health["otgw_connected"]}')
        print(f'  PIC Available: {health["picavailable"]}')
        print(f'  Heap Free: {health["heap_free"]} bytes')
        print(f'  WiFi Signal: {health["wifi_rssi"]} dBm')
        print(f'  Uptime: {health["uptime"]} seconds')
    except Exception as e:
        print(f'❌ Error: {e}')


def example_temperatures():
    """Example 2: Get current temperatures"""
    print('\n' + '='*50)
    print('Example 2: Current Temperatures')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    try:
        temps = client.get_temperatures()
        print('✓ Current Temperatures:')
        print(f'  Room: {temps["room"]}°C')
        print(f'  Boiler: {temps["boiler"]}°C')
        print(f'  DHW: {temps["dhw"]}°C')
    except Exception as e:
        print(f'❌ Error: {e}')


def example_control_heating():
    """Example 3: Control heating system"""
    print('\n' + '='*50)
    print('Example 3: Control Heating')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    try:
        # Set room temperature
        print('Setting room temperature to 22°C...')
        client.set_room_temperature(22.0)
        print('✓ Temperature set')
        
        # Enable heating
        print('Enabling heating...')
        client.enable_heating()
        print('✓ Heating enabled')
        
        # Get current status
        temps = client.get_temperatures()
        print(f'Current temperatures: {temps}')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_settings():
    """Example 4: Manage device settings"""
    print('\n' + '='*50)
    print('Example 4: Device Settings Management')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    try:
        # Get current settings
        print('Fetching device settings...')
        settings = client.get_settings()
        print('✓ Current Settings:')
        for key in ['hostname', 'mqtt_broker', 'mqtt_port', 'ntp_server', 'tz_info']:
            if key in settings:
                print(f'  {key}: {settings[key]}')
        
        # Update settings
        print('\nUpdating settings...')
        new_settings = {
            'hostname': 'OpenTherm-Gateway',
            'mqtt_broker': '192.168.1.50',
            'mqtt_port': 1883,
            'ntp_server': 'pool.ntp.org'
        }
        result = client.update_settings(new_settings)
        print(f'✓ Settings updated')
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_monitoring():
    """Example 5: Monitor device health"""
    print('\n' + '='*50)
    print('Example 5: Device Monitoring (10 seconds)')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    print('Starting monitoring (polling every 3 seconds)...')
    print('(Press Ctrl+C to stop)\n')
    
    try:
        client.monitor_health(interval=3, duration=10)
    except Exception as e:
        print(f'❌ Error: {e}')


def example_data_formats():
    """Example 6: Export data in different formats"""
    print('\n' + '='*50)
    print('Example 6: Data Export Formats')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    try:
        # Telegraf/InfluxDB format
        print('Telegraf Format (InfluxDB line protocol):')
        telegraf = client.get_telegraf()
        print(telegraf.split('\n')[0:3])
        for line in telegraf.split('\n')[0:3]:
            print(f'  {line}')
        print()
        
        # OTmonitor format
        print('OTmonitor Format:')
        otmonitor = client.get_otmonitor()
        for line in otmonitor.split('\n')[0:5]:
            print(f'  {line}')
        print()
        
    except Exception as e:
        print(f'❌ Error: {e}')


def example_polling_pattern():
    """Example 7: Polling pattern with error handling"""
    print('\n' + '='*50)
    print('Example 7: Polling Pattern with Error Handling')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    print('Starting polling with error recovery...')
    print('(Polling every 10 seconds, 30 second max)\n')
    
    start_time = time.time()
    poll_count = 0
    
    while (time.time() - start_time) < 30:
        try:
            if client.is_healthy():
                health = client.get_health()
                temps = client.get_temperatures()
                
                poll_count += 1
                timestamp = datetime.now().strftime('%H:%M:%S')
                print(f'[{timestamp}] Poll #{poll_count}: '
                      f'Room={temps["room"]}°C, '
                      f'Boiler={temps["boiler"]}°C, '
                      f'Heap={health["heap_free"]}B')
            else:
                print('⚠️ Device not healthy')
                
        except Exception as e:
            print(f'❌ Poll error: {e}')
        
        time.sleep(10)
    
    print(f'\n✓ Polling completed ({poll_count} successful polls)')


def example_dashboard():
    """Example 8: Display monitoring dashboard"""
    print('\n' + '='*50)
    print('Example 8: Monitoring Dashboard')
    print('='*50 + '\n')
    
    client = OTGWClientV1('192.168.1.100')
    
    try:
        health = client.get_health()
        temps = client.get_temperatures()
        
        # ASCII dashboard
        dashboard = f"""
╔════════════════════════════════════════════════════════╗
║          OTGW Device Monitoring Dashboard             ║
╠════════════════════════════════════════════════════════╣
║ Status                     │ {health['status']:40} ║
║ OTGW Connected             │ {str(health['otgw_connected']):40} ║
║ PIC Available              │ {str(health['picavailable']):40} ║
╠════════════════════════════════════════════════════════╣
║ Room Temperature           │ {temps['room']:>5.1f}°C {' '*32}║
║ Boiler Temperature         │ {temps['boiler']:>5.1f}°C {' '*32}║
║ DHW Temperature            │ {temps['dhw']:>5.1f}°C {' '*32}║
╠════════════════════════════════════════════════════════╣
║ Heap Free                  │ {health['heap_free']:>5} bytes {' '*28}║
║ WiFi Signal                │ {health['wifi_rssi']:>3} dBm {' '*34}║
║ Uptime                     │ {health['uptime']:>5} seconds {' '*28}║
║ MQTT Connected             │ {str(health['mqtt_connected']):40} ║
╚════════════════════════════════════════════════════════╝
"""
        print(dashboard)
        
    except Exception as e:
        print(f'❌ Error: {e}')


def print_api_reference():
    """Print quick API reference"""
    print('\n' + '='*50)
    print('OTGW REST API v1 - Quick Reference')
    print('='*50 + '\n')
    
    endpoints = [
        ('GET', '/health', 'Check device status'),
        ('GET', '/flashstatus', 'Check ESP flash status'),
        ('GET', '/settings', 'Get device settings'),
        ('POST', '/settings', 'Update device settings'),
        ('GET', '/pic/flashstatus', 'Check PIC flash status'),
        ('GET', '/otgw/telegraf', 'Export Telegraf format'),
        ('GET', '/otgw/otmonitor', 'Export OTmonitor format'),
        ('GET', '/otgw/id/{msgid}', 'Get OT value by ID'),
        ('GET', '/otgw/label/{label}', 'Get OT value by label'),
        ('POST', '/otgw/command/{cmd}', 'Send OTGW command'),
        ('POST', '/otgw/autoconfigure', 'Trigger HA auto-discovery'),
    ]
    
    print('Endpoints:')
    for method, path, description in endpoints:
        print(f'  {method:6} /api/v1{path:40} - {description}')
    
    print('\nCommon Commands:')
    commands = [
        ('CS=1', 'Enable Central Heating'),
        ('CS=0', 'Disable Central Heating'),
        ('TT=22.0', 'Set room temperature'),
        ('TR=50.0', 'Set DHW setpoint'),
        ('SW=1', 'Enable DHW'),
        ('SW=0', 'Disable DHW'),
    ]
    
    for cmd, description in commands:
        print(f'  {cmd:15} - {description}')


# ============================================================================
# MAIN - Run examples
# ============================================================================

if __name__ == '__main__':
    print('\n' + '█'*50)
    print('█  OTGW REST API v1 - Python Examples')
    print('█'*50)
    
    # Note: Replace IP address with your device IP
    print('\nNote: Make sure to update device IP address in examples!')
    print('Current IP: 192.168.1.100\n')
    
    # Print reference
    print_api_reference()
    
    # Run examples (commented out by default)
    print('\n' + '='*50)
    print('To run specific examples, uncomment them below:')
    print('='*50 + '\n')
    
    print('Available examples:')
    print('  1. example_basic_status()')
    print('  2. example_temperatures()')
    print('  3. example_control_heating()')
    print('  4. example_settings()')
    print('  5. example_monitoring()')
    print('  6. example_data_formats()')
    print('  7. example_polling_pattern()')
    print('  8. example_dashboard()')
    
    print('\n' + '='*50)
    print('Quick Start:')
    print('='*50)
    print("""
from python_examples import OTGWClientV1

client = OTGWClientV1('192.168.1.100')

# Get health
health = client.get_health()
print(f"Status: {health['status']}")

# Get temperatures
temps = client.get_temperatures()
print(f"Room: {temps['room']}°C")

# Set room temperature
client.set_room_temperature(22.0)

# Enable heating
client.enable_heating()
""")
