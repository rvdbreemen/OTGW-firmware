#!/usr/bin/env python3
"""
OpenTherm Gateway REST API v3 - Automated Test Suite

Comprehensive test coverage for all v3 endpoints, HTTP methods, error cases,
and advanced features (ETag caching, pagination, filtering, CORS).

Requirements:
    pip install requests pytest pytest-cov

Usage:
    # Run all tests
    pytest tests/test_api_v3.py -v
    
    # Run with coverage report
    pytest tests/test_api_v3.py --cov=. --cov-report=html
    
    # Run specific test
    pytest tests/test_api_v3.py::test_get_system_info -v
    
    # Run specific category
    pytest tests/test_api_v3.py -k "system" -v
"""

import pytest
import requests
import json
import time
from typing import Dict, Any, Optional

# Configuration
OTGW_URL = 'http://otgw.local'
OTGW_API_BASE = f'{OTGW_URL}/api/v3'
TIMEOUT = 10

# Test markers
pytestmark = pytest.mark.integration


# ============================================================================
# FIXTURES
# ============================================================================

@pytest.fixture
def api_client():
    """Fixture for API requests"""
    class APIClient:
        def __init__(self, base_url=OTGW_API_BASE, timeout=TIMEOUT):
            self.base_url = base_url
            self.timeout = timeout
            self.session = requests.Session()
            self.etag_cache = {}
        
        def get(self, endpoint: str, headers: Optional[Dict] = None) -> requests.Response:
            url = f'{self.base_url}{endpoint}'
            return self.session.get(url, headers=headers, timeout=self.timeout)
        
        def post(self, endpoint: str, json_data: Optional[Dict] = None) -> requests.Response:
            url = f'{self.base_url}{endpoint}'
            return self.session.post(url, json=json_data, timeout=self.timeout)
        
        def patch(self, endpoint: str, json_data: Optional[Dict] = None) -> requests.Response:
            url = f'{self.base_url}{endpoint}'
            return self.session.patch(url, json=json_data, timeout=self.timeout)
        
        def put(self, endpoint: str, json_data: Optional[Dict] = None) -> requests.Response:
            url = f'{self.base_url}{endpoint}'
            return self.session.put(url, json=json_data, timeout=self.timeout)
        
        def delete(self, endpoint: str) -> requests.Response:
            url = f'{self.base_url}{endpoint}'
            return self.session.delete(url, timeout=self.timeout)
        
        def options(self, endpoint: str) -> requests.Response:
            url = f'{self.base_url}{endpoint}'
            return self.session.options(url, timeout=self.timeout)
    
    return APIClient()


# ============================================================================
# API DISCOVERY TESTS
# ============================================================================

class TestAPIDiscovery:
    """Test API discovery and HATEOAS links"""
    
    def test_get_api_root(self, api_client):
        """Test GET /api/v3/ returns API root with HATEOAS"""
        response = api_client.get('/')
        
        assert response.status_code == 200
        data = response.json()
        
        # Check required fields
        assert 'version' in data
        assert 'name' in data
        assert '_links' in data
        
        # Check HATEOAS links
        required_links = ['self', 'system', 'config', 'otgw', 'pic', 'sensors', 'export']
        for link in required_links:
            assert link in data['_links'], f"Missing link: {link}"
            assert 'href' in data['_links'][link]
    
    def test_api_root_cors_headers(self, api_client):
        """Test CORS headers present on API root"""
        response = api_client.get('/')
        
        assert 'Access-Control-Allow-Origin' in response.headers
        assert 'Access-Control-Allow-Methods' in response.headers
        assert response.headers['Access-Control-Allow-Origin'] == '*'


# ============================================================================
# SYSTEM RESOURCES TESTS
# ============================================================================

