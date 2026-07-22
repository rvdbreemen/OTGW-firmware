---
id: TASK-1047
title: Bench hypothese-testsuite voor de ~82-min heap-dood (TASK-1037)
status: To Do
assignee: []
created_date: '2026-07-21 21:13'
updated_date: '2026-07-22 01:02'
labels: []
dependencies: []
priority: high
ordinal: 166000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Hypothese-testsuite voor de ~82-min uptime-gebonden heap-dood (TASK-1037), uitgevoerd op bench-toestel 192.168.88.68 (onset-diag build 1.7.2-onset.1 e.v.). Bench heeft MQTT UIT, DHCP, NTP AAN, geen PIC.

Acceleratie-inzicht: de vermoedelijke trigger hangt aan de SDK-SNTP-update (SNTP_UPDATE_DELAY=3600000ms=1u, weak function overschrijfbaar). Door die op 30s te zetten gebeurt een SNTP-gebonden lek 120x zo vaak; dood dan in minuten i.p.v. 65min. Dat maakt een 90-min soak een 20-min test.

HYPOTHESES, gerangschikt:
H1 SDK-SNTP-update op 3600s (top). Test: probe-build met sntp_update_delay=30s. Snel dood => bevestigd. Overleeft => uitgesloten of bench reproduceert niet.
H2 DHCP-lease-vernieuwing (~1u). Test: static IP zetten (runtime-setting), reboot, soak. Geen DHCP-renew meer.
H3 do5minevent-tick. Test: alleen af te leiden uit onset-diag 1Hz-heap op eventniveau.
H4 MQTT-load. Al deels getest: bench heeft MQTT UIT. Sterft de bench toch => MQTT uitgesloten.

RUNTIME-vs-COMPILE: NTP-enable, static-IP en MQTT-enable zijn runtime-settings (REST /api/v2/settings), dus H2/H4 zonder rebuild. H1 vraagt een probe-build (sntp-interval). H3 vraagt alleen observatie.

VOLGORDE (fail-fast op sterkste hypothese): probe-1 fast-SNTP eerst. Dood in minuten bevestigt H1 EN dat de bench reproduceert, in 20 min.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fast-SNTP probe (30s interval) gedraaid op bench; heap-gedrag vastgelegd
- [x] #2 H1 SDK-SNTP bevestigd of uitgesloten met bewijs
- [x] #3 Bij uitsluiting H1: H2 (static IP) en H4 (MQTT) getest via runtime-settings
- [x] #4 Conclusie met de bewezen of resterende oorzaak vastgelegd in TASK-1037
<!-- AC:END -->
