<!-- markdownlint-disable-file -->

# Task Research Notes: MQTT command matching and JSON escape declaration cleanup

## Research Executed

### File Analysis

- src/OTGW-firmware/MQTTstuff.ino
  - Verified the recent topic-matching change is isolated to `handleMQTTcallback()`, using `setcmds[] PROGMEM`, `MQTT_set_cmd_t`, and inline PROGMEM reads to accept either long MQTT command names or two-letter OTGW commands.
- src/OTGW-firmware/jsonStuff.ino
  - Verified `escapeJsonStringTo()` is defined at file top and currently has no shared declaration except a local forward declaration added in `settingStuff.ino`.
- src/OTGW-firmware/settingStuff.ino
  - Verified `writeJsonStringKV()` is the only current caller of `escapeJsonStringTo()` and it relies on global scratch buffer `cMsg` to avoid heap allocation.
- src/OTGW-firmware/OTGW-firmware.h
  - Verified this is the existing shared declaration point for cross-module helpers and late-defined `.ino` functions that Arduino auto-prototype generation may miss.
- src/OTGW-firmware/OTGW-firmware.ino
  - Verified the sketch includes `OTGW-firmware.h` from the main `.ino`, so declarations placed there are visible after sketch concatenation.
- docs/adr/ADR-002-modular-ino-architecture.md
  - Verified accepted guidance that multi-file `.ino` sketches share one translation unit but still require explicit declarations to avoid hidden compile-order dependencies.
- docs/adr/ADR-004-static-buffer-allocation.md
  - Verified this refactor must preserve static buffers, bounded copies, and no new `String` usage in hot paths.
- docs/adr/ADR-006-mqtt-integration-pattern.md
  - Verified MQTT command handling is part of an accepted MQTT integration pattern and should stay buffer-bounded and lightweight.
- docs/adr/ADR-009-progmem-string-literals.md
  - Verified PROGMEM use is mandatory for string literals and `_P` functions should remain in place for PROGMEM comparisons.

### Code Search Results

- escapeJsonStringTo|setcmds|MQTT_settopic|two-letter|long command|forward declaration
  - Found the relevant edits only in `src/OTGW-firmware/MQTTstuff.ino`, `src/OTGW-firmware/jsonStuff.ino`, `src/OTGW-firmware/settingStuff.ino`, and existing shared declaration patterns in `src/OTGW-firmware/OTGW-firmware.h`.
- ^void [A-Za-z0-9_]+\([^;]*\);
  - Found local forward declarations in `MQTTstuff.ino`, `settingStuff.ino`, and `outputs_ext.ino`, confirming the codebase already uses explicit declarations when `.ino` auto-prototypes are unreliable or readability benefits.
