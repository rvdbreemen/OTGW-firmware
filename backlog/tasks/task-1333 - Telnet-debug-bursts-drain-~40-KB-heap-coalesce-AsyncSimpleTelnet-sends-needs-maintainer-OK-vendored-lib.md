---
id: TASK-1333
title: >-
  Telnet debug bursts drain ~40 KB heap: coalesce AsyncSimpleTelnet sends (needs
  maintainer OK, vendored lib)
status: To Do
assignee: []
created_date: '2026-10-23T21:30:00Z'
labels:
  - heap
  - telnet
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 456000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1328. A burst of telnet debug output (the capture script turns on every debug toggle and sends 'q' read-settings + 'D' dump on each connect) drains free heap from ~72 KB to ~32 KB within 0.5 s after only ~10 KB of output, and the native watermark reaches 776 bytes on the OTGW32 bench (Sergeant D's unit: 732, alpha.474). Without a telnet client minFree stays ~24.7 KB. Mechanism (src/libraries/SimpleTelnet/src/AsyncSimpleTelnet.h write()/_flushTx): every write() does c->add(COPY)+c->send() immediately, so each debug fragment becomes its own small TCP segment with its own lwIP pbuf / WiFi dynamic TX buffer (CONFIG_ESP_WIFI_TX_BUFFER_TYPE 1, 32 dynamic; TCP_SND_BUF 5744) and AsyncTCP ack events. Not split yet which layer holds most. lwIP/WiFi are prebuilt (ADR-185 wrapper cannot reach them). Options: (A) coalesce in AsyncSimpleTelnet: only send() when the ring holds >= one MSS or on a short timer / line end (library change, needs Robert's OK); (B) firmware-side: debug writers build a whole line in one buffer and call write once per line (fewer, larger segments; no lib change); (C) capture script: do not flip all toggles / no q+D dump by default. Open: Sergeant D's unit went silent ~70 s after the dump and came back with 'Power on' (asked whether he power-cycled it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The layer that holds the heap is named (lwIP pbufs, WiFi TX buffers or AsyncTCP events) with a bench measurement
- [ ] #2 A fix chosen by the maintainer lowers the watermark in the same bench scenario (all toggles on, q+D, emulator, MQTT): before 776 B, after measured
- [ ] #3 Whether the device can wedge under this burst is settled (Sergeant D's answer or a bench run of 10+ minutes)
<!-- AC:END -->
