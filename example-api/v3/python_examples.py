#!/usr/bin/env python3
"""
OpenTherm Gateway REST API v3 - Python Examples

This module demonstrates how to interact with the OTGW REST API using Python.

Requirements:
    pip install requests

Usage:
    # Import and use
    from python_examples import OTGWClient
    
    client = OTGWClient('otgw.local')
    info = client.get_system_info()
    print(info)
    
    # Or run examples directly
    python3 python_examples.py
"""

import requests
import json
import time
from typing import Dict, List, Optional, Any
from datetime import datetime
import sys

# Configuration
OTGW_HOST = 'otgw.local'
OTGW_PORT = 80
OTGW_TIMEOUT = 10

# Color codes for terminal output
class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    END = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


class OTGWClient:
    """OpenTherm Gateway REST API v3 Client"""
    
    def __init__(self, host: str = OTGW_HOST, port: int = OTGW_PORT, timeout: int = OTGW_TIMEOUT):
        """Initialize OTGW client"""
        self.host = host
        self.port = port
        self.timeout = timeout
        self.base_url = f'http://{host}:{port}/api/v3'
        self.session = requests.Session()
        self.etag_cache = {}
    
    def _request(self, method: str, endpoint: str, json_data: Optional[Dict] = None, 
                 headers: Optional[Dict] = None) -> Dict[str, Any]:
        """Make API request with error handling"""
        url = f'{self.base_url}{endpoint}'
        request_headers = {'Content-Type': 'application/json'}
        
        if headers:
            request_headers.update(headers)
        
        try:
            if method == 'GET':
                response = self.session.get(url, headers=request_headers, timeout=self.timeout)
            elif method == 'POST':
                response = self.session.post(url, json=json_data, headers=request_headers, timeout=self.timeout)
            elif method == 'PATCH':
                response = self.session.patch(url, json=json_data, headers=request_headers, timeout=self.timeout)
            elif method == 'PUT':
                response = self.session.put(url, json=json_data, headers=request_headers, timeout=self.timeout)
            elif method == 'DELETE':
                response = self.session.delete(url, headers=request_headers, timeout=self.timeout)
            else:
                raise ValueError(f'Unsupported HTTP method: {method}')
            
            # Check for HTTP errors
            response.raise_for_status()
            
            # Try to parse JSON response
            try:
                data = response.json()
            except json.JSONDecodeError:
                data = response.text
            
            return {
                'status': response.status_code,
                'headers': dict(response.headers),
                'data': data
            }
        
        except requests.exceptions.Timeout:
            print(f'{Colors.RED}ERROR: Request timeout{Colors.END}')
            raise
        except requests.exceptions.ConnectionError:
            print(f'{Colors.RED}ERROR: Connection failed to {url}{Colors.END}')
            raise
        except requests.exceptions.HTTPError as e:
            try:
                error_data = e.response.json()
                print(f'{Colors.RED}API Error ({e.response.status_code}): {error_data.get("error")}{Colors.END}')
                print(f'{Colors.RED}Message: {error_data.get("message")}{Colors.END}')
            except:
                print(f'{Colors.RED}HTTP Error {e.response.status_code}: {e}{Colors.END}')
            raise
        except Exception as e:
            print(f'{Colors.RED}ERROR: {e}{Colors.END}')
            raise
    
    # ========================================================================
    # API DISCOVERY
    # ========================================================================
    
    def get_api_root(self) -> Dict:
        """Get API root with HATEOAS links"""
        result = self._request('GET', '/')
        return result['data']
    
    def discover_api(self) -> Dict:
        """Discover all available resources"""
        root = self.get_api_root()
        print(f'{Colors.BOLD}Available API Resources:{Colors.END}')
        for key, link in root.get('_links', {}).items():
            title = link.get('title', '')
            print(f'  {Colors.GREEN}{key}{Colors.END}: {link["href"]} - {title}')
        return root
    
    # ========================================================================
    # SYSTEM INFORMATION
    # ========================================================================
    
    def get_system_info(self) -> Dict:
        """Get device information"""
        result = self._request('GET', '/system/info')
        return result['data']
    
    def get_health_status(self) -> Dict:
        """Get system health status"""
        result = self._request('GET', '/system/health')
        return result['data']
    
    def get_system_time(self) -> Dict:
        """Get system time and timezone"""
        result = self._request('GET', '/system/time')
        return result['data']
    
    def get_network_info(self) -> Dict:
        """Get network configuration"""
        result = self._request('GET', '/system/network')
        return result['data']
    
    def get_system_stats(self) -> Dict:
        """Get system statistics"""
        result = self._request('GET', '/system/stats')
        return result['data']
    
    # ========================================================================
    # CONFIGURATION MANAGEMENT
    # ========================================================================
    
    def get_device_config(self) -> Dict:
        """Get device configuration"""
        result = self._request('GET', '/config/device')
        return result['data']
    
    def update_device_config(self, **kwargs) -> Dict:
        """Update device configuration"""
        result = self._request('PATCH', '/config/device', json_data=kwargs)
        return result['data']
    
    def update_hostname(self, hostname: str) -> Dict:
        """Update device hostname"""
        return self.update_device_config(hostname=hostname)
    
    def update_timezone(self, timezone: str) -> Dict:
        """Update timezone"""
        return self.update_device_config(ntp_timezone=timezone)
    
    def get_mqtt_config(self) -> Dict:
        """Get MQTT configuration"""
        result = self._request('GET', '/config/mqtt')
        return result['data']
    
    def update_mqtt_config(self, **kwargs) -> Dict:
        """Update MQTT configuration"""
        result = self._request('PATCH', '/config/mqtt', json_data=kwargs)
        return result['data']
    
    def get_features(self) -> Dict:
        """Get available features"""
        result = self._request('GET', '/config/features')
        return result['data']
    
    # ========================================================================
    # OPENTHERM DATA
    # ========================================================================
    
    def get_gateway_status(self) -> Dict:
        """Get gateway status"""
        result = self._request('GET', '/otgw/status')
        return result['data']
    
    def get_otgw_data(self) -> Dict:
        """Get current OpenTherm data"""
        result = self._request('GET', '/otgw/data')
        return result['data']
    
    def get_messages(self, page: int = 0, per_page: int = 20) -> Dict:
        """Get paginated OpenTherm messages"""
        endpoint = f'/otgw/messages?page={page}&per_page={per_page}'
        result = self._request('GET', endpoint)
        return result['data']
    
    def get_messages_by_category(self, category: str) -> List[Dict]:
        """Get messages by category (temperature, pressure, flow, status, setpoint)"""
        endpoint = f'/otgw/messages?category={category}'
        result = self._request('GET', endpoint)
        return result['data'].get('messages', [])
    
    def get_message(self, message_id: int) -> Optional[Dict]:
        """Get single message by ID"""
        endpoint = f'/otgw/messages?id={message_id}'
        result = self._request('GET', endpoint)
        messages = result['data'].get('messages', [])
        return messages[0] if messages else None
    
    def send_command(self, command: str) -> Dict:
        """Send command to OpenTherm Gateway (format: XX=YYYY)"""
        result = self._request('POST', '/otgw/command', json_data={'command': command})
        return result['data']
    
    def set_temperature(self, temperature: float) -> Dict:
        """Set temporary temperature (TT command)"""
        return self.send_command(f'TT={temperature}')
    
    def set_return_temperature(self, temperature: float) -> Dict:
        """Override return temperature (TR command)"""
        return self.send_command(f'TR={temperature}')
    
    def get_otmonitor_data(self) -> Dict:
        """Get OTmonitor format data"""
        result = self._request('GET', '/otgw/monitor')
        return result['data']
    
    # ========================================================================
    # CACHING WITH ETAGS
    # ========================================================================
    
    def get_cached_system_info(self) -> Dict:
        """Get system info with ETag caching"""
        endpoint = '/system/info'
        headers = {}
        
        # Add ETag if we have one cached
        if endpoint in self.etag_cache:
            headers['If-None-Match'] = self.etag_cache[endpoint]
        
        response = self.session.get(f'{self.base_url}{endpoint}', headers=headers, timeout=self.timeout)
        
        # 304 Not Modified - return cached data
        if response.status_code == 304:
            print(f'{Colors.CYAN}[Cached]{Colors.END} System info unchanged')
            return self.get_system_info()
        
        response.raise_for_status()
        data = response.json()
        
        # Update cache with ETag
        if 'ETag' in response.headers:
            self.etag_cache[endpoint] = response.headers['ETag']
        
        return data
    
    def clear_etag_cache(self):
        """Clear ETag cache"""
        self.etag_cache.clear()
    
    # ========================================================================
    # SENSORS
    # ========================================================================
    
    def get_dallas_sensors(self) -> List[Dict]:
        """Get Dallas temperature sensors"""
        result = self._request('GET', '/sensors/dallas')
        return result['data'].get('sensors', [])
    
    def get_s0_counter(self) -> Dict:
        """Get S0 pulse counter"""
        result = self._request('GET', '/sensors/s0')
        return result['data']
    
    def reset_s0_counter(self) -> Dict:
        """Reset S0 pulse counter"""
        result = self._request('PUT', '/sensors/s0', json_data={})
        return result['data']
    
    # ========================================================================
    # PIC FIRMWARE
    # ========================================================================
    
    def get_pic_info(self) -> Dict:
        """Get PIC firmware information"""
        result = self._request('GET', '/pic/info')
        return result['data']
    
    def get_pic_flash_status(self) -> Dict:
        """Get PIC flash status"""
        result = self._request('GET', '/pic/flash')
        return result['data']
    
    # ========================================================================
    # DATA EXPORT
    # ========================================================================
    
    def export_settings(self) -> Dict:
        """Export settings as JSON"""
        result = self._request('GET', '/export/settings')
        return result['data']
    
    def export_telegraf(self) -> str:
        """Get Telegraf metrics format"""
        response = self.session.get(f'{self.base_url}/export/telegraf', timeout=self.timeout)
        response.raise_for_status()
        return response.text
    
    def export_otmonitor(self) -> Dict:
        """Get OTmonitor export format"""
        result = self._request('GET', '/export/otmonitor')
        return result['data']
    
    def get_logs(self) -> str:
        """Get recent debug logs"""
        response = self.session.get(f'{self.base_url}/export/logs', timeout=self.timeout)
        response.raise_for_status()
        return response.text
    
    # ========================================================================
    # MONITORING
    # ========================================================================
    
    def monitor_health(self, interval: int = 30, duration: Optional[int] = None):
        """Monitor device health"""
        print(f'{Colors.BOLD}Starting health monitor (interval: {interval}s){Colors.END}')
        start_time = time.time()
        
        try:
            while True:
                health = self.get_health_status()
                timestamp = datetime.now().strftime('%H:%M:%S')
                status_color = Colors.GREEN if health['status'] == 'UP' else Colors.RED
                print(f'[{timestamp}] Status: {status_color}{health["status"]}{Colors.END}, '
                      f'Heap: {health["heap_free"]} bytes')
                
                if duration and (time.time() - start_time) > duration:
                    break
                
                time.sleep(interval)
        
        except KeyboardInterrupt:
            print(f'\n{Colors.YELLOW}Monitor stopped{Colors.END}')
    
    def monitor_temperature(self, interval: int = 60, duration: Optional[int] = None):
        """Monitor temperature changes"""
        print(f'{Colors.BOLD}Starting temperature monitor (interval: {interval}s){Colors.END}')
        start_time = time.time()
        last_temp = None
        
        try:
            while True:
                data = self.get_otgw_data()
                current_temp = float(data.get('boiler_temp', 0))
                timestamp = datetime.now().strftime('%H:%M:%S')
                
                if last_temp is not None and abs(current_temp - last_temp) > 1.0:
                    print(f'[{timestamp}] Temperature changed: {Colors.YELLOW}{last_temp}°C{Colors.END} '
                          f'→ {Colors.GREEN}{current_temp}°C{Colors.END}')
                else:
                    print(f'[{timestamp}] Boiler temp: {current_temp}°C')
                
                last_temp = current_temp
                
                if duration and (time.time() - start_time) > duration:
                    break
                
                time.sleep(interval)
        
        except KeyboardInterrupt:
            print(f'\n{Colors.YELLOW}Monitor stopped{Colors.END}')


