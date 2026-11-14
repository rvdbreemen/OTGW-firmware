---
id: TASK-1862
title: >-
  Research: opt-in minimal OT log on the device and a daily upload to a free
  service (design proposal)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T06:32:00Z'
updated_date: '2026-11-14T15:27:00Z'
labels:
  - diagnostics
  - research
  - needs-maintainer-ok
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 938000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, in session 2026-11-12: research and a design proposal for an opt-in minimal OpenTherm log on the device (date + high-resolution timestamp, the OT frame, the most essential SAT events), its size for 24 h and 7 days, and a simple way to upload it once a day to a free service (Cloudflare or similar) where the data is collected. Opt-in, switchable during onboarding or in the settings. His words: 'Maak hier een aparte feature van en geef deze in onderzoek en kom met een ontwerpvoorstel naar de maintainer toe.'

Related: TASK-1473 (parked cloud recording, 2.1.0), ADR-213 (local recording; decision 1: the firmware never sends a recording anywhere), ADR-218 (outbound TLS on PSRAM boards, Proposed, parked), ADR-003. Proposal: docs/diagnostics/minimal-ot-log-and-upload-proposal.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sizes for 24 h and 7 days measured on a real capture and cross-checked against a second source, per frame profile and time resolution
- [x] #2 Storage options (LittleFS, PSRAM ring) with budget, reboot/filesystem-update survival, RAM and wear compared
- [x] #3 Upload options compared (service limits, transport against the no-HTTPS rule, privacy), conflicts with ADR-213, TASK-1473 and ADR-218 named
- [ ] #4 Design proposal written and the open decisions put to Robert one per DM (via the coordinator)
- [ ] #5 Robert's decisions recorded in this task; follow-up implementation task(s) and any ADR filed from them
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Measurements (2026-11-12, a 20 h tester capture kept outside the repo, 178,607 frames, 8,872/h, device_time deltas): ADR-213 layout B every frame ms 848 KiB/24 h, 5.80 MiB/7 d; us 1,111 KiB, 7.60 MiB; changes-only ms 190 KiB, 1.30 MiB; us 236 KiB, 1.61 MiB. Host zlib per 4 KiB block (optimistic bound): every frame ms 479 KiB/day, changes-only 114 KiB/day. Text log line per frame 4,938 KiB/day. SATTICK (1,920 ticks, 95/h, 30 fields): delta-coded 38 KiB/day, int16 139 KiB/day, text 693 KiB/day. Cross-check: recording-budget-1470.md field run 881 KiB/day OT frames (4 % apart), 307 KiB/day changes-only.

Storage: LittleFS 768 KiB, beta.197 image about 110/192 blocks non-erased (estimate), FS_RUNTIME_RESERVE_KIB 96 (build.py:823) -> about 230 KiB unreserved; a filesystem write wipes the log (TASK-1524); no-PSRAM low-water 536 B (TASK-1528). PSRAM ring 1.5 MB (ADR-213) holds about 40 h of every frame + SAT. Upload: Cloudflare free Workers 100k req/day, 1k/min, 10 ms CPU; R2 10 GB-month, 1M class A, 10M class B (developers.cloudflare.com, 2026-11-12). http://workers.dev answers 301 to HTTPS (curl, 2026-11-12); plain HTTP to a Worker on a custom domain unverified; R2 payment method and EU jurisdiction on the free plan unverified.

Conflicts named in the proposal: ADR-213 decision 1, TASK-1473 (parked), the no-HTTPS rule / ADR-218 (PSRAM only, parked), GDPR (presence and heating habits; consent, random device id, retention, deletion, EU).

Proposal committed locally (0632d889d, lane wt-95rt, not pushed: beta.203 release freeze). The four open decisions went to the coordinator for Robert's DM, one per message, 2026-11-12: (1) log content and storage, rec. C = every frame in the PSRAM ring on PSRAM boards + changes-only hourly LittleFS files elsewhere; (2) transport, rec. C = LAN relay first, ADR-218 HTTPS later, not plain HTTP; (3) daily vs hourly, rec. hourly; (4) relation to TASK-1473/ADR-213, rec. keep 1862 separate, phase 1 local only, upload behind a new ADR amending ADR-213 decision 1. AC4 waits for the DMs to be sent; AC5 for the answers.

