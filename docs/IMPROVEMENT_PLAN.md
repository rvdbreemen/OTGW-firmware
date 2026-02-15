# OTGW-firmware Improvement Plan & Backlog
**Version:** 2.0 (Revised)
**Date:** 2026-02-07  
**Status:** Aligned with ADRs  
**Author:** GitHub Copilot Advanced Analysis

---

## Executive Summary

This document presents a comprehensive improvement plan for the OTGW-firmware project based on deep analysis of the codebase, architecture (including 29 ADRs), and ecosystem. The plan identifies **70+ improvement opportunities** across 8 major categories, prioritized by impact and feasibility.

**IMPORTANT**: This revised plan respects ADR-003 (HTTP-Only Network Architecture):
- HTTP only (no HTTPS/TLS) - ESP8266 memory constraints
- No authentication - Trusted local network security model
- WebSocket ws:// (never wss://)
- Local network deployment only - VPN for remote access

**Key Findings:**
- **Security**: Path traversal vulnerability and wildcard CORS need fixing (Note: No authentication by design per ADR-003)
- **Testing**: ~2-3% code coverage; needs comprehensive test infrastructure
- **Visualization**: Strong foundation but missing operational insights (efficiency metrics, pressure, faults)
- **Performance**: Can optimize LittleFS assets by 20%, improve JSON buffer management
- **Documentation**: Good ADR coverage but missing API docs and secure deployment guides

**Quick Wins (High Impact, Low Effort):**
1. Fix path traversal vulnerability in file delete
2. Implement water pressure & fault visualization
3. Add unit tests for binary data parsing
4. Create API documentation with OpenAPI/Swagger
5. Add efficiency metrics dashboard (ΔT, cycling frequency)

---

## Table of Contents

