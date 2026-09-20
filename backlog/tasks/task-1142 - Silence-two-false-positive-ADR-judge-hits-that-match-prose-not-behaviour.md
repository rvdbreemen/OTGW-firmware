---
id: TASK-1142
title: 'Silence two false-positive ADR judge hits that match prose, not behaviour'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-20 11:21'
updated_date: '2026-09-20 11:22'
labels:
  - housekeeping
  - adr
dependencies: []
priority: low
ordinal: 223000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The whole-codebase ADR audit of 2026-09-20 flags two lines that violate nothing:

1. ADR-003 on OTGW-ModUpdateServer.h:95. The header carries "namespace BearSSL { using ESP8266HTTPUpdateServerSecure = ...Template<WiFiServerSecure>; }", a type alias inherited from the upstream ESP8266HTTPUpdateServer header. Nothing in the firmware names that alias; no TLS is linked or used. The Enforcement pattern is the bare word BearSSL, so the alias trips it forever.

2. ADR-079 on helperStuff.ino:457. A comment reads "After debugTelnet.stop() runs inside prepareForReboot()". The pattern debugTelnet\.(stop|disconnect)\(\) is meant for emergencyHeapRecovery() but the glob covers the whole file, so the prose matches.

Both ADRs are Accepted, so their Enforcement blocks cannot be sharpened without a supersession. The cheaper route is on the code side: delete the dead alias and reword the comment so neither carries the literal token. No behaviour changes.

Out of scope, left as known noise: ADR-042 matching its own citation in a comment at MQTTstuff.ino:51, and ADR-094 wanting the dhwWaterMeterHasData() gate on the same line as setMQTTConfigPending (it is two lines above, MQTTstuff.ino:1160). Those need sharper patterns in a superseding ADR, not code edits.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW-ModUpdateServer.h no longer declares the BearSSL namespace alias, and grep confirms nothing references ESP8266HTTPUpdateServerSecure
- [ ] #2 The helperStuff.ino comment keeps its meaning without the literal debugTelnet.stop() token
- [ ] #3 adr-judge over the two files reports no ADR-003 and no ADR-079 hit; the whole-codebase audit drops from 14 to 12 hits
- [ ] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->
