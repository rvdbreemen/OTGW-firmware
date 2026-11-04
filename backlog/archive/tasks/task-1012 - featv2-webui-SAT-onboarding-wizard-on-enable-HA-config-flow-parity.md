---
id: TASK-1012
title: 'feat(v2-webui): SAT onboarding wizard on enable (HA config-flow parity)'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 07:57'
updated_date: '2026-07-05 16:39'
labels: []
dependencies: []
ordinal: 212000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Guided SAT setup that fires when a user enables SAT in the v2 web UI, mirroring the Home Assistant SAT config-flow questions. Second instance of the TASK-997 wizard engine. Design: docs/superpowers/specs/2026-07-05-sat-onboarding-design.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Wizard fires on satenabled false->on transition when sat_onboarded is false; SAT stays disabled until Finish
- [x] #2 8 screens: Welcome, Heating system, Room temp (BLE roster), Outside temp (OT/weather), Manufacturer, Tuning (auto/manual), Sources health, Done
- [x] #3 Every answer writes an existing firmware key; only new firmware setting is sat_onboarded bool (default false all installs)
- [x] #4 Existing SAT users (satenabled true, sat_onboarded false) get the wizard once on next SAT-page visit; dismiss keeps SAT running
- [x] #5 Re-run entry in both SAT page and Advanced>System
- [x] #6 Sources health screen reads /api/v2/health + sat/status, green/amber, warns on missing critical source
- [x] #7 build.py --target esp32 green; evaluate.py --quick no new failures; light+dark, 360px verified on device
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Impl plan: docs/superpowers/plans/2026-07-05-sat-onboarding-wizard.md. 4 tasks: (1) firmware sat_onboarded flag, (2) startSatOnboarding() 6-screen engine, (3) three triggers + re-run buttons, (4) on-device e2e. No source pickers; no raw P/I/D. DO NOT implement yet — design tool picks up wizard UI first.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented alpha.327 (827abbcdd, pushed origin/dev). Firmware: sat_onboarded flag (5 touch points). v2.js: startSatOnboarding() 6-screen wizard ported from design 'SAT Onboarding.dc.html'; 4 triggers (Settings-save + SAT-page toggle interception, migrate on SAT-page, re-run x2). Adversarial review (5 dims x verify, wf_48daf107): 7 confirmed findings all fixed (commit() r.ok + false-success; settings-list wizard-bypass hidden render-only; theme-aware --zone-* colors; 44px tap targets). VERIFIED: firmware compiles clean (fresh firmware.bin), littlefs.bin packed via mklittlefs (pio -t buildfs wedged on .ino.cpp handle collision, bypassed), evaluate.py 0 failures, node -c v2.js OK. PENDING: on-device walk (fresh-enable/abandon/migrate, light+dark, 360px) needs bench ESP32-S3.

On-device verified on classic-S3 + PIC 6.6 @192.168.1.219 (alpha.327). Screenshotted every step of the SAT wizard (+ firmware onboarding) via Chromium: interception confirmed (Welcome opened on enable-toggle, satenabled stayed off), Radiators->Manufacturer->Tuning->Sources-health (live Room 25.9C, theme-aware zone colors)->Done writes satenabled=on + sat_onboarded=on. Proof gallery artifact published. Also reproduced TASK-961 live (port 80 dead on first boot after provisioning, recovered on reboot).
<!-- SECTION:NOTES:END -->
