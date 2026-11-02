---
id: "ADR-207"
title: "SAT sends MM, the Immergas TP and CH on change, CS keeps the 5 s resend (partly supersedes ADR-203)"
status: "Accepted"
date: "2026-11-02"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-203"
topics:
  - "sat"
  - "opentherm"
  - "command-cadence"
  - "max-modulation"
  - "ch-enable"
aliases:
  - "MM on change"
  - "CH on change"
  - "Immergas TP on change"
  - "write-on-change"
  - "SAT_CMD_REFRESH_MS"
  - "60 s repair resend"
  - "Q-1537c"
  - "Q-1570a"
components:
  - "SAT MM and CH command cache"
symbols:
  - "satSendMM"
  - "satSendCH"
  - "satResetCmdCache"
  - "satNotifyPicRestart"
  - "satCmdResend"
  - "SAT_MMCH_REPAIR_MS"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-207 SAT sends MM, the Immergas TP and CH on change, CS keeps the 5 s resend (partly supersedes ADR-203)

## Status

Accepted, 2026-11-02.

The cadence is Sergeant D's decision in entry 237 of `docs/sat/sergeant-d-corrections.md` (2026-11-02, Discord messages 1566917384483381390 and 1566917834351575095). The 60 s repair resend is his decision too: entry 238 confirmed the resend after a cache reset or PIC restart, and entry 239 set the repair period to one minute (Discord message 1566926416170455091). The maintainer agreed (Discord messages 1566928520410832958, 1566928637830103083, 1566928681140490352). Q-1570a is answered.

This ADR replaces ADR-203 for MM and CH only. ADR-203 stays in force for CS (its 5 s resend). The front-matter `supersedes` list stays empty and ADR-203 is not touched by this proposal (see Consequences for what acceptance needs).

## Status History

