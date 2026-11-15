---
id: TASK-1963
title: >-
  PoC: outbound TLS on a board without PSRAM with dynamic mbedTLS buffers
  (pioarduino custom_sdkconfig)
status: To Do
assignee: []
created_date: '2026-11-15T09:35:00Z'
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
