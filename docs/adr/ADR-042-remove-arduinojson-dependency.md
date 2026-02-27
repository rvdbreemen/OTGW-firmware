# ADR-042: Remove ArduinoJson Dependency

**Status:** Accepted  
**Date:** 2026-02-27
**Supersedes:** ADR-018

## Context

The firmware still depended on ArduinoJson for a limited set of tasks (settings file I/O, a few REST request parses, and Dallas label handling). This created an extra external dependency in both Makefile and build.py while most API generation was already handled with existing manual JSON helpers.

Constraints:
- ESP8266 RAM is limited; dependency footprint matters.
- Existing JSON API contracts must remain backward compatible.
- Changes must be minimal and low-risk for v1.3.0-beta branch.

## Decision

Remove ArduinoJson completely from runtime code and build dependencies.

Refactor affected paths to use bounded manual JSON handling:
- `settingStuff.ino`: manual JSON write/read helpers for settings file.
- `restAPI.ino`: manual parsing for `name/value` and `command` request bodies; manual JSON responses where ArduinoJson was still used.
- `sensors_ext.ino`: lightweight label-file merge logic without ArduinoJson.
- `Makefile`, `build.py`, `OTGW-firmware.h`: remove ArduinoJson include/install references.

## Alternatives Considered

1. **Keep ArduinoJson (status quo)**  
   - Pros: Mature parser, less custom parsing code.  
   - Cons: Violates requirement to remove dependency; keeps external library footprint.

2. **Replace with another JSON library**  
   - Pros: Maintains generic parsing abstraction.  
   - Cons: Adds/churns dependency instead of removing one.

3. **Manual bounded JSON handling (chosen)**  
   - Pros: Zero JSON library dependency; small targeted changes; full control over memory usage.  
   - Cons: Less general than a full JSON parser; requires careful bounded parsing.

## Consequences

Positive:
- ArduinoJson dependency is removed from code and build paths.
- Less external dependency surface for firmware builds.
- Critical flows keep existing payload formats.

Trade-offs / Risks:
- Manual parsers are intentionally limited to expected payload shapes.
- Invalid or unexpected JSON structures are rejected more conservatively.

Mitigation:
- Use bounded buffers and explicit validation for all parsed values.
- Keep JSON response contracts unchanged for existing endpoints.

## Related

- Supersedes: `docs/adr/ADR-018-arduinojson-data-interchange.md`
- Code: `src/OTGW-firmware/settingStuff.ino`, `src/OTGW-firmware/restAPI.ino`, `src/OTGW-firmware/sensors_ext.ino`
- Build: `Makefile`, `build.py`
