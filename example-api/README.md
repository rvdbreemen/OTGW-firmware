# OTGW REST API Examples

Comprehensive examples and documentation for all OTGW REST API versions.

## Available Versions

### [v1 Examples](v1/) - Stable & Production-Ready

The original REST API with simple endpoints.

**Status**: ✅ Active, Stable, Recommended for existing projects

**Features:**
- Simple HTTP endpoints
- JSON request/response
- Health checks and monitoring
- Temperature control
- Settings management
- Multiple export formats

**Files:**
- `curl_examples.sh` - 13+ bash examples
- `javascript_examples.js` - OTGWClientV1 class, 6 examples
- `python_examples.py` - OTGWClientV1 class, 8 examples
- `README.md` - Complete v1 documentation

**When to use:** Existing projects, simple use cases, maximum compatibility

---

### [v2 Examples](v2/) - Enhanced & Backward Compatible

Improved v1 with enhanced OTmonitor endpoint, 100% v1 compatible.

**Status**: ✅ Recommended for new projects

**Features:**
- All v1 features included
- Enhanced OTmonitor endpoint
- Better structured data
- Easier parsing
- Full backward compatibility (zero breaking changes)
- Migration helpers

**Files:**
- `curl_examples.sh` - v2 features with v1 compatibility info
- `javascript_examples.js` - OTGWClientV2 (extends v1)
- `python_examples.py` - OTGWClientV2 (extends v1)
- `README.md` - v2 documentation and migration guide

**When to use:** New projects, want cleaner data structures, no reason to upgrade existing projects

---

### [v3 Examples](v3/) - Modern & RESTful

Modern Richardson Level 3 API with HATEOAS, advanced features, and comprehensive documentation.

**Status**: ✅ Recommended for new feature-rich projects

**Features:**
- Richardson Maturity Level 3 with HATEOAS
- 31 endpoints across 6 categories
- ETag caching and 304 responses
- Pagination with HATEOAS links
- Query filtering
- Rate limiting
- Full HTTP method support (GET, POST, PATCH, PUT, DELETE, OPTIONS)
- Standards compliance (RFC 7231, 7232, 5789, W3C CORS)

**Files:**
- `curl_examples.sh` - 15+ curl examples
- `javascript_examples.js` - OTGWClient, 50+ functions
- `python_examples.py` - OTGWClient, 30+ methods
- `use_cases.md` - 8+ real-world scenarios
- `README.md` - v3 overview and getting started
- Plus comprehensive API documentation

**When to use:** New projects, want modern REST practices, complex integrations, requires pagination/filtering

---

## Quick Comparison

| Feature | v1 | v2 | v3 |
|---------|----|----|-----|
| **Status** | ✅ Stable | ✅ Enhanced | ✅ Modern |
| **Endpoints** | 11 | 11 | 31 |
| **HTTP Methods** | GET, POST | GET, POST | GET, POST, PATCH, PUT, DELETE, OPTIONS |
| **HATEOAS** | ❌ | ❌ | ✅ |
| **ETag Caching** | ❌ | ❌ | ✅ |
| **Pagination** | ❌ | ❌ | ✅ |
| **Filtering** | ❌ | ❌ | ✅ |
| **Rate Limiting** | ❌ | ❌ | ✅ |
| **Backward Compatible** | N/A | ✅ 100% | ⚠️ v0-v2 coexist |
| **Maturity Level** | 1-2 | 1-2 | 3 |
| **Breaking Changes** | N/A | None | None (v0-v2 untouched) |

## Feature Matrix

### Endpoints

| Endpoint | v1 | v2 | v3 |
|----------|----|----|-----|
| System Info | ✅ | ✅ | ✅ |
| Health Check | ✅ | ✅ | ✅ |
| Settings | ✅ | ✅ | ✅ |
| Flash Status | ✅ | ✅ | ✅ |
| OpenTherm Data | ✅ | ✅ | ✅ |
| OTGW Commands | ✅ | ✅ | ✅ |
| Sensor Data | ❌ | ❌ | ✅ |
| PIC Information | ❌ | ❌ | ✅ |
| Export Formats | ✅ | ✅ | ✅ |
| API Discovery | ❌ | ❌ | ✅ |

