# REST API v3 - Quick Testing Guide

## Prerequisites

- OTGW device on local network
- Device hostname or IP address
- Network access to device

## Basic Testing Commands

### 1. API Root - Discover Available Resources

```bash
curl http://otgw.local/api/v3
```

**Expected Response:**
```json
{
  "version": "3.0.0",
  "api_level": 3,
  "description": "OTGW Firmware REST API v3 with HATEOAS support",
  "_links": {
    "self": { "href": "/api/v3" },
    "api_root": { "href": "/api/v3" },
    "health": {
      "href": "/api/v3/system/health",
      "description": "System health status"
    },
    ...
  }
}
```

### 2. System Health Check

```bash
curl http://otgw.local/api/v3/system/health
```

**Expected Response:**
```json
{
  "status": "UP",
  "timestamp": 1738368000,
  "components": {
    "wifi": "UP",
    "mqtt": "UP",
    "otgw": "UP",
    "filesystem": "UP"
  },
  "metrics": {
    "uptime": 12345,
    "free_heap": 25600,
    "heap_fragmentation": 15,
    "wifi_rssi": -45
  },
  "_links": {
    "self": { "href": "/api/v3/system/health" },
    "api_root": { "href": "/api/v3" },
    "system_info": { "href": "/api/v3/system/info" }
  }
}
```

### 3. Device Information

```bash
curl http://otgw.local/api/v3/system/info
```

**Expected Response:**
```json
{
  "hostname": "OTGW",
  "firmware_version": "v1.0.0-rc6",
  "compile_date": "Jan 31 2026",
  "chip_id": "12345678",
  "core_version": "2.7.4",
  ...
}
```

### 4. OTGW Status

```bash
curl http://otgw.local/api/v3/otgw/status
```

**Expected Response:**
```json
{
  "available": true,
  "timestamp": 1738368000,
  "room_setpoint": 21.0,
  "room_temp": 20.5,
  "boiler_temp": 65.0,
  "dhw_temp": 55.0,
  "return_temp": 60.0,
  "ch_pressure": 1.5,
  "_links": {
    "self": { "href": "/api/v3/otgw/status" },
    "otgw": { "href": "/api/v3/otgw" },
    "messages": { "href": "/api/v3/otgw/messages" }
  }
}
```

### 5. Get Specific OpenTherm Message

```bash
# Room temperature (message ID 24)
curl http://otgw.local/api/v3/otgw/messages/24
```

**Expected Response:**
```json
{
  "id": 24,
  "label": "Tr",
  "unit": "°C",
  "value": 20.5,
  "_links": {
    "self": { "href": "/api/v3/otgw/messages/24" },
    "api_root": { "href": "/api/v3" },
    "messages": { "href": "/api/v3/otgw/messages" }
  }
}
```

### 6. Send OTGW Command

```bash
# Set temporary temperature override to 21.5°C
curl -X POST http://otgw.local/api/v3/otgw/command \
  -H "Content-Type: application/json" \
  -d '{"command":"TT=21.5"}'
```

**Expected Response:**
```json
{
  "success": true,
  "command": "TT=21.5",
  "queued": true,
  "timestamp": 1738368000,
  "_links": {
    "self": { "href": "/api/v3/otgw/command" },
    "api_root": { "href": "/api/v3" },
    "status": { "href": "/api/v3/otgw/status" }
  }
}
```

### 7. MQTT Configuration

```bash
curl http://otgw.local/api/v3/config/mqtt
```

**Expected Response:**
```json
{
  "enabled": true,
  "broker": "192.168.1.10",
  "port": 1883,
  "user": "homeassistant",
  "topic_prefix": "otgw",
  "ha_prefix": "homeassistant",
  "_links": {
    "self": { "href": "/api/v3/config/mqtt" },
    "config": { "href": "/api/v3/config" }
  }
}
```

### 8. Sensors Index

```bash
curl http://otgw.local/api/v3/sensors
```

**Expected Response:**
```json
{
  "description": "External sensor data",
  "_links": {
    "self": { "href": "/api/v3/sensors" },
    "api_root": { "href": "/api/v3" },
    "dallas": {
      "href": "/api/v3/sensors/dallas",
      "description": "Dallas temperature sensors"
    },
    "s0": {
      "href": "/api/v3/sensors/s0",
      "description": "S0 pulse counter"
    }
  }
}
```

## Common Message IDs

