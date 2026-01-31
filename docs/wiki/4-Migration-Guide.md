# Migration Guide

Upgrading from REST API v1 or v2 to v3? This guide covers everything you need to know.

## Good News: Zero Breaking Changes! ✅

REST API v3 is **100% backward compatible** with v1 and v2:

- ✅ All v0, v1, v2 endpoints continue working unchanged
- ✅ All existing response formats are preserved
- ✅ All existing commands work the same
- ✅ v3 is *additive*, not breaking
- ✅ No code changes required
- ✅ Can run v1, v2, and v3 code side-by-side

## Why Upgrade to v3?

### 1. Modern REST Architecture
- Proper HTTP semantics for all operations
- HATEOAS for API discovery
- Richardson Maturity Level 3
- Standards-compliant (RFC 7231, 7232, 5789)

### 2. Advanced Features
- **ETag Caching**: Conditional requests for bandwidth savings
- **Pagination**: Handle large message collections efficiently
- **Query Filtering**: Filter by category, timestamp, status
- **Rate Limiting**: Fair resource usage
- **CORS**: Better browser integration
- **OPTIONS method**: Check allowed operations

### 3. Better Error Handling
- Detailed error objects with codes
- Clear problem descriptions
- HTTP status codes follow standards
- Client-friendly error formats

### 4. Performance
- Optimized response formats
- Caching support
- Query filtering reduces data transfer
- Pagination for large datasets

### 5. API Discovery
- GET `/api/v3` returns full HATEOAS graph
- Clients can discover available operations
- No hardcoded endpoint URLs needed
- Self-documenting API

## Migration Strategies

### Strategy 1: Phased Upgrade (Recommended)

**Phase 1: Coexistence (1-2 weeks)**
- Keep using v1/v2 endpoints
- Add new v3 code for new features
- Both work simultaneously
- Zero disruption

**Phase 2: Migration (2-4 weeks)**
- Gradually migrate endpoints to v3
- Update API clients to use v3 URLs
- Leverage new v3 features
- Monitor for issues

**Phase 3: Cleanup (1 week)**
- Remove v1/v2 code paths
- Use v3 everywhere
- Enjoy modern REST!

### Strategy 2: Quick Upgrade (If you prefer rip-and-replace)

1. Update all endpoint URLs from `/api/v1/` to `/api/v3/`
2. No code logic changes needed
3. Test in dev environment first
4. Deploy to production

## Side-by-Side Comparison

| Feature | v1 | v2 | v3 |
|---------|-----|-----|-----|
| **Endpoints** | 11 | 13 | 31 |
| **HTTP Methods** | GET, POST | GET, POST | GET, POST, PATCH, PUT, DELETE, OPTIONS |
| **HATEOAS** | ❌ | ❌ | ✅ |
| **ETag Caching** | ❌ | ❌ | ✅ |
| **Pagination** | ❌ | ❌ | ✅ |
| **Query Filtering** | ❌ | ❌ | ✅ |
| **Error Objects** | Simple | Simple | Detailed with codes |
| **Rate Limiting** | ❌ | ❌ | ✅ |
| **CORS** | ❌ | ❌ | ✅ |
| **API Discovery** | ❌ | ❌ | ✅ |
| **Standards Compliance** | Partial | Partial | Full (RFC 7231, 7232, 5789) |

## Common Migration Paths

### Health Check Endpoint

**v1:**
```bash
curl http://device/api/v1/health
```

**v3:**
```bash
curl http://device/api/v3/system/health
# Additional features:
# - Includes uptime, free heap, RSSI, temperature
# - Smaller payload
# - ETag support for caching
```

### Getting Device Info

**v1:**
```bash
curl http://device/api/v1/info
```

**v3:**
```bash
curl http://device/api/v3/system/info
# Includes HATEOAS links to all operations
# Supports ETag conditional requests
```

### Sending OTGW Commands

**v1:**
```bash
curl -X POST http://device/api/v1/otgw/command \
  -H "Content-Type: application/json" \
  -d '{"command": "TT", "value": 65}'
```

**v3:**
```bash
curl -X POST http://device/api/v3/otgw/command \
  -H "Content-Type: application/json" \
  -d '{"command": "TT", "value": 65}'
# Same syntax, same results
# Just different URL path
```

### Getting OpenTherm Messages

**v1:**
```bash
curl http://device/api/v1/otgw/messages
```

**v3:**
```bash
# Basic (same as v1):
curl http://device/api/v3/otgw/messages

# With pagination:
curl "http://device/api/v3/otgw/messages?limit=50&offset=0"

# With filtering:
curl "http://device/api/v3/otgw/messages?category=Temperature&limit=50"

# With caching:
curl -H "If-None-Match: \"abc123\"" http://device/api/v3/otgw/messages
```

### Getting Configuration

**v1:**
```bash
curl http://device/api/v1/config
```

**v3:**
```bash
# Device config:
curl http://device/api/v3/config/device

# Network config:
curl http://device/api/v3/config/network

# MQTT config:
curl http://device/api/v3/config/mqtt

# OTGW config:
curl http://device/api/v3/config/otgw

# All configs:
curl http://device/api/v3 | jq '.links | .[] | select(.rel=="configuration")'
```

### Updating Configuration

