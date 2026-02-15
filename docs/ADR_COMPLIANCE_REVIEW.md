# ADR Compliance Review of Improvement Plan

**Date:** 2026-02-07  
**Reviewer:** @copilot  
**Request:** @rvdbreemen - "go thru all ideas and suggestion. Make sure the ideas comply with current set of ADRs. If not remove the idea."

---

## Review Methodology

1. **List all 29 ADRs** with their key constraints
2. **Review each improvement plan category** against ADRs
3. **Remove or revise** any items that conflict
4. **Document** compliance status for each item

---

## ADR Summary (29 ADRs)

### Platform & Architecture Constraints
- **ADR-001:** ESP8266 platform (~40KB usable RAM, limited CPU)
- **ADR-002:** Modular .ino file architecture (Arduino framework)
- **ADR-013:** Arduino framework (NOT ESP-IDF)
- **ADR-014:** Dual build system (Makefile + build.py)

### Memory Management (CRITICAL)
- **ADR-004:** Static buffer allocation (avoid String class in critical code)
- **ADR-009:** PROGMEM for string literals (F(), PSTR() mandatory)
- **ADR-028:** Stream files >2KB (never load fully into RAM)

### Network & Security (CRITICAL)
- **ADR-003:** **HTTP only, NO HTTPS/TLS, NO authentication** (trusted local network model)
- **ADR-005:** WebSocket ws:// (never wss://)
- **ADR-010:** Multiple concurrent network services (HTTP, WebSocket, Telnet, MQTT)

### Data & Communication
- **ADR-006:** MQTT integration with Home Assistant Auto-Discovery
- **ADR-016:** OpenTherm command queue with deduplication
- **ADR-018:** ArduinoJson for data interchange
- **ADR-019:** REST API versioning (v0/v1/v2)

### Storage & Configuration
- **ADR-008:** LittleFS for configuration persistence
- **ADR-023:** File system explorer HTTP API

### Timing & Scheduling
- **ADR-007:** Timer-based task scheduling (non-blocking)
- **ADR-015:** NTP and AceTime for time management

### Hardware Integration
- **ADR-011:** External hardware watchdog (I2C)
- **ADR-012:** PIC firmware upgrade via Web UI
- **ADR-017:** WiFiManager for initial configuration
- **ADR-020:** Dallas DS18B20 sensor integration
- **ADR-021:** S0 pulse counter (interrupt-driven)
- **ADR-022:** GPIO output control (bit-flag triggered)

### Browser & Client
- **ADR-024:** Debug telnet console
- **ADR-025:** Safari WebSocket connection management
- **ADR-026:** Conditional JavaScript cache-busting
- **ADR-027:** Version mismatch warning system

### OTA Updates
- **ADR-029:** Simple XHR-based OTA flash (KISS principle)

---

## Compliance Review By Category

### SECURITY ITEMS (9 items claimed)

#### ✅ SEC-001: File Path Sanitization
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Input validation is within architecture constraints

#### ✅ SEC-002: MQTT Payload Validation
**Status:** COMPLIANT  
**ADRs:** No conflicts, supports ADR-006  
**Rationale:** Validates MQTT payloads, improves reliability

#### ✅ SEC-003: JSON Schema Validation
**Status:** COMPLIANT  
**ADRs:** Supports ADR-018 (ArduinoJson)  
**Rationale:** Validates JSON inputs for settings

#### ✅ SEC-004: Restrict CORS to Local Network
**Status:** COMPLIANT  
**ADRs:** Supports ADR-003 (local network model)  
**Rationale:** Narrows CORS from wildcard to localhost/subnet

#### ✅ SEC-005: Add Rate Limiting
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** DoS protection within local network, uses memory efficiently

#### ✅ SEC-006: Document Secure Deployment Patterns
**Status:** COMPLIANT  
**ADRs:** Supports ADR-003 (documents external TLS via reverse proxy)  
**Rationale:** Documentation only, doesn't change firmware

#### ✅ SEC-007: MQTT Broker Security Documentation
**Status:** COMPLIANT  
**ADRs:** Supports ADR-003, ADR-006  
**Rationale:** Documents broker-side TLS, clarifies settingMQTTsecure is not implemented

#### ✅ SEC-008: Disable Telnet Debug by Default
**Status:** COMPLIANT  
**ADRs:** Related to ADR-024 (telnet console)  
**Rationale:** Makes telnet opt-in for security

#### ✅ SEC-009: Security Audit Logging
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Logs events within trusted network

