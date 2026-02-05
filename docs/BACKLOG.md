# OTGW-firmware Development Backlog

**Last Updated:** 2026-02-05  
**Status:** Active Planning

This backlog contains actionable items from the Improvement Plan, organized for sprint planning and GitHub Issues creation.

---

## How to Use This Backlog

1. **For Sprint Planning:** Pick items from "Ready for Development"
2. **For GitHub Issues:** Each item can become a GitHub Issue
3. **For Contributors:** Look for "Good First Issue" tags
4. **For Prioritization:** Items ordered by Priority (P0 > P1 > P2 > P3)

---

## Quick Win Items (High Impact, Low Effort)

These items deliver significant value with minimal development time. **Start here!**

| ID | Title | Priority | Effort | Impact | Labels |
|----|-------|----------|--------|--------|--------|
| SEC-008 | Restrict CORS to Local Network | P1 | Low (1d) | High | security, quick-win |
| SEC-010 | Disable Telnet Debug by Default | P2 | Low (1d) | Medium | security, config |
| UI-002 | Add Water Pressure Visualization | P1 | Low (1-2d) | High | ui, visualization |
| UI-007 | Enhanced Hover Tooltips | P2 | Low (1-2d) | Medium | ui, ux |
| UI-008 | Improved Color Differentiation | P2 | Low (1d) | Medium | ui, accessibility |
| TEST-003 | Binary Data Parsing Tests | P0 | Low (1-2d) | Critical | testing, bug-fix |
| SEC-004 | File Path Sanitization | P0 | Low (1d) | Critical | security, bug-fix |
| SEC-005 | MQTT Payload Validation | P1 | Low (1d) | High | security, mqtt |
| API-007 | MQTT QoS Configuration | P2 | Low (1-2d) | Medium | mqtt, config |
| PERF-003 | Heap Fragmentation Dashboard | P2 | Low (1-2d) | Medium | monitoring, ui |
| DEV-003 | Automated Code Formatting | P1 | Low (1-2d) | High | developer-experience |
| DEV-006 | API Changelog | P2 | Low (1d) | Medium | documentation |

**Total Quick Wins:** 12 items, ~12-18 days effort

---

## Critical Priority (P0) - Must Do First

Security and stability issues that must be addressed immediately.

### SEC-001: Implement HTTP Basic Authentication
**Status:** Ready for Development  
**Priority:** P0 (Critical)  
**Effort:** Medium (2-3 days)  
**Impact:** Prevents unauthorized access  

**Description:**
Add Basic Auth middleware to all REST API endpoints using existing `settingAdminPassword`.

**Tasks:**
- [ ] Add Basic Auth header validation
- [ ] Use bcrypt for password hashing
- [ ] Add authentication bypass for /health endpoint
- [ ] Add password change UI
- [ ] Add failed auth logging
- [ ] Write tests for auth middleware

**Files:** `restAPI.ino`, `settingStuff.ino`  
**Labels:** security, authentication, breaking-change  
**References:** ADR-003

---

### SEC-002: WebSocket Authentication
**Status:** Blocked by SEC-001  
**Priority:** P0 (Critical)  
**Effort:** Medium (2-3 days)  
**Impact:** Prevents log access by unauthorized clients  

**Description:**
Add token-based authentication to WebSocket connections on port 81.

**Tasks:**
- [ ] Generate session tokens after HTTP auth
- [ ] Validate tokens on WebSocket handshake
- [ ] Implement token expiration (configurable)
- [ ] Add token refresh mechanism
- [ ] Log failed WebSocket auth attempts
- [ ] Write tests for WebSocket auth

**Files:** `webSocketStuff.ino`  
**Labels:** security, websocket, authentication  
**Blocked By:** SEC-001

---

### SEC-004: File Path Sanitization
**Status:** Ready for Development  
**Priority:** P0 (Critical)  
**Effort:** Low (1 day)  
**Impact:** Prevents arbitrary file deletion  

**Description:**
Validate file paths in FSexplorer to prevent path traversal attacks.

**Tasks:**
- [ ] Add path normalization function
- [ ] Reject paths containing ".."
- [ ] Whitelist allowed directories
- [ ] Log path traversal attempts
- [ ] Write tests for path validation
- [ ] Update FSexplorer UI with validation

**Files:** `FSexplorer.ino`  
**Labels:** security, bug-fix, good-first-issue

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
- [ ] Configure coverage reporting
- [ ] Document testing workflow

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
**Status:** Ready for Development (can start before TEST-001)  
**Priority:** P0 (Critical)  
**Effort:** Low (1-2 days)  
**Impact:** Prevents crashes in PIC firmware parsing  

