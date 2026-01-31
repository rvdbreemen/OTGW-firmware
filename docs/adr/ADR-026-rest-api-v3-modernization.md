# ADR-026: REST API v3 Modernization

**Status:** Accepted  
**Date:** 2026-01-31

## Context
- The REST API already supports versioned endpoints (v0/v1/v2) for backward compatibility.
- v1 and v2 formats are optimized for specific clients but are less discoverable and harder to evolve without breaking changes.
- New integrations require richer metadata, consistent error handling, pagination, and conditional requests.
- Data export integrations need format-specific endpoints (Prometheus and Influx line protocol) without changing existing clients.
- ESP8266 constraints (RAM, flash, HTTP-only networking) require streaming responses and static buffers.

## Decision
- Introduce REST API v3 as a modernized, discoverable API while keeping v0/v1/v2 unchanged.
- Use hypermedia (HATEOAS) to expose links and relationships for discoverability.
- Support pagination and filtering for large collections to control response size.
- Support conditional requests via ETag to reduce bandwidth and improve client efficiency.
- Expose export endpoints for Prometheus text exposition and Influx line protocol.
- Keep the resource model version-scoped and consistent across responses and errors.

## Alternatives Considered
- **Evolve v2 with breaking changes** (rejected): would break existing clients and integrations.
- **Keep v2 and add ad-hoc endpoints** (rejected): discoverability and consistency would remain fragmented.
- **Use JSON for telemetry exports** (rejected): poorer compatibility with Prometheus/Influx tooling and larger payloads.

## Consequences
- **Positive:** Backward compatibility is preserved while enabling modern clients and standardized tooling.
- **Negative:** Additional documentation and testing effort for v3 alongside v0/v1/v2.
- **Risks & Mitigation:**
  - Increased flash/RAM usage from new handlers → mitigate via streaming responses and static buffers (ADR-004).
  - Client confusion between versions → mitigate via clear versioned paths and documentation.
  - HATEOAS overhead → mitigate by keeping link sets minimal and consistent.

## Related Decisions
- ADR-003 (HTTP-only, no HTTPS)
- ADR-004 (Static Buffer Allocation)
- ADR-005 (WebSocket Real-Time Streaming)
- ADR-009 (PROGMEM for String Literals)
- ADR-019 (REST API Versioning Strategy)

## References
- docs/api/v3/API_REFERENCE.md
- docs/api/v3/RESOURCE_MODEL.md
- restAPI_v3.ino