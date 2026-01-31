# Changelog

Overview of REST API versions and what's changed.

## v3 (Current - January 2026)

### New Features
✨ **31 Endpoints** (vs 11 in v1, 13 in v2)
- Complete system information and health monitoring
- Comprehensive configuration management with PATCH support
- Full OpenTherm message history with pagination and filtering
- Advanced sensor data (Dallas, S0 counter)
- Multiple export formats (Telegraf, OTmonitor, settings)

✨ **Modern REST Architecture**
- Richardson Maturity Level 3 compliance
- HATEOAS hypermedia links for API discovery
- Proper HTTP method semantics (GET, POST, PATCH, PUT, DELETE, OPTIONS)
- Standards compliance: RFC 7231, 7232, 5789, W3C CORS

✨ **Advanced HTTP Features**
- **ETag Caching**: Conditional requests with 304 Not Modified responses
- **Pagination**: Efficiently handle large message collections
- **Query Filtering**: Server-side filtering by category, timestamp, status
- **Rate Limiting**: 100 requests/minute per IP, returns 429 when exceeded
- **CORS Support**: Cross-origin requests from browsers
- **OPTIONS Method**: Clients can check allowed operations

✨ **Detailed Error Handling**
- Structured error objects with error codes
- Clear problem descriptions and resolution hints
- Proper HTTP status codes (200, 201, 204, 304, 400, 404, 405, 429, 500)
- Client-friendly error formats

✨ **API Discovery**
- `GET /api/v3` returns complete HATEOAS graph
- All endpoints discoverable from API root
- No hardcoded URLs required
- Self-documenting API

### Performance
- Simple GET: < 50ms
- Complex JSON: < 200ms
- POST/PATCH: < 100ms
- Memory per request: < 2KB
- Smaller payloads (optimized JSON)

### Backward Compatibility
✅ **100% Compatible with v1 and v2**
- All v0, v1, v2 endpoints continue working unchanged
- No breaking changes
- Can run v1, v2, and v3 code simultaneously
- Seamless upgrade path

### Endpoints Added in v3

**System Category (5 endpoints)**
- `GET /system/` - System operations root
- `GET /system/info` - Device information
- `GET /system/health` - Health status
- `GET /system/time` - Time and timezone
- `GET /system/network` - Network information
- `GET /system/stats` - Runtime statistics

**Configuration Category (10 endpoints)**
- `GET /config/` - Configuration root
- `GET /config/device` - Device settings
- `PATCH /config/device` - Update device settings
- `GET /config/network` - Network settings
- `PATCH /config/network` - Update network settings
- `GET /config/mqtt` - MQTT settings
- `PATCH /config/mqtt` - Update MQTT settings
- `GET /config/otgw` - OTGW settings
- `PATCH /config/otgw` - Update OTGW settings
- `GET /config/features` - Feature flags
- `PATCH /config/features` - Update features

**OpenTherm Category (6 endpoints)**
- Enhanced with pagination, filtering, HATEOAS
- `GET /otgw/status` - Current boiler status (enhanced)
- `GET /otgw/messages` - All messages (with pagination/filtering)
- `GET /otgw/message/{id}` - Specific message
- `GET /otgw/data` - Latest snapshot
- `POST /otgw/command` - Send command (enhanced error handling)
- `GET /otgw/monitor` - OTmonitor export

**PIC Firmware Category (3 endpoints)**
- `GET /pic/info` - PIC information
- `GET /pic/versions` - Available firmware versions
- `GET /pic/diagnostics` - PIC diagnostics

**Sensors Category (4 endpoints)**
- `GET /sensors/dallas` - All Dallas sensors
- `GET /sensors/dallas/{address}` - Specific sensor
- `GET /sensors/s0` - S0 pulse counter
- `POST /sensors/s0/reset` - Reset counter

**Export Category (4 endpoints)**
- `GET /export/telegraf` - Telegraf/InfluxDB format
- `GET /export/otmonitor` - OTmonitor format
- `GET /export/settings` - All settings as JSON
- `GET /export/logs` - Device logs

**Metadata (2 endpoints)**
- `GET /api/v3` - Full API with HATEOAS links
- `OPTIONS *` - CORS preflight

### Breaking Changes
❌ **None!** v3 is fully backward compatible

## v2 (Stable - Maintenance Only)

### New in v2 vs v1
- Enhanced OTmonitor endpoint with structured data parsing
- Improved configuration responses
- Additional filtering options
- Better error messages

### Endpoints (13 total)
- `/api/v2/health` - Device health
- `/api/v2/info` - Device information
- `/api/v2/otgw/status` - Boiler status
- `/api/v2/otgw/messages` - Message history
- `/api/v2/otgw/command` - Send commands
- `/api/v2/config` - Device configuration
- And 7 more endpoints

