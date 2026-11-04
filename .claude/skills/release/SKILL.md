---
name: release
description: Stable 1.x (ESP8266) release. Redirects to the authoritative skill in the otgw-1.x.x worktree; this dev-tree copy holds no process of its own
disable-model-invocation: true
---

# /release - 1.x stable release (redirect)

**This file is a pointer, not the process.** The 1.x release skill lives on the
`otgw-1.x.x` branch, in its worktree, and only that copy is authoritative:

```
<otgw-1.x.x worktree>/.claude/skills/release/SKILL.md
```

Find the worktree and open it, then follow that file from Phase 0 on, with the
arguments given to this command (`ARGUMENTS` below):

```bash
WT=$(git worktree list --porcelain | awk '/^worktree /{p=$2} /^branch refs\/heads\/otgw-1\.x\.x$/{print p}')
echo "$WT"                                   # e.g. D:/Users/Robert/Documents/GitHub/RvdB/wt-otgw-1.x.x
test -f "$WT/.claude/skills/release/SKILL.md" && echo "skill found"
```

Read `$WT/.claude/skills/release/SKILL.md` in full and execute it. Run every
command of that process from `$WT` (its Phase 0 preflight asserts the branch).

Stop and tell the user if:

- no worktree has `otgw-1.x.x` checked out (create one with
  `git worktree add ../wt-otgw-1.x.x otgw-1.x.x` only after the user agrees), or
- the skill file is missing there.

**Never run a 1.x release from this tree or from `dev`.** `dev` is the 2.0.0
ESP32-S3 line. An older copy of this skill told the reader to `git checkout dev`
and `git merge dev` into `main`, which would publish the 2.0.0 line as a 1.x
stable release. A published tag is immutable. For 2.x releases use
`/release-2x` (stable) or `/beta-release-2x` (beta).

Why a redirect instead of a copy: two copies drift. On 2026-11-04 `/release 1.7.9`
from a dev-tree session loaded this stale copy while the 1.x copy had long been
corrected (Robert: the 1.x release skill must come from the otgw-1.x.x worktree,
not from dev).
