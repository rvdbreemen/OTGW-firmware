# Cerebrum

> OpenWolf's learning memory. Updated automatically as the AI learns from interactions.
> Do not edit manually unless correcting an error.
> Last updated: 2026-05-01

## User Preferences

<!-- How the user likes things done. Code style, tools, patterns, communication. -->

## Key Learnings

- **Project:** OTGW-firmware
- **Description:** [![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)
- On the 2.0.0 branch, `hd_drip_cooldown_skip` is a post-status-burst pacing counter from `MQTTstuff.ino`, not a heap-pressure counter; the actual heap-pressure counter is `hd_drip_slowmode`.
- On ESP32, `platformMaxFreeBlock()` uses `ESP.getMaxAllocHeap()` as a live runtime value. Docs and diagnostics should describe it as the largest allocatable block, not with the ESP8266-specific `getMaxFreeBlockSize()` wording.

## Do-Not-Repeat

<!-- Mistakes made and corrected. Each entry prevents the same mistake recurring. -->
<!-- Format: [YYYY-MM-DD] Description of what went wrong and what to do instead. -->

## Decision Log

- 2026-05-05: MQTT discovery drip policy is platform-aware on 2.0.0. ESP8266 keeps the existing `HEAP_LOW` / 2000ms cooldown behavior; ESP32 uses a shorter status-burst cooldown and only enters discovery slow-mode when both free heap and largest allocatable block are genuinely low.

<!-- Significant technical decisions with rationale. Why X was chosen over Y. -->
