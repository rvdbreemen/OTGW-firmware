# OTGW REST API v1 - Examples and Documentation

Version: 1.0.0  
API Level: v1 (Stable, Production-Ready)  
Status: Active and Maintained

## Overview

The REST API v1 provides simple, straightforward HTTP endpoints for interacting with the OTGW device. It's production-ready, stable, and recommended for most use cases.

## Features

- ✅ Simple HTTP endpoints
- ✅ JSON request/response format
- ✅ GET/POST methods for all operations
- ✅ No authentication required (for local network use)
- ✅ Real-time OpenTherm data access
- ✅ Device configuration management
- ✅ MQTT auto-discovery support
- ✅ Multiple export formats (Telegraf, OTmonitor)

## Quick Start

### 1. Check Device Health

```bash
curl http://192.168.1.100/api/v1/health | jq .
```

Response:
```json
{
  "status": "UP",
  "uptime": 86400,
  "otgw_connected": true,
  "picavailable": true,
  "mqtt_connected": true,
  "wifi_rssi": -45,
  "heap_free": 25600,
  "heap_max": 40960
}
```

### 2. Get Current Room Temperature

```bash
curl http://192.168.1.100/api/v1/otgw/id/16 | jq '.value'
```

Response: `21.5` (°C)

### 3. Set Room Temperature

```bash
curl -X POST http://192.168.1.100/api/v1/otgw/command/TT=22.0
```

### 4. Enable Central Heating

```bash
curl -X POST http://192.168.1.100/api/v1/otgw/command/CS=1
```

## Endpoints

### Health & Status

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/health` | Check device health and status |
| GET | `/api/v1/flashstatus` | Check ESP flash status |
| GET | `/api/v1/pic/flashstatus` | Check PIC flash status (polling) |

### Configuration

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/settings` | Get all device settings |
| POST | `/api/v1/settings` | Update device settings |

### Data Export

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/otgw/telegraf` | Export in Telegraf/InfluxDB format |
| GET | `/api/v1/otgw/otmonitor` | Export in OTmonitor format |

### OpenTherm Data Access

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/otgw/id/{msgid}` | Get OT message by numeric ID |
| GET | `/api/v1/otgw/label/{label}` | Get OT message by label |
| POST | `/api/v1/otgw/command/{command}` | Send OTGW command |
| POST | `/api/v1/otgw/autoconfigure` | Trigger MQTT auto-discovery |

## Common OpenTherm Commands

```
CS=1        Enable Central Heating
CS=0        Disable Central Heating
SW=1        Enable DHW (Domestic Hot Water)
SW=0        Disable DHW
TT=22.0     Set room temperature (temporary override)
TR=50.0     Set DHW setpoint
```

## Example Files

### cURL Examples (`curl_examples.sh`)

Comprehensive bash script with 13+ example sections:
- Health checks
- Temperature queries
- Settings management
- OTGW commands
- Error handling
- Polling patterns
- Performance tips

**Usage:**
```bash
bash curl_examples.sh
# or
export DEVICE_IP=192.168.1.100
bash curl_examples.sh
```

### JavaScript Examples (`javascript_examples.js`)

Full `OTGWClientV1` class with Node.js and browser support:
- 6 example functions
- Async/await patterns
- Error handling
- Monitoring dashboard
- Health checks
- Temperature management

**Usage (Node.js):**
```javascript
const { OTGWClientV1 } = require('./javascript_examples.js');
const client = new OTGWClientV1('192.168.1.100');

// Get health
const health = await client.getHealth();
console.log(health);

// Set temperature
await client.setRoomTemperature(22.0);
```

**Usage (Browser):**
```html
<script src="javascript_examples.js"></script>
<script>
  const client = new OTGWClientV1('192.168.1.100');
  client.getHealth().then(health => {
    console.log(health);
  });
</script>
```

### Python Examples (`python_examples.py`)

Full `OTGWClientV1` class with 8 example functions:
- Basic operations
- Temperature control
- Settings management
- Data monitoring
- Health polling
- Monitoring dashboard
- Export formats

**Usage:**
```python
from python_examples import OTGWClientV1

client = OTGWClientV1('192.168.1.100')

# Get health
health = client.get_health()
print(f"Status: {health['status']}")

# Get temperatures
temps = client.get_temperatures()
print(f"Room: {temps['room']}°C")

# Set temperature
client.set_room_temperature(22.0)

# Enable heating
client.enable_heating()
```

