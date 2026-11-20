---
id: TASK-2947
title: >-
  Web UI About/Credits redesign: source research for the contributor list
  (makers, credits, community)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T10:29:00Z'
updated_date: '2026-11-20T10:59:00Z'
labels:
  - webui
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1077000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-20 via the coordinator: the About/Credits screen of the v2 web UI gets a redesign with proper attribution. This task is the source research (otgw-docs): GitHub merged PRs, issues and commit contributors of rvdbreemen/OTGW-firmware (no parent repo; bots and rvdbreemen excluded), Discord message counts per author over all readable channels and threads, and the credits for upstream work (Schelte Bron PIC/OTGW and OTGWSerial, George Ntellas SAT algorithm, Alexwijn SAT HA component, Nodo Shop kit, vendored libraries). Output: a file with three groups (Makers, Credits, Community) with the source per name, for otgw-webui to build the screen.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GitHub authors of merged PRs and issues counted per person with gh api, all pages, bots and rvdbreemen excluded
- [x] #2 Discord message counts per author over all readable channels and threads (REST, token from the environment, counts only, no message text stored)
- [x] #3 Credits group checked against the code (library authors, file headers, README credits) for missing upstream names
- [x] #4 Output file with Makers (Robert first), Credits and Community, sorted by contribution, public display name and the source per name, sent to the coordinator and otgw-webui
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 (95): research file C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\87fc885c-0e3d-40a7-9313-4a23af8d2bc3\scratchpad\credits\credits-research.md (197 lines). GitHub (gh api --paginate, 2026-11-20): 325 merged PRs, 224 issues (116 distinct authors excl. rvdbreemen/bots), 21 commit contributors; repo has no parent. Discord (REST, 20 readable text channels + active/archived threads, 2021-02-21..2026-11-20): 50,650 messages, 283 human authors, 4 bots excluded; counts only, no message text stored. Credits checked against file headers, library.properties, platformio.ini, README and v2.js:643-652; candidates not on the screen yet: Willem Aandewiel (ESP8266 framework), Ihor Melnyk (OpenTherm lib), Bert Melis (espMqttClient), Jens Fleischer (FSexplorer), PlatformIO library authors, RobR (S0 counter). Unverified GitHub/Discord identity pairs marked as such (STemplar, vampywiz17, Roos-AID/RobR, Alexwijn/alexw47). Open for Robert: mention AI agents (claude account 145 commits) or not; real names that only appear on GitHub profiles.

Robert's decisions 2026-11-20, asked interactively in session 95 and by DM via the coordinator (same outcome): Q1 one neutral line about AI coding assistants, products may be named (DM 1573287971652763792); used as evidenced by commit trailers and repo files: GitHub Copilot (copilot-swe-agent[bot] from 2025-10-18, .github/copilot-instructions.md), Claude Code (Claude co-author trailers from 2026-02-24), OpenAI Codex (AGENTS.md since 940ac49dd, an openai-code-agent co-author trailer). Q2 public names only: handles or Discord display names (DM 1573288075210264688). Q3 short list in the UI, full list in CONTRIBUTORS.md, updated with every public stable release (DM 1573288218370244765): step added to .claude/skills/release-2x/SKILL.md Phase 2 item 3b and docs/process/RELEASE_PROCESS.md documentation checklist. Q4 (interactive, session 95): all four pairs are the same person: Roos-AID = RobR, Alexwijn = alexw47, STemplar = Simon Templar, vampywiz17 = Vamp. CONTRIBUTORS.md draft in wt-95-docs (not committed): 17 code contributors, 54 Discord community members (40+ messages), 21 GitHub issue reporters (2+). Finding for otgw-webui: the current Contributors card (v2.html:797 area) uses real names from GitHub profiles (e.g. Dave Davenport, Robbert Lagerweij, Nicole Koopmans, Rob Roos, Dirk Zwager, Tommy van der Vorst); under Q2 these become handles.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: gh api --paginate on rvdbreemen/OTGW-firmware 2026-11-20: 325 merged PRs, 224 issues (116 distinct authors), 21 commit contributors; no parent repo; bots and rvdbreemen excluded. AC2: Discord REST over 20 readable channels plus active/archived threads, 2021-02-21..2026-11-20: 50,650 messages, 283 human authors, counts only (discord_counts.py/.json in the scratchpad). AC3: credits checked against file headers, library.properties, platformio.ini, README and v2.html/v2.js; added Willem Aandewiel, RobR and the library authors. AC4: research file and decisions sent to the coordinator and otgw-webui; Robert's decisions (AI line with products, public names only, short UI list + CONTRIBUTORS.md updated every public stable release, four identity pairs confirmed) are in CONTRIBUTORS.md and the release-2x / RELEASE_PROCESS steps. Not verified: completeness of the visible-contribution notes (only what project notes recorded); Discord activity before the bot joined channels it could not read.
<!-- SECTION:FINAL_SUMMARY:END -->