# ============================================================================
# EXAMPLE FUNCTIONS
# ============================================================================

def print_header(text: str):
    """Print formatted section header"""
    print(f'\n{Colors.BOLD}{Colors.BLUE}=== {text} ==={Colors.END}\n')


def example_discovery():
    """Example: API Discovery"""
    print_header('API Discovery')
    client = OTGWClient()
    client.discover_api()


def example_system_info():
    """Example: Get system information"""
    print_header('System Information')
    client = OTGWClient()
    
    info = client.get_system_info()
    print(f'{Colors.GREEN}Hostname:{Colors.END} {info["hostname"]}')
    print(f'{Colors.GREEN}Version:{Colors.END} {info["version"]}')
    print(f'{Colors.GREEN}Compiled:{Colors.END} {info["compiled"]}')
    
    health = client.get_health_status()
    print(f'{Colors.GREEN}Status:{Colors.END} {health["status"]}')
    print(f'{Colors.GREEN}Uptime:{Colors.END} {health["uptime"]} seconds')
    print(f'{Colors.GREEN}Free Heap:{Colors.END} {health["heap_free"]} bytes')


def example_configuration():
    """Example: Configuration management"""
    print_header('Configuration Management')
    client = OTGWClient()
    
    # Get current config
    config = client.get_device_config()
    print(f'{Colors.GREEN}Current Config:{Colors.END}')
    print(json.dumps(config, indent=2))
    
    # Update hostname (uncomment to run)
    # client.update_hostname('my-otgw')
    # print(f'{Colors.GREEN}Hostname updated{Colors.END}')