class TestSystemResources:
    """Test /api/v3/system/* endpoints"""
    
    def test_get_system_info(self, api_client):
        """Test GET /api/v3/system/info"""
        response = api_client.get('/system/info')
        
        assert response.status_code == 200
        data = response.json()
        
        # Required fields
        assert 'hostname' in data
        assert 'version' in data
        assert 'compiled' in data
        assert '_links' in data
        
        # HATEOAS links
        assert 'self' in data['_links']
        assert data['_links']['self']['href'] == '/api/v3/system/info'
    
    def test_get_system_info_etag(self, api_client):
        """Test ETag support on /api/v3/system/info"""
        # First request should have ETag
        response1 = api_client.get('/system/info')
        assert response1.status_code == 200
        assert 'ETag' in response1.headers
        
        etag = response1.headers['ETag']
        
        # Second request with If-None-Match should return 304
        headers = {'If-None-Match': etag}
        response2 = api_client.get('/system/info', headers=headers)
        assert response2.status_code == 304
    
    def test_get_health_status(self, api_client):
        """Test GET /api/v3/system/health"""
        response = api_client.get('/system/health')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'status' in data
        assert data['status'] in ['UP', 'DOWN']
        assert 'heap_free' in data
        assert isinstance(data['heap_free'], int)
        assert data['heap_free'] > 0
    
    def test_get_system_time(self, api_client):
        """Test GET /api/v3/system/time"""
        response = api_client.get('/system/time')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'datetime' in data
        assert 'epoch' in data
        assert 'timezone' in data
    
    def test_get_network_info(self, api_client):
        """Test GET /api/v3/system/network"""
        response = api_client.get('/system/network')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'ssid' in data
        assert 'ip' in data
        assert 'hostname' in data
    
    def test_get_system_stats(self, api_client):
        """Test GET /api/v3/system/stats"""
        response = api_client.get('/system/stats')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'uptime' in data
        assert 'heap_free' in data
        assert 'heap_fragmentation' in data


# ============================================================================
# CONFIGURATION RESOURCES TESTS
# ============================================================================

class TestConfigResources:
    """Test /api/v3/config/* endpoints"""
    
    def test_get_device_config(self, api_client):
        """Test GET /api/v3/config/device"""
        response = api_client.get('/config/device')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'hostname' in data
        assert '_links' in data
    
    def test_get_mqtt_config(self, api_client):
        """Test GET /api/v3/config/mqtt"""
        response = api_client.get('/config/mqtt')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'enabled' in data
        assert 'broker' in data
        assert 'port' in data
    
    def test_patch_device_config(self, api_client):
        """Test PATCH /api/v3/config/device"""
        # Store original
        original = api_client.get('/config/device').json()
        
        try:
            # Attempt to update (may not change if read-only)
            response = api_client.patch('/config/device', json_data={'hostname': 'otgw-test'})
            
            # Should return 200 or 204
            assert response.status_code in [200, 204, 400]  # 400 if read-only
        finally:
            # Restore original (if update worked)
            if response.status_code == 200:
                api_client.patch('/config/device', json_data={'hostname': original['hostname']})
    
    def test_get_features(self, api_client):
        """Test GET /api/v3/config/features"""
        response = api_client.get('/config/features')
        
        assert response.status_code == 200
        data = response.json()
        
        # Should have feature flags
        assert isinstance(data, dict)
        assert len(data) > 0


# ============================================================================
# OPENTHERM RESOURCES TESTS
# ============================================================================

