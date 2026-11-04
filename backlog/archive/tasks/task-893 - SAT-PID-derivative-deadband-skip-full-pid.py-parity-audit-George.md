---
id: TASK-893
title: 'SAT: PID derivative deadband-skip + full pid.py parity audit (George)'
status: Done
assignee: []
created_date: '2026-06-21 07:56'
updated_date: '2026-06-21 08:06'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATpid.ino
  - other-projects/SAT-releases-thermo-nova/custom_components/sat/pid.py
priority: high
ordinal: 117000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George (#dev-sat-mqtt 2026-06-21) reported the PID D-term swings 0->7 on a stable temperature with +-0.05 BLE-sensor jitter -> unstable controller. Root cause: firmware _update_derivative lacks the Python pid.py:205 guard 'if abs(error) <= DEADBAND: skip derivative', so sensor jitter at setpoint feeds the derivative. George chose option 1 (add the deadband-skip = exact parity) AND asked for a thorough pid.py-vs-SATpid.ino parity audit confirming the implementation is identical.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add the |error| <= deadband derivative-skip to SATpid.ino _update_derivative, matching pid.py:205 (skip when within deadband; update last_derivative timestamp).
- [x] #2 Thorough pid.py (thermo-nova) vs SATpid.ino parity audit across P, I, D, deadband, integral accumulation+windup/clamp, derivative filter+cap+sign, gains, sample-time gating; document each discrepancy and fix it or justify the intentional difference.
- [x] #3 build esp32 exit 0; evaluate.py --quick 0-fail; prerelease bumped; pushed.
- [x] #4 Report parity confirmation (or the justified differences) to George on #dev-sat-mqtt.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Full pid.py (thermo-nova) parity audit: SATpid.ino is term-by-term identical (kp=coeff*hcv/[4 underfloor|3 else], ki=kp/8400, kd=0.07*8400*kp, P=kp*error, integral reset-outside/accumulate-inside-deadband clamped +/-curve, derivative -dT/dt with >60s gate / +/-5 cap / alpha=dt/(60+dt), output=curve+P+I+D). The deadband derivative-skip was already present; fixed the only non-identical bit (it checked the previous-cycle error -> now uses the current error, matching pid.py:205). The reported D 0->7 swing is inherent to kd*rawDeriv under sensor jitter while error>deadband (present in pid.py too; D freezes at setpoint), not a port bug; any taming is a deliberate divergence pending George. Two firmware-only safety extras (integral solar-freeze + abs-cap; 10s min-update gate) left intact pending George's keep/strip call. esp32 SUCCESS (fw+fs), evaluate 0-fail. Audit reported to George (msg 1518163886585548901).
<!-- SECTION:FINAL_SUMMARY:END -->
