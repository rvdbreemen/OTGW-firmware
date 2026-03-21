# ADR-056: Protected Admin Endpoint Security and Secret-Handling Contract

**Status:** Proposed  
**Date:** 2026-03-21  
**Supersedes:** ADR-054 (fully); continues the partial supersession of ADR-032 for protected admin endpoints and secret-handling behavior

## Context

ADR-032 established the baseline OTGW security model: the device is a trusted-local-network appliance and does not require authentication on its general network interfaces. ADR-054 then added optional HTTP Basic Authentication for settings access.

The implemented firmware behavior now goes beyond ADR-054 in several important ways that need an explicit contract:

1. The protected surface is broader than "settings" and now includes multiple admin and maintenance operations.
2. The implementation uses a same-origin check for browser-style admin API requests after successful authentication.
3. Password fields follow a protected round-trip contract in the settings API so stored secrets are never returned in cleartext.
4. OTA update credentials are coupled to the same protected-endpoints password and are updated immediately when that password changes.
5. The design still lives inside ADR-003 and ADR-032 constraints: HTTP only, no TLS, trusted local network only.

Without a consolidated ADR, the repository leaves important operator and implementation guarantees split across ADR-032, ADR-054, API docs, and code comments.

## Decision

Define a single protected-admin contract for OTGW-firmware.

### 1. Protection boundary

A single optional password, stored in `settings.sHTTPpasswd`, defines the protected admin boundary.

When `settings.sHTTPpasswd` is empty:
- Admin protection is disabled.
- The firmware behaves like the ADR-032 baseline local-network mode.

When `settings.sHTTPpasswd` is non-empty:
- The fixed username is `admin`.
- The following routes and operations are protected:
  - `GET /api/v2/settings`
  - `POST /api/v2/settings`
  - `POST /upload`
  - File delete through the file-management delete query flow
  - `GET /ReBoot`
  - `GET /ResetWireless`
  - `GET /update`
  - `POST /update`
  - `POST` or `PUT /api/v2/webhook/test?state=...`

The following remain outside this protected boundary and continue to follow ADR-032:
- Health and flash-status reads
- OpenTherm data and device-info reads
- WebSocket streaming
- MQTT publishing
- Other local-network read paths that do not modify admin state or expose secret values

### 2. Authentication and same-origin contract

Protected admin requests use HTTP Basic Authentication with username `admin`.

For routes guarded through `checkHttpAuth()`:
- `HTTP OPTIONS` is allowed for preflight handling.
- A valid Basic Auth header is required when protection is enabled.
- If `Origin` or `Referer` is present, its host:port must match the request `Host` header exactly.
- A malformed `Origin` or `Referer` is rejected.
- A mismatched origin is rejected with HTTP 403.
- Requests without `Origin` or `Referer` are allowed for backward compatibility with non-browser or legacy clients.
- Requests without `Host` are also allowed because the server cannot validate origin in that case.

This same-origin check is not a session or token framework. It is a lightweight browser-focused CSRF mitigation layered on top of HTTP Basic Authentication for protected admin routes.

### 3. Password field round-trip contract

The settings API must never return the actual stored admin password or MQTT password.

For protected password fields:
- `GET /api/v2/settings` returns `password=N`, where `N` is the stored password length.
- `password=0` means no password is currently stored.
- `POST /api/v2/settings` with `notthispassword` preserves the existing stored value.
- `POST /api/v2/settings` with an empty string clears the stored value.
- Any other submitted string replaces the stored value.
- New admin password values are trimmed for leading and trailing whitespace before storage.

The documented contract is:
- Read: `password=N`
- Preserve on write: `notthispassword`
- Clear on write: empty string
- Replace on write: any other value

The implementation may continue to accept older placeholder aliases for compatibility, but the contract above is the canonical external behavior.

### 4. OTA credential propagation

The OTA update server uses the same protected-endpoints password as the rest of the admin boundary.

The firmware must:
- Apply the current `settings.sHTTPpasswd` to the OTA update server during WiFi/server startup
- Update OTA credentials immediately when the password changes
- Clear OTA credentials immediately when the password is cleared

