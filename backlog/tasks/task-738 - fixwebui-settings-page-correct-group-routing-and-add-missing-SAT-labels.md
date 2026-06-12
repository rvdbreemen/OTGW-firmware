---
id: TASK-738
title: 'fix(webui): settings page - correct group routing and add missing SAT labels'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 23:38'
updated_date: '2026-05-29 18:33'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Settings page routes MQTT/NTP/SAT/GPIO/Webhook settings to Other group instead of their named sections. Root cause: SETTINGS_GROUPS prefixes are uppercase (MQTT, NTP, OTGW, etc.) but REST API returns lowercase keys (mqttenable, ntpenable, etc.). Case-sensitive indexOf match always fails. Additionally, translateFields is missing ~20 SAT entries added in recent tasks (satautotune, satautotunerate, satsensormaxage, saterrormon, satpresethome, sattempstep, satsimulation, satsimheatrate, satsimcoolrate, satautogains, satheatingmode, satcyclesperhour, satvalveoffset, satsolarfreezeint, satpvboost* series) and mqttuselegacyottopics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT settings appear in MQTT section, not Other
- [x] #2 NTP settings appear in NTP section, not Other
- [x] #3 SAT settings appear in SAT section, not Other
- [x] #4 All SAT settings show human-readable labels
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped across commits a302ed3d/e3eafba6/500904e3 (alpha.82-84), pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support. Verified in shipped data/index.js: getSettingsGroupId() is now case-insensitive (keyLower.indexOf(prefix.toLowerCase())) so MQTT/NTP/SAT/OTGW/GPIO/Webhook keys route to their named sections instead of Other; SAT group added to SETTINGS_GROUPS; 21 missing SAT translateFields labels + mqttuselegacyottopics added. All four ACs are code-verifiable and confirmed met; no field AC. Closed per maintainer rule: shipped+pushed and all code-verifiable ACs satisfied.
<!-- SECTION:FINAL_SUMMARY:END -->
