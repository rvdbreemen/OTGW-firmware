# REST API v3 HTTP Status Codes

**Version:** 3.0.0  
**Status:** Design Specification  
**Last Updated:** 2026-01-31

---

## Overview

The v3 API uses HTTP status codes according to **RFC 7231** standards. This document defines when each status code is used and what it means.

---

## Success Codes (2xx)

### 200 OK

**Usage:** Standard success response

**Applies To:**
- Successful GET requests
- Successful PATCH requests
- Successful PUT requests
- Successful DELETE requests (with response body)

**Example:**
```
GET /api/v3/system/info
→ 200 OK
{
  "info": { ... }
}
```

---

### 201 Created

**Usage:** Resource successfully created

**Applies To:**
- Successful POST requests that create new resources

**Headers:**
```
Location: /api/v3/otgw/commands/12345
```

**Example:**
```
POST /api/v3/otgw/commands
{
  "command": "TT=20.5"
}

→ 201 Created
Location: /api/v3/otgw/commands/12345
{
  "command": {
    "id": "12345",
    "command": "TT=20.5",
    "status": "queued"
  }
}
```

---

### 202 Accepted

**Usage:** Request accepted for asynchronous processing

**Applies To:**
- Long-running operations (PIC flash, autoconfigure)
- Operations that complete in background

**Example:**
```
POST /api/v3/otgw/autoconfigure

→ 202 Accepted
{
  "message": "Auto-configuration triggered",
  "status": "processing"
}
```

---

### 204 No Content

**Usage:** Success with no response body

**Applies To:**
- Successful DELETE requests
- Successful actions with no data to return

**Example:**
```
DELETE /api/v3/otgw/commands/12345
→ 204 No Content
(no response body)
```

---

## Redirection Codes (3xx)

### 304 Not Modified

**Usage:** Resource hasn't changed (ETag match)

**Applies To:**
- GET requests with If-None-Match header when resource unchanged

**Headers:**
```
ETag: "abc123"
Cache-Control: max-age=3600
```

**Example:**
```
GET /api/v3/system/info
If-None-Match: "abc123"

→ 304 Not Modified
(no response body)
```

---

## Client Error Codes (4xx)

### 400 Bad Request

**Usage:** Malformed request, invalid syntax

**Applies To:**
- Invalid JSON in request body
- Invalid query parameters
- Missing required fields in JSON
- Invalid data types

**Error Codes:**
- `INVALID_JSON`
- `INVALID_PARAMETER`
- `INVALID_MESSAGE_ID`
- `INVALID_COMMAND`

**Example:**
```
POST /api/v3/otgw/commands
{
  "invalid json
}

→ 400 Bad Request
{
  "error": {
    "code": "INVALID_JSON",
    "message": "Request body must be valid JSON"
  }
}
```

---

### 404 Not Found

**Usage:** Resource does not exist

**Applies To:**
- Unknown endpoint
- Resource ID doesn't exist
- Sensor address not found

**Error Codes:**
- `RESOURCE_NOT_FOUND`

**Example:**
```
GET /api/v3/otgw/messages/999

→ 404 Not Found
{
  "error": {
    "code": "INVALID_MESSAGE_ID",
    "message": "Message ID must be between 0 and 127"
  }
}
```

---

### 405 Method Not Allowed

**Usage:** HTTP method not supported for resource

**Applies To:**
- Using POST on GET-only endpoint
- Using DELETE on non-deletable resource
- Any unsupported HTTP method

**Headers:**
```
Allow: GET, PATCH
```

**Error Codes:**
- `METHOD_NOT_ALLOWED`

**Example:**
```
DELETE /api/v3/system/info

→ 405 Method Not Allowed
Allow: GET
{
  "error": {
    "code": "METHOD_NOT_ALLOWED",
    "message": "Method not allowed",
    "details": {
      "method": "DELETE",
      "allowed_methods": ["GET"]
    }
  }
}
```

