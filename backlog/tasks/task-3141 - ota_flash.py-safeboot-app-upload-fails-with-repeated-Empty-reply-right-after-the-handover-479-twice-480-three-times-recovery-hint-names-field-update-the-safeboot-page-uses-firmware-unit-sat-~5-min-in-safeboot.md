---
id: TASK-3141
title: >-
  ota_flash.py: safeboot app upload fails with repeated 'Empty reply' right
  after the handover (479 twice, 480 three times); recovery hint names field
  'update', the safeboot page uses 'firmware'; unit sat ~5 min in safeboot
status: To Do
assignee: []
created_date: '2026-11-27T21:07:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1189000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on .63 (Classic + S3, BLE on) on 2026-11-27 (otgw-bench-tester). ota_flash.py hands over to safeboot (POST /safeboot) and then POSTs the app to /update: beta.479 try 1 and 2 = 'HTTP 000 curl: (52) Empty reply from server', try 3 OK; beta.480 (2274336 B) 3 of 3 tries 'Empty reply' => 'app upload to safeboot failed; the unit stays in safeboot'. Two manual curl uploads (field name 'update' as in the script's recovery hint, then 'firmware') also returned HTTP 000 within ~10 s; a third curl with -v (field 'firmware', Origin header set) completed: HTTP/1.1 200 'Update OK. Restarting into the new firmware...' (2274600 B in ~15 s) and the unit came back on 480. The unit sat in safeboot about 5 min (no ESP firmware running). Same class as B2/TASK-3032 (bd719cf image rejected twice by safeboot with 'Empty reply'). The recovery rule printed by ota_flash.py uses form field 'update' while the safeboot page JS uses 'firmware' (FormData.append('firmware', file)). It hits every tester who updates over OTA; not fixed tonight (coordinator). Evidence: OTGW-field-data/bench-63/2026-11-28_telemetry/run480/ota480.log (script run 21:45), ota480_app2.log (script refuses: unit is in safeboot), safeboot page source quoted above, NAS runs/telem63_480/telnet.raw (handover lines 21:45:41). Open questions: why does the first upload after the handover get an empty reply (safeboot HTTP server not ready? upload before the safeboot WiFi/stack settles? Expect-100-continue handling? image size?), is a wait or a retry-with-backoff or a different field name needed, and should the script print the correct recovery curl.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: a host or bench test shows the failing sequence (handover, immediate upload) and the cause, with the safeboot log or a capture of the failing request (what the server does with the first request)
- [ ] #2 ota_flash.py waits for safeboot to answer (GET / contains 'OTGW safeboot') and retries the upload with backoff; the manual recovery hint uses the field name the safeboot page uses ('firmware') and includes the Origin header
- [ ] #3 Bench (otgw-bench-tester): 5 consecutive OTA flashes of the combo image on a BLE-on unit and a no-PSRAM unit complete without 'Empty reply' fallback or the unit staying in safeboot > 60 s
<!-- AC:END -->
