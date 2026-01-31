# Quick Start Guide

Get the REST API v3 up and running in 5 minutes!

## Prerequisites

- OTGW device on your network
- Device IP address (find via WiFi router or mDNS: `otgw-[serial].local`)
- Basic familiarity with REST APIs

## Step 1: Find Your Device (30 seconds)

### Option A: Use mDNS (Recommended)
```bash
# Linux/Mac
ping otgw-12345.local

# Windows PowerShell
Resolve-DnsName otgw-12345.local
```

Replace `12345` with your device serial number (visible on device or in router).

### Option B: Manual IP Discovery
Check your WiFi router's connected devices list for an entry named `OTGW-*`.

## Step 2: Verify Connection (1 minute)

```bash
# Test with device health check
curl http://otgw-12345.local/api/v3/system/health

# Expected response:
{
  "status": "UP",
  "picavailable": true,
  "uptime": 3600,
  "freeheap": 45000,
  "rssi": -45,
  "temperature": 42.5
}
```

If successful, your device is ready! ✅

## Step 3: Get Device Info (1 minute)

```bash
# Get full device information with HATEOAS links
curl http://otgw-12345.local/api/v3/system/info | jq .

# Key information:
# - hostname: Device name
# - version: Firmware version
# - compiledate: Build date
# - buildnumber: Build ID
```

## Step 4: Check Boiler Status (1 minute)

```bash
# Get current heating system status
curl http://otgw-12345.local/api/v3/otgw/status | jq .

# Key values:
# - controlsetpoint: Target temperature
# - outsidetemperature: External temp
# - dhwsetpoint: Hot water temperature
# - roomtemperature: Indoor temp
```

## Step 5: Send Your First Command (1 minute)

```bash
# Get current DHW setpoint
curl http://otgw-12345.local/api/v3/system/info | jq .

# Change boiler control setpoint to 65°C
curl -X POST http://otgw-12345.local/api/v3/otgw/command \
  -H "Content-Type: application/json" \
  -d '{"command": "TT", "value": 65}'

# Expected response:
{
  "command": "TT",
  "value": 65,
  "status": "queued"
}
```

## Common Operations

### Check System Health
```bash
curl http://otgw-12345.local/api/v3/system/health
```

### Get OpenTherm Messages (Latest 50)
```bash
curl http://otgw-12345.local/api/v3/otgw/messages?limit=50
```

### Filter Messages by Category
```bash
# Status messages only
curl "http://otgw-12345.local/api/v3/otgw/messages?category=Status"

# Temperature messages only
curl "http://otgw-12345.local/api/v3/otgw/messages?category=Temperature"
```

### Get Current Configuration
```bash
# Device config
curl http://otgw-12345.local/api/v3/config/device | jq .

# MQTT config
curl http://otgw-12345.local/api/v3/config/mqtt | jq .
```

### Explore Available Endpoints (HATEOAS)
```bash
# Get full API map with all available links
curl http://otgw-12345.local/api/v3 | jq .

# This returns the complete HATEOAS graph showing all available operations
```

## Using with Programming Languages

### JavaScript/Node.js
```javascript
const apiUrl = 'http://otgw-12345.local/api/v3';

// Get system health
fetch(`${apiUrl}/system/health`)
  .then(r => r.json())
  .then(data => console.log('Status:', data.status));

// Send command
fetch(`${apiUrl}/otgw/command`, {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ command: 'TT', value: 65 })
})
.then(r => r.json())
.then(data => console.log('Command queued:', data));
```

### Python
```python
import requests

api_url = 'http://otgw-12345.local/api/v3'

# Get system health
health = requests.get(f'{api_url}/system/health').json()
print(f"Status: {health['status']}")

# Send command
response = requests.post(
    f'{api_url}/otgw/command',
    json={'command': 'TT', 'value': 65}
)
print(f"Command queued: {response.json()}")
```

### cURL (Bash)
See [example-api/README.md](../../example-api/README.md) for comprehensive bash examples.

## API Discovery with HATEOAS

One of v3's best features is HATEOAS - the API tells you what you can do:

```bash
# Get API root with all available operations
curl http://otgw-12345.local/api/v3 | jq '.links'

# Result includes links like:
# - /system/info
# - /system/health
# - /otgw/status
# - /otgw/messages
# - /config/device
# - ... and 26 more endpoints
```

## Pagination

Working with large message collections? Use pagination:

```bash
# Get first 100 messages
curl "http://otgw-12345.local/api/v3/otgw/messages?limit=100&offset=0"

# Get next page
curl "http://otgw-12345.local/api/v3/otgw/messages?limit=100&offset=100"

# Response includes:
# - data: array of messages
# - pagination: { total, limit, offset, hasNext }
```

## Caching with ETags

v3 supports HTTP caching to reduce bandwidth:

```bash
# First request
curl -i http://otgw-12345.local/api/v3/system/info

# Response includes ETag header:
# ETag: "abc123def456"

# Next request with ETag:
curl -i -H 'If-None-Match: "abc123def456"' \
  http://otgw-12345.local/api/v3/system/info

# If unchanged: 304 Not Modified (no body sent)
# If changed: 200 OK with new data and new ETag
```

## Troubleshooting

### Device Not Found
1. Check device is powered on
2. Check device is on same WiFi network
3. Check device hostname: `otgw-12345.local` (check serial number)
4. Verify IP address with: `ping otgw-12345.local`

### Connection Refused
- Device may be rebooting (wait 30 seconds)
- Check firewall isn't blocking port 80
- Verify device is on network: `ping otgw-12345.local`

### API Returns 500 Error
- Check device logs: Browse to `http://otgw-12345.local/`
- Device may need reboot
- Check available disk space (LittleFS)

### Commands Not Executing
- Check OTGW PIC is available: `curl http://otgw-12345.local/api/v3/system/health`
- Command queue may be full (try again in 5 seconds)
- Check command syntax (e.g., "TT" not "tt")

## Next Steps

1. **Read Full API Reference** → [Complete API Reference](Complete-API-Reference)
2. **See Code Examples** → [example-api folder](../../example-api/)
3. **Handle Errors** → [Error Handling Guide](Error-Handling-Guide)
4. **Upgrade from v1/v2** → [Migration Guide](Migration-Guide)

## API Response Times

- Simple GET: ~40-50ms
- Complex JSON response: ~100-200ms
- POST/PATCH commands: ~80-100ms
- OpenTherm messages (filtered): ~150-200ms

## Rate Limiting

- Default: 100 requests per minute per IP
- Exceeding limit returns: `HTTP 429 Too Many Requests`
- Reset after 60 seconds

## Need Help?

- 📖 **Full Examples**: [example-api folder](../../example-api/)
- 🐛 **Report Issues**: [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)
- 💬 **Ask Questions**: [GitHub Discussions](https://github.com/rvdbreemen/OTGW-firmware/discussions)
- 📚 **Full API Docs**: [Complete API Reference](Complete-API-Reference)

---

**Congratulations!** You've completed the Quick Start Guide. You're ready to use the REST API v3! 🎉