**Description:**
Comprehensive tests for hex file parsing to prevent Exception (2) crashes.

**Tasks:**
- [ ] Test GetVersion() with valid hex files
- [ ] Test banner detection (memcmp_P)
- [ ] Test truncated file handling
- [ ] Test invalid banner handling
- [ ] Test buffer boundary conditions
- [ ] Add regression tests for known bugs

**Files:** New `tests/test_version_parsing.cpp`  
**Labels:** testing, bug-fix, good-first-issue  
**References:** DEV_RC4 review, BUG_FIX_ASSESSMENT.md

---

## High Priority (P1) - Important Features

Features that significantly improve the project but aren't critical.

### Security (P1)

#### SEC-003: MQTT Authentication Enforcement
**Effort:** Low (1 day) | **Impact:** High | **Labels:** security, mqtt

**Tasks:**
- [ ] Make MQTT username/password mandatory
- [ ] Add validation warnings in UI
- [ ] Support MQTT TLS (mqtts://)
- [ ] Update documentation

---

#### SEC-005: MQTT Payload Validation
**Effort:** Low (1 day) | **Impact:** High | **Labels:** security, mqtt, good-first-issue

**Tasks:**
- [ ] Validate payload length (max 256 bytes)
- [ ] Add numeric range checks (setpoint 0-30°C)
- [ ] Reject malformed payloads
- [ ] Log validation failures

---

#### SEC-006: JSON Schema Validation
**Effort:** Medium (2 days) | **Impact:** High | **Labels:** security, validation

**Tasks:**
- [ ] Define JSON schemas for settings
- [ ] Implement schema validation
- [ ] Add 400 error responses for invalid JSON
- [ ] Version schemas for backward compatibility

---

#### SEC-008: Restrict CORS to Local Network
**Effort:** Low (1 day) | **Impact:** High | **Labels:** security, quick-win, good-first-issue

**Tasks:**
- [ ] Replace wildcard CORS with localhost
- [ ] Add configurable CORS whitelist
- [ ] Default to local subnet only
- [ ] Test with browsers

---

### Testing (P1)

#### TEST-004: MQTT AutoDiscovery Tests
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** testing, mqtt

**Tasks:**
- [ ] Test discovery message generation
- [ ] Test command mapping (setcmds array)
- [ ] Test topic structure validation
- [ ] Integration tests with mosquitto
- [ ] Document MQTT test patterns

---

#### TEST-005: WebSocket Protocol Tests
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** testing, websocket

**Tasks:**
- [ ] Test connection/disconnection
- [ ] Test message buffering
- [ ] Test backpressure handling
- [ ] Test multi-client scenarios
- [ ] Validate protocol compliance

---

#### TEST-006: JSON Serialization Tests
**Effort:** Low (1-2 days) | **Impact:** High | **Labels:** testing, good-first-issue

**Tasks:**
- [ ] Test all API endpoints (v0, v1, v2)
- [ ] Test buffer overflow scenarios
- [ ] Test malformed input
- [ ] Validate output formats

---

#### TEST-007: REST API Integration Tests
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** testing, api

**Tasks:**
- [ ] Test all /api/v1/* endpoints
- [ ] Test error responses (404, 400, 500)
- [ ] Test authentication flows
- [ ] Validate CORS headers

---

#### TEST-008: Settings Persistence Tests
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** testing

**Tasks:**
- [ ] Test read/write cycle
- [ ] Test JSON parsing
- [ ] Test default values
- [ ] Test settings migration

---

### UI/UX (P1)

#### UI-001: System Efficiency Dashboard
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** ui, feature, visualization

**Tasks:**
- [ ] Design KPI panel layout
- [ ] Calculate ΔT (delta-T) efficiency
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
- [ ] Add pressure warning thresholds (<1 bar, >3 bar)
- [ ] Include in statistics tab
- [ ] Update documentation

---

#### UI-003: Fault & Error Trend Visualization
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** ui, visualization, troubleshooting

**Tasks:**
- [ ] Add 6th panel for faults/errors
- [ ] Display fault codes with descriptions
- [ ] Show communication errors
- [ ] Add fault history table
- [ ] Implement annotations on timeline

---

### API (P1)

#### API-001: OpenAPI/Swagger Documentation
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** documentation, api

**Tasks:**
- [ ] Create OpenAPI 3.0 spec for all endpoints
- [ ] Add request/response schemas
- [ ] Include example payloads
- [ ] Integrate Swagger UI at /api/docs
- [ ] Auto-generate from code (optional)

---

### Performance (P1)

#### PERF-001: Reduce PROGMEM Usage in Web UI Files
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** performance, memory

**Tasks:**
- [ ] Minify JavaScript files
- [ ] Implement gzip compression
- [ ] Optimize SVG icons
- [ ] Remove unused CSS rules
- [ ] Measure before/after sizes

---

#### PERF-002: Optimize JSON Buffer Sizes
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** performance, memory

**Tasks:**
- [ ] Calculate exact buffer sizes needed
- [ ] Replace DynamicJsonDocument with Static
- [ ] Add buffer overflow detection
- [ ] Log buffer usage statistics
- [ ] Document buffer sizing decisions

---

#### PERF-007: Watchdog Enhancement
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** reliability, monitoring

**Tasks:**
- [ ] Add software watchdog
- [ ] Monitor critical tasks
- [ ] Log reset reasons to LittleFS
- [ ] Add reset info to /api/v1/health
- [ ] Document watchdog behavior

---

#### PERF-008: Graceful Degradation Under Load
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** reliability, performance

**Tasks:**
- [ ] Implement load shedding (heap <10KB)
- [ ] Disable non-critical features under load
- [ ] Prioritize OpenTherm communication
- [ ] Show warning in Web UI
- [ ] Add load testing scenarios

---

### Developer Experience (P1)

#### DEV-003: Automated Code Formatting
**Effort:** Low (1-2 days) | **Impact:** High | **Labels:** developer-experience, quick-win

**Tasks:**
- [ ] Add .clang-format configuration
- [ ] Set up pre-commit hooks
- [ ] Add CI formatting check
- [ ] Document formatting rules
- [ ] Update contribution guide

---

#### DEV-004: Contribution Guide
**Effort:** Low (1-2 days) | **Impact:** High | **Labels:** documentation, community

**Tasks:**
- [ ] Create CONTRIBUTING.md
- [ ] Document development workflow
- [ ] Add PR template
- [ ] Include code review guidelines
- [ ] Link from README

---

#### DEV-005: Architecture Documentation
**Effort:** Medium (3-4 days) | **Impact:** High | **Labels:** documentation, onboarding

**Tasks:**
- [ ] Create C4 model diagrams
- [ ] Document module interactions
- [ ] Add sequence diagrams for key flows
- [ ] Generate from code where possible
- [ ] Publish to GitHub Pages

---

### Documentation (P1)

#### DOC-001: Video Tutorials
**Effort:** High (5-7 days) | **Impact:** High | **Labels:** documentation, community

**Tasks:**
- [ ] Create setup/installation video
- [ ] Create MQTT integration video
- [ ] Create troubleshooting video
- [ ] Publish to YouTube
- [ ] Link from README and wiki

---

#### DOC-002: Troubleshooting Guide
**Effort:** Medium (2-3 days) | **Impact:** High | **Labels:** documentation, support

**Tasks:**
- [ ] Document 20+ common issues
- [ ] Create FAQ section
- [ ] Include diagnostic commands
- [ ] Link from error messages
- [ ] Add to wiki

---

## Medium Priority (P2) - Nice to Have

Features that improve the project but can be deferred.

### Security (P2)

- SEC-007: Implement HTTPS/TLS Support (High effort, may conflict with ADR-003)
- SEC-009: Add Rate Limiting
- SEC-010: Disable Telnet Debug by Default (Quick win!)
- SEC-011: Implement CSRF Protection
- SEC-012: Security Audit Logging

### Testing (P2)

- TEST-009: Sensor Data Processing Tests
- TEST-010: Memory/Heap Stress Tests
- TEST-011: OpenTherm Protocol Compliance Tests

### UI/UX (P2)

- UI-004: Configurable Series Visibility
- UI-005: Advanced Zoom & Pan Controls
- UI-006: Mobile-Responsive Layout
- UI-007: Enhanced Hover Tooltips (Quick win!)
- UI-008: Improved Color Differentiation (Quick win!)

### API (P2)

- API-002: API Client Libraries (Python, JavaScript)
- API-003: InfluxDB Integration
- API-004: Prometheus Metrics Endpoint
- API-006: MQTT Discovery for Non-HA Clients
- API-007: MQTT QoS Configuration (Quick win!)

### Performance (P2)

- PERF-003: Heap Fragmentation Monitoring Dashboard (Quick win!)
- PERF-004: Lazy Loading for Web UI
- PERF-005: Debounce MQTT Publishing
- PERF-006: Optimize WebSocket Buffer Management
- PERF-009: Automatic Recovery from Network Failures

### Developer (P2)

- DEV-001: VSCode Extension for OTGW Development
- DEV-002: Local Development Environment with Emulator
- DEV-006: API Changelog (Quick win!)
- DEV-007: Semantic Versioning Automation
- DEV-008: Multi-Platform Build Artifacts

### Documentation (P2)

- DOC-003: Deployment Patterns
- DOC-004: Example Integrations Repository
- DOC-005: Community Forum/Discord Enhancement

---

## Low Priority (P3) - Future/Innovation

Advanced features and experimental capabilities.

### Features (P3)

- FEAT-001: Predictive Maintenance (ML-based)
- FEAT-002: Energy Optimization Recommendations
- FEAT-003: Cloud Integration (AWS IoT, Azure IoT)
- FEAT-004: Voice Assistant Integration
- FEAT-005: Solar Panel Integration
- FEAT-006: Multi-Zone Support

### UI (P3)

- UI-009: Historical Data Comparison
- UI-010: Alert & Anomaly Detection

### API (P3)

- API-005: GraphQL API

---

## Sprint Recommendations

### Sprint 1: Security Foundation (2 weeks)
**Goal:** Address critical security vulnerabilities

**Items:**
- SEC-001: HTTP Basic Authentication (3d)
- SEC-004: File Path Sanitization (1d)
- SEC-008: CORS Restrictions (1d)
- SEC-005: MQTT Payload Validation (1d)
- Documentation updates (2d)

**Total:** ~8 days work

---

### Sprint 2: Testing Infrastructure (2 weeks)
**Goal:** Establish automated testing

**Items:**
- TEST-001: PlatformIO Framework (4d)
- TEST-002: Mock Libraries (5d)
- TEST-003: Binary Parsing Tests (2d)

**Total:** ~11 days work (allocate 2 weeks for complexity)

---

### Sprint 3: Security Part 2 & UI Quick Wins (2 weeks)
**Goal:** Complete core security and deliver visible improvements

**Items:**
- SEC-002: WebSocket Authentication (3d)
- UI-002: Water Pressure Viz (2d)
- UI-008: Color Improvements (1d)
- PERF-003: Heap Dashboard (2d)
- DEV-003: Code Formatting (2d)

**Total:** ~10 days work

---

### Sprint 4: Visualization & Monitoring (2 weeks)
**Goal:** Enhanced user insights

**Items:**
- UI-001: Efficiency Dashboard (4d)
- UI-003: Fault Trends (3d)
- UI-007: Better Tooltips (2d)
- PERF-007: Watchdog Enhancement (3d)

**Total:** ~12 days work

---

### Sprint 5: API & Documentation (2 weeks)
**Goal:** Professional API and docs

**Items:**
- API-001: OpenAPI Docs (4d)
- DOC-002: Troubleshooting Guide (3d)
- DEV-004: Contribution Guide (2d)
- DEV-005: Architecture Docs (4d)

**Total:** ~13 days work

---

### Sprint 6: Performance & Reliability (2 weeks)
**Goal:** Optimize and stabilize

**Items:**
- PERF-001: PROGMEM Optimization (4d)
- PERF-002: JSON Buffer Optimization (3d)
- PERF-008: Graceful Degradation (4d)
- TEST-004: MQTT Tests (3d)

**Total:** ~14 days work

---

## Good First Issues

Items suitable for new contributors:

1. **SEC-004:** File Path Sanitization (Low effort, clear scope)
2. **SEC-005:** MQTT Payload Validation (Clear validation rules)
3. **SEC-008:** CORS Restrictions (Well-defined change)
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

- **SEC-002** (WebSocket Auth) → Requires **SEC-001** (HTTP Auth)
- **TEST-002** (Mock Libraries) → Requires **TEST-001** (Framework)
- **TEST-004-008** (Specific Tests) → Requires **TEST-001-002** (Infrastructure)
- **All Testing** → Benefits from **DEV-003** (Code Formatting)
- **API-002** (Client Libraries) → Requires **API-001** (OpenAPI Spec)

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

## Tracking Progress

Update this backlog monthly with:
1. Items moved from "Ready" → "In Progress" → "Done"
2. New items discovered during development
3. Re-prioritization based on user feedback
4. Effort/impact reassessments

Use GitHub Projects or similar tool to track status visually.

---

*End of Backlog*
