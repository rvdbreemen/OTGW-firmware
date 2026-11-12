---
id: TASK-1837
title: >-
  Home: show the appliance type as a label (Boiler, Heat pump, or both for
  hybrid) instead of a Boiler/Heat pump switch
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:47:00Z'
updated_date: '2026-11-12T08:01:00Z'
labels:
  - webui
dependencies: []
priority: medium
ordinal: 915000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-12: the Home 'Boiler | Heat pump' pair reads as two labels that can never both be true; show only the one that applies, both for hybrid. Now #applToggle (v2.html design A status line) is a two-button switch: a click POSTs satsource (setSource) and applyApplianceToggle() highlights effectiveSource() (satsource, or sat.detected when Auto). The appliance type is set in Settings/onboarding, so Home only shows it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Home shows one non-clickable label: 🔥 Boiler (gas), ⚡ Heat pump (heat pump and on/off heat pump), both for hybrid; Auto shows the detected type
- [x] #2 Home no longer writes satsource; the schematic still switches to the heat pump drawing
- [x] #3 Test red on the old code; browser screenshots for gas, heat pump, hybrid
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: #applTags with two non-clickable labels (.appl-tag, hidden by default); applianceTags(src, known): gas for 1 and 3, heat pump for 2, 3 and 4, nothing until sat/status answered (sat.srcKnown); the schematic's heat pump drawing for 2 and 4. setSource() and the click wiring are removed, so Home no longer POSTs satsource (the type is set in Settings/onboarding). Dead .seg2btn CSS removed. Tests: home-appliance-label.test.mjs 2 red on the old code, 2 green; home-heatpump-wording 3, i18n-no-hardcoded 3, v2-undefined-names 2. Headless Chrome with fake sat/status: gas ['🔥 Boiler', boiler drawing], hp ['⚡ Heat pump', hp drawing], hybrid ['🔥 Boiler','⚡ Heat pump'], 0 exceptions; screenshots C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1837-{gas,hp,hybrid}.png.

Bench 2026-11-12 by otgw-bench-tester (09) on 2.0.0-beta.208+e449692 (covers beta.204-208), OTA app+fs on .61 (OT-Direct, no PIC; bootcount 445->447) and .63 (Classic, PIC 6.8; 15->17), githash checked on both, settings unchanged, crashlog clean; screenshots cropped (no host/IP/SSID) in 09's scratchpad b208/. TASK-1837 PASS: no switch element (#applToggle / .seg2btn count 0); .61 (satsource 3, hybrid) shows '🔥 Boiler' and '⚡ Heat pump'; .63 shows '🔥 Boiler'. Gas-only and heat-pump-only not tested live on the bench (no settings changes allowed); covered by the headless screenshots t1837-{gas,hp}.png with a fake sat/status.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Home shows the appliance type as a non-clickable label (gas: Boiler, heat pump: Heat pump, hybrid: both) and no longer writes satsource (2.0.0-beta.206, pushed in ef9aabf4c). AC1: bench 09 on beta.208+e449692: .61 hybrid two labels, .63 Boiler, 0 switch elements; gas/heat-pump-only from headless Chrome with fake sat/status (t1837-gas/hp/hybrid.png). AC2: setSource() and its click wiring removed; heat pump drawing still selected for sources 2 and 4 (home-appliance-label.test.mjs). AC3: home-appliance-label.test.mjs 2 red on the old code, 2 green (rerun after the rebase: 2 pass); home-heatpump-wording 3, i18n-no-hardcoded 3, v2-undefined-names 2.
<!-- SECTION:FINAL_SUMMARY:END -->
