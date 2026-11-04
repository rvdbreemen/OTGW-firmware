---
id: TASK-1158
title: Body-less REST requests can inherit the previous request's body
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 18:14'
updated_date: '2026-09-23 18:28'
labels:
  - rest
  - bug
dependencies: []
priority: high
ordinal: 290000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
g_requestBody is owned by request pointer only and never cleared. A request without a body that is allocated at the same heap address as the previous request passes the owner check and reads the stale body. Observed 2026-09-23: POST /api/v2/sat/enable/1 (no body) right after a settings POST with value false was parsed as false (log: newValue[0], SAT: disabled, HTTP 200); POST /api/v2/sat/externaltemp/18 got 400 the same way. Affects every route that reads the plain body.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A request whose Content-Length is 0 never sees a body captured for an earlier request
- [x] #2 POST /api/v2/sat/enable/1 without a body directly after a settings POST enables SAT on the bench
- [x] #3 Routes that do send a body keep working (settings POST, sensor-areas PATCH)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause: g_requestBody is owned by request pointer and never cleared; a body-less request allocated at the previous request's heap address passed the owner check (ABA). Pre-fix evidence, alpha.372 telnet 08:16:53-54: settings POST {"name":"satbleenable","value":false}, then POST /api/v2/sat/enable/1 without body logged 'field[SATenabled], newValue[0]' + 'SAT: disabled' with HTTP 200; the next POST /sat/externaltemp/18 got 400 (stale body 'false' is not numeric). Fix (alpha.373): hasBodyCompat()/bodyCompat() also require the request's own contentLength() > 0. Bench alpha.373: three runs of settings-POST(false) followed by body-less /sat/enable/1 all left satenabled=true; body-less /sat/externaltemp/18 returns ok; settings POST and sensor-areas PATCH with bodies still apply.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Body-less REST requests could read the previous request's body when the async server reused its heap address, so POST /api/v2/sat/enable/1 right after a settings POST acted on that POST's 'false'. hasBodyCompat()/bodyCompat() now also require the request's own Content-Length, verified on the OTGW32 bench.
<!-- SECTION:FINAL_SUMMARY:END -->
