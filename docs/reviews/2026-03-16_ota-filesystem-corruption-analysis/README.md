---
# METADATA
Document Title: OTA Filesystem Corruption Analysis Archive
Review Date: 2026-03-16 06:12:00 UTC
Commits Fixed: ab692e14c673c935572ad6f810882b3390716833, c628f248, 1d2543fe, 8d44d8ba
Target Version: v1.3.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: Archive Overview
PR Branch: copilot/fix-usb-crash-dump-issue
Status: COMPLETE
---

# OTA Filesystem Corruption Analysis

## Overview

This archive documents the root-cause analysis for three bugs that caused crashes after OTA (Over-The-Air) filesystem updates in v1.3.0-beta. All three bugs had to co-occur for the crash to be visible. The crash only became apparent after performing an OTA filesystem flash and examining USB crash dumps — WiFi was unavailable on the affected device, making telnet and MQTT output unreachable.

## What Was Fixed

### Deep Cause — `cMsg` Re-entrancy in Settings Loading (`c628f24`, `1d2543f`)

After a filesystem OTA, the device reboots to a clean filesystem and calls `readSettings()`. This function used the global `cMsg` buffer as its line-read buffer. `writeJsonStringKV()` in `writeSettings()` also used `cMsg` as an escape scratch (`escapeJsonStringTo(value, cMsg, ...)`). When `file.readBytesUntil()` called `yield()` internally (while reading from flash), the HTTP server could run another handler that triggered `writeSettings()`, overwriting `cMsg` mid-parse. The result: all key names were garbage, no `strcasecmp_P(field, PSTR("key"))` comparisons matched, and all settings loaded as defaults — including an empty MQTT broker string that caused a subsequent crash.

**Fix:** Replace `cMsg` with a private `char lineBuf[256]` stack variable in `readSettings()`, and clear `settingsDirty = false` immediately after loading to prevent the deferred flush from triggering a re-write mid-read.

### Bug 1 — WiFi Reconnect Mid-OTA Tearing Down the Upload (`ab692e1`)

`loopWifi()` was called unconditionally in `doBackgroundTasks()`, even during active flash operations. During `Update.write()`, the ESP8266 briefly suspends flash reads, causing `WiFi.status()` to report `WL_DISCONNECTED`. The non-blocking state machine reconnected, and the `WIFI_RECONNECTED` branch restarted `startWebSocket()` / `startMQTT()`, tearing down the HTTP connection mid-upload. Result: LittleFS partition partially written.

**Fix:** Guard `loopWifi()` with `!isFlashing()`.

### Bug 2 — Partial LittleFS Partition Erase Leaving Stale Inodes (`ab692e1`)

`Update.begin(uploadTotal, U_FS)` only erased sectors covered by the image (~125 of 256 blocks). LittleFS always mounts with `block_count = 256`, scanning all blocks including the stale upper half. Result: stale inodes in blocks 125–255 caused mount failure or silent file corruption on previously-used devices.

**Fix:** Always pass the full `fsSize` to `Update.begin()`.

## Why It Was Hard to Find

See [`ROOT_CAUSE_ANALYSIS.md`](ROOT_CAUSE_ANALYSIS.md) for the complete analysis. In short:

- All three bugs had to co-occur — no single bug was sufficient; individually they seemed harmless
- The deep cause (`cMsg` re-entrancy) only fired on a fresh filesystem (post OTA), never on a normal boot
- Only triggered by filesystem OTA (not firmware OTA, which is far more commonly tested)
- Bug 1 is a race condition — non-deterministic; strong WiFi connections mask it entirely
- Silent regression: the implicit flash-guard from v1.2.0 was lost when refactoring to the non-blocking WiFi state machine
- The crash happens *after* a "successful" upload, on the next reboot — a temporal gap between cause and effect
- With settings unreadable, WiFi cannot start, eliminating all remote debug channels
- USB crash dump was the only visible output, and its stack trace pointed to LittleFS, not OTA code or settings

## Documents in This Archive

| File | Purpose |
|------|---------|
| `README.md` | This file — archive overview |
| `ROOT_CAUSE_ANALYSIS.md` | Full speculative analysis: why each factor made the bug hard to find |

## Timeline

| Date | Event |
|------|-------|
| v1.3.0-beta | `writeJsonStringKV` starts using `cMsg` as escape scratch (Bug 0 introduced) |
| v1.3.0-beta | `loopWifi()` added to `doBackgroundTasks()` without flash guard (Bug 1 introduced) |
| 2026-03-01 | `c628f24` — settings lineBuf fix; `1d2543f` — settingsDirty clear fix |
| 2026-03-13 | Bug discovered via USB crash dump after filesystem OTA |
| 2026-03-13 | `ab692e1` — Bugs 1 & 2 fixed |
| 2026-03-15 | OTA flow refactored and hardened (`dfa61a8`, `56f19aa`) |
| 2026-03-16 | This root-cause analysis written and updated |