def example_opentherm():
    """Example: OpenTherm data"""
    print_header('OpenTherm Data')
    client = OTGWClient()
    
    # Gateway status
    status = client.get_gateway_status()
    print(f'{Colors.GREEN}Gateway Status:{Colors.END}')
    print(f'  Boiler: {status["boiler_enabled"]}')
    print(f'  Central Heating: {status["ch_enabled"]}')
    print(f'  Flame: {status["flame"]}')
    
    # Current data
    data = client.get_otgw_data()
    print(f'\n{Colors.GREEN}Current Data:{Colors.END}')
    print(f'  Room Setpoint: {data.get("room_setpoint", "N/A")}°C')
    print(f'  Room Temp: {data.get("room_temp", "N/A")}°C')
    print(f'  Boiler Temp: {data.get("boiler_temp", "N/A")}°C')
    
    # Temperature messages
    temps = client.get_messages_by_category('temperature')
    print(f'\n{Colors.GREEN}Temperature Messages:{Colors.END}')
    for msg in temps[:5]:  # Show first 5
        print(f'  {msg["label"]}: {msg["value"]} {msg["unit"]}')


def example_pagination():
    """Example: Pagination"""
    print_header('Pagination Example')
    client = OTGWClient()
    
    # Get first page
    page1 = client.get_messages(page=0, per_page=10)
    print(f'{Colors.GREEN}Total Messages:{Colors.END} {page1["total"]}')
    print(f'{Colors.GREEN}Page 1 Items:{Colors.END}')
    for msg in page1['messages'][:5]:
        print(f'  {msg["label"]}: {msg["value"]}')
    
    # Check pagination links
    if '_links' in page1:
        print(f'\n{Colors.GREEN}Available Links:{Colors.END}')
        for link_name in page1['_links'].keys():
            print(f'  - {link_name}')


