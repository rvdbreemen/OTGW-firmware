# HATEOAS Navigation Guide

Understanding and using HATEOAS (Hypermedia As The Engine Of Application State) in REST API v3.

## What is HATEOAS?

HATEOAS is a core constraint of REST that enables **API discovery** without hardcoded endpoint URLs.

### The Problem Without HATEOAS

```javascript
// Without HATEOAS: Hardcoded URLs everywhere
const systemHealth = fetch('http://device/api/v3/system/health');
const otgwStatus = fetch('http://device/api/v3/otgw/status');
const config = fetch('http://device/api/v3/config/device');
const messages = fetch('http://device/api/v3/otgw/messages');

// If API changes URLs, code breaks
```

### The Solution With HATEOAS

```javascript
// With HATEOAS: Discover endpoints from API response
const response = fetch('http://device/api/v3');
const apiRoot = await response.json();

// Get any endpoint from links
const healthLink = apiRoot.links.find(l => l.rel === 'system-health').href;
const statusLink = apiRoot.links.find(l => l.rel === 'otgw-status').href;
const configLink = apiRoot.links.find(l => l.rel === 'config-device').href;

// If API changes URLs, code still works!
```

## HATEOAS Concepts

### Links

Each response contains a `links` array describing available operations:

```json
{
  "status": "UP",
  "links": [
    {
      "rel": "self",
      "href": "/api/v3/system/health",
      "method": "GET"
    },
    {
      "rel": "system-info",
      "href": "/api/v3/system/info",
      "method": "GET"
    }
  ]
}
```

### Link Components

| Field | Description |
|-------|-------------|
| `rel` | Relationship type (e.g., "self", "system-info") |
| `href` | Full URL or path to endpoint |
| `method` | HTTP method to use (GET, POST, PATCH, etc.) |
| `type` | Optional: response type (application/json) |
| `title` | Optional: human-readable description |

### Common Relationship Types

| Rel | Description |
|-----|-------------|
| `self` | Current resource |
| `first`, `previous`, `next`, `last` | Pagination links |
| `system-*` | System endpoints |
| `config-*` | Configuration endpoints |
| `otgw-*` | OpenTherm endpoints |
| `sensor-*` | Sensor endpoints |
| `pic-*` | PIC firmware endpoints |

## API Root Discovery

Start by getting the API root - it contains links to everything:

```bash
curl http://device/api/v3 | jq .
```

Response:
```json
{
  "version": "3.0",
  "timestamp": "2026-01-31T10:30:00Z",
  "links": [
    {
      "rel": "self",
      "href": "/api/v3",
      "method": "GET"
    },
    {
      "rel": "system-info",
      "href": "/api/v3/system/info",
      "method": "GET"
    },
    {
      "rel": "system-health",
      "href": "/api/v3/system/health",
      "method": "GET"
    },
    {
      "rel": "otgw-status",
      "href": "/api/v3/otgw/status",
      "method": "GET"
    },
    // ... 28 more links ...
  ]
}
```

## Building HATEOAS-Aware Clients

### JavaScript Client

