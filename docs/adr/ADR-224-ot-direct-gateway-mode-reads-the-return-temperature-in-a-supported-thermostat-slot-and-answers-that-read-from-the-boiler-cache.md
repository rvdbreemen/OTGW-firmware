---
id: "ADR-224"
title: "OT-Direct gateway mode reads the return temperature in a supported thermostat slot and answers that read from the boiler cache"
status: "Proposed"
date: "2026-11-09"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
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
---

<!-- markdownlint-disable MD025 -->

# ADR-224 OT-Direct gateway mode reads the return temperature in a supported thermostat slot and answers that read from the boiler cache

## Status

Proposed, 2026-11-09.

**Decision Maker:** to be decided by User: Robert van den Breemen (maintainer). The choice of option A+C itself was his, made on 2026-11-09 at 21:52:17 UTC in #dev-sat-mqtt: "I think A+C would be best, for each hardware setup the optimal solution." (msg 1569469705461440577; corrections entry 290).

This ADR records option A's deviation from the PIC model and its parameters. It was written by agent otgw-firmware-030509 and is not accepted until Robert says so. The interval and scope (W1, W2) wait for Sergeant D (Q-1759a).

## Status History

```yaml
status_history:
  - date: 2026-11-09
    status: Proposed
    changed_by: "Agent: otgw-firmware-030509"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

SAT (Smart Autotune Thermostat, the firmware port of thermo-nova) needs the boiler return temperature, OpenTherm MsgID 28. Sergeant D said so in entry 287: "We need to ask for return temperature".

- **PWM flame-off hold:** thermo-nova sets it to return temperature + flame-off offset. When the return temperature is missing it makes no override at all (`heating_control.py:392-399` at pin 7beaaf5).
- **Cycle tracker:** thermo-nova uses flow minus return (`cycles/tracker.py:262-265`).
- **Firmware today:** without MsgID 28 it falls back to flow + offset after 5 minutes (`satNoReturnTemp()` / `satPwmFlameOffHold()` in `SATcontrol.ino`; entries 135, 149, 151, 263).

On OT-Direct in gateway mode, TASK-1707 (beta.132) copied the PIC model. OT-Direct is the board's own OpenTherm hardware, without a PIC microcontroller.

- The boiler gets exactly one frame per thermostat frame.
- The gateway sends its own read only in the slot of a thermostat request that the boiler answers with Unknown-DataID. See `otdAltSlotTaken()` (`OTDirect.ino:1850-1882`) and, on the PIC, `gateway.asm:2387-2414`.

A thermostat that asks only for supported IDs therefore never lets the gateway read MsgID 28. tjfs's setup is such a case: an Ideal Halo Lite thermostat with an Ideal Logic boiler. The thermostat asks only IDs 0, 1, 16, 17, 24 and 25 (capture 2026-11-09, `ret=0.0`).

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
- Never MsgID 0 (status and CH enable).
- Never MsgID 9 or 100 (remote override, ADR-223).
- Never an ID with an active SR= or UI= entry, and never MsgID 28 itself.
- Only an ID whose cached answer is younger than the interval.
- An unsupported-ID slot (TASK-1707) keeps priority, because the schedule already reads MsgID 28 there.

**Cache age, proposed variant (KISS): one stamp of about 8 B static RAM.**

- The gateway keeps one candidate ID: the first eligible READ_DATA ID that the thermostat repeats. In tjfs's log that is 25 or 17.
- With it, the gateway stores the time of the last boiler reply for that ID.
- Only frames of the candidate ID are ever replaced.
- When the candidate has not been read for longer than the interval, a new candidate is chosen.

This covers the need as long as the thermostat repeats at least one eligible read; every thermostat observed so far does. The per-slot variant (512 B) is the fallback if review wants any ID to be replaceable.

Sergeant D decides the interval (W1, proposal 30 s) and whether this runs only with SAT on or always (W2, proposal: SAT only), under Q-1759a. The decision does not depend on their values.

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
- only frames of the candidate READ ID are replaced, and the thermostat gets READ_ACK with the cached value;
- nothing is replaced while MsgID 28 is fresh or Unknown, while the cache is stale, or outside gateway mode.

The test must fail on the code before the change and pass after it.

Field proof: a capture from tjfs. There is no bench with a thermostat in gateway mode (TASK-1707 AC4).

## Alternatives Considered

- **B, own frames between the thermostat's frames.** Rejected. TASK-1701 and TASK-1707 removed exactly this after tjfs's field log, which showed 634 own frames against 369 thermostat frames in 6 minutes. The PIC never does it, and it risks delaying the thermostat's next frame.
- **Only the unsupported-ID slots (status quo, TASK-1707).** Rejected for the setups that matter: with a thermostat that asks only supported IDs, as tjfs's does, it reads nothing. It stays the preferred path whenever such a slot exists.
- **Do nothing and keep the flow + offset fallback.** Rejected by entry 287 (Sergeant D) and entry 290 (Robert). The fallback is a recorded deviation from thermo-nova, and the return temperature also feeds the cycle tracker.
- **A cache age per slot, `otBoilerCacheMs[128]`.** This costs 512 B of static RAM in internal DRAM (BSS), also on the OTGW32 without PSRAM. It would let any eligible ID be replaced, with an exact age per ID. It is not proposed first, because the single-stamp variant covers the observed thermostats with about 1/64 of the RAM and less code. It remains the fallback.
- **Answer the thermostat Unknown-DataID for the replaced read, as the TASK-1707 slot does.** Rejected. The ID is supported, and three Unknown-DataID answers could make a thermostat stop asking for a value it uses.

## Consequences

**Positive:**

- SAT gets a real return temperature on OT-Direct gateway installations whose thermostat asks only supported IDs. The flame-off hold becomes return + offset, as in thermo-nova, instead of the flow + offset fallback.
- `Tret` reaches MQTT and Home Assistant on those installations.
- The load on the boiler bus does not change: one frame per thermostat frame.

**Negative:**

- This is a deliberate deviation from the PIC model and from ADR-223's "like the PIC". It must be recorded in the OT-Direct docs next to TASK-1707.
- One thermostat read per interval gets a value up to one interval old. With tjfs's numbers (1 frame/s, ID 25 every ~3.2 s, interval 30 s) that is about 3 % of the frames and about 1 in 10 reads of ID 25, with a value that is typically ~3 s old.
  - Risk: a thermostat that compares successive answers could notice.
  - Mitigation: only reads are replaced, never MsgID 0 or a write; the cache age is bounded by the interval; the effect is measured in the replay test.
- RAM: about 8 B for the proposed variant, or 512 B for the per-slot fallback. To be shown with the RAM line of the build output, before and after.

## Open Questions

- [ ] W1: the interval (proposal 30 s), Sergeant D (Q-1759a).
- [ ] W2: only while SAT is enabled, or always (proposal: SAT only), Sergeant D (Q-1759a).
- [ ] W3: Sergeant D's agreement with A+C (entry 287 is his).
- [ ] Cache-age variant: one stamp (8 B, candidate ID) or per slot (512 B), Robert / review.
- [ ] Acceptance of the deviation from ADR-223's PIC parity for this one read, Robert.

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
