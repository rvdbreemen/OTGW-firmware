---
id: TASK-760
title: 'fix(network): triple-reset must clear WiFi credentials per ADR-043'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 10:25'
updated_date: '2026-05-29 10:28'
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
- [ ] #1 startWiFi() forcePortal path calls resetSettings() so stored WiFi credentials are erased on triple-reset
- [ ] #2 Behaviour matches ADR-043 (clear creds, then force portal)
- [ ] #3 Docs/manuals updated by TASK-752 re-aligned to 'credentials cleared' wording
- [ ] #4 ESP8266 + ESP32 build green; field-verify triple-reset wipes creds
<!-- AC:END -->