Robert's research brief via the coordinator (2026-11-12, verbatim): 'Voor het feature om data op te slaan in van OT-logs, onderzoek wat hier een goede optie is, bv via REST of MQTT naar cloudflare of google, in elk geval naar een dienst die schaalbaar is, en gratis. bij voorkeur geen SSL nodig heeft, maar als het nodig is, dan wil ik dat weten. Het doel is om datacaptures op te slaan van gebruikers die in beta mode meedoen en opt-in hebben op het delen van data te verbetering van de firmware.' Done in commit 5cf1e4269: comparison Cloudflare / Google / cloud MQTT / NAS on primary pages; curl verified that Workers on *.workers.dev take plain HTTP (200, no HSTS: demo.turnstile, demo-endpoints.pydantic, remix-cloudflare-demo.jacob-ebey), bare workers.dev 301, script.google.com 301 to HTTPS; Google free tier needs a billing account and stores only in US regions; EMQX 1 GB/month, HiveMQ Serverless retires 2026-12-31. Correction: my first proposal said workers.dev forces HTTPS; wrong, only the apex redirects. Q2 was resent to the coordinator as a replacement (A plain HTTP + HMAC, B + payload encryption, C ADR-218 HTTPS, D LAN relay; rec. B). Not verified: R2 payment method and EU jurisdiction on a free account, POST over plain HTTP, ToS/DPA terms, EMQX ports, payload-encryption cost.

Robert answered Q2 by DM (via coordinator, msgs 1570328825353609217 and 1570329291030270033, verbatim in the proposal): none of the batch-upload options; stream every log line to a cloud service, encrypted, HTTPS allowed. Streaming research in commit 14cb58c72: 2.46 frames/s per gateway makes a message per frame exhaust every free tier; lines keep their own timestamps and go in 10 s batches over one TLS connection. Compared (primary pricing pages, 2026-11-12): Cloudflare Worker+Durable Object+R2 (~230 gw), Grafana Cloud Loki (~300 gw, 14 d), InfluxDB Cloud (~135 gw, 30 d), AWS IoT Core (12 months free), Azure IoT Hub Free, Better Stack, EMQX Serverless, Google Pub/Sub (pricing page unreadable today). Streaming needs TLS: PSRAM boards only (ADR-218 PoC). Questions S1 (granularity, rec. 10 s batches), S2 (service, rec. Cloudflare DO+R2 EU, Loki fallback), S3 (non-PSRAM boards + ADR-218, rec. accept ADR-218, measure app-slot fit first) sent to the coordinator. Not verified: Grafana Free EU region choice and byte counting, DO duration under hibernation, app-slot fit of TLS + WebSocket client, DPAs.

ADR numbering (coordinator, 2026-11-12): ADR-231 is reserved for d3 (TASK-1817). Any ADR this task needs (accepting ADR-218 for streaming, amending ADR-213 decision 1) asks the coordinator for the next number first (then ADR-232).

Requirements from Robert in #dev-sat-mqtt (2026-11-12, relayed by the coordinator, recorded by otgw-firmware-95): (R1) EU hosting: 'I need to look at something that lands into a European server' (message 1570518857121271941). (R2) Explicit opt-in dialog: replying to Sergeant D's 'We need their permission first' (1570519191797112963), Robert: 'Yep it would be an opt in dialog' (1570520075755323554). Consequence for the ranking: only services that store the data in an EU region stay in; candidates without an EU data location drop out. Re-rank on that basis before the next proposal; the opt-in dialog becomes part of the onboarding/settings design, off by default.

2026-11-14 Robert (session): encode the stream for minimal bytes and maximal information, minimal storage, streamed to a cloud service. Study in docs/diagnostics/ot-stream-encoding-study.md, prototype scripts/ot_stream_codec_study.py (lossless encoder+decoder, round trip verified). Generated 24 h stream calibrated against the real capture (layout B 831 vs 848 KiB/day measured, layout B+zlib 425 vs 479): OT-aware model + adaptive range coder 0.95 B/frame (209 KiB/day, 3600 s chunks), 1.10 B/frame per 600 s chunk, vs layout B 3.79 and layout B+zlib 1.94. Time is about 5 of 7 bits per frame; 4 ms resolution 0.68 B/frame, 10 ms 0.56 (continuous model). Wire overhead dominates: 10 s batches 77 % headers (WS+TLS1.3+TCP/IP), 60 s 37 %, 300 s 10 %; recommendation 60 s batches on one connection, stored chunks of 10-60 min. Model state about 8.7 KB for 34 IDs. Debug text: tokenized logging candidate pw_tokenizer (Apache-2.0), Trice licence unverified. Not verified: the real capture (generated stream only), real-ID model RAM, C port.

2026-11-14 cost per gateway per month (Cloudflare DO + R2 pricing pages): hibernatable WS DO, 1 SQLite row per batch, hourly R2 flush; 60 s batches $0.050 marginal, 300 s $0.014; free plan binds on SQLite rows written (100k/day): about 67 gateways at 60 s, 297 at 300 s (earlier 1,380 from requests alone was wrong, corrected in the study); Workers Paid $5 minimum covers about 340 / 840 gateways. Handler times estimated, not measured.