### Advanced Features

| Feature | v1 | v2 | v3 |
|---------|----|----|-----|
| HTTP Caching (ETag) | ❌ | ❌ | ✅ |
| 304 Not Modified | ❌ | ❌ | ✅ |
| Pagination | ❌ | ❌ | ✅ |
| Query Filtering | ❌ | ❌ | ✅ |
| Rate Limiting | ❌ | ❌ | ✅ |
| CORS Support | ❌ | ❌ | ✅ |
| Method Validation | ✅ | ✅ | ✅ |
| Error Codes | ✅ | ✅ | ✅ Extended |

## Getting Started

### I'm new to OTGW - which API should I use?

**→ Start with v3** if you want modern, feature-rich API
**→ Use v1** if you prefer simplicity and proven stability

### I have a v1 app - should I upgrade?

**→ Not required** - v1 continues to work indefinitely
**→ Consider v2** if you want cleaner OTmonitor parsing (zero effort migration)
**→ Migrate to v3** when you need pagination, caching, or filtering

### I'm building something new - what should I choose?

**→ v3** for complex projects with filtering, pagination, caching needs
**→ v2** for straightforward projects that want better data structures
**→ v1** if you need maximum simplicity and compatibility

## File Organization

```
example-api/
├── v1/                          # Stable v1 examples
│   ├── README.md               # v1 documentation
│   ├── curl_examples.sh        # 13+ curl examples
│   ├── javascript_examples.js  # OTGWClientV1 class
│   └── python_examples.py      # OTGWClientV1 class
│
├── v2/                          # Enhanced v2 examples
│   ├── README.md               # v2 documentation
│   ├── curl_examples.sh        # v2 feature examples
│   ├── javascript_examples.js  # OTGWClientV2 class
│   └── python_examples.py      # OTGWClientV2 class
│
├── v3/                          # Modern v3 examples
│   ├── README.md               # v3 getting started
│   ├── curl_examples.sh        # 15+ curl examples
│   ├── javascript_examples.js  # OTGWClient class, 50+ functions
│   ├── python_examples.py      # OTGWClient class, 30+ methods
│   ├── use_cases.md            # 8+ real-world scenarios
│   └── API_REFERENCE.md        # Complete API documentation
│
└── README.md                    # This file
```

## Usage Examples

### cURL - Get Current Room Temperature

**v1/v2/v3** (all the same):
```bash
curl http://192.168.1.100/api/v1/otgw/id/16 | jq '.value'
```

### JavaScript - Monitor Temperature

**v1:**
```javascript
const client = new OTGWClientV1('192.168.1.100');
const temps = await client.getTemperatures();
console.log(`Room: ${temps.room}°C`);
```

**v2:**
```javascript
const client = new OTGWClientV2('192.168.1.100');
const status = await client.getStatusInfo();
console.log(`Room: ${status.room_temperature}°C`);
```

**v3:**
```javascript
const client = new OTGWClient('192.168.1.100');
const system = await client.getSystemInfo();
const temps = await client.getSensorTemperatures();
console.log(`Room: ${temps.room}°C`);
```

### Python - Control Heating

**v1:**
```python
client = OTGWClientV1('192.168.1.100')
client.set_room_temperature(22.0)
client.enable_heating()
```

**v2:** (identical - full compatibility)
```python
client = OTGWClientV2('192.168.1.100')
client.set_room_temperature(22.0)
client.enable_heating()
```

**v3:**
```python
client = OTGWClient('192.168.1.100')
await client.patch_config_otgw({'temperature_override': 22.0})
```

## Running Examples

### cURL Examples

```bash
# v1
cd v1
bash curl_examples.sh

# v2
cd v2
bash curl_examples.sh

# v3
cd v3
bash curl_examples.sh
```

### JavaScript Examples

```bash
# Install dependencies
npm install requests

# v1
node -e "const {example_basicStatus} = require('./v1/javascript_examples.js'); example_basicStatus();"

# v3
node -e "const {example_basicStatus} = require('./v3/javascript_examples.js'); example_basicStatus();"
```

### Python Examples