| ID | Label | Description | Unit |
|----|-------|-------------|------|
| 0 | Status | Status flags | - |
| 16 | TrSet | Room Setpoint | °C |
| 17 | RelModLevel | Relative Modulation Level | % |
| 24 | Tr | Room Temperature | °C |
| 25 | Tboiler | Boiler Flow Water Temperature | °C |
| 26 | Tdhw | DHW Temperature | °C |
| 28 | Tret | Return Water Temperature | °C |
| 41 | CHPressure | Water Pressure in CH Circuit | bar |
| 42 | DHWFlowRate | Water Flow Rate in DHW Circuit | l/min |
| 56 | TdhwSet | DHW Setpoint | °C |
| 57 | MaxTSet | Max CH Water Setpoint | °C |

## OTGW Commands

| Command | Description | Example |
|---------|-------------|---------|
| `TT=temp` | Temporary temperature override | `TT=21.5` |
| `TC=temp` | Constant temperature override | `TC=20.0` |
| `SB=temp` | Set boiler setpoint | `SB=65` |
| `SW=temp` | Set DHW setpoint | `SW=55` |
| `CS=temp` | Set central heating setpoint | `CS=21` |

## Error Responses

### 404 Not Found
```json
{
  "error": true,
  "code": 404,
  "message": "Not found",
  "details": "/api/v3/invalid/path",
  "timestamp": 1738368000,
  "_links": {
    "api_root": { "href": "/api/v3" }
  }
}
```

### 400 Bad Request
```json
{
  "error": true,
  "code": 400,
  "message": "Invalid message ID",
  "details": "ID must be between 0 and 127",
  "timestamp": 1738368000,
  "_links": {
    "api_root": { "href": "/api/v3" }
  }
}
```

### 405 Method Not Allowed
```json
{
  "error": true,
  "code": 405,
  "message": "Method not allowed"
}
```

## Testing with PowerShell (Windows)

```powershell
# Get API root
Invoke-RestMethod -Uri "http://otgw.local/api/v3"

# Get system health
Invoke-RestMethod -Uri "http://otgw.local/api/v3/system/health"

# Get room temperature
Invoke-RestMethod -Uri "http://otgw.local/api/v3/otgw/messages/24"

# Send command
$body = @{command="TT=21.5"} | ConvertTo-Json
Invoke-RestMethod -Uri "http://otgw.local/api/v3/otgw/command" -Method POST -Body $body -ContentType "application/json"
```

## Testing with JavaScript (Browser Console)

```javascript
// Get API root
fetch('http://otgw.local/api/v3')
  .then(r => r.json())
  .then(data => console.log(data));

// Get system health
fetch('http://otgw.local/api/v3/system/health')
  .then(r => r.json())
  .then(data => console.log(data));

// Send command
fetch('http://otgw.local/api/v3/otgw/command', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({command: 'TT=21.5'})
})
  .then(r => r.json())
  .then(data => console.log(data));
```

## Testing with Python

```python
import requests

# Get API root
response = requests.get('http://otgw.local/api/v3')
print(response.json())

# Get system health
response = requests.get('http://otgw.local/api/v3/system/health')
print(response.json())

# Send command
response = requests.post(
    'http://otgw.local/api/v3/otgw/command',
    json={'command': 'TT=21.5'}
)
print(response.json())
```

## Navigation Using HATEOAS

1. **Start at API root** to discover available resources
2. **Follow links** from responses to navigate
3. **No hardcoded URLs** needed except the root
4. **Self-documenting** - descriptions in links guide usage

Example navigation flow:
```
GET /api/v3 
  → discover /system/health
    → GET /system/health
      → discover /system/info
        → GET /system/info
```

## Status Codes Reference

| Code | Meaning | When |
|------|---------|------|
| 200 | OK | Successful request |
| 400 | Bad Request | Invalid input (bad message ID, malformed JSON) |
| 404 | Not Found | Resource doesn't exist |
| 405 | Method Not Allowed | Wrong HTTP method (e.g., POST on GET-only endpoint) |
| 501 | Not Implemented | Feature planned but not yet implemented |

## Next Steps

1. Try the API root to see all available endpoints
2. Use HATEOAS links to navigate without documentation
3. Check system health before querying OTGW data
4. Use proper HTTP methods (GET for reading, POST for commands)
5. Handle error responses gracefully

## Troubleshooting

### "Connection refused"
- Check device is powered on
- Verify IP address/hostname
- Ensure device is on same network

### "404 Not found" on /api/v3
- Firmware may not be updated to version with v3 support
- Check firmware version: `GET /api/v3/system/info`

### "OTGW not available"
- Check OTGW hardware connection
- Verify PIC firmware is running
- Check `GET /api/v3/system/health` for component status

## Additional Resources

- Full API Reference: `docs/wiki/3-Complete-API-Reference.md`
- Migration Guide: `docs/wiki/4-Migration-Guide.md`
- Error Handling: `docs/wiki/6-Error-Handling-Guide.md`
- Troubleshooting: `docs/wiki/8-Troubleshooting.md`
