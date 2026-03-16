---
# METADATA
Document Title: OTA Filesystem Corruption Analysis Archive
Review Date: 2026-03-16 06:12:00 UTC
Commits Fixed: ab692e14c673c935572ad6f810882b3390716833
Target Version: v1.3.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: Archive Overview
PR Branch: copilot/fix-usb-crash-dump-issue
Status: COMPLETE
---

# OTA Filesystem Corruption Analysis

## Overview

This archive documents the root-cause analysis for two bugs that caused LittleFS filesystem corruption after OTA (Over-The-Air) filesystem updates in v1.3.0-beta. The bugs only became visible after performing an OTA filesystem flash and examining USB crash dumps — WiFi was unavailable on the corrupted device, making telnet and MQTT output unreachable.

## What Was Fixed

Two bugs were fixed in commit `ab692e1`:

1. **WiFi reconnect mid-OTA tearing down the upload HTTP connection** — `loopWifi()` was called unconditionally in `doBackgroundTasks()`, even during active flash operations. During `Update.write()`, the ESP8266 briefly suspends flash reads, causing `WiFi.status()` to report `WL_DISCONNECTED`. The non-blocking state machine then reconnected, and the `WIFI_RECONNECTED` branch restarted `startWebSocket()` / `startMQTT()`, tearing down the HTTP connection mid-upload. Fix: guard `loopWifi()` with `!isFlashing()`.

2. **Partial LittleFS partition erase leaving stale inodes** — `Update.begin(uploadTotal, U_FS)` only erased sectors covered by the image (~125 of 256 blocks). LittleFS always mounts with `block_count = 256`, scanning all blocks including the stale upper half. Fix: always pass the full `fsSize` to `Update.begin()`.

## Why It Was Hard to Find

See [`ROOT_CAUSE_ANALYSIS.md`](ROOT_CAUSE_ANALYSIS.md) for the complete analysis. In short:

- Only triggered by filesystem OTA (not firmware OTA, which is far more commonly tested)
- Bug 1 is a race condition — non-deterministic; strong WiFi connections mask it entirely
- Silent regression: the implicit flash-guard from v1.2.0 was lost when refactoring to the non-blocking WiFi state machine
- The crash happens *after* a "successful" upload, on the next reboot — a temporal gap between cause and effect
- With LittleFS corrupted, WiFi cannot start (settings unreadable), eliminating all remote debug channels
- USB crash dump was the only visible output, and its stack trace pointed to LittleFS, not OTA code

## Documents in This Archive

| File | Purpose |
|------|---------|
| `README.md` | This file — archive overview |
| `ROOT_CAUSE_ANALYSIS.md` | Full speculative analysis: why each factor made the bug hard to find |

## Timeline

| Date | Event |
|------|-------|
| v1.3.0-beta | Bug introduced — `loopWifi()` added to `doBackgroundTasks()` without flash guard |
| 2026-03-13 | Bug discovered via USB crash dump after filesystem OTA |
| 2026-03-13 | `ab692e1` — both bugs fixed |
| 2026-03-15 | OTA flow refactored and hardened (`dfa61a8`, `56f19aa`) |
| 2026-03-16 | This root-cause analysis written |
