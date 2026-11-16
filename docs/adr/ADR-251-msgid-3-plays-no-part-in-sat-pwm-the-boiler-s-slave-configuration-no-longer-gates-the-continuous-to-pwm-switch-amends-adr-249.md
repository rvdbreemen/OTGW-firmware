---
id: "ADR-251"
title: "MsgID 3 plays no part in SAT PWM; the boiler's slave configuration no longer gates the continuous-to-PWM switch (amends ADR-249)"
status: "Accepted"
date: "2026-11-16"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-249"
topics:
  - "sat"
  - "pwm"
  - "msgid 3"
  - "slave configuration"
  - "master low-off"
  - "pwm permission"
aliases:
  - "hb bit 4"
  - "low-off veto"
  - "pwm veto"
  - "entry 392"
  - "entry 393"
components:
  - "satPwmPermitted"
symbols:
  - "satPwmPermitted"
  - "satPwmEntryAllowed"
  - "satCycleCheckLowLoadVeto"
  - "satNoteSlaveConfig"
  - "bSlaveCfgValid"
  - "bSlaveLowOffDenied"
  - "iPwmVetoReason"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-251 MsgID 3 plays no part in SAT PWM; the boiler's slave configuration no longer gates the continuous-to-PWM switch (amends ADR-249)

## Status

Accepted, 2026-11-16.

Amends ADR-249 (its Decision clause "the boiler's MsgID 3 permission comes first (entry 213)"). Guideline-level per ADR-080: the behaviour is checked by host tests, not by an evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-16
    status: Proposed
    changed_by: "Agent (95)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-16
    status: Proposed
    changed_by: "Agent (95)"
    reason: Related to ADR-249
    changed_via: adr-kit lifecycle
  - date: 2026-11-16
    status: Accepted
    changed_by: "Robert van den Breemen (decision, DM 1571893214372438150), recorded by Agent (95)"
    reason: "Accepted: decision Robert van den Breemen (DM 1571893214372438150, answer A), recorded by Agent (95)"
    changed_via: adr-kit lifecycle
```

## Context

Since TASK-1412.4 (Sergeant D entries 213, 214 and 215, 2026-11-02) every SAT (Smart Autotune Thermostat) entry into PWM (pulse-width modulation: the boiler burns in timed on/off cycles) needed the boiler's permission: a valid
OpenTherm MsgID 3 (slave configuration) reply with HB bit 4 ("master low-off & pump control function", OT spec v4.2: 1 = not
allowed) clear, and bit 4 turning 1 ended a running PWM. ADR-249 kept that rule in its Decision section: "the boiler's MsgID 3
permission comes first (entry 213)". thermo-nova (pinned 7beaaf5) only shows the bit (coordinator/serial/binary_sensor.py:54) and
never acts on it.

In the field the rule kept a Viessmann Vitodens 100-W (indigo_light, beta.339) in continuous mode forever: it reports bit 4 = 1, so
`pwm_veto_reason` 2 / `pwm_veto_path` 1 / `slave_low_off_denied` true. Sergeant D, 2026-11-16 (docs/sat/sergeant-d-corrections.md):

- Entry 392: "Don't look at bit 4 at all. You should not consider MsgID3:HB4. boiler should be able to enter PWM" and "Probably was
  a wrong decision from my side". Reverses entries 213 and 214.
- Entry 393 (Q-392a A): MsgID 3 plays no part in PWM; PWM is allowed from the start, no wait for a valid MsgID 3 reply. Reverses
  entry 215.

## Decision

MsgID 3 plays no part in SAT PWM: neither HB bit 4 nor the presence of a valid MsgID 3 reply allows, refuses or ends PWM, on any
path (the continuous-to-PWM switch of ADR-249 and its fail-safe, the end-of-cycle switch, the restore after a restart, the
`control_mode` command, any other `satSetControlMode(SAT_MODE_PWM)` caller). The reply is still decoded, recorded and shown
(`slave_cfg_valid`, `slave_low_off_denied` in the SAT status). This replaces only ADR-249's clause "the boiler's MsgID 3 permission
comes first (entry 213)"; the rest of ADR-249 stands.

## Decision Contract

### Must

- `satPwmPermitted()` returns true whatever MsgID 3 said or whether it was seen.
- The MsgID 3 reply keeps being recorded through `satNoteSlaveConfig()` and shown in the SAT status.

### Must Not

- Must not refuse a PWM entry, or end a running PWM, because of MsgID 3 HB bit 4 or a missing MsgID 3 reply.
- Must not use MsgID 3 as a heating-source detector for PWM (ADR-157 unchanged).

### Exceptions

- None.

### Verification

- `tests/test_sat_slave_lowload.py` (real `satNoteSlaveConfig`, `satPwmPermitted`, `satPwmEntryAllowed`,
  `satCycleCheckLowLoadVeto`, `satSetControlMode`, `satRestoreControlMode`, `satHandleControlMode`): every MsgID 3 state (no reply,
  bit 4 = 0, bit 4 = 1) reaches PWM on every path and a running PWM is not ended. 20/20 on the fix, 16 fail on the code before it
  (`OTGW_SATCYCLES_INO` / `OTGW_SAT_INO`).
- `tests/test_sat_loop_integration.py::test_s1_pwm_reached_when_boiler_reports_low_off_not_allowed`: the whole SAT loop with the
  boiler model reporting bit 4 = 1 reaches PWM with its on and off phases on the wire; the code before it never reaches PWM.

## Alternatives Considered

- **Keep bit 4 as a veto (entries 213/214).** Rejected by Sergeant D (entry 392): it keeps boilers such as the Vitodens 100-W out of
  PWM although they run PWM fine.
- **Ignore bit 4 but keep waiting for a valid MsgID 3 reply (entry 215, Q-392a option B).** Rejected by Sergeant D (entry 393): the
  reply was only needed for bit 4; a gateway that never gets one would never use PWM.

## Consequences

**Positive:**

- Boilers that report bit 4 = 1 can use PWM, as on thermo-nova; a restart restores PWM at once instead of after the first MsgID 3 reply.

**Negative:**

- A boiler that really cannot handle the master switching it off between phases is no longer protected by the bit. Mitigation:
  PWM can be switched off by the user (`control_mode continuous`), and the field validation after the release watches for it.
- The refusal code (`satPwmEntryAllowed`, `satPwmNoteRefused`, `satCycleCheckLowLoadVeto`, `pwm_veto_reason`) stays as call points
  and never refuses; removing it is a later clean-up.

## Open Questions

- None.

## Related Decisions

- **ADR-249 (continuous-to-PWM switch with its own margin)**: amended; its clause "the boiler's MsgID 3 permission comes first
  (entry 213)" no longer holds.
- **ADR-157 (manual heating source stays authoritative)**: unchanged; MsgID 3 was never a source detector.
- **ADR-196 (SAT ground truth)**: Sergeant D's entries 392 and 393 outrank the earlier entries 213-215.

## References

- docs/sat/sergeant-d-corrections.md entries 213, 214, 215, 392, 393.
- `src/OTGW-firmware/SATcycles.ino` `satPwmPermitted()` (TASK-2000).
- thermo-nova 7beaaf5 coordinator/serial/binary_sensor.py:54.
- TASK-2000, TASK-1412.4, TASK-1998 (field check on the Vitodens 100-W).
