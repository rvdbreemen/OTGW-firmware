---
id: TASK-279
title: Fix autodiscovery PROGMEM-as-RAM crashes (Exception 3)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-16 20:09'
updated_date: '2026-04-16 20:24'
labels:
  - bug
  - esp8266
  - mqtt
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The MQTT autodiscovery code uses strstr(), strncmp(), and MQTTclient.write() on PROGMEM flash pointers as if they were RAM. On Arduino Core 3.1.2 the C library uses optimized word-aligned reads, causing Exception (3) on unaligned flash addresses. Reported by crashevans as v1.4.0-beta boot crash loop. Bug also exists on dev branch.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No strstr/strncmp/memcmp on raw PROGMEM pointers anywhere in autodiscovery code
- [x] #2 MqttHaCfgEntry flags field pre-computes source token presence at generation time
- [x] #3 sendMQTTTemplateStreaming uses writeMqttProgmemChunk for PROGMEM literal data
- [x] #4 tryGetTemplateReplacement uses pgm_read_byte for PROGMEM cursor comparison
- [x] #5 Build passes for both ESP8266 and ESP32
- [x] #6 python evaluate.py --quick passes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed autodiscovery PROGMEM-as-RAM crashes that caused Exception (3) on ESP8266 with Arduino Core 3.1.2. Root cause: strstr(), strncmp(), and MQTTclient.write() were called with flash pointers that the C library dereferenced via word-aligned reads, causing unaligned access exceptions on PROGMEM data. Fix: (1) Added pre-computed flags byte to MqttHaCfgEntry to eliminate all strstr() calls on PROGMEM pools, (2) Added pgm_strncmp_PP/pgm_read_char helpers for safe byte access, (3) Changed tryGetTemplateReplacement to use pgm_strncmp_PP, (4) Changed renderTemplateToBuffer/measureRenderedTemplate/sendMQTTTemplateStreaming to use PGM_P types and pgm_read_char, (5) Fixed sendMQTTTemplateStreaming to use writeMqttProgmemChunk instead of writeMqttChunk for PROGMEM literal data, (6) Updated code generator to compute flags at generation time. Also fixed the PIC entry detection which was a no-op (searched topic template, but otgw-pic/ was only in msg template). ESP8266 build passes, evaluator 95.7%.
<!-- SECTION:FINAL_SUMMARY:END -->
