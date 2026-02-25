# OTGW-firmware REST API Documentation

This directory contains formal API documentation for the OTGW-firmware REST API.

## OpenAPI Specification

The `openapi.yaml` file provides a complete OpenAPI 3.0 specification of the REST API.

### Using the Specification

**View in Swagger UI**:
1. Visit https://editor.swagger.io/
2. Click File → Import File
3. Upload `openapi.yaml`

**Generate Client Code**:
Use the OpenAPI specification to generate client libraries:
```bash
# Install OpenAPI Generator
npm install @openapitools/openapi-generator-cli -g

# Generate Python client
openapi-generator-cli generate -i openapi.yaml -g python -o ./client/python

# Generate JavaScript client
openapi-generator-cli generate -i openapi.yaml -g javascript -o ./client/javascript
```

**Validate the Specification**:
```bash
# Using swagger-cli
npm install -g @apidevtools/swagger-cli
swagger-cli validate openapi.yaml
```

## API Overview

### Base URL
```
http://{device-ip}/api
```

### API Versions

- **v1**: Current stable API with text-based error responses  
- **v2**: RESTful-compliant API with JSON errors, proper status codes, and resource naming. **Recommended for new integrations.**

> **Note:** v0 endpoints are deprecated and not documented here. Migrate to v1 or v2.

### Authentication

None. The device is designed for local network use only.

**Security Note**: Do not expose the device directly to the internet. Use VPN for remote access.

## Quick Reference

### Health & Status
- `GET /v2/health` - Device health status (JSON errors)
- `GET /v1/health` - Device health status (text errors)
- `GET /v2/device/info` - Comprehensive device information (map format)
- `GET /v2/device/time` - Current date/time

### OpenTherm Data
- `GET /v2/otgw/messages/{msgid}` - Get value by message ID (0-127, RESTful)
- `GET /v1/otgw/id/{msgid}` - Get value by message ID (v1 format)
- `GET /v1/otgw/label/{label}` - Get value by label name
- `GET /v2/otgw/otmonitor` - All OpenTherm data (optimized v2 format)
- `GET /v1/otgw/otmonitor` - All OpenTherm data (v1 format)
- `GET /v2/otgw/telegraf` - OpenTherm data in Telegraf format

### Commands
- `POST /v2/otgw/commands` - Send OTGW command (JSON body: `{"command":"TT=20.5"}`, returns 202)
- `POST /v1/otgw/command/{command}` - Send OTGW command (URL path format)
- `POST /v2/otgw/discovery` - Trigger MQTT autodiscovery (returns 202)
- `POST /v1/otgw/autoconfigure` - Trigger MQTT autodiscovery

### Settings
- `GET /v2/settings` - Get all settings (JSON errors)
- `POST /v2/settings` - Update settings (JSON errors)
- `GET /v1/settings` - Get all settings (text errors)
- `POST /v1/settings` - Update settings (text errors)

### Sensors
- `GET /v2/sensors/labels` - Get Dallas sensor labels (JSON errors)
- `POST /v2/sensors/labels` - Update Dallas sensor labels (JSON errors)

### Firmware & Filesystem
- `GET /v2/flash/status` - Flash status (ESP + PIC)
- `GET /v2/pic/flash-status` - PIC flash status only
- `GET /v2/firmware/files` - List PIC firmware files
- `GET /v2/filesystem/files` - List filesystem files
- `GET /v1/flashstatus` - Flash status (v1 format)
- `GET /v1/pic/flashstatus` - PIC flash status (v1 format)

## Response Formats

### Standard JSON Response
```json
{
  "health": {
    "status": "UP",
    "uptime": "2d 3h 45m",
    "heap": 25600,
    "wifirssi": -65,
    "mqttconnected": "true",
    "otgwconnected": "true"
  }
}
```

### Error Responses

**v2 (JSON errors — recommended):**
```json
{"error":{"status":400,"message":"Invalid message ID"}}
```

**v1 (text errors):**
```
400: invalid msgid\r\n
```

### 405 Method Not Allowed

All 405 responses include an `Allow` header listing valid HTTP methods (RFC 7231 §6.5.5):
```
HTTP/1.1 405 Method Not Allowed
Allow: GET
Content-Type: application/json

{"error":{"status":405,"message":"Method not allowed"}}
```

### CORS Support

All v2 responses include `Access-Control-Allow-Origin: *`.

v2 endpoints support **OPTIONS preflight** for cross-origin requests:
```
OPTIONS /api/v2/health HTTP/1.1

HTTP/1.1 204 No Content
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS
Access-Control-Allow-Headers: Content-Type
Access-Control-Max-Age: 86400
```

### v2 Queued Operations
Commands and discovery return **202 Accepted**:
```json
{"status":"queued"}
```

## Boolean Values

**Important**: The v1 API returns boolean values as strings:
- `"true"` (string)
- `"false"` (string)

The v2 `/device/info` endpoint returns proper JSON booleans (`true`/`false`).
Parse accordingly based on which API version you use.

## OpenTherm Message IDs

