# ADR-162: Sanctioned SAT Force-Boiler Test Hook in Production Firmware

## Status

Proposed. Date: 2026-06-29.

**Decision Maker:** User: Robert (maintainer), who chose option A (a firmware test hook) over the alternatives after the options analysis in `docs/plan/SAT_SIM_F7_BOILER_EMULATOR_OPTIONS.md`.

## Status History

status_history:
  - date: 2026-06-29
    status: Proposed
    changed_by: Agent
    reason: Sanction a narrow, constrained test-hook pattern so the SAT section 4.2 boiler-availability gate (TASK-795) is bench-verifiable without a physical boiler (TASK-802).
    changed_via: adr-kit

## Context

The SAT (Smart Autotune Thermostat) section 4.2 boiler-availability gate (TASK-795) enforces a safety invariant: simulation must auto-disable the moment a real boiler appears on the OpenTherm bus, and enabling simulation while a boiler is present must be refused (REST 409, MQTT enable-reject). The gate is real safety logic, but it had no bench-reachable trigger. TASK-795 acceptance criteria AC#8 (edge auto-disable), AC#9 (REST 409) and AC#10 (MQTT reject) could not be exercised without owning and connecting a physical boiler.

The obvious idea, a host-side boiler emulator (TASK-802 F7), does not work on the OTGW32 (OT-Direct) hardware. The TCP bridge (port 25238) relays host bytes toward the PIC and command path; it is **not** a boiler-response injection point. The firmware's boiler-presence belief is set only from genuine OT-bus responses: `state.otBus.bBoilerState` (PIC path) and `otDirectBoilerPresent()` (OT-Direct path), read together by `satBoilerHardwarePresent()` (`SATcontrol.ino:1160`). A host emulator cannot forge those, so it cannot make the firmware believe a boiler answered.

Three options were weighed in `docs/plan/SAT_SIM_F7_BOILER_EMULATOR_OPTIONS.md`: (A) a firmware test hook, (B) a serial-level PIC emulator requiring real hardware, (C) park the gate as hardware-validation-only. The maintainer chose A. This ADR records that choice and, more importantly, the constraints that make shipping a test hook in production firmware acceptable, so the precedent is bounded rather than open-ended.

## Decision

Add a deliberate, transient, auth-gated **test hook**: a REST (Representational State Transfer) endpoint `POST /api/v2/sat/force-boiler` with body `0` or `1`. It calls `satSetDebugForceBoilerPresent(bool)` (`SATcontrol.ino:1176`), which sets a file-static RAM flag that `satBoilerHardwarePresent()` ORs in ahead of the real bus checks (`SATcontrol.ino:1162`). Asserting the flag also calls `satNotifyBoilerFrameSeen()`, which raises the deferred section 4.2 edge so the "boiler appeared, simulation off" teardown (`satOnBoilerDetected()`, `SATcontrol.ino:1187`) runs within about 1000 ms (one SAT control-loop tick), exactly as a real boiler frame would. The host script `scripts/sat_boiler_emulator.py --rest-force-boiler on|off` drives it.

This ADR **sanctions the narrow pattern** of a test hook in production firmware, bound by four load-bearing constraints. A test hook is acceptable only if it is all of:

1. **Transient.** RAM-only, reboot-cleared. It changes no persisted state, so it leaves no surprising residue after a power cycle. `satDebugForceBoilerPresent` is a `static bool` initialized `false`.
2. **Auth-gated.** Reached through the central mutating-method gate plus `handleSAT()`'s `checkHttpAuth()`, identical to every other mutating admin route (ADR-056). It is not a hidden bypass of HTTP (HyperText Transfer Protocol) auth.
3. **Idempotent / flash-safe.** `satOnBoilerDetected()` early-returns when simulation is already off (`SATcontrol.ino:1189`), so repeated asserts cause no extra `writeSettings()` and no flash wear.
4. **No new capability.** An authenticated actor can already toggle `settings.sat.bSimulation` via the settings POST; this hook grants nothing they did not already have. It is a test affordance, not a privilege escalation or a backdoor.

A test hook that cannot meet all four must instead be compile-gated out of release builds.

On-device validated on OTGW32, firmware alpha.285: enable simulation, `POST force-boiler/1`, simulation auto-disabled in about 2000 ms, a re-enable attempt returned 409, then `POST force-boiler/0` cleaned up.

## Alternatives Considered

### Alternative A: Firmware test hook (chosen)

Add `satSetDebugForceBoilerPresent()` plus the REST endpoint so the section 4.2 gate becomes self-testable from the bench. Small surface (one flag, one OR, one endpoint), makes a real safety path verifiable, and the four constraints bound the risk. Cost: a small test affordance lives in shipped firmware. Chosen.

### Alternative B: Serial-level PIC emulator

Emulate a boiler at the PIC serial layer so the firmware sees genuine-looking OT responses. **Rejected for routine use.** It needs real hardware (a serial loopback rig or a second device), which defeats the entire "no extra hardware on the bench" goal that motivated TASK-802. It remains the most faithful approach for a one-off hardware-in-the-loop validation, but not for the everyday regression loop.