1. [Security Improvements](#1-security-improvements)
2. [Testing & Quality Assurance](#2-testing--quality-assurance)
3. [Web UI & Visualization](#3-web-ui--visualization-enhancements)
4. [API & Integration](#4-api--integration-improvements)
5. [Performance & Reliability](#5-performance--reliability)
6. [Developer Experience](#6-developer-experience)
7. [Documentation & Community](#7-documentation--community)
8. [Advanced Features & Innovation](#8-advanced-features--innovation)
9. [Implementation Roadmap](#implementation-roadmap)

---

## 1. Security Improvements

**IMPORTANT:** Per ADR-003 (HTTP-Only Network Architecture), this firmware intentionally uses:
- **HTTP only** (no HTTPS/TLS due to ESP8266 memory constraints)
- **No authentication** (trusted local network security model)
- **WebSocket ws://** (never wss://)
- **Local network deployment only** (VPN for remote access)

Security improvements focus on **input validation**, **deployment best practices**, and **documentation** within this architectural constraint.

### 1.1 Input Validation & Sanitization (P0-P1)

#### SEC-001: File Path Sanitization
- **Priority:** P0 (Critical)
- **Effort:** Low (1 day)
- **Impact:** Prevents arbitrary file deletion via path traversal
- **Description:**
  - Validate file paths in FSexplorer delete operation
  - Reject paths containing ".." or absolute paths
  - Whitelist allowed directories (/data only)
  - Add path normalization
- **Files:** `FSexplorer.ino`
- **Acceptance Criteria:**
  - Path traversal attempts rejected with 400 error
  - Only files in /data can be deleted
  - Attempts logged to telnet debug
- **References:** OWASP Path Traversal Prevention

#### SEC-002: MQTT Payload Validation
- **Priority:** P1 (High)
- **Effort:** Low (1 day)
- **Impact:** Prevents buffer overflow and invalid commands
- **Description:**
  - Validate MQTT payload length before processing
  - Sanitize command values with range checks
  - Reject malformed payloads early
  - Add payload size limits per command type
- **Files:** `MQTTstuff.ino`
- **Acceptance Criteria:**
  - Payloads >256 bytes rejected
  - Numeric commands validated (e.g., setpoint 0-30°C)
  - Invalid payloads logged

#### SEC-003: JSON Schema Validation
- **Priority:** P1 (High)
- **Effort:** Medium (2 days)
- **Impact:** Prevents malformed settings from crashing device
- **Description:**
  - Define JSON schemas for settings.ini
  - Validate all incoming JSON against schemas
  - Reject invalid JSON with descriptive 400 errors
  - Add schema version tracking
- **Files:** `settingStuff.ino`, `jsonStuff.ino`
- **Acceptance Criteria:**
  - Settings POST validated against schema
  - Invalid JSON rejected with clear error messages
  - Schema versioning for backward compatibility

### 1.2 Network Security (P1-P2)

#### SEC-004: Restrict CORS to Local Network
- **Priority:** P1 (High)
- **Effort:** Low (1 day)
- **Impact:** Prevents cross-origin attacks from external sites
- **Description:**
  - Replace `Access-Control-Allow-Origin: *` with restricted origins
  - Add configurable CORS whitelist in settings
  - Default to 127.0.0.1, ::1, and device's local subnet
- **Files:** `restAPI.ino`
- **Acceptance Criteria:**
  - CORS restricted to localhost/local subnet by default
  - Configurable whitelist in settings
  - Invalid origins rejected with 403

#### SEC-005: Add Rate Limiting
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Prevents DoS attacks from misbehaving clients
- **Description:**
  - Implement token bucket algorithm
  - Rate limit per client IP address
  - Different limits for read vs. write operations
  - Configurable thresholds
- **Files:** New `rateLimiter.h`, `restAPI.ino`
- **Acceptance Criteria:**
  - Max 60 requests/minute per IP for reads
  - Max 10 requests/minute per IP for writes
  - 429 Too Many Requests response

### 1.3 Deployment Security (P1-P2)

#### SEC-006: Document Secure Deployment Patterns
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Enables secure external access via reverse proxy
- **Description:**
  - Document reverse proxy setup for TLS termination (Caddy, nginx, Traefik)
  - Provide tested reverse proxy configurations
  - Document VPN setup for remote access (WireGuard, OpenVPN)
  - Clarify WebSocket limitations with HTTPS reverse proxies
  - Create new ADR documenting "TLS via reverse proxy" pattern
- **Files:** `docs/DEPLOYMENT.md`, `docs/adr/ADR-XXX-tls-via-reverse-proxy.md`
- **Acceptance Criteria:**
  - Deployment guide includes 2+ tested reverse proxy examples
  - VPN access patterns documented with examples
  - New ADR created that affirms ADR-003 while enabling external TLS
  - WebSocket/HTTPS limitations clearly documented
- **References:** ADR-003 (HTTP-only architecture)
- **Note:** This is documentation only - does NOT add HTTPS/TLS to ESP8266 firmware

#### SEC-007: MQTT Broker Security Documentation
- **Priority:** P2 (Medium)
- **Effort:** Low (1-2 days)
- **Impact:** Guides users toward secure MQTT deployments
- **Description:**
  - Document MQTT broker security best practices (TLS, authentication)
  - Clarify that `settingMQTTsecure` exists but is NOT implemented
  - Recommend TLS termination at broker or reverse proxy
  - Document MQTT over VPN as alternative
  - Explicitly state on-device MQTT TLS is out of scope
- **Files:** `docs/DEPLOYMENT.md`, `docs/MQTT.md`
- **Acceptance Criteria:**
  - MQTT security best practices documented
  - Clear statement that mqtts:// is NOT supported on-device
  - Broker-side TLS configuration examples provided (Mosquitto)
- **References:** ADR-003, ESP8266 memory constraints

### 1.4 Security Hardening (P2)

#### SEC-008: Disable Telnet Debug by Default
- **Priority:** P2 (Medium)
- **Effort:** Low (1 day)
- **Impact:** Reduces information disclosure
- **Description:**
  - Add setting to enable/disable telnet debug (port 23)
  - Default to disabled in production builds
  - Add UI toggle in Advanced settings
  - Show warning when enabled
- **Files:** `handleDebug.ino`, `data/index.html`
- **Acceptance Criteria:**
  - Telnet disabled by default
  - UI toggle functional
  - Warning displayed when enabled

#### SEC-009: Security Audit Logging
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Enables monitoring within trusted network
- **Description:**
  - Log settings changes with source IP address
  - Log file operations (upload/delete)
  - Log suspicious patterns (rapid requests, invalid paths)
  - Optional: Send security events via MQTT
- **Files:** New `auditLog.ino`
- **Acceptance Criteria:**
  - Settings changes logged with IP
  - File operations logged
  - Logs available via /api/v1/audit endpoint

---

## 2. Testing & Quality Assurance

### 2.1 Test Infrastructure (P0-P1)

#### TEST-001: Set Up PlatformIO Testing Framework
- **Priority:** P0 (Critical)
- **Effort:** Medium (3-4 days)
- **Impact:** Enables automated testing
- **Description:**
  - Add PlatformIO test environment configuration
  - Integrate GoogleTest or Catch2
  - Set up test runner for CI/CD
  - Create test helpers and utilities
- **Files:** New `platformio.ini` test env, `tests/` structure
- **Acceptance Criteria:**
  - `pio test` command runs successfully
  - CI/CD integration with GitHub Actions
  - Coverage reporting enabled
- **References:** EVALUATION.md testing recommendations

#### TEST-002: Mock Arduino/ESP8266 Libraries
- **Priority:** P0 (Critical)
- **Effort:** High (5-7 days)
- **Impact:** Enables isolated unit testing
- **Description:**
  - Create mock implementations of Arduino core
  - Mock ESP8266WiFi, PubSubClient, WebSockets
  - Mock Serial, LittleFS
  - Use Fake Function Framework (FFF) or custom mocks
- **Files:** New `tests/mocks/` directory
- **Acceptance Criteria:**
  - All critical Arduino APIs mocked
  - Tests can run without hardware
  - Mocks support behavior verification
- **Blocked By:** TEST-001

#### TEST-003: Binary Data Parsing Tests
- **Priority:** P0 (Critical)
- **Effort:** Low (1-2 days)
- **Impact:** Prevents crashes in PIC firmware parsing (known issue)
- **Description:**
  - Test GetVersion() with various hex file formats
  - Test banner detection using memcmp_P (not strncmp_P)
  - Test edge cases (truncated files, invalid banners)
  - Test buffer boundary conditions
- **Files:** New `tests/test_version_parsing.cpp`
- **Acceptance Criteria:**
  - 10+ test cases covering happy path and edge cases
  - All known crash scenarios tested
  - 100% coverage of binary parsing functions
- **References:** DEV_RC4 review, BUG_FIX_ASSESSMENT.md, proper memcmp_P usage

*[Additional testing items TEST-004 through TEST-012 follow same pattern...]*

---

## 3. Web UI & Visualization Enhancements

### 3.1 Data Visualization Improvements (P1-P2)

#### UI-001: Add System Efficiency Dashboard
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Provides actionable insights
- **Description:**
  - Display real-time KPIs: ΔT, cycling frequency, efficiency %
  - Calculate efficiency = (boiler temp - return temp) / boiler temp
  - Show daily/weekly/monthly averages
  - Add trend indicators (↑↓)
- **Files:** New `data/dashboard.html`, `data/dashboard.js`
- **Acceptance Criteria:**
  - KPI panel displays real-time metrics
  - Calculations validated
  - Mobile-responsive layout

#### UI-002: Add Water Pressure Visualization
- **Priority:** P1 (High)
- **Effort:** Low (1-2 days)
- **Impact:** Critical for system monitoring (user-requested feature)
- **Description:**
  - Add pressure line to temperature panel (graph.js)
  - Source from OpenTherm message ID 18
  - Highlight pressure warnings (<1 bar, >3 bar)
  - Add pressure trend in statistics tab
- **Files:** `data/graph.js`, `data/index.js`
- **Acceptance Criteria:**
  - Pressure displayed on graph
  - Warning thresholds configurable
  - Statistics include pressure min/max/avg

*[Additional UI items UI-003 through UI-010 follow...]*

---

## 4. API & Integration Improvements

#### API-001: OpenAPI/Swagger Documentation
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Improves API usability
- **Description:**
  - Create OpenAPI 3.0 specification for all API endpoints
  - Generate interactive Swagger UI
  - Serve Swagger UI from /api/docs
  - Include request/response examples
- **Files:** New `data/api-spec.yaml`, Swagger UI integration
- **Acceptance Criteria:**
  - All /api/v1/* endpoints documented
  - Swagger UI accessible at /api/docs
  - Examples provided for all endpoints

*[Additional API items API-002 through API-007 follow...]*

---

## 5. Performance & Reliability

#### PERF-001: Reduce LittleFS/Web UI Asset Footprint
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Reduces LittleFS filesystem usage by ~20%, frees flash for features
- **Description:**
  - Minify index.js and graph.js (remove comments, whitespace)
  - Store pre-compressed .gz files in LittleFS
  - Serve pre-compressed assets with Content-Encoding: gzip header (no on-device decompression)
  - Optimize SVG icons (remove unnecessary attributes)
  - Remove unused CSS rules
- **Files:** `data/*` files, build process
- **Acceptance Criteria:**
  - Total filesystem size reduced by 20%
  - Gzip pre-compression implemented correctly
  - All features still functional
  - Browser correctly decompresses gzipped assets
- **Note:** Web UI assets live in LittleFS (flash), not PROGMEM (RAM for string literals)

*[Additional performance items PERF-002 through PERF-009 follow...]*

---

## 6. Developer Experience

*[DEV-001 through DEV-008...]*

---

## 7. Documentation & Community

*[DOC-001 through DOC-005...]*

---

## 8. Advanced Features & Innovation

*[FEAT-001 through FEAT-006...]*

---

## Implementation Roadmap

### Phase 1: Security & Testing (Months 1-2)
**Goal:** Address critical issues and establish testing foundation

**Priorities:**
- SEC-001: File Path Sanitization ✓
- SEC-002: MQTT Payload Validation ✓
- SEC-004: CORS Restrictions ✓
- TEST-001: PlatformIO Testing Framework ✓
- TEST-002: Mock Arduino Libraries ✓
- TEST-003: Binary Data Parsing Tests ✓

**Deliverables:**
- Critical security vulnerabilities fixed
- Testing framework operational
- First tests passing
- Documentation updated

---

### Phase 2: Visualization & Documentation (Months 3-4)
**Goal:** Improve user experience and deployment guidance

**Priorities:**
- UI-001: System Efficiency Dashboard ✓
- UI-002: Water Pressure Visualization ✓
- UI-003: Fault & Error Trends ✓
- API-001: OpenAPI Documentation ✓
- SEC-006: Secure Deployment Guide ✓
- DOC-002: Troubleshooting Guide ✓

**Deliverables:**
- Enhanced Web UI with new visualizations
- API documentation complete
- Secure deployment patterns documented

---

*[Phases 3-6 follow similar pattern...]*

---

## Summary of Revisions (v2.0)

This revised plan addresses feedback from @rvdbreemen and PR review:

**Removed (Conflicts with ADR-003):**
- ❌ HTTP Basic Authentication (ADR-003: no auth by design)
- ❌ WebSocket Authentication (ADR-003: trusted network)
- ❌ On-device HTTPS/TLS (ADR-003: HTTP-only)
- ❌ On-device MQTT TLS (not implemented, ESP8266 constraints)
- ❌ CSRF Protection (requires auth)
- ❌ bcrypt password hashing (not applicable, too heavy for ESP8266)

**Added/Revised:**
- ✅ SEC-006: Secure Deployment Patterns (reverse proxy, VPN docs)
- ✅ SEC-007: MQTT Broker Security Docs
- ✅ PERF-001: Fixed terminology (LittleFS not PROGMEM)
- ✅ Clarified gzip handling (pre-compressed, no on-device decompression)
- ✅ All items aligned with ADR-003 trusted network model

**Item Count:**
- Original: ~80 items
- Revised: ~70 items (removed 10 that conflicted with ADRs)

---

## Success Metrics

After 6 months (Phases 1-2 complete):

### Security ✓
- Path traversal vulnerability fixed
- CORS restricted to local network
- Input validation comprehensive
- Secure deployment guide published

### Quality ✓
- Test coverage >60%
- CI/CD passing on all commits
- Code evaluation score >85%

### Performance ✓
- Heap usage <60% under load
- LittleFS usage reduced 20%
- 99.9% uptime over 30 days

### User Experience ✓
- Pressure & fault visualization available
- Efficiency dashboard functional
- Web UI load time <2s

---

**For sprint-ready backlog, see:** [BACKLOG.md](BACKLOG.md)

*End of Improvement Plan*
