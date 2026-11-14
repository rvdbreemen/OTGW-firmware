---
id: TASK-1938
title: >-
  Collection server for diagnostic recordings: PocketBase on Robert's Synology
  behind a Cloudflare Tunnel
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T15:21:00Z'
updated_date: '2026-11-14T17:24:00Z'
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
- [x] #5 Host export tool: fetches a device's batches from PocketBase, decodes the OT stream and writes an otframes.log in the ADR-209 replay layout; a round trip (generated stream -> batches -> PocketBase -> export) gives identical frames and ms times, gaps in seq are marked
- [x] #6 Debug stream sized: bytes per line for text, tokenized and compressed forms measured on lines built from the firmware's real format strings; the batches schema and limits fit the result; a sustained debug upload to the local PocketBase is accepted and the rate limiter refuses a burst
- [x] #7 Decoders for all three streams: a generated recording (OT, both SATTICK formats, debug lines from call sites resolved in a real firmware.bin) goes through mix records, PocketBase and decode_all.py and comes back identical (OT frames + ms, SATTICK text, debug telnet bytes vs C snprintf), for the no-PSRAM and PSRAM profiles; a one-bit corruption makes it fail
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 written: tools/cloud-recorder/pocketbase/ (Dockerfile with checksum check, compose.yaml, migration, retention hook, test_client.py, README guide). Reviewed against pocketbase.io docs (js-migrations, js-database, RateLimitRule, TrustedProxyConfig, api-rules: locked rule = 403, failed createRule = 400). Not in the docs as a worked example: authRule, authToken, @request.auth.collectionName. Rate-limit label changed to the documented tag form batches:create. Self-review found and fixed: RETENTION_DAYS=0 fell back to 30 (parseInt || 30). node --check passes on both JS files; test_client sample payload = 916 B codec chunk (786 frames, 300 s). Could not run PocketBase here (binary download refused in this container). AC 2 and 3 need Robert's Synology run.

2026-11-14 local Docker test (Robert approved): image built from the shipped Dockerfile plus only the sandbox proxy CA (test context in scratchpad); release checksum OK (pocketbase_0.40.6_linux_amd64.zip: OK). Migration applied, no errors; read back: devices auth (authRule 'enabled = true', all CRUD rules locked, authToken 1209600), batches (createRule as written, other rules locked, fields device/stream/seq/t0/fmt/payload/created, unique index device+stream+seq, index on created), trustedProxy CF-Connecting-IP, rate limits batches:create 30/60s, auth-with-password 5/60s, /api/ 120/60s. test_client.py vs http://127.0.0.1:8090: 1 login 200, 2 post 200 (916 B codec chunk, 786 frames), 3 resend 400, 4 other device 400, 5 list 403, 6 wrong password 400 = PASS; 7 admin UI 200 locally = expected FAIL without the tunnel path rule. Extra: disabled device login 403, post without token 400. Retention via POST /api/crons/otgwBatchRetention: RETENTION_DAYS=30 deleted 1 batch backdated to 2026-10-05, kept today's (cutoff 2026-10-15 15:46:08Z); RETENTION_DAYS=0 deleted the rest (0 left). Fixed during the test: test_client crashed on a non-JSON 200 (admin HTML); README typo pbinstal -> pbinstall; README retention test now uses the cron API. AC 2 and 3 stay open by their wording (Synology run, incl. the tunnel).

2026-11-14 export tool and debug sizing (branch research/cloud-recording). Round trip: simulate_gateway.py 30 min, 300 s batches, batch 3 skipped -> 5 batches, 3,920 frames, 4,683 B (1.19 B/frame); export_batches.py -> 3,920 frames identical in ms time, letter and frame, one '# gap: batches 3..3 missing' line. Real replay code (tests/test_replay_timing.py harness from OTGW-Core.ino) on the first 400 exported lines: 400/400 dispatched as bare frames, mode=timed, 0 ms deviation. Debug sizing (dbg_size_study.py, 985 real format strings, generated values): at ~3 KB/s text 104 B/line, text+zlib 36, address token 16, token+zlib 12 (0.9 GB per gateway-month); largest 60 s token+zlib batch 21.7 KB -> payload max raised 16384 -> 65536 chars. Fresh-database run: test_client 1-6 PASS (7 needs the tunnel); 25 paced 13.3 KB dbg batches 200; burst of 40 -> 30x200, 10x429; 65,600 chars 400, 65,536 chars 200.

