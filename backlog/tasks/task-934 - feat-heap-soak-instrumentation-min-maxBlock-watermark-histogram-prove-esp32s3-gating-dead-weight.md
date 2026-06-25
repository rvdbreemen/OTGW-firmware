---
id: TASK-934
title: >-
  feat(heap): soak instrumentation — min-maxBlock watermark + histogram to prove
  ESP32-S3 preventive gating is dead weight
status: To Do
assignee:
  - '@claude'
created_date: '2026-06-25 17:35'
labels:
  - heap
  - instrumentation
  - esp32-s3
  - soak
dependencies: []
ordinal: 148000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
dev (2.0.0 ESP32-S3) already carries a sophisticated heap-fragmentation defense inherited from the ESP8266 (1.x) failure mode: maxBlock-aware tier promotion (getHeapHealth + HEAP_FRAG_PROMOTE_MAXBLOCK, helperStuff.ino:897), per-consumer ladders (ADR-121), drip throttle gating (slow-mode/burst-skip/cooldown) with cumulative counters in HeapDiagSection (OTGW-firmware.h:447), direct maxBlock<8192 emergency/serve gates (MQTTstuff.ino:2092/2109, SATcontrol.ino:1975), and a loop-gap watermark (iMaxLoopGapMs).

Hypothesis (maintainer): on ESP32-S3 (512KB SRAM + PSRAM, async stack) this preventive gating and any delay(1)-style loop pacing are unnecessary — the contiguous-block collapse that crashed ESP8266 does not occur. To prove it we need observability the current build lacks: dev publishes only INSTANTANEOUS max_block hourly, which cannot catch a transient dip.

Phase 1 (this task, code): add since-boot watermarks + a coarse histogram, sampled at 1 Hz in doTaskEvery1s() (off the hot path; getMaxFreeBlockSize() walks the free list). Expose via MQTT stats + telnet, with a telnet reset key to start a fresh soak window without reboot. PURE OBSERVATION — no behaviour change; existing gating stays active so its counters can be watched.

Phase 2 (maintainer-run soak): build+flash (build.py / flash_otgw.bat), drive fragmenting load (scripts/sat_boiler_emulator.py + concurrent Web UI + MQTT discovery republish), capture via scripts/capture-mqtt-debug.bat -DurationSeconds N -Topic "otgw-firmware/stats/#".

Phase 3 (gated on soak data, NOT in this task): if proven dead, remove drip/tier gating — touches the evaluate.py ADR-089 gate (requires getHeapHealth() to reference HEAP_FRAG_PROMOTE_MAXBLOCK + platformMaxFreeBlock()) and ADR-121/089 docs.

Coordination: firmware files (OTGW-firmware.h/.ino, MQTTstuff.ino, handleDebug.ino, helperStuff.ino) are owned by this work; the v2 Web UI (data/*) is owned by another agent — do NOT touch UI assets.

Proof criterion: min_max_block stays well above gating thresholds (8192 emergency / 1536 promote) AND drip_slowmode/mqtt_drops/ws_drops/enter_* == 0 AND max_loop_gap_ms low, over a representative soak under fragmenting load.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 HeapDiagSection gains iMinMaxBlock (init 0xFFFFFFFF) and coarse histogram bucket counters (<2k/<4k/<8k/<16k/>=16k), updated at 1 Hz in doTaskEvery1s() from platformMaxFreeBlock()
- [ ] #2 No change to getHeapHealth() or any existing gating/threshold logic; evaluate.py ADR-089 gate still passes
- [ ] #3 sendMQTTheapdiag() publishes stats/min_max_block, stats/min_free_heap (native getMinFreeHeap), stats/max_loop_gap_ms, and the histogram bucket topics; topic-count comment in OTGW-firmware.h updated
- [ ] #4 Telnet debug dump (handleDebug.ino) shows min_max_block, min_free_heap, histogram, and a reset key that zeroes the watermark+histogram for a fresh soak window without reboot
- [ ] #5 Builds clean for ESP32-S3 via build.py; capture-mqtt-debug.bat -Topic otgw-firmware/stats/# shows the new topics
- [ ] #6 Test-automation workflow (build/flash/load/capture) documented in CLAUDE.md
<!-- AC:END -->