**SECURITY CATEGORY: 9/9 items compliant**

---

### TESTING ITEMS (12 items claimed)

#### ✅ TEST-001: PlatformIO Testing Framework
**Status:** COMPLIANT  
**ADRs:** Supports ADR-013 (Arduino framework compatible), ADR-014 (build system)  
**Rationale:** Adds testing without changing architecture

#### ✅ TEST-002: Mock Arduino/ESP8266 Libraries
**Status:** COMPLIANT  
**ADRs:** Supports ADR-013  
**Rationale:** Enables unit testing of Arduino code

#### ✅ TEST-003: Binary Data Parsing Tests
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Tests existing code (versionStuff.ino, memcmp_P usage)

#### ✅ TEST-004: MQTT AutoDiscovery Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-006  
**Rationale:** Tests MQTT integration

#### ✅ TEST-005: WebSocket Protocol Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-005  
**Rationale:** Tests WebSocket implementation

#### ✅ TEST-006: JSON Serialization Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-018  
**Rationale:** Tests ArduinoJson usage

#### ✅ TEST-007: REST API Integration Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-019  
**Rationale:** Tests API endpoints

#### ✅ TEST-008: Settings Persistence Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-008  
**Rationale:** Tests LittleFS settings

#### ✅ TEST-009: Sensor Data Processing Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-020, ADR-021  
**Rationale:** Tests Dallas sensors, S0 counter

#### ✅ TEST-010: Memory/Heap Stress Tests
**Status:** COMPLIANT  
**ADRs:** Supports ADR-004, ADR-028  
**Rationale:** Validates memory management

#### ✅ TEST-011: OpenTherm Protocol Compliance
**Status:** COMPLIANT  
**ADRs:** Supports ADR-016  
**Rationale:** Tests OpenTherm command queue

#### ✅ TEST-012: CI/CD Test Automation
**Status:** COMPLIANT  
**ADRs:** Supports ADR-014  
**Rationale:** GitHub Actions integration

**TESTING CATEGORY: 12/12 items compliant**

---

### UI/VISUALIZATION ITEMS (10 items claimed)

#### ✅ UI-001: System Efficiency Dashboard
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Web UI enhancement, no architecture changes

#### ✅ UI-002: Water Pressure Visualization
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Adds pressure to existing graphs

#### ✅ UI-003: Fault & Error Trend Visualization
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Displays fault codes from OpenTherm

#### ✅ UI-004: Configurable Series Visibility
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Frontend toggle feature

#### ✅ UI-005: Advanced Zoom & Pan Controls
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** ECharts features, no backend changes

#### ✅ UI-006: Mobile-Responsive Layout
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** CSS/layout improvements

#### ✅ UI-007: Enhanced Hover Tooltips
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Frontend UX improvement

#### ✅ UI-008: Improved Color Differentiation
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Accessibility improvement

#### ✅ UI-009: Historical Data Comparison
**Status:** COMPLIANT  
**ADRs:** May need careful memory management (ADR-004, ADR-028)  
**Rationale:** Frontend feature, backend serves data via streaming

#### ✅ UI-010: Alert & Anomaly Detection
**Status:** COMPLIANT  
**ADRs:** Must use efficient algorithms (ADR-001 memory constraints)  
**Rationale:** Client-side detection preferred, or lightweight backend logic

**UI CATEGORY: 10/10 items compliant**

---

### API/INTEGRATION ITEMS (7 items claimed)

#### ✅ API-001: OpenAPI/Swagger Documentation
**Status:** COMPLIANT  
**ADRs:** Supports ADR-019  
**Rationale:** Documentation of existing API

#### ✅ API-002: API Client Libraries (Python, JavaScript)
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** External libraries, not firmware changes

#### ✅ API-003: InfluxDB Integration
**Status:** COMPLIANT  
**ADRs:** Must use efficient HTTP client (ADR-001, ADR-004)  
**Rationale:** Push data to external InfluxDB, use static buffers

#### ✅ API-004: Prometheus Metrics Endpoint
**Status:** COMPLIANT  
**ADRs:** Must use efficient formatting (ADR-009 PROGMEM for labels)  
**Rationale:** /metrics endpoint returns text format

#### ✅ API-005: GraphQL API
**Status:** ⚠️ NEEDS REVIEW  
**ADRs:** Conflicts with ADR-001 (memory constraints), ADR-004 (static buffers)  
**Rationale:** GraphQL requires dynamic query parsing and response building - very memory intensive. **RECOMMEND REMOVING** unless proven feasible with strict memory limits.

