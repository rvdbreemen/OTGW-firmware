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
- **2026-05-26**: Version bump in 2.0.0 src files (comment headers) from a `build.py` run lands as unstaged changes. The pre-commit hook blocks the commit with "does not bump _VERSION_PRERELEASE" even when the version was already bumped in a previous commit. Use `OTGW_BUMP_HOOK_DISABLE=1 git commit` with a `chore(meta):` prefix to commit the housekeeping header updates. The commit-msg hook requires either a TASK-NNN reference or an exemption prefix (`chore(meta):`, `chore(release):`, `chore(housekeeping):`, `chore(daily-report):`).
- **2026-05-26**: The backlog CLI auto-stages and auto-commits task file changes. The adr-kit pre-commit hook can auto-change ADR Status from Proposed to Accepted. Do not manually re-write or re-commit ADR files after `backlog task edit` — check git log first to avoid duplicating the commit.
<!-- Format: [YYYY-MM-DD] Description of what went wrong and what to do instead. -->

## Decision Log

- 2026-05-05: MQTT discovery drip policy is platform-aware on 2.0.0. ESP8266 keeps the existing `HEAP_LOW` / 2000ms cooldown behavior; ESP32 uses a shorter status-burst cooldown and only enters discovery slow-mode when both free heap and largest allocatable block are genuinely low.

<!-- Significant technical decisions with rationale. Why X was chosen over Y. -->