---

### 409 Conflict

**Usage:** Resource state conflicts with operation

**Applies To:**
- Trying to flash PIC while already flashing
- State conflicts
- Concurrent modification conflicts

**Error Codes:**
- `RESOURCE_CONFLICT`

**Example:**
```
POST /api/v3/pic/flash
(while already flashing)

→ 409 Conflict
{
  "error": {
    "code": "RESOURCE_CONFLICT",
    "message": "PIC flash already in progress"
  }
}
```

---

### 413 Payload Too Large

**Usage:** Request body exceeds size limit

**Applies To:**
- POST/PUT/PATCH with body > limit
- Command string too long

**Error Codes:**
- `PAYLOAD_TOO_LARGE`

**Example:**
```
PATCH /api/v3/config/settings
{
  "hostname": "very-long-hostname-that-exceeds-limit..."
}

→ 413 Payload Too Large
{
  "error": {
    "code": "PAYLOAD_TOO_LARGE",
    "message": "Request body exceeds 256 bytes"
  }
}
```

---

### 414 URI Too Long

**Usage:** Request URI exceeds buffer limit

**Applies To:**
- URI > 50 characters (ESP8266 constraint)

**Error Codes:**
- `URI_TOO_LONG`

**Example:**
```
GET /api/v3/very/long/uri/that/exceeds/buffer/size...

→ 414 URI Too Long
{
  "error": {
    "code": "URI_TOO_LONG",
    "message": "Request URI too long"
  }
}
```

---

### 422 Unprocessable Entity

**Usage:** Valid JSON but semantic errors

**Applies To:**
- Field value out of range
- Invalid field combinations
- Business logic validation failures

**Error Codes:**
- `INVALID_FIELD`
- `MISSING_REQUIRED_FIELD`

**Example:**
```
PATCH /api/v3/config/mqtt
{
  "port": 99999
}

→ 422 Unprocessable Entity
{
  "error": {
    "code": "INVALID_FIELD",
    "message": "Port must be between 1 and 65535"
  }
}
```

---

### 429 Too Many Requests

**Usage:** Rate limit exceeded

**Applies To:**
- Client exceeding 60 requests/minute

**Headers:**
```
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1738324275
Retry-After: 30
```

**Error Codes:**
- `RATE_LIMIT_EXCEEDED`

**Example:**
```
GET /api/v3/otgw/messages
(61st request in 1 minute)

→ 429 Too Many Requests
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 0
Retry-After: 30
{
  "error": {
    "code": "RATE_LIMIT_EXCEEDED",
    "message": "Rate limit exceeded, retry after 30 seconds"
  }
}
```

---

## Server Error Codes (5xx)

### 500 Internal Server Error

**Usage:** Unexpected server error

**Applies To:**
- Unhandled exceptions
- Filesystem errors
- Internal state corruption

**Error Codes:**
- `SYSTEM_FILESYSTEM_ERROR`
- Generic internal errors

**Example:**
```
GET /api/v3/config/settings

→ 500 Internal Server Error
{
  "error": {
    "code": "SYSTEM_FILESYSTEM_ERROR",
    "message": "Failed to read settings file"
  }
}
```

---

### 503 Service Unavailable

**Usage:** Service temporarily unavailable

**Applies To:**
- Low heap condition (< 4096 bytes)
- System busy (PIC flash in progress)
- OTGW not connected

**Headers:**
```
Retry-After: 5
```

**Error Codes:**
- `SYSTEM_LOW_HEAP`
- `SYSTEM_BUSY`
- `RESOURCE_UNAVAILABLE`

**Example:**
```
GET /api/v3/otgw/messages

→ 503 Service Unavailable
Retry-After: 5
{
  "error": {
    "code": "SYSTEM_LOW_HEAP",
    "message": "Insufficient memory, retry later",
    "details": {
      "free_heap": 3500,
      "minimum_required": 4096
    }
  }
}
```

