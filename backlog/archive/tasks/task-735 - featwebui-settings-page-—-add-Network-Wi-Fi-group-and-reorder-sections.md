---
id: TASK-735
title: 'feat(webui): settings page — add Network/Wi-Fi group and reorder sections'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 21:42'
updated_date: '2026-05-27 22:06'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
De algemene settings pagina heeft geen netwerk-sectie. WiFi-status velden (ssid, wifirssi, wifiquality) en Ethernet-instellingen (ethstaticip etc.) vallen in de 'other' catch-all groep, ver uit elkaar. De WiFi Scan knop staat helemaal onderaan als losse sectie.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 WiFi-status en Ethernet-instellingen staan in dezelfde Network/Wi-Fi kaart
- [x] #2 WiFi Scan knop staat in de Network kaart, niet onderaan de pagina
- [x] #3 Geen netwerk-gerelateerde instellingen meer in de 'other' groep
- [x] #4 Volgorde van groups is logisch: system → network → mqtt → ntp → behavior → ui → otgw → sensors → s0 → outputs → webhook → other
- [x] #5 translateFields entries aanwezig voor alle netwerk-velden
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Network/Wi-Fi group toegevoegd aan SETTINGS_GROUPS, WiFi Scan panel staat nu direct na de Network kaart, groups worden pre-created in volgorde.
<!-- SECTION:FINAL_SUMMARY:END -->
