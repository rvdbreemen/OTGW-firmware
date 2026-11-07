---
id: TASK-1710
title: 'CI: skip the Claude Code Review job on pull requests from forks'
status: To Do
assignee: []
created_date: '2026-11-07T19:31:00Z'
updated_date: '2026-11-07T19:33:00Z'
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
- [ ] #2 same-repo PRs still get the review
- [ ] #3 a fork PR shows the check as skipped, not failed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07. Evidence: the condition replayed against the last 60 claude-code-review runs (head_repository.full_name from the Actions API): 15 failure -> all from forks, would now skip; 45 success -> all same-repo, would still run. YAML parses (PyYAML). Not verified: no actionlint on this machine, and the skipped check on a real fork PR and a same-repo review run after the change can only be seen on the next PRs (AC 2 and 3 stay open until then).
<!-- SECTION:NOTES:END -->
