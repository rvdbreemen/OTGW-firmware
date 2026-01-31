---
# API v3 Documentation
Title: REST API v3 Complete Reference
Version: 3.0.0
Last Updated: 2026-01-31
---

# OpenTherm Gateway REST API v3 Reference

## Overview

The REST API v3 provides a RESTful interface to the OpenTherm Gateway firmware. It implements Richardson Maturity Model Level 3 with full HATEOAS support, enabling API discoverability and self-documentation.

**Base URL:** `http://<device-ip>/api/v3`

**API Version:** 3.0.0

**Status:** All resources documented, endpoints fully implemented

## Key Features

- **Richardson Level 3 HATEOAS** - Hypermedia links for API navigation
- **Proper HTTP Methods** - GET, POST, PUT, PATCH, DELETE, OPTIONS
- **ETag Caching** - If-None-Match support for 304 Not Modified responses
- **Pagination** - Large collections with page/per_page parameters
- **Query Filtering** - Filter by category, timestamp, and flags
- **Error Handling** - Structured JSON error responses with timestamps
- **CORS Support** - Cross-origin requests from browsers
- **Rate Limiting** (Optional) - Per-IP request throttling

## Table of Contents

1. [Authentication](#authentication)
2. [HTTP Status Codes](#http-status-codes)
3. [Error Responses](#error-responses)
4. [Headers](#headers)
5. [API Root Discovery](#api-root-discovery)
6. [System Resources](#system-resources)
7. [Configuration Resources](#configuration-resources)
8. [OpenTherm Gateway Resources](#opentherm-gateway-resources)
9. [PIC Firmware Resources](#pic-firmware-resources)
10. [Sensor Resources](#sensor-resources)
11. [Export Resources](#export-resources)
12. [Caching & ETags](#caching--etags)
13. [Pagination](#pagination)
14. [Filtering](#filtering)
15. [CORS](#cors)
16. [Migration Guide](#migration-guide-from-v1v2)

## Authentication

**No authentication is required.** The API is designed for local network use only and should not be exposed to the internet without additional security measures.

For remote access, use a VPN or reverse proxy with authentication enabled.

## HTTP Status Codes

| Code | Name | Usage |
|------|------|-------|
| 200 | OK | Successful GET, POST, PUT, PATCH with data |
| 202 | Accepted | Asynchronous request accepted (e.g., queued command) |
| 204 | No Content | Successful PUT/PATCH with no response body |
| 304 | Not Modified | ETag matches, client has current version |
| 400 | Bad Request | Invalid parameters, malformed JSON |
| 404 | Not Found | Resource does not exist |
| 405 | Method Not Allowed | HTTP method not supported for resource |
| 413 | Payload Too Large | Request body exceeds size limit |
| 415 | Unsupported Media Type | Content-Type is not application/json |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Device error (low heap, etc.) |
| 501 | Not Implemented | Feature not yet implemented |

## Error Responses

All error responses follow this JSON structure:

```json
{
  "error": "ERROR_CODE",
  "message": "Human-readable error message",
  "status": 400,
  "timestamp": 1706800000,
  "path": "/api/v3/resource/path",
  "details": "Optional additional information"
}
```

### Common Error Codes

- `INVALID_REQUEST` - Request format is invalid
- `MISSING_FIELD` - Required field missing from request body
- `INVALID_JSON` - Request body is not valid JSON
- `INVALID_COMMAND_FORMAT` - Command does not match XX=YYYY format
- `INVALID_MESSAGE_ID` - Message ID out of range (0-127)
- `RESOURCE_NOT_FOUND` - Requested resource does not exist
- `METHOD_NOT_ALLOWED` - HTTP method not supported
- `MISSING_CONTENT_TYPE` - Content-Type header missing
- `UNSUPPORTED_MEDIA_TYPE` - Content-Type must be application/json
- `COMMAND_TOO_LONG` - Command exceeds maximum length
- `NOT_CONFIGURED` - Feature not configured (GPIO not set)
- `RATE_LIMIT_EXCEEDED` - Too many requests from IP address
- `NOT_IMPLEMENTED` - Feature not yet implemented

## Headers

### Request Headers

| Header | Value | Required | Purpose |
|--------|-------|----------|---------|
| Content-Type | application/json | For POST/PUT/PATCH | Indicates JSON request body |
| If-None-Match | "etag-value" | Optional | Conditional GET (304 response) |
| If-Match | "etag-value" | Optional | Conditional update |

### Response Headers

| Header | Example | Purpose |
|--------|---------|---------|
| Content-Type | application/json | Response format |
| Content-Length | 1234 | Response body size |
| ETag | "a1b2c3d4-5678901" | Resource version identifier |
| Cache-Control | public, max-age=3600 | Caching instructions |
| X-OTGW-API-Version | 3.0 | REST API version |
| X-OTGW-Version | 1.0.0 | Firmware version |
| X-OTGW-Heap-Free | 25000 | Free heap memory (bytes) |
| Access-Control-Allow-Origin | * | CORS support |
| Access-Control-Allow-Methods | GET, POST, PUT, PATCH, DELETE, OPTIONS | Allowed HTTP methods |
| Access-Control-Allow-Headers | Content-Type, If-None-Match, If-Match | Allowed request headers |
| Allow | GET, PUT, PATCH | Allowed methods (405 response) |
| Retry-After | 60 | Seconds to wait before retry (429 response) |
| X-RateLimit-Limit | 60 | Maximum requests per minute |
| X-RateLimit-Remaining | 45 | Remaining requests in window |

## API Root Discovery

### GET /api/v3

Get API overview with hypermedia links to all resources.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3
```

**Response (200 OK):**
```json
{
  "version": "3.0",
  "name": "OTGW REST API",
  "description": "OpenTherm Gateway REST API v3 with HATEOAS",
  "_links": {
    "self": {"href": "/api/v3/"},
    "system": {
      "href": "/api/v3/system",
      "title": "System information and health"
    },
    "config": {
      "href": "/api/v3/config",
      "title": "Device configuration"
    },
    "otgw": {
      "href": "/api/v3/otgw",
      "title": "OpenTherm Gateway data and commands"
    },
    "pic": {
      "href": "/api/v3/pic",
      "title": "PIC firmware and status"
    },
    "sensors": {
      "href": "/api/v3/sensors",
      "title": "Sensor data (Dallas, S0)"
    },
    "export": {
      "href": "/api/v3/export",
      "title": "Data export formats"
    }
  }
}
```

## System Resources

### GET /api/v3/system

Get system resources index with links.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/system
```

**Response (200 OK):**
```json
{
  "resource": "system",
  "description": "System information and diagnostics",
  "_links": {
    "self": {"href": "/api/v3/system"},
    "info": {"href": "/api/v3/system/info", "title": "Device information"},
    "health": {"href": "/api/v3/system/health", "title": "System health check"},
    "time": {"href": "/api/v3/system/time", "title": "System time and timezone"},
    "network": {"href": "/api/v3/system/network", "title": "Network configuration"},
    "stats": {"href": "/api/v3/system/stats", "title": "System statistics"}
  }
}
```

### GET /api/v3/system/info

Get device information (cacheable with ETag).

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/system/info
```

**Response (200 OK):**
```json
{
  "hostname": "otgw",
  "version": "1.0.0",
  "compiled": "Jan 31 2026 20:00:00",
  "chipid": "1a2b3c",
  "corever": "2.7.4_25-g13b93b8",
  "sdkver": "v2.2.2",
  "cpufreq": 80,
  "flashsize": 4194304,
  "flashspeed": 40000000,
  "_links": {
    "self": {"href": "/api/v3/system/info"},
    "system": {"href": "/api/v3/system"},
    "health": {"href": "/api/v3/system/health"}
  }
}
```

**Headers:**
```
ETag: "a1b2c3d4-5678901"
Cache-Control: public, max-age=3600
```

### GET /api/v3/system/health

Get system health check (dynamic, not cached).

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/system/health
```

**Response (200 OK):**
```json
{
  "status": "UP",
  "uptime": 86400,
  "heap_free": 25000,
  "heap_fragmentation": 30,
  "picavailable": true,
  "_links": {
    "self": {"href": "/api/v3/system/health"},
    "parent": {"href": "/api/v3/system"}
  }
}
```

### GET /api/v3/system/time

Get system time and timezone.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/system/time
```

**Response (200 OK):**
```json
{
  "datetime": "2026-01-31T20:00:00",
  "epoch": 1706730000,
  "timezone": "Europe/Amsterdam",
  "_links": {
    "self": {"href": "/api/v3/system/time"},
    "parent": {"href": "/api/v3/system"}
  }
}
```

### GET /api/v3/system/network

Get network configuration.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/system/network
```

**Response (200 OK):**
```json
{
  "ssid": "MyNetwork",
  "rssi": -65,
  "ip": "192.168.1.100",
  "subnet": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns": "192.168.1.1",
  "mac": "AA:BB:CC:DD:EE:FF",
  "hostname": "otgw",
  "_links": {
    "self": {"href": "/api/v3/system/network"},
    "parent": {"href": "/api/v3/system"}
  }
}
```

### GET /api/v3/system/stats

Get system statistics and resource usage.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/system/stats
```

**Response (200 OK):**
```json
{
  "uptime": 86400,
  "heap_free": 25000,
  "heap_fragmentation": 30,
  "sketch_size": 512000,
  "free_sketch_space": 2097152,
  "flash_chip_size": 4194304,
  "cpu_freq_mhz": 80,
  "_links": {
    "self": {"href": "/api/v3/system/stats"},
    "parent": {"href": "/api/v3/system"}
  }
}
```

## Configuration Resources

### GET /api/v3/config

Get configuration resources index with links.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/config
```

**Response (200 OK):**
```json
{
  "resource": "config",
  "description": "Device configuration management",
  "_links": {
    "self": {"href": "/api/v3/config"},
    "device": {"href": "/api/v3/config/device", "title": "Device settings"},
    "network": {"href": "/api/v3/config/network", "title": "Network settings"},
    "mqtt": {"href": "/api/v3/config/mqtt", "title": "MQTT configuration"},
    "otgw": {"href": "/api/v3/config/otgw", "title": "OTGW settings"},
    "features": {"href": "/api/v3/config/features", "title": "Available features"}
  }
}
```

### GET /api/v3/config/device

Get device configuration.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/config/device
```

**Response (200 OK):**
```json
{
  "hostname": "otgw",
  "ntp_timezone": "Europe/Amsterdam",
  "led_mode": 1,
  "_links": {
    "self": {"href": "/api/v3/config/device"},
    "parent": {"href": "/api/v3/config"}
  }
}
```

### PATCH /api/v3/config/device

Update device configuration.

**Request:**
```bash
curl -X PATCH http://otgw.local/api/v3/config/device \
  -H "Content-Type: application/json" \
  -d '{
    "hostname": "my-otgw",
    "ntp_timezone": "Europe/Amsterdam"
  }'
```

**Response (200 OK):**
```json
{
  "status": "updated",
  "hostname": "my-otgw",
  "ntp_timezone": "Europe/Amsterdam"
}
```

### GET /api/v3/config/network

Get network configuration (IP, DHCP, DNS).

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/config/network
```

**Response (200 OK):**
```json
{
  "ssid": "MyNetwork",
  "dhcp_enabled": true,
  "ip": "192.168.1.100",
  "netmask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "192.168.1.1",
  "dns2": "8.8.8.8",
  "_links": {
    "self": {"href": "/api/v3/config/network"},
    "parent": {"href": "/api/v3/config"}
  }
}
```

### GET /api/v3/config/mqtt

Get MQTT configuration.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/config/mqtt
```

**Response (200 OK):**
```json
{
  "enabled": true,
  "broker": "192.168.1.50",
  "port": 1883,
  "topic_prefix": "otgw",
  "unique_id": "otgw-001",
  "ha_auto_discovery": true,
  "_links": {
    "self": {"href": "/api/v3/config/mqtt"},
    "parent": {"href": "/api/v3/config"}
  }
}
```

### PATCH /api/v3/config/mqtt

Update MQTT configuration.

**Request:**
```bash
curl -X PATCH http://otgw.local/api/v3/config/mqtt \
  -H "Content-Type: application/json" \
  -d '{
    "broker": "192.168.1.100",
    "port": 1883,
    "enabled": true
  }'
```

**Response (200 OK):**
```json
{
  "status": "updated"
}
```

### GET /api/v3/config/features

List available features and their status.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/config/features
```

**Response (200 OK):**
```json
{
  "mqtt": true,
  "webserver": true,
  "websocket": true,
  "rest_api": true,
  "ha_discovery": true,
  "dallas_sensors": true,
  "s0_counter": true,
  "debug_telnet": true,
  "_links": {
    "self": {"href": "/api/v3/config/features"},
    "parent": {"href": "/api/v3/config"}
  }
}
```

## OpenTherm Gateway Resources

### GET /api/v3/otgw

Get OpenTherm Gateway resources index.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/otgw
```

**Response (200 OK):**
```json
{
  "resource": "otgw",
  "description": "OpenTherm Gateway data and control",
  "available": true,
  "_links": {
    "self": {"href": "/api/v3/otgw"},
    "status": {"href": "/api/v3/otgw/status", "title": "Gateway status"},
    "messages": {"href": "/api/v3/otgw/messages", "title": "All OpenTherm messages"},
    "data": {"href": "/api/v3/otgw/data", "title": "Current data values"},
    "command": {"href": "/api/v3/otgw/command", "title": "Send OTGW command"},
    "monitor": {"href": "/api/v3/otgw/monitor", "title": "Monitor format data"}
  }
}
```

### GET /api/v3/otgw/status

Get gateway status (boiler state, flags, heating status).

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/otgw/status
```

**Response (200 OK):**
```json
{
  "available": true,
  "sync": "0",
  "boiler_enabled": true,
  "ch_enabled": true,
  "dhw_enabled": false,
  "flame": true,
  "cooling_enabled": false,
  "ch2_enabled": false,
  "diag_fault": false,
  "_links": {
    "self": {"href": "/api/v3/otgw/status"},
    "parent": {"href": "/api/v3/otgw"}
  }
}
```

### GET /api/v3/otgw/messages

Get paginated list of all OpenTherm messages. Supports filtering and pagination.

**Request (all messages, page 1):**
```bash
curl -X GET "http://otgw.local/api/v3/otgw/messages?page=0&per_page=20"
```

**Request (single message by ID):**
```bash
curl -X GET "http://otgw.local/api/v3/otgw/messages?id=25"
```

**Response (200 OK):**
```json
{
  "total": 128,
  "page": 0,
  "per_page": 20,
  "total_pages": 7,
  "messages": [
    {
      "id": 0,
      "label": "Status",
      "unit": "",
      "value": "0"
    },
    {
      "id": 16,
      "label": "Room Setpoint",
      "unit": "°C",
      "value": "21.5"
    },
    {
      "id": 25,
      "label": "Boiler Water Temperature",
      "unit": "°C",
      "value": "65.2"
    }
  ],
  "pagination": {
    "page": 0,
    "per_page": 20,
    "total": 128,
    "total_pages": 7
  },
  "_links": {
    "self": {"href": "/api/v3/otgw/messages?page=0&per_page=20"},
    "next": {"href": "/api/v3/otgw/messages?page=1&per_page=20"},
    "last": {"href": "/api/v3/otgw/messages?page=6&per_page=20"}
  }
}
```

### POST /api/v3/otgw/command

Send a command to the OpenTherm Gateway. Command format: `XX=YYYY` where XX is command code and YYYY is value.

**Request:**
```bash
curl -X POST http://otgw.local/api/v3/otgw/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "TT=21.5"
  }'
```

**Response (202 Accepted):**
```json
{
  "status": "queued",
  "command": "TT=21.5",
  "queue_position": 1
}
```

### GET /api/v3/otgw/data

Get current OpenTherm data (key temperatures, pressures, flow rates).

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/otgw/data
```

**Response (200 OK):**
```json
{
  "room_setpoint": "21.5",
  "room_temp": "20.3",
  "boiler_temp": "65.2",
  "dhw_temp": "52.1",
  "boiler_return_temp": "45.8",
  "ch_pressure": "1.2",
  "dhw_flow_rate": "10.5",
  "_links": {
    "self": {"href": "/api/v3/otgw/data"},
    "parent": {"href": "/api/v3/otgw"}
  }
}
```

## PIC Firmware Resources

### GET /api/v3/pic

Get PIC firmware resources index.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/pic
```

**Response (200 OK):**
```json
{
  "resource": "pic",
  "description": "PIC16F firmware information",
  "available": true,
  "_links": {
    "self": {"href": "/api/v3/pic"},
    "info": {"href": "/api/v3/pic/info", "title": "PIC firmware information"},
    "flash": {"href": "/api/v3/pic/flash", "title": "Flash programming status"},
    "diag": {"href": "/api/v3/pic/diag", "title": "Diagnostics information"}
  }
}
```

### GET /api/v3/pic/info

Get PIC firmware version information.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/pic/info
```

**Response (200 OK):**
```json
{
  "available": true,
  "gateway_version": "4.3.1",
  "interface_version": "1.2.3",
  "diagnostics_version": "1.0.0",
  "product_version": "1",
  "pic_type": "PIC16F1847",
  "_links": {
    "self": {"href": "/api/v3/pic/info"},
    "parent": {"href": "/api/v3/pic"}
  }
}
```

### GET /api/v3/pic/flash

Get PIC flash programming status and available versions.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/pic/flash
```

**Response (200 OK):**
```json
{
  "is_flashing": false,
  "flash_status": 0,
  "flash_error": 0,
  "bytes_flashed": 0,
  "available_versions": [
    {
      "type": "gateway",
      "path": "/api/v3/export/pic-versions/gateway"
    },
    {
      "type": "interface",
      "path": "/api/v3/export/pic-versions/interface"
    },
    {
      "type": "diagnostics",
      "path": "/api/v3/export/pic-versions/diagnostics"
    }
  ],
  "_links": {
    "self": {"href": "/api/v3/pic/flash"},
    "parent": {"href": "/api/v3/pic"}
  }
}
```

## Sensor Resources

### GET /api/v3/sensors

Get sensor resources index.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/sensors
```

**Response (200 OK):**
```json
{
  "resource": "sensors",
  "description": "External sensor data (Dallas, S0)",
  "_links": {
    "self": {"href": "/api/v3/sensors"},
    "dallas": {"href": "/api/v3/sensors/dallas", "title": "Dallas temperature sensors"},
    "s0": {"href": "/api/v3/sensors/s0", "title": "S0 pulse counter"}
  }
}
```

### GET /api/v3/sensors/dallas

Get all connected Dallas temperature sensors.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/sensors/dallas
```

**Response (200 OK):**
```json
{
  "enabled": true,
  "sensors": [
    {
      "index": 0,
      "address": "28:FF:12:34:56:78:90:AB",
      "name": "Room Temperature",
      "temperature": 20.5,
      "unit": "°C"
    },
    {
      "index": 1,
      "address": "28:FF:12:34:56:78:90:CD",
      "name": "Outside Temperature",
      "temperature": 5.2,
      "unit": "°C"
    }
  ],
  "_links": {
    "self": {"href": "/api/v3/sensors/dallas"},
    "parent": {"href": "/api/v3/sensors"}
  }
}
```

### GET /api/v3/sensors/s0

Get S0 pulse counter data.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/sensors/s0
```

**Response (200 OK):**
```json
{
  "enabled": true,
  "pulses": 12345,
  "unit": "pulses",
  "energy_kwh": 12.345,
  "_actions": {
    "reset": {
      "href": "/api/v3/sensors/s0",
      "method": "PUT",
      "title": "Reset counter to zero"
    }
  },
  "_links": {
    "self": {"href": "/api/v3/sensors/s0"},
    "parent": {"href": "/api/v3/sensors"}
  }
}
```

### PUT /api/v3/sensors/s0

Reset S0 pulse counter to zero.

**Request:**
```bash
curl -X PUT http://otgw.local/api/v3/sensors/s0
```

**Response (200 OK):**
```json
{
  "status": "reset",
  "previous_count": 12345,
  "current_count": 0
}
```

## Export Resources

### GET /api/v3/export

Get data export formats index.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/export
```

**Response (200 OK):**
```json
{
  "resource": "export",
  "description": "Data export in various formats",
  "_links": {
    "self": {"href": "/api/v3/export"},
    "telegraf": {
      "href": "/api/v3/export/telegraf",
      "title": "Telegraf metric format",
      "media_type": "text/plain"
    },
    "prometheus": {
      "href": "/api/v3/export/prometheus",
      "title": "Prometheus text format",
      "media_type": "text/plain"
    },
    "otmonitor": {
      "href": "/api/v3/export/otmonitor",
      "title": "OTmonitor format",
      "media_type": "application/json"
    },
    "settings": {
      "href": "/api/v3/export/settings",
      "title": "Current settings as JSON",
      "media_type": "application/json"
    },
    "logs": {
      "href": "/api/v3/export/logs",
      "title": "Recent debug logs",
      "media_type": "text/plain"
    }
  }
}
```

### GET /api/v3/export/telegraf

Get data in Telegraf line protocol format.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/export/telegraf
```

**Response (200 OK, text/plain):**
```
otgw,host=otgw,location=kitchen room_temp=20.3,boiler_temp=65.2,dhw_temp=52.1 1706730000000000000
```

### GET /api/v3/export/prometheus

Get data in Prometheus text exposition format.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/export/prometheus
```

**Response (200 OK, text/plain; version=0.0.4):**
```
otgw_room_temperature_celsius 20.5
otgw_boiler_temperature_celsius 65.2
otgw_flamestatus 1
...
```

### GET /api/v3/export/settings

Export all current settings as JSON.

**Request:**
```bash
curl -X GET http://otgw.local/api/v3/export/settings
```

**Response (200 OK, application/json):**
```json
{
  "hostname": "otgw",
  "dhcp": true,
  "ip": "192.168.1.100",
  "netmask": "255.255.255.0",
  "gateway": "192.168.1.1",
  "dns1": "192.168.1.1",
  "mqtt_enabled": true,
  "mqtt_broker": "192.168.1.50",
  "mqtt_port": 1883,
  "mqtt_topic_prefix": "otgw",
  "mqtt_unique_id": "otgw-001",
  "ntp_timezone": "Europe/Amsterdam",
  "led_mode": 1
}
```

## Caching & ETags

### Using ETags for Conditional Requests

The API supports HTTP caching using ETags. Cacheable endpoints include:

- `GET /api/v3/system/info` (changes only on firmware update)
- `GET /api/v3/config/*` (changes when settings are updated)
- `GET /api/v3/pic/info` (changes when PIC firmware is updated)

### Example: Conditional GET

**First request:**
```bash
curl -i -X GET http://otgw.local/api/v3/system/info

HTTP/1.1 200 OK
ETag: "a1b2c3d4-5678901"
Cache-Control: public, max-age=3600
Content-Type: application/json

{
  "hostname": "otgw",
  "version": "1.0.0",
  ...
}
```

**Subsequent request with ETag:**
```bash
curl -i -X GET http://otgw.local/api/v3/system/info \
  -H 'If-None-Match: "a1b2c3d4-5678901"'

HTTP/1.1 304 Not Modified
ETag: "a1b2c3d4-5678901"
Cache-Control: public, max-age=3600
```

The 304 response saves bandwidth by sending only headers, not the body.

## Pagination

### Using Pagination for Large Collections

The `GET /api/v3/otgw/messages` endpoint returns paginated results.

### Query Parameters

| Parameter | Type | Default | Max | Description |
|-----------|------|---------|-----|-------------|
| page | integer | 0 | N/A | Page number (0-indexed) |
| per_page | integer | 20 | 50 | Items per page |

### Example: Getting Page 2

**Request:**
```bash
curl -X GET "http://otgw.local/api/v3/otgw/messages?page=1&per_page=20"
```

**Response includes pagination metadata:**
```json
{
  "total": 128,
  "page": 1,
  "per_page": 20,
  "total_pages": 7,
  "messages": [...],
  "_links": {
    "self": {"href": "/api/v3/otgw/messages?page=1&per_page=20"},
    "first": {"href": "/api/v3/otgw/messages?page=0&per_page=20"},
    "prev": {"href": "/api/v3/otgw/messages?page=0&per_page=20"},
    "next": {"href": "/api/v3/otgw/messages?page=2&per_page=20"},
    "last": {"href": "/api/v3/otgw/messages?page=6&per_page=20"}
  }
}
```

## Filtering

### Query Parameter Filters

Supported on `GET /api/v3/otgw/messages`:

| Parameter | Values | Description |
|-----------|--------|-------------|
| category | temperature, pressure, flow, status, setpoint | Filter by message category |
| updated_after | Unix timestamp | Return only messages updated after timestamp |
| connected | true, false | For sensors, show only connected devices |

### Example: Filter by Category

**Request (temperature messages only):**
```bash
curl -X GET "http://otgw.local/api/v3/otgw/messages?category=temperature"
```

**Response:**
```json
{
  "messages": [
    {"id": 24, "label": "Room Temperature", "value": "20.3"},
    {"id": 25, "label": "Boiler Water Temperature", "value": "65.2"},
    {"id": 26, "label": "DHW Temperature", "value": "52.1"},
    {"id": 27, "label": "Outside Temperature", "value": "5.2"},
    {"id": 28, "label": "Return Temperature", "value": "45.8"}
  ]
}
```

## CORS

### Cross-Origin Requests

The API supports CORS for browser-based requests.

**Response headers on every request:**
```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, If-None-Match, If-Match
```

**Preflight request (OPTIONS):**
```bash
curl -X OPTIONS http://otgw.local/api/v3/otgw \
  -H "Access-Control-Request-Method: PATCH"

HTTP/1.1 204 No Content
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, If-None-Match, If-Match
```

## Migration Guide (from v1/v2)

### Changes in v3

| Feature | v1 | v2 | v3 |
|---------|----|----|-----|
| API Root | /api/v1/ | /api/v2/ | /api/v3/ |
| HATEOAS | No | No | **Yes** |
| ETag Caching | No | No | **Yes** |
| Pagination | No | No | **Yes** |
| Query Filtering | No | No | **Yes** |
| PATCH Method | No | No | **Yes** |
| DELETE Method | No | No | **Yes** |
| Structured Errors | Partial | Partial | **Yes** |
| CORS Support | No | No | **Yes** |

### Backward Compatibility

**v0, v1, and v2 endpoints remain fully functional:**

```bash
# v0 still works
curl -X GET http://otgw.local/api/v0/otgw/25

# v1 still works
curl -X GET http://otgw.local/api/v1/otgw/id/25

# v2 still works
curl -X GET http://otgw.local/api/v2/otgw/otmonitor

# v3 new way
curl -X GET http://otgw.local/api/v3/otgw/messages?id=25
```

### Migration Path

1. **Update clients to use v3 endpoints** - New HATEOAS links guide discovery
2. **Leverage ETag caching** - Reduce bandwidth with 304 Not Modified responses
3. **Use pagination** - Handle large collections efficiently
4. **Implement filtering** - Reduce payload size with query parameters
5. **Handle new HTTP methods** - Use PATCH for partial updates, DELETE for removal
6. **Adopt structured errors** - Parse error codes and timestamps

### Example Migration: Getting Boiler Temperature

**v1 approach:**
```bash
curl -X GET http://otgw.local/api/v1/otgw/id/25
```

**v3 approach (with HATEOAS discovery):**
```bash
# Get API root to discover resources
curl -X GET http://otgw.local/api/v3/

# Use links to navigate to messages
curl -X GET http://otgw.local/api/v3/otgw/messages?id=25

# Response includes HATEOAS links for next request
# No need to hardcode URLs
```

## Rate Limiting (Optional)

If rate limiting is enabled, the following headers appear:

### Headers

| Header | Example | Meaning |
|--------|---------|---------|
| X-RateLimit-Limit | 60 | Maximum requests per minute |
| X-RateLimit-Remaining | 45 | Requests remaining in window |
| Retry-After | 60 | Seconds to wait before retrying |

### Status Code 429 (Too Many Requests)

```json
{
  "error": "RATE_LIMIT_EXCEEDED",
  "message": "Too many requests. Maximum 60 per minute.",
  "status": 429,
  "timestamp": 1706730000
}
```

## Best Practices

1. **Use HATEOAS Links** - Don't hardcode URLs, follow `_links` in responses
2. **Implement ETag Caching** - Check for 304 responses to save bandwidth
3. **Handle Pagination** - Use `page` and `per_page` for large collections
4. **Validate Input** - Catch 400 errors for malformed requests
5. **Respect Rate Limits** - Honor Retry-After header if implemented
6. **Use Appropriate Methods** - GET (read), POST (create), PUT (replace), PATCH (update), DELETE (remove)
7. **Check Response Codes** - Treat 2xx as success, 4xx as client errors, 5xx as server errors

---

**Documentation Version:** 3.0.0  
**Last Updated:** 2026-01-31  
**API Version:** 3.0.0
