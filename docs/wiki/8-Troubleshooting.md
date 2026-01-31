# Troubleshooting Guide

Common issues and solutions for the REST API v3.

## Connection Issues

### Device Not Reachable

**Symptoms:**
```
curl: (7) Failed to connect to otgw-12345.local port 80: Connection refused
curl: (6) Could not resolve host: otgw-12345.local
```

**Diagnosis Checklist:**

1. **Is device powered on?**
   ```bash
   # Check if you can ping the device
   ping otgw-12345.local
   ```

2. **Is device on same WiFi network?**
   - Check WiFi router connected devices list
   - Look for device named `OTGW-*`
   - Verify you're on the same WiFi network

3. **Is device hostname correct?**
   ```bash
   # Find device serial number on device label
   # Then use: otgw-XXXX where XXXX is serial
   ping otgw-XXXX.local
   
   # If mDNS doesn't work, find IP address:
   # Check router's DHCP client list for OTGW entry
   ```

4. **Is firewall blocking port 80?**
   ```bash
   # Try via IP address instead of hostname
   curl http://192.168.1.100/api/v3/system/health
   
   # If this works, issue is DNS/hostname resolution
   # If this fails, check firewall rules
   ```

5. **Device recently rebooted?**
   ```bash
   # Device takes ~30 seconds to boot after power-on
   # Wait 30 seconds and try again
   sleep 30
   curl http://device/api/v3/system/health
   ```

**Solutions:**

| Issue | Solution |
|-------|----------|
| Device offline | Power cycle device (off/on) |
| Wrong hostname | Check serial number on device |
| Wrong network | Connect to same WiFi as device |
| Firewall blocking | Allow port 80 in firewall settings |
| Device rebooting | Wait 30-60 seconds for device to boot |

---

## API Response Issues

### 404 Not Found

**Error:**
```json
{
  "error": {
    "code": "ENDPOINT_NOT_FOUND",
    "message": "Endpoint not found: /api/v1/otgw/status",
    "statusCode": 404
  }
}
```

**Causes:**
- Wrong API version (v1 vs v3)
- Misspelled endpoint URL
- Endpoint doesn't exist

**Solutions:**

```bash
# ❌ Wrong version
curl http://device/api/v1/otgw/status  # 404

# ✅ Correct version
curl http://device/api/v3/otgw/status

# ❌ Misspelled
curl http://device/api/v3/otgw/statuss  # 404

# ✅ Correct spelling
curl http://device/api/v3/otgw/status

# 🔍 Discover valid endpoints
curl http://device/api/v3 | jq '.links[] | .href'
```

---

### 400 Bad Request

**Error:**
```json
{
  "error": {
    "code": "INVALID_REQUEST_BODY",
    "message": "JSON parsing error: Unexpected token",
    "statusCode": 400
  }
}
```

**Common Causes:**

1. **Invalid JSON syntax**
   ```bash
   # ❌ Missing quotes or commas
   curl -X POST http://device/api/v3/otgw/command \
     -d '{command: "TT", value: 65}'
   
   # ✅ Valid JSON
   curl -X POST http://device/api/v3/otgw/command \
     -d '{"command": "TT", "value": 65}'
   ```

2. **Missing Content-Type header**
   ```bash
   # ❌ No Content-Type header
   curl -X POST http://device/api/v3/otgw/command \
     -d '{"command": "TT", "value": 65}'
   
   # ✅ With Content-Type
   curl -X POST http://device/api/v3/otgw/command \
     -H "Content-Type: application/json" \
     -d '{"command": "TT", "value": 65}'
   ```

3. **Missing required fields**
   ```bash
   # ❌ Missing "command" field
   curl -X POST http://device/api/v3/otgw/command \
     -H "Content-Type: application/json" \
     -d '{"value": 65}'
   
   # ✅ Include required fields
   curl -X POST http://device/api/v3/otgw/command \
     -H "Content-Type: application/json" \
     -d '{"command": "TT", "value": 65}'
   ```

**Solutions:**

```bash
# Validate JSON before sending
echo '{"command": "TT", "value": 65}' | jq .

# Use verbose cURL to see full error
curl -v -X POST http://device/api/v3/otgw/command \
  -H "Content-Type: application/json" \
  -d '{"command": "TT", "value": 65}'
```

---

### 429 Too Many Requests

**Error:**
```json
{
  "error": {
    "code": "RATE_LIMIT_EXCEEDED",
    "message": "Rate limit exceeded: 100 requests per minute",
    "statusCode": 429
  }
}
```