2026-11-14 batch size measured (batch_size_study.py, 1 h of data per variant, fresh local PocketBase, keep-alive): OT per line (50/batch request) 189 req, 1,750 KB sent, 1,634 KB db, 24.7 ms/req; OT 10 s 193 KB / 98 KB; 60 s 49 / 29 KB; 300 s 20.6 KB sent, 3.3 KB received, 16 KB db; 900 s 14.9 / 16 KB. Debug 10 s 1,977 / 1,933 KB; 30 s 1,773 / 1,868; 60 s 1,725 / 1,745; 120 s 1,696 / 1,720. Found: the create reply echoed the whole payload (received = sent); ?fields=id cuts it to about 275 B per request. Chosen: OT and SAT 300 s, debug 60 s, early flush at 48 KiB (otrec.BATCH_SECONDS, MAX_PAYLOAD_BYTES, CREATE_PATH); 84 requests/h per gateway vs 30/min limit; backlog resend paced at <=25/min. Regression after the change: test_client 1-6 PASS (batch now in the real otrec fmt 1, 786 frames decode), round trip 60 min at 300 s: 8,626 frames identical, one gap note.

2026-11-14 Robert: one stream with all three on fixed intervals, also for boards without PSRAM. Built mix records (fmt 4, stream 'mix': OT/SAT/debug sections in one record per interval; otrec.pack_mix/unpack_mix; schema select gains 'mix'; export_batches.py --stream mix). Found and fixed: the OT section decoded every frame at the record t0 (codec assumes the first frame at t0); the section now starts with the offset of its first frame. mix_study.py, 1 h per profile, fresh local PocketBase: PSRAM 60 s full debug zlib: 60 rec/h, 0 early, max 22.1 KB, 1.75 MB sent, 1.78 MB db; no PSRAM 60 s 4 KB buffer debug 0.3 KB/s: 60 rec/h, 0 early, max 3.8 KB, 301 KB sent, 291 KB db; no PSRAM full debug: 480 rec/h, 420 early, max 4.1 KB, 2.49 MB sent; OT decoded identical in all three. End to end: 59 mix records posted (one left out), export --stream mix: 9,253 frames identical, one gap line. Boards without PSRAM: no TLS (ADR-218), so plain HTTP to the NAS on the LAN only.

2026-11-14 decoders for all streams + proof (proof_all_streams.py). Image: CI artifact firmware.bin of research/cloud-recording 48a0eb2 (esp32-combo, digest sha256:59d8f3d8528ba433cfc55518dc0a76221dd5a83bd8fe724a55cbb93ada1f0f7c, only firmware.bin, no ELF). 791/854 BOL format strings and 801 function names found as whole C strings in DROM; both SATTICK formats found (0x3c1bee86, 0x3c1be7bc). Reference text by ref_printf.c (C snprintf per conversion). no-psram 30 min: 4,706 frames, 48 ticks, 5,554 debug lines (580,910 B) -> 51 mix records, largest 4,062 B, 128,953 B; decoded: PASS OT, PASS SATTICK, PASS debug bytes. psram 30 min: 53,857 debug lines (5,596,803 B) -> 30 records, largest 19,528 B, 568,926 B; PASS all three. Negative control: one flipped bit in a debug section -> debug identical False.

2026-11-14 CodeQL on PR #701 (alerts 71-75, uncontrolled format string, ref_printf.c:54-58): real. The conversion char came unchecked from the input, the width was unbounded (memcpy into spec[64]) and a trailing '%' ran past the string. Old binary: 'a%n' abort (glibc '%n in writable segment'), 'abc%' SIGSEGV; fixed: spec rebuilt from a whitelist (flags, <=3 width, <=2 precision digits, conversion in diuxXcfsS), argument type must fit, output bounded; all three refused with exit 1. Proof rerun with the fixed renderer (no-psram, 30 min): PASS OT, PASS SATTICK, PASS debug bytes. The taint path fgets->snprintf stays by design (the tool renders formats it is given); left to Robert whether to dismiss the alerts.

2026-11-14 Robert: the CodeQL alerts may go. No dismiss tool in this session, so the cause is removed: ref_printf.c now only passes fixed format literals to snprintf (flag '', '0', '-', '+' x conversion table, width/precision as '*' arguments; integers width only). Builds clean with -Wall -Wextra -Wformat=2 -Wformat-nonliteral; hostile specs (%n, long width, trailing %, space flag, type mismatch) refused. Proof rerun: no-psram and psram, PASS OT, PASS SATTICK, PASS debug bytes (same counts as before).
<!-- SECTION:NOTES:END -->
