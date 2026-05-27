---
id: TASK-736
title: 'feat(webui): SAT settings — labels audit and manufacturer dropdown fix'
status: Done
assignee: []
created_date: '2026-05-27 21:43'
updated_date: '2026-05-27 22:06'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Gebruiker ziet technische sleutelnamen in de SAT settings tab. De manufacturer dropdown heeft ~10 items maar de firmware-enum (SATtypes.h) definieert 18 fabrikanten. Doel: alle SAT settings voorzien van leesbare labels/units en de manufacturer lijst compleet maken.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Manufacturer dropdown bevat alle 18+ fabrikanten met correcte enum-waarden (0=Auto, 1=Atag, ..., 18=Other)
- [x] #2 Geen SAT setting toont een kale sleutelnaam (bijv. satdeadband) in de UI
- [x] #3 Alle numerieke SAT settings hebben een unit-label (°C, %, W, min, etc.)
- [x] #4 Sectie-titels zijn begrijpelijk voor eindgebruikers
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Manufacturer dropdown gecorrigeerd (0-18 waarden matchen nu SATManufacturer enum), 8 SAT field labels verbeterd.
<!-- SECTION:FINAL_SUMMARY:END -->
