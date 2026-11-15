---
id: TASK-1925
title: >-
  OT-Direct: a boiler Data-Invalid answer is cached as a value
  (otBoilerCacheStore); master mode then answers READ_ACK with the Data-Invalid
  data bytes
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T08:07:00Z'
updated_date: '2026-11-15T02:19:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 986000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1808. otBoilerCacheStore() (OTDirect.ino) clears the slot only for UNKNOWN_DATA_ID; a DATA_INVALID reply stores its data bytes and sets otBoilerCacheValid. handleMasterModeSlaveFrame() then answers a thermostat READ of that id with READ_ACK and those bytes (host check during TASK-1808: boiler B6:5:0000 then thermostat READ 5 gave type 4 data 0000). Every other cache reader (flow temp 25, flame ratio 0, MsgID 3, bounds 48/49, ret slot) sees the same. Spec v4.2 4.3: Data-Invalid carries no valid value. Fixing it changes all those readers, so it is a separate change: decide whether DATA_INVALID clears the slot or keeps the previous value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A boiler Data-Invalid reply no longer becomes a cached value; the chosen rule (clear or keep previous) is named with its consequence for each cache reader
- [x] #2 Host test against the real code: red on the old code, green on the fix
- [x] #3 esp32-combo build, evaluate.py --quick, bench where the hardware allows
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Finding (d3): the defect is not reachable on the current code. A real boiler's DATA-INVALID arrives with library status INVALID; handleMasterResponse() takes the boilerDataInvalid branch, which bridges the frame and never calls otBoilerCacheStore() ('Not cached', OTDirect.ino handleMasterResponse). The other caller, the loopback path in sendMasterRequestAsync(), stores every simulated reply, but simulateLoopbackResponse() only returns WRITE_ACK, UNKNOWN_DATA_ID and READ_ACK. The TASK-1808 host check called otBoilerCacheStore() directly. Fix as defence in depth.

Rule (AC1): keep the previous value. otBoilerCacheStore() returns on DATA_INVALID, the rule handleMasterResponse already applies to a real boiler, so no reader changes behaviour on hardware today. Per reader: master-mode slave answers (handleMasterModeSlaveFrame) keep answering the last valid value, or Data-Invalid if none yet; flow temp 25 / flame bit in MsgID 0 (SAT, loop stats), MsgID 3 (otIsVentSlave, otDirectBoilerPresent), bounds 48/49 (otdUpperBoundF88) and the MsgID 28 ret slot keep the last valid value. Clearing instead was not chosen: it would change hardware behaviour (a boiler that answers Data-Invalid once would empty its slot), which needs its own decision. Evidence: tests/test_otd_master_empty_slot.py new cases data_invalid_is_not_cached_on_an_empty_slot and data_invalid_keeps_the_previous_value: fix 8/8 OK; old (HEAD ced0614fa) FAILED (failures=2): [(5, 4, '0000')] and [(25, 4, '1234')]. test_otd_loopback_not_boiler_for_sat OK, host suites test/host/test_override_reply.py OK, evaluate.py --quick 0 failed. c4-code-otdirect.md updated. AC3 open (build + bench; nothing bench-visible: unreachable path).

Lane-1924 pushed 2026-11-15: origin/dev 5da0c91df..2f4818aa1. Build esp32-combo 2.0.0-beta.281+fe0e61a OK (build/logs/build-20261115-020302.log, 'Build completed successfully'); 'git diff fe0e61a48 2f4818aa1 -- src/' empty, so the pushed code is the benched code. evaluate.py --quick 0 failed; OTD tests 42 files, host suites, SAT suite per file green on the rebased tree. Not bench-visible (the path is unreachable on current code); host-tested only. Version beta.280.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
otBoilerCacheStore() keeps the previous value on a Data-Invalid reply (the rule the hardware path already applies). AC1/AC2: test_otd_master_empty_slot 2 new cases red/green. AC3: build beta.281+fe0e61a OK, evaluate 0 failed, nothing bench-visible. Shipped in beta.280.
<!-- SECTION:FINAL_SUMMARY:END -->