```yaml
status_history:
  - date: 2026-11-02
    status: Proposed
    changed_by: Claude
    reason: Initial proposal from TASK-1570 and Sergeant D entry 237
    changed_via: adr-kit
  - date: 2026-11-02
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer akkoord 2026-11-02; implements Sergeant D entries 237-239 (MM, TP and CH on change, once-a-minute repair resend, CS keeps the 5 s resend)
    changed_via: adr-kit lifecycle
  - date: 2026-11-02
    status: Accepted
    changed_by: Robert van den Breemen
    reason: ADR-207 partly supersedes ADR-203 for MM and CH (Sergeant D entries 237-239); CS keeps ADR-203's 5 s resend
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat) drives the boiler through the OpenTherm (OT) Gateway (OTGW) PIC (Peripheral Interface Controller, the gateway's microcontroller) with four commands: CS= (control setpoint), MM= (maximum modulation level), CH= (central heating enable) and, on an explicit Immergas boiler, TP=11:12 (transparent slave parameter write of the maximum modulation level, `min(MM, 80)`).

ADR-203 (Accepted, 2026-10-30) made SAT resend CS, MM and CH every 5 s, also when unchanged. That followed Sergeant D's entry 44 (Q13, "Follow thermo-nova"). thermo-nova does resend on its 5 s loop (`climate.py:706-710`, `coordinator/mqtt/opentherm.py:211-220`).

On 2026-11-02 Sergeant D changed this. Entry 237 answers Q-1537c: "Apply it on every boiler. Typical or Immergas. Send MM, TP command on every change." and, two minutes later, "Moreover we can do the same for the CH command." Entry 236 was the first proposal. Entry 237 changes entry 44 for MM and CH. CS is not named, so CS keeps its 5 s resend.

The repeat is not needed for MM and CH because the PIC does not expire them. It expires only SC, CS and C2 (`other-projects/otgw-6.6/gateway.asm:493-496` heartbeat flags, `:1619-1641` command expiry). `MM=` stays until a new command (`:4752-4764`), `CH=` likewise, and `TP=` writes a boiler parameter. OT-Direct (the PIC-less path) expires only CS, C2 and SC (`src/OTGW-firmware/OTDirect.ino:450-463`, `:2161-2180`). So the 5 s repeat of MM, CH and TP only loads the PIC command queue. TASK-1567 drips that queue at about one command per second, which makes the load matter.

This is a deliberate departure from thermo-nova, recorded as such: the ground-truth order of ADR-196 puts Sergeant D's recorded decision (entry 237) above the thermo-nova code.

## Decision

SAT sends MM=, the Immergas TP=11:12 and CH= to the PIC only when the value changes. CS= keeps the 5 s resend of ADR-203 and the 30 s PIC keepalive. This applies to every boiler, typical or Immergas.

The repair resend is Sergeant D's decision (entries 238 and 239; our first proposal was 300 s and he shortened it to one minute). `satSendMM()` and `satSendCH()` in `src/OTGW-firmware/SATcontrol.ino` return early only when all three hold: the value equals the last value sent, the matching `_sat_mmSentValid` or `_sat_chSentValid` flag is set, and the last send is younger than `SAT_MMCH_REPAIR_MS` (60 s). Otherwise they send. The repair resend covers three cases a change-only rule cannot see: a dropped enqueue (`addCommandToQueue()` returns void, and a full queue or a PIC reset in progress can drop a command, TASK-1555), an external `MM=` or `CH=` from the REST (Representational State Transfer) API (application programming interface), MQTT (Message Queuing Telemetry Transport), Home Assistant or port 25238, and a PIC restart that was missed.

`satResetCmdCache()` clears both flags, so the next call sends. It runs on hand-back, when the OT link returns, when SAT is enabled and at boot. A new `satNotifyPicRestart()` (it calls `satResetCmdCache()`) is called from the runtime PIC banner branch in `OTGW-Core.ino` `processOT()`: the "OpenTherm Gateway" banner means the PIC started and forgot MM and CH (`gateway.asm:1281`). `satCmdResend()` (5 s) now repeats CS only.

## Decision Contract

### Must

- MM=, the Immergas TP=11:12 and CH= are enqueued only on a changed value, after a cache reset, or when the last send is 60 s old or older (`SAT_MMCH_REPAIR_MS`). `SAT_CMD_REFRESH_MS` (300 s) stays the staleness window of the change-checked CS helper `satEnqueueIfChangedCS()` and of the stale timestamps in `satResetCmdCache()`; it is not the repair period.
- The Immergas TP follows MM: it goes out in the same call as the MM it derives from.
- `satResetCmdCache()` runs on hand-back, OT link back, SAT enabled and boot; `satNotifyPicRestart()` runs from the runtime PIC banner branch.
- `satCmdResend()` repeats CS only.

### Must Not

- The 5 s resend must not repeat MM, TP or CH.
- The CS resend (5 s) and the 30 s PIC CS keepalive must not be removed or slowed by this ADR.

### Exceptions

- None.

### Verification

- `tests/test_sat_cmd_cache.py` (18 tests): send on change, resend after a PIC restart, Immergas TP follows MM, the 60 s repair resend, and a structural check that the banner branch calls `satNotifyPicRestart`. On the beta.17 code 9 tests fail and 1 errors; on the change all pass.

## Alternatives Considered

- **Keep ADR-203 unchanged (resend CS, MM and CH every 5 s).** Rejected: Sergeant D decided otherwise in entry 237, and the repeat has no effect on the boiler because the PIC does not expire MM or CH. It costs two extra PIC queue slots per 5 s cycle.
- **Change-only with no repair resend.** Rejected: one dropped enqueue, one external `MM=` or `CH=`, or one missed PIC restart would leave the boiler on a wrong value until the next real change, which on a steady load can be hours. The 60 s resend bounds that error at one minute for about 0.017 commands per second per command (one per minute).
- **Repair resend every 300 s (5 minutes, our first proposal).** Rejected by Sergeant D in entry 239 in favour of one minute: a smaller interval between these commands matters, especially during start-up, where a free PIC queue lets the CS refresh fast. The maintainer agreed because the queue redesign (TASK-1567) makes the queue faster.
- **Repair resend on a still shorter period (for example 30 s).** Not chosen: Sergeant D set one minute, and a shorter period spends more queue slots for a fault that is rare.
- **Detect a PIC restart only by the repair timer.** Rejected: the banner is an exact signal at no cost, so the cache is cleared at once instead of after up to 60 s (entry 238 confirmed the resend after a PIC restart).

## Consequences

**Positive:**

- Steady SAT load drops to one CS per 5 s plus MM and CH only on a change or once a minute. This fits TASK-1567, which drips the PIC queue at about one command per second.
- The decision follows Sergeant D's entry 237 for every boiler type, with one rule instead of a typical/Immergas split.

**Negative:**

- An external `MM=` or `CH=` (REST, MQTT, Home Assistant, port 25238) now stays in force for up to 60 s instead of 5 s.
- When a command is dropped, the boiler keeps the old MM or CH for up to 60 s.
- This departs from thermo-nova's 5 s resend. A future thermo-nova re-diff must treat this as intended.
- ADR-203 needs a "partly superseded by ADR-207" note at acceptance. adr-kit has no partial-supersession form: `adr supersede ADR-203 --by ADR-207` would mark all of ADR-203 Superseded and so also drop its CS rule. The maintainer decides at accept time; the expected route is `adr relate ADR-207 --to ADR-203` (reference bookkeeping) and a pointer in ADR-203's text, as was done for ADR-199.

## Open Questions

- [x] Q-1570a: keep a repair resend of an unchanged MM, TP and CH (our first proposal: every 300 s, Discord message 1566922842015006882), or send strictly on change? **Answered 2026-11-02 by Sergeant D (entry 239, message 1566926416170455091), the maintainer agreeing:** resend once a minute when nothing changes; implemented as `SAT_MMCH_REPAIR_MS` (60 s).

## Related Decisions

- **ADR-203 (SAT sends CS MM and CH on every control update)**: partly superseded. Its 5 s resend of CS stays; for MM and CH it is replaced by this ADR. Not marked Superseded (see Consequences).
- **ADR-196 (SAT behaviour ground truth)**: the source order this ADR follows (entries 237 to 239 above thermo-nova).
- **ADR-179 (MsgID 1 ownership)**: SAT's own CS= still goes through `satEnqueueOwnCS()`; unchanged.
- **ADR-080 (Binding ADR rules must have a CI (continuous integration) gate)**: this ADR is guideline-level with a test gate.

## References

- `docs/sat/sergeant-d-corrections.md` entries 237, 238 and 239 (this decision and the 60 s repair resend), entry 236 (first proposal), entry 44 (changed for MM and CH), with their Discord links.
- thermo-nova (pinned copy `other-projects/SAT-releases-thermo-nova/`): `climate.py:706-710`; `coordinator/mqtt/opentherm.py:211-220`.
- PIC firmware: `other-projects/otgw-6.6/gateway.asm:493-496`, `:1281`, `:1619-1641`, `:4752-4764`.
- OT-Direct: `src/OTGW-firmware/OTDirect.ino:450-463`, `:2161-2180`.
- Firmware: `src/OTGW-firmware/SATcontrol.ino` (`satSendMM`, `satSendCH`, `satResetCmdCache`, `satCmdResend`, `SAT_MMCH_REPAIR_MS`), `src/OTGW-firmware/OTGW-Core.ino` (`processOT()` banner branch).
- Tasks: TASK-1570 (implementation), TASK-1555 (dropped enqueue), TASK-1567 (PIC queue drip).
- Test: `tests/test_sat_cmd_cache.py`.

No Enforcement block: the rule is about which SAT call sites send which command and when, which no path-scoped regex can state. The named test is the gate.
