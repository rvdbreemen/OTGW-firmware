# Quick Reference - REST API Examples

## File Locations

### v1 Examples (Stable)
```
example-api/v1/
├── README.md                 ← Start here
├── curl_examples.sh          ← Run: bash curl_examples.sh
├── javascript_examples.js    ← 25+ methods, 6 examples
└── python_examples.py        ← Run: python3 python_examples.py
```

### v2 Examples (Enhanced, Backward Compatible)
```
example-api/v2/
├── README.md                 ← Read first (v1 → v2 migration)
├── curl_examples.sh          ← Shows v2 features + v1 compat
├── javascript_examples.js    ← OTGWClientV2 with 10+ v2 methods
└── python_examples.py        ← OTGWClientV2 with parsing helpers
```

### v3 Examples (Modern REST)
```
example-api/v3/
├── README.md                 ← Getting started with v3
├── curl_examples.sh          ← 15+ curl examples
├── javascript_examples.js    ← 50+ methods, 8+ examples
├── python_examples.py        ← 30+ methods, 8+ examples
├── use_cases.md              ← 8+ real-world scenarios
└── API_REFERENCE.md          ← Complete API documentation
```

## Quick Start (30 seconds)

### Get Device Health
```bash
curl http://192.168.1.100/api/v1/health | jq .
```

### Get Room Temperature
```bash
curl http://192.168.1.100/api/v1/otgw/id/16 | jq '.value'
```

### Set Room Temperature to 22°C
```bash
curl -X POST http://192.168.1.100/api/v1/otgw/command/TT=22.0
```

## Which Version Should I Use?

| Need | Version | Why |
|------|---------|-----|
| Simple monitoring | **v1** | Proven, stable, zero dependencies |
| Better data structures | **v2** | Enhanced parsing, 100% compatible |
| Modern REST features | **v3** | Caching, pagination, filtering, HATEOAS |
| Existing v1 code | **v1 or v2** | Both work, v2 optional upgrade |

## Run Examples by Language

### cURL (Bash)
```bash
# v1
cd example-api/v1
bash curl_examples.sh

# v2
cd example-api/v2
bash curl_examples.sh

# v3
cd example-api/v3
bash curl_examples.sh
```

### JavaScript
```javascript
// v1
const { OTGWClientV1 } = require('./v1/javascript_examples.js');
const client = new OTGWClientV1('192.168.1.100');
const health = await client.getHealth();

// v2
const { OTGWClientV2 } = require('./v2/javascript_examples.js');
const client = new OTGWClientV2('192.168.1.100');
const status = await client.get_status_info();

// v3
const { OTGWClient } = require('./v3/javascript_examples.js');
const client = new OTGWClient('192.168.1.100');
const system = await client.getSystemInfo();
```

### Python
```python
# v1
from v1.python_examples import OTGWClientV1
client = OTGWClientV1('192.168.1.100')
health = client.get_health()

# v2
from v2.python_examples import OTGWClientV2
client = OTGWClientV2('192.168.1.100')
status = client.get_status_info()

# v3
from v3.python_examples import OTGWClient
client = OTGWClient('192.168.1.100')
system = client.get_system_info()
```

## Common Operations

### Check Device Status
```bash
# All versions
curl http://192.168.1.100/api/v1/health | jq '{status, otgw: .otgw_connected}'
```

### Get Current Temperatures
```bash
# v1/v2 - Individual endpoints
curl http://192.168.1.100/api/v1/otgw/id/16  # Room
curl http://192.168.1.100/api/v1/otgw/id/17  # Boiler
curl http://192.168.1.100/api/v1/otgw/id/18  # DHW
```

### Set Temperature
```bash
# All versions
curl -X POST http://192.168.1.100/api/v1/otgw/command/TT=22.0
```

### Enable/Disable Heating
```bash
# Enable
curl -X POST http://192.168.1.100/api/v1/otgw/command/CS=1

# Disable
curl -X POST http://192.168.1.100/api/v1/otgw/command/CS=0
```

### Export Data
```bash
# Telegraf/InfluxDB format
curl http://192.168.1.100/api/v1/otgw/telegraf

# OTmonitor format (v2 enhanced)
curl http://192.168.1.100/api/v2/otgw/otmonitor

# v1 OTmonitor format
curl http://192.168.1.100/api/v1/otgw/otmonitor
```

## Example Functions

### v1 - 8 Functions
1. `example_basic_status()` - Health check
2. `example_temperatures()` - Get temps
3. `example_control_heating()` - Set temperature & enable heating
4. `example_settings()` - Manage settings
5. `example_monitoring()` - Real-time monitoring
6. `example_data_formats()` - Export formats
7. `example_polling_pattern()` - Polling with error handling
8. `example_dashboard()` - ASCII monitoring dashboard

### v2 - 6 Functions
1. `example_basic_v2()` - v2 usage
2. `example_parse_otmonitor()` - Parse enhanced OTmonitor
3. `example_migration()` - Migration from v1
4. `example_backward_compatibility()` - v1 vs v2 test
5. `example_data_comparison()` - Compare data sources
6. `example_structured_data()` - Get structured status (v2 feature)

