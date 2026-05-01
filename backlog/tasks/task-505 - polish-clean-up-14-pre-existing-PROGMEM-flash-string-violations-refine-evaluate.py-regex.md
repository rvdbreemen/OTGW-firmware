---
id: TASK-505
title: >-
  polish: clean up 14 pre-existing PROGMEM flash-string violations + refine
  evaluate.py regex
status: To Do
assignee: []
created_date: '2026-05-01 18:39'
labels:
  - polish
  - progmem
  - tooling
  - follow-up
dependencies: []
references:
  - src/OTGW-firmware/debugStuff.ino
  - src/OTGW-firmware/FSexplorer.ino
  - src/OTGW-firmware/jsonStuff.ino
  - src/OTGW-firmware/MQTTstuff.ino
  - src/OTGW-firmware/OTGW-Core.ino
  - src/OTGW-firmware/platform_esp32.h
  - 'evaluate.py:1742-1810 (check_progmem_compliance)'
  - 'ADR-004 (no String in hot paths, PROGMEM contract)'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

`python evaluate.py --quick` reports `Found 14 PROGMEM violations (wastes RAM)` under the **Flash string compliance** check. Non-fatal (script exit 0), but counts toward Health Score (currently 95.5%). Discovered while validating TASK-503/504 on 2026-05-01; the violations are pre-existing baseline, not regressions from recent batches.

## The 14 sites

Reproduced via the same regex as `evaluate.py:check_progmem_compliance`:

**Real violations (11):**

| File:Line | Pattern | Fix |
|---|---|---|
| `debugStuff.ino:91` | `snprintf(cachedPrefix, ...)` | `snprintf_P(... , PSTR(...))` |
| `debugStuff.ino:100` | `snprintf(_bol, ...)` | same |
| `debugStuff.ino:101` | `snprintf(_bol + written, ...)` | same |
| `jsonStuff.ino:35` | `snprintf(hex, ..., "\\u%04X", ...)` | same |
| `MQTTstuff.ino:1512` | `snprintf(buffer, ..., "%d", value)` | same |
| `MQTTstuff.ino:1518` | `snprintf(buffer, ..., "%d", value)` | same |
| `MQTTstuff.ino:1565` | `snprintf(sourceTopic, ..., "%s/%s", topic, sourceKeyBuf)` | same |
| `platform_esp32.h:221` | `snprintf(key, ..., "s%u", slot)` | same |
| `platform_esp32.h:231` | `snprintf(key, ..., "s%u", slot)` | same |
| `OTGW-Core.ino:4847` | `strcmp(state.pic.sDeviceid, "unknown")` | `strcmp_P(state.pic.sDeviceid, PSTR("unknown"))` |
| `OTGW-Core.ino:4930` | same `strcmp(..., "unknown")` | same |

**False positives in evaluate.py regex (3):**

| File:Line | Code | Why false-positive |
|---|---|---|
| `FSexplorer.ino:111` | `strcmp(httpServer.header(F("If-None-Match")).c_str(), etag)` | The only literal is inside `F(...)`. Regex `strcmp\([^)]*"` matches any `"` between `strcmp(` and `)`, including those inside an `F()` wrapper that is already PROGMEM-correct. |
| `FSexplorer.ino:192` | `strcmp(httpServer.arg("v").c_str(), fsHash)` | The literal `"v"` is the argument-name to `httpServer.arg()`; that is a Method-name lookup, not a flash candidate. (Arduino `httpServer.arg(const char*)` does not benefit from PSTR.) |
| `FSexplorer.ino:214` | identical `httpServer.arg("v").c_str()` | same |

The FSexplorer:111 case is actually a regex bug worth fixing in `evaluate.py`. The other two `httpServer.arg("v")` cases are debatable: if we wanted strictness we could use `arg(F("v"))` if the Arduino API supports it (it does in newer cores). Cheap to do.

## Approach

Two sub-tasks executed in this order:

1. **Source fixes** (mechanical wrap of literals).
2. **evaluate.py regex refinement** so future runs no longer flag `strcmp(..., F("..."))` patterns. Optional: also exempt `httpServer.arg("...")` when the only literal sits there, OR convert those calls to `arg(F("..."))` consistently.

No behavioural change expected from the source fixes: `snprintf` and `snprintf_P` produce identical output; same for `strcmp` and `strcmp_P`. Only the format/literal string moves from RAM to flash.

## Why low priority

Pure observability/RAM-budget polish. ESP8266 is the constrained platform (~40KB RAM); 14 short literals is single-digit-byte territory per literal, so total RAM saved is small. The real value is **getting evaluate.py to a clean PASS** so future regressions stand out immediately. Bundle in a polish-batch run, not standalone work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 11 real violations migrated to PROGMEM equivalents (snprintf -> snprintf_P with PSTR, strcmp -> strcmp_P with PSTR); enumerated in description above
- [ ] #2 evaluate.py regex for strcmp/strcasecmp checks refined so a string literal nested inside an F() wrapper does not count as a violation (covers FSexplorer.ino:111 case)
- [ ] #3 Either (a) the two httpServer.arg("v") cases are converted to arg(F("v")) for consistency, or (b) evaluate.py is taught to exempt them with a clear comment explaining why
- [ ] #4 python evaluate.py --quick reports 0 PROGMEM violations and the Flash string compliance check moves from FAIL to PASS
- [ ] #5 Health Score climbs (target: as close to 100 percent as the other 56 checks allow; was 95.5 percent at baseline)
- [ ] #6 Build clean on ESP8266 and ESP32-S3 (python build.py); no behavioural change
- [ ] #7 No new violations introduced anywhere; verify by re-running python evaluate.py --quick post-fix
<!-- AC:END -->