**Cause:**
- Sent more than 100 requests per minute from this IP

**Solutions:**

```bash
# Wait 60 seconds before retrying
sleep 60
curl http://device/api/v3/system/health

# Reduce request frequency in your client
# Add delays between requests
sleep 1
curl http://device/api/v3/system/health
```

**In Code:**

```python
import time

def rate_limited_request(endpoint, max_requests_per_min=100):
    min_interval = 60 / max_requests_per_min  # ~0.6 seconds
    time.sleep(min_interval)
    return requests.get(endpoint)
```

---

### 500 Internal Server Error

**Error:**
```json
{
  "error": {
    "code": "INTERNAL_SERVER_ERROR",
    "message": "Unexpected error processing request",
    "statusCode": 500
  }
}
```

**Possible Causes:**
- Unhandled exception in device code
- Memory exhaustion
- Corrupted configuration
- Device overloaded

**Solutions:**

1. **Check device logs:**
   ```
   Open http://device/ in browser
   Click "Debug" tab
   Look for error messages
   ```

2. **Reboot device:**
   ```bash
   # Send reboot command
   curl -X POST http://device/api/v3/system/reboot
   
   # Or power cycle manually
   ```

3. **Check free memory:**
   ```bash
   curl http://device/api/v3/system/health | jq .freeheap
   
   # If < 5000 bytes, device is out of memory
   # Reboot to clear
   ```

4. **Check disk space:**
   ```bash
   curl http://device/api/v3/system/stats | jq .filesystem
   
   # If mostly full, clear old logs
   ```

---

## OpenTherm Specific Issues

### PIC Not Available

**Error:**
```json
{
  "code": "PIC_NOT_AVAILABLE",
  "message": "OTGW PIC controller is not responding",
  "statusCode": 503
}
```

**Diagnosis:**

```bash
# Check device health
curl http://device/api/v3/system/health | jq .picavailable
# Should return: true

# If false, PIC is not available
```

**Causes:**
- PIC firmware not loaded
- Serial cable disconnected
- PIC hardware failure
- Serial communication error

**Solutions:**

1. **Check PIC firmware:**
   ```bash
   curl http://device/api/v3/pic/info
   
   # If error, firmware may not be installed
   ```

2. **Check hardware connections:**
   - Verify serial cable is connected
   - Check for bent pins
   - Power cycle device

3. **Reload PIC firmware:**
   - See device firmware upgrade instructions
   - Browse to device UI
   - Use "PIC Firmware" tab

4. **Check device logs:**
   ```
   http://device/ → Debug tab
   Look for serial communication errors
   ```

---

### Command Not Executing

**Symptoms:**
```bash
# Command is accepted but not executed
curl -X POST http://device/api/v3/otgw/command \
  -H "Content-Type: application/json" \
  -d '{"command": "TT", "value": 65}'

# Response: { "status": "queued" }
# But boiler doesn't change temperature
```

**Diagnosis Checklist:**

1. **Is command syntax valid?**
   ```bash
   # Commands are two uppercase letters: TT, SW, SG, etc.
   # ❌ Invalid: "tt" (lowercase), "Temp" (not two letters)
   # ✅ Valid: "TT", "SW"
   
   # Check boiler supports command
   curl http://device/api/v3/pic/info
   ```

2. **Is PIC available?**
   ```bash
   curl http://device/api/v3/system/health | jq .picavailable
   # Must be: true
   ```

3. **Check command queue:**
   ```bash
   # Queue has 20 command limit
   # If full, commands wait
   # Wait 5 seconds and try again
   ```

4. **Check boiler is responding:**
   ```bash
   curl http://device/api/v3/otgw/status
   # Should return current boiler data
   ```

**Solutions:**

| Check | Fix |
|-------|-----|
| Command syntax | Use uppercase two-letter codes (TT, SW, SG) |
| PIC available | Reboot device or reload PIC firmware |
| Queue full | Wait 5 seconds and retry |
| Boiler offline | Check boiler power and connections |

---

## Performance Issues

### API Requests Slow

**Diagnosis:**

```bash
# Measure response time
curl -o /dev/null -s -w 'Time: %{time_total}s\n' \
  http://device/api/v3/system/health

# Expected: < 100ms
# If > 500ms, performance issue
```

**Causes:**
- Device is overloaded
- Network issues
- Large response (many messages)
- WiFi signal weak

