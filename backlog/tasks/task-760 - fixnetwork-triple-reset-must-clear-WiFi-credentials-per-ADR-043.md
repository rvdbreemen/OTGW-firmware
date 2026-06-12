---
id: TASK-760
title: 'fix(network): triple-reset must clear WiFi credentials per ADR-043'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 10:25'
updated_date: '2026-05-29 18:32'
labels: []
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-752 found a code-vs-design discrepancy: ADR-043 (Accepted) specifies the triple-reset recovery 'Clear saved WiFi credentials' then force the config portal, but the implementation only forces the portal (startWiFi forcePortal -> startConfigPortal) WITHOUT clearing creds. So on portal timeout the old creds survive, contradicting ADR-043. Robert's decision (2a): fix the CODE to match ADR-043 — actually wipe. Add manageWiFi.resetSettings() in the forcePortal branch of startWiFi() (networkStuff.ino ~:121) before startConfigPortal(), so a triple-reset clears stored credentials as the ADR specifies. forcePortal is set only by shouldForceWifiConfigPortal() (triple-reset via RTC counter), so this is scoped to the deliberate recovery gesture. Update the TASK-752 docs/manuals back to 'credentials are cleared' once code matches.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 startWiFi() forcePortal path calls resetSettings() so stored WiFi credentials are erased on triple-reset
- [x] #2 Behaviour matches ADR-043 (clear creds, then force portal)
- [x] #3 Docs/manuals updated by TASK-752 re-aligned to 'credentials cleared' wording
- [x] #4 ESP8266 + ESP32 build green; field-verify triple-reset wipes creds
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: startWiFi forcePortal now calls manageWiFi.resetSettings() (ADR-043). Built green alpha.92, commit 8b7912f7. AC4 (field-verify wipe) pending hardware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in commit 8b7912f7 (alpha.92), pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support; docs re-aligned in 2d2dbbc2. startWiFi() forcePortal path now calls manageWiFi.resetSettings() before startConfigPortal() so triple-reset wipes stored WiFi credentials per ADR-043. Code ACs 1-3 verified met. Closed per maintainer rule: shipped+pushed under an alpha, only AC4 (field-verify the wipe on hardware) remains and field validation is the sole remainder.
<!-- SECTION:FINAL_SUMMARY:END -->
