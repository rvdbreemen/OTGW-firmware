---
id: "ADR-242"
title: "OT-Direct state has one writer: commands from other tasks run on the loop task through an ordered text ring"
status: "Proposed"
date: "2026-11-15"
binding: false
gate: "tests/test_otd_loop_task_commands.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "ot-direct"
  - "concurrency"
  - "command queue"
  - "freertos tasks"
  - "rest commands"
aliases:
  - "otdCmdTextRing"
  - "loop task single writer"
  - "async_tcp command race"
  - "GW=R race"
components:
  - "otdCmdTextRing"
  - "platformOnLoopTask"
symbols:
  - "otdCmdTextPush"
  - "otdCmdTextDrain"
  - "platformOnLoopTask"
  - "platformMarkLoopTask"
  - "addCommandToQueue"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-242 OT-Direct state has one writer: commands from other tasks run on the loop task through an ordered text ring

## Status

Proposed, 2026-11-15. Guideline-level per ADR-080 until accepted; the gate is `tests/test_otd_loop_task_commands.py`.

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: Claude (session 95) for Robert van den Breemen
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

On the OTGW32 (OT-Direct, no PIC) every OpenTherm command is handled by `handleOTDirectCommand()` in
`src/OTGW-firmware/OTDirect.ino`, which writes the OT-Direct state: the schedule table, the write caches and
overrides, the remote override, `otCurrentMode` and, for `GW=`/`GW=R`, the whole `resetTransientState()`.
`addCommandToQueue()` (OTGW-Core.ino) called it on the caller's FreeRTOS task. The REST handlers
(`/api/v2/otgw/commands`, `/api/v2/otdirect/mode`, the override actions, restAPI.ino) and the settings POST
(settingStuff.ino, `HW=`) run on the AsyncTCP task (`async_tcp`), while `loopOTDirect()` runs on the Arduino loop
task. So a REST `GW=R` reset the state while the loop task was in the middle of a frame, and a `CS=` sent right
after it in the same burst ran first on `async_tcp` and was then wiped by the reset (found in the TASK-1752
review, TASK-1761). TASK-1752 made only the frame ring safe (a consumer-side flush). MQTT command callbacks fire
from `MQTTclient.loop()` on the loop task (MQTTstuff.ino), and the port 25238 bridge, boot commands, the SC= clock
and SAT also run there, so REST and settings are the only other-task callers today.

## Decision

Every OT-Direct state change runs on the loop task: `addCommandToQueue()` and `sendPICSerial()` hand a command
that arrives on another task to `otdCmdTextPush()`, a ring of command texts in arrival order (8 slots, one kept
free to tell full from empty, so 7 commands) guarded by a portMUX for several producers, and the loop task runs
them with `otdCmdTextDrain()`.

- A queued command never runs while a master request is in flight. `GW=` and `GW=R` call `setOTDirectMode()` at
  once (they do not go through the TASK-1702 pending mode), so this rule is what keeps a REST mode switch from
  cutting a frame in half: `loopOTDirect()` drains at the start of a pass only with no request in flight.
- A caller on the loop task drains the ring first and then runs its own command directly, so it never overtakes
  an older queued command and keeps the synchronous `Sent`/`Dropped` result SAT relies on (TASK-1593). While
  commands wait and a request is in flight (`otdCmdTextMustWait()`), its command queues behind them instead and
  answers `Queued`, the answer a command to the PIC gets; the order wins over the synchronous result in that
  short window (found by the second-reader review, TASK-1761).
- A command from another task answers `Queued`; a full ring, or a text longer than `PIC_CMD_MAX_LEN`, answers
  `Dropped`, the REST handlers answer 503 for it instead of 202, and `addCommandToQueue()`/`sendPICSerial()` log it.
- `platformOnLoopTask()` (Platform library) compares the current task with the handle `setup()` marks with
  `platformMarkLoopTask()`; before the mark only the loop task runs, so it counts as the loop task.

## Decision Contract

### Must

- Run `handleOTDirectCommand()` only on the loop task.
- Keep the arrival order of commands from every task.
- Answer `Dropped` for a command the ring cannot hold, and report it (REST 503, a log line on the device).
- Run a queued command only with no master request in flight.

