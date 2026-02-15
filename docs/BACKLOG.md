# OTGW-firmware Development Backlog

**Last Updated:** 2026-02-07 (Revised)
**Status:** Aligned with ADRs

This backlog contains actionable items from the Improvement Plan, organized for sprint planning and GitHub Issues creation.

---

## Important: Alignment with ADR-003

**This backlog respects ADR-003 (HTTP-Only Network Architecture):**
- HTTP only (no HTTPS/TLS) - ESP8266 memory constraints
- No authentication - Trusted local network security model
- WebSocket ws:// (never wss://)
- Local network deployment only - VPN for remote access

Authentication and on-device TLS items have been **removed** from this backlog.

---

## Quick Win Items (High Impact, Low Effort)

These items deliver significant value with minimal development time. **Start here!**

| ID | Title | Priority | Effort | Impact | Labels |
|----|-------|----------|--------|--------|--------|
| SEC-001 | File Path Sanitization | P0 | Low (1d) | Critical | security, bug-fix, good-first-issue |
| SEC-004 | Restrict CORS to Local Network | P1 | Low (1d) | High | security, quick-win |
| TEST-003 | Binary Data Parsing Tests | P0 | Low (1-2d) | Critical | testing, bug-fix |
| UI-002 | Add Water Pressure Visualization | P1 | Low (1-2d) | High | ui, visualization, quick-win |
| SEC-002 | MQTT Payload Validation | P1 | Low (1d) | High | security, mqtt, good-first-issue |
| UI-008 | Improved Color Differentiation | P2 | Low (1d) | Medium | ui, accessibility, quick-win |
| DEV-003 | Automated Code Formatting | P1 | Low (1-2d) | High | developer-experience, quick-win |
| SEC-008 | Disable Telnet Debug by Default | P2 | Low (1d) | Medium | security, config |
| API-007 | MQTT QoS Configuration | P2 | Low (1-2d) | Medium | mqtt, config |
| DEV-006 | API Changelog | P2 | Low (1d) | Medium | documentation |

**Total Quick Wins:** 10 items, ~10-14 days effort

---

## Critical Priority (P0) - Must Do First

### SEC-001: File Path Sanitization
**Status:** Ready for Development  
**Priority:** P0 (Critical)  
**Effort:** Low (1 day)  
**Impact:** Prevents path traversal vulnerability  

**Description:**
Fix path traversal vulnerability in FSexplorer file delete operation.

**Tasks:**
- [ ] Add path normalization function
- [ ] Reject paths containing ".."
- [ ] Whitelist allowed directories (/data only)
- [ ] Log path traversal attempts to debug
- [ ] Write tests for path validation
- [ ] Update FSexplorer UI with validation feedback

**Files:** `FSexplorer.ino`  
**Labels:** security, bug-fix, good-first-issue  
**References:** OWASP Path Traversal Prevention

---

### TEST-001: Set Up PlatformIO Testing Framework
**Status:** Ready for Development  
**Priority:** P0 (Critical)  
**Effort:** Medium (3-4 days)  
**Impact:** Enables automated testing  

**Description:**
Establish comprehensive testing infrastructure using PlatformIO and GoogleTest.

**Tasks:**
- [ ] Create platformio.ini test environment
- [ ] Install GoogleTest/Catch2
- [ ] Set up test directory structure
- [ ] Create test helpers and utilities
- [ ] Configure coverage reporting (gcov)
- [ ] Document testing workflow in CONTRIBUTING.md

**Files:** New `platformio.ini`, `tests/` structure  
**Labels:** testing, infrastructure, developer-experience  
**References:** EVALUATION.md

---

### TEST-002: Mock Arduino/ESP8266 Libraries
**Status:** Blocked by TEST-001  
**Priority:** P0 (Critical)  
**Effort:** High (5-7 days)  
**Impact:** Enables isolated unit testing  

**Description:**
Create mock implementations of critical Arduino and ESP8266 libraries.

**Tasks:**
- [ ] Mock Arduino core (Serial, millis, delay, etc.)
- [ ] Mock ESP8266WiFi library
- [ ] Mock PubSubClient (MQTT)
- [ ] Mock WebSockets library
- [ ] Mock LittleFS filesystem
- [ ] Document mock usage patterns

**Files:** New `tests/mocks/` directory  
**Labels:** testing, infrastructure  
**Blocked By:** TEST-001

---

### TEST-003: Binary Data Parsing Tests
**Status:** Ready for Development  
**Priority:** P0 (Critical)  
**Effort:** Low (1-2 days)  
**Impact:** Prevents crashes in PIC firmware parsing (known bug)  