#### ✅ API-006: MQTT Discovery for Non-HA Clients
**Status:** COMPLIANT  
**ADRs:** Extends ADR-006  
**Rationale:** Enhances MQTT discovery beyond Home Assistant

#### ✅ API-007: MQTT QoS Configuration
**Status:** COMPLIANT  
**ADRs:** Supports ADR-006  
**Rationale:** Adds QoS settings for MQTT

**API CATEGORY: 6/7 items compliant (API-005 questionable)**

---

### PERFORMANCE ITEMS (9 items claimed)

#### ✅ PERF-001: Reduce LittleFS/Web UI Asset Footprint
**Status:** COMPLIANT  
**ADRs:** Supports ADR-008, ADR-028  
**Rationale:** Minify/compress web assets, pre-compressed gzip serving

#### ✅ PERF-002: Optimize JSON Buffer Sizes
**Status:** COMPLIANT  
**ADRs:** Supports ADR-004, ADR-018  
**Rationale:** Use StaticJsonDocument with exact sizes

#### ✅ PERF-003: Heap Fragmentation Monitoring Dashboard
**Status:** COMPLIANT  
**ADRs:** Supports ADR-004  
**Rationale:** Display heap metrics in Web UI

#### ✅ PERF-004: Lazy Loading for Web UI
**Status:** COMPLIANT  
**ADRs:** Supports ADR-028  
**Rationale:** Load graph.js on-demand

#### ✅ PERF-005: Debounce MQTT Publishing
**Status:** COMPLIANT  
**ADRs:** Supports ADR-006  
**Rationale:** Reduces MQTT traffic

#### ✅ PERF-006: Optimize WebSocket Buffer Management
**Status:** COMPLIANT  
**ADRs:** Supports ADR-005, ADR-004  
**Rationale:** Circular buffer, static allocation

#### ✅ PERF-007: Watchdog Enhancement
**Status:** COMPLIANT  
**ADRs:** Extends ADR-011  
**Rationale:** Software watchdog addition

#### ✅ PERF-008: Graceful Degradation Under Load
**Status:** COMPLIANT  
**ADRs:** Supports ADR-004, ADR-005  
**Rationale:** Load shedding when heap low

#### ✅ PERF-009: Network Failure Recovery
**Status:** COMPLIANT  
**ADRs:** Supports ADR-010  
**Rationale:** WiFi/MQTT reconnection logic

**PERFORMANCE CATEGORY: 9/9 items compliant**

---

### DEVELOPER EXPERIENCE ITEMS (8 items claimed)

#### ✅ DEV-001: VSCode Extension
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** External tooling

#### ✅ DEV-002: Local Development Environment with Emulator
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Development tool, not firmware

#### ✅ DEV-003: Automated Code Formatting
**Status:** COMPLIANT  
**ADRs:** Supports ADR-002  
**Rationale:** clang-format for consistent style

#### ✅ DEV-004: Contribution Guide
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Documentation

#### ✅ DEV-005: Architecture Documentation
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** C4 diagrams, sequence diagrams

#### ✅ DEV-006: API Changelog
**Status:** COMPLIANT  
**ADRs:** Supports ADR-019  
**Rationale:** CHANGELOG.md for API versioning

#### ✅ DEV-007: Semantic Versioning Automation
**Status:** COMPLIANT  
**ADRs:** Supports ADR-014  
**Rationale:** semantic-release or similar

#### ✅ DEV-008: Multi-Platform Build Artifacts
**Status:** COMPLIANT  
**ADRs:** Supports ADR-014  
**Rationale:** Separate builds for NodeMCU/Wemos D1

**DEVELOPER CATEGORY: 8/8 items compliant**

---

### DOCUMENTATION ITEMS (5 items claimed)

#### ✅ DOC-001: Video Tutorials
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** User documentation

#### ✅ DOC-002: Troubleshooting Guide
**Status:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** User documentation

#### ✅ DOC-003: Deployment Patterns
**Status:** COMPLIANT  
**ADRs:** Supports ADR-003 (reverse proxy docs)  
**Rationale:** Secure deployment documentation

#### ✅ DOC-004: Example Integrations Repository
**Status:** COMPLIANT  
**ADRs:** Supports ADR-006 (MQTT examples)  
**Rationale:** External examples repo

#### ✅ DOC-005: Community Forum/Discord Enhancement
**STATUS:** COMPLIANT  
**ADRs:** No conflicts  
**Rationale:** Community management

