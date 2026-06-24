# ADR-150: SAT Per-Heating-System COLD_SETPOINT (Active Boiler-Off Cutoff on Low Demand)

## Status

Accepted. Date: 2026-06-24 (maintainer approval; landed: satGetColdSetpoint()). Originally Proposed 2026-06-21.

## Status History

status_history:
  - date: 2026-06-21
    status: Proposed
    changed_by: Agent
    reason: Initial decision record
    changed_via: adr-kit

## Context

The SAT (Smart Autotune Thermostat) subsystem in this firmware is a C port of the thermo-nova "Smart Autotune Thermostat" Python component. The Python reference defines a COLD_SETPOINT constant: when the PID/curve requested setpoint falls below it, the home needs no heating, so the boiler is commanded fully off.

The OTGW firmware port had deliberately omitted COLD_SETPOINT. The original rationale, captured only as a code comment (SATcontrol.ino:1965-1969, the comment the task refers to as "~1927" before line drift), argued: "OTGW is a gateway, not the thermostat." The firmware acted as a pass-through that never commanded the boiler fully off on low demand, on the reasoning that the physical thermostat retains authority and Python uses a warm-idle cutoff only because it is itself the thermostat.

During the SAT spec-parity audit (epic TASK-891), George (the SAT author, on #dev-sat-mqtt 2026-06-20) ruled COLD_SETPOINT a critical SAT function, not an optional thermostat-only nicety. He further ruled that the Python's single global value (22C) was a bug: the correct cutoff is per heating system, because radiators and underfloor circuits idle at very different flow temperatures. Robert (maintainer) approved implementing it (TASK-891.2, 2026-06-20/21). This builds on the heating-source/heating-system enum split introduced in TASK-891.8, which separates the heating SYSTEM axis (radiators vs underfloor) from the heating SOURCE axis (boiler vs heat pump).

## Decision

When SAT is the active controller and the requested setpoint falls below a per-heating-system COLD_SETPOINT, SAT commands the boiler off rather than passing demand through.

`satGetColdSetpoint()` (SATcontrol.ino:242-246) returns the cutoff for the current heating system: radiators 28.2C (`SAT_COLD_SETPOINT_RAD`) or underfloor 21.0C (`SAT_COLD_SETPOINT_FLOOR`), selected via `satGetEffectiveHeatingSystem()` (constants at SATcontrol.ino:55-56). The values are fixed constants supplied by George, not user settings.

The cutoff has three effects in the SAT control path:

1. **Boiler-off command.** When `pidOutput < satGetColdSetpoint()` (SATcontrol.ino:4666-4672), SAT sets the final setpoint to `SAT_MIN_SETPOINT` (10.0C), modulation to 100, and the resulting commands sent to the boiler are CS=10 (CS is the gateway Control Setpoint command, the CH water target), CH=0 (CH is the central-heating-enable command, here heating off, derived because `finalSetpoint > SAT_MIN_SETPOINT` is false), and MM=100 (MM is the Maximum Modulation command, restored to maximum so the boiler is unconstrained when it next fires). This mirrors Python `heating_control.py` `heater_state` (line 72).

2. **PWM auto-switch gating.** The continuous/PWM mode auto-switch guards (overshoot-enable / underheat-disable) are skipped while below COLD_SETPOINT: the auto-switch only runs when `pidOutput > satGetColdSetpoint()` (SATcontrol.ino:4623). Mirrors Python guard gating (heating_control.py lines 280, 317).

3. **Flame-off PWM hold clamp.** The flame-off PWM hold setpoint is clamped to the range [COLD_SETPOINT, maximum_setpoint] (SATcontrol.ino:834-837). Mirrors Python flame-off-hold clamp (heating_control.py lines 397-398).

**Scope: this fires only while SAT is the active controller.** It does not change the SAT-disabled behavior. When SAT is turned off, the firmware still releases boiler control to the physical thermostat (CS=0, SATcontrol.ino:1965-1969); that path is unchanged. COLD_SETPOINT lives inside the active SAT control loop, not in the disable/handover path.

## Alternatives Considered

### Alternative A: Keep the pure pass-through stance (no COLD_SETPOINT)

This was the prior behavior: never command the boiler fully off on low demand, on the "OTGW is a gateway, not the thermostat" reasoning. Rejected because when SAT is enabled it IS the master controller (the thermo-nova design philosophy), and a master controller that cannot say "no heat needed" forces unnecessary low-demand firing. George, the SAT author, ruled the omission a defect, not a deliberate gateway virtue. The pass-through stance is correct only when SAT is disabled, which the decision preserves.

### Alternative B: A single global COLD_SETPOINT (Python's 22C)

Port the Python constant verbatim (22C for all systems). Rejected because George identified the single value as a bug in the Python reference. Radiator systems idle around a much higher flow setpoint than underfloor; a single threshold either lets radiators waste cycles (too low) or starves underfloor of legitimate low-grade heat (too high). Per-system values (28.2 / 21) are the corrected behavior.

### Alternative C: Make COLD_SETPOINT a user setting

Expose the two thresholds (or one configurable value) in settings. Rejected on KISS/YAGNI grounds: George supplied two known-good constants for the two supported heating systems, the heating-system axis is already a user choice (TASK-891.8), and adding two more tunables invites mis-tuning with no evidence of demand. If field validation later shows the constants need adjusting per installation, that is a future ADR, not speculative configurability now.

## Consequences

**Benefits**

- SAT now actively commands the boiler off on genuinely low demand (CS=10, CH=0, MM=100), matching the thermo-nova reference and removing unnecessary low-demand firing.
- The cutoff is correct per heating system (radiators 28.2C, underfloor 21C), avoiding the single-value bug in the Python source.
- One coherent cold-state definition gates all three dependent behaviors (boiler-off, PWM auto-switch, flame-off hold), so "cold" means the same thing everywhere in the control loop.

**Trade-offs**

- This departs from the prior pure-pass-through gateway stance: the firmware now takes an active boiler-off decision while SAT is enabled. This is intentional. SAT is the master controller when active; the gateway-only posture is retained only in the SAT-disabled path.
- The thresholds are fixed constants. An installation with atypical radiator or underfloor characteristics cannot retune them without a firmware change. Accepted deliberately (Alternative C).
- The threshold lives on the heating-SYSTEM axis (radiators/underfloor from TASK-891.8), not per device or per source, so a mixed-system installation uses one system selection.

**Risks and mitigations**

- *Risk*: a wrong heating-system selection applies the wrong cutoff (for example a radiator cutoff of 28.2C on an underfloor system would suppress legitimate low-grade heat). *Mitigation*: heating-system selection is an explicit user choice (TASK-891.8) surfaced in settings; the two constants are George's validated values; field validation on the 2.0.0 alpha line confirms behavior on real installations.
- *Risk*: the active boiler-off command surprises users who expected pure pass-through. *Mitigation*: documented here as a deliberate, author-endorsed behavior change; the SAT-disabled handover path is unchanged, so disabling SAT restores full thermostat authority.

## Related Decisions

- **ADR-085 (SAT Integration)**: parent decision that folds SAT into the firmware as an optional, opt-in master controller. This ADR refines SAT control behavior under that umbrella.
- **ADR-110 (SAT PV-surplus setpoint boost)**: composes cleanly with this decision and does not conflict. The PV-surplus boost is additive on the effective target; if it raises the requested setpoint at or above COLD_SETPOINT, heating re-enables, which is the intended behavior (surplus demand legitimately overrides the cold cutoff). No ordering dependency.
- **TASK-891.8 (heating-source / heating-system enum split)**: provides the radiators/underfloor heating-SYSTEM axis that `satGetColdSetpoint()` dispatches on. No dedicated ADR exists for the enum split yet; it is tracked as a task.
- **TASK-891 (SAT thermo-nova spec-parity audit, epic)** and **TASK-891.2 (this change)**: the audit that surfaced the omission and the task that implements it.
- This decision supersedes the deliberate-omission stance previously documented only as a code comment (SATcontrol.ino:1965-1969). That comment's rationale ("OTGW declines Python's COLD_SETPOINT because it is a gateway") is now overturned for the SAT-enabled path; the comment's still-true behavioral claim (SAT-disabled releases to the thermostat with CS=0) remains.

## References

- TASK-891.2 (implementation), TASK-891.8 (heating-system enum), TASK-891 (spec-parity audit epic).
- SATcontrol.ino:55-56 (cold setpoint constants), :242-246 (`satGetColdSetpoint`), :4666-4672 (boiler-off cutoff), :4623 (PWM auto-switch gate), :834-837 (flame-off hold clamp), :1965-1969 (unchanged SAT-disabled release-to-thermostat path with the now-partly-stale rationale comment).
- thermo-nova Python reference `heating_control.py`: line 72 (`heater_state` boiler off), lines 280 and 317 (guard gating), lines 397-398 (flame-off-hold clamp).
- George (SAT author) ruling, #dev-sat-mqtt, 2026-06-20. Maintainer approval (Robert), 2026-06-20/21.
- Field validation: 2.0.0 alpha line.

## Enforcement

Manual review only. This is an algorithmic runtime control decision with no clean declarative code surface: the rule is "below a per-system float threshold, command the boiler off across three coupled control points," which a regex on float constants cannot meaningfully assert and which would be brittle against legitimate refactoring. Compliance is verified by reading the SAT control path (the anchors in References) and by field validation on the 2.0.0 alpha line.
