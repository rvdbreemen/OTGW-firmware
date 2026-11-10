---
id: TASK-1793
title: >-
  Discord MCP server: robust autostart (supervisor + scheduled task +
  SessionStart hook)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-10T23:29:00Z'
updated_date: '2026-11-10T23:30:00Z'
labels:
  - tooling
  - discord
milestone: 2.0.0
dependencies: []
ordinal: 888000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-10: the Discord MCP server (saseq, http://localhost:8085/mcp) did not always come up after a reboot, and Claude sessions then had no discord tools. Root cause (from the server log): Spring Boot logs in to Discord (JDA) before Tomcat opens port 8085; started from the Startup-folder shortcut at logon without network, the login failed, the JVM exited and nothing restarted it; the launcher logged nothing. Fix outside the repo in D:\Users\Robert\Documents\GitHub\RvdB\discord-mcp-saseq: supervise-discord-mcp.ps1, start-supervisor-hidden.vbs, scheduled task 'OTGW Discord MCP', ensure-discord-mcp.ps1 SessionStart hook; repo docs updated.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The server comes back by itself after a crash or hang, and only one instance ever holds the bot token
- [ ] #2 Autostart at logon waits for the network instead of failing
- [x] #3 A Claude session start brings the server up when it is down
- [x] #4 .claude/rules/discord.md and docs/guides/discord-mcp-server.md describe the new setup
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-11-10 23:22-23:31 (logs in discord-mcp-saseq\logs\supervisor.log): adopt a running server (no 2nd java); a 2nd supervisor exits on the mutex (code 0); java killed 23:23:02 -> back 23:23:55 (JDA Login Successful, port answers); supervisor killed -> back after 54 s via the 1-min task trigger (first design relied on the task's restart-on-failure, which did not fire for a hard-killed supervisor, LastTaskResult 0: replaced by a 1-min trigger with IgnoreNew); everything down -> SessionStart hook brings :8085 up in 38 s; a running Claude session keeps working across server restarts without /mcp (get_server_info before and after). Task registered without admin. Not verified: a real reboot / logon without network (AC2 implemented: wait for discord.com:443 before each start; the boot path itself is untested), and whether Claude Code waits for the SessionStart hook before its MCP connect.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: supervisor restarts a killed java in 53 s and itself comes back in 54 s via the 1-min task trigger; mutex + adopt keep one instance (tested). AC3: the SessionStart hook brought :8085 up in 38 s from all-down (tested). AC4: rules + guide updated. AC2 open: network wait is implemented but a real reboot has not been observed; check discord-mcp-saseq\logs\supervisor.log after the next reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