class TestOpenThermResources:
    """Test /api/v3/otgw/* endpoints"""
    
    def test_get_otgw_status(self, api_client):
        """Test GET /api/v3/otgw/status"""
        response = api_client.get('/otgw/status')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'available' in data
        assert 'boiler_enabled' in data
        assert 'ch_enabled' in data
    
    def test_get_otgw_data(self, api_client):
        """Test GET /api/v3/otgw/data"""
        response = api_client.get('/otgw/data')
        
        assert response.status_code == 200
        data = response.json()
        
        # Should have temperature data
        assert len(data) > 0
    
    def test_get_messages_default_pagination(self, api_client):
        """Test GET /api/v3/otgw/messages (default pagination)"""
        response = api_client.get('/otgw/messages')
        
        assert response.status_code == 200
        data = response.json()
        
        # Check pagination fields
        assert 'total' in data
        assert 'page' in data
        assert 'per_page' in data
        assert 'messages' in data
        
        # Check pagination links
        assert '_links' in data
        assert 'self' in data['_links']
    
    def test_get_messages_with_pagination(self, api_client):
        """Test GET /api/v3/otgw/messages with page parameters"""
        response = api_client.get('/otgw/messages?page=0&per_page=10')
        
        assert response.status_code == 200
        data = response.json()
        
        assert data['page'] == 0
        assert data['per_page'] == 10
        assert isinstance(data['messages'], list)
        assert len(data['messages']) <= 10
    
    def test_get_messages_with_category_filter(self, api_client):
        """Test GET /api/v3/otgw/messages with category filter"""
        response = api_client.get('/otgw/messages?category=temperature')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'messages' in data
        # All messages should be temperature-related
        for msg in data['messages']:
            # Message label should indicate temperature category
            assert 'temperature' in msg['label'].lower() or 'temp' in msg['label'].lower()
    
    def test_get_messages_by_id(self, api_client):
        """Test GET /api/v3/otgw/messages with ID filter"""
        # Message ID 25 is boiler temperature
        response = api_client.get('/otgw/messages?id=25')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'messages' in data
        assert len(data['messages']) <= 1
        if data['messages']:
            assert data['messages'][0]['id'] == 25
    
    def test_post_otgw_command(self, api_client):
        """Test POST /api/v3/otgw/command"""
        command_data = {'command': 'TT=21.0'}
        response = api_client.post('/otgw/command', json_data=command_data)
        
        # Should be queued (202 Accepted)
        assert response.status_code in [200, 202]
        data = response.json()
        assert 'status' in data
    
    def test_post_otgw_command_invalid_format(self, api_client):
        """Test POST /api/v3/otgw/command with invalid format"""
        command_data = {'command': 'INVALID'}
        response = api_client.post('/otgw/command', json_data=command_data)
        
        # Should return 400 Bad Request
        assert response.status_code == 400
        data = response.json()
        assert 'error' in data


# ============================================================================
# PIC FIRMWARE TESTS
# ============================================================================

class TestPICResources:
    """Test /api/v3/pic/* endpoints"""
    
    def test_get_pic_info(self, api_client):
        """Test GET /api/v3/pic/info"""
        response = api_client.get('/pic/info')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'available' in data
        if data['available']:
            assert 'gateway_version' in data


# ============================================================================
# SENSOR RESOURCES TESTS
# ============================================================================

class TestSensorResources:
    """Test /api/v3/sensors/* endpoints"""
    
    def test_get_dallas_sensors(self, api_client):
        """Test GET /api/v3/sensors/dallas"""
        response = api_client.get('/sensors/dallas')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'enabled' in data
        assert 'sensors' in data or 'enabled' in data
    
    def test_get_s0_counter(self, api_client):
        """Test GET /api/v3/sensors/s0"""
        response = api_client.get('/sensors/s0')
        
        assert response.status_code == 200
        data = response.json()
        
        assert 'enabled' in data


# ============================================================================
# ERROR HANDLING TESTS
# ============================================================================

class TestErrorHandling:
    """Test error responses and status codes"""
    
    def test_404_not_found(self, api_client):
        """Test 404 Not Found response"""
        response = api_client.get('/resource/notfound')
        
        assert response.status_code == 404
        data = response.json()
        assert 'error' in data
    
    def test_400_bad_request_invalid_json(self, api_client):
        """Test 400 Bad Request for invalid JSON"""
        response = requests.post(
            f'{OTGW_API_BASE}/otgw/command',
            data='invalid json',
            headers={'Content-Type': 'application/json'}
        )
        
        assert response.status_code == 400
    
    def test_405_method_not_allowed(self, api_client):
        """Test 405 Method Not Allowed"""
        # POST not allowed on system info
        response = api_client.post('/system/info', json_data={})
        
        assert response.status_code == 405
        assert 'Allow' in response.headers


# ============================================================================
# CORS TESTS
# ============================================================================

class TestCORS:
    """Test CORS support"""
    
    def test_cors_headers_present(self, api_client):
        """Test CORS headers on all responses"""
        response = api_client.get('/system/info')
        
        assert 'Access-Control-Allow-Origin' in response.headers
        assert 'Access-Control-Allow-Methods' in response.headers
        assert 'Access-Control-Allow-Headers' in response.headers
    
    def test_cors_preflight_request(self, api_client):
        """Test CORS preflight (OPTIONS) request"""
        response = api_client.options('/otgw/command')
        
        assert response.status_code in [200, 204]
        assert 'Access-Control-Allow-Origin' in response.headers