```javascript
class OTGWClient {
  constructor(deviceUrl) {
    this.deviceUrl = deviceUrl;
    this.links = {};
  }

  async discover() {
    // Get all available endpoints
    const response = await fetch(`${this.deviceUrl}/api/v3`);
    const root = await response.json();
    
    // Index links by relationship
    root.links.forEach(link => {
      this.links[link.rel] = link;
    });
    
    console.log(`Discovered ${Object.keys(this.links).length} endpoints`);
  }

  async request(rel, options = {}) {
    const link = this.links[rel];
    if (!link) {
      throw new Error(`Unknown endpoint: ${rel}`);
    }

    const url = new URL(link.href, this.deviceUrl);
    
    // Add query parameters
    Object.entries(options.params || {}).forEach(([key, value]) => {
      url.searchParams.set(key, value);
    });

    const response = await fetch(url.toString(), {
      method: link.method,
      headers: options.headers || { 'Content-Type': 'application/json' },
      body: options.body ? JSON.stringify(options.body) : undefined
    });

    if (!response.ok) {
      const error = await response.json();
      throw new Error(`${error.error.code}: ${error.error.message}`);
    }

    return response.json();
  }

  // High-level convenience methods
  async getHealth() {
    return this.request('system-health');
  }

  async getInfo() {
    return this.request('system-info');
  }

  async getStatus() {
    return this.request('otgw-status');
  }

  async getMessages(limit = 50, offset = 0) {
    return this.request('otgw-messages', {
      params: { limit, offset }
    });
  }

  async sendCommand(command, value) {
    return this.request('otgw-command', {
      method: 'POST',
      body: { command, value }
    });
  }
}

// Usage
const client = new OTGWClient('http://device');
await client.discover();

const health = await client.getHealth();
const status = await client.getStatus();
const messages = await client.getMessages(100);
```

### Python Client

```python
import requests
from typing import Dict, List, Any

class OTGWClient:
    def __init__(self, device_url: str):
        self.device_url = device_url.rstrip('/')
        self.links: Dict[str, Dict] = {}
    
    def discover(self) -> None:
        """Discover all available endpoints"""
        response = requests.get(f'{self.device_url}/api/v3')
        root = response.json()
        
        # Index links by relationship
        for link in root.get('links', []):
            self.links[link['rel']] = link
        
        print(f"Discovered {len(self.links)} endpoints")
    
    def request(self, rel: str, params: Dict = None, json: Dict = None) -> Dict:
        """Make request using HATEOAS link"""
        if rel not in self.links:
            raise ValueError(f"Unknown endpoint: {rel}")
        
        link = self.links[rel]
        url = f"{self.device_url}{link['href']}"
        method = link.get('method', 'GET')
        
        response = requests.request(
            method=method,
            url=url,
            params=params,
            json=json
        )
        
        if response.status_code >= 400:
            error = response.json()
            raise RuntimeError(f"{error['error']['code']}: {error['error']['message']}")
        
        return response.json()
    
    # Convenience methods
    def get_health(self) -> Dict:
        return self.request('system-health')
    
    def get_info(self) -> Dict:
        return self.request('system-info')
    
    def get_status(self) -> Dict:
        return self.request('otgw-status')
    
    def get_messages(self, limit: int = 50, offset: int = 0) -> Dict:
        return self.request('otgw-messages', params={'limit': limit, 'offset': offset})
    
    def send_command(self, command: str, value: float) -> Dict:
        return self.request('otgw-command', json={'command': command, 'value': value})

# Usage
client = OTGWClient('http://device')
client.discover()

health = client.get_health()
status = client.get_status()
messages = client.get_messages(limit=100)
```

## Pagination with HATEOAS

Responses with paginated data include navigation links:

```bash
curl "http://device/api/v3/otgw/messages?limit=50" | jq '.links'
```

Response:
```json
{
  "data": [...50 messages...],
  "pagination": {
    "total": 1000,
    "limit": 50,
    "offset": 0,
    "hasNext": true
  },
  "links": [
    {
      "rel": "self",
      "href": "/api/v3/otgw/messages?limit=50&offset=0",
      "method": "GET"
    },
    {
      "rel": "first",
      "href": "/api/v3/otgw/messages?limit=50&offset=0",
      "method": "GET"
    },
    {
      "rel": "next",
      "href": "/api/v3/otgw/messages?limit=50&offset=50",
      "method": "GET"
    },
    {
      "rel": "last",
      "href": "/api/v3/otgw/messages?limit=50&offset=950",
      "method": "GET"
    }
  ]
}
```

### Navigate Pages

