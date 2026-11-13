---
id: TASK-1913
title: >-
  writeSettings truncates settings.ini in place: a power cut mid-write can lose
  all settings
status: To Do
assignee: []
created_date: '2026-11-13T23:14:00Z'
labels: []
dependencies: []
priority: medium
ordinal: 979000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1912, not reproduced. writeSettings() (settingStuff.ino:288) opens /settings.ini with mode w, which truncates before the new content is written. A reset or power cut between the truncate and the close leaves an empty or partial settings.ini, and the next boot runs on defaults (WiFi, MQTT, SAT). The TASK-1495 check (settingStuff.ino:521) only detects a short write afterwards; the old file is already gone. restAPI.ino:1813-1831 and the TASK-1912 Dallas labels flush use the safe pattern: write a temp file, then LittleFS.rename() over the original.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 writeSettings() writes a temp file and renames it over /settings.ini; a failed write keeps the old file
- [ ] #2 Host test: a write cut off after the truncate leaves the old settings readable (red on the old code, green on the fix)
- [ ] #3 Bench: settings survive save + reboot; a reset during a save keeps a valid settings.ini
<!-- AC:END -->