**Description:**
Comprehensive tests for hex file parsing using correct memcmp_P (not strncmp_P).

**Tasks:**
- [ ] Test GetVersion() with valid hex files
- [ ] Test banner detection using memcmp_P
- [ ] Test truncated file handling
- [ ] Test invalid banner handling
- [ ] Test buffer boundary conditions
- [ ] Add regression tests for known crashes

**Files:** New `tests/test_version_parsing.cpp`  
**Labels:** testing, bug-fix, good-first-issue  
**References:** DEV_RC4 review, BUG_FIX_ASSESSMENT.md, proper memcmp_P usage

---

## High Priority (P1) - Important Features

### Security (P1)

#### SEC-002: MQTT Payload Validation
**Effort:** Low (1 day) | **Impact:** High | **Labels:** security, mqtt, good-first-issue

**Tasks:**
- [ ] Validate payload length (max 256 bytes)
- [ ] Add numeric range checks (setpoint 0-30°C)
- [ ] Reject malformed payloads
- [ ] Log validation failures to debug

---

#### SEC-003: JSON Schema Validation
**Effort:** Medium (2 days) | **Impact:** High | **Labels:** security, validation

**Tasks:**
- [ ] Define JSON schemas for settings
- [ ] Implement schema validation
- [ ] Add 400 error responses for invalid JSON
- [ ] Version schemas for backward compatibility

---

#### SEC-004: Restrict CORS to Local Network
**Effort:** Low (1 day) | **Impact:** High | **Labels:** security, quick-win, good-first-issue

**Tasks:**
- [ ] Replace wildcard CORS with localhost/subnet
- [ ] Add configurable CORS whitelist
- [ ] Default to local network only
- [ ] Test with browsers

---

#### SEC-006: Document Secure Deployment Patterns
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** documentation, security, deployment

**Tasks:**
- [ ] Document reverse proxy TLS termination (Caddy, nginx)
- [ ] Provide tested configuration examples
- [ ] Document VPN access patterns (WireGuard, OpenVPN)
- [ ] Create ADR for "TLS via reverse proxy" pattern
- [ ] Clarify WebSocket/HTTPS reverse proxy limitations

**Note:** Documentation only - does NOT add HTTPS to ESP8266 firmware (per ADR-003)

---

### UI/UX (P1)

#### UI-001: System Efficiency Dashboard
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** ui, feature, visualization

**Tasks:**
- [ ] Design KPI panel layout
- [ ] Calculate ΔT efficiency
- [ ] Display cycling frequency
- [ ] Show daily/weekly/monthly averages
- [ ] Add trend indicators
- [ ] Make mobile-responsive

---

#### UI-002: Add Water Pressure Visualization
**Effort:** Low (1-2 days) | **Impact:** High | **Labels:** ui, visualization, quick-win

**Tasks:**
- [ ] Add pressure series to graph.js
- [ ] Source from OpenTherm message ID 18
- [ ] Add pressure warning thresholds
- [ ] Include in statistics tab
- [ ] Update documentation

---

### API (P1)

#### API-001: OpenAPI/Swagger Documentation
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** documentation, api

**Tasks:**
- [ ] Create OpenAPI 3.0 spec for all endpoints
- [ ] Add request/response schemas
- [ ] Include example payloads
- [ ] Integrate Swagger UI at /api/docs
- [ ] Document security model (no auth per ADR-003)

---

### Performance (P1)

#### PERF-001: Reduce LittleFS/Web UI Asset Footprint
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** performance, memory

**Tasks:**
- [ ] Minify JavaScript files (remove comments/whitespace)
- [ ] Create pre-compressed .gz files
- [ ] Serve with Content-Encoding: gzip header
- [ ] Optimize SVG icons
- [ ] Remove unused CSS rules
- [ ] Measure before/after sizes

**Note:** Web UI assets live in LittleFS (flash), not PROGMEM (RAM for string literals)

---

## Sprint Recommendations

### Sprint 1: Critical Fixes & Testing Setup (2 weeks)
**Goal:** Fix vulnerabilities and enable testing

**Items:**
- SEC-001: File Path Sanitization (1d)
- SEC-004: CORS Restrictions (1d)
- SEC-002: MQTT Payload Validation (1d)
- TEST-001: PlatformIO Framework (4d)
- TEST-003: Binary Parsing Tests (2d)

**Total:** ~9 days work

---

### Sprint 2: Testing Infrastructure (2 weeks)
**Goal:** Build comprehensive test coverage

