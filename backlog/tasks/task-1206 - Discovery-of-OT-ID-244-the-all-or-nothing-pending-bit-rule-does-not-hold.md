---
id: TASK-1206
title: 'Discovery of OT ID 244: the all-or-nothing pending-bit rule does not hold'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 17:55'
updated_date: '2026-10-04 09:16'
labels:
  - ha-discovery
  - mqtt
dependencies: []
priority: low
ordinal: 328000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier. The comment at MQTTstuff.ino:2764-2767 says the pending bit for ID 244 clears only when all nine configs published. But the two ID-244 binary-sensor rows (MQTTHaDiscovery.cpp:1459-1460) set result=true in the binary section (MQTTstuff.ino:2697) before the piccontrols block (:2768-2777), which can only add true. A failed Reset Gateway button or GPIO/LED select publish is therefore never retried by the drip.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Either the rule holds (the pending bit stays set until every ID-244 config published, shown by a host test with an injected publish failure) or the comment is corrected to describe the actual rule
- [x] #2 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (rule made to hold, not the comment weakened): in doAutoConfigureMsgid() the ID-244 block assigns result = allOk instead of OR-ing it. The two ID-244 binary-sensor rows set result true first, which hid a failed Reset Gateway button or GPIO/LED select publish, and the drip never retried them. Infinite-retry check (the PR #596 failure mode): the harness shows the button and selects 0..7 return true on both engines with a working publish, so result = allOk only stays false while a publish actually fails.
Evidence: host harness part E injects a publish failure at the mqttPublishRaw double and calls doAutoConfigureMsgid(244). test/host/test_ha_discovery_json.py --old-rev 8795bacc0 gives RESULT: PASS. FIX returns {none: 1, /resetgateway/: 0, /select/: 0}; OLD returns {none: 1, /resetgateway/: 1, /select/: 1}, i.e. done despite the failure.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The ID-244 all-or-nothing rule now holds. doAutoConfigureMsgid() assigns result = allOk in the PIC-controls block instead of OR-ing it, so the two ID-244 binary-sensor rows can no longer hide a failed Reset Gateway button or GPIO/LED select publish, and the drip keeps the pending bit and retries. Evidence: host harness part E injects a publish failure at the mqttPublishRaw double. FIX returns 0 for a failed button or select and 1 with no failure; OLD (8795bacc0) returns 1 in all three cases. Composers 0..7 and the button return true on both engines, so the PR #596 infinite retry cannot occur. Builds: esp32, esp32-classic, esp32-combo SUCCESS (alpha.412); evaluate.py --quick 0 failures.
<!-- SECTION:FINAL_SUMMARY:END -->
