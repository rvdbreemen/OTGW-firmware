# REST API v3 Response Headers

**Version:** 3.0.0  
**Status:** Design Specification  
**Last Updated:** 2026-01-31

---

## Overview

All v3 API responses include standard HTTP headers plus custom headers for OTGW-specific information.

---

## Standard Headers

### Content-Type

**Always Present:** Yes  
**Value:** `application/json; charset=utf-8`

```
Content-Type: application/json; charset=utf-8
```

**Usage:** Indicates JSON response with UTF-8 encoding

---

### Access-Control-Allow-Origin

**Always Present:** Yes  
**Value:** `*`

```
Access-Control-Allow-Origin: *
```

**Usage:** CORS support - allows requests from any origin (local network use)

---

### Access-Control-Allow-Methods

**Present On:** OPTIONS requests, error responses

```
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD
```

**Usage:** Lists all supported HTTP methods for CORS preflight

---

### Access-Control-Allow-Headers

**Present On:** OPTIONS requests

```
Access-Control-Allow-Headers: Content-Type, If-None-Match, If-Match
```

**Usage:** Lists headers clients can send in actual request

---

### Access-Control-Max-Age

**Present On:** OPTIONS requests

```
Access-Control-Max-Age: 86400
```

**Usage:** How long (seconds) to cache preflight response (24 hours)

---

### Allow

**Present On:** 405 Method Not Allowed responses

```
Allow: GET, PATCH
```

**Usage:** Lists HTTP methods supported for this resource

**Example:**
```
DELETE /api/v3/system/info

→ 405 Method Not Allowed
Allow: GET
```

---

## Caching Headers

### ETag

**Present On:** Cacheable GET responses

```
ETag: "abc123"
```

**Format:** Quoted hexadecimal string (CRC32 or similar hash)

**Usage:** Resource version for conditional requests

**Example:**
```
GET /api/v3/system/info

→ 200 OK
ETag: "a1b2c3d4"
{
  "info": { ... }
}

# Subsequent request
GET /api/v3/system/info
If-None-Match: "a1b2c3d4"

→ 304 Not Modified
ETag: "a1b2c3d4"
(no body)
```

---

### Cache-Control

**Present On:** All responses

**Values:**
- `no-cache` - Dynamic data, validate before use
- `max-age=3600` - Static data, cache for 1 hour
- `no-store` - Sensitive data, never cache

**Examples:**

```
# Dynamic data (status, measurements)
Cache-Control: no-cache

# Semi-static data (device info)
Cache-Control: max-age=3600

# Sensitive data (configuration with passwords)
Cache-Control: no-store, no-cache, must-revalidate
```

**Per Endpoint:**

| Endpoint | Cache-Control | Reason |
|----------|---------------|--------|
| `/system/info` | `max-age=3600` | Rarely changes |
| `/system/health` | `no-cache` | Real-time status |
| `/otgw/status` | `no-cache` | Real-time data |
| `/otgw/messages` | `no-cache` | Real-time data |
| `/config/*` | `no-store` | Sensitive data |
| `/sensors/*` | `no-cache` | Real-time data |

---

### Last-Modified

**Present On:** Resources with known modification time

```
Last-Modified: Fri, 31 Jan 2026 10:30:00 GMT
```

**Usage:** Alternative to ETag for older clients

---

## Rate Limiting Headers

### X-RateLimit-Limit

**Present On:** All responses (when rate limiting enabled)

```
X-RateLimit-Limit: 60
```

**Usage:** Maximum requests allowed per time window

---

### X-RateLimit-Remaining

**Present On:** All responses (when rate limiting enabled)

```
X-RateLimit-Remaining: 42
```

**Usage:** Requests remaining in current window

---

### X-RateLimit-Reset

**Present On:** All responses (when rate limiting enabled)

```
X-RateLimit-Reset: 1738324275
```

**Format:** Unix timestamp  
**Usage:** When rate limit window resets

---

### Retry-After

**Present On:** 429 Too Many Requests, 503 Service Unavailable

```
Retry-After: 30
```