**Items:**
- TEST-002: Mock Arduino Libraries (5d)
- TEST-004: MQTT AutoDiscovery Tests (3d)
- TEST-005: WebSocket Protocol Tests (2d)
- SEC-003: JSON Schema Validation (2d)

**Total:** ~12 days work

---

### Sprint 3: Visualization & Quick Wins (2 weeks)
**Goal:** Deliver visible improvements

**Items:**
- UI-002: Water Pressure Viz (2d)
- UI-003: Fault Trends (3d)
- UI-008: Color Improvements (1d)
- UI-007: Enhanced Tooltips (2d)
- DEV-003: Code Formatting (2d)
- SEC-008: Disable Telnet Default (1d)

**Total:** ~11 days work

---

### Sprint 4: Performance & Documentation (2 weeks)
**Goal:** Optimize and document

**Items:**
- PERF-001: LittleFS Asset Optimization (4d)
- PERF-002: JSON Buffer Optimization (3d)
- UI-001: Efficiency Dashboard (4d)
- SEC-006: Deployment Guide (3d)

**Total:** ~14 days work

---

### Sprint 5: API & Advanced Visualization (2 weeks)
**Goal:** Professional API and insights

**Items:**
- API-001: OpenAPI Documentation (4d)
- UI-003: Fault Trends (3d if not done in Sprint 3)
- SEC-007: MQTT Security Docs (2d)
- DEV-004: Contribution Guide (2d)
- UI-004: Configurable Series Visibility (3d)

**Total:** ~14 days work

---

### Sprint 6: Integrations & Reliability (2 weeks)
**Goal:** Extend capabilities

**Items:**
- API-003: InfluxDB Integration (4d)
- API-004: Prometheus Metrics (3d)
- PERF-007: Watchdog Enhancement (3d)
- PERF-008: Graceful Degradation (4d)

**Total:** ~14 days work

---

## Good First Issues

Items suitable for new contributors:

1. **SEC-001:** File Path Sanitization (Clear scope, input validation)
2. **SEC-002:** MQTT Payload Validation (Well-defined rules)
3. **SEC-004:** CORS Restrictions (Configuration change)
4. **TEST-003:** Binary Parsing Tests (Test writing practice)
5. **TEST-006:** JSON Tests (Good intro to testing)
6. **UI-002:** Water Pressure Viz (Frontend + data integration)
7. **UI-008:** Color Improvements (Design + accessibility)
8. **DEV-003:** Code Formatting (Tooling setup)
9. **DEV-006:** Changelog (Documentation practice)
10. **API-007:** MQTT QoS Config (Config + MQTT basics)

---

## Dependency Graph

Items that depend on others:

- **TEST-002** (Mock Libraries) → Requires **TEST-001** (Framework)
- **TEST-004-008** (Specific Tests) → Requires **TEST-001-002** (Infrastructure)
- **All Testing** → Benefits from **DEV-003** (Code Formatting)
- **API-002** (Client Libraries) → Requires **API-001** (OpenAPI Spec)
- **SEC-006** (Deployment Docs) → May reference **API-001** (API docs)

---

## Issue Template

Use this template when creating GitHub Issues from backlog items:

```markdown
## [ID]: [Title]

**Priority:** P0/P1/P2/P3  
**Effort:** Low/Medium/High  
**Impact:** Critical/High/Medium/Low  

### Description
[Description from backlog]

### Tasks
- [ ] Task 1
- [ ] Task 2
- [ ] ...

### Acceptance Criteria
- Criterion 1
- Criterion 2
- ...

### Files Affected
- `file1.ino`
- `file2.h`

### References
- Link to ADRs
- Link to related issues
- Link to documentation

### Labels
`label1`, `label2`, `label3`
```

---

## Revisions in v2.0

**Removed (Conflicts with ADR-003):**
- ❌ HTTP Basic Authentication  
- ❌ WebSocket Authentication
- ❌ On-device HTTPS/TLS  
- ❌ On-device MQTT TLS
- ❌ CSRF Protection
- ❌ bcrypt password hashing

**Added/Revised:**
- ✅ SEC-006: Secure Deployment Patterns (reverse proxy docs)
- ✅ SEC-007: MQTT Broker Security Docs
- ✅ PERF-001: Fixed terminology (LittleFS not PROGMEM)
- ✅ All items aligned with ADR-003

**Item Count:**
- Original: ~80 items
- Revised: ~70 items (removed 10 conflicting with ADRs)

---

*End of Backlog*
