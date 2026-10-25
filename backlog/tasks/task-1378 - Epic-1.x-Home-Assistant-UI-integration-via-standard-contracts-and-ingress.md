---
id: TASK-1378
title: 'Epic (1.x): Home Assistant UI integration via standard contracts and ingress'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T09:10:00Z'
updated_date: '2026-10-25T11:21:00Z'
labels:
  - epic
  - home-assistant
  - mqtt
  - webui
dependencies: []
priority: medium
ordinal: 252000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x counterpart of the dev epic TASK-1377 (dev backlog). Research: docs/research/2026-10-25-home-assistant-ui-integration.md (identical on dev and otgw-1.x.x).

Maintainer decision 2026-10-25: minimal integration, make HA ingress work properly now. MQTT discovery and REST stay the only contracts. No custom integration, no dashboard card, no HA Marketplace listing for now.

Children on this branch:
A. configuration_url (cu) in the MQTT discovery device block (mqtt_configuratie.cpp, kDev ~l.1948).
B. Bench spike: classic UI behind hass_ingress (HA over HTTP and HTTPS), first with rewrite rules only.
C. Base-path agnostic classic UI, visible degradation of the port-81 live log, proxy-safe CSRF (restAPI.ino:107, ADR-054), scoped by B.
D. Manual/README section.

Cross-branch coordination: the 2.0.0 siblings are TASK-1377.1 (A), .3 (B), .5 (C), .7 (D) in the dev backlog. B here reuses the ingress component choice from dev .3; the CSRF decision is taken once in the 2.0.0 ADR (amending ADR-056) and mirrored by a 1.x ADR (amending ADR-054). Ordering per line: A independent, C after B, D after A and C.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All child tasks are Done, or explicitly dropped with a reason
- [x] #2 HA shows a working 'Visit device' link for a 1.x OTGW, shown by a screenshot
- [x] #3 The classic UI opens from the HA sidebar through the chosen ingress component with HA over HTTPS; reads and admin actions work, live log shows its degraded state, shown by screenshots or a bench transcript
- [x] #4 MQTT discovery and REST contracts unchanged apart from the added cu field, shown by a discovery payload diff
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- 1378.1, .2, .4 Done. 1378.3: all ACs but #4 (ADR) met; #4 waits for the maintainer (no ADR-054 change was needed).
- Work in worktree wt-1378 from origin/otgw-1.x.x, not in wt-otgw-1.x.x: that tree holds 35 unpushed local commits and an uncommitted TEMP DIAG build (TASK-1262/1302); left untouched.
- Bench .88.68 now runs 1.7.8-beta.6+54cbe02 (fw + LittleFS); it ran the TASK-1302 no-GARP diag build before. MQTT broker setting restored to homeassistant.local, no HTTP password.
<!-- SECTION:NOTES:END -->
