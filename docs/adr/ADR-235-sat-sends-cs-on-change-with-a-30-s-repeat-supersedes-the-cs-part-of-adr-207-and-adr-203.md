---
id: "ADR-235"
title: "SAT sends CS= on change with a 30 s repeat (supersedes the CS part of ADR-207 and ADR-203)"
status: "Proposed"
date: "2026-11-14"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-203"
  - "ADR-207"
topics:
  - "sat"
  - "opentherm"
  - "command-cadence"
  - "control-setpoint"
aliases:
  - "CS on change"
  - "30 s CS repeat"
  - "5 s resend"
  - "SAT_CS_REPEAT_MS"
  - "PIC CS expiry"
  - "Q-337b"
components:
  - "SAT CS= send cadence"
symbols:
  - "satSendCS"
  - "satCmdResend"
  - "satPicCSKeepalive"
  - "SAT_CS_REPEAT_MS"
  - "SAT_CS_RETRY_MS"
  - "SAT_PIC_CS_KEEPALIVE_MS"
  - "satContinuousClampRefresh"
  - "satContinuousSetpoint"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-235 SAT sends CS= on change with a 30 s repeat (supersedes the CS part of ADR-207 and ADR-203)

## Status

Proposed, 2026-11-14. Guideline-level per ADR-080: verified by host tests, no evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: Agent (d3)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Related to ADR-203
    changed_via: adr-kit lifecycle
  - date: 2026-11-14
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Related to ADR-207
    changed_via: adr-kit lifecycle
