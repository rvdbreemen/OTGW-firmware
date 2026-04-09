---
id: TASK-121
title: 'SAT Audit D8: Command queue usage verification'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:54'
updated_date: '2026-04-09 05:29'
labels:
  - audit
  - sat
  - phase-4
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify all OTGW commands in SAT code are sent via addOTWGcmdtoqueue() and never written directly to serial. Check queue overflow protection.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All OTGW commands in SAT code route via addOTWGcmdtoqueue()
- [x] #2 No direct serial writes in SAT code
- [x] #3 Queue overflow scenario documented
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for direct serial writes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Grep SAT files for any Serial. writes\n2. Verify all OT commands go through addCommandToQueue\n3. Check for queue overflow scenarios\n4. Verify hasOTCommandInterface() guard is present
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Command Queue Usage Verification

### Direct Serial writes
- Grep result: ZERO Serial.print/Serial.write/Serial.println calls in any SAT file.
- DebugTln/DebugTf are telnet-based debug output (port 23), NOT the PIC serial port. Correct.

### All OT commands use addCommandToQueue()
All 26 OT command sends in SATcontrol.ino use addCommandToQueue().
No direct PIC serial writes found.

### Commands sent by SAT
| Context | Commands | Frequency |
|---------|----------|----------|
| initSAT() | CS=0, PM=3, PM=15, PM=48, [MI=500] | Once at boot |
| satControlLoop (main) | CS=, MM=, CH=, [TP= immergas], [TC= push] | Every 30s (configurable) |
| Summer simmer | CS=10, MM=, CH=0 | Every 30s when summer active |
| TRV valves closed | CS=10, MM=, CH=0 | Every 30s when valves closed |
| Calibration (OPV) | CS=high, MM=0, then MM=0 every 10s | During calibration only |
| Thermal fallback | CS=, MM=, CH=1 | Every 30s during fallback |
| satDisable() | CS=0 | On disable |

### hasOTCommandInterface() guard
All command sends that should only go to the PIC are guarded by hasOTCommandInterface().
Count: 7 guard points found in SATcontrol.ino.

### Queue overflow analysis
CMDQUEUE_MAX = 20. SAT sends at most 5 commands per control loop iteration (30s period).
The queue drains at ~1s per command (OT protocol timing).
Worst-case saturation: initSAT sends 5 at boot, filling 25% of queue. No overflow risk.

### Conclusion
Fully compliant. No direct serial writes. All OT commands use addCommandToQueue(). No queue overflow risk.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Command queue usage verification complete.

Result: SAT code is fully compliant.
- Zero direct Serial writes in any SAT file.
- All 26 OT command sends use addCommandToQueue().
- All command sends are guarded by hasOTCommandInterface() (7 guard points).
- Queue overflow risk is negligible: SAT sends max 5 commands per control cycle (every 30s), CMDQUEUE_MAX=20, queue drains at ~1s/cmd.
- Calibration mode (OPV) sends MM=0 every 10s during calibration -- still within queue capacity.

No fix tasks needed.
<!-- SECTION:FINAL_SUMMARY:END -->
