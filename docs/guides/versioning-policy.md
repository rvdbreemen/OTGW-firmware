# Versioning policy (agents)

Split out of CLAUDE.md by TASK-1631. The git hooks in `.githooks/` enforce it.

Field-test users on Discord identify which build they are running by the prerelease tag in `_VERSION_PRERELEASE` (e.g. `alpha.6`, `beta.23`). Every commit that changes firmware behaviour must ship under its own prerelease tag so a tester can A/B two builds and report which one regressed. Implicit batching ("multiple fixes shipped under one tag") destroys that signal — so the policy is enforced mechanically.

**What requires a prerelease bump** — any staged path under:

- `src/OTGW-firmware/**` (the `.ino`/`.h`/`.cpp` sources and the LittleFS data assets), **except** `src/OTGW-firmware/version.h` itself;
- `src/libraries/**` (vendored libs that link into the firmware).

**What does NOT require a bump** (commit can land with no version change):

- `*.md`, `docs/**`, `backlog/**`, `.claude/**` — pure documentation / project metadata;
- `scripts/**`, `bin/**`, `.githooks/**` — host-side tooling not flashed to the device;
- top-level `.py` / `.sh` / `.bat` — build orchestration, not firmware.

**How to bump.** From the project root:

```bash
bin/bump-prerelease.sh        # bumps tag, syncs ALL version banners, stages everything it touched
git commit -m "..."
```

The bump script updates `version.h`, `data/version.hash`, **and every
source-file version banner** (via `autoinc-semver.py --update-all`), then
stages all of them itself. Do NOT hand-stage only `version.h` + `version.hash`
— that leaves the ~43 banner updates uncommitted and the headers drift out of
sync with the version file (observed: banners at alpha.165 while version.h was
at alpha.170). The whole bump lands in one commit, banner churn included.

The helper requires the current tag to match `^[a-zA-Z]+\.[0-9]+$` (so `alpha.6` → `alpha.7`, `beta.23` → `beta.24`). If you need to change the alpha/beta word itself, edit `version.h` by hand and let `scripts/autoinc-semver.py` handle the rest.

**Enforcement.** `.githooks/pre-commit` checks the staged diff. If any triggering path is staged but `_VERSION_PRERELEASE` is unchanged, the commit is rejected with a clear error. The hook is wired via `core.hooksPath = .githooks` (already set on this worktree).

**Bypass for one commit.** Use sparingly — e.g. an emergency rollback, or a commit that only re-formats whitespace and genuinely does not need its own tag:

```bash
OTGW_BUMP_HOOK_DISABLE=1 git commit -m "..."
```

If you find yourself reaching for the bypass routinely, the rule is wrong, not the commit. Open a backlog task to revisit the policy.