- escapeJsonStringTo\(|escapeJsonString\(
  - Found `escapeJsonStringTo()` defined once in `jsonStuff.ino` and called once in `settingStuff.ino`; `escapeJsonString()` remains separate and is only used in `jsonStuff.ino` file-writing helpers.
- setcmds\[|MQTT_set_cmd_t|nrcmds|s_otgw_|s_cmd_
  - Found all MQTT command metadata in `MQTTstuff.ino` with long names and OTGW two-letter commands stored as PROGMEM tables and consumed only by `handleMQTTcallback()`.

### External Research

- #githubRepo:"arduino/arduino-cli sketch build process concatenated .ino files starting with file matching folder name then alphabetical order auto-generated prototypes documentation"
  - Verified from Arduino CLI build-process docs/source that sketch preprocessing concatenates the main `.ino` first, then other `.ino` files alphabetically, adds `#include <Arduino.h>`, and attempts auto-generated prototypes that can fail for complex cases; manual declarations remain the documented fallback.
- #fetch:https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
  - Verified ESP8266 `PROGMEM`, `PSTR()`, `F()`, and `_P` string helpers are the correct way to keep literals in flash and compare/read flash-backed strings safely.

### Project Conventions

- Standards referenced: `.github/copilot-instructions.md`, ADR-002, ADR-004, ADR-006, ADR-009
- Instructions followed: modular `.ino` architecture, explicit shared declarations for cross-module helpers, PROGMEM-resident command tables, bounded buffers, no architecture-significant changes without ADR

## Key Discoveries

### Project Structure

This workspace uses a modular Arduino sketch layout under `src/OTGW-firmware/`. The main file `OTGW-firmware.ino` includes `OTGW-firmware.h`, then Arduino preprocessing merges the remaining `.ino` files into a single generated `.cpp` in this order: main file first, then other `.ino` files alphabetically. For the touched files that means `jsonStuff.ino` is merged before `MQTTstuff.ino` and before `settingStuff.ino`. Even though that currently makes `escapeJsonStringTo()` visible before `settingStuff.ino`, ADR-002 and Arduino CLI docs both warn against relying on hidden merge order or auto-generated prototypes for maintainability.

### Implementation Patterns

- MQTT command metadata is centralized in `MQTTstuff.ino` as `const MQTT_set_cmd_t setcmds[] PROGMEM`, with each row holding long topic name, OTGW two-letter command, and command type.
- `handleMQTTcallback()` currently performs topic parsing, then scans `setcmds[]` inline and reads `setcmd`, `otgwcmd`, and `ottype` from PROGMEM on each loop iteration.
- The recent two-name matching enhancement is correct but embedded directly in the callback, which now mixes namespace parsing, command lookup, raw-vs-typed command branching, and queue submission.
- `escapeJsonStringTo()` is defined in `jsonStuff.ino`, but its declaration was added locally to `settingStuff.ino` instead of the shared header already used for cross-module declarations.
- `writeJsonStringKV()` intentionally uses the global `cMsg` scratch buffer to satisfy ADR-004 static-buffer guidance.

### Complete Examples

```cpp
// Source: src/OTGW-firmware/MQTTstuff.ino
for (i=0; i<nrcmds; i++){
  // Read setcmd and otgwcmd pointers from Flash
  PGM_P pSetCmd = (PGM_P)pgm_read_ptr(&setcmds[i].setcmd);
  PGM_P pOtgwCmd = (PGM_P)pgm_read_ptr(&setcmds[i].otgwcmd);

  // Match either the long command name (e.g. "setpoint") or the short raw command (e.g. "TT")
  if ((strcasecmp_P(token, pSetCmd) == 0) || ((strcasecmp_P(token, pOtgwCmd) == 0) && strlen_P(pOtgwCmd) > 0)){
    //found a match
    // Read ottype from Flash
    PGM_P pOtType = (PGM_P)pgm_read_ptr(&setcmds[i].ottype);

    if (strcasecmp_P("raw", pOtType) == 0){
      snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s"), msgPayload);
      addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
    } else {
      char cmdBuf[10];
      strncpy_P(cmdBuf, pOtgwCmd, sizeof(cmdBuf));
      cmdBuf[sizeof(cmdBuf)-1] = 0;

      snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s=%s"), cmdBuf, msgPayload);
      addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
    }
    break;
  }
}
```

### API and Schema Documentation

- MQTT command topic contract is `<Base_Topic>/set/<Node_ID>/<command>`.
- The current implementation accepts both long topic names (for example `setpoint`) and two-letter OTGW command names (for example `TT`) by looking up both `setcmd` and `otgwcmd` from `setcmds[]`.
- The `command` entry is a special raw-command path whose `otgwcmd` is the empty PROGMEM string and whose `ottype` is `raw`.
- `escapeJsonStringTo(const char* src, char* dest, size_t destSize)` is a bounded escaping helper that writes into caller-owned storage and returns results via `dest` only.

### Configuration Examples

```text
Sketch merge order relevant here:
1. OTGW-firmware.ino
2. FSexplorer.ino
3. handleDebug.ino
4. helperStuff.ino
5. jsonStuff.ino
6. MQTTstuff.ino
7. OTGW-Core.ino
8. outputs_ext.ino
9. restAPI.ino
10. s0PulseCount.ino
11. sensors_ext.ino
12. settingStuff.ino
13. versionStuff.ino
14. webhook.ino
15. webSocketStuff.ino
```

### Technical Requirements

- Preserve `setcmds[] PROGMEM` and `_P` comparison functions; do not introduce RAM copies for the table itself.
- Preserve the empty-short-command guard for the raw `command` topic entry; otherwise the blank `otgwcmd` row can become a false match path.
- Preserve case-insensitive behavior because the current code uses `strcasecmp()`/`strcasecmp_P()` for both long and short command tokens.
- Keep stack and static buffer usage bounded; no new `String` use should be introduced into MQTT callback hot paths.
- The declaration move for `escapeJsonStringTo()` is not architecturally significant; no new ADR appears required.
- There is one touched-area convention conflict to note: `escapeJsonStringTo()` currently uses normal C string literals like `"\\n"` and `"\\\\"`, which does not align with the repo’s strict ADR-009 PROGMEM guidance. That is outside the approved cleanup scope unless explicitly bundled.
- No current compile errors were reported by the editor for the touched files, and the latest recorded `python build.py` completed successfully.

## Recommended Approach

Use one small helper extraction in `MQTTstuff.ino` plus one shared declaration move in `OTGW-firmware.h`.

For MQTT readability, the minimal codebase-consistent refactor is to extract the inline `setcmds[]` scan into a file-local helper that encapsulates PROGMEM reads and dual-name matching, returning the resolved `otgwcmd` and `ottype` for the caller. This keeps the command table and callback in the same file, avoids new headers or data-structure churn, and reduces `handleMQTTcallback()` to topic parsing plus command execution. A helper shaped like `static bool findMqttSetCommand(const char* token, PGM_P& otgwCmd, PGM_P& otType)` or equivalent best matches existing file-local utility style.

For the JSON helper cleanup, the minimal change is to move `escapeJsonStringTo()`'s declaration from `settingStuff.ino` into `OTGW-firmware.h`, beside other cross-module forward declarations. That removes the local one-off prototype and aligns with the existing shared-header pattern already used for `readSettings()`, `writeSettings()`, `updateSetting()`, and other later-defined `.ino` functions.

This approach avoids relying on sketch merge order, does not change public behavior, and stays inside accepted ADR boundaries.

## Implementation Guidance

- **Objectives**: simplify `handleMQTTcallback()` without changing topic behavior; centralize `escapeJsonStringTo()` declaration in the normal shared header location.
- **Key Tasks**: extract one file-local MQTT command lookup helper; replace the inline lookup call site; add one declaration to `OTGW-firmware.h`; remove the local declaration from `settingStuff.ino`.
- **Dependencies**: `setcmds[] PROGMEM`, `MQTT_set_cmd_t`, `_P` string helpers from `pgmspace.h`, shared globals from `OTGW-firmware.h`, Arduino sketch preprocessing behavior.
- **Success Criteria**: long-name MQTT topics and two-letter MQTT topics both still enqueue identical OTGW commands; raw `command` topic behavior is unchanged; `escapeJsonStringTo()` is declared only once in shared header scope; build remains clean with no new warnings or memory-pattern regressions in touched files.