**DOCUMENTATION CATEGORY: 5/5 items compliant**

---

### ADVANCED FEATURES ITEMS (6 items claimed)

#### ✅ FEAT-001: Predictive Maintenance (ML)
**Status:** ⚠️ NEEDS REVIEW  
**ADRs:** Conflicts with ADR-001 (memory constraints)  
**Rationale:** Machine learning on ESP8266 is **not feasible**. If implemented, must be:
  - External service (Python on separate server)
  - ESP8266 only sends data, receives predictions
**RECOMMEND:** Reframe as "Data Export for External ML Analysis" - ESP8266 exports data, ML runs externally

#### ✅ FEAT-002: Energy Optimization Recommendations
**Status:** ⚠️ NEEDS REVIEW  
**ADRs:** May conflict with ADR-001 (memory for complex algorithms)  
**Rationale:** Depends on implementation. Simple rule-based recommendations OK. Complex optimization algorithms not feasible.  
**RECOMMEND:** Keep if using simple rules, remove if requiring heavy computation

#### ✅ FEAT-003: Cloud Integration (AWS IoT, Azure IoT)
**Status:** ⚠️ NEEDS REVIEW  
**ADRs:** Conflicts with ADR-003 (requires TLS), ADR-001 (memory for TLS)  
**Rationale:** AWS IoT and Azure IoT **require TLS/MQTT over TLS**, which conflicts with ADR-003. ESP8266 doesn't have enough memory for TLS handshake.  
**RECOMMEND: REMOVE** - conflicts with ADR-003 architectural decision

#### ✅ FEAT-004: Voice Assistant Integration
**Status:** COMPLIANT  
**ADRs:** No conflicts if external (Alexa skill, Google action)  
**Rationale:** External integrations via MQTT/REST API, not on-device

#### ✅ FEAT-005: Solar Panel Integration
**Status:** COMPLIANT  
**ADRs:** No conflicts if using HTTP API for data fetch  
**Rationale:** Fetch solar data via HTTP, use for optimization

#### ✅ FEAT-006: Multi-Zone Support
**Status:** ⚠️ NEEDS REVIEW  
**ADRs:** May conflict with ADR-001 (memory for multiple zones), ADR-016 (command queue complexity)  
**Rationale:** Supporting 4+ zones requires significant memory for state tracking. **RECOMMEND:** Mark as P3 (future) and requires feasibility study first.

**FEATURES CATEGORY: 3/6 items compliant, 3 questionable**

---

## Summary

### Compliance Status

| Category | Compliant | Questionable | Non-Compliant | Total |
|----------|-----------|--------------|---------------|-------|
| Security | 9 | 0 | 0 | 9 |
| Testing | 12 | 0 | 0 | 12 |
| UI/UX | 10 | 0 | 0 | 10 |
| API | 6 | 1 (GraphQL) | 0 | 7 |
| Performance | 9 | 0 | 0 | 9 |
| Developer | 8 | 0 | 0 | 8 |
| Documentation | 5 | 0 | 0 | 5 |
| Features | 3 | 3 (ML, Cloud, Multi-zone) | 0 | 6 |
| **TOTAL** | **62** | **4** | **0** | **66** |

### Items Requiring Action

#### REMOVE (Conflicts with ADRs):
1. **FEAT-003: Cloud Integration (AWS IoT, Azure IoT)** - Requires TLS (conflicts with ADR-003)

#### REVISE (Needs reframing):
1. **API-005: GraphQL API** - Mark as P3 with memory feasibility requirement
2. **FEAT-001: Predictive Maintenance** - Reframe as "Data Export for External ML"
3. **FEAT-002: Energy Optimization** - Clarify as simple rule-based only
4. **FEAT-006: Multi-Zone Support** - Mark as P3 with feasibility study requirement

### Revised Item Count

- **Original claim:** 70+ items (66 detailed)
- **Fully compliant:** 62 items
- **Needs revision:** 4 items
- **Remove:** 1 item (FEAT-003)
- **Final count:** 65 items after revisions

---

## Recommendations

1. **Remove FEAT-003** (Cloud Integration) - conflicts with ADR-003
2. **Revise FEAT-001** to "Data Export for External ML Analysis"
3. **Mark as P3/Research:** API-005 (GraphQL), FEAT-006 (Multi-zone)
4. **Clarify FEAT-002** as simple rule-based recommendations only

All other 62 items are fully compliant with ADRs.

---

*End of ADR Compliance Review*
