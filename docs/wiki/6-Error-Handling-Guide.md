# Error Handling Guide

Understanding and handling errors from the REST API v3.

## HTTP Status Codes

REST API v3 follows HTTP standards for status codes:

### Success Responses

| Code | Meaning | When Used |
|------|---------|-----------|
| **200** | OK | Request succeeded, response body included |
| **201** | Created | Resource created successfully (POST) |
| **204** | No Content | Request succeeded, no response body (DELETE) |
| **304** | Not Modified | Data unchanged (ETag match), cached version ok |

### Client Error Responses

| Code | Meaning | When Used |
|------|---------|-----------|
| **400** | Bad Request | Invalid request syntax or parameters |
| **404** | Not Found | Resource doesn't exist (e.g., sensor not found) |
| **405** | Method Not Allowed | Wrong HTTP method (e.g., PUT instead of PATCH) |
| **429** | Too Many Requests | Rate limit exceeded (100 req/min per IP) |

### Server Error Responses

| Code | Meaning | When Used |
|------|---------|-----------|
| **500** | Internal Server Error | Unexpected server error |
| **503** | Service Unavailable | Device rebooting or overloaded |

## Error Response Format

All error responses follow a consistent structure:

```json
{
  "error": {
    "code": "INVALID_COMMAND",
    "message": "Unknown OTGW command: XZ",
    "statusCode": 400,
    "timestamp": "2026-01-31T10:30:00Z",
    "hint": "Valid commands are TT, SW, SG, etc."
  }
}
```

### Error Fields

| Field | Type | Description |
|-------|------|-------------|
| `code` | string | Machine-readable error code (e.g., INVALID_COMMAND) |
| `message` | string | Human-readable error description |
| `statusCode` | number | HTTP status code (200, 400, 500, etc.) |
| `timestamp` | string | ISO 8601 timestamp when error occurred |
| `hint` | string | Optional: Suggestion for fixing the error |

## Common Error Codes

### Device/Connection Errors

#### DEVICE_UNAVAILABLE
Device is temporarily unavailable.

```json
{
  "code": "DEVICE_UNAVAILABLE",
  "message": "Device not responding",
  "statusCode": 503,
  "hint": "Try again after 30 seconds or check device power"
}
```

**Causes:**
- Device is rebooting
- Network connection lost
- Device overloaded

**Solutions:**
1. Wait 30 seconds and retry
2. Check device is powered on
3. Check WiFi connection
4. Reboot device if persistent

#### PIC_NOT_AVAILABLE
OpenTherm Gateway PIC is not available.

```json
{
  "code": "PIC_NOT_AVAILABLE",
  "message": "OTGW PIC controller is not responding",
  "statusCode": 503
}
```

**Causes:**
- PIC firmware not loaded
- Serial communication failure
- PIC hardware issue

**Solutions:**
1. Check PIC firmware is installed
2. Check serial cable connection
3. Power cycle device
4. Check OTGW hardware connections

### Request Validation Errors

#### INVALID_REQUEST_BODY
Request body is malformed.

```json
{
  "code": "INVALID_REQUEST_BODY",
  "message": "JSON parsing error: Unexpected token",
  "statusCode": 400,
  "hint": "Ensure Content-Type: application/json header is set"
}
```

**Causes:**
- Invalid JSON syntax
- Missing Content-Type header
- Incomplete JSON object

**Solutions:**
1. Validate JSON with JSON linter
2. Set `Content-Type: application/json` header
3. Check for missing brackets or commas

#### INVALID_PARAMETER
Query parameter is invalid.

```json
{
  "code": "INVALID_PARAMETER",
  "message": "Invalid limit value: abc (must be number)",
  "statusCode": 400,
  "hint": "Use limit=50 for pagination"
}
```

**Causes:**
- Non-numeric value for numeric parameter
- Invalid parameter value
- Unsupported parameter

**Solutions:**
1. Check parameter type (string vs number)
2. Verify parameter name spelling
3. See API reference for valid values

#### MISSING_REQUIRED_FIELD
Required field is missing from request.

