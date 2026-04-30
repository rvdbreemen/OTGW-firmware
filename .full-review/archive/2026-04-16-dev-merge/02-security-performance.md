# Phase 2: Security & Performance Review

## Security Findings

### Medium Severity

1. **`strstr()` on PROGMEM pointers -- undefined behavior** (CWE-758, MQTTstuff.ino:1711-1724, 1842-1844)
   - Works on ESP8266 (exception handler) and ESP32 (PROGMEM=no-op). Not exploitable but correctness risk on future platforms/SDK updates.
   - Also affects `renderTemplateToBuffer()`, `sendMQTTTemplateStreaming()`, and `writeMqttChunk()` which pass PROGMEM pointers where RAM expected.
   - `writeMqttProgmemChunk()` (line 282) already exists as the correct pattern but is unused in the streaming path.

2. **Generator not in build pipeline** (CWE-1104 adapted, tools/generate_mqttha_progmem.py)
   - No automated staleness check. Editing `mqttha.cfg` without regenerating silently uses stale discovery data. No hash verification between source and generated output.

### Low Severity

3. **Stray files committed** (CWE-540, networkStuff.h.tmp, $f) -- hygiene issue, no credentials exposed.
4. **Nightly restart auth/timing** (CWE-400) -- properly auth-gated, input validated, narrow timing window. Within trusted-LAN threat model.
5. **Template rendering trusts null terminator** (CWE-120) -- safe under normal conditions (generator adds \0). Corrupted build artifact could leak flash content to MQTT. Not remotely exploitable.
6. **Dead `sLine[1200]` buffer** (CWE-561) -- 1200 bytes wasted RAM, not a security issue but reclaim opportunity.
7. **`delay(200)` before restart, no settings flush** -- pending settings write could be lost. Nightly restart is deliberate, so impact minimal.

### Positive Findings (Informational)

- MQTT drip rate limiting: well-designed, no DoS concern. One publish per tick, timer-gated, heap-guarded.
- Bitmap bounds checking: correct, no out-of-bounds possible (uint8_t input, 8-element array).
- REST API auth for new settings: properly secured, consistent with existing patterns.
- Input validation: `nightlyrestarthour` correctly clamped 0-23, `nightlyrestart` uses EVALBOOLEAN.

## Performance Findings

### High Severity

1. **`strstr()` on PROGMEM triggers exception handler -- ~60-120ms per entry** (MQTTstuff.ino:1711-1724, 1842-1844)
   - Every unaligned byte access on ESP8266 PROGMEM costs ~20-40us (Xtensa LoadStoreErrorCause exception handler).
   - 6 strstr calls per entry x ~500 avg chars = ~3000 byte accesses = ~60-120ms per entry.
   - `doAutoConfigure()` (345 entries): **~20-40 seconds** in exception handler overhead.
   - `doAutoConfigureMsgid()` (drip, 1 entry): ~60-120ms per invocation (tolerable at 3s intervals).
   - Same issue in `renderTemplateToBuffer()`, `sendMQTTTemplateStreaming()`, `tryGetTemplateReplacement()`.
   - **Fix**: Add precomputed `flags` field to `MqttHaCfgEntry` (isPIC, isOTDirect, hasSourceTokens). Generator precomputes. Eliminates all 6 strstr calls per entry. For template rendering: use `writeMqttProgmemChunk` pattern for PROGMEM literal spans.

### Medium Severity

2. **17-minute discovery window** (MQTTstuff.ino loopMQTTDiscovery)
   - 345 entries at 3s each = ~17 minutes for full HA discovery. Under heap pressure (30s): ~2.8 hours.
   - Old JIT approach discovered ~60 active IDs in ~2 minutes as OT traffic arrived.
   - **Fix**: Reduce normal interval to 1s (6 minutes), or hybrid JIT+drip for active IDs.

3. **sLine[1200] now reclaimable** (OTGW-firmware.h:1002)
   - 1200 bytes = 3% of 40KB RAM budget. Only remaining user: restAPI.ino OTDirect JSON.
   - **Fix**: Remove or shrink; use local buffer in REST handler.

4. **sendMQTTTemplateStreaming uses writeMqttChunk on PROGMEM** (MQTTstuff.ino:346)
   - `MQTTclient.write()` receives PROGMEM pointer where RAM expected. Works via exception handler but slow.
   - **Fix**: Use existing `writeMqttProgmemChunk()` (line 282) which correctly stages via RAM buffer.

### Low Severity

5. **getHeapHealth() called every loop iteration** (MQTTstuff.ino:1573)
   - `ESP.getFreeHeap()` ~1-2us per call, at 1000Hz = ~1-2ms/s. Unnecessary before timer fires.
   - **Fix**: Move adaptive interval logic after `DUE()` check.

6. **Failed discovery silently dropped** (MQTTstuff.ino:1614)
   - Pending bit cleared on publish failure. Entry missing from HA until next restart/reconnect.
   - **Fix**: Keep bit set on failure with backoff, or add retry counter.

7. **mqttha.cfg possibly still in LittleFS image** -- ~170KB wasted flash if no longer read at runtime. Remove from `data/`.

### Major Positive Improvements

- **processOT() latency: 50-200ms reduced to ~1us** -- bitmap set replaces LittleFS open/scan/close + MQTT publish inline.
- **Heap fragmentation elimination** -- zero LittleFS I/O during normal operation. No more file handle/sector cache/read buffer alloc/free cycles.
- **New RAM usage negligible** -- 47 bytes total (32 bytes bitmap + 2 bytes settings + 13 bytes timer).
- **Flash usage roughly neutral** -- ~164KB PROGMEM replaces ~170KB LittleFS file. Index overhead <4KB.

## Critical Issues for Phase 3 Context

- **Testing gap**: The strstr-on-PROGMEM performance issue should be validated with timing measurements on real hardware. The 20-40s estimate for full discovery is based on exception handler cost modeling, not measurement.
- **Documentation gap**: The PROGMEM pointer access pattern (relying on exception handler) is a deliberate engineering choice but undocumented. Future contributors may "fix" it by adding _P helpers that don't exist for this use case.
- **Integration test need**: The 17-minute discovery window should be tested with HA to verify entities appear correctly during the drip phase.