**Run examples:**
```bash
python3 python_examples.py
```

## Typical Use Cases

### 1. Monitor Device Health

```bash
while true; do
  curl -s http://192.168.1.100/api/v1/health | jq \
    '{status: .status, otgw: .otgw_connected, heap: .heap_free}'
  sleep 30
done
```

### 2. Home Assistant Integration

```yaml
sensor:
  - platform: rest
    resource: http://192.168.1.100/api/v1/otgw/id/16
    name: Room Temperature
    unit_of_measurement: °C
    value_template: '{{ value_json.value }}'
    scan_interval: 60
```

### 3. Export to InfluxDB

```bash
curl -s http://192.168.1.100/api/v1/otgw/telegraf | \
  curl -X POST http://influxdb:8086/write?db=otgw --data-binary @-
```

### 4. Automated Control

```python
client = OTGWClientV1('192.168.1.100')

# Set temperature based on time of day
if datetime.now().hour < 6:
    client.set_room_temperature(16.0)  # Night
else:
    client.set_room_temperature(21.0)  # Day
```

## OpenTherm Message IDs

Common message IDs for reference:

| ID | Label | Unit |
|----|-------|------|
| 0 | Status | - |
| 1 | Control Setpoint | °C |
| 16 | Room Setpoint | °C |
| 17 | Boiler Temperature | °C |
| 18 | DHW Setpoint | °C |
| 19 | DHW Temperature | °C |
| 24 | Modulation | % |
| 25 | Boiler Capacity | % |

For complete list, see [OpenTherm Protocol Specification](../../Specification/)

## Response Formats

### JSON Response Example

```json
{
  "id": 16,
  "label": "Room Setpoint",
  "value": 21.5,
  "unit": "°C",
  "type": "float"
}
```

### OTmonitor Format Example

```
Status:                       84 (CH enabled, DHW enabled, Flame on)
Control Setpoint:            21.5 °C
Boiler Setpoint:             60.0 °C
Boiler Actual:               54.3 °C
DHW Setpoint:                50.0 °C
DHW Actual:                  45.2 °C
Modulation:                  80.5 %
```

### Telegraf Format Example

```
otgw_heating,host=gateway room_temp=21.5,setpoint=22.0,boiler_temp=55.3,modulation=80.5 1609459200000000000
otgw_dhw,host=gateway dhw_temp=45.2,dhw_setpoint=50.0,dhw_active=1i 1609459200000000000
```

## Error Handling

### Common Error Responses

| Status | Meaning | Example |
|--------|---------|---------|
| 200 | Success | Normal operation |
| 400 | Bad Request | Invalid message ID or parameter |
| 404 | Not Found | Endpoint doesn't exist |
| 405 | Method Not Allowed | Using wrong HTTP method |
| 500 | Server Error | Device internal error |

### Example Error Response

```bash
$ curl -s http://192.168.1.100/api/v1/otgw/id/invalid
400: invalid msgid
```

## Performance Characteristics

- **Response Time**: < 100ms typically
- **Payload Size**: 100-500 bytes per response
- **Rate Limit**: ~10 requests/second per client
- **Concurrent Requests**: 4 simultaneous supported

## Known Limitations

- No HTTPS/TLS support (local network only)
- No authentication (device should be on trusted network)
- No request rate limiting
- Static buffer sizes (≤4KB responses)
- No pagination support

## Migration to v2

The v2 API provides enhanced OTmonitor endpoint while maintaining full v1 compatibility:

```bash
# Current (v1)
curl http://192.168.1.100/api/v1/otgw/otmonitor

# Future (v2 - improved format)
curl http://192.168.1.100/api/v2/otgw/otmonitor
```

**Note**: v1 endpoints continue to work indefinitely. Migration is optional and non-breaking.

## Next Steps

1. **Start with cURL**: Try `curl_examples.sh` to understand endpoints
2. **Choose Your Language**: Pick JavaScript or Python for automation
3. **Build Your Integration**: Use examples as templates
4. **Monitor in Production**: Use health endpoints for status tracking

## Support & Documentation

- [API Reference](../v3/API_REFERENCE.md)
- [REST API v3 (Modern)](../v3/)
- [OTGW GitHub](https://github.com/rvdbreemen/OTGW-firmware)
- [OpenTherm Specification](../../Specification/)

## License

These examples are provided as-is. See [LICENSE](../../LICENSE) for details.
