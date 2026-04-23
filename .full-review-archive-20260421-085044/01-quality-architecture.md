# Phase 1: Code Quality & Architecture Review

## Code Quality Findings

### Medium Severity

**CQ-1: `sendMQTTStreaming` missing throttle confirm calls**
- **File:** MQTTstuff.ino:1021-1065
- **Issue:** `sendMQTTStreaming()` does not call `confirmMQTTPublishSlot()` / `confirmMQTTPublishBitSlot()` / `confirmMQTTPublishByteSlot()` on success. Currently only used for non-throttled topics (LWT, autoconfigure), but if throttle-gated data is ever routed through this path, pending slots will never confirm — causing infinite republish.
- **Fix:** Add confirm calls after successful `endPublish()`, or add a comment documenting the exclusion.

**CQ-2: `isGatewayFirmware()` silently disables PIC settings for non-gateway firmware**
- **File:** OTGW-firmware.h:505, line 607
- **Issue:** `queryNextPICsetting()` guard `if (!isPICEnabled() || !isGatewayFirmware()) return;` skips all PIC settings readout for monitor-mode firmware. This may be intentional but is undocumented.
- **Fix:** Add a comment explaining that PR= queries are only supported by gateway firmware.

**CQ-3: Direct serial write in 60s probe bypasses flash-in-progress guard**
- **File:** OTGW-firmware.ino:410-411
- **Issue:** `OTGWSerial.write("PR=A\r\n")` bypasses `addOTWGcmdtoqueue()` intentionally, but also bypasses the flashing check (`state.flash.bESPactive || state.flash.bPICactive`). Sending `PR=A` during PIC flashing could interfere with the flash protocol.
- **Fix:** Add a flash-in-progress guard before the serial write.

### Low Severity

**CQ-4: `bOnline` default changed from `true` to `false`**
- **File:** OTGW-firmware.h:146
- **Issue:** Users may see a brief "offline" blip on every reboot in HA dashboards until first OT message arrives. Behavioral change is correct but should be documented.

**CQ-5: Scattered `isPICEnabled()` guards across ~15 functions**
- **Files:** OTGW-Core.ino, restAPI.ino, MQTTstuff.ino (multiple locations)
- **Issue:** Defense-in-depth is good, but the enforcement boundaries vs. early-exit guards are not documented, making it easy to miss a guard when adding new PIC-dependent code.
- **Fix:** Document which functions are enforcement boundaries (addOTWGcmdtoqueue, sendOTGW, executeCommand).

**CQ-6: Double `getMsgLastUpdated()` evaluation in `sendOTmonitorV2`**
- **File:** restAPI.ino:642+
- **Issue:** Each conditional OT field calls `getMsgLastUpdated()` twice — once for the guard, once as parameter. Negligible performance impact but slightly wasteful.

**CQ-7: Frontend `applyPICAvailability` called with cached state in `refreshSettings`**
- **File:** index.js:4393
- **Issue:** Relies on `refreshDevInfo()` running before `refreshSettings()` to set `picAvailable`. Fragile ordering dependency, though currently correct.

## Architecture Findings

### Medium Severity

**AR-1: Single pending slot vulnerable to re-entrancy**
- **File:** OTGW-Core.ino:1425,1454 / MQTTstuff.ino pending slot structs
- **Issue:** Only one `mqttPendingSlot` exists. The contract is: `shouldPublishMQTTForID()` sets pending, then exactly one `sendMQTTData()` confirms it. If `doBackgroundTasks()` re-enters via `feedWatchDog()` → `yield()` → `handleOTGW()` → `processOT()`, another `shouldPublishMQTTForID()` could overwrite the pending slot before the outer publish confirms.
- **Note:** The bit/byte variants have a mitigation (`pending = false` at entry of `shouldPublishTrackedStatusBit`), but `shouldPublishMQTTForID` does NOT.
- **Fix:** Add `mqttPendingSlot.pending = false;` at the top of `shouldPublishMQTTForID()` and `shouldPublishMQTTForPSField()` to match the bit/byte pattern. Document the single-slot/single-threaded assumption.

### Low Severity

**AR-2: REST API conditional fields may break third-party parsers**
- **Files:** restAPI.ino (`sendDeviceInfoV2`, `sendOTmonitorV2`)
- **Issue:** PIC-related and unseen OT fields are now conditionally omitted from JSON responses. The frontend handles this correctly via `picavailable` discriminator, but third-party integrations expecting a fixed schema will see missing keys.
- **Fix:** Document conditional field behavior in REST API docs or changelog.

**AR-3: `queryNextPICsetting` guard correctly refined**
- **File:** OTGW-firmware.h:607
- **Issue:** Removed `bOnline` dependency (correct — PIC settings don't need OT bus traffic) and added `isGatewayFirmware()` check (correct — PR=* commands are gateway-specific). Good change.

### Positive Findings (Commendations)

**AR+1: PIC guard defense-in-depth is well-layered** — Guards at entry points, mid-level orchestrators, and low-level serial functions. ADR-060 documents the pattern.

**AR+2: Frontend `pic-only` CSS class pattern** — Clean separation of concerns using CSS-driven visibility toggling.

**AR+3: Architectural consistency maintained** — PROGMEM discipline, state architecture (ADR-051), command queue discipline, no String class in hot paths, proper ADR documentation.

## Critical Issues for Phase 2 Context

1. **MQTT throttle slot re-entrancy** (AR-1) — Security/performance reviewers should consider whether the `feedWatchDog()` yield window in `sendMQTTData` could cause message duplication or lost updates under load.
2. **Direct serial write during flash** (CQ-3) — Could have safety implications if PIC firmware upgrade is in progress.
3. **REST API schema changes** (AR-2) — May affect integrations; security reviewer should check if missing fields could cause null-reference issues in consumers.
