---
id: TASK-1938
title: >-
  Collection server for diagnostic recordings: PocketBase on Robert's Synology
  behind a Cloudflare Tunnel
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T15:21:00Z'
updated_date: '2026-11-14T15:46:00Z'
labels:
  - diagnostics
  - tooling
  - infrastructure
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 992000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-14 (session): he has a domain on Cloudflare and asks for help to set up the self-hosted collection point from TASK-1862 (PocketBase on his Synology, reached through a Cloudflare Tunnel). Server side only: no firmware change; TASK-1473 (device streaming) stays on hold. Deliverables in tools/cloud-recorder/: container project for Synology Container Manager (PocketBase + cloudflared, pinned versions), schema migration (devices auth collection, batches collection, create-only rules, unique device+stream+seq), retention cron, a stdlib host test client that acts as a gateway, and a step-by-step setup guide. Only the ingestion paths are exposed through the tunnel; the admin UI stays on the LAN.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Container project, migration, retention hook and tunnel path rule written and reviewed against the PocketBase and Cloudflare docs
- [ ] #2 Host test client: a device authenticates, posts a batch, a resend of the same seq is refused, a read of batches and the admin path through the tunnel are refused (run on Robert's Synology, transcript in the notes)
- [ ] #3 Retention: records older than the configured days are deleted by the cron (evidence from the Synology run)
- [x] #4 Setup guide with every manual step on Synology and in the Cloudflare dashboard
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 written: tools/cloud-recorder/pocketbase/ (Dockerfile with checksum check, compose.yaml, migration, retention hook, test_client.py, README guide). Reviewed against pocketbase.io docs (js-migrations, js-database, RateLimitRule, TrustedProxyConfig, api-rules: locked rule = 403, failed createRule = 400). Not in the docs as a worked example: authRule, authToken, @request.auth.collectionName. Rate-limit label changed to the documented tag form batches:create. Self-review found and fixed: RETENTION_DAYS=0 fell back to 30 (parseInt || 30). node --check passes on both JS files; test_client sample payload = 916 B codec chunk (786 frames, 300 s). Could not run PocketBase here (binary download refused in this container). AC 2 and 3 need Robert's Synology run.

2026-11-14 local Docker test (Robert approved): image built from the shipped Dockerfile plus only the sandbox proxy CA (test context in scratchpad); release checksum OK (pocketbase_0.40.6_linux_amd64.zip: OK). Migration applied, no errors; read back: devices auth (authRule 'enabled = true', all CRUD rules locked, authToken 1209600), batches (createRule as written, other rules locked, fields device/stream/seq/t0/fmt/payload/created, unique index device+stream+seq, index on created), trustedProxy CF-Connecting-IP, rate limits batches:create 30/60s, auth-with-password 5/60s, /api/ 120/60s. test_client.py vs http://127.0.0.1:8090: 1 login 200, 2 post 200 (916 B codec chunk, 786 frames), 3 resend 400, 4 other device 400, 5 list 403, 6 wrong password 400 = PASS; 7 admin UI 200 locally = expected FAIL without the tunnel path rule. Extra: disabled device login 403, post without token 400. Retention via POST /api/crons/otgwBatchRetention: RETENTION_DAYS=30 deleted 1 batch backdated to 2026-10-05, kept today's (cutoff 2026-10-15 15:46:08Z); RETENTION_DAYS=0 deleted the rest (0 left). Fixed during the test: test_client crashed on a non-JSON 200 (admin HTML); README typo pbinstal -> pbinstall; README retention test now uses the cron API. AC 2 and 3 stay open by their wording (Synology run, incl. the tunnel).
<!-- SECTION:NOTES:END -->
