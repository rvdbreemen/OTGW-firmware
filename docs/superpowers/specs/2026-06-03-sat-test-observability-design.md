# SAT Test Observability — Design

Date: 2026-06-03
Status: Approved (brainstorm), pending implementation plan
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support
Component: Smart Thermostat (SAT) + Web Interface (live-log)

## Problem

During SAT testing (especially simulation mode) there is no single, plain-language
account of *what is happening and why*. State exists but is scattered and terse:

- SAT dashboard shows **state labels** (boiler status, cycle quality, simple status line).
- Telnet shows **terse engineer logs** (`SAT SIM: flame ON/OFF`, `simulation started`).
- WebSocket emits a few **one-shot alerts** (`'!'` simulation blocked bus tx).

An observer (demo watcher, developer at the bench, or remote field tester) cannot
follow the test as a running narrative. Two explicit requirements:

1. The **Telnet log** must carry SAT simulation narration so it is clear what is happening.
2. The **Web UI** must give a human-readable account, with **minimal backend impact**.

## Decision

Add a single **narration source** that emits each line once and fans it out to **two
sinks** so they never drift:

- **Telnet** (`DebugTln`) — the engineer log.
- **Web UI live-log** (`sendEventToWebSocket('S', ...)`) — reuses the existing WS OT-log
  stream; SAT lines carry the dedicated prefix char `S`.

Narration is **always-on and transition-only** (no periodic/heartbeat lines), so volume
stays low whether simulating or driving a real boiler. The Web UI gets a **"SAT only"**
filter toggle on the existing log panel.

### Why this approach

- Satisfies both requirements with one mechanism; Telnet and Web UI cannot disagree.
- Reuses existing infrastructure (`DebugTln`, `sendEventToWebSocket`); no new REST,
  settings, or persisted state.
- Transition-only volume avoids the over-logging failure mode seen with the MQTT
  status gate.

### Rejected alternatives

- **Web-UI-only synthesis** (browser diffs `/sat/status` snapshots): leaves Telnet bare,
  fails requirement 1.
- **Dedicated SAT narration card + ring buffer**: more frontend + a little backend state
  for history-on-load. Deferred (YAGNI); the live stream + filter is enough.

## Architecture

```
SAT transition (flame, cycle, scenario, mode, safety, sim, window/summer)
        │
        ▼
  satNarrate_P(msg)  /  satNarratef_P(fmt, ...)     // SATcontrol.ino
        │  (format once into one stack buffer, snprintf_P)
        ├──────────────► DebugTln(buf)                    → Telnet (port 23)
        └──────────────► sendEventToWebSocket('S', buf)   → WS OT-log → browser panel
                                                              └─ optional "SAT only" filter
```

`SATcontrol.ino` is part of the single Arduino translation unit, so it may call the
file-static `sendEventToWebSocket` (defined in `OTGW-Core.ino`) directly. `S` is an
unused prefix (`!`, `*`, `.` are taken).

## Components

### Firmware — narration helper (`SATcontrol.ino`)

- `static void satNarrate_P(PGM_P msg_P)` — fixed PROGMEM line.
- `static void satNarratef_P(PGM_P fmt_P, ...)` — printf-style for lines with live
  temps/percentages. Builds into one reused stack buffer (~96 bytes) with `vsnprintf_P`.
- Both emit to Telnet (`DebugTln`) and Web UI (`sendEventToWebSocket('S', buf)`).
- No `String` (ADR-004). All literals in PSTR.

### Firmware — call sites (transition-only, ~10)

Replace/augment the existing terse `SATDebugTln` edges so there is no double-logging:

| Transition | Example line |
|---|---|
| Simulation start / stop | `Simulation started — synthetic boiler, no bus traffic` |
| Heat demand begins | `Heat demand: room 18.4°C → target 21.0°C, setpoint 45.0°C` |
| Flame ON | `Flame lit — flow 22°C below setpoint 45°C` |
| Flame OFF | `Flame off — flow reached 47°C (setpoint + margin)` |
| Cycle classified | `Cycle complete: GOOD — 6m50s on, return ΔT 18°C` (Good/Short/Overshoot/Underheat/Uncertain) |
| Scenario injected | `Scenario: sensor noise injected (60s)` / `DHW draw simulated` |
| Window / summer | `Window open detected — heating paused` / `Summer mode — heating off` |
| Control mode change | `Control mode → PWM` |
| Safety trip / clear | `SAFETY TRIPPED: <reason>` / `Safety cleared` |
| Real boiler detected | `Real boiler detected on bus — simulation disabled` |

Deliberately **excluded** (anti-spam): continuous modulation ticks, per-frame
temperatures, periodic heartbeats. Only state transitions narrate.

### Web UI (`data/`)

- The existing live-log panel already renders WS lines; `S`-prefixed lines appear with no
  change.
- **"SAT only" toggle** in the log panel:
  - `index.html` — a toggle control in the log toolbar.
  - `index.js` — client-side filter: when on, show only lines whose prefix is `S`.
  - `components.css` — optional severity coloring of `S` lines (green = progress,
    amber = cycle verdict / attention, red = pause / fault / safety). Marked optional;
    can ship monochrome first.
- Honors the log container contract: `.ot-log-content` is `white-space: pre`,
  `\n`-separated, prefer `textContent`.

## Data flow

1. A SAT state transition fires inside the existing control/sim loop.
2. The narration helper formats one line and writes it to both sinks.
3. Telnet clients see it immediately.
4. Web UI clients receive it on the WS log stream; the panel renders it, and the
   "SAT only" toggle optionally filters the view client-side.

## Error handling

- Helper is null-safe on its message pointer.
- The two sinks are independent: if the WebSocket is down, the Telnet line still emits
  (and vice-versa). No new shared failure mode.
- `sendEventToWebSocket` already bounds the buffer copy; the helper bounds its own
  `snprintf_P`. No heap allocation.

## Platform / abstraction

- Helper uses only platform-neutral calls (`DebugTln`, `sendEventToWebSocket`); no raw
  `#ifdef ESP8266/ESP32`. SAT is a 2.0.0 subsystem; no new `HAS_*` flag needed.

## Testing

- **Web UI (Playwright + Chrome):** reuse the IP-octet harness pattern. Inject a mixed
  set of log lines (OT frames + `S` narration) into the panel, assert the "SAT only"
  toggle filters to exactly the `S` lines, and assert optional severity styling.
- **Firmware:** `python build.py` clean on esp32 + esp8266 (grep per-env bins);
  `python evaluate.py --quick` green (PROGMEM, ADR-004 String gate).
- **Manual / sim:** enable simulation, confirm Telnet and the web log both narrate at
  each transition (flame, cycle, scenario, window, safety) and stay quiet between
  transitions.

## Scope guard (YAGNI)

Not in this design: dedicated narration card, history-on-load ring buffer, per-session
debug toggle, new REST endpoint, new settings, or an ADR (small feature within the
existing `sendEventToWebSocket` / `DebugTln` patterns; `S` prefix is a convention, not a
new pattern).

## Versioning / delivery

Firmware + LittleFS change → prerelease bump per policy. Single backlog task; commit
prefix `feat(sat)` / `feat(webui)`. 2.0.0 branch only (SAT is 2.0.0-specific; no dev port).