**Solutions:**

1. **Use pagination:**
   ```bash
   # ❌ Slow: Get all 1000 messages
   curl http://device/api/v3/otgw/messages
   
   # ✅ Fast: Get 50 messages at a time
   curl "http://device/api/v3/otgw/messages?limit=50&offset=0"
   ```

2. **Use filtering:**
   ```bash
   # ❌ Slow: All messages including noise
   curl http://device/api/v3/otgw/messages
   
   # ✅ Fast: Only temperature messages
   curl "http://device/api/v3/otgw/messages?category=Temperature"
   ```

3. **Use ETag caching:**
   ```bash
   # First request
   response=$(curl -i http://device/api/v3/system/health)
   etag=$(echo "$response" | grep -i etag | awk '{print $2}')
   
   # Follow-up requests with caching
   curl -H "If-None-Match: $etag" \
     http://device/api/v3/system/health
   # Returns 304 Not Modified (no body sent)
   ```

4. **Check WiFi signal:**
   ```bash
   curl http://device/api/v3/system/health | jq .rssi
   
   # RSSI values:
   # -30 to -70: Excellent to good
   # -71 to -85: Fair
   # -86+: Poor
   
   # If < -80, move device closer to router
   ```

5. **Reduce request frequency:**
   ```bash
   # Wait between requests instead of flooding
   for i in {1..10}; do
     curl http://device/api/v3/system/health
     sleep 1  # Wait 1 second between requests
   done
   ```

---

### Device Becomes Unresponsive

**Symptoms:**
- API stops responding
- Device won't reboot
- Can't SSH or access device UI

**Causes:**
- Out of memory
- Infinite loop
- Watchdog timeout
- Hardware failure

**Recovery:**

1. **Power cycle device:**
   - Unplug power cable
   - Wait 10 seconds
   - Plug back in
   - Wait 30 seconds for boot

2. **If still unresponsive:**
   - Check device LEDs (should blink)
   - Check WiFi router (device should appear as connected)
   - Try accessing via IP address instead of hostname

3. **Last resort - factory reset:**
   - Hold reset button for 10+ seconds
   - Device will reboot to factory settings
   - Reconfigure WiFi via WiFiManager

---

## Debugging Techniques

### Enable Verbose Output

```bash
# cURL verbose mode
curl -v http://device/api/v3/system/health

# Shows: Request headers, response headers, full response body

# With pretty-printed JSON
curl -s http://device/api/v3/system/health | jq .
```

### Check Device Logs in Real-Time

```bash
# Open device UI in browser
http://device/

# Go to Debug tab
# Logs stream in real-time
# Look for errors and warnings
```

### Validate JSON Responses

```bash
# Ensure response is valid JSON
curl -s http://device/api/v3/system/health | jq empty

# If no error output, JSON is valid
# If error, response is malformed
```

### Test with Simple Commands

```bash
# Start with simplest endpoint to isolate issue
curl http://device/api/v3/system/health

# If works, try more complex:
curl http://device/api/v3/system/info
curl http://device/api/v3/otgw/status
curl "http://device/api/v3/otgw/messages?limit=10"
```

---

## FAQ

**Q: My requests work sometimes but fail other times?**  
A: Device may be overloaded. Use pagination and filtering. Reduce request frequency. Check available memory.

**Q: Why is my command queued but not executing?**  
A: Check PIC is available. Verify command syntax (uppercase). Check queue isn't full (wait 5s).

**Q: Device becomes unresponsive after a while?**  
A: Out of memory. Power cycle device. Reduce request frequency. Check for memory leaks.

**Q: Can I access the API from the internet?**  
A: Not recommended. Device is for local network only. Use VPN for remote access.

**Q: Why are my requests slow?**  
A: Use pagination for large datasets. Use filtering. Check WiFi signal. Cache with ETags.

**Q: How do I know what commands the boiler supports?**  
A: Check boiler manual or OTGW documentation. Common: TT (setpoint), SW (DHW), SG (setpoint water).

---

## Still Need Help?

- 📖 **Full Examples**: [example-api](../../example-api/)
- 📚 **API Reference**: [Complete API Reference](Complete-API-Reference)
- 🐛 **Report Issues**: [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)
- 💬 **Ask Questions**: [GitHub Discussions](https://github.com/rvdbreemen/OTGW-firmware/discussions)

---

**Most issues are resolved by:** Checking connectivity, validating JSON, using pagination, and power cycling! 🔧
