# Phase 3: Testing & Documentation Review

## Test Coverage Findings

### High Severity

1. **Generator script has zero tests** (tools/generate_mqttha_progmem.py)
   - Pure Python, no hardware dependency, testable with pytest. Bugs corrupt all 345 discovery entries.
   - Highest-ROI testing investment: parse, pool building, index construction, C string escaping.

2. **Bitmap operations untested** (MQTTstuff.ino:1487-1543)
   - Core of drip publisher. Pure arithmetic, testable on host PC. Boundary cases: ID 0, 32, 127, 255.

3. **Discovery drip loop state machine untested** (MQTTstuff.ino:1559-1627)
   - Scan order, "already done" skip, failure-clears-pending behavior. Partially extractable for testing.

### Medium Severity

4. **Generator not in build pipeline** -- no staleness detection between mqttha.cfg and generated files.
5. **Nightly restart timing edge cases** -- 60s window miss, settings load-time validation gap (no bounds check when loading from JSON, only on REST update).
6. **strstr() on PROGMEM performance not measured** -- needs on-device instrumentation.
7. **Failed discovery silently dropped** -- no user-visible indication, no retry counter.
8. **No heap usage regression baseline** -- no automated check for RAM/BSS growth.

### Low Severity

9. **Settings validation inconsistent** between load path and update path.
10. **Discovery completion time not instrumented** -- no log when all pending bits cleared.

### Testable Without Hardware (prioritized)

| Priority | What | How | Effort |
|----------|------|-----|--------|
| 1 (HIGH) | Generator script | pytest unit tests | 2-4 hours |
| 2 (HIGH) | Bitmap operations | Host-compiled C++ test | 1-2 hours |
| 3 (MEDIUM) | Generated file staleness | mtime check in evaluate.py | 30 min |
| 4 (MEDIUM) | Settings validation | Python script parsing settingStuff.ino | 1-2 hours |

## Documentation Findings

### High Severity

1. **No ADR for PROGMEM migration** -- significant architectural change (LittleFS -> PROGMEM, code generator, direct flash pointer access) undocumented. ADR-040/041 are Accepted (immutable) and reference the old approach.

2. **C4 MQTT code doc references LittleFS** (docs/c4/c4-code-mqtt.md) -- describes doAutoConfigure as "Opens /mqttha.cfg from LittleFS". Now uses PROGMEM index lookup.

3. **Code generator workflow undocumented** -- tools/generate_mqttha_progmem.py not mentioned in CLAUDE.md, README, or any developer guide. Developers won't know to re-run it after editing mqttha.cfg.

4. **MQTT.md incorrectly describes LittleFS-based discovery** (docs/api/MQTT.md:851-871) -- states templates stored in /mqttha.cfg on LittleFS. Now compiled into PROGMEM at build time.

### Medium Severity

5. **No ADR for drip publisher pattern** -- new async discovery approach (bitmap + timer + adaptive interval) not architecturally documented.
6. **REST API docs missing nightly restart settings** -- nightlyrestart, nightlyrestarthour not in API docs or OpenAPI spec.
7. **C4 docs missing loopMQTTDiscovery and pending bitmap** -- new functions and data structures not in code-level docs.
8. **Nightly restart not in CHANGELOG** -- user-facing feature undocumented.
9. **17-minute discovery window undocumented** -- behavioral change users will notice (entities appear progressively vs all-at-once).
10. **ESP32 PROGMEM pointer assumption not documented** -- cross-platform implication of direct flash reads.

### Low Severity

11. **CLAUDE.md missing PROGMEM tables reference** -- new architectural pattern not mentioned.