def example_etag_caching():
    """Example: ETag caching"""
    print_header('ETag Caching')
    client = OTGWClient()
    
    print(f'{Colors.CYAN}First request (cached){Colors.END}')
    info1 = client.get_cached_system_info()
    print(f'  Hostname: {info1["hostname"]}')
    
    print(f'\n{Colors.CYAN}Second request (should be cached){Colors.END}')
    info2 = client.get_cached_system_info()
    print(f'  Hostname: {info2["hostname"]}')


def example_sensors():
    """Example: Sensor data"""
    print_header('Sensor Data')
    client = OTGWClient()
    
    # Dallas sensors
    sensors = client.get_dallas_sensors()
    if sensors:
        print(f'{Colors.GREEN}Dallas Sensors:{Colors.END}')
        for sensor in sensors:
            print(f'  {sensor["name"]}: {sensor["temperature"]}°C')
    else:
        print(f'{Colors.YELLOW}No Dallas sensors found{Colors.END}')
    
    # S0 counter
    s0 = client.get_s0_counter()
    print(f'\n{Colors.GREEN}S0 Counter:{Colors.END}')
    print(f'  Pulses: {s0["pulses"]}')
    print(f'  Energy: {s0.get("energy_kwh", "N/A")} kWh')


def example_export():
    """Example: Data export"""
    print_header('Data Export')
    client = OTGWClient()
    
    # Export settings
    settings = client.export_settings()
    print(f'{Colors.GREEN}Settings (JSON):{Colors.END}')
    print(json.dumps(settings, indent=2)[:200] + '...')
    
    # Export Telegraf
    print(f'\n{Colors.GREEN}Telegraf Metrics:{Colors.END}')
    telegraf = client.export_telegraf()
    print(telegraf[:100] + '...')


def run_all_examples():
    """Run all examples"""
    print(f'{Colors.BOLD}{Colors.HEADER}OpenTherm Gateway REST API v3 - Python Examples{Colors.END}')
    
    try:
        example_discovery()
        example_system_info()
        example_opentherm()
        example_pagination()
        example_sensors()
        example_export()
        
        print(f'\n{Colors.BOLD}{Colors.GREEN}All examples completed successfully!{Colors.END}\n')
    
    except Exception as e:
        print(f'\n{Colors.RED}Error running examples: {e}{Colors.END}')
        sys.exit(1)


if __name__ == '__main__':
    run_all_examples()
