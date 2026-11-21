# PS=1 Lean Translator Refactor Plan

## Summary

This document captures the minimal-impact refactor plan for PS=1 summary handling in OTGW-firmware.

The target architecture is a lean PS translator plus shared publish/state helpers:

- keep `processOT()` focused on line classification and dispatch
- keep `processPSSummary()` focused on validation, tokenization, and translation
- share publish/state side effects where it meaningfully reduces duplication
- avoid synthetic OT frame strings, heap allocation, and retained intermediate objects

## Goals

- Reduce duplication in PS=1 summary handling
- Improve parity with raw OT processing for status-oriented MsgIDs
- Centralize PS mode state transitions
- Tighten PS field validation
- Keep memory impact minimal and bounded
- Preserve current behavior unless a parity fix is intentional

## Constraints

- ESP8266 memory discipline remains mandatory
- No new `String` usage
- No new persistent buffers or queues
- PROGMEM tables remain in flash
- Changes should stay local to OT core handling where possible

## Planned Implementation Phases

### Phase 1 — Centralize PS mode state

- Introduce one helper for entering/leaving PS mode
- Route all PS state transitions through it
- Keep side effects explicit and small

### Phase 2 — Share status-oriented side effects

- Extract small internal helpers for status-like MsgIDs where PS and raw handling overlap
- Prioritize MsgID 0, MsgID 6, and MsgID 70
- Reuse these helpers from both raw and PS paths where practical

### Phase 3 — Slim `processPSSummary()`

- Reduce it to validation, tokenization, mapping, and dispatch
- Replace permissive parsing with validation-aware parsing
- Keep local buffers fixed-size and small

### Phase 4 — Narrow `processOT()`

- Keep PS summary branching as dispatch only
- Remove duplicated PS mode writes and stale message text

### Phase 5 — Validate

- Build with `python build.py`
- Run `python evaluate.py --quick`
- Recheck PS/raw parity for key summary MsgIDs

## Expected Outcome

The result should be cleaner and easier to maintain without a broad rewrite:

- smaller PS footprint in the OT processing path
- less duplicated business logic
- better parity for status-style summary fields
- no meaningful increase in RAM usage
