---
name: beta-prerelease
description: 1.x (ESP8266) beta prerelease. Redirects to the authoritative skill in the otgw-1.x.x worktree; this dev-tree copy holds no process of its own. For a 2.x beta use /beta-release-2x
disable-model-invocation: true
---

# /beta-prerelease - 1.x beta prerelease (redirect)

**This file is a pointer, not the process.** The 1.x beta-prerelease skill lives
on the `otgw-1.x.x` branch, in its worktree, and only that copy is authoritative:

```
<otgw-1.x.x worktree>/.claude/skills/beta-prerelease/SKILL.md
```

Find the worktree and open it, then follow that file from Phase 0 on:

```bash
WT=$(git worktree list --porcelain | awk '/^worktree /{p=$2} /^branch refs\/heads\/otgw-1\.x\.x$/{print p}')
echo "$WT"                                   # e.g. D:/Users/Robert/Documents/GitHub/RvdB/wt-otgw-1.x.x
test -f "$WT/.claude/skills/beta-prerelease/SKILL.md" && echo "skill found"
```

Read `$WT/.claude/skills/beta-prerelease/SKILL.md` in full and execute it. Run
every command of that process from `$WT` (its Phase 0 asserts the branch).

Stop and tell the user if:

- no worktree has `otgw-1.x.x` checked out (create one with
  `git worktree add ../wt-otgw-1.x.x otgw-1.x.x` only after the user agrees), or
- the skill file is missing there.

**Never cut a 1.x beta from this tree or from `dev`.** `dev` is the 2.0.0
ESP32-S3 line; for a 2.x beta use `/beta-release-2x`. A tag pushed from the wrong
branch fires the 1.x prerelease workflow on the wrong code, and a published
release is immutable.

Why a redirect instead of a copy: two copies drift. On 2026-11-04 Robert asked
that the 1.x release skills come from the otgw-1.x.x worktree, not from dev
(same change as `/release`).
