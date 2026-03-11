# ADR-046: PS=1 Summary Translation with Shared Publish Helpers

**Status:** Accepted  
**Date:** 2026-03-07

## Context

ADR-045 documented PS=1 support as a synthetic-frame adapter that would feed each summary field back into `decodeAndPublishOTValue()` so the existing raw OpenTherm pipeline owned all downstream behavior.

The implemented refactor in `src/OTGW-firmware/OTGW-Core.ino` took a narrower path:

1. `processPSSummary()` remains a dedicated tokenizer and field dispatcher for the 25-field and 34-field PIC summary formats.
2. PS-mode state changes are centralized via `enterPSMode()` and `leavePSMode()`.
3. Shared helper functions were extracted for status-oriented MsgIDs (`Statusflags`, `RBPflags`, `StatusVH`) so PS and raw OT processing can reuse the same MQTT/state side effects where it materially reduces duplication.
4. Numeric parsing is strict (`strtol`, `strtoul`, `strtod` with full-token validation) instead of permissive `atoi`/`atof` conversion.

This keeps the external PS=1 behavior additive and compatible, but it is not the same internal architecture that ADR-045 described. Accepted ADRs are immutable, so the architectural record must be aligned by supersession rather than rewriting ADR-045.

### Constraints

- **ESP8266 memory discipline:** No heap allocation, bounded buffers only (ADR-004).
- **PROGMEM discipline:** Summary MsgID lookup tables remain in flash (ADR-009).
- **MQTT compatibility:** PS=1 publishes must continue using the normal topic namespace and JIT discovery flow (ADR-006, ADR-041).
- **Gateway mode behavior:** `bPSmode` must remain authoritative for PS-mode side effects such as time-sync suppression (ADR-037).
- **Synchronous processing model:** Work still occurs inline within the OTGW serial processing path (ADR-038), so validation and helper reuse should stay lightweight.

## Decision

**Treat PS=1 as a dedicated summary-translation path with shared publish/state helpers, not as a synthetic raw-frame adapter.**

### What this means in the current implementation

1. **Dedicated PS tokenizer stays in place.** `processPSSummary()` validates field count, maps positions to MsgIDs, and dispatches by `OTlookupitem.type`.
2. **Strict parsing is required.** Malformed tokens are ignored instead of being silently coerced to `0`.
3. **Shared side effects are extracted selectively.** Status-oriented handlers now reuse helper functions across raw and PS paths where parity matters most.
4. **MQTT discovery and WebSocket field logging remain explicit PS actions.** They are invoked directly from the PS path after a field is accepted.
5. **User-visible PS-mode state is centralized.** All current PS enter/leave transitions flow through `enterPSMode()` / `leavePSMode()`.

This supersedes ADR-045's synthetic-frame design as the governing architectural description for PS=1 handling.

## Alternatives Considered

### Alternative 1: Keep ADR-045 unchanged and treat the mismatch as an implementation detail
**Pros:**
- No documentation changes
- No ADR churn

**Cons:**
- Leaves the accepted ADR materially inaccurate
- Misleads future refactors toward a pipeline that is not implemented

**Why not chosen:** The mismatch is architectural, not editorial. The repository rules require the ADR record to match the accepted decision.

### Alternative 2: Rewrite ADR-045 in place
**Pros:**
- Single ADR file for PS=1
- Minimal index churn

**Cons:**
- Violates accepted-ADR immutability
- Rewrites historical rationale after implementation changed

**Why not chosen:** Project ADR rules only allow status updates or supersession for accepted ADRs.

### Alternative 3: Refactor the code back to the ADR-045 synthetic-frame pipeline
**Pros:**
- Restores the original ADR/code alignment
- Could further reduce duplicated semantics if done well

**Cons:**
- Broader code change than requested
- Reopens correctness and memory-risk questions in a stable firmware path
- Requires fresh validation beyond this cleanup task

**Why not chosen:** The current implementation is already landed and operational. This task is to align the architectural record with minimal impact, not to force a larger redesign.

## Consequences

**Positive:**
- ADRs now match the implemented PS=1 architecture.
- PS-mode transitions are explicitly documented as centralized helpers.
- Strict parsing and selective helper reuse are recorded as intentional design choices.

**Negative:**
- PS=1 is still not a full reuse of the canonical raw-frame decode pipeline.
- Some downstream behavior remains PS-specific, increasing long-term maintenance pressure versus a fully unified pipeline.

**Risks and Mitigation:**
- *Risk:* Future contributors may assume PS and raw OT behavior are fully identical.  
  *Mitigation:* This ADR explicitly documents selective helper reuse rather than full pipeline reuse.
- *Risk:* Additional PS-only branches could drift again.  
  *Mitigation:* Future architectural changes to PS handling should reference ADR-046 and supersede it if the design changes materially.

## Related Decisions

- **ADR-004:** Static Buffer Allocation Strategy
- **ADR-006:** MQTT Integration Pattern
- **ADR-009:** PROGMEM Usage for String Literals
- **ADR-037:** Gateway Mode Detection via PR=M Polling
- **ADR-038:** OpenTherm Message Data Flow Pipeline
- **ADR-041:** JIT HA Discovery
- **Supersedes:** ADR-045: PS=1 Print Summary Parsing

## References

- Implementation: `src/OTGW-firmware/OTGW-Core.ino`
- PS summary tables: `PSSUMMARY_MSGIDS_OLD`, `PSSUMMARY_MSGIDS_NEW`
- Shared helpers: `enterPSMode()`, `leavePSMode()`, `publishCombinedStatusState()`, `publishCombinedStatusVHState()`, `publishRBPFlagsState()`