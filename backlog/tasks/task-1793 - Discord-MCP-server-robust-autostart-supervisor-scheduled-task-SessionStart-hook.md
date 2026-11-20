---
id: TASK-1793
title: >-
  Discord MCP server: robust autostart (supervisor + scheduled task +
  SessionStart hook)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T23:29:00Z'
updated_date: '2026-11-18T06:35:00Z'
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
- [x] #2 Autostart at logon waits for the network instead of failing
- [x] #3 A Claude session start brings the server up when it is down
- [x] #4 .claude/rules/discord.md and docs/guides/discord-mcp-server.md describe the new setup
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-11-10 23:22-23:31 (logs in discord-mcp-saseq\logs\supervisor.log): adopt a running server (no 2nd java); a 2nd supervisor exits on the mutex (code 0); java killed 23:23:02 -> back 23:23:55 (JDA Login Successful, port answers); supervisor killed -> back after 54 s via the 1-min task trigger (first design relied on the task's restart-on-failure, which did not fire for a hard-killed supervisor, LastTaskResult 0: replaced by a 1-min trigger with IgnoreNew); everything down -> SessionStart hook brings :8085 up in 38 s; a running Claude session keeps working across server restarts without /mcp (get_server_info before and after). Task registered without admin. Not verified: a real reboot / logon without network (AC2 implemented: wait for discord.com:443 before each start; the boot path itself is untested), and whether Claude Code waits for the SessionStart hook before its MCP connect.

2026-11-18 AC2 check (otgw-docs). Real reboots: boot 07:30:22 (Win32_OperatingSystem.LastBootUpTime); supervisor.log 07:31:06 'supervisor start (pid 19888)', 07:31:13 'started java pid 3424', 07:32:43 'server pid 3424 answers on :8085'. The reboot before that went the same way (07:06:31 start, 07:07:48 answers). No 'waiting for network' line on either boot: the network was already up within 44 s of boot, so the wait loop did not run. Isolated test of the real code: Test-Tcp and Wait-Network taken from supervise-discord-mcp.ps1 by AST, Test-Tcp pointed at 127.0.0.1:47342, a listener opened after 12 s. Result: '07:34:49 waiting for network (discord.com:443 not reachable)', '07:35:01 network up after ~10 s', Wait-Network returned after 14 s. Start-Server only runs after Wait-Network returns (main loop, line 114). Not observed: a boot with no network at logon. The SessionStart hook message 'was down; started it' at this boot came while java was still in its ~90 s Spring/JDA start (07:31:13 to 07:32:43); the supervisor log has one start only, so the task was already running and the hook's start was a no-op. That is not evidence that the hook was needed. Scheduled task LastTaskResult 0x800710E0 at 07:34:02: read as the minute trigger refused by IgnoreNew while the supervisor runs; interpretation, not checked further.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: the supervisor restarts a killed java in 53 s and comes back itself in 54 s through the 1-min task trigger; mutex plus adopt keep one instance (tested 2026-11-10). AC2: two real reboots on 2026-11-18 (boot 07:30:22 -> supervisor 07:31:06 -> :8085 at 07:32:43; same at 07:06) came up without help. The real Wait-Network loop, tested in isolation against a port that opens after 12 s, logs 'waiting for network' and returns once the port is reachable ('network up after ~10 s'). Not observed: a logon with no network at all. AC3: the SessionStart hook brought :8085 up in 38 s from all-down (tested 2026-11-10). AC4: .claude/rules/discord.md and docs/guides/discord-mcp-server.md describe the setup.
<!-- SECTION:FINAL_SUMMARY:END -->
