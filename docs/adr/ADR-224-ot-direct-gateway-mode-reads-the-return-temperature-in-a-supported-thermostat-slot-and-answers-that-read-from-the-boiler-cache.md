---
id: "ADR-224"
title: "OT-Direct gateway mode reads the return temperature in a supported thermostat slot and answers that read from the boiler cache"
status: "Accepted"
date: "2026-11-10"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "otdirect"
  - "opentherm"
  - "gateway-mode"
  - "return-temperature"
  - "sat"
  - "pic-parity"
aliases:
  - "MsgID 28 on OT-Direct"
  - "Tret read without thermostat"
  - "cache-answered slot"
  - "TASK-1759 option A"
  - "entry 290"
components:
  - "OT-Direct gateway-mode return-temperature slot (cache-answered thermostat read)"
symbols:
  - "otdAltSlotTaken"
  - "otBoilerCache"
  - "otBoilerCacheStore"
  - "replyToThermostat"
  - "feedTret"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-224 OT-Direct gateway mode reads the return temperature in a supported thermostat slot and answers that read from the boiler cache

## Status

Accepted, 2026-11-10.

**Decision Maker:** to be decided by User: Robert van den Breemen (maintainer). The choice of option A+C itself was his, made on 2026-11-09 at 21:52:17 UTC (Coordinated Universal Time) in #dev-sat-mqtt: "I think A+C would be best, for each hardware setup the optimal solution." (msg 1569469705461440577; corrections entry 290).