2026-11-14 Robert: must be free, never pay. Zero-cost guarantee = no payment method on the account, so limits become hard errors. Options checked (primary pages 2026-11-14): (1) Cloudflare Workers Free + Durable Objects SQLite, no R2 (R2 reportedly asks for a payment method): no card, 'If you exceed any one of the free tier limits, further operations of that type will fail with an error' (DO pricing page); 5 GB SQLite; binding limit rows written 100k/day: about 67 gateways at 60 s batches, 297 at 300 s; buffering batches in the WebSocket attachment (2,048 B, third-party source) before one row write could raise that, unverified; DO EU jurisdiction on Free unverified. (2) Firebase Realtime Database Spark: 'No payment method needed', 100 simultaneous connections, 1 GB stored, 360 MB/day download; europe-west1 exists, Spark eligibility not stated; no Cloud Functions on Spark, so retention cleanup must run elsewhere. (3) Firestore Spark: 20K writes/day, 1 GiB. (4) Grafana Cloud Free: no card, 50 GB/month, 14 d, EU choice on Free unverified. (5) Supabase Free: 500 MB, paused after 1 week inactivity, EU region not stated. Recommendation: Cloudflare Free without R2, 300 s batches, firmware drops quietly on errors (TASK-1471).

2026-11-14 Firebase Realtime Database cost (firebase.google.com/pricing and /docs/database/usage/billing): Spark no payment method, 1 GB stored, 360 MB/day download, 100 simultaneous connections; Blaze $5/GB-month stored, $1/GB downloaded, budget alerts do not cap. Writes are not billed as such, but all outbound traffic is, incl. about 3.5 KB TLS handshake per connection and tens of bytes per TLS record. Estimate per gateway-month (binary batch base64 in JSON, +40 B key): 300 s batches 8.5 MB stored, 1 MB download with keep-alive / 31 MB with a new TLS connection per batch; 60 s 9.9 MB, 5 / 156 MB. Spark capacity at 30-day retention bound by storage: about 117 gateways (300 s), 100 (60 s); download with new TLS per batch: 319 / 63. Blaze beyond free: about $0.07 per gateway-month (300 s). REST with ?print=silent keeps responses empty. No Cloud Functions on Spark: retention cleanup from a host script. Unverified: Spark behaviour at the limit, europe-west1 on Spark, auth token flow on the device.

2026-11-14 Google costs above the free tier. Firestore Standard europe-west1 (cloud.google.com/firestore/pricing, parsed 2026-11-14): writes $0.099/100k (free 20k/day), reads $0.033/100k (50k/day), deletes and TTL deletes $0.011/100k (TTL no free quota), storage $0.000226027 per GiB-hour = about $0.165/GiB-month (1 GiB free); eur3 writes $0.18/100k. No monthly minimum. One document per batch, TTL for 30-day retention. Monthly totals, 300 s batches: 100 gw $0.36, 300 gw $2.53, 1000 gw $10.20; 60 s: 100 gw $4.22, 300 gw $14.17, 1000 gw $49.02. RTDB Blaze ($5/GB stored, $1/GB down, new TLS per batch): 300 s 300 gw $7.80, 1000 gw $58.96. Blaze has no hard cap (budget alerts do not cap); a hard stop needs the budget-to-Pub/Sub-to-function pattern that disables billing. Firestore is the cheaper Google route at scale; RTDB storage price dominates.

2026-11-14 self-hosted Firebase-like on Robert's Synology (Container Manager). PocketBase: MIT, single Go binary with SQLite, REST, auth, realtime, admin UI; pocketbase.io shows v0.42.2; the GitHub repo was archived 2026-10-26 (last GitHub release v0.40.6), development moved to ganigeorgiev.com; pre-1.0, no backward-compatibility guarantee. Retention via cronAdd in pb_hooks (JS, ES5.1+). Supabase self-hosted: 4 GB RAM min, 8 GB recommended, 2 cores, 40 GB SSD, about 13 components: too heavy for most Synology models. Appwrite not checked. Internet exposure for testers' gateways: Synology reverse proxy + Let's Encrypt with port 443 forwarded, or Cloudflare Tunnel (free plan, outbound only, needs a domain on Cloudflare DNS; one source says the Zero Trust dashboard asks for a payment method, unverified). Robert's own gateways on the LAN could post over plain HTTP (no TLS, also no-PSRAM boards). Recommendation: PocketBase in a container, one collection for batches, per-device auth records with create-only rules, nightly retention cron, Synology Hyper Backup for the SQLite file.

2026-11-14 pushes to origin/dev from the cloud session today (before the coordinator's push rules): 3262a496c..3c30e3926 (8 commits, 13:05-15:25 UTC; docs/diagnostics, scripts/ot_stream_codec_study.py, tools/cloud-recorder/, backlog task-1862/1473/1938, .wolf). From now on: research on branch research/cloud-recording, local commits, at most one batch push per hour under the coordinator's rules.
<!-- SECTION:NOTES:END -->
