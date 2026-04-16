# Phase 1: Code Quality & Architecture Review

## Code Quality Findings

### High Severity

1. **`strstr()` on PROGMEM pointers -- platform assumption** (MQTTstuff.ino:1711-1724, 1842-1844)
   - `strstr(topicTemplate, "otgw-pic/")` where `topicTemplate` points into PROGMEM pool. Works on ESP8266 due to load-store exception handler for flash reads, and on ESP32 where PROGMEM is a no-op. But technically UB for libc. Neither `strstr` nor `strstr_P` is fully correct here (haystack is PROGMEM, not needle).
   - **Action**: Document as explicit platform assumption. No correct libc function exists for PROGMEM-haystack + RAM-needle without copying.

2. **`%s` format with PROGMEM pointer in debug log** (MQTTstuff.ino:1832)
   - `MQTTDebugTf(PSTR("Found PROGMEM entry for %d: [%s]\r\n"), OTid, topicTemplate)` -- same PROGMEM pointer issue. Works on ESP8266 but slow (exception per byte). Debug-only, acceptable.

### Medium Severity

3. **Stray/temporary files committed** (src/OTGW-firmware/$f, src/OTGW-firmware/networkStuff.h.tmp)
   - Editor/build artifacts. Neither referenced by any build or #include. Pollutes repo.
   - **Action**: Remove files, add `*.tmp` to `.gitignore`.

4. **Nightly restart has narrow 60s match window** (OTGW-firmware.ino:418-427)
   - Checks `hour == iRestartHour && minute == 0`. If the 60s timer drifts or main loop is delayed, the restart could be missed for that day. The `uptime > 3600` guard prevents restart loops.
   - **Action**: Acceptable with current guards. Document as latent fragility.

5. **Stale comment: says 12000 but value is 8000** (MQTTstuff.ino:46-50)
   - Comment references old threshold `12000` but `MQTT_DISCOVERY_HEAP_MIN = 8000`.
   - **Action**: Fix comment to match actual value.

6. **Duplicated source token detection block** (MQTTstuff.ino:1722-1724 and 1842-1844)
   - Identical 3-line `strstr` block in both `doAutoConfigure()` and `doAutoConfigureMsgid()`.
   - **Action**: Extract to a `detectSourceTokens()` helper.

7. **Stale `sLine[1200]` global buffer** (OTGW-firmware.h:89, 1002)
   - 1200 bytes of RAM. No longer used by MQTT autoconfig (switched to PROGMEM). Only remaining users appear to be in restAPI.ino for OT-direct JSON.
   - **Action**: Audit usage; reduce size or make local if only used by REST handler.

### Low Severity

8. **Inconsistent bitmap helper style** (MQTTstuff.ino:1487-1535) -- old uses `0b11100000`, new uses `0x07`. Cosmetic.
9. **`getMQTTConfigPending()` defined but never called** (MQTTstuff.ino:1524-1529) -- dead code, wastes flash.
10. **Direct access to timer macro internals** (MQTTstuff.ino:1574) -- `timerDiscoveryDrip_interval` couples to macro naming.
11. **No string deduplication in PROGMEM generator** (generate_mqttha_progmem.py) -- msg pool is 140KB, could shrink 30-50% with interning.

## Architecture Findings

### Critical Severity

1. **`strstr()` on PROGMEM pointers -- ESP32 portability risk** (MQTTstuff.ino:1711-1724)
   - Works on ESP8266 (exception handler) and ESP32 (PROGMEM=no-op), but if ESP32-C3/C6 (RISC-V) or future cores change PROGMEM behavior, these break silently.
   - **Action**: Use `strstr_P()` where possible, or add compile-time guard/static_assert for non-ESP8266/ESP32 platforms. At minimum, document the assumption.

### Medium Severity

2. **Unused Python struct format in generator** (generate_mqttha_progmem.py:28)
   - `ENTRY_FMT = '<BxHI'` is defined but never used (generator outputs C source, not binary). Creates false sense of verification.
   - **Action**: Remove dead code or add clarifying comment.

3. **Heap guard threshold inconsistency** (MQTTstuff.ino:50 vs helperStuff.ino:609)
   - `MQTT_DISCOVERY_HEAP_MIN=8000` vs `HEAP_WARNING_THRESHOLD=5120`. Comment says "keep in sync" but values differ intentionally (8000 is more conservative). Misleading comment.
   - **Action**: Clarify comment to document the intentional two-tier design.

4. **Code generator not integrated in build pipeline** (tools/generate_mqttha_progmem.py)
   - Generated files committed but `build.py` does not invoke the generator. If `mqttha.cfg` is edited without regenerating, firmware silently uses stale discovery data.
   - **Action**: Add CI check or pre-build step to verify generated files match source.

### Low Severity

5. **Nightly restart settings not in named sub-section per ADR-051** (OTGW-firmware.h:975-976) -- acceptable for two fields.
6. **Silent failure in drip discovery** (MQTTstuff.ino loopMQTTDiscovery) -- pending bit cleared on publish failure without counter/log. Recovered by next reconnect cycle.
7. **`doAutoConfigure()` iterates all 345 entries without early exit** (MQTTstuff.ino) -- intentional force-republish for manual trigger. No regression.

## Critical Issues for Phase 2 Context

- **PROGMEM pointer dereference pattern**: The `strstr()` / `%s` on flash pointers is the central platform concern. Security and performance review should evaluate whether this creates any exploitable behavior or measurable performance impact.
- **Heap thresholds**: Two different subsystems use different heap thresholds. Performance review should verify the 8000-byte threshold is appropriate for the MQTT discovery publish path.
- **Stale sLine buffer**: 1200 bytes of potentially reclaimable RAM on a 40KB platform.
