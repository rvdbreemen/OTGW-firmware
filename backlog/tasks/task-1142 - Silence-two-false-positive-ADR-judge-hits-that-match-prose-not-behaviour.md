---
id: TASK-1142
title: 'Silence two false-positive ADR judge hits that match prose, not behaviour'
status: Done
assignee:
  - '@claude'
created_date: '2026-09-20 11:21'
updated_date: '2026-09-20 11:28'
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
- [x] #1 OTGW-ModUpdateServer.h no longer declares the BearSSL namespace alias, and grep confirms nothing references ESP8266HTTPUpdateServerSecure
- [x] #2 The helperStuff.ino comment keeps its meaning without the literal debugTelnet.stop() token
- [x] #3 adr-judge over the two files reports no ADR-003 and no ADR-079 hit; the whole-codebase audit drops from 14 to 12 hits
- [x] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Two false-positive ADR judge hits removed at the code side; no behaviour change.

- OTGW-ModUpdateServer.h: deleted the inherited BearSSL namespace alias (ESP8266HTTPUpdateServerSecure over WiFiServerSecure). grep over src/ confirms nothing referenced it; the firmware is HTTP-only per ADR-003.
- helperStuff.ino: reworded the comment that carried the literal debugTelnet.stop() token; same meaning.

Evidence: adr-judge on the diff 0 violations; whole-codebase adr-audit dropped from 14 to 12 hits, with ADR-003 and ADR-079 no longer listed. build.bat produced fresh firmware and filesystem (13:26, Build completed successfully). evaluate.py --quick 38/38, health 100 percent.

Remaining 12: ten ADR-049 String sites tracked in TASK-1141, plus two pattern-literal hits (ADR-042 matching its own citation in a comment, ADR-094 wanting the HasData gate on the same line) that need sharper Enforcement patterns via supersession, not code.

Shipped on otgw-1.x.x as 7be7678a8.
<!-- SECTION:FINAL_SUMMARY:END -->
