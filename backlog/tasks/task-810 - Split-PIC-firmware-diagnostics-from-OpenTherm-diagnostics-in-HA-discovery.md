---
id: TASK-810
title: Split PIC firmware diagnostics from OpenTherm diagnostics in HA discovery
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 06:20'
updated_date: '2026-06-02 06:49'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field feedback: HA discovery currently lumps OpenTherm diagnostics, ESP/firmware stats, and PIC firmware info/settings/controls into one device's single 'diagnostic' entity_category. Split them so PIC-related and ESP/stats-related entities form distinct groupings separate from OpenTherm diagnostics. HA's only native grouping beyond entity_category is separate devices linked via via_device.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-084 reviewed and accepted by user before merge
- [x] #2 PIC info/settings/controls (pseudo-IDs 249/250/251) register under a distinct OTGW PIC HA device via via_device
- [x] #3 ESP stats + firmware diagnostics (pseudo-IDs 247/248) register under a distinct OTGW ESP HA device via via_device
- [x] #4 OpenTherm diagnostics and all OT values remain on the main OpenTherm Gateway device
- [x] #5 unique_id, MQTT topic paths, and entity_category values are unchanged (entity history + ADR-062 verification preserved)
- [x] #6 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. ADR-084 (Proposed) documents multi-device topology — await user acceptance before flipping to Accepted.
2. MQTTstuff.h: add enum class HaDeviceGroup { main, esp, pic }; add HaDiscoveryContext::deviceGroup field (default main).
3. mqtt_configuratie.cpp: add kViaDevice PROGMEM key; extend writeDeviceBlock() to emit sub-device block (ids <nodeId>-pic/-esp, name OTGW PIC/ESP (host), via_device <nodeId>) when deviceGroup != main; main keeps first-entity-full optimisation.
4. MQTTstuff.ino: add deviceGroupForMsgId(msgId) helper (247/248->esp, 249/250/251->pic, else main); set ctx.deviceGroup in doAutoConfigureMsgid after buildDiscoveryContext.
5. Build (python build.py --firmware) exit 0; evaluate.py --quick green.
6. Commit to claude/pic-firmware-diagnostics-split-axwtP, push, open draft PR.
7. Create 2.0.0 sibling task for the cross-worktree port (cannot push 2.0.0 from this branch-scoped session).
<!-- SECTION:PLAN:END -->