### v3 - 8+ Functions
1. `example_api_discovery()` - Auto-discover endpoints
2. `example_caching_etag()` - ETag caching
3. `example_pagination()` - Page through messages
4. `example_filtering()` - Filter messages
5. `example_all_operations()` - CRUD on all resources
6. `example_sensors()` - Read sensor data
7. `example_export()` - Export data
8. `example_monitoring_dashboard()` - Real-time monitoring

## Client Methods

### v1 Core Methods (25+)
```
get_health()                    - Device status
get_settings()                  - Device settings
get_otgw_by_id(msg_id)         - OT value by ID
get_otgw_by_label(label)       - OT value by label
send_command(command)           - Send OTGW command
get_temperatures()              - Room, boiler, DHW temps
set_room_temperature(temp)      - Set room temp
enable_heating() / disable_heating()
enable_dhw() / disable_dhw()
get_telegraf()                  - Export Telegraf format
get_otmonitor()                 - Export OTmonitor format
is_healthy()                    - Check device health
monitor_health(interval, duration)
```

### v2 Additional Methods (10+)
```
get_otmonitor_v2()              - Enhanced OTmonitor
parse_otmonitor()               - Parse to dict
get_status_info()               - Structured status
supports_v2()                   - Check v2 support
migrate_from_v1()               - Migration info
get_temperatures_from_otmonitor()
```

### v3 Additional Methods (50+)
See `v3/API_REFERENCE.md` for complete list. Includes:
- System endpoints (6)
- Config endpoints (6 with PATCH)
- OTGW endpoints (6 with pagination)
- PIC endpoints (4)
- Sensor endpoints (4)
- Export endpoints (5)
- Plus helpers for caching, pagination, filtering

## Feature Comparison

| Feature | v1 | v2 | v3 |
|---------|----|----|-----|
| **Endpoints** | 11 | 11 | 31 |
| **HTTP Methods** | GET, POST | GET, POST | GET, POST, PATCH, PUT, DELETE, OPTIONS |
| **ETag Caching** | ❌ | ❌ | ✅ |
| **Pagination** | ❌ | ❌ | ✅ |
| **Filtering** | ❌ | ❌ | ✅ |
| **Rate Limiting** | ❌ | ❌ | ✅ |
| **HATEOAS** | ❌ | ❌ | ✅ |
| **API Discovery** | ❌ | ❌ | ✅ |
| **Backward Compatible** | N/A | ✅ 100% | ✅ (v0-v2) |

## OpenTherm Commands

```
CS=1        Enable Central Heating
CS=0        Disable Central Heating
SW=1        Enable DHW
SW=0        Disable DHW
TT=22.5     Set room temperature (°C)
TR=50.0     Set DHW setpoint (°C)
```

## Error Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 202 | Accepted (async operation) |
| 204 | No Content |
| 304 | Not Modified (cached, v3 only) |
| 400 | Bad Request |
| 404 | Not Found |
| 405 | Method Not Allowed |
| 429 | Rate Limited (v3 only) |
| 500 | Server Error |

## Configuration

Set device IP before running examples:
```bash
export DEVICE_IP=192.168.1.100
```

Or edit examples to change hardcoded IP.

## Environment Setup

### Python
```bash
pip install requests
python3 example.py
```

### JavaScript/Node.js
```bash
npm install requests
node example.js
```

### cURL
```bash
bash example.sh
```

No setup required!

## Documentation Files

| File | Purpose |
|------|---------|
| `example-api/README.md` | Master index, version comparison |
| `example-api/SUMMARY.md` | Creation summary, statistics |
| `v1/README.md` | v1 getting started and reference |
| `v2/README.md` | v2 features and migration guide |
| `v3/README.md` | v3 overview and quick start |
| `v3/API_REFERENCE.md` | Complete v3 API documentation |
| `v3/use_cases.md` | Real-world scenarios |

## Migration Path

```
Existing v1 App
      ↓
Option 1: Stay on v1 (still supported)
Option 2: Migrate to v2 (1 line change, 0 compatibility issues)
Option 3: Upgrade to v3 (new endpoints, v0-v2 still work)
```

## Getting Help

1. **Start with README**: Each version has README.md
2. **Try cURL Examples**: `curl_examples.sh` in each folder
3. **Check Code Comments**: All examples well documented
4. **Review Use Cases**: `v3/use_cases.md` for scenarios
5. **Read API Reference**: `v3/API_REFERENCE.md` for complete API

## Device IP Configuration

Update these values in examples:
```python
client = OTGWClientV1('192.168.1.100')  # Change IP here
```

Or set environment variable:
```bash
export DEVICE_IP=your.device.ip
./example.sh
```

## Next Steps

1. Pick your API version (v1, v2, or v3)
2. Read the version README
3. Run cURL examples first
4. Choose JavaScript or Python for automation
5. Build your integration!

---

**Quick Links**
- [v1 Examples](v1/) - Stable production-ready
- [v2 Examples](v2/) - Enhanced, backward compatible
- [v3 Examples](v3/) - Modern REST with HATEOAS
- [Master README](README.md) - Overview and comparison
- [API v3 Reference](v3/API_REFERENCE.md) - Complete API docs

**Status**: All examples ready to use ✅