```json
{
  "code": "MISSING_REQUIRED_FIELD",
  "message": "Missing required field: command",
  "statusCode": 400,
  "hint": "POST body must include: { command: 'TT', value: 65 }"
}
```

**Causes:**
- Required field not included in request body
- Field name misspelled
- Empty request body

**Solutions:**
1. Add required field to request
2. Check field name spelling
3. Consult API reference for required fields

### OpenTherm Command Errors

#### INVALID_COMMAND
Unknown or invalid OTGW command.

```json
{
  "code": "INVALID_COMMAND",
  "message": "Unknown OTGW command: XZ",
  "statusCode": 400,
  "hint": "Valid commands: TT (setpoint), SW (DHW), SG (setpoint water), etc."
}
```

**Causes:**
- Misspelled command code
- Unsupported command on your device
- Command not implemented

**Solutions:**
1. Check command spelling (TT not tt)
2. See OTGW documentation for valid commands
3. Check device supports command

#### INVALID_COMMAND_VALUE
Command value is out of valid range.

```json
{
  "code": "INVALID_COMMAND_VALUE",
  "message": "Value 100 out of range for command TT (0-63)",
  "statusCode": 400,
  "hint": "Setpoint must be between 0°C and 63°C"
}
```

**Causes:**
- Value exceeds valid range
- Wrong data type for value
- Negative value where positive required

**Solutions:**
1. Check valid range for command in API reference
2. Ensure value is correct data type
3. Verify unit (Celsius vs Kelvin, etc.)

#### COMMAND_QUEUE_FULL
Command queue is full, cannot accept more commands.

```json
{
  "code": "COMMAND_QUEUE_FULL",
  "message": "Command queue is full, try again in 5 seconds",
  "statusCode": 429,
  "hint": "Maximum 20 queued commands"
}
```

**Causes:**
- Too many commands sent too quickly
- OTGW can't process commands fast enough
- Previous command hasn't completed

**Solutions:**
1. Wait 5 seconds before retrying
2. Reduce command sending frequency
3. Check previous commands completed

### Resource Not Found Errors

#### RESOURCE_NOT_FOUND
Resource doesn't exist.

```json
{
  "code": "RESOURCE_NOT_FOUND",
  "message": "Sensor not found: 28A1B2C3D4E5F601",
  "statusCode": 404,
  "hint": "Connected sensors: 28A1B2C3D4E5F602, 28A1B2C3D4E5F603"
}
```

**Causes:**
- Sensor address doesn't exist
- Sensor disconnected
- Sensor ID misspelled

**Solutions:**
1. Check connected sensors: `GET /sensors/dallas`
2. Verify sensor address is correct
3. Check sensor is powered and connected

#### ENDPOINT_NOT_FOUND
API endpoint doesn't exist.

```json
{
  "code": "ENDPOINT_NOT_FOUND",
  "message": "Endpoint not found: /api/v3/invalid/endpoint",
  "statusCode": 404,
  "hint": "Valid endpoints: GET /api/v3/system/info, GET /api/v3/otgw/status, ..."
}
```

**Causes:**
- Typo in endpoint URL
- Wrong API version
- Endpoint doesn't exist

**Solutions:**
1. Check URL spelling
2. Use `/api/v3/` not `/api/v1/` or `/api/v2/`
3. See API reference for valid endpoints

### Rate Limiting Errors

#### RATE_LIMIT_EXCEEDED
Too many requests from this IP.

```json
{
  "code": "RATE_LIMIT_EXCEEDED",
  "message": "Rate limit exceeded: 100 requests per minute",
  "statusCode": 429,
  "hint": "Wait 60 seconds before retrying"
}
```

**Causes:**
- Sent > 100 requests in 60 seconds
- Client not respecting rate limits
- Automated script running too fast

**Solutions:**
1. Implement exponential backoff
2. Add delays between requests
3. Use ETag caching to reduce requests
4. Wait 60 seconds and retry

### Server Errors

#### INTERNAL_SERVER_ERROR
Unexpected server error.

```json
{
  "code": "INTERNAL_SERVER_ERROR",
  "message": "Unexpected error processing request",
  "statusCode": 500,
  "hint": "Check device logs or report to GitHub Issues"
}
```

