---
id: "ADR-209"
title: "Replay file lines may carry the otframes.log device time and the replay waits the recorded gap"
status: "Proposed"
date: "2026-11-04"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "replay"
  - "simulation"
  - "otframes"
  - "timing"
aliases:
  - "timed replay"
  - "otgw_simulation.log format"
components:
  - "OTGW replay (/api/v2/simulate)"
symbols:
  - "replayNextOTGWSimulationLine"
  - "classifyReplayLine"
  - "parseReplayDeviceTime"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-209 Replay file lines may carry the otframes.log device time and the replay waits the recorded gap

## Status

Proposed, 2026-11-04.

Guideline-level per ADR-080 (Binding ADR rules must have a CI gate): the test gate is `tests/test_replay_timing.py`, which compiles the real parser and scheduler cut from `OTGW-Core.ino`. It is not enforced by `evaluate.py`.

## Status History

```yaml
status_history:
  - date: 2026-11-04
    status: Proposed
    changed_by: Claude
    reason: Initial proposal from TASK-1508 (maintainer chose "the firmware reads the times" on 2026-11-04)
    changed_via: adr-kit
```

## Context

The replay (`POST /api/v2/simulate/start`) reads `/otgw_simulation.log` from LittleFS, or the built-in fixture, one bare frame per line (`T00390000`), and dispatches one line every `interval_ms` = 750 (`OTGW-Core.ino`, `replayNextOTGWSimulationLine()`). The capture tool (TASK-1503) records `otframes.log` with a recorded time per frame. A 1.0 s field cadence therefore replays at 0.75 s, and SAT timers that depend on real time drift (bench 2026-10-30: a capture with a 0.9995 s median gap replayed at 750 ms).

Options (TASK-1508): (a) the firmware reads an optional time per line and waits the recorded gap; (b) a host-side replayer streams frames with the recorded gaps. The maintainer chose (a) on 2026-11-04: no host dependency, the same file works from the web UI upload, and the cost is a small parser and six statics.

## Decision

1. **Format.** A replay line is either a bare frame, as today, or a tab separated line in the `otframes.log` layout `host_time TAB device_time TAB source TAB frame`. Only the last three columns are read, so `device_time TAB source TAB frame` also works. `device_time` is `HH:MM:SS` with an optional fraction of 1 to 6 digits (`HH:MM:SS.uuuuuu`). A line that does not match this shape is treated as a bare line. A line starting with `#` is a note and is skipped.
2. **Timing.** A timed line is rewritten in place to `<source><frame>` (what a bare file holds) and dispatched once the difference between its device time and the previous timed line's device time has elapsed. The difference wraps at midnight and is clamped to 0..60000 ms; a step backwards of more than half a day counts as a restarted clock and means no wait. The wait is measured with `millis()`, wrap-safe, from the ideal dispatch time of the previous line, so one late loop pass does not shift every later line; a replay more than 1 s behind restarts from now.
3. **Bare lines** keep the fixed interval (750 ms). A bare file and the built-in fixture replay exactly as before. The first timed line after a start goes out at once; after a wrap-around it follows the fixed interval.
4. **Streaming.** The file is still read one line at a time; the one waiting line stays in the existing line buffer.
5. **Status.** `GET /api/v2/simulate` reports `timing`: `"timed"` while the last line read carried a device time, otherwise `"fixed"`.

## Decision Contract

### Must

- A timed line is dispatched within 50 ms of its recorded gap (the device time difference), for gaps up to 60 s.
- A line without device time columns keeps the fixed `interval_ms` spacing.

### Must Not

- Must not load the replay file into RAM.
- Must not dispatch the tab separated line itself to the parser; the parser gets the bare `<source><frame>` form.

### Exceptions

- None.

### Verification

- `tests/test_replay_timing.py` (mixed gaps of 0.2 s, 1.0 s, 3.5 s and 0.9995 s within 50 ms; midnight wrap and the 60 s clamp; bare file and built-in fixture at 750 ms). Old code: the timed cases fail (fixed 750 ms, tab line dispatched unparsed).

## Alternatives Considered

- **Host-side replayer** (option b). Rejected by the maintainer: it needs a host process for every replay and a new upload or stream path; the firmware change is small.
- **A plain `<ms> <frame>` form.** Not added (YAGNI): the `otframes.log` layout is what the capture tool already writes.

## Consequences

**Positive:** a capture replays at its recorded pace; SAT timers see realistic gaps; bare files are unchanged.

**Negative:** the device logs a frame one frame late (scripts/README.md), so a recorded gap is the gap shifted by one frame; the replay reproduces that shift. A file with a gap above 60 s replays it as 60 s.

## Related Decisions

- ADR-177 (PIC byte stream verbatim): not involved; the replay synthesises its lines.
- ADR-080 (guideline-level with a test gate).

## References

- TASK-1508, TASK-1503 (otframes.log), TASK-1294 (built-in fixture).
- `src/OTGW-firmware/OTGW-Core.ino` (replay section), `scripts/otgw_capture.py` (`FRAMES_HEADER`), `docs/api/README.md` (simulate).
