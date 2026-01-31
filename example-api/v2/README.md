# OTGW REST API v2 - Examples and Documentation

Version: 1.0.0  
API Level: v2 (Enhanced, Backward Compatible)  
Status: Recommended for New Projects

## Overview

REST API v2 is an enhanced version of v1 with improved OTmonitor endpoint and better data structure, while maintaining 100% backward compatibility with v1.

## Key Improvements

- ✅ Enhanced OTmonitor endpoint (better formatting)
- ✅ All v1 endpoints remain unchanged
- ✅ Full backward compatibility (zero breaking changes)
- ✅ Better structured response data
- ✅ Easier data parsing
- ✅ Seamless migration from v1

## When to Use v2

### Use v2 if:
- Starting a new project
- Want cleaner data structures
- Prefer enhanced OTmonitor format
- No existing code dependencies

### Keep using v1 if:
- Have existing v1 implementations
- Need no changes to current code
- Prefer older OTmonitor format
- Want guaranteed stability

## Quick Comparison

| Feature | v1 | v2 |
|---------|----|----|
| **Health Check** | ✓ | ✓ |
| **Settings Management** | ✓ | ✓ |
| **Temperature Control** | ✓ | ✓ |
| **OTmonitor Export** | ✓ | ✓ Enhanced |
| **Data Export** (Telegraf) | ✓ | ✓ |
| **Backward Compatible** | N/A | ✓ |
| **Breaking Changes** | N/A | None |

## Quick Start

### 1. Check Device Health (Same as v1)

```bash
curl http://192.168.1.100/api/v1/health | jq .
```

### 2. Get Enhanced OTmonitor (v2 Feature)

```bash
# v1 format
curl http://192.168.1.100/api/v1/otgw/otmonitor

# v2 enhanced format
curl http://192.168.1.100/api/v2/otgw/otmonitor
```

### 3. Python - Structured Data (v2 Feature)

```python
from python_examples import OTGWClientV2

client = OTGWClientV2('192.168.1.100')

# Parse OTmonitor into structured data
data = client.parse_otmonitor()
print(f"Room: {data['room_setpoint_temperature']}°C")
print(f"Boiler: {data['boiler_actual_temperature']}°C")

# Get status info (v2 convenience method)
status = client.get_status_info()
print(f"Status: {status['status']}")
```

### 4. JavaScript - Structured Data (v2 Feature)

```javascript
const { OTGWClientV2 } = require('./javascript_examples.js');
const client = new OTGWClientV2('192.168.1.100');

// Parse OTmonitor
const data = await client.parseOTmonitorData();
console.log(`Room: ${data.room_setpoint_temperature}°C`);

// Get temperatures from OTmonitor
const temps = await client.getTemperaturesFromOTmonitor();
console.log(`Boiler: ${temps.boiler}°C`);
```

## Endpoints

### v1 Endpoints (Still Supported)

All v1 endpoints continue to work in v2:

```
GET    /api/v1/health
GET    /api/v1/settings
POST   /api/v1/settings
GET    /api/v1/flashstatus
GET    /api/v1/pic/flashstatus
GET    /api/v1/otgw/telegraf
GET    /api/v1/otgw/otmonitor    (original format)
GET    /api/v1/otgw/id/{msgid}
GET    /api/v1/otgw/label/{label}
POST   /api/v1/otgw/command/{cmd}
POST   /api/v1/otgw/autoconfigure
```

### v2 Endpoints (Enhanced)

```
GET    /api/v2/otgw/otmonitor    (enhanced format)
```

All other v1 endpoints work the same through v2 prefix.

## What's New in v2

### 1. Enhanced OTmonitor Endpoint

**v1 Format:**
```
Boiler Status:      84 (CH enabled, DHW enabled, Flame on, ModMode)
Boiler Setpoint Temperature: 60.0
```

**v2 Format:**
```
Boiler Status:                   84 (CH enabled, DHW enabled, Flame on, ModMode)
Boiler Setpoint Temperature:     60.0 °C
```

Better alignment and units for easier parsing.

### 2. Structured Parsing Support

v2 clients provide helper methods to parse OTmonitor into structured data:

```python
# v2 convenience method
client = OTGWClientV2('192.168.1.100')
parsed = client.parse_otmonitor()
# Returns: {'boiler_status': '84 (CH enabled...)', 'boiler_setpoint_temperature': 60.0, ...}
```

### 3. Migration Helpers

Check if device supports v2:

```python
client = OTGWClientV2('192.168.1.100')
if client.supports_v2():
    print("Device supports v2!")
    
migration = client.migrate_from_v1()
print(f"Breaking changes: {migration['breaking_changes']}")  # Always False
```

## Migration Guide

### Step 1: Update Client Import

**Before (v1):**
```python
from python_examples_v1 import OTGWClientV1
client = OTGWClientV1('192.168.1.100')
```

**After (v2):**
```python
from python_examples import OTGWClientV2
client = OTGWClientV2('192.168.1.100')
```

### Step 2: No Code Changes Required!

All existing v1 code works unchanged:

```python
# This still works exactly the same
health = client.get_health()
temps = client.get_temperatures()
client.set_room_temperature(22.0)
```

### Step 3: Use New v2 Features (Optional)

Add enhanced OTmonitor parsing:

