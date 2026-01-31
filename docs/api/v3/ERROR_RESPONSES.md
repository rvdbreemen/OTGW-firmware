# REST API v3 Error Responses

**Version:** 3.0.0  
**Status:** Design Specification  
**Last Updated:** 2026-01-31

---

## Overview

All v3 API errors return **structured JSON responses** with consistent format, proper HTTP status codes, and machine-readable error codes.

---

## Error Response Format

```json
{
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable description",
    "details": {
      "key": "value"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/resource/path"
  }
}
```

### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `code` | string | Yes | Machine-readable error code |
| `message` | string | Yes | Human-readable error message |
| `details` | object | No | Additional context about the error |
| `timestamp` | integer | Yes | Unix timestamp when error occurred |
| `path` | string | Yes | Request path that caused the error |

---

## Error Code Taxonomy

### System Errors (`SYSTEM_*`)

Errors related to system resources and state.

#### SYSTEM_LOW_HEAP
**Status:** 503 Service Unavailable  
**Description:** Insufficient heap memory to process request

```json
{
  "error": {
    "code": "SYSTEM_LOW_HEAP",
    "message": "Insufficient memory to process request",
    "details": {
      "free_heap": 3500,
      "minimum_required": 4096
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/messages"
  }
}
```

#### SYSTEM_FILESYSTEM_ERROR
**Status:** 500 Internal Server Error  
**Description:** LittleFS filesystem error

```json
{
  "error": {
    "code": "SYSTEM_FILESYSTEM_ERROR",
    "message": "Failed to access filesystem",
    "details": {
      "operation": "read",
      "file": "settings.json"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/config/settings"
  }
}
```

#### SYSTEM_BUSY
**Status:** 503 Service Unavailable  
**Description:** System is currently busy processing another operation

```json
{
  "error": {
    "code": "SYSTEM_BUSY",
    "message": "System is busy, please retry",
    "details": {
      "operation": "PIC flash in progress"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/pic/flash"
  }
}
```

---

### Validation Errors (`INVALID_*`)

Errors related to invalid input or malformed requests.

#### INVALID_JSON
**Status:** 400 Bad Request  
**Description:** Request body is not valid JSON

```json
{
  "error": {
    "code": "INVALID_JSON",
    "message": "Request body must be valid JSON",
    "details": {
      "parse_error": "Unexpected token at position 42"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/config/settings"
  }
}
```

#### INVALID_MESSAGE_ID
**Status:** 400 Bad Request  
**Description:** OpenTherm message ID out of range

```json
{
  "error": {
    "code": "INVALID_MESSAGE_ID",
    "message": "Message ID must be between 0 and 127",
    "details": {
      "provided": "256",
      "min": 0,
      "max": 127
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/messages/256"
  }
}
```

#### INVALID_PARAMETER
**Status:** 400 Bad Request  
**Description:** Invalid query parameter value

```json
{
  "error": {
    "code": "INVALID_PARAMETER",
    "message": "Invalid value for parameter 'per_page'",
    "details": {
      "parameter": "per_page",
      "provided": "200",
      "max": 128
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/messages"
  }
}
```

#### INVALID_COMMAND
**Status:** 400 Bad Request  
**Description:** Invalid OTGW command format

```json
{
  "error": {
    "code": "INVALID_COMMAND",
    "message": "Invalid command format",
    "details": {
      "provided": "TT",
      "expected_format": "XX=value"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/commands"
  }
}
```

#### INVALID_FIELD
**Status:** 422 Unprocessable Entity  
**Description:** Field value fails semantic validation

```json
{
  "error": {
    "code": "INVALID_FIELD",
    "message": "Invalid field value",
    "details": {
      "field": "mqttbrokerport",
      "provided": "99999",
      "valid_range": "1-65535"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/config/mqtt"
  }
}
```

#### MISSING_REQUIRED_FIELD
**Status:** 422 Unprocessable Entity  
**Description:** Required field not provided

```json
{
  "error": {
    "code": "MISSING_REQUIRED_FIELD",
    "message": "Required field missing",
    "details": {
      "field": "command",
      "required_fields": ["command"]
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/commands"
  }
}
```

---

### Resource Errors (`RESOURCE_*`)

Errors related to resource availability and state.

#### RESOURCE_NOT_FOUND
**Status:** 404 Not Found  
**Description:** Requested resource does not exist

```json
{
  "error": {
    "code": "RESOURCE_NOT_FOUND",
    "message": "Resource not found",
    "details": {
      "resource_type": "sensor",
      "resource_id": "28AABBCCDDEE9999"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/sensors/dallas/28AABBCCDDEE9999"
  }
}
```

#### RESOURCE_UNAVAILABLE
**Status:** 503 Service Unavailable  
**Description:** Resource exists but is currently unavailable

```json
{
  "error": {
    "code": "RESOURCE_UNAVAILABLE",
    "message": "OTGW is not connected",
    "details": {
      "resource": "otgw",
      "reason": "PIC not responding"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/status"
  }
}
```

#### RESOURCE_CONFLICT
**Status:** 409 Conflict  
**Description:** Resource state conflicts with operation

```json
{
  "error": {
    "code": "RESOURCE_CONFLICT",
    "message": "Resource state conflict",
    "details": {
      "resource": "pic_flash",
      "current_state": "flashing",
      "required_state": "idle"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/pic/flash"
  }
}
```

---

### Request Errors

