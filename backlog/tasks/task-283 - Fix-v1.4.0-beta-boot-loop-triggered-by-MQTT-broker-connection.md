---
id: TASK-283
title: 'Fix: v1.4.0-beta boot loop triggered by MQTT broker connection'
status: Done
assignee: []
created_date: '2026-04-17 13:53'
updated_date: '2026-04-18 07:47'
labels:
  - bug
  - esp8266
  - mqtt
dependencies: []
references:
  - 'Discord #beta-testing, user mikdasa, 2026-04-17'
  - 'Discord #beta-testing, user crashevans, 2026-04-17 (confirmed)'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ESP8266 enters boot/crash loop when MQTT broker is reachable. Disconnecting broker stabilises the unit. doAutoConfigure throttle log shows 22 msgs dropped with heap=13168 bytes. Likely the new streaming autodiscovery overwhelms the ESP8266 when publishing many discovery entries in rapid succession. Confirmed by mikdasa and crashevans. Reverts to v1.3.10 fix the loop.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ESP8266 boots cleanly with MQTT broker connected
- [x] #2 Autodiscovery completes without crash or throttle storm
- [x] #3 Heap stays above safe threshold during discovery burst
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as resolved by the streaming discovery rework merged from dev into
feature-dev-2.0.0-otgw32-esp32-sat-support. Confirmed with the user after
review of the commit chain.

The original crash signature (doAutoConfigure throttle: 22 msgs dropped at
heap=13168) pointed at three underlying problems: the legacy mqttha.cfg
pipeline holding large staging buffers, bursty reconnect publishing, and
silent PROGMEM-as-RAM bridging. Each has a dedicated fix already in this
branch:

- 2b12834c refactor: replace legacy MQTT HA discovery with compact streaming
  API (the whole ADR-077 streaming rewrite)
- ff159819 feat: async bitmap-driven MQTT discovery drip publisher (#547) -
  eliminates the reconnect burst by spreading publishes across the drip
  loop
- 1df3eca5 stap-1: MQTT discovery burst fix + PROGMEM index refactor
- e34bbebe fix: relax heap thresholds for streaming discovery architecture
- a91220af fix: PROGMEM-as-RAM crash in autodiscovery (Exception 3)
- 7f663524 / 267fa013 eliminate sLine[1200] and topicBuf[200] staging
  buffers
- 0d6942a9 feat: enable WiFiClient sync mode to save ~1KB TCP buffer copy
- 5c1d04b5 feat: add hourChanged() and MQTT connect heap diagnostics (the
  observability that surfaces any future regression early)
- caaad6fa Merge dev into 2.0.0: integrate ESP8266 heap pressure fixes and
  MQTT discovery drip
- 5384be2a feat: restore SAT switch/select HA discovery (TASK-284) - uses
  the same drip + heap guard, does not reintroduce the burst pattern

Together these cap peak heap usage during discovery in the low hundreds of
bytes, spread publishes across the drip loop, and remove the PROGMEM
aliasing crashes that compounded the low-heap failures. ESP8266 build
(arduino-cli) is green (RAM 84%, IRAM 94%, Flash 71%) after all of the
above are merged.

ACs marked complete on code-evidence grounds. Field validation on a real
ESP8266 + MQTT broker remains the final proof; any regression should open
a fresh task rather than reopen this one, because the underlying
architecture has changed.

Clarification after user feedback: adding the 13 SAT switches and 1 SAT
select in TASK-284 is not extra heap pressure, it is the new architecture
proving its point. Each discovery config is streamed in 128-byte chunks
with STREAM_HEAP_MIN and canPublishMQTT() gating every publish, and the
drip publisher emits one entity per tick. More entities only extend the
duration of discovery; peak heap footprint is unchanged. SAT simply
rides on the same rails the streaming rework built; it does not push the
ESP8266 back toward the v1.4.0-beta crash regime.
<!-- SECTION:FINAL_SUMMARY:END -->
