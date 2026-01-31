# Complete API Reference

See [docs/api/v3/API_REFERENCE.md](../../docs/api/v3/API_REFERENCE.md) for the complete REST API v3 reference documentation.

The complete reference includes:

## Content

### System Endpoints
- `GET /api/v3/system/info` - Device information
- `GET /api/v3/system/health` - Health status
- `GET /api/v3/system/time` - Time and timezone
- `GET /api/v3/system/network` - Network status
- `GET /api/v3/system/stats` - Statistics

### Configuration Endpoints
- `GET /api/v3/config/device` - Device config
- `PATCH /api/v3/config/device` - Update device config
- `GET /api/v3/config/network` - Network settings
- `PATCH /api/v3/config/network` - Update network
- `GET /api/v3/config/mqtt` - MQTT settings
- `PATCH /api/v3/config/mqtt` - Update MQTT
- `GET /api/v3/config/otgw` - OTGW settings
- `PATCH /api/v3/config/otgw` - Update OTGW
- `GET /api/v3/config/features` - Feature flags
- `PATCH /api/v3/config/features` - Update features

### OpenTherm Endpoints
- `GET /api/v3/otgw/status` - Current boiler status
- `GET /api/v3/otgw/messages` - All messages
- `GET /api/v3/otgw/message/{id}` - Specific message
- `GET /api/v3/otgw/data` - Latest snapshot
- `POST /api/v3/otgw/command` - Send command
- `GET /api/v3/otgw/monitor` - OTmonitor export

### PIC Firmware Endpoints
- `GET /api/v3/pic/info` - PIC information
- `GET /api/v3/pic/versions` - Available versions
- `GET /api/v3/pic/diagnostics` - Diagnostics

### Sensor Endpoints
- `GET /api/v3/sensors/dallas` - Dallas sensors
- `GET /api/v3/sensors/dallas/{address}` - Specific sensor
- `GET /api/v3/sensors/s0` - S0 counter
- `POST /api/v3/sensors/s0/reset` - Reset counter

### Export Endpoints
- `GET /api/v3/export/telegraf` - Telegraf format
- `GET /api/v3/export/otmonitor` - OTmonitor format
- `GET /api/v3/export/settings` - Settings JSON
- `GET /api/v3/export/logs` - Device logs

### Metadata
- `GET /api/v3` - API overview with HATEOAS
- `OPTIONS *` - CORS preflight

## Features

Each endpoint documentation includes:
- Full URL path
- Supported HTTP methods
- Query parameters
- Request body schema
- Response body schema
- Example requests and responses
- Status codes
- Error handling
- HATEOAS links
- Rate limiting notes

## Access the Reference

**File Location**: [docs/api/v3/API_REFERENCE.md](../../docs/api/v3/API_REFERENCE.md)

The reference is automatically updated as the API evolves and is the authoritative source for endpoint specifications.

## Quick Links by Category

- **System Info**: [docs/api/v3/API_REFERENCE.md#system](../../docs/api/v3/API_REFERENCE.md#system)
- **Configuration**: [docs/api/v3/API_REFERENCE.md#configuration](../../docs/api/v3/API_REFERENCE.md#configuration)
- **OpenTherm**: [docs/api/v3/API_REFERENCE.md#opentherm](../../docs/api/v3/API_REFERENCE.md#opentherm)
- **Sensors**: [docs/api/v3/API_REFERENCE.md#sensors](../../docs/api/v3/API_REFERENCE.md#sensors)
- **Exports**: [docs/api/v3/API_REFERENCE.md#exports](../../docs/api/v3/API_REFERENCE.md#exports)

## Need Examples?

See [example-api](../../example-api/) for:
- **cURL**: Bash script examples for all endpoints
- **JavaScript**: Full client library with examples
- **Python**: Full client library with examples
- **v1/v2 migration**: Comparison and upgrade guides

## Related Documentation

- [API v3 Overview](API-v3-Overview) - Architecture and features
- [Quick Start Guide](Quick-Start-Guide) - Get started in 5 minutes
- [Error Handling Guide](Error-Handling-Guide) - Error responses and codes
- [HATEOAS Navigation](HATEOAS-Navigation-Guide) - API discovery
- [Migration Guide](Migration-Guide) - Upgrade from v1/v2
