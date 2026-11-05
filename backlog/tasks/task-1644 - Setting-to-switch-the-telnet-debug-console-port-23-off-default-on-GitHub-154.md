---
id: TASK-1644
title: >-
  Setting to switch the telnet debug console (port 23) off, default on (GitHub
  #154)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T04:09:00Z'
updated_date: '2026-11-05T04:21:00Z'
labels:
  - github
  - security
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 769000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/154 point 4. Robert 2026-11-05: a setting, default ON. Today debugTelnet.begin(false) runs unconditionally (networkStuff.ino:640) and the console has no login while keys u/i drive the output GPIO (handleDebug.ino:425-434).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Persisted bool setting (ADR-051 naming), default true, in GET/POST /api/v2/settings, v2 Settings UI and settingsHelp.h
- [ ] #2 Off: TCP connect to port 23 refused without reboot (apply-live like applyLegacyPort25238Setting); On: accepted again; Debug macros safe with no telnet server
- [ ] #3 Host test red on old code / green on fix, evaluate --quick green, bench: connect refused/accepted on a bench unit
- [x] #4 docs/api settings docs and ADR-032 consequence note updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
6c 2026-11-05 05:22: implemented in wt-6c-1644 on origin/dev 8111d17ad; patch at C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/9ad25eb8-1a77-4659-887b-f3c41994cc95/scratchpad/task-1644.patch (git apply --check OK on wt-int4 0ac39c9d7). Setting settings.bTelnetEnabled (key telnetenabled, default true): struct, settings file write/read, live apply via SIDE_EFFECT_TELNET -> startTelnet() in flushSettings() (same pattern as SIDE_EFFECT_OTGWSTREAM), GET + knownSettings + device/info settings.telnet_enabled, settingsHelp.h, v2.js SET_META (system). startTelnet() closes port 23 (debugTelnet.stop()) and returns when off; all callers (boot, WiFi reconnect, telnet r, deferred apply) go through it; grep shows no other debugTelnet.begin. Evidence: tests/test_telnet_console_setting.py on old sources (OTGW_SRC_DIR) 5 FAIL / 1 ok (on_listens), on the fix 6/6 OK; test_settings_whitelist OK; test_adr_governance OK; evaluate --quick 89 checks, 82 pass, 0 fail, 0 warn. Debug macros with no server: checked by reading code only (write() returns 0 with no client, flush() loops over inactive slots, AsyncServer::end() is a no-op without a pcb, so stop() at boot on a never-begun server is safe). Pre-existing: tests/webui/settings-help.test.mjs 18 KB limit already red on origin/dev (18682 B), 18814 B with this change. Open for the bench: port 23 refused after POST telnetenabled=false and accepted after true, no reboot (AC2/AC3).
<!-- SECTION:NOTES:END -->