```bash
# v1
python3 v1/python_examples.py

# v2
python3 v2/python_examples.py

# v3
python3 v3/python_examples.py
```

## API Endpoints Summary

### Core Endpoints (All Versions)

```
GET    /api/v1/health
GET    /api/v1/settings
POST   /api/v1/settings
GET    /api/v1/otgw/id/{msgid}
GET    /api/v1/otgw/label/{label}
POST   /api/v1/otgw/command/{command}
```

### Additional v2

```
GET    /api/v2/otgw/otmonitor    (enhanced format)
```

### Full v3

```
GET    /api/v3/
GET    /api/v3/system/*
GET    /api/v3/config/*
PATCH  /api/v3/config/*
GET    /api/v3/otgw/*
POST   /api/v3/otgw/command
GET    /api/v3/pic/*
GET    /api/v3/sensors/*
GET    /api/v3/export/*
```

Plus comprehensive endpoint discovery and HATEOAS links!

## Migration Paths

### v1 → v2
- **Effort**: None (100% compatible)
- **Breaking changes**: None
- **Reason**: Better OTmonitor parsing

### v1 → v3
- **Effort**: Moderate (new endpoints)
- **Breaking changes**: None (v1 endpoints still work)
- **Reason**: Modern REST, caching, pagination, filtering

### v2 → v3
- **Effort**: Moderate
- **Breaking changes**: None (v1/v2 endpoints still work)
- **Reason**: Advanced features

## Standards Compliance

### v1 & v2
- HTTP/1.1 basics
- Simple JSON

### v3
- ✅ RFC 7231 (HTTP Semantics)
- ✅ RFC 7232 (Conditional Requests)
- ✅ RFC 5789 (PATCH Method)
- ✅ W3C CORS
- ✅ Richardson Maturity Level 3
- ✅ HATEOAS (Hypermedia)

## Performance

All versions:
- Response time: < 100ms typical
- Payload: 100-500 bytes (v1/v2), up to 4KB (v3)
- Concurrent: 4+ simultaneous requests
- Rate limit: ~10 req/sec per client

## Common Use Cases

| Use Case | Recommended |
|----------|-------------|
| Simple temperature monitoring | v1 or v2 |
| Home Assistant integration | v1, v2, or v3 |
| Data export to InfluxDB | v1, v2, or v3 |
| Complex filtering/pagination | v3 |
| Mobile app backend | v3 |
| IoT device integration | v1 (resource constrained) |
| Web dashboard | v3 |
| Legacy system integration | v1 |

## Device Compatibility

All examples work with:
- OTGW-firmware v1.0+ (v1 endpoints)
- OTGW-firmware v1.0+ (v2 endpoints)
- OTGW-firmware v1.1+ (v3 endpoints)

Check device version:
```bash
curl -s http://192.168.1.100/api/v1/health | jq '.version'
```

## Documentation Structure

Each API version has:
- **README.md** - Getting started and overview
- **curl_examples.sh** - Executable bash examples
- **javascript_examples.js** - Working client library + examples
- **python_examples.py** - Working client library + examples
- **Additional docs** (v3 only) - Comprehensive guides

## Support & Issues

If you encounter issues with examples:

1. **Check the version-specific README**
2. **Try the curl examples first** (easiest to debug)
3. **Review the example code** for patterns
4. **Check GitHub issues** for similar problems
5. **Update to latest firmware** if possible

## Contributing

Examples are maintained in this repository. To contribute:

1. Follow existing patterns and style
2. Include working code that others can run
3. Test on actual hardware when possible
4. Update relevant README files
5. Submit pull request with clear description

## License

All example code and documentation are provided under the same license as OTGW-firmware. See [LICENSE](../LICENSE) for details.

## Next Steps

1. **Read a Version-Specific README**: [v1](v1/), [v2](v2/), or [v3](v3/)
2. **Try Some cURL Examples**: Easiest way to understand endpoints
3. **Pick Your Language**: JavaScript or Python client libraries
4. **Build Your Integration**: Use examples as starting point
5. **Monitor in Production**: Use health endpoints for health checks

---

**Last Updated**: January 2026  
**API Versions**: v1 (stable), v2 (enhanced), v3 (modern)  
**Status**: All versions actively maintained