```python
# Old way (still works)
temps = {
    'room': client.get_otgw_by_id(16)['value'],
    'boiler': client.get_otgw_by_id(17)['value'],
}

# New way (v2 feature)
status = client.get_status_info()
temps = client.get_temperatures_from_otmonitor()
```

## Example Files

### cURL Examples (`curl_examples.sh`)

Demonstrates v2 features with backward compatibility:
- Enhanced OTmonitor endpoint
- v1 compatibility verification
- Migration patterns
- Common workflows

**Usage:**
```bash
bash curl_examples.sh
```

### JavaScript Examples (`javascript_examples.js`)

`OTGWClientV2` class with 6 examples:
- Basic v2 usage
- OTmonitor parsing
- Migration helpers
- Version compatibility
- Structured monitoring
- Backward compatibility tests

**Usage:**
```javascript
const { OTGWClientV2 } = require('./javascript_examples.js');
const client = new OTGWClientV2('192.168.1.100');

// Parse OTmonitor
const data = await client.parseOTmonitorData();

// Get structured status
const status = await client.getStatusInfo();
```

### Python Examples (`python_examples.py`)

`OTGWClientV2` class extending v1 with 6 examples:
- Basic v2 usage
- OTmonitor parsing
- Structured data extraction
- Migration guidance
- Data source comparison
- Backward compatibility verification

**Usage:**
```python
from python_examples import OTGWClientV2

client = OTGWClientV2('192.168.1.100')

# Parse OTmonitor
data = client.parse_otmonitor()
print(f"Room: {data['room_setpoint_temperature']}°C")

# Get status info
status = client.get_status_info()
print(f"Boiler: {status['boiler_temperature']}°C")
```

## Typical Use Cases

### 1. New Project with Clean Architecture

```python
from python_examples import OTGWClientV2

class HeatingController:
    def __init__(self, device_ip):
        self.client = OTGWClientV2(device_ip)
    
    def get_status(self):
        # v2 feature: structured data
        return self.client.get_status_info()
    
    def set_temperature(self, temp):
        return self.client.set_room_temperature(temp)
```

### 2. Migrate Existing v1 App

```python
# Step 1: Change import
from python_examples import OTGWClientV2  # Changed from V1

# Step 2: That's it! Everything works
client = OTGWClientV2('192.168.1.100')
health = client.get_health()  # Works as before
temps = client.get_temperatures()  # Works as before

# Step 3 (optional): Use new features
status = client.get_status_info()  # New in v2
```

### 3. Home Assistant Integration

```yaml
sensor:
  - platform: rest
    resource: http://192.168.1.100/api/v1/otgw/id/16
    name: Room Temperature
    unit_of_measurement: °C
    value_template: '{{ value_json.value }}'
```

Same as v1 - no changes needed!

### 4. Data Export with Better Parsing

```python
client = OTGWClientV2('192.168.1.100')

# Get structured data from enhanced OTmonitor
status = client.parse_otmonitor()

# Clean up and export
data = {
    'timestamp': datetime.now().isoformat(),
    'room_temperature': status.get('room_temperature'),
    'boiler_temperature': status.get('boiler_actual_temperature'),
    'modulation': status.get('modulation'),
}

# Save to JSON
with open('heating_data.json', 'w') as f:
    json.dump(data, f)
```

## Backward Compatibility Guarantee

v2 makes no breaking changes to v1:

- ✅ All v1 endpoints continue to work
- ✅ Response format unchanged (except enhanced OTmonitor)
- ✅ Same status codes and error messages
- ✅ Same parameter formats
- ✅ Zero migration effort required

## Performance

v2 has identical performance to v1:

- **Response Time**: < 100ms
- **Payload Size**: Same as v1
- **Rate Limits**: Same as v1
- **Concurrent Requests**: 4 simultaneous

## When to Upgrade from v1 to v2

| Scenario | Action |
|----------|--------|
| New projects | Use v2 from start |
| Existing v1 apps | No pressure, v1 still works |
| Want better parsing | Upgrade when convenient |
| Need v2 features | Change import, test, deploy |

## FAQs

### Q: Will upgrading break my app?
**A:** No. v2 is 100% backward compatible. All existing v1 code works unchanged.

### Q: Do I have to migrate?
**A:** No. v1 will continue to work indefinitely.

### Q: What if device only supports v1?
**A:** Use v1 endpoints. v2 client gracefully handles this with `supports_v2()` check.

### Q: Can I mix v1 and v2 endpoints?
**A:** Yes. You can use v1 endpoints with a v2 client or vice versa.

## Next Steps

1. **Try v2 Examples**: Run JavaScript or Python examples
2. **Check Compatibility**: Use `supports_v2()` helper
3. **Compare Output**: See v1 vs v2 OTmonitor format
4. **Gradually Migrate**: Add v2 features to existing code as needed

## See Also

- [REST API v1 Examples](../v1/)
- [REST API v3 (Modern, with HATEOAS)](../v3/)
- [API Reference](../v3/API_REFERENCE.md)
- [OpenTherm Specification](../../Specification/)

## Support

For issues or questions:
- Check example files for working code
- Review v1 examples for base functionality
- See v3 documentation for advanced features
- Check [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)

## License

These examples are provided as-is. See [LICENSE](../../LICENSE) for details.