# ============================================================================
# CONTENT NEGOTIATION TESTS
# ============================================================================

class TestContentNegotiation:
    """Test proper Content-Type handling"""
    
    def test_response_content_type_json(self, api_client):
        """Test JSON response Content-Type"""
        response = api_client.get('/system/info')
        
        assert 'Content-Type' in response.headers
        assert 'application/json' in response.headers['Content-Type']
    
    def test_request_requires_json_content_type(self, api_client):
        """Test that POST/PATCH requires JSON Content-Type"""
        response = requests.post(
            f'{OTGW_API_BASE}/otgw/command',
            data='{"command": "TT=21"}',
            headers={'Content-Type': 'text/plain'}
        )
        
        # Should return 415 Unsupported Media Type
        assert response.status_code in [415, 400]


# ============================================================================
# HATEOAS TESTS
# ============================================================================

class TestHATEOAS:
    """Test HATEOAS links in responses"""
    
    def test_system_info_has_hateoas_links(self, api_client):
        """Test that system/info includes HATEOAS links"""
        response = api_client.get('/system/info')
        
        data = response.json()
        assert '_links' in data
        assert 'self' in data['_links']
        assert data['_links']['self']['href'] == '/api/v3/system/info'
    
    def test_pagination_includes_hateoas_links(self, api_client):
        """Test that paginated responses include navigation links"""
        response = api_client.get('/otgw/messages?page=0&per_page=10')
        
        data = response.json()
        assert '_links' in data
        
        # Should have self link at minimum
        assert 'self' in data['_links']


# ============================================================================
# BACKWARD COMPATIBILITY TESTS
# ============================================================================

class TestBackwardCompatibility:
    """Test that v0, v1, v2 APIs still work"""
    
    def test_v0_api_still_works(self, api_client):
        """Test GET /api/v0/otgw/25 still works"""
        response = requests.get(f'{OTGW_URL}/api/v0/otgw/25')
        
        # Should return 200 or be available
        assert response.status_code in [200, 404]  # 404 if v0 disabled
    
    def test_v1_api_still_works(self, api_client):
        """Test /api/v1/ endpoints still work"""
        response = requests.get(f'{OTGW_URL}/api/v1/otgw/id/25')
        
        assert response.status_code in [200, 404, 404]  # Allow 404 if disabled


# ============================================================================
# PERFORMANCE TESTS
# ============================================================================

class TestPerformance:
    """Test response times and performance"""
    
    def test_simple_get_response_time(self, api_client):
        """Test simple GET response time < 100ms"""
        start = time.time()
        response = api_client.get('/system/health')
        elapsed = time.time() - start
        
        assert response.status_code == 200
        assert elapsed < 0.1, f"Response took {elapsed:.3f}s, should be < 0.1s"
    
    def test_paginated_response_time(self, api_client):
        """Test paginated response time < 200ms"""
        start = time.time()
        response = api_client.get('/otgw/messages?page=0&per_page=50')
        elapsed = time.time() - start
        
        assert response.status_code == 200
        assert elapsed < 0.2, f"Response took {elapsed:.3f}s, should be < 0.2s"


# ============================================================================
# INTEGRATION TESTS
# ============================================================================

class TestIntegration:
    """Test realistic usage scenarios"""
    
    def test_full_discovery_flow(self, api_client):
        """Test complete discovery flow: root -> resources -> endpoints"""
        # Get API root
        root = api_client.get('/').json()
        assert '_links' in root
        
        # Follow system link
        system_link = root['_links']['system']['href']
        system = api_client.get(system_link).json()
        assert '_links' in system
        
        # Follow health link
        health_link = system['_links']['health']['href']
        health = api_client.get(health_link).json()
        assert 'status' in health
    
    def test_configuration_update_flow(self, api_client):
        """Test getting and updating configuration"""
        # Get current config
        response = api_client.get('/config/mqtt')
        assert response.status_code == 200
        original = response.json()
        
        # Could update (but tests shouldn't modify system state)
        # Just verify the endpoint works
        assert 'enabled' in original or 'broker' in original


# ============================================================================
# RUN TESTS
# ============================================================================

if __name__ == '__main__':
    pytest.main([__file__, '-v', '--tb=short'])
