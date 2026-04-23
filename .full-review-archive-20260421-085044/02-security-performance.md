# Phase 2: Security & Performance Review

## Security Findings

### Medium Severity

**SEC-1: MQTT pending slot overwrite on re-entrancy (CWE-362)**
- **Files:** OTGW-Core.ino:245-261, MQTTstuff.ino:958-960
- **Issue:** Single pending slot for MQTT throttle deferral. `shouldPublishMQTTForID()` sets `mqttPendingSlot` expecting one `sendMQTTData()` to confirm it. If re-entrant `processOT()` fires (via `doBackgroundTasks()`), the slot can be overwritten. Bit/byte variants have defensive `pending = false` discard at entry; value slot does not.
- **Impact:** Under high OT rates with MQTT failures, throttle state could become inconsistent — causing duplicate publishes or missed updates. Not remotely exploitable but affects reliability.
- **Fix:** Add `mqttPendingSlot.pending = false` at top of `shouldPublishMQTTForID()` and `shouldPublishMQTTForPSField()`.

### Low Severity

**SEC-2: Direct serial write bypasses flash-in-progress guard (CWE-662)**
- **File:** OTGW-firmware.ino:410
- **Issue:** 60-second PIC probe (`PR=A`) doesn't check `state.flash.bPICactive`. Could corrupt PIC flash protocol if timing coincides with firmware upgrade.
- **Fix:** Add `!isFlashing()` guard.

**SEC-3: PIC upgrade filename path traversal (CWE-22, pre-existing)**
- **File:** OTGW-Core.ino:4628-4656
- **Issue:** `upgradepic()` uses `httpServer.arg("name")` in LittleFS path without sanitizing `../`. Pre-existing issue, not introduced by this changeset. Mitigated by HTTP auth and LittleFS path constraints.
- **Fix:** Reject filenames containing `/` or `..`.

**SEC-4: REST API conditional field omission (CWE-754)**
- **Files:** restAPI.ino:699-753, 882-1067
- **Issue:** PIC-related and unseen OT fields now conditionally omitted from JSON. Backwards-incompatible schema change within v2 API. `picavailable` is always present (correct discriminator).
- **Fix:** Document or use sentinel values for stable schema.

**SEC-5: `strstr_P` on text buffer — correct but needs clarifying comment**
- **File:** MQTTstuff.ino:1297
- **Issue:** Usage is safe (text data, not binary), but proximity to project guideline warnings could cause future confusion.

### Informational

**SEC-6: XSS via tooltip content — No issue found**
- Tooltip feature uses `setAttribute("title", ...)` with static lookup table. No injection vector.

**SEC-7: No authentication on GET endpoints — Accepted risk per LAN-only design**
- Consistent with project security model. New `picavailable` field is low-sensitivity.

### Positive Security Improvements

1. PIC availability guard pattern — defense-in-depth across 12+ entry points
2. MQTT throttle deferral — prevents state corruption on publish failure
3. Default `bOnline = false` — safer fail-closed default
4. REST 503 for PIC-disabled commands — correct HTTP semantics
5. Frontend PIC-only visibility — reduces interaction surface for non-functional features

### OWASP IoT Top 10 Assessment

| Category | Status |
|----------|--------|
| I1: Weak Passwords | Acceptable |
| I2: Insecure Network Services | Acceptable |
| I3: Insecure Ecosystem Interfaces | Low Risk (schema change) |
| I4: Lack of Secure Update | Pre-existing (no signature verification) |
| I7: Insecure Data Transfer | Accepted (HTTP-only by design) |
| I9: Insecure Default Settings | Improved (bOnline=false) |

## Performance Findings

### Overall Assessment: No material negative performance impact

The changes add minimal overhead (~20 bytes RAM, <2µs per cycle) while improving MQTT reliability and reducing REST response sizes.

### Positive Changes

**PERF+1: REST API response size reduction (~200-500 bytes)**
- Conditional omission of unseen OT fields reduces JSON payload, meaningful on ESP8266.

**PERF+2: `setMsgLastUpdated` now behind `is_value_valid` check**
- Avoids wasted switch-case lookup for invalid/skipped OT messages.

**PERF+3: -8 bytes static RAM from removed `processOT` variables**
- `epochGatewaylastseen` and `bOTGWgatewaypreviousstate` eliminated.

### Low Severity

**PERF-1: +20 bytes static RAM for pending throttle structs**
- Three new `MQTTPending*` structs. 0.05% of usable RAM. Acceptable.

**PERF-2: Double `getMsgLastUpdated()` in `sendOTmonitorV2`**
- ~30µs extra per REST call (15 fields × 2µs each). Optional micro-optimization: cache in local variable.

**PERF-3: 3× `confirmMQTTPublish*()` calls per `sendMQTTData`**
- ~0.3µs per publish (three bool checks). Negligible vs TCP I/O cost.

**PERF-4: `isPICEnabled()` ~20 inline bool reads per cycle**
- <2µs total. Compiler inlines to single load instruction.

**PERF-5: `applyPICAvailability()` DOM operations**
- ~10 DOM lookups, ~1ms, called 2-3x during page load. No layout thrashing.

### No Regressions Found

- No new Flash I/O in hot paths
- No new blocking operations
- No new memory leaks or unbounded allocations
- Watchdog timing unchanged (`feedWatchDog` still at same position)

## Critical Issues for Phase 3 Context

1. **No test infrastructure exists** — ESP8266 Arduino project without unit test framework. Security and correctness concerns (SEC-1, SEC-2) cannot be validated via automated tests.
2. **REST API schema change** (SEC-4) — Documentation review should check if API contract is documented anywhere for third-party consumers.
3. **Path traversal in PIC upgrade** (SEC-3, pre-existing) — Should be added to test backlog if testing is ever introduced.
