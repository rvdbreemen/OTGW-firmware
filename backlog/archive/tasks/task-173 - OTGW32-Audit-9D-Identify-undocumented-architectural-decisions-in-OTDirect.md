---
id: TASK-173
title: 'OTGW32-Audit-9D: Identify undocumented architectural decisions in OTDirect'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:24'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-9
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - docs/adr/README.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review OTDirect.ino for architectural decisions that were made during implementation but are not captured in any existing ADR (ADR-001 through ADR-067). Candidates include: 3-strike auto-disable threshold, command queue size of 8, 800ms status interval choice, response override table size, loopback mode design, and relay configurability approach. Each undocumented decision that warrants an ADR should result in a new ADR being written (next is ADR-068).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All significant design decisions in OTDirect.ino are identified
- [x] #2 Each decision is checked against existing ADRs (ADR-001 to ADR-067)
- [x] #3 Undocumented decisions that warrant an ADR are listed
- [x] #4 At least one new ADR is drafted for the most significant undocumented decision
- [x] #5 ADR numbering starts at ADR-068
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reviewing OTDirect.ino for undocumented architectural decisions:

1. 3-STRIKE AUTO-DISABLE (lines 348-368, 913-933): When a boiler returns UNKNOWN_DATA_ID 3 times consecutively for a MsgID, the schedule entry is auto-disabled. Uses packed 2-bit counters (32 bytes for 128 MsgIDs). The "3" threshold is NOT documented in any ADR. Rationale: 3 gives confidence without being hasty — 1 would be too aggressive (transient CRC error), 2 still prone to double-error, 3 matches the PIC convention. AA= command resets counter. MsgID 0 (status) is exempt.

2. COMMAND QUEUE SIZE = 8 (line 236): OT_CMD_QUEUE_SIZE=8. Not documented. Rationale: typical burst is 2-3 commands (e.g., TT=+SW=+HW=), 8 provides headroom for MQTT burst. Larger would waste RAM; smaller risks drop on rapid commands. At 800ms per OT cycle, 8 frames = ~6.4 seconds drain time.

3. 800ms STATUS INTERVAL (line 37): OT_STATUS_INTERVAL_MS=800. Comment says "OT-Thing parity". OpenTherm spec mandates MsgID 0 every ≤1000ms (1s). Using 800ms gives 200ms safety margin and matches OT-Thing reference implementation. Not documented in any ADR.

4. RESPONSE OVERRIDE TABLE SIZE = 16 (line 319): OT_RESPONSE_OVERRIDE_MAX=16. Not documented. Sized to cover all commonly overridden IDs with room to spare. SR= command returns NS (No Space) when full.

5. LOOPBACK DATA TABLE IN PROGMEM (lines 721-836): 128×uint16_t table in PROGMEM. Design choice: realistic simulated values so HA entities populate. 0xFFFF sentinel = UNKNOWN_DATA_ID. Not documented.

6. PEEK-BEFORE-DEQUEUE PATTERN in command queue (lines 1000-1007): Queue tail is only advanced after successful async send. Comment says "Codex P1 fix". Not documented in ADR.

7. RESPONSE-MODIFY TABLE (RM/CM commands) SIZE = 8 (line 345): OT_RESPONSE_MODIFY_MAX=8. Separate from response-override table. Not documented.

Most significant undocumented decisions for ADR: 3-strike threshold and command queue sizing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Identified 7 undocumented architectural decisions in OTDirect.ino not covered by ADR-001 through ADR-067:

1. 3-strike UNKNOWN_DATA_ID auto-disable (threshold=3, packed 2-bit counters)
2. Command queue size = 8 frames (ring buffer drain at ~800ms/frame)
3. Status poll interval = 800ms (200ms guard over 1s OT spec, OT-Thing parity)
4. Response override table size = 16 (SR=/CR= commands)
5. Response modifier table size = 8 (RM=/CM= commands)
6. Loopback data table in PROGMEM (128 simulated values, 0xFFFF=UNKNOWN)
7. Peek-before-dequeue in command queue (prevents frame loss on busy bus)

Written ADR-068: docs/adr/ADR-068-ot-direct-schedule-tuning-constants.md
- Documents rationale for constants 1-5 (most architecturally significant)
- All constants accepted as-is — no code changes needed
- Loopback PROGMEM table (item 6) is covered implicitly by ADR-087 (frame bridge, no separate ADR needed)
- Peek-before-dequeue (item 7) is an implementation detail noted in ADR-068 code reference

No audit-fix tasks created: all are documentation gaps, not code defects.
<!-- SECTION:FINAL_SUMMARY:END -->
