---
id: TASK-2905
title: >-
  Cloud recorder: SATTICK always recorded, TLS session resumption, second send
  slot on PSRAM boards, bench job
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T14:48:00Z'
updated_date: '2026-11-15T15:13:00Z'
labels:
  - cloud-recording
dependencies: []
priority: low
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15 answers: 1 A (SATTICK always to the recorder while it records, not to telnet; SAT behaviour unchanged), 2 C (accept that the Cloudflare edge can read the payload; no device-side encryption), 3 A (TLS session resumption), 4 A (a second send slot on PSRAM boards only; without PSRAM the loss stays reported in SEC_LOST), 5 B (rate limit per IP stays), 6 yes (write the bench job), 7 no (PocketBase not on the Synology yet: the migration may stay edited in place). Research branch only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SATTICK reaches the recorder with SAT debug off, and goes to telnet only with SAT debug on (no double record); build compiles
- [ ] #2 Uploads use esp-tls with client session tickets; a failed resumption falls back to a full handshake; build compiles
- [x] #3 PSRAM boards queue two records; boards without PSRAM keep one slot; host-testable part tested
- [x] #4 Bench job written for the bench-tester with every on-device check of TASK-2901, 1971, 1972 and this task
- [x] #5 Decision 2 C recorded in the docs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 evidence. (1) SATTICK: SATTickTf in SATcontrol.ino (both SATTICK sites): SAT debug on -> DebugTf as before (recorded from telnet once), off -> cloudRecSatTickf() (recorder only, same vsnprintf text, format matched against crec::SAT_FORMATS); normal builds get a no-op stub, single macro definition (the Arduino prototype pass reads both #if branches and warned on a two-branch define). (2) TLS resumption: CloudRecTls (Stream over esp-tls, cfg.client_session, ticket kept after any HTTP answer, a failed resumed connect retried once in full; counters 'TLS resumed n full m' in the upload line); CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS=y in the env. Found on the way: the pioarduino hybrid compile never copied mbedTLS's nested archives, so poc.1-poc.4 linked the prebuilt Arduino mbedTLS (no dynamic buffers, no tickets) and esp-tls with tickets failed to link (esp_mbedtls_dynamic_set_rx_buf_static). scripts/tlsdyn_mbedtls_libs.py (pre: extra script, checkprogsize post action before pioarduino's copy) copies them; the poc.5 ELF now has esp_mbedtls_free_buf, esp_mbedtls_reset_free_rx_buffer, esp_mbedtls_dynamic_set_rx_buf_static, esp_tls_get_client_session, mbedtls_ssl_get_session. A new custom_sdkconfig line needed sdkconfig.esp32-combo-tlsdyn deleted first. (3) Send queue: crec::Slot/slotFree/slotOldest in CloudRecPolicy.h, slot 1 from platformPsramAllocOnce; tests/test_cloudrec_policy.py queue test (1 slot: 4 lost, oldest 13; 2 slots: 3 lost, oldest 11) passes, a mutant that sends the newest first fails. (4) Bench job: docs/diagnostics/cloudrec-bench-job.md (13 checks, quick tunnel for a Cloudflare certificate). (5) Decision 2 C in cloud-telemetry-completeness.md and the PocketBase README. Build esp32-combo-tlsdyn 2.0.0-poc.5 exit 0: RAM 155,820 B, app 2,250,611 B (98.1 %, 42 KB left: the rebuilt mbedTLS is 21 KB smaller). evaluate --quick 0 FAIL 0 WARN; codec, policy, lz tests OK. AC 1 and 2 need the bench (checks 6, 8, 9).
<!-- SECTION:NOTES:END -->
