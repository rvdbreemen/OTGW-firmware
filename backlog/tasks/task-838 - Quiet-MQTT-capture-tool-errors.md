---
id: TASK-838
title: Quiet MQTT capture tool errors
status: Done
assignee:
  - '@codex'
created_date: '2026-06-07 10:01'
updated_date: '2026-06-12 22:56'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Delivered on dev by commit 29dad008 (fix(capture): quiet MQTT debug tool stderr, 21 min after task creation) + refinement 55bb874a: capture-mqtt-debug.bat redirects mosquitto/headless-Edge/script stderr into mqtt.stderr.log / browser.stderr.log / script.error.log, aggregates via New-ToolErrorLog into error.txt merged into the transcript, intermediate logs cleaned up. Verified intact at dev HEAD (anchors :794/:802/:867/:923/:1863/:2175); embedded PS payload parses with 0 syntax errors. Tooling-only change (scripts/**), exempt from bump/build gates. Task record was an empty stub; this summary restores traceability.
<!-- SECTION:FINAL_SUMMARY:END -->