```javascript
// Start at first page
let response = await client.request('otgw-messages');

// Follow "next" link to get next page
while (response.links.some(l => l.rel === 'next')) {
  const nextLink = response.links.find(l => l.rel === 'next');
  response = await fetch(nextLink.href).then(r => r.json());
  console.log(`Got page with ${response.data.length} items`);
}
```

## Configuration Updates with HATEOAS

```bash
# 1. Get current configuration
curl http://device/api/v3/config/device | jq .

# Response includes:
{
  "hostname": "otgw-12345",
  "links": [
    {
      "rel": "self",
      "href": "/api/v3/config/device",
      "method": "GET"
    },
    {
      "rel": "update",
      "href": "/api/v3/config/device",
      "method": "PATCH"
    }
  ]
}

# 2. Use "update" link to modify configuration
curl -X PATCH http://device/api/v3/config/device \
  -H "Content-Type: application/json" \
  -d '{"hostname": "my-otgw"}'
```

## Embedded Resources

Some responses include related data directly:

```bash
curl http://device/api/v3/otgw/status | jq .
```

Response:
```json
{
  "controlsetpoint": 65,
  "roomtemperature": 22.5,
  "links": [
    {
      "rel": "self",
      "href": "/api/v3/otgw/status"
    },
    {
      "rel": "messages",
      "href": "/api/v3/otgw/messages"
    },
    {
      "rel": "command",
      "href": "/api/v3/otgw/command",
      "method": "POST"
    }
  ]
}
```

## Advantages of HATEOAS

### 1. API Versioning

```javascript
// URLs can change, code still works
// Old: /api/v3/otgw/status
// New: /api/v3/opentherm/boiler/status
// Client doesn't care - discovers from links
```

### 2. Reduced Coupling

```javascript
// No tight coupling to URL structure
const client = new OTGWClient('http://device');
// Client doesn't hardcode any URLs
```

### 3. Self-Documenting

```javascript
// API tells you what you can do
await client.discover();
console.log(Object.keys(client.links));
// Prints all available operations
```

### 4. Better Error Messages

```bash
# If endpoint moves, you get clear error
curl http://device/api/v3/invalid/old/path
# { "error": { "code": "ENDPOINT_NOT_FOUND" } }

# Discover correct location from API root
curl http://device/api/v3
```

## Best Practices

### ✅ Do This

```javascript
// Discover endpoints from API
const client = new OTGWClient('http://device');
await client.discover();

// Use relationship names
const health = await client.request('system-health');

// Handle missing endpoints gracefully
if ('new-feature' in client.links) {
  const result = await client.request('new-feature');
}
```

### ❌ Don't Do This

```javascript
// Hardcoded URLs
const health = await fetch('http://device/api/v3/system/health');

// No discovery
const status = await fetch('http://device/api/v3/otgw/status');

// Tight coupling to URL structure
const messages = fetch(`http://device/api/v${apiVersion}/otgw/messages`);
```

## Testing HATEOAS

```bash
# 1. Discover all endpoints
curl http://device/api/v3 | jq '.links | length'

# 2. Verify all links are accessible
curl http://device/api/v3 | jq '.links[] | .href' | while read link; do
  echo "Testing $link"
  curl -s "http://device$link" > /dev/null && echo "✓ OK" || echo "✗ FAIL"
done

# 3. Check for broken links
curl http://device/api/v3 | jq '.links[] | .href' | sort | uniq -d
```

## Need Help?

- 📖 **Full Examples**: [example-api](../../example-api/)
- 📚 **Quick Start**: [Quick Start Guide](Quick-Start-Guide)
- 🐛 **Report Issues**: [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)

## Further Reading

- [REST Architectural Principles](https://restfulapi.net/hateoas/)
- [Richardson Maturity Model](https://martinfowler.com/articles/richardsonMaturityModel.html)
- [JSON API Standard](https://jsonapi.org/)

---

**HATEOAS makes APIs resilient to change.** Build flexible, future-proof clients! 🚀
