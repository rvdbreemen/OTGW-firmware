---
id: "ADR-208"
title: "OT-Direct gateway mode applies the CH= override to the relayed thermostat MsgID 0"
status: "Proposed"
date: "2026-11-04"
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
  - "ch-enable"
  - "master-status"
  - "sat"
aliases:
  - "MsgID 0 relay rewrite"
  - "CH enable on the relayed status frame"
  - "TASK-1238 option B"
  - "two masters on MsgID 0"
components:
  - "OT-Direct relayed MsgID 0 CH-enable rewrite"
symbols:
  - "applyOverrides"
  - "buildOverriddenReply"
  - "otdEffectiveMasterFlags"
  - "otdCsOverrideActive"
  - "buildStatusRequest"
  - "otChModeOff"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-208 OT-Direct gateway mode applies the CH= override to the relayed thermostat MsgID 0

## Status

Proposed, 2026-11-04.

Guideline-level per ADR-080 (Binding ADR rules must have a CI (continuous integration) gate): no gate exists yet, because the behaviour is not implemented. The implementation task adds the host test named under Verification; with that test in place the maintainer may reclassify this ADR as pattern-level at acceptance.

The option was chosen by the maintainer on 2026-10-30 (TASK-1238, option B). On 2026-11-04 the maintainer waived TASK-1238 AC1 (a bench capture with a live thermostat in OT-Direct gateway mode): neither bench board has a thermostat connected. That capture becomes a follow-up field capture from an OT-Direct user with a thermostat (see Consequences).

## Status History

```yaml
status_history:
  - date: 2026-11-04
    status: Proposed
    changed_by: Claude
    reason: Initial proposal from TASK-1238 (maintainer chose option B on 2026-10-30; AC1 waived 2026-11-04)
    changed_via: adr-kit
```

## Context

OpenTherm (OT) MsgID 0 is the status exchange. The master (thermostat) sends a READ_DATA frame whose high byte is the master status; bit 0 of that byte is CH (central heating) enable. A frame of type READ_DATA asks the boiler for a value, WRITE_DATA sends one. CS (control setpoint) is MsgID 1, the flow temperature the gateway asks for. The boiler answers with a READ_ACK whose low byte is the slave status (fault, CH mode, DHW (domestic hot water) mode, flame).

On OT-Direct boards (no PIC (Peripheral Interface Controller); the ESP32-S3 drives the OT bus itself) in gateway mode there are two masters on MsgID 0:

- **The gateway's own frame.** `scheduleMasterRequest()` (`src/OTGW-firmware/OTDirect.ino:1540`) polls MsgID 0 every 800 ms (`OT_STATUS_INTERVAL_MS`, `:55`) through `buildStatusRequest()` (`:778-785`). The scheduler runs in every mode except monitor (`:2144-2145`). Bit 0 of this frame comes from `otdEffectiveMasterFlags(otMasterStatusFlags, otChModeOff, otdCsOverrideActive())` (`:761-775`, TASK-1346): while a CS= override of MsgID 1 is active, the CH= state (`otChModeOff`, set by the CH= handler at `:3025-3031`) sets bit 0; without a CS override the board's own bit stands. This is the PIC rule "CH=1 Override the CH enable bit when CS != 0" (`other-projects/otgw-6.6/gateway.asm:53`).
- **The thermostat's relayed frame.** A thermostat frame goes to the boiler through `applyOverrides()` (`:571-603`, called at `:2130`), which returns early for anything but WRITE_DATA (`:585`). The thermostat's READ_DATA MsgID 0 therefore reaches the boiler unchanged, with the thermostat's own CH bit.

So while SAT (Smart Autotune Thermostat) holds a CS override and sends CH=0 (cold cutoff, PWM (pulse-width modulation) OFF phase, all valves closed, summer simmer: ADR-199, sent through `satSendCH()` at `src/OTGW-firmware/SATcontrol.ino:706-714`), the boiler sees bit 0 = 0 every 800 ms from the gateway and bit 0 = 1 from the thermostat whenever the thermostat asks for heat. Which bit the boiler acts on is not known: it depends on the boiler, and TASK-1238 AC1 (the capture that would show it) could not be run. SAT's CH=0 is therefore not reliably honoured on OT-Direct. ADR-179 recorded this as its open MsgID 0 question and moved it to TASK-1238.

