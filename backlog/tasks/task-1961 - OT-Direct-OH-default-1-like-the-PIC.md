---
id: TASK-1961
title: 'OT-Direct: OH= default 1 like the PIC'
status: To Do
assignee: []
created_date: '2026-11-15T07:58:00Z'
labels:
  - otdirect
dependencies: []
priority: low
ordinal: 1003000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1750. The PIC factory default sets OverrideHigh (gateway.asm 6.8 SavedSettings :5459, IgnoreErr1 and OverrideHigh; OverrideHigh = settings bit 6 :512), so a PIC answers a thermostat MsgID 100 read with the override flags in both bytes (MessageID100 :3293-3311) and PR=T shows 11 (tests/serial-2.stc). OT-Direct defaults OH to 0 (settings.otd.bOverrideHB = false, OTDirecttypes.h:131). Robert chose A by DM 1571432519373557801 (on the coordinator's recommendation DM 1571432372916850780): default 1 like the PIC, only for new installs and units without a stored OH; a stored OH=0 stays. Finding before the build (session 95): writeSettings() writes every key including OTDoverridehb (settingStuff.ino:499, since TASK-1836, first in v2.0.0-beta.203) and boot writes settings.ini only when it is missing, so 'not stored' is only a new install or a settings.ini last written before beta.203; any unit on beta.203+ that saved a setting since has false stored and keeps 0. The coordinator put that nuance back to Robert (A as stated or B a one-time migration with a marker): HOLD the implementation until that answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's answer on the nuance (A as stated / B one-time migration) recorded with the DM id
- [ ] #2 Settings default changed as decided; host test on the real settings load: a new install (no settings.ini) gives OH=1, a stored OTDoverridehb=false gives 0 (and, for B, the migration runs once)
<!-- AC:END -->