Message IDs range from 0-127. Common IDs:

| ID | Label | Description | Unit |
|----|-------|-------------|------|
| 0 | Status | Master/Slave status flags | - |
| 1 | controlsetpoint | Control setpoint | °C |
| 5 | ASFflags | Application-specific fault flags | - |
| 17 | relmodlvl | Relative modulation level | % |
| 25 | boilertemperature | Boiler water temperature | °C |
| 26 | dhwtemperature | DHW temperature | °C |
| 28 | returnwatertemperature | Return water temperature | °C |
| 56 | dhwsetpoint | DHW setpoint | °C |

See OpenTherm Protocol specification for complete list.

## OTGW Commands

Commands use two-letter codes followed by `=value`:

| Command | Description | Example |
|---------|-------------|---------|
| TT | Temporary temperature override | `TT=20.5` |
| SW | Set DHW setpoint | `SW=60` |
| SH | Set max CH water setpoint | `SH=80` |
| GW | Set gateway mode | `GW=1` |
| PR | Print report | `PR=A` |
| PS | Print summary | `PS=1` |

See [OTGW firmware documentation](https://otgw.tclcode.com/firmware.html) for complete command reference.

## Rate Limiting

### Recommended Polling Intervals

- **Health checks**: 30-60 seconds
- **OpenTherm data**: 5-10 seconds  
- **Flash status (during upgrade)**: 1-2 seconds
- **Settings**: On-demand only

### Memory Protection

The API enforces a minimum 4KB free heap before processing requests. If heap is below this threshold, the API returns:
```
500: internal server error (low heap)\r\n
```

This prevents crashes and ensures stable operation.

## Examples

### Get Device Health (v2)
```bash
curl http://otgw.local/api/v2/health
```

### Get Device Information (v2)
```bash
curl http://otgw.local/api/v2/device/info
```

### Get Boiler Temperature (v2)
```bash
curl http://otgw.local/api/v2/otgw/messages/25
```

### Send Temperature Override (v2)
```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"command":"TT=21.5"}' \
  http://otgw.local/api/v2/otgw/commands
```

### Get All OpenTherm Data (v2)
```bash
curl http://otgw.local/api/v2/otgw/otmonitor
```

### Trigger MQTT Autodiscovery (v2)
```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

### Get Boiler Temperature (v1)
```bash
curl http://otgw.local/api/v1/otgw/id/25
```

### Send Temperature Override (v1)
```bash
curl -X POST http://otgw.local/api/v1/otgw/command/TT=21.5
```

## Integration Examples

### Home Assistant REST Sensor (v2)
```yaml
sensor:
  - platform: rest
    name: "OTGW Boiler Temperature"
    resource: "http://otgw.local/api/v2/otgw/messages/25"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "°C"
    scan_interval: 10
```

### Python Script (v2)
```python
import requests

# Get health status
response = requests.get('http://otgw.local/api/v2/health')
health = response.json()['health']
print(f"Status: {health['status']}, Heap: {health['heap']} bytes")

# Get device info
response = requests.get('http://otgw.local/api/v2/device/info')
device = response.json()['device']
print(f"Firmware: {device['fwversion']}, Gateway: {device['gatewaymode']}")

# Set temperature override (v2: JSON body, returns 202)
cmd = requests.post('http://otgw.local/api/v2/otgw/commands',
                     json={"command": "TT=21.5"})
print(cmd.json())  # {"status": "queued"}
```

### JavaScript Fetch (v2)
```javascript
// Get OpenTherm data
fetch('http://otgw.local/api/v2/otgw/otmonitor')
  .then(response => {
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.json();
  })
  .then(data => {
    // v2 otmonitor returns optimized flat structure with name/value/unit/lastupdated per entry
    console.log('OpenTherm Data:', data.otmonitor);
  })
  .catch(error => console.error('Error:', error));

// Send command (v2: JSON body)
fetch('http://otgw.local/api/v2/otgw/commands', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({command: 'TT=21.5'})
})
  .then(response => response.json())
  .then(data => console.log(data));  // {status: "queued"}
```

## Testing Tools

### Swagger Editor
View and test the API interactively:
1. Go to https://editor.swagger.io/
2. Import `openapi.yaml`
3. Use "Try it out" feature to test endpoints

### Postman
Import the OpenAPI specification into Postman:
1. File → Import
2. Select `openapi.yaml`
3. Create requests from the imported collection

### cURL
All examples in this documentation use cURL and can be run directly from command line.

## Support & Documentation

- **Main README**: See repository root for general firmware documentation
- **OpenTherm Specification**: `Specification/` directory in repository
- **OTGW Hardware**: https://otgw.tclcode.com/
- **Issues**: https://github.com/rvdbreemen/OTGW-firmware/issues

## Contributing

Found an error in the API documentation? Please open an issue or submit a pull request.

When updating the API:
1. Update `openapi.yaml` with new endpoints/changes
2. Validate the specification: `swagger-cli validate openapi.yaml`
3. Test endpoints on actual hardware
4. Update examples in this README if needed
