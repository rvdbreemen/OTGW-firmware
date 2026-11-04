---
id: TASK-383
title: >-
  Docs: add Arduino Core 3.1.2 upgrade warning (LittleFS partition change causes
  ~10 min boot + settings loss)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-22 06:35'
updated_date: '2026-04-22 06:47'
labels:
  - docs
  - migration
  - arduino-core
dependencies: []
references:
  - 'Discord #beta-testing, andrebrait, 2026-04-22'
  - 'Arduino Core 3.1.2 changelog: LittleFS partition 1MB → 2MB'
  - 'platformio.ini: check which Arduino Core version is pinned'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

Reported by **andrebrait** in Discord `#beta-testing` on 2026-04-22, upgrading from 1.3.10 → 1.4.1:

> "FYI 1.4.1 coming from 1.3.10 is taking forever to reboot, after the LittleFS is flashed... it took a good 10 minutes or so for it to come back online."
> "Up to 1.3.10 it was working normally"

Maintainer confirmed: "I think it's caused by upgrading Arduino Core 3.1.2 to be honest"

## Root Cause

1.4.1 uses Arduino Core 3.1.2, which changed the LittleFS partition from **1MB to 2MB**. Users upgrading from 1.3.x (Core 2.7.4, 1MB LittleFS) face:

1. **If only firmware is flashed (not filesystem):** `LittleFS.begin()` finds the old 1MB filesystem at the wrong sector offsets. The core reformats the partition. Reformatting a 2MB LittleFS on ESP8266 takes 5–10 minutes. During this time the device appears unresponsive ("forever to reboot").

2. **After reformat:** `settings.ini` no longer exists. All settings reset to factory defaults: MQTT broker → `homeassistant.local`, credentials → blank. Users must re-enter all settings manually.

3. **HA auto-discovery shows "Unnamed device":** Separate firmware bug (see related task), but made worse because settings (hostname, MQTT topic) are also lost.

## Required documentation changes

- **Release notes / upgrade notes for v1.4.1** (and any release based on Arduino Core 3.1.2): explicit warning that BOTH firmware.bin AND littlefs.bin must be flashed when upgrading from 1.3.x
- **README / BREAKING_CHANGES.md**: note the Arduino Core upgrade and filesystem partition change as a breaking migration step
- **Web UI / OTA page**: consider adding a UI warning if the detected core version changed, prompting the user to also flash the filesystem

## Acceptance Criteria — docs only, no code changes required for this task
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BREAKING_CHANGES.md has a v1.4.1 section explicitly warning about LittleFS partition change
- [x] #2 Upgrade notes state: 'Flash both firmware.bin AND littlefs.bin when upgrading from 1.3.x or any build based on Arduino Core 2.7.4'
- [x] #3 The warning explains the ~10 min boot and settings loss consequence if only firmware is flashed
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated `docs/BREAKING_CHANGES.md` v1.4.1 section. The existing text only warned about settings not persisting; it did not mention the ~10 minute boot time or complete settings loss specific to 1.3.x upgrades. Expanded the section with two subsections: upgrading from v1.3.x (Core 2.7.4) describing the reformat hang and full settings wipe, and upgrading from v1.4.x describing the silent persist failure. Step 5 added to the upgrade procedure. Committed together with TASK-382 on branch fix-issue-mqtt-discovery-device-name."
<!-- SECTION:FINAL_SUMMARY:END -->
