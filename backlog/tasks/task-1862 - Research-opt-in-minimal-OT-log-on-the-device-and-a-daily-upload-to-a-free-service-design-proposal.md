---
id: TASK-1862
title: >-
  Research: opt-in minimal OT log on the device and a daily upload to a free
  service (design proposal)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T06:32:00Z'
updated_date: '2026-11-14T13:05:00Z'
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
<!-- SECTION:NOTES:END -->
