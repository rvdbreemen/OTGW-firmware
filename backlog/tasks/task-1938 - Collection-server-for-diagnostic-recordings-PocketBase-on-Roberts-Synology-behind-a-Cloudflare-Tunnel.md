---
id: TASK-1938
title: >-
  Collection server for diagnostic recordings: PocketBase on Robert's Synology
  behind a Cloudflare Tunnel
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T15:21:00Z'
updated_date: '2026-11-14T15:25:00Z'
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
<!-- SECTION:NOTES:END -->