The PIC does not have this problem. Its `MessageID0` handler (`other-projects/otgw-6.6/gateway.asm:2700-2743`) rewrites master status bits of the thermostat's own MsgID 0 before it goes to the boiler. Its header comment (`:2700-2708`) says CH mode must be enabled for an overridden control setpoint to have effect, and that the original master status is restored before the reply goes back to the thermostat. `statusreturn` (`:2745-2750`) copies the boiler's slave status byte into that reply. A field capture on a PIC board shows the result: "every later MsgID 0 request to the boiler with CH enable = 0 (R80000200), the thermostat's own CH=1 rewritten to CH=0" (`docs/sat/sergeant-d-corrections.md` entry 60, gruijter's capture of 2026-10-26; his PIC board is recorded at line 4144 of that file).

The SAT ground truth (ADR-196) has nothing on this. thermo-nova drives an OTGW (OpenTherm Gateway) PIC over MQTT (Message Queuing Telemetry Transport) or serial and has no OT-Direct relay, and no Sergeant D entry decides how the bit is carried on OT-Direct. This is a transport decision on the OT-Direct side, not a change to when SAT wants CH=0. Checked entries: entries 5 and 17 (PWM OFF: CS=10 with CH=0) and ADR-199's cutoff rule decide when SAT sends CH=0, and this ADR only makes that bit arrive (agrees). Entry 26, as quoted in entry 39 ("When SAT is off or faulted, the thermostat controls the boiler again"), holds, because without a CS override the thermostat's bit passes through (Decision, point 3; agrees). Entry 60 is the PIC capture above (agrees). No conflict with a recorded entry was found.

The maintainer weighed two options on 2026-10-30 (TASK-1238): (A) suppress the gateway's own MsgID 0 while a live thermostat is present, or (B) apply SAT's CH bit to the relayed READ_DATA frame. He chose B.

## Decision

In OT-Direct gateway mode, bit 0 of the master status in a relayed thermostat READ_DATA MsgID 0 frame is set by the same rule as the gateway's own MsgID 0 frame: `otdEffectiveMasterFlags(<thermostat high byte>, otChModeOff, otdCsOverrideActive())`.

1. **What the boiler sees.** While a CS override of MsgID 1 is active, the relayed frame carries bit 0 = 0 after CH=0 and bit 0 = 1 after CH=1, whatever the thermostat sent. All other bits of the thermostat's high byte, and the low byte, pass unchanged. The parity bit is recomputed (`setOTParityBit()`). Both MsgID 0 frames the boiler receives then carry the same CH enable bit.
2. **What the thermostat gets back.** The reply carries the boiler's low byte (slave status: fault, CH mode, DHW mode, flame) and the thermostat's original high byte, as the PIC's `statusreturn` does. The current overridden-reply path, `buildOverriddenReply()` (`OTDirect.ino:1289-1301`), echoes the thermostat's own request data for any MsgID except 56 and 57. For MsgID 0 that would drop the boiler's slave status, so MsgID 0 needs its own case there.
3. **While SAT is off.** With no CS override active (SAT disabled: `satSendRelease()` sends CS=0 and then CH=1, `SATcontrol.ino:794-796`; or a CS override cleared by its TASK-442 heartbeat expiry), `otdEffectiveMasterFlags()` returns the thermostat's byte unchanged, so the thermostat's own CH bit reaches the boiler as today.
4. **Logging.** A rewritten frame is logged as the T/R pair of a gateway substitution (`bridgeSentRequest()`, `OTDirect.ino:1338-1341`), and a reply that differs from the boiler's frame as A (`replyToThermostat()`, `:1309-1332`), so the parser keeps the boiler's B frame as the canonical value (ADR-096, ADR-103). A frame whose bit 0 already matches is not marked modified and is relayed as today.
5. **The bus-offline probe is unchanged.** The gateway keeps its own MsgID 0 schedule. Offline is marked only when a MsgID 0 request gets no reply (`OTDirect.ino:1514-1523`), whatever its origin; this ADR changes neither.

Scope: OT-Direct gateway mode only. Monitor mode stays a transparent pass-through (no rewrite, no own frames); bypass, master and loopback modes are not affected. PIC boards are not affected: the PIC already does this itself. Bit 0 is the only bit rewritten.

This is a deliberate exception to the contract in the comment above `applyOverrides()` (`OTDirect.ino:565-570`, "A READ-DATA passes unchanged") and to its early return at `:585`. The comment is updated with the implementation.

## Decision Contract

### Must

- In gateway mode, a relayed thermostat READ_DATA MsgID 0 frame gets bit 0 of its high byte from `otdEffectiveMasterFlags()`, with the same `otChModeOff` and `otdCsOverrideActive()` inputs that `buildStatusRequest()` uses, and its parity recomputed.
- The reply to the thermostat for a rewritten MsgID 0 carries the boiler's low byte and the thermostat's original high byte.
- The gateway's own MsgID 0 schedule and the MsgID-0-based offline detection stay in place.

### Must Not

- Must not rewrite any MsgID 0 bit other than bit 0 (CH enable).
- Must not rewrite the frame without an active CS override, or in monitor mode.
- Must not suppress the gateway's own MsgID 0 while a thermostat is connected (option A).

### Exceptions

- None.

### Verification

- To be added by the implementation task: a host test that compiles the real rewrite and reply code cut from `src/OTGW-firmware/OTDirect.ino` (the cut-between-markers pattern of `tests/test_otd_ch_override.py`, which already covers `otdEffectiveMasterFlags()` and `buildStatusRequest()` for TASK-1346). It checks: CH=0 with an active CS override clears bit 0 of a relayed MsgID 0 with bit 0 = 1; no CS override leaves the frame unchanged; the reply keeps the boiler's low byte and the thermostat's high byte; the parity is valid. It must fail on the current `applyOverrides()` (old-vs-fix).
- Field validation, open: a capture from an OT-Direct user with a live thermostat in gateway mode, showing which MsgID 0 frames reach the boiler and that the boiler follows SAT's CH=0 (TASK-1238 AC1, waived for this decision).

## Alternatives Considered

- **Option A: suppress the gateway's own MsgID 0 while a live thermostat is present** (`state.otd.bThermostatConnected`, `OTDirect.ino:2601-2625`). Rejected: SAT's CH=0 then has no carrier at all, because the only MsgID 0 left on the bus is the thermostat's, with the thermostat's bit. The cold cutoff would rely on CS=10 alone, which leaves CH enabled on the boiler and so does not match ADR-199's CS=10 with CH=0. The gateway's offline probe would also depend on the thermostat's polling rate, and the 5 s offline retry (`OT_OFFLINE_RETRY_MS`, `:56`) would no longer be the gateway's own.
- **Do nothing (keep two masters with different CH bits).** Rejected: the boiler's behaviour then depends on which interleaved frame it acts on, which is boiler-specific and unmeasured. SAT's cold cutoff and PWM OFF phase are not reliable on OT-Direct with a thermostat attached.
- **Rewrite the relayed frame only while SAT is the active controller (`satOwnsControlSetpoint()`), not on any CS override.** Rejected: the gateway's own frame already uses the CS-override predicate (TASK-1346). A second, narrower predicate would let the two frames disagree again after a manual `CS=` with `CH=0`, which is the defect this ADR removes. While SAT is active it always holds a CS override (it resends CS every 5 s, ADR-203 and ADR-207), so for SAT both predicates give the same result.

## Consequences

**Positive:**

- SAT's CH=0 reaches the boiler on every MsgID 0 frame in OT-Direct gateway mode, as it does on PIC boards (`gateway.asm:2700-2743`). OT-Direct and PIC boards then behave the same way for the CH enable bit.
- The thermostat still sees the boiler's real slave status (flame, fault, CH mode), so its display and fault handling keep working.
- No change while SAT is off: the thermostat controls CH as today.
- The offline detection is untouched.

**Negative:**

- New relay semantics: the boiler no longer sees the thermostat's own CH request while a CS override is active. That is the intent, but a thermostat that reacts to the boiler's CH mode bit staying 0 while it asks for heat may behave differently. Mitigation: the field capture below.
- The rewrite also forces CH on: after CH=1 under a CS override, the boiler gets CH enable even when the thermostat sends 0 (thermostat switched off, or in its own summer mode). This is what the PIC and the gateway's own frame already do, and while SAT is active it is SAT's decision, but it is the half a user will notice. SAT's hand-back (CS=0, then CH=1) clears the CS override, so after SAT is disabled the thermostat's 0 reaches the boiler again.
- The rule also applies to a manual `CS=` plus `CH=0` from the REST (Representational State Transfer) API (application programming interface), MQTT, Home Assistant or port 25238, not only to SAT. This matches the PIC and the gateway's own frame, but it widens the effect beyond SAT. The maintainer confirms this scope at acceptance (Open Questions).
- Only bit 0 is aligned. The other master bits of the two MsgID 0 frames can still differ: the board's `otMasterStatusFlags` (DHW enable, cooling, CH2 via H2=, summer via SM=) against the thermostat's. Out of scope for this ADR; a known limitation.
- The decision rests on code reading and the PIC precedent, not on a measurement on OT-Direct. TASK-1238 AC1 was waived by the maintainer on 2026-11-04.

**Open field validation:** a follow-up task asks an OT-Direct user with a thermostat in gateway mode for a capture (`capture-mqtt-debug.bat` or `capture-otgw.sh` transcript) after the implementation ships. It must show the T/R pairs of rewritten MsgID 0 frames, the gateway's own R frames and the boiler's slave status, through at least one SAT cold cutoff or PWM OFF phase.

**Revisit this decision when** the field capture shows that:

- the boiler ignores the rewritten bit, or flaps between CH on and off;
- a thermostat misbehaves on the changed exchange (errors, a lost connection, or its own fallback);
- the remaining differences in the other master bits change the boiler's behaviour, so more than bit 0 has to be aligned or option A has to be weighed again.

## Open Questions

- [ ] Scope of the trigger: is the CS-override predicate (manual `CS=` with `CH=` included, PIC parity) accepted, or does the maintainer want the rewrite limited to SAT? Recommendation: the CS-override predicate, for the reason under Alternatives Considered.

## Related Decisions

- **ADR-179 (Name the owner of the OpenTherm control setpoint while SAT is enabled)**: depends on it and answers its MsgID 0 open question (TASK-1238). MsgID 1 ownership is unchanged; ADR-179 is not edited.
- **ADR-199 (SAT cold state and CH-enable rule)**: complements it. ADR-199 decides when SAT wants CH=0; this ADR decides how that bit reaches the boiler on OT-Direct. ADR-199's risk "CH=0 may not reach the boiler in OT-Direct gateway mode" is what this ADR addresses.
- **ADR-203 and ADR-207 (SAT command cadence)**: SAT's CS resend every 5 s keeps the CS override active, which is the condition for the rewrite. Unchanged.
- **ADR-096 and ADR-103 (MQTT source-topic semantics, proxy-answer routing)**: the T/R/A logging of a rewritten frame follows them.
- **ADR-196 (SAT behaviour ground truth)**: the source order checked in Context; no recorded entry covers OT-Direct transport.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level until the host test exists.

## References

- Tasks: TASK-1238 (decision, option B chosen 2026-10-30, AC1 waived 2026-11-04); TASK-1346 (CH= under a CS override); TASK-1237 (moved the question out of ADR-179).
- `src/OTGW-firmware/OTDirect.ino`: `:55-56` (status and offline intervals), `:61-65` (`otMasterStatusFlags`, `otChModeOff`), `:565-603` (`applyOverrides()`, READ_DATA early return at `:585`), `:761-785` (`otdCsOverrideActive()`, `otdEffectiveMasterFlags()`, `buildStatusRequest()`), `:1289-1301` (`buildOverriddenReply()`), `:1309-1332` (`replyToThermostat()`), `:1338-1341` (`bridgeSentRequest()`), `:1514-1523` (offline on MsgID 0 failure), `:1540` (`scheduleMasterRequest()`), `:2130` (thermostat frame forwarding through `applyOverrides()`), `:2144-2145` (scheduler excludes monitor mode), `:2601-2625` (thermostat connected detection), `:3025-3031` (CH= handler).
- `src/OTGW-firmware/SATcontrol.ino`: `:706-714` (`satSendCH()`), `:794-796` (`satSendRelease()`: CS=0, CH=1, MM=T).
- PIC firmware: `other-projects/otgw-6.6/gateway.asm:53` (CH= only under CS), `:461-465` (`statusflags`, `CHModeOff`), `:2700-2743` (MsgID 0 master status rewrite), `:2745-2750` (`statusreturn`).
- `docs/sat/sergeant-d-corrections.md` entries 5, 17, 39 (quoting entry 26) and 60.
- thermo-nova, pinned at commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`: https://github.com/Alexwijn/SAT/tree/releases/thermo-nova (local copy `other-projects/SAT-releases-thermo-nova/`); it drives a PIC OTGW and has no OT-Direct relay.
- OpenTherm Protocol Specification v4.2 (`docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`), MsgID 0 master and slave status flags.
- Test pattern: `tests/test_otd_ch_override.py`.

## Enforcement

The rule needs judgement (which frames are rewritten, which bit, what the reply carries), so the LLM (large language model) judge checks it on changes to `OTDirect.ino`. The one declarative rule keeps the shared helper that both MsgID 0 frames must use, and scopes the judge to that file.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "\\botdEffectiveMasterFlags\\(",
      "path_glob": "src/OTGW-firmware/OTDirect.ino",
      "message": "ADR-208: the gateway's own and the relayed MsgID 0 frames take bit 0 from otdEffectiveMasterFlags()."
    }
  ],
  "llm_judge": true
}
```
