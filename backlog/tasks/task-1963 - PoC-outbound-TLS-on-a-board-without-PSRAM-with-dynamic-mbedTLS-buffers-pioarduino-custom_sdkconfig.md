---
id: TASK-1963
title: >-
  PoC: outbound TLS on a board without PSRAM with dynamic mbedTLS buffers
  (pioarduino custom_sdkconfig)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T09:35:00Z'
updated_date: '2026-11-15T09:38:00Z'
labels:
  - research
  - tls
  - needs-maintainer-ok
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 993000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15 (TASK-1862): the diagnostic stream must be secure from the device itself, with no proxy, also on boards without PSRAM. The prebuilt Arduino-ESP32 libraries allocate two ~16.7 KB TLS record buffers at mbedtls_ssl_setup(), which do not fit the 31.7 KB largest block on the no-PSRAM unit (ADR-218: -0x7F00 on 14/14). Espressif documents CONFIG_MBEDTLS_DYNAMIC_BUFFER with the dynamic free options at about 22 KB peak heap vs about 42 KB default (ESP-IDF mbedTLS docs, https_request example). This needs the IDF libraries rebuilt through pioarduino custom_sdkconfig. Proof of concept only: no feature, no merge into the release build until Robert decides. Related: ADR-218 (outbound TLS on PSRAM boards), TASK-1473 (on hold), TASK-1528 (no-PSRAM low-water 536 B), TASK-1938 (collection point).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Build: esp32-combo with custom_sdkconfig setting CONFIG_MBEDTLS_DYNAMIC_BUFFER and the dynamic free options builds through build.sh, together with include/sdkconfig.h (ADR-185) and LTO; build time and flash size recorded against the normal build
- [ ] #2 Bench on the no-PSRAM unit (bench-tester): 20 HTTPS POSTs of a 4 KB mix record to the collection point through the Cloudflare Tunnel succeed with certificate verification; peak heap during the handshake, largest free block and low-water recorded, also with the web UI open
- [ ] #3 No regression on that unit in a 1 h soak with uploads every 60 s (no reboot, loop gap within normal, MQTT and web UI working); capture transcript
- [ ] #4 Result and a recommendation (adopt, adopt with limits, or fall back to payload encryption) put to Robert
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Full proof of concept on branch research/cloud-recording (Robert 2026-11-15). Never merged or published as is.

1. Build: a separate env esp32-combo-tlsdyn that extends esp32-combo with custom_sdkconfig (CONFIG_MBEDTLS_DYNAMIC_BUFFER=y plus the dynamic free options). The normal esp32-combo stays untouched. Built here in the cloud container through build.sh; records build time, flash size, and that the option is in the generated sdkconfig. Risk: the hybrid compile (link bug reported for pioarduino 55.03.34; we are on 55.03.35).
2. Firmware module CloudRec (new CloudRec.ino + CloudRec.h, behind a HAS_CLOUD_REC flag set only in that env):
   - one static 4 KB record buffer (ADR-053, ADR-213 budget for boards without PSRAM)
   - OT section: C++ port of the range-coder model with bounded tables; frames taken in processOT()
   - debug section: address tokens taken in _debugBOL / _debugPrintf_P / the println path
   - SAT section: the SATTICK text when SAT debug output is on (PoC limit)
   - uploader: its own FreeRTOS task (webhook pattern, ADR-137), a mix record every 60 s or when the buffer is full, HTTPS POST with certificate verification against a few pinned roots, PocketBase password login and token refresh; fails quietly (TASK-1471)
   - config from /cloudrec.cfg on LittleFS (enabled, url, device email, password); off by default; telnet status line
   - prerelease word poc.N (never an alpha tag, never posted to testers)
3. Evidence in this container: host test that compiles the real C++ encoders and checks the Python decoders give back identical OT frames, SATTICK lines and debug bytes; evaluate.py --quick; build of esp32-combo and esp32-combo-tlsdyn.
4. Bench (bench-tester, not this session): flash the no-PSRAM unit, 20 uploads, heap peak and low-water, 1 h soak (AC 2, 3).
Out of scope for the PoC: web UI settings, consent dialog, SAT encoding without debug output, token persistence, ADR (an ADR follows only if Robert adopts it; it would amend ADR-003 and ADR-218).
<!-- SECTION:PLAN:END -->