#### METHOD_NOT_ALLOWED
**Status:** 405 Method Not Allowed  
**Description:** HTTP method not supported for this resource

```json
{
  "error": {
    "code": "METHOD_NOT_ALLOWED",
    "message": "Method not allowed",
    "details": {
      "method": "DELETE",
      "allowed_methods": ["GET", "PATCH"]
    },
    "timestamp": 1738324245,
    "path": "/api/v3/config/mqtt"
  }
}
```

**Headers:**
```
Allow: GET, PATCH
```

#### PAYLOAD_TOO_LARGE
**Status:** 413 Payload Too Large  
**Description:** Request body exceeds size limit

```json
{
  "error": {
    "code": "PAYLOAD_TOO_LARGE",
    "message": "Request body exceeds maximum size",
    "details": {
      "size": 512,
      "max_size": 256
    },
    "timestamp": 1738324245,
    "path": "/api/v3/config/settings"
  }
}
```

#### URI_TOO_LONG
**Status:** 414 URI Too Long  
**Description:** Request URI exceeds buffer size

```json
{
  "error": {
    "code": "URI_TOO_LONG",
    "message": "Request URI too long",
    "details": {
      "length": 100,
      "max_length": 50
    },
    "timestamp": 1738324245,
    "path": "/api/v3/..."
  }
}
```

---

### Rate Limiting

#### RATE_LIMIT_EXCEEDED
**Status:** 429 Too Many Requests  
**Description:** Client exceeded rate limit

```json
{
  "error": {
    "code": "RATE_LIMIT_EXCEEDED",
    "message": "Rate limit exceeded",
    "details": {
      "limit": 60,
      "window": "1 minute",
      "retry_after": 30
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/messages"
  }
}
```

**Headers:**
```
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 0
X-RateLimit-Reset: 1738324275
Retry-After: 30
```

---

## Error Response Examples

### Example 1: Invalid Message ID

**Request:**
```
GET /api/v3/otgw/messages/999
```

**Response:** 400 Bad Request
```json
{
  "error": {
    "code": "INVALID_MESSAGE_ID",
    "message": "Message ID must be between 0 and 127",
    "details": {
      "provided": "999",
      "min": 0,
      "max": 127
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/messages/999"
  }
}
```

---

### Example 2: Low Heap

**Request:**
```
GET /api/v3/otgw/messages?per_page=128
```

**Response:** 503 Service Unavailable
```json
{
  "error": {
    "code": "SYSTEM_LOW_HEAP",
    "message": "Insufficient memory to process request",
    "details": {
      "free_heap": 3200,
      "minimum_required": 4096
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/messages"
  }
}
```

---

### Example 3: Invalid Field

**Request:**
```
PATCH /api/v3/config/mqtt
{
  "port": 99999
}
```

**Response:** 422 Unprocessable Entity
```json
{
  "error": {
    "code": "INVALID_FIELD",
    "message": "Invalid field value",
    "details": {
      "field": "port",
      "provided": "99999",
      "valid_range": "1-65535"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/config/mqtt"
  }
}
```

---

### Example 4: Resource Not Found

**Request:**
```
GET /api/v3/sensors/dallas/28XXXXXXXXXX
```

**Response:** 404 Not Found
```json
{
  "error": {
    "code": "RESOURCE_NOT_FOUND",
    "message": "Sensor not found",
    "details": {
      "resource_type": "dallas_sensor",
      "address": "28XXXXXXXXXX"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/sensors/dallas/28XXXXXXXXXX"
  }
}
```

---

### Example 5: OTGW Not Connected

**Request:**
```
POST /api/v3/otgw/commands
{
  "command": "TT=20.5"
}
```

**Response:** 503 Service Unavailable
```json
{
  "error": {
    "code": "RESOURCE_UNAVAILABLE",
    "message": "OTGW is not connected",
    "details": {
      "resource": "otgw",
      "reason": "PIC not responding"
    },
    "timestamp": 1738324245,
    "path": "/api/v3/otgw/commands"
  }
}
```

---

## Client Handling Guide

### Retry Logic

| Status Code | Retry? | Strategy |
|-------------|--------|----------|
| 400-499 | No | Client error - fix request |
| 500 | Yes | Temporary error - retry with backoff |
| 503 | Yes | Service unavailable - retry after delay |
| 429 | Yes | Rate limited - respect Retry-After header |

### Example Client Code (JavaScript)

```javascript
async function apiRequest(url, options = {}) {
  try {
    const response = await fetch(url, options);
    
    if (!response.ok) {
      const error = await response.json();
      
      // Handle specific error codes
      switch (error.error.code) {
        case 'RATE_LIMIT_EXCEEDED':
          const retryAfter = error.error.details.retry_after;
          console.log(`Rate limited, retry after ${retryAfter}s`);
          break;
          
        case 'SYSTEM_LOW_HEAP':
          console.log('System busy, retry later');
          break;
          
        case 'INVALID_MESSAGE_ID':
          console.error('Invalid input:', error.error.message);
          break;
          
        default:
          console.error('API error:', error.error);
      }
      
      throw error;
    }
    
    return await response.json();
  } catch (err) {
    console.error('Request failed:', err);
    throw err;
  }
}
```

---

## See Also

- [ADR-025: REST API v3 Design](../adr/ADR-025-rest-api-v3-design.md)
- [Resource Model](RESOURCE_MODEL.md)
- [HTTP Status Codes](HTTP_STATUS_CODES.md)
- [Response Headers](RESPONSE_HEADERS.md)
