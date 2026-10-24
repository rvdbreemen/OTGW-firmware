# Open questions for Sergeant D

This is the queue of SAT questions that the firmware port cannot answer by itself.
Sergeant D (George) designed SAT, so his answer decides. We ask **one question at a
time** in #alpha-testing, in plain English, with the possible answers and our recommendation.
Each answer is recorded in [`sergeant-d-corrections.md`](sergeant-d-corrections.md)
and then becomes a backlog task.

George: if a question below is unclear or wrongly put, just say so. A correction of
the question is as useful as an answer.

Status values: **queued** (not asked yet), **asked** (waiting for the answer),
**answered** (recorded as an entry), **dropped** (no longer needed, with the reason).

| # | Status | Topic | Blocks |
|---|---|---|---|
| Q1 | answered (entry 19: remove it) -> TASK-1341 | Keep the firmware's "PWM auto switch" setting at all? | nothing |
| Q2 | asked (2026-10-24) | End-of-cycle switch: compare the tail flow with the control setpoint per sample, as thermo-nova | TASK-1331 |
| Q3 | queued | Overshoot margin 2.0 equals the flow offset 2.0 | TASK-1331 |
| Q4 | queued | Classifier steps the firmware keeps that classifier.py does not have | TASK-1331 |

More questions follow from the SAT port audit of 2026-10-24
([`sat-port-audit-2026-10-24.md`](sat-port-audit-2026-10-24.md)).

## Q1. Keep the "PWM auto switch" setting?

**Context.** The firmware has a setting "PWM auto switch" (on by default,
`SATtypes.h` `bPwmAutoSwitch`). When it is off, SAT does not use the 60-second
overshoot switch into PWM (`SATcycles.ino` `satCycleCheckAutoSwitch`; thermo-nova
`heating_control.py:280-304`). thermo-nova has no such setting. Entry 18 already
says the new end-of-cycle switch ignores it.

**Proposals.**
- A. Remove the setting, as we did with Force PWM (entry 13). SAT always picks the
  mode itself, exactly as thermo-nova.
- B. Keep it as an expert option for the 60-second switch only.

## Q2. End-of-cycle switch: per-sample control setpoint

**Context.** thermo-nova classifies a cycle by comparing each tail sample's flow
with that sample's own control setpoint (`cycles/tracker.py:267-271`,
`sample.control_setpoint`). Our classifier uses one control setpoint taken when the flame came on.
Once the classification switches the mode (entry 15), this matters: if SAT leaves
PWM in the middle of a burn, the flow is compared with the old, low PWM setpoint and
the cycle looks like an overshoot, so SAT goes straight back to PWM at flame-off.
thermo-nova does not do that.

**Proposals.**
- A. Store the control setpoint per tail sample, as thermo-nova (about 720 bytes of
  memory), and only then switch modes at the end of a cycle. (Recommended.)
- B. Ship the end-of-cycle switch with the per-cycle setpoint and accept the
  ping-pong risk.
- C. Keep the end-of-cycle switch off until A is done.

## Q3. Overshoot margin 2.0 and the flow offset 2.0

**Context.** You chose margin 2.0 for the end-of-cycle overshoot (entry 15;
thermo-nova uses 3.0, `cycles/const.py:10`). In continuous mode thermo-nova keeps
the control setpoint at least at the boiler temperature minus the flow offset, 2.0
by default (`heating_control.py:485-489`, `const.py:154`), so a clamped sample has an error of
at most 2.0, below thermo-nova's 3.0. With margin 2.0 and "2.0 or more counts", a
cycle whose tail sits on that clamp would count as an overshoot and switch to PWM.

**Proposals.**
- A. Keep 2.0 but count only "more than 2.0".
- B. Use 3.0, as thermo-nova.
- C. Keep 2.0 and "2.0 or more" (accept that clamped cycles switch to PWM).

## Q4. Classifier steps that classifier.py does not have

**Context.** Entry 15 asked us to align the cycle classifier with `classifier.py`
first. Our classifier still has steps that thermo-nova does not have: a "short
cycle" result for continuous cycles (after the overshoot test), "shorter than
120 s = uncertain", and no "uncertain" result when the cycle kind is unknown or
percentiles are missing (`classifier.py:22-23`, `:42-43`, `:71-72`). Other parts of
the firmware read the "short cycle" result (flame status, cycle health).

**Proposals.**
- A. Remove all of them and match `classifier.py` exactly.
- B. Keep the continuous "short cycle" result (used for display only) and remove the
  rest.
- C. Keep them as they are.
