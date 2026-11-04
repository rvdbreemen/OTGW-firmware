---
id: TASK-670
title: 'docs: update for changes since v1.4.2-beta'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 07:12'
updated_date: '2026-05-22 08:18'
labels:
  - docs
  - update-docs
dependencies: []
priority: high
ordinal: 51000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Standalone /update-docs run on 2.0.0 feature branch. Scope: 638 commits, 976 files between v1.4.2-beta and HEAD (origin/feature-dev-2.0.0-otgw32-esp32-sat-support@026de118). Affected subsystems: network (4 files), MQTT (2), REST (1), OTGW-Core (4), SAT (7), sensors (1), settings (1), web UI (16), build (3), main (2); 25 .ino files total. Affected docs: manual chapters 1/3/4/5/6/8/9 (EN+NL), all major API docs, C4 architecture, cleanup phase.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Manual chapter 1 (ch01-introduction.md, h01-introductie.md) updated
- [x] #2 Manual chapter 3 (ch03-web-interface.md, h03-webinterface.md) updated
- [x] #3 Manual chapter 4 (ch04-home-assistant.md, h04-home-assistant.md) updated
- [x] #4 Manual chapter 5 (ch05-sat-thermostat.md, h05-sat-thermostaat.md) updated
- [x] #5 Manual chapter 6 (ch06-network.md, h06-netwerk.md) updated
- [x] #6 Manual chapter 8 (ch08-developer-guide.md, h08-ontwikkelaarsgids.md) updated
- [x] #7 Manual chapter 9 (ch09-api-reference.md, h09-api-referentie.md) updated
- [x] #8 API documentation (openapi.yaml, README.md, MQTT.md, WEBSOCKET_FLOW.md, DALLAS_SENSOR_LABELS_API.md) updated to match current implementation
- [x] #9 C4 architecture docs (c4-code-*.md and c4-component-*.md affected by the 25 changed .ino files) updated
- [x] #10 Cleanup phase complete: archive old releases, move misplaced files, reorg reviews, verify docs/archive
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Ch01 done: EN highlights + NL highlights updated for ADR-100/101/106, BLE, Ethernet failover, English-only UI, alpha-status callout. No new architectural decisions; documented existing ADRs.

Ch03 done: EN+NL updated for English-only UI, FS-mismatch banner, log sub-tabs, Wi-Fi scan panel, BLE+Dallas SAT panels, security/CSRF, ETag flow, FSexplorer changes. No architectural calls.

Ch04 done: EN+NL — ADR-100/101/102/105/106 (topic shapes, JIT discovery, availability), TASK-668 resetgateway HA button + 5s rate-limit, BLE sensor entities, bUseLegacyOtTopics + bSeparateSources toggles.

Ch05 done: EN+NL — PID clamp, full Open-Meteo current-conditions, tail-window cycle classification, DS18B20 area mapping, P75 zone aggregation, NimBLE BLE rewrite (~400 KB freed), TT=/TC= PIC parity, OTDirect coalesce. ADR-092 cited.

Ch01 manual follow-up: corrected 'ESP8266 now uses Arduino core 3.1.2' claim in EN+NL — the v1.5.x LTS line reverted from 3.1.2 to 2.7.4 (confirmed in dev CHANGELOG.md). AceTime 4.x note kept (core-version independent).

Ch06 done: EN+NL — Ethernet runtime failover (TASK-581) + retained network/mode topic + REST device/info fields + 1s polling; WiFi reconnect ADR-075 cited; static-IP REST keys + DHCP fallback; ADR-056 protected-admin contract for OTA.

Ch08 done: EN+NL — ADR-079/081 per-component types.h, Core 2.7.4 LTS toolchain (explicit revert documented), NimBLE (ADR-092), JIT discovery (ADR-100), flat topics (ADR-101), legacy toggle (ADR-106), OLED capability flag, OTDirect coalesce-by-MsgID, flash-wear no-op writeSettings, dispatch table setcmds[] rename, PlatformIO env table updated.

Ch09 done: EN+NL — route dispatch ADR-050/056, error 409/429, device/info Ethernet+wifiquality+failover fields, OTDirect mode table, SAT vocab + /sat/markers /sensor-areas /weather /ble/* sections, JIT discovery TASK-578, ADR-097/100/102/103/104/105/106 MQTT semantics, WebSocket TASK-563. FINDING: openapi.yaml is missing /api/v2/debug, /v2/network/scan, /v2/sat/ble/*, /v2/sat/markers, /v2/sat/sensor-areas, /v2/sat/weather/needs-setup — feed into AC#8 API agent.

API docs done: openapi.yaml +10 paths (debug, network/scan, sat/weather/needs-setup, sat/ble/*, sat/markers, sat/sensor-areas) — every kV2Routes[] entry now spec'd. README.md sections added. MQTT.md: resetgateway payload+rate-limit (TASK-668), ADR-100/101/105/106 callouts, mqttuselegacyottopics setting. WebSocket + Dallas docs unchanged (no wire protocol change).

C4 docs done: 17/20 files updated (sensors + utilities + component-sensors unchanged). Code-level: mqtt/network/otdirect/otgw-core/rest-api/sat/settings/web-assets all refreshed for ADR-079/081/092/100/101/103/106 + TASK-466/487/494/508/511/569/581/665/668. Component-level + container + context: also refreshed. Existing C4 structure preserved.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Standalone /update-docs run on 2.0.0 since v1.4.2-beta. Updated 7 manual chapters (EN+NL), all major API docs (openapi.yaml +10 endpoints, README, MQTT.md), and 17 of 20 C4 files. Phase 4 cleanup: moved RELEASE_NOTES_1.4.1.md and RELEASE_GITHUB_1.4.1.md from repo root to docs/releases/; releases archive (4 files) below the 10-file threshold; reviews dir (14 subdirs) below the 15-dir threshold; daily-issue-report.md already untracked + gitignored (from TASK-668 merge). Total: 33 modified files + 2 renames. Surfaced ADRs documented: 056, 062, 075, 079, 081, 092, 097, 100, 101, 102, 103, 104, 105, 106. Chapter 1 follow-up correction: ESP8266 Core 2.7.4 LTS (the 3.1.2 attempt was reverted on v1.5.x). One discrepancy from Ch9 agent (openapi.yaml endpoint gaps) was closed by AC#8 API agent. No new architectural decisions made; all changes document existing Accepted ADRs and merged TASKs.
<!-- SECTION:FINAL_SUMMARY:END -->
