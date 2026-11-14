---
id: TASK-1710
title: 'CI: skip the Claude Code Review job on pull requests from forks'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T19:31:00Z'
updated_date: '2026-11-14T20:53:00Z'
labels:
  - ci
  - github-actions
dependencies: []
ordinal: 821000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
claude-code-review.yml runs on pull_request. GitHub gives a fork PR no repository secrets and no OIDC token, so the action fails after about 25 s ('Could not fetch an OIDC token', PR #699) and every outside contribution shows a red check. Skip the job for forks; pull_request_target is rejected because it would run with CLAUDE_CODE_OAUTH_TOKEN on a job steered by an outside PR's text.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 the claude-review job has if: github.event.pull_request.head.repo.full_name == github.repository
- [x] #2 same-repo PRs still get the review
- [ ] #3 a fork PR shows the check as skipped, not failed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07. Evidence: the condition replayed against the last 60 claude-code-review runs (head_repository.full_name from the Actions API): 15 failure -> all from forks, would now skip; 45 success -> all same-repo, would still run. YAML parses (PyYAML). Not verified: no actionlint on this machine, and the skipped check on a real fork PR and a same-repo review run after the change can only be seen on the next PRs (AC 2 and 3 stay open until then).

2026-11-09 (system32-78) check: the fix a3ff23645 is on origin/dev (the default branch, so PRs into dev run the new workflow from their merge ref); claude-code-review.yml line 20 has the condition. origin/main (1.x line, 19f0f5225) does NOT have it, so a fork PR into main still fails red; port it to the 1.x line only if fork PRs into main are expected (Robert's call). gh run list --workflow claude-code-review.yml: newest run 2026-11-07T17:59:42Z (PR claude/project-thread-shp96q, failure), i.e. no run since the fix (committed 2026-11-07T19:33Z); no open PRs. AC2/AC3 stay open until the next same-repo PR and the next fork PR into dev; nothing left to code. Not done on purpose: opening a test PR (outward action, not asked).

2026-11-14 (comms): AC2 evidence: after a3ff23645 (2026-11-07T19:33Z, the fork guard on dev) the same-repo PRs #700 (2026-11-13) and #701 (2026-11-14T15:47Z) ran the claude-review job: run of #701 job claude-review conclusion success, 6 steps (not skipped). AC3 not evidenceable now: no fork PR since the fix (last fork PR #699 by Crash-Evans ran 2026-11-07T17:59Z, before a3ff23645, and failed). Gap found: a pull_request run uses the workflow file of the PR's base branch. origin/main and origin/otgw-1.x.x still have the guard commented out (claude-code-review.yml line 16 '# if: |'), so a fork PR against main or otgw-1.x.x will still show a red check. Porting the guard to otgw-1.x.x (and main via the release flow) is a cross-line decision for Robert.

2026-11-14 (comms): first fork PR since the guard: #702 (justink0k, base dev, 20:41Z). Its claude-code-review run 43638815774 is completed/action_required: GitHub holds workflows of a first-time contributor until a maintainer approves them, so the job has not run yet and AC3 (skipped, not failed) is still not shown. It shows once Robert approves the workflow run (or for a fork PR from a returning contributor).
<!-- SECTION:NOTES:END -->