### Alternative C: Park the gate as hardware-validation-only

Leave TASK-795 AC#8/#9/#10 as "verifiable only with a physical boiler" and rely on manual field validation. **Rejected.** It leaves three acceptance criteria on a safety-relevant gate permanently unverifiable in CI or on the bench, so regressions in the auto-disable / reject logic would surface only in the field, on a user's heating system. That is precisely the failure mode the gate exists to prevent.

### Alternative D: Compile-time `#if DEBUG` hook excluded from release builds

Keep the hook but strip it from production binaries with a build-flag guard. **Rejected for now.** The firmware has no established debug/release build split (the three fixed targets in ADR-126 are hardware variants, not debug/release variants), so introducing one solely for this hook is disproportionate, and the four constraints already bound the risk to "an authenticated LAN user can briefly fake boiler presence in RAM". Revisit if a future release-hardening pass introduces a debug/release split and wants test hooks compiled out: this is the documented upgrade path, not a closed door.

## Consequences

**Benefits**

- The section 4.2 availability gate is bench- and regression-testable without a physical boiler; TASK-795 AC#8/#9/#10 move from "field-only" to "scriptable" via `scripts/sat_boiler_emulator.py --rest-force-boiler`.
- The teardown path exercised is the real one: the hook trips `satNotifyBoilerFrameSeen()`, the same deferred edge a genuine frame uses, not a parallel test-only code path that could drift from production behaviour.
- The pattern is now explicit and bounded. Future contributors have a written rule for when a test hook may ship and when it must be compiled out.

**Trade-offs**

- A small test surface exists in production firmware. Bounded by the four constraints: transient, auth-gated, idempotent, no new capability. The worst case an authenticated LAN actor achieves is briefly forcing `satBoilerHardwarePresent()` true in RAM until reboot, which only disables simulation (a fail-safe direction) and grants nothing beyond the existing settings POST.

**Risks and mitigations**

- *Risk*: the precedent is read as blanket permission to ship arbitrary test or debug hooks. *Mitigation*: this ADR sanctions only hooks meeting all four constraints; anything failing them must be compile-gated. Code review checks new hooks against the list.
- *Risk*: the flag is left asserted and masks a real "no boiler" state during later testing. *Mitigation*: it is RAM-only and reboot-clears; the host script's documented teardown is `--rest-force-boiler off`, and a power cycle is the guaranteed reset.
- *Risk*: future refactors reorder `satBoilerHardwarePresent()` so the override no longer trips the section 4.2 edge, silently breaking the test path. *Mitigation*: the setter, not the getter, raises the edge (`satSetDebugForceBoilerPresent` calls `satNotifyBoilerFrameSeen`), keeping the trigger co-located with the override and verifiable by reading one function.

## Related Decisions

- **ADR-056 (Protected admin endpoint security and secret-handling contract)**: the force-boiler endpoint inherits ADR-056's HTTP Basic Auth plus CSRF same-origin enforcement and trusted-LAN assumption; constraint 2 (auth-gated) is ADR-056 applied to this route. No conflict: this is a normal mutating admin route, not an exception to it.
- **ADR-117 (SAT simulation contract)**: defines the simulation bus-isolation and boiler-absence availability semantics that the section 4.2 gate enforces. ADR-162 does not change those semantics; it adds a way to exercise the boiler-present edge of that contract on the bench. Complements, does not amend.

## References

- TASK-802 (this hook; commit `a226731d`); TASK-795 (the section 4.2 availability gate this validates, AC#8/#9/#10).
- Code: `src/OTGW-firmware/SATcontrol.ino` — `satBoilerHardwarePresent()` (`:1160`), `satSetDebugForceBoilerPresent()` (`:1176`), `satNotifyBoilerFrameSeen()` (`:1216`), `satOnBoilerDetected()` (`:1187`); `src/OTGW-firmware/restAPI.ino` `handleSAT()` force-boiler block (`:1054-1073`); `scripts/sat_boiler_emulator.py` `--rest-force-boiler` (`run_rest_force_boiler`, `:398`).
- Options analysis: `docs/plan/SAT_SIM_F7_BOILER_EMULATOR_OPTIONS.md`.
- On-device validation: OTGW32, firmware alpha.285, 2026-06-29 — enable sim, force-boiler/1, sim auto-disabled in roughly 2 seconds, re-enable returned 409, force-boiler/0 cleanup.
- OWASP guidance on test hooks and backdoors in shipped software (why the pattern must be constrained): https://owasp.org/www-community/vulnerabilities/

## Enforcement

No automated gate. This is a guideline-level ADR (per ADR-080): the four constraints (transient, auth-gated, idempotent, no new capability) are a design rubric applied at code review when any new test or debug hook is proposed, not a pattern a matcher can check. The load-bearing invariant for this specific hook — that `satDebugForceBoilerPresent` is RAM-only and never persisted — is small enough to verify by grepping its single declaration and its readers at review time.
