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

### API Version

**v2** is the only supported API version. v0 and v1 have been removed and return **410 Gone**.

### Authentication

None. The device is designed for local network use only.

**Security Note**: Do not expose the device directly to the internet. Use VPN for remote access.

## Quick Reference

### Health & Status

- `GET /v2/health` - Device health status
- `GET /v2/device/info` - Comprehensive device information
- `GET /v2/device/time` - Current date/time

### OpenTherm Data

- `GET /v2/otgw/messages/{msgid}` - Get value by message ID (0-127)
- `GET /v2/otgw/label/{label}` - Get value by label name
- `GET /v2/otgw/otmonitor` - All OpenTherm data
- `GET /v2/otgw/telegraf` - OpenTherm data in Telegraf format

### Commands

- `POST /v2/otgw/commands` - Send OTGW command (JSON body: `{"command":"TT=20.5"}`, returns 202)
- `POST /v2/otgw/discovery` - Trigger MQTT autodiscovery (returns 202)

### Settings

- `GET /v2/settings` - Get all settings
- `POST /v2/settings` - Update settings

### Sensors

- `GET /v2/sensors/labels` - Get Dallas sensor labels
- `POST /v2/sensors/labels` - Update Dallas sensor labels

### Firmware & Filesystem

- `GET /v2/flash/status` - Flash status (ESP + PIC)
- `GET /v2/pic/flash-status` - PIC flash status only
- `GET /v2/pic/update-check` - Check for PIC firmware updates
- `GET /v2/firmware/files` - List PIC firmware files
- `GET /v2/filesystem/files` - List filesystem files
- `GET /v2/filesystem/hash-check` - Check firmware/filesystem version match

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

All errors return structured JSON:
```json
{"error":{"status":400,"message":"Invalid message ID"}}
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

All responses include `Access-Control-Allow-Origin: *`.

All endpoints support **OPTIONS preflight** for cross-origin requests:
```
OPTIONS /api/v2/health HTTP/1.1

HTTP/1.1 204 No Content
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS
Access-Control-Allow-Headers: Content-Type
Access-Control-Max-Age: 86400
```

### Queued Operations

Commands and discovery return **202 Accepted**:
```json
{"status":"queued"}
```

## Boolean Values

Most endpoints return boolean values as strings (`"true"` / `"false"`).
The `/v2/device/info` endpoint returns proper JSON booleans (`true`/`false`).

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

- **Health checks**: 30-60 seconds minimum — **each call writes a probe file to LittleFS flash** (see note below)
- **OpenTherm data**: 5-10 seconds
- **Flash status (during upgrade)**: 1-2 seconds
- **Settings**: On-demand only

> **Health endpoint flash write**: `GET /v2/health` calls `updateLittleFSStatus()` on every
> request, which writes a small probe file (`/.health`) to LittleFS to verify the filesystem is writable.
> This is intentional — it confirms the flash is not just mounted but actively writeable — but it means
> each health request incurs a LittleFS write cycle. The designed use-case is post-OTA polling, which stops
> immediately once `status: UP` is received. Avoid using this endpoint as a high-frequency external monitor.

### Memory Protection

The API enforces a minimum 4KB free heap before processing requests. If heap is below this threshold, the API returns:
```json
{"error":{"status":500,"message":"Internal server error (low heap)"}}
```

This prevents crashes and ensures stable operation.

## Examples

### Get Device Health

```bash
curl http://otgw.local/api/v2/health
```

### Get Device Information

```bash
curl http://otgw.local/api/v2/device/info
```

### Get Boiler Temperature

```bash
curl http://otgw.local/api/v2/otgw/messages/25
```

### Send Temperature Override

```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"command":"TT=21.5"}' \
  http://otgw.local/api/v2/otgw/commands
```

### Get All OpenTherm Data

```bash
curl http://otgw.local/api/v2/otgw/otmonitor
```

### Trigger MQTT Autodiscovery

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

## Integration Examples

### Home Assistant REST Sensor

```yaml
sensor:
  - platform: rest
    name: "OTGW Boiler Temperature"
    resource: "http://otgw.local/api/v2/otgw/messages/25"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "°C"
    scan_interval: 10
```

### Python Script

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

# Set temperature override (JSON body, returns 202)
cmd = requests.post('http://otgw.local/api/v2/otgw/commands',
                     json={"command": "TT=21.5"})
print(cmd.json())  # {"status": "queued"}
```

### JavaScript Fetch

```javascript
// Get OpenTherm data
fetch('http://otgw.local/api/v2/otgw/otmonitor')
  .then(response => {
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return response.json();
  })
  .then(data => console.log('OpenTherm Data:', data.otmonitor))
  .catch(error => console.error('Error:', error));

// Send command (JSON body)
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
- **OpenTherm Specification**: `docs/opentherm specification/` directory in repository
- **OTGW Hardware**: https://otgw.tclcode.com/
- **Issues**: https://github.com/rvdbreemen/OTGW-firmware/issues

## Contributing

Found an error in the API documentation? Please open an issue or submit a pull request.

When updating the API:
1. Update `openapi.yaml` with new endpoints/changes
2. Validate the specification: `swagger-cli validate openapi.yaml`
3. Test endpoints on actual hardware
4. Update examples in this README if needed