```

## Context

While SAT controls the boiler it sends the control setpoint as `CS=<value>` (CS: control setpoint, OpenTherm MsgID 1) to the
PIC, or to the OT-Direct command handler. ADR-203 made every control tick send CS=, MM= and CH=, with a 5 s resend between
ticks, as thermo-nova does: its heating loop runs every 5 s (`climate.py:706-710` at the pinned commit 7beaaf5) and sends all
three at the end of each update (`heating_control.py:236-238`). ADR-207 moved MM=, TP= and CH= to on-change plus a 60 s
repair (Sergeant D entries 237, 239) and kept CS= on the 5 s resend.

On 2026-11-14 Robert ordered CS= to go out only when its value changes, and at least once a minute while unchanged
(Sergeant D corrections entry 334); asked 60 s or 30 s, he chose 30 s ("It give a margin", entry 334). Sergeant D agreed
(entry 335: "the max limit is to send a fresh CS value every minute otherwise on the CS value change").

The constraint is the PIC's CS expiry: the PIC drops a control setpoint of 8 C or more when no CS= arrived in its expiry
window (`gateway.asm` CommandExpiry, measured at about 63 s in TASK-1834). A repeat every 30 s keeps a 2x margin.
OT-Direct does not expire a CS while SAT owns MsgID 1 (`OTDirect.ino`, TASK-1150), so there the repeat only repairs a lost
command.

## Decision

SAT sends CS= when the value it sends, rounded to 0.1, changes, and repeats an unchanged CS= about every 30 s: the control
tick repeats it once it is 29 s old, and a loop repeat timed on raw `millis()` sends it once it is 31 s old when no tick did.

- `satSendCS()` (`SATcontrol.ino:687`) skips a value equal to the last accepted one while that one is younger than
  `SAT_CS_REPEAT_MS` (30 000 ms, `:600`) minus a 1 s slack. A cache reset (`satResetCmdCache()`: hand-back, PIC restart,
  OpenTherm link back, boot) stamps the cache stale, so the first CS= after it always goes out.
- `satCmdResend()` (`:793`) runs every loop pass and repeats the last accepted CS= once it is 31 s old
  (`SAT_CS_REPEAT_MS` + the 1 s slack). Its gates stay: armed by a tick send, SAT active, an OT command interface, and
  paused by the live DHW and fault bits. The 31 s, not 30 s, keeps a loop repeat out of the pass of a 30 s tick: the tick
  repeats the CS itself from 29 s, so at the default interval the loop repeat never fires, and at a 60 s interval it keeps
  the largest gap at about 31 s (TASK-1924 Part 2; at 30 s the loop sent the old value 2-6 ms before a tick that brought a
  new one, in 29 of 37 changes on the beta.242 bench).
- A CS= that the command queue dropped (TASK-1574 commit-on-success) is retried after `SAT_CS_RETRY_MS` (5 s) with the value
  that was dropped. Before, the 5 s resend repeated the older cached value.
- `satPicCSKeepalive()` (`:721`, PIC boards, CS of 8 C or more, also during hot water) waits 31 s as well
  (`SAT_PIC_CS_KEEPALIVE_MS`, `:719`). It runs first in the loop, so on a PIC board it was the sender of the old value at a
  30 s keepalive. It shares the last-sent stamp with the repeat, so the two send one repeat per 31 s together, not two.
- In continuous mode the CS is also recomputed every 5 s while the flame is on (`satContinuousClampRefresh()`, `:2577`,
  timer `:963`; Sergeant D entry 337, thermo-nova's 5 s heating loop `climate.py:706-710`) and goes out through
  `satSendCS()`, so on change only. The value comes from `satContinuousSetpoint()` (`:2560`), shared with the tick: for
  60 s after a CH flame-on (`SAT_CONT_STARTUP_MS`, `:951`) max(request, flow - `fModSupOffset`), then the entry-112 clamp
  (Robert entry 338, Sergeant D entries 344 and 347). That control rule is Sergeant D's (recorded in
  `docs/sat/sergeant-d-corrections.md`); this ADR records only that its output uses the on-change sender.
- The no-return start-up flood (TASK-1371, Sergeant D entries 45, 142, 149, 151) keeps its own cadence of one CS= every
  2 s, also when the value is unchanged (`satSendCS(cs, true)`, `:1987`, `:1999`). Entry 334 is read as being about the
  control tick. Whether the flood should also go on-change is queued as a low-priority question for Sergeant D (through the
  comms session); this ADR keeps it as it is.
- MM=, TP= and CH= are unchanged: on change plus the 60 s repair (ADR-207, entries 237 and 239).

## Decision Contract

### Must

- Send CS= at once when the 0.1-rounded value changes.
- Repeat an unchanged CS= no later than 31 s after the last accepted CS= while SAT is active and not paused.
- Time the repeat on `millis()` in the loop, not on the control tick (a tick-timed check slips to 60 s, the TASK-1602 lesson).
- Retry a dropped CS= with its own value within 5 s.

### Must Not

- Repeat an unchanged CS= more often than every 29 s, except in the start-up flood.
- Let the PIC keepalive and the repeat send two repeats in the same window.
- Send a loop repeat or PIC keepalive in the 30 s after the last accepted CS= (it would race the 30 s tick and send the old
  value just before a new one).

### Exceptions

- The no-return start-up flood (TASK-1371) sends every 2 s by design.
- `satEnqueueIfChangedCS()` (the 3 s PWM suppression refresh, entry 23) stays change-checked with its own 5 min staleness
  window; the 30 s repeat covers it in between.

### Verification

- `tests/test_sat_cmd_cache.py::CmdCacheReset::test_no_stale_repeat_just_before_a_30s_tick`: 10 stale repeats in 5 min of
  30 s ticks on the code before TASK-1924 Part 2, 0 on this decision; a mutant with the keepalive back at 30 s also gives
  10. `test_largest_gap_at_a_60s_interval_stays_inside_the_pic_expiry`: largest gap at most 31 s.
- `tests/test_sat_continuous_startup.py`: the real `satContinuousSetpoint()` and the 5 s refresh structure (gates, ceiling
  cap before `satSendCS(cs)`).
- `tests/test_sat_cmd_cache.py::CmdCacheReset::test_steady_state_sends_two_cs_per_minute_and_a_change_at_once`: 60 s of
  steady state sends 2 CS= on this decision and 13 on the code before it (origin/dev `SATcontrol.ino` before TASK-1924).
- `test_resend_repeats_the_unchanged_cs_after_30s_not_before`,
  `test_dropped_satsendcs_is_retried_with_its_own_value_after_5s`, `test_pic_keepalive_and_the_repeat_share_one_30s_window`
  in the same file; `tests/test_sat_ceiling_paths.py` (the CS sender inventory, including the forced flood sends).
- Bench, beta.250 on a Classic with PIC (TASK-1924, 09): at a 30 s interval no old CS= within 1 s before a new value (beta.242:
  29 of 37), largest gap 30.10 s; at a 60 s interval largest gap 31.00 s, no PIC expiry; the 5 s recompute stepped the CS
  down 0.1 at a time with a falling flow. Open: AC6 (no PIC CS expiry over 30 min continuous).

## Alternatives Considered

- **Keep the 5 s resend (ADR-203, ADR-207).** Rejected: Robert's order, entry 334. It sends 12 to 13 CS= per minute for an
  unchanged value, which fills the PIC command queue and the logs without changing anything at the boiler.
- **Repeat every 60 s.** Rejected by Robert (entry 334): 60 s sits right at the PIC's ~63 s expiry; a delayed command could
  let the setpoint lapse.
- **Fold the PIC keepalive into the repeat.** Rejected: the keepalive also runs during hot water and boiler faults, where
  the repeat pauses (entry 232). Sharing the stamp gives the same single cadence without losing that.
- **Put the flood on-change too.** Not done in this ADR: it would change a cadence Sergeant D set (entries 45, 142, 149,
  151) without asking him. Queued as a question.

## Consequences

**Positive:**

- About 2 CS= per minute instead of 12 to 13 for an unchanged setpoint (host test above).
- No out-of-date CS= right before a tick that changes it.
- A dropped CS= is retried with the value SAT meant to send, not the stale one.

**Negative:**

- A command lost outside the queue (for example a PIC that missed a line) is corrected after up to 30 s instead of 5 s.
  Mitigation: 30 s stays inside the ~63 s PIC expiry with a 2x margin, as Robert chose.
- A setpoint limit lowered between ticks reaches the boiler at the next tick or repeat (up to 30 s) instead of 5 s
  (TASK-1536 capping is unchanged, only its occasion is rarer).
- This deviates from thermo-nova, which sends CS on every 5 s update.

## Open Questions

- [ ] Should the no-return start-up flood (TASK-1371) also send CS= on change only? (queued for Sergeant D, recommendation: keep 2 s)

## Related Decisions

- **ADR-203 (SAT sends CS MM and CH on every control update)**: this ADR supersedes its CS cadence.
- **ADR-207 (SAT sends MM, the Immergas TP and CH on change, CS keeps the 5 s resend)**: this ADR supersedes its "CS keeps
  the 5 s resend" part; its MM/TP/CH rules stay.

## References

- TASK-1924; Sergeant D corrections entries 334, 335, 337, 338, 344, 347 (`docs/sat/sergeant-d-corrections.md`), 44, 237, 239.
- `src/OTGW-firmware/SATcontrol.ino:600`, `:687`, `:719`, `:721`, `:793`, `:951`, `:963`, `:1987`, `:1999`, `:2560`, `:2577`
  (lane-1924 at beta.250, baf84d9).
- thermo-nova 7beaaf5: `custom_components/sat/climate.py:706-710`, `custom_components/sat/heating_control.py:236-238`.
- TASK-1834 (PIC CS expiry measured at about 63 s on the bench).
