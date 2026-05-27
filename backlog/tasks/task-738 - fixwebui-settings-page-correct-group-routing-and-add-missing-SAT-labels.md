---
id: TASK-738
title: 'fix(webui): settings page - correct group routing and add missing SAT labels'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-27 23:38'
updated_date: '2026-05-27 23:38'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Settings page routes MQTT/NTP/SAT/GPIO/Webhook settings to Other group instead of their named sections. Root cause: SETTINGS_GROUPS prefixes are uppercase (MQTT, NTP, OTGW, etc.) but REST API returns lowercase keys (mqttenable, ntpenable, etc.). Case-sensitive indexOf match always fails. Additionally, translateFields is missing ~20 SAT entries added in recent tasks (satautotune, satautotunerate, satsensormaxage, saterrormon, satpresethome, sattempstep, satsimulation, satsimheatrate, satsimcoolrate, satautogains, satheatingmode, satcyclesperhour, satvalveoffset, satsolarfreezeint, satpvboost* series) and mqttuselegacyottopics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT settings appear in MQTT section, not Other
- [ ] #2 NTP settings appear in NTP section, not Other
- [ ] #3 SAT settings appear in SAT section, not Other
- [ ] #4 All SAT settings show human-readable labels
<!-- AC:END -->