### Status
✅ **Stable** - No planned changes  
✅ **Backward Compatible** - All v1 endpoints work  
📦 **Maintenance Only** - Bug fixes only  
⏰ **Sunset**: No forced migration date

## v1 (Stable - Maintenance Only)

### Endpoints (11 total)
- `/api/v1/health` - Device health
- `/api/v1/info` - Device information
- `/api/v1/otgw/status` - Boiler status
- `/api/v1/otgw/messages` - Message history
- `/api/v1/otgw/command` - Send commands
- `/api/v1/otgw/monitor` - OTmonitor data
- `/api/v1/config` - Configuration
- And 4 more endpoints

### Features
- ✅ Basic REST endpoints
- ✅ JSON responses
- ✅ OTGW command queue integration
- ✅ Stable and reliable

### Status
✅ **Stable** - No planned changes  
📦 **Maintenance Only** - Bug fixes only  
⏰ **Sunset**: No forced migration date  
✅ **Still Works**: 100% operational

## v0 (Legacy)

### Status
⚠️ **Deprecated** - No longer maintained  
❌ **Discouraged** - Use v1, v2, or v3  
✅ **Still Works**: Endpoints functional for backward compatibility

## Version Comparison Matrix

| Feature | v0 | v1 | v2 | v3 |
|---------|----|----|----|----|
| **Endpoints** | 8 | 11 | 13 | 31 |
| **Health Check** | ✅ | ✅ | ✅ | ✅ |
| **Device Info** | ✅ | ✅ | ✅ | ✅ |
| **OTGW Commands** | ✅ | ✅ | ✅ | ✅ |
| **Message History** | ✅ | ✅ | ✅ | ✅ (with pagination) |
| **Configuration** | ✅ | ✅ | ✅ | ✅ (with PATCH) |
| **Pagination** | ❌ | ❌ | ❌ | ✅ |
| **Query Filtering** | ❌ | ❌ | ❌ | ✅ |
| **ETag Caching** | ❌ | ❌ | ❌ | ✅ |
| **HATEOAS** | ❌ | ❌ | ❌ | ✅ |
| **PATCH Method** | ❌ | ❌ | ❌ | ✅ |
| **Rate Limiting** | ❌ | ❌ | ❌ | ✅ |
| **CORS** | ❌ | ❌ | ❌ | ✅ |
| **API Discovery** | ❌ | ❌ | ❌ | ✅ |
| **Error Objects** | Basic | Basic | Basic | Detailed |
| **Standards** | Partial | Partial | Partial | Full (RFC 7231, 7232, 5789) |
| **Status** | Legacy | Stable | Stable | Current |
| **Maintenance** | None | Fixes only | Fixes only | Active |

## Upgrade Timeline

```
2024
├─ v0 (Legacy - No support)
├─ v1 (Stable - Maintenance only)
└─ v2 (Stable - Maintenance only)

2025
├─ v1 (Stable - Maintenance only)
├─ v2 (Stable - Maintenance only)
└─ v3 (Released - Active development)

2026 (Current)
├─ v1 (Stable - Maintenance only)
├─ v2 (Stable - Maintenance only)
└─ v3 (Current - Recommended)

Future
├─ v1 (Stable - Maintenance only) [No sunset date]
├─ v2 (Stable - Maintenance only) [No sunset date]
└─ v3 (Stable - Maintenance only) [v4 when released]
```

## Migration Recommendations

**→ From v0 to v1/v2/v3**: Update endpoint URLs (immediate benefit)  
**→ From v1 to v2**: Update endpoint URLs + leverage enhanced OTmonitor  
**→ From v2 to v3**: Update endpoint URLs + use pagination, filtering, HATEOAS  
**→ From v1 to v3**: Recommended path (largest feature gain)

## Support

### v3 (Current)
🟢 **Full Support** - All features, bug fixes, improvements

### v1 & v2 (Stable)
🟡 **Maintenance** - Critical bug fixes only  
✅ **Still Works** - 100% operational  
⏰ **No Sunset Date** - Will continue indefinite support

### v0 (Legacy)
🔴 **No Support** - Deprecated  
⚠️ **Use v1/v2/v3 instead**

## Release Notes

For detailed release notes, see [RELEASE_NOTES_1.0.0.md](../../RELEASE_NOTES_1.0.0.md)

## Next Steps

- **Currently on v0?** → [Migration Guide](Migration-Guide)
- **Currently on v1?** → [Migration Guide](Migration-Guide) (optional)
- **Currently on v2?** → [Migration Guide](Migration-Guide) (optional)
- **Want v3 features?** → [Quick Start Guide](Quick-Start-Guide)
- **See examples?** → [example-api](../../example-api/)

---

**Summary**: v3 is the recommended version with modern REST architecture. v1 and v2 remain fully supported. Zero breaking changes. 🚀
