# Branch Hygiene Status

Snapshot date: 2026-02-26
Repository: `origin`
Policy source: `docs/process/release-workflow.md`

## Current summary

- `active`: 20
- `stale-unmerged`: 20
- `stale-merged`: 0

## Scope

- No branch deletions in this pass.
- Objective is inventory and classification only.

## Permanent branches

- `origin/main`
- `origin/dev`

## Stale-merged candidates (merged into `origin/dev`)

- None currently.

## Stale-unmerged candidates (review required)

- `origin/dev-branch-v1.2.0-beta-adr040-clean`
- `origin/dev-bunch-of-improvements`
- `origin/dev-improvement-rest-api-compatibility`
- `origin/dev-improving-mqtt-api`
- `origin/dev-streaming-MQTT`
- `origin/dev-working-rc6`
- `origin/revert-360-dev`
- `origin/dev-trying-userfs`
- `origin/dev-wemos-d1-esp32`
- `origin/copilot/review-wifi-manager-implementation`
- `origin/copilot/monitor-memory-usage`
- `origin/copilot/fix-wemos-d1-bootloop-issue`
- `origin/copilot/fix-web-ui-gateway-mode`
- `origin/copilot/review-last-commits-critically`
- `origin/copilot/update-labels-in-dashboard`
- `origin/copilot/sub-pr-406`
- `origin/copilot/fix-action-job-issue`
- `origin/copilot/update-readme-contributions-section`
- `origin/copilot/create-improvement-plan`
- `origin/copilot/sub-pr-420-again`
- `origin/copilot/sub-pr-420`
- `origin/codex/add-visual-indicator-for-gateway-mode-thaj95`

## Next review checklist

1. Confirm branch owner and intent for each stale-unmerged branch.
2. Decide per branch: keep active, archive marker/tag, or schedule deletion.
3. Move stale-merged branches to deletion queue when present.
4. Re-run inventory after next stable release tag.

## Execution-ready command list (no-delete flow)

Generate the review queue CSV:

```pwsh
pwsh -File scripts/branch-hygiene-queue.ps1 -Remote origin -BaseBranch dev -InactiveDays 14 -OutputCsv docs/process/branch-hygiene-queue.csv
```

Review only branches requiring owner action:

```pwsh
Import-Csv docs/process/branch-hygiene-queue.csv |
  Where-Object { $_.Status -eq 'stale-unmerged' -or $_.Status -eq 'stale-merged' } |
  Select-Object Branch, Status, Author, LastCommitDate, ProposedAction, Owner, Decision, Notes
```

Prepare a dry-run deletion command list (do not execute):

```pwsh
Import-Csv docs/process/branch-hygiene-queue.csv |
  Where-Object { $_.Status -eq 'stale-merged' -and $_.Decision -eq 'delete' } |
  ForEach-Object {
    $shortName = $_.Branch -replace '^origin/', ''
    "git push origin --delete $shortName"
  }
```

Optional archive marker before deletion decision:

```pwsh
$branch = 'origin/example-branch'
$tag = 'archive/' + ($branch -replace '^origin/', '') + '-' + (Get-Date -Format 'yyyyMMdd')
git tag $tag "refs/remotes/$branch"
git push origin $tag
```

## Commands used for this snapshot

```pwsh
git fetch --all --prune
git for-each-ref --sort=-committerdate --format='%(refname:short)|%(committerdate:short)|%(authorname)|%(upstream:short)' refs/heads
git for-each-ref --sort=-committerdate --format='%(refname:short)|%(committerdate:short)|%(authorname)' refs/remotes/origin
git branch -r --merged origin/dev
git branch -r --no-merged origin/dev
```