**Format:** Seconds to wait before retry  
**Usage:** Tells client when to retry request

**Example:**
```
→ 429 Too Many Requests
Retry-After: 30
X-RateLimit-Reset: 1738324275
```

---

## Custom OTGW Headers

### X-OTGW-Version

**Always Present:** Yes

```
X-OTGW-Version: 1.0.0
```

**Usage:** Firmware version for debugging and compatibility

---

### X-OTGW-Heap-Free

**Always Present:** Yes

```
X-OTGW-Heap-Free: 12345
```

**Format:** Bytes  
**Usage:** Current free heap for monitoring health

---

### X-OTGW-API-Version

**Always Present:** Yes (on v3 endpoints)

```
X-OTGW-API-Version: 3.0
```

**Usage:** API version for client compatibility checks

---

## Location Header

### Location

**Present On:** 201 Created responses

```
Location: /api/v3/otgw/commands/12345
```

**Usage:** URI of newly created resource

**Example:**
```
POST /api/v3/otgw/commands

→ 201 Created
Location: /api/v3/otgw/commands/12345
{
  "command": {
    "id": "12345",
    "_links": {
      "self": {"href": "/api/v3/otgw/commands/12345"}
    }
  }
}
```

---

## Complete Response Examples

### Example 1: Successful GET with ETag

```
GET /api/v3/system/info HTTP/1.1
Host: otgw.local

HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Access-Control-Allow-Origin: *
Cache-Control: max-age=3600
ETag: "a1b2c3d4"
X-OTGW-Version: 1.0.0
X-OTGW-Heap-Free: 12345
X-OTGW-API-Version: 3.0
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 59
X-RateLimit-Reset: 1738324275
Content-Length: 456

{
  "info": { ... }
}
```

---

### Example 2: Conditional Request (Not Modified)

```
GET /api/v3/system/info HTTP/1.1
Host: otgw.local
If-None-Match: "a1b2c3d4"

HTTP/1.1 304 Not Modified
Access-Control-Allow-Origin: *
Cache-Control: max-age=3600
ETag: "a1b2c3d4"
X-OTGW-Version: 1.0.0
X-OTGW-Heap-Free: 12345
X-OTGW-API-Version: 3.0
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 58
X-RateLimit-Reset: 1738324275
Content-Length: 0

(no body)
```

---

### Example 3: Rate Limited

```
GET /api/v3/otgw/messages HTTP/1.1
Host: otgw.local

HTTP/1.1 429 Too Many Requests
Content-Type: application/json; charset=utf-8
Access-Control-Allow-Origin: *
Retry-After: 30
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1738324275
X-OTGW-Version: 1.0.0
X-OTGW-Heap-Free: 12345
X-OTGW-API-Version: 3.0
Content-Length: 234

{
  "error": {
    "code": "RATE_LIMIT_EXCEEDED",
    "message": "Rate limit exceeded",
    "details": {
      "retry_after": 30
    }
  }
}
```

---

### Example 4: OPTIONS (CORS Preflight)

```
OPTIONS /api/v3/config/mqtt HTTP/1.1
Host: otgw.local
Origin: http://192.168.1.50
Access-Control-Request-Method: PATCH
Access-Control-Request-Headers: Content-Type

HTTP/1.1 204 No Content
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS, HEAD
Access-Control-Allow-Headers: Content-Type, If-None-Match, If-Match
Access-Control-Max-Age: 86400
X-OTGW-Version: 1.0.0
X-OTGW-Heap-Free: 12345
X-OTGW-API-Version: 3.0
Content-Length: 0

(no body)
```

---

### Example 5: Created Resource

```
POST /api/v3/otgw/commands HTTP/1.1
Host: otgw.local
Content-Type: application/json

{
  "command": "TT=20.5"
}

HTTP/1.1 201 Created
Content-Type: application/json; charset=utf-8
Access-Control-Allow-Origin: *
Location: /api/v3/otgw/commands/12345
X-OTGW-Version: 1.0.0
X-OTGW-Heap-Free: 11234
X-OTGW-API-Version: 3.0
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 57
X-RateLimit-Reset: 1738324275
Content-Length: 198

{
  "command": {
    "id": "12345",
    "command": "TT=20.5",
    "status": "queued"
  }
}
```

