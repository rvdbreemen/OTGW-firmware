---
id: TASK-881
title: >-
  docs(api): correct openapi.yaml after async migration (34 confirmed
  mismatches, 8 HIGH)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-18 00:03'
labels: []
dependencies: []
ordinal: 97000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Adversarial OpenAPI-vs-implementation audit (workflow wf_9f4d5d4d-af0, 43 agents) found 34 confirmed mismatches between docs/api/openapi.yaml and the actual async REST API. Structurally sound on paths/verbs; materially drifted on response bodies + auth. The ONLY firmware bug (POST/PUT body-capture) is already fixed (TASK-880). The rest are doc corrections. HIGH: /v2/sat/status SatStatus schema entirely wrong (flat-not-wrapped, types, field names vs satSendStatusJSON SATcontrol.ino:2026); /v2/otdirect/mode GW=-map inverted (gateway=GW=1,monitor=GW=0,bypass=GW=P,master=GW=S,loopback=GW=L per restAPI.ino:414); /v2/otdirect/mode phantom 404 + bypass-400 (bypass is valid 200); /v2/otdirect/mode requestBody should be x-www-form-urlencoded not JSON; /v2/discovery/republish missing 429 rate-limit; /v2/device/info otcommandinterface boolean->string enum[PIC,OT-Direct,None]; /v2/filesystem/files heterogeneous-array model; /v2/sat/status?detail=full needs separate SatHealth schema. MEDIUM/LOW: missing basicAuth security scheme (absent file-wide) + per-op security on SAT/otdirect mutations, missing PUT aliases, missing fields (ipaddress, version, flame_duty_pct, hysteresis/vent settings), 503 on /v2/pic/* when no PIC, etc. Full list in workflow wf_9f4d5d4d-af0 output.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 8 HIGH corrections applied to openapi.yaml
- [ ] #2 openapi.yaml validates (yaml parse OK)
<!-- AC:END -->
