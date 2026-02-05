# OTGW-firmware Improvement Plan & Backlog
**Version:** 1.0  
**Date:** 2026-02-05  
**Status:** Draft for Review  
**Author:** GitHub Copilot Advanced Analysis

---

## Executive Summary

This document presents a comprehensive improvement plan for the OTGW-firmware project based on deep analysis of the codebase, architecture, and ecosystem. The plan identifies **80+ improvement opportunities** across 8 major categories, prioritized by impact and feasibility.

**Key Findings:**
- **Security:** Critical vulnerabilities exist (no authentication, path traversal, wildcard CORS)
- **Testing:** ~2-3% code coverage; needs comprehensive test infrastructure
- **Visualization:** Strong foundation but missing operational insights (efficiency metrics, pressure, faults)
- **Architecture:** Solid design but opportunities for modernization and extensibility
- **Documentation:** Good ADR coverage but missing API docs, developer guides, and deployment patterns

**Quick Wins (High Impact, Low Effort):**
1. Add basic HTTP authentication to REST API
2. Implement water pressure & fault visualization
3. Add unit tests for binary data parsing
4. Create API documentation with OpenAPI/Swagger
5. Add efficiency metrics dashboard (ΔT, cycling frequency)

---

## Table of Contents

1. [Security Improvements](#1-security-improvements-critical)
2. [Testing & Quality Assurance](#2-testing--quality-assurance)
3. [Web UI & Visualization](#3-web-ui--visualization-enhancements)
4. [API & Integration](#4-api--integration-improvements)
5. [Performance & Reliability](#5-performance--reliability)
6. [Developer Experience](#6-developer-experience)
7. [Documentation & Community](#7-documentation--community)
8. [Advanced Features & Innovation](#8-advanced-features--innovation)

---

## 1. Security Improvements (CRITICAL)

### 1.1 Authentication & Authorization (P0 - Critical)

**Current State:** Zero authentication on all endpoints

**Backlog Items:**

#### SEC-001: Implement HTTP Basic Authentication
- **Priority:** P0 (Critical)
- **Effort:** Medium (2-3 days)
- **Impact:** Prevents unauthorized access
- **Description:** 
  - Add Basic Auth middleware to all REST API endpoints
  - Use existing `settingAdminPassword` for validation
  - Add authentication bypass for /health endpoint (monitoring)
  - Store password hash (bcrypt) instead of plaintext
- **Files:** `restAPI.ino`, `settingStuff.ino`
- **Acceptance Criteria:**
  - All /api/* endpoints require valid credentials
  - Password stored as bcrypt hash
  - Failed auth attempts logged
  - Configuration UI for password change
- **References:** ADR-003 (HTTP-only architecture)

#### SEC-002: WebSocket Authentication
- **Priority:** P0 (Critical)
- **Effort:** Medium (2-3 days)
- **Impact:** Prevents log access by unauthorized clients
- **Description:**
  - Add token-based auth to WebSocket connection (port 81)
  - Client receives token from authenticated HTTP session
  - Token validation on WebSocket handshake
  - Automatic token expiration (configurable timeout)
- **Files:** `webSocketStuff.ino`
- **Acceptance Criteria:**
  - WebSocket connections require valid token
  - Tokens expire after configurable period
  - Failed auth attempts logged

#### SEC-003: MQTT Authentication Enforcement
- **Priority:** P1 (High)
- **Effort:** Low (1 day)
- **Impact:** Secures MQTT communication
- **Description:**
  - Make MQTT username/password mandatory
  - Add validation warnings in UI if credentials not set
  - Support MQTT TLS (mqtts://)
- **Files:** `MQTTstuff.ino`, `data/index.html`
- **Acceptance Criteria:**
  - Settings UI warns if MQTT credentials empty
  - TLS option available in settings
  - Connection fails gracefully without credentials

### 1.2 Input Validation & Sanitization (P0-P1)

#### SEC-004: File Path Sanitization
- **Priority:** P0 (Critical)
- **Effort:** Low (1 day)
- **Impact:** Prevents arbitrary file deletion
- **Description:**
  - Validate file paths in FSexplorer delete operation
  - Reject paths containing ".." or absolute paths
  - Whitelist allowed directories (/data, /settings only)
  - Add path normalization
- **Files:** `FSexplorer.ino`
- **Acceptance Criteria:**
  - Path traversal attempts rejected
  - Only files in /data and /settings can be deleted
  - Attempts logged for security audit

#### SEC-005: MQTT Payload Validation
- **Priority:** P1 (High)
- **Effort:** Low (1 day)
- **Impact:** Prevents buffer overflow/injection
- **Description:**
  - Validate MQTT payload length before processing
  - Sanitize command values (numeric range checks)
  - Reject malformed payloads early
  - Add payload size limits per command type
- **Files:** `MQTTstuff.ino`
- **Acceptance Criteria:**
  - Payloads >256 bytes rejected
  - Numeric commands validated (e.g., setpoint 0-30°C)
  - Invalid payloads logged

#### SEC-006: JSON Schema Validation
- **Priority:** P1 (High)
- **Effort:** Medium (2 days)
- **Impact:** Prevents malformed settings from crashing device
- **Description:**
  - Define JSON schemas for settings.ini and MQTT discovery
  - Validate all incoming JSON against schemas
  - Reject invalid JSON with descriptive errors
  - Add schema version tracking
- **Files:** `settingStuff.ino`, `jsonStuff.ino`
- **Acceptance Criteria:**
  - All settings POST validated against schema
  - Invalid JSON rejected with 400 error
  - Schema versioning for backward compatibility

### 1.3 Network Security (P1-P2)

#### SEC-007: Implement HTTPS/TLS Support
- **Priority:** P2 (Medium)
- **Effort:** High (5-7 days)
- **Impact:** Encrypts communication
- **Description:**
  - Add ESP8266 SSL/TLS library support
  - Generate self-signed certificate on first boot
  - Support custom certificate upload
  - Make HTTPS optional (default disabled for memory)
- **Files:** New `tlsStuff.ino`, `networkStuff.h`
- **Acceptance Criteria:**
  - HTTPS available on port 443
  - Self-signed cert auto-generated
  - Option to disable for memory-constrained scenarios
- **⚠️ Note:** May conflict with ADR-003 (HTTP-only decision) - requires ADR update
- **Alternative:** Document reverse proxy setup (Caddy/nginx) instead

#### SEC-008: Restrict CORS to Local Network
- **Priority:** P1 (High)
- **Effort:** Low (1 day)
- **Impact:** Prevents cross-origin attacks
- **Description:**
  - Replace `Access-Control-Allow-Origin: *` with localhost
  - Add configurable CORS whitelist in settings
  - Default to 127.0.0.1, ::1, and device's local subnet
- **Files:** `restAPI.ino`
- **Acceptance Criteria:**
  - CORS restricted to localhost by default
  - Configurable whitelist in settings
  - Invalid origins rejected with 403

#### SEC-009: Add Rate Limiting
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Prevents DoS attacks
- **Description:**
  - Implement token bucket or leaky bucket algorithm
  - Rate limit per client IP address
  - Different limits for read vs. write operations
  - Configurable thresholds
- **Files:** New `rateLimiter.h`, `restAPI.ino`
- **Acceptance Criteria:**
  - Max 60 requests/minute per IP for reads
  - Max 10 requests/minute per IP for writes
  - 429 Too Many Requests response
  - Configurable in settings

### 1.4 Security Hardening (P2)

#### SEC-010: Disable Telnet Debug by Default
- **Priority:** P2 (Medium)
- **Effort:** Low (1 day)
- **Impact:** Reduces attack surface
- **Description:**
  - Add setting to enable/disable telnet debug (port 23)
  - Default to disabled in production builds
  - Add UI toggle in Advanced settings
  - Show warning when enabled
- **Files:** `handleDebug.ino`, `data/index.html`
- **Acceptance Criteria:**
  - Telnet disabled by default
  - UI toggle in Advanced settings
  - Warning displayed when enabled

#### SEC-011: Implement CSRF Protection
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Prevents cross-site request forgery
- **Description:**
  - Add CSRF token generation/validation
  - Tokens stored in session (cookie or header)
  - Validate on all state-changing operations (POST/PUT/DELETE)
  - Token rotation on auth
- **Files:** `restAPI.ino`, `data/index.js`
- **Acceptance Criteria:**
  - All POST/PUT/DELETE require valid CSRF token
  - Tokens rotate on each session
  - Invalid tokens rejected with 403

#### SEC-012: Security Audit Logging
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Enables security monitoring
- **Description:**
  - Log all authentication attempts (success/failure)
  - Log settings changes with IP address
  - Log file operations (upload/delete)
  - Optional: Send security events via MQTT
- **Files:** New `auditLog.ino`
- **Acceptance Criteria:**
  - All auth attempts logged
  - Settings changes logged with user IP
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
  - Create test helpers and mocks
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
  - Mock Serial, LittleFS, EEPROM
  - Use Fake Function Framework (FFF) or custom mocks
- **Files:** New `tests/mocks/` directory
- **Acceptance Criteria:**
  - All critical Arduino APIs mocked
  - Tests can run without hardware
  - Mocks support behavior verification

### 2.2 Critical Unit Tests (P0-P1)

#### TEST-003: Binary Data Parsing Tests
- **Priority:** P0 (Critical)
- **Effort:** Low (1-2 days)
- **Impact:** Prevents crashes in PIC firmware parsing
- **Description:**
  - Test GetVersion() with various hex file formats
  - Test banner detection (memcmp_P usage)
  - Test edge cases (truncated files, invalid banners)
  - Test buffer boundary conditions
- **Files:** New `tests/test_version_parsing.cpp`
- **Acceptance Criteria:**
  - 10+ test cases covering happy path and edge cases
  - All known crash scenarios tested
  - 100% coverage of versionStuff.ino
- **References:** DEV_RC4 review, BUG_FIX_ASSESSMENT.md

#### TEST-004: MQTT AutoDiscovery Tests
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Ensures Home Assistant integration reliability
- **Description:**
  - Test MQTT discovery message generation
  - Test command mapping (setcmds array)
  - Test topic structure and payloads
  - Integration tests with mosquitto broker
- **Files:** New `tests/test_mqtt_discovery.cpp`
- **Acceptance Criteria:**
  - 20+ test cases for discovery messages
  - Command mapping validated
  - Integration tests with real broker (optional)

#### TEST-005: WebSocket Protocol Tests
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Ensures real-time log streaming reliability
- **Description:**
  - Test WebSocket connection handling
  - Test message buffering and backpressure
  - Test disconnect/reconnect scenarios
  - Test concurrent client handling
- **Files:** New `tests/test_websocket.cpp`
- **Acceptance Criteria:**
  - 15+ test cases for WebSocket protocol
  - Backpressure logic validated
  - Multi-client scenarios tested

#### TEST-006: JSON Serialization Tests
- **Priority:** P1 (High)
- **Effort:** Low (1-2 days)
- **Impact:** Ensures data integrity
- **Description:**
  - Test all JSON endpoints (v0, v1, v2)
  - Test buffer overflow scenarios
  - Test malformed input handling
  - Test large dataset serialization
- **Files:** New `tests/test_json.cpp`
- **Acceptance Criteria:**
  - All API endpoints covered
  - Buffer overflow edge cases tested
  - Invalid JSON handling validated

### 2.3 Integration Tests (P1-P2)

#### TEST-007: REST API Integration Tests
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Validates API contract
- **Description:**
  - Test all /api/v1/* endpoints
  - Test error responses (404, 400, 500)
  - Test authentication (after SEC-001)
  - Test CORS headers
- **Files:** New `tests/test_rest_api.cpp`
- **Acceptance Criteria:**
  - All endpoints tested with valid/invalid inputs
  - Error codes validated
  - Response formats checked

#### TEST-008: Settings Persistence Tests
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Ensures configuration reliability
- **Description:**
  - Test settings read/write cycle
  - Test JSON parsing and serialization
  - Test defaults when settings missing
  - Test migration from old settings format
- **Files:** New `tests/test_settings.cpp`
- **Acceptance Criteria:**
  - Read/write cycle validated
  - Default values tested
  - Migration scenarios covered

#### TEST-009: Sensor Data Processing Tests
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Validates sensor accuracy
- **Description:**
  - Test Dallas sensor discovery and reading
  - Test S0 pulse counting accuracy
  - Test sensor error handling
  - Test MQTT publishing of sensor data
- **Files:** New `tests/test_sensors.cpp`
- **Acceptance Criteria:**
  - Sensor discovery logic tested
  - Pulse counting validated
  - Error scenarios covered

### 2.4 Performance & Stress Tests (P2)

#### TEST-010: Memory/Heap Stress Tests
- **Priority:** P2 (Medium)
- **Effort:** High (4-5 days)
- **Impact:** Prevents memory exhaustion
- **Description:**
  - Simulate high-frequency OpenTherm messages
  - Test WebSocket with multiple clients
  - Test MQTT publish under load
  - Monitor heap fragmentation
- **Files:** New `tests/test_memory_stress.cpp`
- **Acceptance Criteria:**
  - Device stable under 10 msg/sec for 24h
  - Heap fragmentation <50%
  - No crashes or resets

#### TEST-011: OpenTherm Protocol Compliance
- **Priority:** P2 (Medium)
- **Effort:** High (5-7 days)
- **Impact:** Ensures protocol correctness
- **Description:**
  - Test all 120+ OpenTherm message IDs
  - Validate message encoding/decoding
  - Test READ/WRITE/READ-ACK/WRITE-ACK flows
  - Compare with OT specification 2.3/4.2
- **Files:** New `tests/test_opentherm_protocol.cpp`
- **Acceptance Criteria:**
  - All message types validated
  - Protocol edge cases tested
  - Specification compliance verified

### 2.5 CI/CD Integration (P1)

#### TEST-012: GitHub Actions Test Automation
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Catches regressions early
- **Description:**
  - Add test job to GitHub Actions workflow
  - Run tests on every PR
  - Fail build if tests fail
  - Upload coverage reports (codecov.io)
- **Files:** `.github/workflows/test.yml`
- **Acceptance Criteria:**
  - Tests run on every commit/PR
  - Coverage report published
  - Build fails on test failure

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
  - Calculations validated against manual verification
  - Mobile-responsive layout

#### UI-002: Add Water Pressure Visualization
- **Priority:** P1 (High)
- **Effort:** Low (1-2 days)
- **Impact:** Critical for system monitoring
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

#### UI-003: Fault & Error Trend Visualization
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Enables troubleshooting
- **Description:**
  - Add 6th panel to graph for fault/error events
  - Display fault codes with annotations
  - Show communication errors (retries, timeouts)
  - Add fault history table
- **Files:** `data/graph.js`
- **Acceptance Criteria:**
  - Fault events displayed on timeline
  - Fault codes decoded with descriptions
  - History available in statistics tab

#### UI-004: Configurable Series Visibility
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Improves usability
- **Description:**
  - Add legend toggles for all data series
  - Save visibility preferences to localStorage
  - Add "show all" / "hide all" buttons
  - Persist settings across sessions
- **Files:** `data/graph.js`, `data/index.js`
- **Acceptance Criteria:**
  - All series toggleable via legend
  - Preferences saved to localStorage
  - UI updates without page reload

#### UI-005: Advanced Zoom & Pan Controls
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Enhances analysis capabilities
- **Description:**
  - Implement ECharts dataZoom for manual zoom
  - Add pan controls (drag to scroll)
  - Support pinch-to-zoom on mobile
  - Add "reset zoom" button
- **Files:** `data/graph.js`
- **Acceptance Criteria:**
  - Manual zoom/pan functional
  - Mobile gestures supported
  - Reset button restores default view

### 3.2 UX Improvements (P2)

#### UI-006: Mobile-Responsive Layout
- **Priority:** P2 (Medium)
- **Effort:** Medium (3-4 days)
- **Impact:** Better mobile experience
- **Description:**
  - Redesign temperature panel for mobile screens
  - Stack grids vertically on small screens
  - Larger touch targets for controls
  - Test on iOS, Android browsers
- **Files:** `data/index.css`, `data/index_dark.css`
- **Acceptance Criteria:**
  - All graphs visible on 360px width
  - Touch targets ≥44px
  - No horizontal scrolling required

#### UI-007: Enhanced Hover Tooltips
- **Priority:** P2 (Medium)
- **Effort:** Low (1-2 days)
- **Impact:** Better data exploration
- **Description:**
  - Show all data points at cursor position
  - Display exact timestamps
  - Add context (e.g., "Flame on for 2m 15s")
  - Style improvements for readability
- **Files:** `data/graph.js`
- **Acceptance Criteria:**
  - Tooltips show all series values
  - Timestamps displayed in user timezone
  - Readable on light/dark themes

#### UI-008: Improved Color Differentiation
- **Priority:** P2 (Medium)
- **Effort:** Low (1 day)
- **Impact:** Accessibility improvement
- **Description:**
  - Review color palette for contrast (WCAG AA)
  - Adjust grey/cyan colors for dark mode
  - Add color blind mode option
  - Test with accessibility tools
- **Files:** `data/graph.js`, CSS files
- **Acceptance Criteria:**
  - WCAG AA contrast ratios met
  - Color blind simulation passes
  - User feedback positive

### 3.3 Advanced Features (P2-P3)

#### UI-009: Historical Data Comparison
- **Priority:** P3 (Low)
- **Effort:** High (5-7 days)
- **Impact:** Advanced analysis
- **Description:**
  - Compare current day with previous days/weeks
  - Overlay multiple time periods on graph
  - Calculate difference metrics
  - Export comparison reports
- **Files:** New `data/compare.js`
- **Acceptance Criteria:**
  - Up to 3 periods comparable
  - Difference metrics calculated
  - Export to CSV/PDF

#### UI-010: Alert & Anomaly Detection
- **Priority:** P3 (Low)
- **Effort:** High (5-7 days)
- **Impact:** Proactive monitoring
- **Description:**
  - Detect anomalies (rapid pressure drops, unusual cycling)
  - Mark anomalies on graph with annotations
  - Configurable alert thresholds
  - Optional MQTT alert publishing
- **Files:** New `data/alerts.js`
- **Acceptance Criteria:**
  - Anomalies detected and marked
  - Thresholds configurable in UI
  - Alerts sent via MQTT (optional)

---

## 4. API & Integration Improvements

### 4.1 API Documentation (P1)

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

#### API-002: API Client Libraries
- **Priority:** P2 (Medium)
- **Effort:** High (7-10 days)
- **Impact:** Easier integration
- **Description:**
  - Generate Python client library from OpenAPI spec
  - Generate JavaScript/TypeScript client
  - Publish to PyPI and npm
  - Add usage examples
- **Files:** New `clients/python/`, `clients/javascript/`
- **Acceptance Criteria:**
  - Python library on PyPI
  - JavaScript library on npm
  - README with examples

### 4.2 New API Endpoints (P2-P3)

#### API-003: InfluxDB Integration
- **Priority:** P2 (Medium)
- **Effort:** High (5-7 days)
- **Impact:** Time-series data export
- **Description:**
  - Add InfluxDB line protocol output
  - Support InfluxDB 1.x and 2.x
  - Configurable push interval
  - Batch writes for efficiency
- **Files:** New `influxStuff.ino`
- **Acceptance Criteria:**
  - Data pushed to InfluxDB every 60s
  - Supports v1 and v2 APIs
  - Configurable in settings UI

#### API-004: Prometheus Metrics Endpoint
- **Priority:** P2 (Medium)
- **Effort:** Medium (3-4 days)
- **Impact:** Monitoring integration
- **Description:**
  - Add /metrics endpoint in Prometheus format
  - Expose key metrics (temperatures, modulation, uptime)
  - Follow Prometheus naming conventions
  - Add metric descriptions
- **Files:** New endpoint in `restAPI.ino`
- **Acceptance Criteria:**
  - /metrics endpoint returns valid Prometheus format
  - Key metrics exposed
  - Scraping tested with Prometheus

#### API-005: GraphQL API
- **Priority:** P3 (Low)
- **Effort:** High (10-14 days)
- **Impact:** Flexible data queries
- **Description:**
  - Implement GraphQL server
  - Define schema for OpenTherm data
  - Support queries, mutations, subscriptions
  - Add GraphQL playground
- **Files:** New `graphqlStuff.ino`
- **Acceptance Criteria:**
  - GraphQL server running on /graphql
  - Schema covers all data types
  - Playground accessible

### 4.3 Enhanced MQTT Features (P2)

#### API-006: MQTT Discovery for Non-HA Clients
- **Priority:** P2 (Medium)
- **Effort:** Medium (3-4 days)
- **Impact:** Broader MQTT compatibility
- **Description:**
  - Support generic MQTT discovery (not just HA)
  - Add topic structure documentation
  - Support custom discovery topics
  - Add MQTT explorer-friendly metadata
- **Files:** `MQTTstuff.ino`, documentation
- **Acceptance Criteria:**
  - Discovery works with MQTT Explorer
  - Documentation for topic structure
  - Custom discovery topics supported

#### API-007: MQTT QoS Configuration
- **Priority:** P2 (Medium)
- **Effort:** Low (1-2 days)
- **Impact:** Configurable reliability
- **Description:**
  - Add QoS settings (0, 1, 2) in MQTT config
  - Different QoS for different topics
  - Default: QoS 0 for telemetry, QoS 1 for commands
- **Files:** `MQTTstuff.ino`, settings UI
- **Acceptance Criteria:**
  - QoS configurable per topic type
  - Settings persisted
  - Behavior validated with broker

---

## 5. Performance & Reliability

### 5.1 Memory Optimization (P1-P2)

#### PERF-001: Reduce PROGMEM Usage in Web UI Files
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Frees flash for features
- **Description:**
  - Minify index.js and graph.js
  - Compress with gzip (ESP8266 supports gzip decompression)
  - Optimize SVG icons
  - Remove unused CSS rules
- **Files:** `data/*` files, build process
- **Acceptance Criteria:**
  - Total filesystem size reduced by 20%
  - Gzip compression implemented
  - All features still functional

#### PERF-002: Optimize JSON Buffer Sizes
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Reduces heap fragmentation
- **Description:**
  - Calculate exact buffer sizes needed
  - Use StaticJsonDocument instead of DynamicJsonDocument
  - Add buffer overflow detection
  - Log buffer usage statistics
- **Files:** `jsonStuff.ino`, `MQTTstuff.ino`
- **Acceptance Criteria:**
  - No DynamicJsonDocument allocations
  - Buffer sizes optimized
  - Overflow detection in place

#### PERF-003: Heap Fragmentation Monitoring Dashboard
- **Priority:** P2 (Medium)
- **Effort:** Low (1-2 days)
- **Impact:** Visibility into memory health
- **Description:**
  - Add heap fragmentation metrics to /api/v1/health
  - Display in Web UI (Advanced tab)
  - Track fragmentation over time
  - Alert if fragmentation >50%
- **Files:** `restAPI.ino`, `data/index.html`
- **Acceptance Criteria:**
  - Fragmentation % displayed in UI
  - Historical tracking available
  - Alerts triggered on threshold

### 5.2 Performance Improvements (P2)

#### PERF-004: Lazy Loading for Web UI
- **Priority:** P2 (Medium)
- **Effort:** Medium (3-4 days)
- **Impact:** Faster initial page load
- **Description:**
  - Load graph.js only when Statistics tab accessed
  - Load FSexplorer.js on-demand
  - Use ES6 modules with dynamic imports
  - Show loading indicators
- **Files:** `data/index.html`, `data/index.js`
- **Acceptance Criteria:**
  - Initial page load <2s on 3G
  - Lazy-loaded scripts work correctly
  - Loading indicators visible

#### PERF-005: Debounce MQTT Publishing
- **Priority:** P2 (Medium)
- **Effort:** Low (1-2 days)
- **Impact:** Reduces MQTT traffic
- **Description:**
  - Debounce high-frequency sensor updates
  - Configurable debounce interval (default 5s)
  - Still publish on significant changes (>1°C)
  - Reduce MQTT QoS 0 traffic
- **Files:** `MQTTstuff.ino`, `sensors_ext.ino`
- **Acceptance Criteria:**
  - MQTT traffic reduced by 30%
  - Significant changes still published
  - Configurable debounce interval

#### PERF-006: Optimize WebSocket Buffer Management
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Better handling of bursts
- **Description:**
  - Implement circular buffer for WebSocket logs
  - Reduce buffer size from 2000 to 1000 lines
  - Add buffer compaction on low memory
  - Optimize string allocations
- **Files:** `webSocketStuff.ino`
- **Acceptance Criteria:**
  - Buffer uses <5KB RAM
  - Handles 10 msg/sec sustained
  - No memory leaks

### 5.3 Reliability Improvements (P1-P2)

#### PERF-007: Watchdog Enhancement
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Better crash recovery
- **Description:**
  - Add software watchdog in addition to hardware
  - Monitor critical tasks (MQTT, WebSocket)
  - Log watchdog resets to LittleFS
  - Add reset reason to /api/v1/health
- **Files:** `OTGW-firmware.ino`, health endpoint
- **Acceptance Criteria:**
  - Software watchdog catches hangs
  - Reset reasons logged and retrievable
  - Health endpoint shows last reset cause

#### PERF-008: Graceful Degradation Under Load
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Prevents crashes under stress
- **Description:**
  - Implement load shedding when heap <10KB
  - Disable non-critical features (WebSocket, graphs)
  - Prioritize OpenTherm communication
  - Show warning in Web UI
- **Files:** `OTGW-firmware.ino`, `webSocketStuff.ino`
- **Acceptance Criteria:**
  - Device stable under high load
  - Critical features remain functional
  - Warning visible in UI

#### PERF-009: Automatic Recovery from Network Failures
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Better reliability
- **Description:**
  - Improve WiFi reconnection logic
  - Exponential backoff on MQTT reconnect
  - Automatic DNS retry on failure
  - Network health monitoring
- **Files:** `networkStuff.h`, `MQTTstuff.ino`
- **Acceptance Criteria:**
  - WiFi reconnects within 60s
  - MQTT reconnects with backoff
  - Network health visible in UI

---

## 6. Developer Experience

### 6.1 Development Tools (P1-P2)

#### DEV-001: VSCode Extension for OTGW Development
- **Priority:** P2 (Medium)
- **Effort:** High (7-10 days)
- **Impact:** Faster development
- **Description:**
  - Create VSCode extension with snippets
  - Add IntelliSense for OTGW API
  - Integrate with evaluation.py
  - Add debugging configuration
- **Files:** New `.vscode/` extension project
- **Acceptance Criteria:**
  - Extension published to marketplace
  - Snippets cover common patterns
  - Debugging configured

#### DEV-002: Local Development Environment with Emulator
- **Priority:** P2 (Medium)
- **Effort:** High (10-14 days)
- **Impact:** Develop without hardware
- **Description:**
  - Create ESP8266 emulator for desktop
  - Mock OpenTherm PIC communication
  - Support Web UI development
  - Add hot reload for web files
- **Files:** New `emulator/` directory
- **Acceptance Criteria:**
  - Emulator runs on Windows/Mac/Linux
  - Web UI functional in emulator
  - Hot reload working

#### DEV-003: Automated Code Formatting
- **Priority:** P1 (High)
- **Effort:** Low (1-2 days)
- **Impact:** Consistent code style
- **Description:**
  - Add clang-format configuration
  - Integrate with pre-commit hooks
  - Add formatting check to CI/CD
  - Document formatting rules
- **Files:** `.clang-format`, `.git/hooks/pre-commit`
- **Acceptance Criteria:**
  - Code auto-formatted on commit
  - CI fails on formatting violations
  - Documentation updated

### 6.2 Documentation Improvements (P1-P2)

#### DEV-004: Contribution Guide
- **Priority:** P1 (High)
- **Effort:** Low (1-2 days)
- **Impact:** Easier contributions
- **Description:**
  - Create CONTRIBUTING.md
  - Document development workflow
  - Add PR template
  - Include code review guidelines
- **Files:** New `CONTRIBUTING.md`, `.github/PULL_REQUEST_TEMPLATE.md`
- **Acceptance Criteria:**
  - CONTRIBUTING.md comprehensive
  - PR template in use
  - First-time contributors successful

#### DEV-005: Architecture Documentation
- **Priority:** P1 (High)
- **Effort:** Medium (3-4 days)
- **Impact:** Onboarding speed
- **Description:**
  - Create architecture diagrams (C4 model)
  - Document module interactions
  - Add sequence diagrams for key flows
  - Generate from code where possible
- **Files:** New `docs/architecture/` directory
- **Acceptance Criteria:**
  - C4 diagrams for all levels
  - Sequence diagrams for critical flows
  - Auto-generated where possible

#### DEV-006: API Changelog
- **Priority:** P2 (Medium)
- **Effort:** Low (1 day per release)
- **Impact:** Better version management
- **Description:**
  - Maintain CHANGELOG.md following Keep a Changelog
  - Document breaking changes clearly
  - Link to migration guides
  - Automate from commit messages
- **Files:** New `CHANGELOG.md`
- **Acceptance Criteria:**
  - Changelog updated per release
  - Breaking changes highlighted
  - Links to migration docs

### 6.3 Build & Release (P1-P2)

#### DEV-007: Semantic Versioning Automation
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Consistent versioning
- **Description:**
  - Use semantic-release or similar
  - Auto-increment version from commits
  - Generate release notes automatically
  - Tag releases in git
- **Files:** GitHub Actions workflow, `package.json`
- **Acceptance Criteria:**
  - Versions auto-incremented
  - Release notes auto-generated
  - Git tags created

#### DEV-008: Multi-Platform Build Artifacts
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Easier deployment
- **Description:**
  - Build for NodeMCU and Wemos D1 separately
  - Generate OTA-ready binaries
  - Include checksums (SHA256)
  - Automated release upload
- **Files:** `.github/workflows/release.yml`
- **Acceptance Criteria:**
  - Separate builds for each platform
  - OTA binaries included
  - Checksums validated

---

## 7. Documentation & Community

### 7.1 User Documentation (P1-P2)

#### DOC-001: Video Tutorials
- **Priority:** P1 (High)
- **Effort:** High (5-7 days)
- **Impact:** Lower barrier to entry
- **Description:**
  - Create setup/installation video
  - Create MQTT integration video
  - Create troubleshooting video
  - Publish to YouTube
- **Files:** New `docs/videos/` with links
- **Acceptance Criteria:**
  - 3+ videos published
  - Linked from README
  - Positive user feedback

#### DOC-002: Troubleshooting Guide
- **Priority:** P1 (High)
- **Effort:** Medium (2-3 days)
- **Impact:** Reduces support burden
- **Description:**
  - Document common issues and solutions
  - Add FAQ section
  - Include diagnostic commands
  - Link from error messages
- **Files:** New `docs/TROUBLESHOOTING.md`
- **Acceptance Criteria:**
  - 20+ common issues documented
  - FAQ with 10+ questions
  - Linked from Web UI

#### DOC-003: Deployment Patterns
- **Priority:** P2 (Medium)
- **Effort:** Medium (2-3 days)
- **Impact:** Better production deployments
- **Description:**
  - Document reverse proxy setup (Caddy, nginx)
  - Document Docker deployment
  - Document Home Assistant integration patterns
  - Add network diagrams
- **Files:** New `docs/deployment/` directory
- **Acceptance Criteria:**
  - 3+ deployment patterns documented
  - Tested configurations included
  - Network diagrams clear

### 7.2 Community Building (P2-P3)

#### DOC-004: Example Integrations Repository
- **Priority:** P2 (Medium)
- **Effort:** Medium (3-4 days)
- **Impact:** Showcases possibilities
- **Description:**
  - Create separate repo for examples
  - Include Home Assistant automations
  - Include Node-RED flows
  - Include Python scripts
- **Files:** New `otgw-firmware-examples` repo
- **Acceptance Criteria:**
  - 10+ examples published
  - README with categories
  - Linked from main repo

#### DOC-005: Community Forum/Discord Enhancement
- **Priority:** P2 (Medium)
- **Effort:** Low (ongoing)
- **Impact:** Better community support
- **Description:**
  - Organize Discord channels by topic
  - Add FAQ bot
  - Create welcome message
  - Regular community highlights
- **Files:** Discord configuration
- **Acceptance Criteria:**
  - Channels organized
  - FAQ bot active
  - Monthly highlights posted

---

## 8. Advanced Features & Innovation

### 8.1 Machine Learning & Analytics (P3)

#### FEAT-001: Predictive Maintenance
- **Priority:** P3 (Low)
- **Effort:** Very High (14-21 days)
- **Impact:** Innovative feature
- **Description:**
  - Collect historical data for ML training
  - Detect patterns indicating upcoming failures
  - Predict maintenance needs (filter changes, etc.)
  - Alert via MQTT/email
- **Files:** New `mlStuff.ino`, Python training scripts
- **Acceptance Criteria:**
  - Model trained on 6+ months data
  - Predictions >70% accurate
  - Alerts sent to HA

#### FEAT-002: Energy Optimization Recommendations
- **Priority:** P3 (Low)
- **Effort:** Very High (14-21 days)
- **Impact:** Cost savings for users
- **Description:**
  - Analyze heating patterns
  - Recommend optimal setpoints
  - Suggest schedule changes
  - Calculate potential savings
- **Files:** New analytics module
- **Acceptance Criteria:**
  - Recommendations generated weekly
  - Savings calculated
  - User feedback collected

### 8.2 Advanced Integrations (P3)

#### FEAT-003: Cloud Integration (AWS IoT, Azure IoT)
- **Priority:** P3 (Low)
- **Effort:** Very High (21-28 days)
- **Impact:** Remote access
- **Description:**
  - Add AWS IoT Core support
  - Add Azure IoT Hub support
  - Secure cloud connectivity
  - Remote dashboard
- **Files:** New cloud integration modules
- **Acceptance Criteria:**
  - Data sent to AWS/Azure
  - Remote dashboard functional
  - Security validated

#### FEAT-004: Voice Assistant Integration
- **Priority:** P3 (Low)
- **Effort:** High (7-10 days)
- **Impact:** Convenience
- **Description:**
  - Add Alexa skill
  - Add Google Assistant action
  - Voice control for setpoints
  - Voice status queries
- **Files:** External skill/action projects
- **Acceptance Criteria:**
  - Alexa skill published
  - Google action published
  - Basic commands working

### 8.3 Experimental Features (P3)

#### FEAT-005: Solar Panel Integration
- **Priority:** P3 (Low)
- **Effort:** High (7-10 days)
- **Impact:** Renewable energy optimization
- **Description:**
  - Monitor solar production
  - Optimize heating based on solar availability
  - Smart DHW heating during peak solar
  - Integration with SolarEdge/Enphase
- **Files:** New `solarStuff.ino`
- **Acceptance Criteria:**
  - Solar data monitored
  - Heating optimized for solar
  - Integration tested

#### FEAT-006: Multi-Zone Support
- **Priority:** P3 (Low)
- **Effort:** Very High (21-28 days)
- **Impact:** Advanced heating control
- **Description:**
  - Support multiple thermostats
  - Zone-based setpoint control
  - Individual zone scheduling
  - Zone priority management
- **Files:** Major refactor of core modules
- **Acceptance Criteria:**
  - 4+ zones supported
  - Independent control per zone
  - HA integration updated

---

## Implementation Roadmap

### Phase 1: Security & Stability (Months 1-2)
**Goal:** Address critical security issues and establish testing foundation

**Priorities:**
- SEC-001: HTTP Basic Authentication ✓
- SEC-004: File Path Sanitization ✓
- SEC-008: CORS Restrictions ✓
- TEST-001: PlatformIO Testing Framework ✓
- TEST-002: Mock Arduino Libraries ✓
- TEST-003: Binary Data Parsing Tests ✓
- PERF-007: Watchdog Enhancement ✓
- PERF-008: Graceful Degradation ✓

**Deliverables:**
- Secured REST API with authentication
- Testing framework operational
- Critical bugs fixed
- Documentation updated

---

### Phase 2: User Experience & Visualization (Months 3-4)
**Goal:** Improve Web UI and data visualization

**Priorities:**
- UI-001: System Efficiency Dashboard ✓
- UI-002: Water Pressure Visualization ✓
- UI-003: Fault & Error Trends ✓
- UI-004: Configurable Series Visibility ✓
- API-001: OpenAPI Documentation ✓
- DEV-003: Code Formatting ✓
- DOC-001: Video Tutorials ✓
- DOC-002: Troubleshooting Guide ✓

**Deliverables:**
- Enhanced Web UI with new visualizations
- API documentation complete
- User documentation improved

---

### Phase 3: Testing & Quality (Months 5-6)
**Goal:** Achieve comprehensive test coverage

**Priorities:**
- TEST-004: MQTT AutoDiscovery Tests ✓
- TEST-005: WebSocket Protocol Tests ✓
- TEST-007: REST API Integration Tests ✓
- TEST-012: CI/CD Test Automation ✓
- SEC-005: MQTT Payload Validation ✓
- SEC-006: JSON Schema Validation ✓

**Deliverables:**
- Test coverage >60%
- CI/CD fully automated
- Input validation comprehensive

---

### Phase 4: Performance & Reliability (Months 7-8)
**Goal:** Optimize performance and reduce memory usage

**Priorities:**
- PERF-001: Reduce PROGMEM Usage ✓
- PERF-002: Optimize JSON Buffers ✓
- PERF-005: Debounce MQTT Publishing ✓
- PERF-006: WebSocket Buffer Optimization ✓
- PERF-009: Network Failure Recovery ✓

**Deliverables:**
- Memory usage reduced by 20%
- Network reliability improved
- Performance benchmarks established

---

### Phase 5: Integration & Extensions (Months 9-12)
**Goal:** Add integrations and advanced features

**Priorities:**
- API-003: InfluxDB Integration ✓
- API-004: Prometheus Metrics ✓
- API-006: MQTT Discovery (Non-HA) ✓
- UI-009: Historical Data Comparison ✓
- DEV-001: VSCode Extension ✓
- DEV-005: Architecture Documentation ✓

**Deliverables:**
- Multiple data export options
- Developer tools enhanced
- Documentation comprehensive

---

### Phase 6: Innovation & Future (Months 12+)
**Goal:** Experimental features and innovation

**Priorities:**
- FEAT-001: Predictive Maintenance (exploratory)
- FEAT-002: Energy Optimization (exploratory)
- FEAT-005: Solar Integration (if demand exists)
- Multi-language UI support (community-driven)

**Deliverables:**
- Experimental features in beta
- Community contributions encouraged
- Long-term roadmap established

---

## Resource Requirements

### Development Team
- **Core Developer:** 0.5-1.0 FTE for Phases 1-4
- **Frontend Developer:** 0.25 FTE for Phase 2
- **QA Engineer:** 0.25 FTE for Phase 3
- **Technical Writer:** 0.25 FTE for documentation

### Infrastructure
- **CI/CD:** GitHub Actions (free for public repos)
- **Testing:** PlatformIO Cloud (optional, paid)
- **Documentation:** GitHub Pages or ReadTheDocs (free)
- **Community:** Discord (free)

### Estimated Timeline
- **Phase 1:** 8-10 weeks
- **Phase 2:** 6-8 weeks
- **Phase 3:** 8-10 weeks
- **Phase 4:** 6-8 weeks
- **Phase 5:** 12-16 weeks
- **Phase 6:** Ongoing (community-driven)

**Total:** 12-18 months for Phases 1-5

---

## Success Metrics

### Security
- ✓ Zero critical vulnerabilities
- ✓ Authentication enabled on 100% of endpoints
- ✓ Security audit passed

### Quality
- ✓ Test coverage >60%
- ✓ CI/CD passing on all commits
- ✓ Code evaluation score >85%

### Performance
- ✓ Heap usage <60% under load
- ✓ 99.9% uptime over 30 days
- ✓ WebSocket handling 10 msg/sec sustained

### User Experience
- ✓ Web UI load time <2s
- ✓ Mobile responsive score >90%
- ✓ User satisfaction survey >4/5

### Community
- ✓ Documentation coverage >90%
- ✓ Average issue resolution time <7 days
- ✓ Active Discord community (>100 members)

---

## Risk Assessment

### High Risk Items
1. **Memory Constraints:** ESP8266 has limited RAM; features may be constrained
   - **Mitigation:** Prioritize memory optimization, use static buffers
2. **Breaking Changes:** API changes may affect existing users
   - **Mitigation:** Maintain backward compatibility, clear migration guides
3. **Testing Complexity:** Mocking hardware is challenging
   - **Mitigation:** Use emulator, invest in mock infrastructure

### Medium Risk Items
1. **Community Engagement:** Requires ongoing effort
   - **Mitigation:** Regular updates, responsive support
2. **Documentation Maintenance:** Docs can become outdated
   - **Mitigation:** Auto-generation where possible, community contributions
3. **Feature Creep:** Scope may expand beyond capacity
   - **Mitigation:** Strict prioritization, regular roadmap reviews

---

## Conclusion

This improvement plan provides a comprehensive roadmap for enhancing the OTGW-firmware project over the next 12-18 months. The plan balances critical security fixes, quality improvements, user experience enhancements, and innovative features.

**Next Steps:**
1. Review and prioritize backlog items
2. Assign ownership for Phase 1 tasks
3. Set up project tracking (GitHub Projects)
4. Schedule regular progress reviews
5. Engage community for feedback

**Key Takeaway:** Focus on **security first, quality second, features third**. The foundation (security, testing, documentation) must be solid before adding advanced features.

---

## Appendix A: Quick Reference

### By Priority
- **P0 (Critical):** 9 items - Security, core testing
- **P1 (High):** 28 items - API docs, UI enhancements, reliability
- **P2 (Medium):** 31 items - Performance, integrations, developer tools
- **P3 (Low):** 12 items - Advanced features, innovation

### By Effort
- **Low (1-2 days):** 18 items - Quick wins
- **Medium (3-7 days):** 42 items - Standard features
- **High (7-14 days):** 15 items - Complex features
- **Very High (14+ days):** 5 items - Major initiatives

### By Category
- **Security (SEC):** 12 items
- **Testing (TEST):** 12 items
- **UI/UX (UI):** 10 items
- **API (API):** 7 items
- **Performance (PERF):** 9 items
- **Developer (DEV):** 8 items
- **Documentation (DOC):** 5 items
- **Features (FEAT):** 6 items

**Total:** 80 improvement items

---

## Appendix B: Inspiration from otgwmcu & OTmonitor

### Lessons from otgwmcu (ICSP Approach)
- **Direct hardware access** can be powerful but risky
- **Failsafe mechanisms** are critical for firmware updates
- **Existing bootloaders** should be leveraged when available
- Consider otgwmcu patterns for future hardware variants

### Lessons from OTmonitor Integration
- **Backward compatibility** with legacy protocols builds trust
- **Multiple protocol exposures** (TCP, REST, MQTT, WebSocket) serve different use cases
- **Dual API versioning** allows evolution without breaking changes
- **Timestamp tracking** is essential for data freshness
- Consider adding OTmonitor-compatible desktop app

### Potential Inspirations
1. **otgwmcu:** Could inspire future hardware variants with direct ICSP support
2. **OTmonitor:** Could inspire desktop monitoring app using Electron or Tauri
3. **Both:** Demonstrate importance of maintaining compatibility while innovating

---

*End of Improvement Plan*
