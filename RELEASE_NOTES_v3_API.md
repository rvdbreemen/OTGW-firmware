# REST API v3 Release Notes

## Overview
REST API v3 introduces a resource-oriented API with HATEOAS discovery, improved error handling, pagination, and filtering while keeping full backward compatibility with v0/v1/v2.

## Highlights
- HATEOAS API discovery at `/api/v3`
- System, configuration, OTGW, sensors, export, and PIC resources
- Consistent JSON error responses
- Pagination and filtering for OTGW messages
- ETag support for cacheable responses
- Rate limiting with X-RateLimit-* headers
- OPTIONS/HEAD handling for CORS and preflight

## New v3 Endpoints (Summary)
**System**
- `GET /api/v3/system`
- `GET /api/v3/system/info`
- `GET /api/v3/system/health`
- `GET /api/v3/system/time`
- `GET /api/v3/system/network`
- `GET /api/v3/system/stats`

**Configuration**
- `GET /api/v3/config`
- `GET /api/v3/config/settings`
- `PATCH /api/v3/config/settings`
- `GET /api/v3/config/mqtt`
- `PATCH /api/v3/config/mqtt`
- `GET /api/v3/config/ntp`
- `PATCH /api/v3/config/ntp`
- `GET /api/v3/config/network`

**OTGW**
- `GET /api/v3/otgw`
- `GET /api/v3/otgw/status`
- `GET /api/v3/otgw/messages`
- `GET /api/v3/otgw/messages/{id}`
- `GET /api/v3/otgw/thermostat`
- `GET /api/v3/otgw/boiler`
- `GET /api/v3/otgw/commands`
- `POST /api/v3/otgw/commands`
- `POST /api/v3/otgw/autoconfigure`

**Sensors**
- `GET /api/v3/sensors`
- `GET /api/v3/sensors/dallas`
- `GET /api/v3/sensors/dallas/{address}`
- `GET /api/v3/sensors/s0`

**Export**
- `GET /api/v3/export`
- `GET /api/v3/export/telegraf`
- `GET /api/v3/export/otmonitor`
- `GET /api/v3/export/prometheus` (returns 501)

**PIC**
- `GET /api/v3/pic`
- `GET /api/v3/pic/firmware`
- `GET /api/v3/pic/flash/status`

## Backward Compatibility
- v0, v1, v2 endpoints remain unchanged.
- Existing clients will continue to work without modification.

## Migration Notes
- New integrations should use `/api/v3` and follow HATEOAS links.
- Configuration updates should use PATCH endpoints.

## Known Limitations
- Prometheus export is stubbed (501).
- Performance test suite not yet completed.

## Testing
- Use the v3 API quick start and reference documentation in the wiki.
- Example clients are available under `example-api/v3/`.

## Security & Deployment
- HTTP only (no TLS), per platform architecture.
- Intended for trusted local networks.