**v1:**
```bash
# v1 didn't support PATCH, had to replace entire config
curl -X PUT http://device/api/v1/config \
  -d '@full_config.json'
```

**v3:**
```bash
# Partial update (much easier):
curl -X PATCH http://device/api/v3/config/device \
  -H "Content-Type: application/json" \
  -d '{"hostname": "my-otgw"}'

# No need to provide full config
# Only changed fields required
```

## Client Library Updates

### JavaScript

**v1 Code:**
```javascript
const response = await fetch('http://device/api/v1/otgw/status');
const data = await response.json();
```

**v3 Code:**
```javascript
const response = await fetch('http://device/api/v3/otgw/status');
const data = await response.json();

// Optional: Use HATEOAS to discover API
const apiRoot = await fetch('http://device/api/v3').then(r => r.json());
const links = apiRoot.links; // All available endpoints
```

### Python

**v1 Code:**
```python
import requests
response = requests.get('http://device/api/v1/otgw/status')
data = response.json()
```

**v3 Code:**
```python
import requests
response = requests.get('http://device/api/v3/otgw/status')
data = response.json()

# Optional: Use ETag caching
headers = {'If-None-Match': last_etag}
response = requests.get('http://device/api/v3/otgw/status', headers=headers)
if response.status_code == 304:
    # Data unchanged, use cached version
    pass
```

## Deprecation Timeline

- **v0**: Deprecated (legacy)
- **v1**: Stable, maintenance-only
- **v2**: Stable, maintenance-only
- **v3**: Current production (recommended)

All versions will continue working. No forced migration dates.

## New Features in v3 You Should Know About

### 1. HATEOAS for API Discovery
```bash
# Get all available operations
curl http://device/api/v3 | jq '.links'
# Response shows all 31+ endpoints available
```

### 2. ETag Caching
```bash
# First request
curl -i http://device/api/v3/system/info
# Response header: ETag: "abc123"

# Follow-up request
curl -i -H 'If-None-Match: "abc123"' http://device/api/v3/system/info
# Response: 304 Not Modified (no body)
# Saves bandwidth, faster for repeated requests
```

### 3. Pagination for Large Collections
```bash
# Get 100 messages at a time
curl "http://device/api/v3/otgw/messages?limit=100&offset=0"
curl "http://device/api/v3/otgw/messages?limit=100&offset=100"
# Much more efficient than loading all at once
```

### 4. Query Filtering
```bash
# Filter messages by category
curl "http://device/api/v3/otgw/messages?category=Temperature"

# Reduces data transfer
# Server-side filtering is more efficient
```

### 5. Detailed Error Objects
```json
{
  "error": {
    "code": "INVALID_COMMAND",
    "message": "Unknown OTGW command: XZ",
    "statusCode": 400,
    "timestamp": "2026-01-31T10:30:00Z"
  }
}
```

## Testing Your Migration

### 1. Run Parallel Tests
```bash
# Test v1 endpoint
curl http://device/api/v1/otgw/status > v1_result.json

# Test v3 endpoint
curl http://device/api/v3/otgw/status > v3_result.json

# Compare results (should be identical except for links)
diff v1_result.json v3_result.json
```

### 2. Load Testing
```bash
# v1 endpoint
ab -n 1000 http://device/api/v1/health

# v3 endpoint
ab -n 1000 http://device/api/v3/system/health

# v3 should be faster (smaller payloads, better optimization)
```

### 3. Backward Compatibility Verification
```bash
# v1 endpoints still work (unchanged)
curl http://device/api/v1/health
curl http://device/api/v1/info
curl http://device/api/v1/otgw/status

# v2 endpoints still work (unchanged)
curl http://device/api/v2/health

# v3 endpoints work (new)
curl http://device/api/v3/system/health
```

## FAQ

**Q: Do I have to migrate to v3?**  
A: No! v1 and v2 will continue working indefinitely. v3 is optional for new features.

**Q: Will v1/v2 endpoints stop working?**  
A: No breaking changes planned. v1/v2 are stable and maintained for backward compatibility.

**Q: How long does migration take?**  
A: Just changing URL paths is 5 minutes. Leveraging new v3 features takes longer (1-4 weeks).

**Q: Can I run v1 and v3 code together?**  
A: Yes! Both work simultaneously. You can migrate incrementally.

**Q: Are there breaking changes in v3?**  
A: No! v3 is 100% backward compatible. All v1/v2 endpoints work unchanged.

**Q: What if I have custom code that depends on v1?**  
A: No changes needed! v1 endpoints continue working exactly as before.

## Next Steps

1. **Identify your current API version** - Check your code/scripts
2. **Read [Quick Start Guide](Quick-Start-Guide)** - Get familiar with v3
3. **Review [Complete API Reference](Complete-API-Reference)** - See all endpoints
4. **Update endpoint URLs** - Change `/api/v1/` to `/api/v3/`
5. **Test thoroughly** - Verify everything works
6. **Deploy gradually** - Update in phases if possible
7. **Use new features** - Leverage pagination, filtering, caching, HATEOAS

## Support

- 📖 **Full Examples**: [example-api](../../example-api/)
- 🐛 **Report Issues**: [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)
- 💬 **Ask Questions**: [GitHub Discussions](https://github.com/rvdbreemen/OTGW-firmware/discussions)

---

**Migration is easy!** Zero breaking changes, all new features, same solid reliability. 🚀
