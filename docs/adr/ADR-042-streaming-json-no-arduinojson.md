# ADR-042: Streaming JSON I/O — No ArduinoJson

**Status:** Accepted
**Date:** 2026-02-28
**Decision Maker:** User: Rob van den Breemen (rvdbreemen)
**Supersedes:** ADR-018 (ArduinoJson for Data Interchange)

## Context

ADR-018 adopted ArduinoJson 6.17.2 as the library for all structured data interchange
(settings persistence, REST API responses, Dallas label files).  Over time, several
problems emerged:

- `DynamicJsonDocument` allocates on the heap, which causes heap fragmentation on the
  ESP8266's ~40 KB available RAM (cf. ADR-004: Static Buffer Allocation).
- The 1536 B buffer chosen in ADR-018 was too small for worst-case settings (39 fields
  × ~44 B/slot ≈ 1732 B); the buffer silently overflowed, wiping all settings to defaults
  at boot.
- The library adds ~5 KB of flash and introduces a dependency that must be pinned and
  maintained.
- JSON for settings persistence and Dallas sensor labels is simple flat key/value pairs;
  a full-featured parser library is unnecessary.

A dedicated set of lightweight streaming helpers now covers all production use-cases
without ArduinoJson heap documents. Most JSON generation and file parsing uses the
existing global scratch buffer `cMsg[512]` and other bounded stack/global buffers,
with only short-lived `String` heap allocations where HTTP handlers already expose a
`String` body (e.g. `extractJsonField()`).

## Decision

**Never use ArduinoJson in this firmware.  All JSON I/O uses streaming helpers that
operate on global scratch buffers.**

### Rules

1. **No `DynamicJsonDocument`, `StaticJsonDocument`, `JsonObject`, `JsonArray`,
   `deserializeJson`, or `serializeJson`** in any `.ino`, `.cpp`, or `.h` file.
2. **JSON generation** uses the helpers spread across two modules:
   - `wStrF(file, PSTR("key"), value)` / `wBoolF(...)` / `wIntF(...)` in
     `settingStuff.ino` — write settings fields directly to a LittleFS `File`.
   - `sendStartJsonMap()` / `sendJsonMapEntry()` / `sendEndJsonMap()` in
     `jsonStuff.ino` — stream HTTP responses as a JSON object.
   - `sendStartJsonObj()` / `sendNestedJsonObj()` / `sendEndJsonObj()` in
     `jsonStuff.ino` — legacy array-format HTTP responses (OTmonitor, Telegraf).
3. **JSON parsing** uses the helpers in `jsonStuff.ino`:
   - `extractJsonField(body, F("key"), buf, sizeof(buf))` for individual field extraction
     from a `String` body.
   - `readJsonStringPair(file, key, keySize, val, valSize)` for streaming key/value pairs
     from a `File`.
4. **All intermediate buffers must be global or stack-allocated and bounded**.  The global
   `cMsg[512]` scratch buffer is the canonical single-use scratch area for JSON formatting.
   Local buffers are allowed for short-lived values that must not alias `cMsg`.
5. **Strings written to JSON settings files must be escaped** using `wStrF()` from
   `settingStuff.ino` (which performs per-character escaping of `"`, `\`, `\n`, `\r`, `\t`
   while writing to the output `File`, without modifying the input buffer) or an equivalent
   helper.

### What is NOT affected

- `MQTTstuff.ino` MQTT discovery payloads are built with `snprintf_P` / `sendContent`
  directly (no ArduinoJson, no new helpers required).
- WebSocket messages (`webSocketStuff.ino`) use raw string concatenation — unchanged.

## Alternatives Considered

### Alternative 1: Keep ArduinoJson, increase buffer to 2048 B
**Pros:** Minimal code change; type-safe access.  
**Cons:** Still allocates on the heap (fragmentation); 2048 B per call still wastes RAM;
the dependency must be maintained; the original bug (silent overflow → settings reset)
could recur if new settings are added without bumping the buffer size.  
**Why not chosen:** The root cause (heap allocation + implicit sizing) is not addressed.

### Alternative 2: Use a different JSON library (cJSON, PicoJSON, jsmn)
**Pros:** Would remove ArduinoJson dependency.  
**Cons:** All alternatives either use dynamic allocation (cJSON), rely on `std::string`
(PicoJSON), or are token-only without value extraction (jsmn), none of which fit the
ESP8266 constraints documented in ADR-004.  
**Why not chosen:** The streaming helper approach is simpler and fits the existing
`cMsg` pattern already used throughout the codebase.

### Alternative 3: Keep ArduinoJson only for REST API responses
**Pros:** Reduces ArduinoJson usage; complex nested objects remain easy to build.  
**Cons:** Partial removal creates two code paths and keeps the dependency; REST API
responses are generated once and streamed, so the streaming helpers are equally capable.  
**Why not chosen:** Partial removal is harder to audit and maintain than a clean cut.

## Consequences

### Positive
- **Zero heap fragmentation from ArduinoJson documents** — all JSON generation and file
  parsing uses fixed-size global/stack buffers; only short-lived `String` allocations
  remain in HTTP request handlers where the framework already provides a `String` body.
- **Settings file never overflows** — each field is written directly with bounded
  `snprintf_P` calls; there is no document-size constraint.
- **~5 KB flash saved** by removing the ArduinoJson library.
- **Single dependency removed** — easier builds, fewer CVE surface.
- **Forced correctness** — callers must choose the right type overload; implicit
  `double → T` ambiguity becomes a compile error rather than a silent data loss.

### Negative
- **No type-safe key access** — field extraction is string-based; typos in key names are
  not caught at compile time.
- **Manual escaping** — developers must use `wStrF()` from `settingStuff.ino` (or equivalent) for string values;
  forgetting to escape is a latent bug.
- **Limited to flat structures** — the helpers do not support arbitrarily nested JSON;
  deeply nested responses must be hand-crafted with `sendContent_P` calls.

### Risks & Mitigation
- **Missing escape in `writeJsonStringPair()`** — labels containing `"` or `\` would
  produce invalid JSON.  **Mitigation:** use `wStrF`-style character-by-character
  escaping in `writeJsonStringPair()` (`jsonStuff.ino`).
- **`extractJsonField()` returns false for empty string values** — a valid
  `{"value":""}` would be rejected.  **Mitigation:** track whether the field was found
  separately from whether the value is empty; fix the helper to return true when the
  field exists but is empty.
- **New settings must be added to both `writeSettings()` and `applySettingFromFile()`**
  — the two lists are not derived from the same source.  **Mitigation:** code review
  checklist item; the lists are adjacent in `settingStuff.ino`.

## Related Decisions
- ADR-004: Static Buffer Allocation Strategy (mandates bounded, non-heap allocations)
- ADR-008: LittleFS Configuration Persistence (settings stored in `/settings.ini`)
- ADR-009: PROGMEM String Literals (all key literals use `PSTR()` / `F()`)
- ADR-018: ArduinoJson for Data Interchange (**superseded by this ADR**)

## References
- Implementation: `src/OTGW-firmware/jsonStuff.ino` (helpers), `settingStuff.ino`
  (settings read/write), `restAPI.ino` (HTTP responses), `sensors_ext.ino`
  (Dallas label file)
- PR: https://github.com/rvdbreemen/OTGW-firmware/pull/459
- Root-cause analysis: `docs/reviews/2026-02-01_memory-management-bug-fix/`
