---
id: TASK-2947
title: >-
  Web UI About/Credits redesign: source research for the contributor list
  (makers, credits, community)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T10:29:00Z'
updated_date: '2026-11-20T10:38:00Z'
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
- [ ] #1 GitHub authors of merged PRs and issues counted per person with gh api, all pages, bots and rvdbreemen excluded
- [ ] #2 Discord message counts per author over all readable channels and threads (REST, token from the environment, counts only, no message text stored)
- [ ] #3 Credits group checked against the code (library authors, file headers, README credits) for missing upstream names
- [ ] #4 Output file with Makers (Robert first), Credits and Community, sorted by contribution, public display name and the source per name, sent to the coordinator and otgw-webui
<!-- AC:END -->
