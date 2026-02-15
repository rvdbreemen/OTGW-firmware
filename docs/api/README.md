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

- **v0**: Legacy API (deprecated, maintained for backward compatibility)
- **v1**: Current stable API with improved structure  
- **v2**: Optimized API with reduced payload sizes

### Authentication

None. The device is designed for local network use only.

**Security Note**: Do not expose the device directly to the internet. Use VPN for remote access.

## Quick Reference

### Health & Status
- `GET /v1/health` - Device health status
- `GET /v0/devinfo` - Comprehensive device information
- `GET /v1/devtime` - Current date/time

### OpenTherm Data
- `GET /v1/otgw/id/{msgid}` - Get value by message ID (0-127)
- `GET /v1/otgw/label/{label}` - Get value by label name
- `GET /v1/otgw/otmonitor` - All OpenTherm data
- `GET /v2/otgw/otmonitor` - Optimized payload version

### Commands
- `POST /v1/otgw/command/{command}` - Send OTGW command (format: `XX=value`)
- `POST /v1/otgw/autoconfigure` - Trigger MQTT autodiscovery

### Settings
- `GET /v1/settings` - Get all settings
- `POST /v1/settings` - Update settings

### Firmware Updates
- `GET /v1/flashstatus` - Flash status (ESP + PIC)
- `GET /v1/pic/flashstatus` - PIC flash status only

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

### Error Response
```
400: invalid msgid\r\n
```
Errors are returned as plain text with HTTP status codes.

## Boolean Values

**Important**: The v0 and v1 APIs return boolean values as strings:
- `"true"` (string)
- `"false"` (string)

This is intentional for backward compatibility. Parse accordingly.

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

### Get Device Health
```bash
curl http://otgw.local/api/v1/health
```

### Get Boiler Temperature
```bash
curl http://otgw.local/api/v1/otgw/id/25
```

### Send Temperature Override
```bash
curl -X POST http://otgw.local/api/v1/otgw/command/TT=21.5
```

### Get All OpenTherm Data
```bash
curl http://otgw.local/api/v1/otgw/otmonitor
```

### Trigger MQTT Autodiscovery
```bash
curl -X POST http://otgw.local/api/v1/otgw/autoconfigure
```

## Integration Examples

### Home Assistant REST Sensor
```yaml
sensor:
  - platform: rest
    name: "OTGW Boiler Temperature"
    resource: "http://otgw.local/api/v1/otgw/id/25"
    value_template: "{{ value_json.value }}"
    unit_of_measurement: "°C"
    scan_interval: 10
```

### Python Script
```python
import requests

# Get health status
response = requests.get('http://otgw.local/api/v1/health')
health = response.json()['health']
print(f"Status: {health['status']}, Heap: {health['heap']} bytes")

# Set temperature override
cmd = requests.post('http://otgw.local/api/v1/otgw/command/TT=21.5')
print(cmd.text)  # Should print "OK"
```

### JavaScript Fetch
```javascript
// Get OpenTherm data
fetch('http://otgw.local/api/v1/otgw/otmonitor')
  .then(response => response.json())
  .then(data => {
    console.log('OpenTherm Data:', data.otmonitor);
  })
  .catch(error => console.error('Error:', error));
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