---

## Status Code Decision Tree

```
Request received
    │
    ├─ URI too long? → 414 URI Too Long
    │
    ├─ Method allowed? → 405 Method Not Allowed
    │
    ├─ Valid JSON? → 400 Bad Request
    │
    ├─ Heap sufficient? → 503 Service Unavailable
    │
    ├─ Rate limit OK? → 429 Too Many Requests
    │
    ├─ Resource exists? → 404 Not Found
    │
    ├─ Resource available? → 503 Service Unavailable
    │
    ├─ State conflict? → 409 Conflict
    │
    ├─ Valid fields? → 422 Unprocessable Entity
    │
    ├─ Process request
    │   ├─ Success (read) → 200 OK
    │   ├─ Success (create) → 201 Created
    │   ├─ Success (async) → 202 Accepted
    │   ├─ Success (no content) → 204 No Content
    │   ├─ Not modified (ETag) → 304 Not Modified
    │   └─ Error → 500 Internal Server Error
```

---

## HTTP Method to Status Code Mapping

### GET Requests

| Scenario | Status Code |
|----------|-------------|
| Success | 200 OK |
| Not modified (ETag match) | 304 Not Modified |
| Resource not found | 404 Not Found |
| Invalid parameter | 400 Bad Request |
| System unavailable | 503 Service Unavailable |

### POST Requests

| Scenario | Status Code |
|----------|-------------|
| Resource created | 201 Created |
| Async operation accepted | 202 Accepted |
| Invalid JSON | 400 Bad Request |
| Invalid field | 422 Unprocessable Entity |
| Resource conflict | 409 Conflict |
| System unavailable | 503 Service Unavailable |

### PUT Requests

| Scenario | Status Code |
|----------|-------------|
| Success | 200 OK |
| Invalid JSON | 400 Bad Request |
| Invalid field | 422 Unprocessable Entity |
| Resource not found | 404 Not Found |

### PATCH Requests

| Scenario | Status Code |
|----------|-------------|
| Success | 200 OK |
| Invalid JSON | 400 Bad Request |
| Invalid field | 422 Unprocessable Entity |
| Unknown field | 422 Unprocessable Entity |
| Resource not found | 404 Not Found |

### DELETE Requests

| Scenario | Status Code |
|----------|-------------|
| Success with body | 200 OK |
| Success no body | 204 No Content |
| Resource not found | 404 Not Found |
| Cannot delete | 409 Conflict |

### OPTIONS Requests

| Scenario | Status Code |
|----------|-------------|
| Always | 204 No Content |

### HEAD Requests

| Scenario | Status Code |
|----------|-------------|
| Same as GET | Same as corresponding GET |

---

## Examples by Endpoint

### GET /api/v3/system/info

| Status | Scenario |
|--------|----------|
| 200 | Success |
| 304 | Not Modified (ETag match) |
| 503 | Low heap |

### POST /api/v3/otgw/commands

| Status | Scenario |
|--------|----------|
| 201 | Command queued |
| 400 | Invalid JSON or command format |
| 422 | Invalid command value |
| 503 | OTGW not connected |

### PATCH /api/v3/config/mqtt

| Status | Scenario |
|--------|----------|
| 200 | Settings updated |
| 400 | Invalid JSON |
| 422 | Invalid field value |
| 503 | Low heap or system busy |

### GET /api/v3/sensors/dallas/{address}

| Status | Scenario |
|--------|----------|
| 200 | Sensor found |
| 404 | Sensor address not found |
| 503 | Sensors not enabled |

---

## See Also

- [ADR-025: REST API v3 Design](../adr/ADR-025-rest-api-v3-design.md)
- [Resource Model](RESOURCE_MODEL.md)
- [Error Responses](ERROR_RESPONSES.md)
- [Response Headers](RESPONSE_HEADERS.md)