**Causes:**
- Unhandled exception in device code
- Memory exhaustion
- Corrupted data

**Solutions:**
1. Check device logs (browse to http://device/)
2. Reboot device
3. Report issue: https://github.com/rvdbreemen/OTGW-firmware/issues

## Error Handling Patterns

### JavaScript

```javascript
async function sendCommand(command, value) {
  try {
    const response = await fetch('http://device/api/v3/otgw/command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command, value })
    });

    if (!response.ok) {
      const error = await response.json();
      console.error(`Error ${error.error.code}: ${error.error.message}`);
      
      // Handle specific errors
      if (error.error.code === 'COMMAND_QUEUE_FULL') {
        console.log('Queue full, retrying in 5 seconds');
        setTimeout(() => sendCommand(command, value), 5000);
      }
      return null;
    }

    return await response.json();
  } catch (error) {
    console.error('Network error:', error);
    return null;
  }
}
```

### Python

```python
import requests
import time
from typing import Optional

def send_command(command: str, value: float) -> Optional[dict]:
    try:
        response = requests.post(
            'http://device/api/v3/otgw/command',
            json={'command': command, 'value': value},
            timeout=5
        )
        
        if response.status_code == 200:
            return response.json()
        
        elif response.status_code == 429:
            error = response.json()
            if error['error']['code'] == 'COMMAND_QUEUE_FULL':
                print('Queue full, retrying in 5 seconds')
                time.sleep(5)
                return send_command(command, value)
        
        else:
            error = response.json()
            print(f"Error {error['error']['code']}: {error['error']['message']}")
            return None
    
    except requests.RequestException as e:
        print(f"Network error: {e}")
        return None
```

### cURL with Error Handling

```bash
#!/bin/bash

send_command() {
  local cmd=$1
  local val=$2
  local response=$(curl -s -w "\n%{http_code}" -X POST \
    http://device/api/v3/otgw/command \
    -H "Content-Type: application/json" \
    -d "{\"command\": \"$cmd\", \"value\": $val}")
  
  local http_code=$(echo "$response" | tail -n1)
  local body=$(echo "$response" | head -n-1)
  
  if [ "$http_code" = "200" ]; then
    echo "Success: $body"
  elif [ "$http_code" = "429" ]; then
    echo "Queue full, retrying in 5 seconds"
    sleep 5
    send_command "$cmd" "$val"
  else
    echo "Error: $body"
  fi
}
```

## Debugging Tips

### Enable Verbose Logging

```bash
# cURL with verbose output
curl -v http://device/api/v3/system/health

# Shows: request headers, response headers, body
```

### Check Device Logs

```bash
# Browse to device UI
http://device/

# Open browser console (F12)
# Check "Debug" tab for real-time logs
```

### Test with Curl First

```bash
# Test endpoint before using in code
curl -i http://device/api/v3/system/health

# -i flag shows response headers and body
```

### Validate JSON

```bash
# Before sending POST, validate JSON
echo '{"command": "TT", "value": 65}' | jq .

# Will show if JSON is malformed
```

### Check Network Connectivity

```bash
# Is device reachable?
ping otgw-12345.local

# Is API responding?
curl http://otgw-12345.local/api/v3/system/health
```

## Retry Strategies

### Exponential Backoff

```python
import time
import random

def retry_with_backoff(func, max_retries=3):
    for attempt in range(max_retries):
        try:
            return func()
        except Exception as e:
            if attempt == max_retries - 1:
                raise
            
            # Exponential backoff with jitter
            wait_time = (2 ** attempt) + random.uniform(0, 1)
            print(f"Attempt {attempt + 1} failed, retrying in {wait_time:.1f}s")
            time.sleep(wait_time)
```

## Need Help?

- 📖 **Full Examples**: [example-api](../../example-api/)
- 🐛 **Report Issues**: [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)
- 💬 **Ask Questions**: [GitHub Discussions](https://github.com/rvdbreemen/OTGW-firmware/discussions)

---

**Robust error handling ensures reliable applications.** Follow these patterns for production-ready code! 🛡️
