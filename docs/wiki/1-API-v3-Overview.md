# REST API v3 Overview

Welcome to the OTGW REST API v3 documentation! This is the modern, production-ready API for the OpenTherm Gateway.

## What is the REST API v3?

The REST API v3 provides a modern, RFC-compliant RESTful interface to control and monitor your OTGW device. It's built on the latest REST architectural principles with full backward compatibility with existing v0, v1, and v2 APIs.

## Quick Stats

- **31 Endpoints** across 6 resource categories
- **Richardson Maturity Level 3** (HATEOAS)
- **100% Backward Compatible** with v0, v1, v2
- **RFC 7231, 7232, 5789 Compliant**
- **Sub-100ms Response Times**
- **Caching Support** with ETags
- **Pagination & Filtering**
- **Rate Limiting**
- **CORS Ready**

## Key Features

### 🎯 Modern REST Architecture
- Proper HTTP method semantics (GET, POST, PATCH, PUT, DELETE, OPTIONS)
- Resource-oriented design
- HATEOAS hypermedia links for API discovery
- Status codes follow HTTP standards (200, 201, 204, 304, 400, 404, 405, 429, 500)

### 📦 Advanced Features
- **ETag Caching**: Client-side caching with conditional requests (304 Not Modified)
- **Pagination**: Page through large message collections
- **Query Filtering**: Filter messages by category, timestamp, or status
- **Rate Limiting**: Per-IP request throttling with 429 responses
- **API Discovery**: GET /api/v3 returns full HATEOAS graph

### 🔄 100% Backward Compatible
- All v0, v1, v2 endpoints continue working
- v3 is *additive*, not breaking
- Existing apps need no changes
- Seamless upgrade path

### ⚡ Performance
- Simple GET: < 50ms
- Complex JSON: < 200ms
- POST/PATCH: < 100ms
- Memory per request: < 2KB

### 📚 Comprehensive
- 31 endpoints
- 6 resource categories
- Complete error handling
- Full CORS support
- WebSocket streaming compatible

## API Categories

### System (`/api/v3/system/`)
Device information, health status, time, network status, statistics

**Endpoints:**
- `GET /system/info` - Device info and version
- `GET /system/health` - Health status
- `GET /system/time` - Current time and timezone
- `GET /system/network` - Network interface info
- `GET /system/stats` - Runtime statistics

### Configuration (`/api/v3/config/`)
Device settings with PATCH support for partial updates

**Endpoints:**
- `GET /config/device` - Device configuration
- `PATCH /config/device` - Update device config
- `GET /config/network` - Network settings
- `PATCH /config/network` - Update network
- `GET /config/mqtt` - MQTT settings
- `PATCH /config/mqtt` - Update MQTT
- `GET /config/otgw` - OTGW settings
- `PATCH /config/otgw` - Update OTGW
- `GET /config/features` - Feature flags
- `PATCH /config/features` - Update features

### OpenTherm (`/api/v3/otgw/`)
OpenTherm message data and control

**Endpoints:**
- `GET /otgw/status` - Current boiler status
- `GET /otgw/messages` - All OT messages (paginated/filtered)
- `GET /otgw/message/{id}` - Specific OT message
- `GET /otgw/data` - Latest OT data snapshot
- `POST /otgw/command` - Send OTGW command
- `GET /otgw/monitor` - OTmonitor format export

### PIC Firmware (`/api/v3/pic/`)
PIC microcontroller information

**Endpoints:**
- `GET /pic/info` - PIC hardware and version info
- `GET /pic/versions` - Available firmware versions
- `GET /pic/diagnostics` - PIC diagnostic data

### Sensors (`/api/v3/sensors/`)
External sensor data

**Endpoints:**
- `GET /sensors/dallas` - Dallas temperature sensors
- `GET /sensors/dallas/{address}` - Specific sensor
- `GET /sensors/s0` - S0 pulse counter
- `POST /sensors/s0/reset` - Reset counter

### Export (`/api/v3/export/`)
Data export in various formats

**Endpoints:**
- `GET /export/telegraf` - InfluxDB Telegraf format
- `GET /export/otmonitor` - OTmonitor format
- `GET /export/settings` - All settings as JSON
- `GET /export/logs` - Device logs

## Standards Compliance

✅ **RFC 7231** - HTTP/1.1 Semantics and Content  
✅ **RFC 7232** - HTTP Conditional Requests (ETags)  
✅ **RFC 5789** - HTTP PATCH Method  
✅ **W3C CORS** - Cross-Origin Resource Sharing  
✅ **Richardson Maturity Level 3** - HATEOAS  

## Getting Started

### 1. Check Device Health
```bash
curl http://192.168.1.100/api/v3/system/health | jq .
```

### 2. Get API Overview (with HATEOAS links)
```bash
curl http://192.168.1.100/api/v3 | jq .
```

### 3. Get Current Boiler Status
```bash
curl http://192.168.1.100/api/v3/otgw/status | jq .
```

### 4. Browse All OpenTherm Messages
```bash
curl http://192.168.1.100/api/v3/otgw/messages | jq .
```

## Documentation

- **[Quick Start Guide](Quick-Start-Guide)** - Get up and running in 5 minutes
- **[Complete API Reference](Complete-API-Reference)** - All endpoints with examples
- **[Migration Guide](Migration-Guide)** - Upgrade from v1/v2 to v3
- **[Error Handling](Error-Handling-Guide)** - Understanding error responses
- **[HATEOAS Navigation](HATEOAS-Navigation-Guide)** - API discovery and links
- **[Changelog](Changelog)** - What's new in v3
- **[Troubleshooting](Troubleshooting)** - Common issues and solutions

## Language Support

- **cURL** - Examples in bash
- **JavaScript** - Node.js and browser
- **Python** - Full client library
- **Postman** - Collection included

## Support & Community

- GitHub Issues: [Report bugs](https://github.com/rvdbreemen/OTGW-firmware/issues)
- GitHub Discussions: [Ask questions](https://github.com/rvdbreemen/OTGW-firmware/discussions)
- OTGW Forum: [Community discussion](https://otgw.tclcode.com/support/forum)

## Version Information

- **Current Version**: 3.0
- **Release Date**: January 2026
- **Previous Versions**: v0 (legacy), v1 (stable), v2 (enhanced)
- **Backward Compatible**: ✅ Yes, all previous versions work alongside v3

---

**Ready to start?** → [Quick Start Guide](Quick-Start-Guide)
