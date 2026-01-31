# ADR-20260131-Rest-Api-V3-Modernization

## Status
Accepted

## Context
- The REST API already supports versioned endpoints (v0/v1/v2) for backward compatibility.
- v1 and v2 formats are optimized for specific clients but are less discoverable and harder to evolve without breaking changes.
- New integrations require richer metadata, consistent error handling, and modern client patterns (pagination, conditional requests).
- Data export integrations need format-specific endpoints (e.g., Prometheus and Influx line protocol) without breaking existing clients.
- Device constraints (ESP8266 RAM, HTTP-only networking) require streaming responses and careful buffer usage.

## Decision
- Introduce REST API v3 as a modernized, discoverable API while keeping v0/v1/v2 unchanged.
- v3 uses hypermedia (HATEOAS) to expose links and relationships for discoverability.
- v3 supports pagination and filtering for large collections to control response size.
- v3 supports conditional requests via ETag to reduce bandwidth and improve client efficiency.
- v3 exposes export endpoints for Prometheus text exposition and Influx line protocol, aligned with standard tooling.
- v3 responses and errors follow a consistent, version-scoped resource model to allow future extension.

## Consequences
- Backward compatibility is preserved because existing versions remain available.
- Documentation and testing burden increases (v3 must be tested in addition to v0/v1/v2).
- New integrations can use standardized formats and discoverability without custom client logic.
- Implementation must remain memory-safe and stream large payloads to avoid RAM pressure.

## Related
- docs/adr/ADR-019-rest-api-versioning-strategy.md
- docs/adr/ADR-003-http-only-no-https.md
- docs/adr/ADR-004-static-buffer-allocation.md
- docs/api/v3/API_REFERENCE.md
- docs/api/v3/RESOURCE_MODEL.md
