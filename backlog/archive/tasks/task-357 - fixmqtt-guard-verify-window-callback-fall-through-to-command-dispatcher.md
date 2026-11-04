---
id: TASK-357
title: 'fix(mqtt): guard verify-window callback fall-through to command dispatcher'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:33'
updated_date: '2026-04-21 17:19'
labels:
  - code-review
  - mqtt
  - security
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2A MEDIUM (CWE-20): verify-window filter in handleMQTTcallback at MQTTstuff.ino:629-656 only returns on the exact 3-segment haprefix topic structure. When broker delivers a topic under <haprefix>/ without the expected substructure, the filter falls through to the command dispatcher below. A crafted retained topic under the prefix could sneak into the OT command path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Verify-window filter always returns once topic matches the haprefix byte-prefix, regardless of substructure parsing result
- [x] #2 Non-matching substructure is counted as verifyOrphanCount rather than falling through
- [x] #3 Well-formed discovery topics still counted correctly via verifyReceivedCount
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read current callback at handleMQTTcallback (MQTTstuff.ino ~line 649-676).
2. Restructure so that any topic whose byte-prefix matches haprefix unconditionally returns after being counted (verifyReceivedCount or verifyOrphanCount).
3. Make non-matching substructure (missing slash1, missing slash2, oversize node segment) count as orphan rather than fall through.
4. Preserve verifyReceivedCount for well-formed topics matching NodeId.
5. Build and verify.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed a fall-through in the MQTT verify-window callback filter (CWE-20). Any retained topic matching the haprefix byte-prefix now always returns after being counted — no path can leak into the OT command dispatcher.

Changes:
- handleMQTTcallback in MQTTstuff.ino: refactored the verify filter so that malformed substructure (missing slash1, missing slash2, oversize node segment) increments verifyOrphanCount and returns. Only a well-formed <haprefix>/<component>/<nodeId>/... topic matching our NodeId still increments verifyReceivedCount. The single return statement at the end of the haprefix-match block is now unconditional.
- Added inline comment citing TASK-357 and the CWE-20 root cause to prevent future regressions.

Risk: none observable — the pre-fix paths that fell through would only have fired on adversarial or misconfigured brokers under an already-active verify window, which is opt-in (settings.mqtt.bDiscoveryAutoVerify) and naturally short (15s).

Build: python build.py --firmware passed.
<!-- SECTION:FINAL_SUMMARY:END -->