This ADR (architecture decision record) records option A's deviation from the PIC (the Microchip PIC microcontroller of the classic gateway boards) model and its parameters. It was written by agent otgw-firmware-030509. On 2026-11-10 06:40 Robert answered "Direction OK, 8 B cache" (AskUserQuestion in the coordinator's session, relayed to 030509): the deviation from ADR-223 is accepted for this one read per interval, and the 8 B variant is chosen. Sergeant D answered the interval, the scope and his agreement on 2026-11-10 (W1-W3, Q-1759a; corrections entry 291). On 2026-11-10 ~14:40 Robert answered "Accept, scope OT-Direct" (AskUserQuestion in the coordinator's session): option A only; option C (`AA=28` on PIC boards) is out of scope and follows in its own ADR and task after Sergeant D answers Q-1759b. The status change to Accepted is recorded by the coordinator with `adr accept`.

## Status History

```yaml
status_history:
  - date: 2026-11-09
    status: Proposed
    changed_by: "Agent: otgw-firmware-030509"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-10
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Accepted by Robert on 2026-11-10 in the coordinator session, scoped to the direct gateway mode only, as the Decision states.
    changed_via: adr-kit lifecycle
```

## Context

SAT (Smart Autotune Thermostat, the firmware port of thermo-nova) needs the boiler return temperature, OpenTherm MsgID 28 (message identifier 28, the return water temperature). Sergeant D said so in entry 287: "We need to ask for return temperature".

- **PWM (pulse-width modulation) flame-off hold:** thermo-nova sets it to return temperature + flame-off offset. When the return temperature is missing it makes no override at all (`heating_control.py:392-399` at pin 7beaaf5).
- **Cycle tracker:** thermo-nova uses flow minus return (`cycles/tracker.py:262-265`).
- **Firmware today:** without MsgID 28 it falls back to flow + offset after 5 minutes (`satNoReturnTemp()` / `satPwmFlameOffHold()` in `SATcontrol.ino`; entries 135, 149, 151, 263).

On OT-Direct in gateway mode, TASK-1707 (beta.132) copied the PIC model. OT-Direct is the board's own OpenTherm hardware, without a PIC microcontroller.

- The boiler gets exactly one frame per thermostat frame.
- The gateway sends its own read only in the slot of a thermostat request that the boiler answers with Unknown-DataID. See `otdAltSlotTaken()` (`OTDirect.ino:1850-1882`) and, on the PIC, `gateway.asm:2387-2414`.

A thermostat that asks only for supported IDs (OpenTherm data identifiers) therefore never lets the gateway read MsgID 28. tjfs's setup is such a case: an Ideal Halo Lite thermostat with an Ideal Logic boiler. The thermostat asks only IDs 0, 1, 16, 17, 24 and 25 (capture 2026-11-09, `ret=0.0`).

The PIC has the same limit, and `AA=` / `PM=` do not get around it (TASK-1767). That is why Robert chose A on OT-Direct plus C (`AA=28`) on the PIC.

ADR-223 made "like the PIC" the default for how gateway mode answers the thermostat. Option A departs from that on purpose, for one read per interval.

## Decision

In OT-Direct gateway mode the gateway reads MsgID 28 in the slot of a supported thermostat read, and answers that read from the boiler cache.

- **When:** once per interval, and only while MsgID 28 has no fresh, valid boiler answer.
- **To the boiler:** in that slot the gateway sends READ_DATA MsgID 28 instead of the thermostat's read.
- **To the thermostat:** READ_ACK with the cached boiler value for the ID it asked.

The boiler still receives exactly one frame per thermostat frame; only the content of that one slot changes. The thermostat still gets a real boiler value for its own ID, but not a freshly read one.

Which thermostat frames may be used:

- A READ_DATA only, never a WRITE_DATA. A write must reach the boiler; that is the lesson of TASK-1701.
- Never MsgID 0 (status and CH (central heating) enable).
- Never MsgID 9 or 100 (remote override, ADR-223).
- Never an ID with an active SR= (fixed response) or UI= (treat as unknown ID) entry, and never MsgID 28 itself.
- Only an ID whose cached answer is younger than the interval.
- An unsupported-ID slot (TASK-1707) keeps priority, because the schedule already reads MsgID 28 there.

**Cache age: one stamp of about 8 B of static RAM (random-access memory), the simplest variant (KISS: keep it simple), chosen by Robert on 2026-11-10 ("Direction OK, 8 B cache").**

- The gateway keeps one candidate ID: the first eligible READ_DATA ID that the thermostat repeats. In tjfs's log that is 25 or 17.
- With it, the gateway stores the time of the last boiler reply for that ID.
- Only frames of the candidate ID are ever replaced.
- When the candidate has not been read for longer than the interval, a new candidate is chosen.

This covers the need as long as the thermostat repeats at least one eligible read; every thermostat observed so far does. The per-slot variant (512 B) is the fallback if review wants any ID to be replaceable.

Sergeant D set the interval to 30 s, acting only while no valid MsgID 28 arrived for 30 s (W1), and limited it to while SAT is on (W2), corrections entry 291.

**Scope: OT-Direct gateway mode only.** Option C of Q-1759a and entry 290 (`AA=28` on PIC boards while SAT is on) is not part of this decision. It follows in its own ADR and task once Sergeant D answers Q-1759b. PIC boards keep today's behaviour.

## Decision Contract

### Must

- Keep exactly one boiler frame per thermostat frame in gateway mode with a thermostat.
- Replace at most one thermostat frame per interval. Replace only a READ_DATA of an eligible ID (see Decision) whose cached boiler answer is younger than the interval.
- Answer the replaced read with READ_ACK and the cached boiler value of the asked ID. Log both frames ('T' and 'A') as today.
- Do nothing while MsgID 28 is fresh, or while the boiler answers MsgID 28 with Unknown-DataID (`isBoilerMsgIdUnsupportedRead(28)`).
- Let an unsupported-ID slot (TASK-1707) take priority.

### Must Not

- Send extra frames between thermostat frames. That is option B, which TASK-1701 and TASK-1707 removed.
- Replace MsgID 0, 9 or 100, a WRITE_DATA, an SR= or UI= ID, or MsgID 28.
- Answer from a cache entry older than the interval.

### Exceptions

- None in gateway mode. Master mode (no thermostat) is not affected: there the gateway's own schedule already reads MsgID 28 every 10 s (`R_ENTRY(28, OT_TEMP_INTERVAL_MS)`, `OTDirect.ino:154`, `:59`).

### Verification

Host test on the real `OTDirect.ino` slices: `tests/test_otd_gateway_alt_slots.py` plus the tjfs replay (`tests/otd_gateway_replay_tjfs.py`). With tjfs's stream, where every asked ID is supported, it checks that:

- MsgID 28 reaches the boiler once per interval;
- the number of boiler frames equals the number of thermostat frames;
- only frames of the candidate `READ_DATA` ID are replaced, and the thermostat gets READ_ACK with the cached value;
- nothing is replaced while MsgID 28 is fresh or Unknown, while the cache is stale, or outside gateway mode.

The test must fail on the code before the change and pass after it.

Field proof: a capture from tjfs. There is no bench with a thermostat in gateway mode (TASK-1707 AC4).

## Alternatives Considered

- **B, own frames between the thermostat's frames.** Rejected. TASK-1701 and TASK-1707 removed exactly this after tjfs's field log, which showed 634 own frames against 369 thermostat frames in 6 minutes. The PIC never does it, and it risks delaying the thermostat's next frame.
- **Only the unsupported-ID slots (status quo, TASK-1707).** Rejected for the setups that matter: with a thermostat that asks only supported IDs, as tjfs's does, it reads nothing. It stays the preferred path whenever such a slot exists.
- **Do nothing and keep the flow + offset fallback.** Rejected by entry 287 (Sergeant D) and entry 290 (Robert). The fallback is a recorded deviation from thermo-nova, and the return temperature also feeds the cycle tracker.
- **A cache age per slot, `otBoilerCacheMs[128]`.** This costs 512 B of static RAM in internal DRAM (data RAM) as BSS (zero-initialised static data), also on the `OTGW32` board without PSRAM (external pseudo-static RAM). It would let any eligible ID be replaced, with an exact age per ID. It is not proposed first, because the single-stamp variant covers the observed thermostats with about 1/64 of the RAM and less code. It remains the fallback.
- **Answer the thermostat Unknown-DataID for the replaced read, as the TASK-1707 slot does.** Rejected. The ID is supported, and three Unknown-DataID answers could make a thermostat stop asking for a value it uses.

## Consequences

**Positive:**

- SAT gets a real return temperature on OT-Direct gateway installations whose thermostat asks only supported IDs. The flame-off hold becomes return + offset, as in thermo-nova, instead of the flow + offset fallback.
- `Tret` reaches MQTT (the message protocol the firmware publishes to) and Home Assistant on those installations.
- The load on the boiler bus does not change: one frame per thermostat frame.

**Negative:**

- This is a deliberate deviation from the PIC model and from ADR-223's "like the PIC". It must be recorded in the OT-Direct docs next to TASK-1707.
- One thermostat read per interval gets a value up to one interval old. With tjfs's numbers (1 frame/s, ID 25 every ~3.2 s, interval 30 s) that is about 3 % of the frames and about 1 in 10 reads of ID 25, with a value that is typically ~3 s old.
  - Risk: a thermostat that compares successive answers could notice.
  - Mitigation: only reads are replaced, never MsgID 0 or a write; the cache age is bounded by the interval; the effect is measured in the replay test.
- RAM: about 8 B for the proposed variant, or 512 B for the per-slot fallback. To be shown with the RAM line of the build output, before and after.

## Open Questions

- [x] W1: the interval (proposal 30 s), Sergeant D (Q-1759a). — **Answered 2026-11-10 by User: Sergeant D (relayed by otgw-firmware-97):** 30 s, and only while no valid MsgID 28 arrived for 30 s; Sergeant D 2026-11-10, 'the answer is A' (08:29:17 UTC, msg 1569614915222773762) and 'yes' (09:24:44 UTC, msg 1569628868648767570) to our explicit check; corrections entry 291
- [x] W2: only while SAT is enabled, or always (proposal: SAT only), Sergeant D (Q-1759a). — **Answered 2026-11-10 by User: Sergeant D (relayed by otgw-firmware-97):** only while SAT is on; Sergeant D 2026-11-10 'yes' (msg 1569628868648767570) to our check; entry 291
- [x] W3: Sergeant D's agreement with A+C (entry 287 is his). — **Answered 2026-11-10 by User: Sergeant D (relayed by otgw-firmware-97):** agreed for A on OT-Direct; Sergeant D 2026-11-10 'the answer is A' + 'yes' (entry 291). C on PIC not answered by him (Q-1759b queued); C stays Robert's entry 290
- [x] Cache-age variant: one stamp (8 B, candidate ID) or per slot (512 B), Robert / review. — **Answered 2026-11-10 by User: Robert van den Breemen (relayed by the `Coordinator OTGW` session):** single stamp, 8 B (candidate ID, KISS). Robert, 2026-11-10 06:40, 'Direction OK, 8 B cache', via AskUserQuestion in the coordinator's session (relayed to 030509)
- [x] Acceptance of the deviation from ADR-223's PIC parity for this one read, Robert. — **Answered 2026-11-10 by User: Robert van den Breemen (relayed by the `Coordinator OTGW` session):** accepted for this one read per interval. Robert, 2026-11-10 06:40, 'Direction OK', via AskUserQuestion in the coordinator's session (relayed to 030509); the ADR stays Proposed until Sergeant D answers W1-W3

## Related Decisions

- **ADR-223 (OT-Direct gateway mode answers a TT= or TC= override to the thermostat only, like the PIC):** sets PIC parity for the answers gateway mode gives the thermostat. This ADR is a scoped deviation from it, for one cache-answered read per interval. MsgID 9 and 100 stay excluded.

## References

- **Tasks:** TASK-1759 (implementation plan), TASK-1707 (alternative slots), TASK-1701, TASK-1767 (the PIC `PM=` / `AA=` slot limit), Q-1759a.
- **Corrections:** `docs/sat/sergeant-d-corrections.md` entries 287 and 290; entries 135, 149, 151 and 263 (the no-return fallback).
- **`src/OTGW-firmware/OTDirect.ino`:**
  - `:296-297` (`otBoilerCache`)
  - `:1517-1527` (`otBoilerCacheStore`)
  - `:1850-1882` (`otdAltSlotTaken`)
  - `:2183-2228` (the relay path)
  - `:154` and `:59` (`R_ENTRY(28, …)`, 10 s)
- **Other firmware:** `src/OTGW-firmware/SATcontrol.ino:2607` (return feed classification); `src/OTGW-firmware/OTGW-Core.ino:5608-5612` (MsgID 28 unsupported checks).
- **thermo-nova pin 7beaaf5:** `custom_components/sat/heating_control.py:392-399`, `cycles/tracker.py:262-265`, `coordinator/mqtt/opentherm.py:21` and `:113-118`.
- **PIC firmware otgw-6.6 `gateway.asm`:** `:2387-2414`, `:3390`, `:3436`, `:4617-4661`, `:4984-4995`.

## Enforcement

This decision has no machine-checkable surface until the code exists. An Enforcement block is added when the ADR is accepted and implemented, for example a `require_pattern` on the candidate-ID gate in `OTDirect.ino`.