---

### Example 6: Low Heap Error

```
GET /api/v3/otgw/messages?per_page=128 HTTP/1.1
Host: otgw.local

HTTP/1.1 503 Service Unavailable
Content-Type: application/json; charset=utf-8
Access-Control-Allow-Origin: *
Retry-After: 5
X-OTGW-Version: 1.0.0
X-OTGW-Heap-Free: 3200
X-OTGW-API-Version: 3.0
Content-Length: 245

{
  "error": {
    "code": "SYSTEM_LOW_HEAP",
    "message": "Insufficient memory",
    "details": {
      "free_heap": 3200,
      "minimum_required": 4096
    }
  }
}
```

---

## Header Implementation Checklist

### All Responses

- [ ] Content-Type: application/json; charset=utf-8
- [ ] Access-Control-Allow-Origin: *
- [ ] X-OTGW-Version: {firmware_version}
- [ ] X-OTGW-Heap-Free: {bytes}
- [ ] X-OTGW-API-Version: 3.0

### GET Responses (Success)

- [ ] Cache-Control: (no-cache | max-age=N)
- [ ] ETag: "{hash}" (if cacheable)
- [ ] X-RateLimit-* (if enabled)

### 201 Created

- [ ] Location: /path/to/resource

### 304 Not Modified

- [ ] ETag: "{hash}"
- [ ] Cache-Control: max-age=N

### 405 Method Not Allowed

- [ ] Allow: GET, POST, ...

### 429 Too Many Requests

- [ ] Retry-After: {seconds}
- [ ] X-RateLimit-Limit: {max}
- [ ] X-RateLimit-Remaining: 0
- [ ] X-RateLimit-Reset: {timestamp}

### 503 Service Unavailable

- [ ] Retry-After: {seconds} (optional)

### OPTIONS Responses

- [ ] Access-Control-Allow-Methods: ...
- [ ] Access-Control-Allow-Headers: ...
- [ ] Access-Control-Max-Age: 86400

---

## Client Usage Examples

### JavaScript (Fetch API)

```javascript
// Check rate limit before making request
const response = await fetch('/api/v3/system/info');
const rateLimit = {
  limit: response.headers.get('X-RateLimit-Limit'),
  remaining: response.headers.get('X-RateLimit-Remaining'),
  reset: response.headers.get('X-RateLimit-Reset')
};
console.log(`Rate limit: ${rateLimit.remaining}/${rateLimit.limit}`);

// Use ETag for caching
const etag = response.headers.get('ETag');
localStorage.setItem('system-info-etag', etag);

// Next request with ETag
const nextResponse = await fetch('/api/v3/system/info', {
  headers: {
    'If-None-Match': localStorage.getItem('system-info-etag')
  }
});

if (nextResponse.status === 304) {
  console.log('Using cached data');
}
```

---

### Python

```python
import requests

# Make request
response = requests.get('http://otgw.local/api/v3/system/info')

# Check headers
print(f"Firmware: {response.headers['X-OTGW-Version']}")
print(f"Free Heap: {response.headers['X-OTGW-Heap-Free']} bytes")

# Handle rate limiting
if response.status_code == 429:
    retry_after = int(response.headers['Retry-After'])
    print(f"Rate limited, retry after {retry_after} seconds")
    time.sleep(retry_after)
```

---

### cURL

```bash
# View all headers
curl -i http://otgw.local/api/v3/system/info

# Conditional request
curl -i -H "If-None-Match: \"a1b2c3d4\"" http://otgw.local/api/v3/system/info

# CORS preflight
curl -i -X OPTIONS http://otgw.local/api/v3/config/mqtt
```

---

## See Also

- [ADR-025: REST API v3 Design](../adr/ADR-025-rest-api-v3-design.md)
- [Resource Model](RESOURCE_MODEL.md)
- [Error Responses](ERROR_RESPONSES.md)
- [HTTP Status Codes](HTTP_STATUS_CODES.md)
