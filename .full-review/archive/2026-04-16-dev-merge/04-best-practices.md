# Phase 4: Best Practices & Standards

## Framework & Language Findings

### Critical Severity

1. **`strstr()` on PROGMEM pointers -- exception handler abuse** (MQTTstuff.ino:1711-1724, 1842-1844)
   - Each unaligned byte access costs ~60-120us via Xtensa LoadStoreErrorCause exception handler.
   - 345 entries x 6 strstr calls x ~500 bytes avg = ~10-30s CPU in exception handlers for bulk publish.
   - Fix: Use `strstr_P()` (haystack=PROGMEM, needle=RAM -- matches this use case exactly). Or precompute flags in generator.

2. **`writeMqttChunk()` called with PROGMEM literal pointers** (MQTTstuff.ino:339-346)
   - `sendMQTTTemplateStreaming()` passes PROGMEM literal segments to `writeMqttChunk()` which uses `memcpy`/`MQTTclient.write()` expecting RAM.
   - `writeMqttProgmemChunk()` (line 282) already exists and correctly stages via `pgm_read_byte()` but is unused.
   - Fix: Use `writeMqttProgmemChunk()` for literal segments in `sendMQTTTemplateStreaming()`.

3. **`renderTemplateToBuffer()` / `tryGetTemplateReplacement()` dereference PROGMEM without `_P` helpers** (MQTTstuff.ino:216-243, 163)
   - `*cursor` and `strncmp(cursor, ...)` on PROGMEM pointers. Same exception handler penalty per byte.
   - Fix: Create `_P` variants using `pgm_read_byte()` and `strncmp_P()`, or stage template into RAM buffer first (sLine[1200] is available).

### Medium Severity

4. **Heap guard reduced to 8000 without documented justification** (MQTTstuff.ino:51)
   - 8000/1200 = 6.7x margin. On Core 3.x with lwIP2 Low Memory, baseline heap is 15-20KB. Borderline for concurrent TCP activity.
   - Comment says "12000 provides ~10x margin" but value is 8000. Stale comment.

5. **Generator not in build pipeline** (tools/generate_mqttha_progmem.py) -- risk of stale PROGMEM tables. Already flagged in prior phases.

6. **Mixed `strlen`/`strlen_P` usage** -- `strlen_P(msgTemplate)` correct in debug output, but implicit `strlen()` via `*cursor != '\0'` iteration elsewhere.

### Low Severity

7. **Comment/code mismatch on heap constant** -- misleading documentation.
8. **`sLine[1200]` still allocated but unused by MQTT discovery** -- 1200 bytes reclaimable.
9. **`delay(200)` before ESP.restart()** -- acceptable, minor.

### Positive Findings

- Timer macros used correctly (DECLARE_TIMER_SEC, SKIP_MISSED_TICKS, CHANGE_INTERVAL_SEC, DUE).
- Adaptive timer interval (heap pressure -> slow down) is good design.
- MqttHaCfgEntry struct alignment handled correctly (static_assert + memcpy_P).
- `constexpr` for compile-time constants (modern C++).
- Separate TU for PROGMEM data avoids Xtensa relocation overflow.
- No deprecated APIs. Library versions current.

## CI/CD & DevOps Findings

### High Severity

1. **No build CI** -- firmware never compiled in CI. Broken builds can merge undetected.
   - Only workflows: spec audit and Copilot trigger. Neither builds firmware.
   - Fix: Add workflow with `pio run -e esp8266 && pio run -e esp32 && python evaluate.py --quick`. ~40 lines YAML.
   - Single highest-value improvement available.

### Medium Severity

2. **`evaluate.py` not in CI** -- PROGMEM compliance and safety checks purely manual.
3. **Generated PROGMEM files can drift from source** -- no CI verification step.
   - Fix: CI step `python tools/generate_mqttha_progmem.py && git diff --exit-code src/OTGW-firmware/mqttha_progmem.*`

### Low-Medium Severity

4. **No input hash in generated files** -- cheap staleness detection missing.
5. **No build size tracking** -- gradual flash creep could surprise.

### Low Severity

6. **No automated release workflow** -- releases are manual. Acceptable for project maturity.
7. **No debug/release build distinction** -- debug telnet always active. Not urgent.

### Positive Findings

- Version management is solid: `autoinc-semver.py` hooked into both build backends.
- ESP32 partition table supports OTA with 2x 1.5MB app slots.
- Generated files have "DO NOT EDIT" markers.
- Generator produces deterministic output (sorted by ID, UTC timestamps).