### Must Not

- Call `handleOTDirectCommand()` or `resetTransientState()` from `async_tcp` or any other task.
- Change the PIC path of `addCommandToQueue()`.

### Exceptions

- None.

### Verification

- `tests/test_otd_loop_task_commands.py`: the real ring block and the real OT-Direct branch of
  `addCommandToQueue()` compiled with a thread-local `platformOnLoopTask()`: a burst `GW=R`, `CS=20` from another
  thread runs on the loop thread in order; a loop-task `CS=` after a queued `GW=R` runs after it and keeps its
  result; a loop-task `CS=` that arrives while a request is in flight and a `GW=R` waits queues behind it and
  nothing runs until the request is done; a full ring is `Dropped` at the 8th command; two producer threads at once lose and reorder nothing; `loopOTDirect()` drains
  before `otMaster.process()` and only with no request in flight; the PIC part of `addCommandToQueue()` equals
  origin/dev; the REST command handler answers 503 on `Dropped`.

## Alternatives Considered

- **Defer only `GW=`/`GW=R` through the TASK-1702 pending-mode flag.** Rejected: commands after it in the same
  burst (boot `PS=0;GW=1`, a `CS=` right after `GW=R`) still run at once on `async_tcp` and the late reset wipes
  them, so it trades a race for an ordering bug.
- **A mutex around the OT-Direct state.** Rejected: the state is read and written all over `loopOTDirect()` and the
  frame callbacks; a lock across them would block the loop task behind a slow web request, and missing one access
  brings the race back.
- **Every command through the ring, also from the loop task.** Rejected: SAT needs the synchronous `Dropped` for a
  refused or unqueued command (TASK-1593), which a ring cannot give.
- **Do nothing.** Rejected: the REST mode switch and `GW=R` corrupt state written by two tasks (TASK-1761).

## Consequences

**Positive:**

- One writer for all OT-Direct state, so the TASK-1752 class of races cannot come back through a new REST command.
- Commands keep their order across REST, MQTT, the bridge, boot and SAT.

**Negative:**

- A REST command on OT-Direct runs at the next loop pass (tens of milliseconds) instead of at once; the REST answer
  was already 202 "queued", but `/api/v2/otdirect/mode` and the override actions return state that may not show
  the change yet. Mitigation: the comments at those handlers say so; the web UI re-reads the state.
- A SAT command that queues behind waiting commands (only while a REST command waits and a master request is in
  flight) answers `Queued`. SAT treats every answer but `Dropped` as delivered (SATcontrol.ino `!= Dropped` checks),
  so a later refusal by the handler (its frame ring full, TASK-1593) does not reach SAT for that one value; the
  next change or the 30 s `CS=` repeat (ADR-235) sends it again. This is what SAT always has on the PIC path.
- 8 × 18 bytes of RAM for the ring (7 commands).
- `platformOnLoopTask()` counts every task that is not the marked one as "other"; a new caller on a third task is
  handled correctly but has to be named in this ADR.

## Open Questions

- [ ] Robert: accept the one-writer rule as a binding pattern (gated by `tests/test_otd_loop_task_commands.py`)?

## Related Decisions

- **ADR-179** (owner of the control setpoint while SAT is enabled): unchanged; SAT's own commands keep their path
  and result.
- **ADR-213** (diagnostic recorder): `handleOTDirectCommand()` still records each command, now on the loop task.

## References

- TASK-1761, TASK-1752, TASK-1702, TASK-1593 (backlog).
- src/OTGW-firmware/OTDirect.ino: `// --- otdCmdTextRing` block, `loopOTDirect()` drain.
- src/OTGW-firmware/OTGW-Core.ino: `addCommandToQueue()` OT-Direct branch, `sendPICSerial()`.
- src/libraries/Platform/src/platform_esp32.h: `platformOnLoopTask()`, `platformMarkLoopTask()`.
- src/OTGW-firmware/restAPI.ino: 503 on `Dropped`.
- tests/test_otd_loop_task_commands.py.
