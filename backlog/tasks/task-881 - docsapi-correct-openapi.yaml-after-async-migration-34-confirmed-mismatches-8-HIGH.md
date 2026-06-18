---
id: TASK-881
title: >-
  docs(api): correct openapi.yaml after async migration (34 confirmed
  mismatches, 8 HIGH)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-18 00:03'
updated_date: '2026-06-18 04:32'
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
- [x] #1 8 HIGH corrections applied to openapi.yaml
- [x] #2 openapi.yaml validates (yaml parse OK)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Applied 7 corrections so far (commit pending): removed phantom health.picavailable; device/info.otcommandinterface boolean->string enum[PIC,OT-Direct,None]; /v2/otdirect/mode GW=-map corrected (monitor=GW=0, bypass=GW=P, master=GW=S) + removed the application/json requestBody (handler reads argCompat/form only) + removed the phantom bypass-400 clause and the 404; /v2/discovery/republish added the 429 cooldown response; added the missing securitySchemes.basicAuth definition (was referenced 9x, undefined -> invalid OpenAPI). YAML validates. REMAINING (deferred, generate against live device output): the 3 big schema rewrites (SatStatus flat-schema, filesystem/files heterogeneous array, sat/status?detail=full SatHealth) + the medium/low doc-completeness gaps (per-op basicAuth on SAT/otdirect mutations, PUT aliases, missing fields ipaddress/version/flame_duty_pct/hysteresis-vent, 503 on pic/* when no PIC, etc.).

Applied the 3 big HIGH schema rewrites against live device output (alpha.204, 192.168.88.39): SatStatus -> flat 132-field object (was wrapped ~25-field, wrong types: boiler_status string/control_mode int/last_cycle_class int; room_temp nullable); added SatHealth (separate 22-field detail=full view, NOT a superset); /v2/sat/status 200 now oneOf[SatStatus,SatHealth] + field count 50->132; /v2/filesystem/files -> heterogeneous array (file/dir entries with type enum + trailing storage-summary object). YAML validates. All 8 HIGH now complete. gen_sat_props.py helper kept for field-drift regeneration.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Corrected openapi.yaml after the async + ArduinoJson migration. All 8 HIGH fixed (3 big schema rewrites generated against live device output: flat 132-field SatStatus, separate 22-field SatHealth for ?detail=full, heterogeneous filesystem listing; plus the 5 surgical HIGH from commit 99fb675f). High-value medium/low done + verified: basicAuth declared on all 40 v2 POST/PUT mutations (matches firmware H5 central auth gate), 503 PicUnavailable on all 4 pic routes (confirmed live), device/info capped with additionalProperties + board-conditional note + capability fields, anyOf for heterogeneous open-object arrays. YAML validates. Source-dive tail (SAT settings body fields, PUT-alias audit) split to TASK-882. Commits: 99fb675f, dbbf6fa4, bd6696d4.
<!-- SECTION:FINAL_SUMMARY:END -->