This keeps `/update` aligned with the current admin-protection setting without requiring a reboot.

### 5. Local-network and HTTP-only constraints

This protection model is defense-in-depth inside the existing OTGW deployment model:
- HTTP only, never HTTPS
- Local-network appliance, not internet-facing
- No claim of transport confidentiality
- No claim of zero-trust or internet-safe authentication

Configured protection is intended to reduce accidental or casual administrative access on a trusted LAN, not to replace network isolation, WiFi security, router policy, or VPN access for remote administration.

## Alternatives Considered

### Alternative 1: Keep ADR-054 as-is

**Pros:**
- No new ADR needed
- Preserves the original "optional auth for settings" story

**Cons:**
- No longer matches the actual protected route surface
- Does not document CSRF behavior
- Does not document secret round-trip semantics
- Does not document OTA credential propagation
- Leaves operator-visible security behavior split across code and release notes

**Why not chosen:** ADR-054 is now too narrow to describe the implemented contract accurately.

### Alternative 2: Protect every interface and route

**Pros:**
- Simpler conceptual security boundary
- Stronger application-level access control

**Cons:**
- Breaks ADR-032 baseline behavior
- Would interfere with existing local-network integrations
- Adds friction to diagnostics and read-only monitoring paths
- Provides limited value without HTTPS/TLS

**Why not chosen:** OTGW remains a local-network device with open data interfaces by design.

### Alternative 3: Introduce sessions, cookies, CSRF tokens, and role-based auth

**Pros:**
- More conventional web-application security model
- More precise separation of browser and API clients

**Cons:**
- More RAM, code, and state complexity
- Harder to operate on ESP8266
- Misaligned with the project's embedded/local-network design constraints
- Still lacks transport confidentiality because HTTPS is out of scope

**Why not chosen:** Too heavy for ESP8266 and unnecessary for OTGW's local-network operating model.

## Consequences

### Positive

1. The protected admin boundary becomes explicit and testable.
2. Sensitive routes share one password and one user-facing mental model.
3. The settings API can preserve or clear secrets without ever echoing them back.
4. OTA protection stays synchronized with admin protection automatically.
5. Open monitoring and integration paths remain compatible with the ADR-032 local-network model.

### Negative

1. HTTP Basic Authentication still runs over plaintext HTTP.
2. The username is fixed to `admin`.
3. Same-origin validation is intentionally permissive for clients that do not send browser origin headers.
4. The protection boundary is selective rather than universal.
5. Some protected flows use route-specific authentication mechanics rather than a single uniform middleware model.

### Risks & Mitigation

**Risk 1:** Users interpret admin protection as making internet exposure safe.  
**Mitigation:** This ADR explicitly keeps ADR-003 and ADR-032 constraints in force: OTGW remains local-network only.

**Risk 2:** Browser and non-browser clients behave differently because origin headers are not always present.  
**Mitigation:** Document that same-origin enforcement is browser-oriented defense-in-depth, not universal client identity validation.

**Risk 3:** Secret-handling placeholder behavior drifts between UI, API docs, and implementation.  
**Mitigation:** This ADR defines one canonical round-trip contract and treats compatibility aliases as implementation detail.

## Related Decisions

- **ADR-003:** HTTP-Only Network Architecture (No HTTPS)
- **ADR-029:** Simple XHR-Based OTA Flash (KISS Principle)
- **ADR-032:** No Authentication Pattern (Local Network Security Model)
- **ADR-035:** RESTful API Compliance Strategy
- **ADR-050:** Centralized API Route Dispatch Table
- **ADR-054:** Optional HTTP Basic Authentication for Settings

## References

- `src/OTGW-firmware/restAPI.ino`
- `src/OTGW-firmware/settingStuff.ino`
- `src/OTGW-firmware/networkStuff.ino`
- `src/OTGW-firmware/FSexplorer.ino`
- `src/OTGW-firmware/OTGW-ModUpdateServer.h`
- `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h`
- `docs/api/README.md`
- `RELEASE_NOTES_1.3.0.md`
- `README.md`