---
name: beta-release-2x
description: Publish a 2.x (ESP32-S3) beta prerelease from dev: set _VERSION_PRERELEASE to beta.N, roll the CHANGELOG, build esp32-combo, OTA bench check, annotated tag, draft-first GitHub prerelease, announce in #beta-testing
disable-model-invocation: true
---

# /beta-release-2x: OTGW-firmware 2.x Beta Prerelease Skill

Publish one 2.x beta to field testers. Lightweight and repeatable. Runs on `dev`,
does NOT touch `main` or `release/2.x`, does NOT change `_SEMVER_CORE`.

This is the 2.x counterpart of `/beta-prerelease` (1.x only). It codifies the flow
that shipped v2.0.0-beta.1 on 2026-11-01 (TASK-1504, TASK-1545).

## Usage

```
/beta-release-2x            # next beta: alpha.N -> beta.1, or beta.N -> beta.N+1
/beta-release-2x beta.3     # explicit tag (rare: a skipped/burned tag, see Trap 2)
```

## How this differs from the 1.x flows

| Aspect | `/beta-prerelease` (1.x) | `/beta-release-2x` | `/release-2x` |
|---|---|---|---|
| Branch | `otgw-1.x.x` | `dev` | `release/2.x` |
| CI builds the release | yes (`beta-prerelease.yml`) | **no**, local build + `gh` upload | no |
| Bench check before tag | no | **yes**, OTA on a bench unit | yes |
| GitHub flags | prerelease | prerelease, `--latest=false` | stable (Latest: checkpoint) |
| Discord | `#beta-testing` | `#alpha-testing` zip post + `#beta-testing` announcement | NL + EN support |
| Checkpoints | 1 | 2 (Robert's go, announcement text) | 3 |

`dev` has no tag-triggered workflow (`.github/workflows/build.yml` is PR-only), so
nothing builds or publishes for you. `dev` is the GitHub default branch: without
`--latest=false` GitHub may mark a beta as "Latest" over the 1.x stable.

## Writing style rules

- **Never use em dashes.** Colons, periods, commas, parentheses.
- **All release text in English.** No emojis.
- **Never name the OTGW32 board or the `esp32-otgw32` token publicly** (release
  body, Discord, tag message). The public hardware is the OTGW Classic with an S3 Mini.

## Process

### Phase 0: Preconditions and coordination

1. **CHECKPOINT 1: the 12-hour slots** (Robert 2026-11-02 evening; replaces that day's daily 08:00 rule and its
   "every beta public" morning rule): at most one public beta per 12 hours, at 08:00 or 20:00 local time. Every firmware task keeps its own
   `2.0.0-beta.N` bump and its #alpha-testing zip (alpha track); only the daily release gets a
   GitHub prerelease and a #beta-testing post. Candidate: the newest beta.N that ran at least
   1 h on the bench unit (.63) with MQTT connected, without a restart (bootcount = flash + 2, no
   crashlog, heap not trending down). Robert 2026-11-03: "Soak terugbrengen naar zinvol minimum. Beta
   testers zijn er ook om problemen te ontdekken." (was 8 h). 1 h covers boot, the safeboot rollback
   window, WiFi/MQTT connect and the first hourly timers; slow failures are for the field testers.
   No soak waiver: a slot whose candidate has not finished its 1 h soak waits until it has. Every flash
   of the bench unit restarts the soak, so the last flash before a slot is at least 1 h before it. Skip also when nothing user-visible changed or the candidate failed its soak or a bench
   check. Hotfix exception: a public beta that breaks or crashes units may get its fix out at
   once. No per-release question within this rule; still ask (by Discord DM via the coordinator, one question
   each; never an interactive CLI question, TASK-1829) for anything new to weigh: a new waiver, a failed gate, a breaking change. The
   soak/macOS waiver (TASK-1544) carries over and is stated in every notes file.
   The release is a docs-only commit on top of the candidate (notes bundling every beta.N
   since the previous public beta, CHANGELOG rollover, README link), tagged and built from
   that commit (the beta.7 model); Phase 1/2 go into that commit.
2. Run from the main dev tree, on `dev`: `git rev-parse --abbrev-ref HEAD` prints `dev`.
3. `git fetch origin && git status -sb`: local `dev` equals `origin/dev`.
4. **Freeze dev pushes.** Message every live peer session (`ListAgents`) to hold
   pushes and builds until you send "beta.N tag out". A push during the flow moves
   HEAD and forces a rebuild (the githash is in the image).
5. Previous public release and previous 2.x beta:
   ```bash
   gh api repos/rvdbreemen/OTGW-firmware/releases/latest --jq .tag_name   # stays the 1.x stable
   git tag --list 'v2.*' --sort=-v:refname | head -3
   ```
6. Every alpha tag since the last beta is either published or covered: compare
   `.claude/alpha-build-ledger.local.md` with
   `git log -G"_VERSION_PRERELEASE" <prev-beta-or-first-alpha>..origin/dev -- src/OTGW-firmware/version.h`.

### Phase 1: Narrative (release notes, CHANGELOG, README)

The notes at the tagged commit are what testers read; the release body is copied
from the file at create time.

1. Release notes: `docs/releases/RELEASE_NOTES_<semver>-beta.N.md`. For beta.N+1
   start from the previous beta's file. Required sections: disclaimer (beta, what
   was NOT tested: soak, OS runs), SAT experimental/opt-in, changes since the last
   beta, Before you flash (save settings, USB first flash for a new S3 Mini,
   `/update` with the zip for units on 2.0.0, port 25238 opt-in), BREAKING changes,
   Known issues, How to report.
2. Every claim checked against code (`file:line`) or an ADR. No TODOs, no
   placeholders, no em dashes:
   ```bash
   grep -n -E "TODO|TBD|XXX|—" docs/releases/RELEASE_NOTES_<v>.md
   grep -o -E "\]\([^)]+\)" docs/releases/RELEASE_NOTES_<v>.md | grep -v -E "\(https?://|\(#"   # relative links break on the release page
   ```
3. CHANGELOG `[Unreleased]` holds every firmware change since the last beta
   (`git log --oneline <prev>..HEAD -- src/`). Fill gaps now.
4. README: beta disclaimer and highlights match the build. Docs-only commits may be
   pushed before the freeze.
5. Optional, for a beta with many changes: `/update-docs-2x --release <semver>-beta.N`
   updates the manuals, API and C4 docs and drafts items 1 and 4. It does not commit;
   commit its files with the narrative.

### Phase 2: Version bump + CHANGELOG rollover (one commit)

1. Set the tag:
   - First beta after alphas: `bin/bump-prerelease.sh` only increments the number
     (alpha.597 -> alpha.598). Use instead:
     ```bash
     python scripts/autoinc-semver.py src/OTGW-firmware --prerelease beta.1 \
       --update-all --print-updated > "$TMP/beta-updated.txt"
     ```
     and stage each listed file (the list uses `data\FSexplorer.html` with a
     backslash on Windows: stage `src/OTGW-firmware/data/FSexplorer.html` by name).
   - beta.N -> beta.N+1: `bin/bump-prerelease.sh` (stages its own files).
2. Check: `grep -E "PRERELEASE|SEMVER_FULL" src/OTGW-firmware/version.h`.
3. CHANGELOG: rename `## [Unreleased]` to `## [<semver>-beta.N] - <date>`, add an
   empty `## [Unreleased]` above it, update the link refs at the bottom. This must
   happen BEFORE the tag (memory: CHANGELOG Unreleased rollover).
4. `python evaluate.py --quick`: 0 failed.
5. Stage ONLY the bump files, `CHANGELOG.md` and the tracking task file. Never
   `git add -A`: the dev tree carries peer work.
6. Commit: `chore(release): <semver>-beta.N (TASK-NNN)` (the commit-msg hook needs
   that task file staged).

### Phase 3: Build and bench check

1. One build at a time (machine lock). esp32-combo only:
   ```bash
   OTGW_BUILD_JOBS=4 ./.build-python/python.exe build.py > "$TMP/build-beta.log" 2>&1
   grep -c "Successfully created ESP32S3 image" "$TMP/build-beta.log"   # 2 on a cold build (bootloader + firmware), 1 on a warm one (TASK-1921: the bootloader is cached)
   grep "Build completed successfully" "$TMP/build-beta.log"
   ls -la build | grep "beta.N+<githash>"                               # fresh mtimes
   ```
   Exit 0 alone proves nothing (build.py masks failures).
2. The bench-tester agent (`otgw-bench-tester`, maintainer directive 2026-11-11) does steps 2 to 4;
   request it through the coordinator with the githash and the absolute artifact paths. It saves
   settings, then OTA-flashes a **Classic + S3 Mini** bench unit (the supported public hardware):
   ```bash
   curl -s http://<ip>/api/v2/settings -o "$SCRATCH/settings-<ip>-before.json"
   ./.build-python/python.exe scripts/ota_flash.py --host <ip> \
     --app build/OTGW-firmware-esp32-combo-<v>.ino.bin \
     --fs  build/OTGW-firmware-esp32-combo-<v>.littlefs.bin
   ```
   Exit 0 and `settings: ... changed: none`. A settings diff is a finding unless a
   task in this beta adds or removes that key on purpose.
3. Health: `GET /api/v2/device/info` shows the new `fwversion`, PIC detected;
   after at least 5 minutes the bootcount has not moved (the flash itself adds 2:
   safeboot handover + new firmware), no crashlog (`GET /api/v2/device/crashlog`).
4. If possible also run the user route: `/update` with the release zip in a browser.
5. A failed bench check publishes nothing: fix, new commit, rebuild.

### Phase 4: Push, tag, draft release

1. `git fetch origin`; `origin/dev` must still be the parent of your commit.
   Then `git push origin HEAD:dev`.
2. Annotated tag on the bump commit:
   ```bash
   git tag -a v<semver>-beta.N <commit> -m "<semver>-beta.N: <one line>"
   git push origin v<semver>-beta.N
   git rev-parse --short "v<semver>-beta.N^{}"    # must be the bump commit
   ```
3. SHA256SUMS over the assets (in the scratchpad, not in build/):
   flash zip, `.ino.bin`, `.littlefs.bin`, `-merged-full.bin`, `.elf`
   (lets a crash be decoded later), `OTGW-safeboot-<v>.bin`.
4. Draft prerelease:
   ```bash
   gh release create v<semver>-beta.N --draft --prerelease --latest=false --verify-tag \
     --title "v<semver>-beta.N (<short theme>)" \
     --notes-file docs/releases/RELEASE_NOTES_<semver>-beta.N.md \
     build/OTGW-firmware-esp32-combo-<v>-flash.zip build/...ino.bin build/...littlefs.bin \
     build/...-merged-full.bin build/....elf build/OTGW-safeboot-<v>.bin "$SCRATCH/SHA256SUMS"
   gh release view v<semver>-beta.N --json isDraft,isPrerelease,assets \
     --jq '{d:.isDraft,p:.isPrerelease,a:[.assets[]|"\(.name) \(.size)"]}'
   ```
   Sizes must equal `build/`. The flash zip carries the capture tools and flash
   scripts (check with `zipfile`). Anything off: stop and ask, do not publish.

### Phase 5: Publish and verify

1. A draft's URL is `releases/tag/untagged-<id>`. Publish with:
   `gh release edit untagged-<id> --draft=false --prerelease --latest=false --tag v<semver>-beta.N`
2. Verify:
   ```bash
   gh release view v<semver>-beta.N --json isDraft,isPrerelease,tagName --jq .
   gh api repos/rvdbreemen/OTGW-firmware/releases/latest --jq .tag_name   # still the 1.x stable
   curl -sL -o "$TMP/dl.zip" "https://github.com/rvdbreemen/OTGW-firmware/releases/download/v<semver>-beta.N/<zip name with + as %2B>"
   sha256sum "$TMP/dl.zip"                                                # equals SHA256SUMS
   ```
3. Message the peers: "beta.N tag out", dev is free.

### Phase 6: Publish the zip and announce

1. **#alpha-testing zip post** (every tag is published): the post script refuses
   unless the zip githash is HEAD. If HEAD moved after the tag, post from a temp
   worktree at the tag:
   ```bash
   git worktree add --detach ../wt-beta-post <tag-commit>
   OTGW_REPO="$(cd ../wt-beta-post && pwd)" bash scripts/discord-post-build.sh \
     -m "<short note + release link>" "$PWD/build/<flash zip>"
   git worktree remove ../wt-beta-post && git worktree prune
   ```
   Add the row to `.claude/alpha-build-ledger.local.md`.
2. **#beta-testing announcement** (sent without showing it first under the 12-hour slot rule
   above; show it first only when it announces something new to weigh).
   Channel `914498730001072149`, via `mcp__discord-mcp__send_message` (the webhook
   only reaches #alpha-testing; if the MCP is down, ask Robert to `/mcp` reconnect).
   Under 2000 characters. Content, for testers coming from 1.x:
   - title line with the version, the release link
   - what 2.x is (ESP32-S3, S3 Mini replaces the Wemos, PIC stays, combo zip)
   - 4-6 highlights, SAT experimental and off by default
   - the beta disclaimer: what was not tested, stay on 1.x if you need stability,
     going back to 1.x means the Wemos back in
   - before you flash: save settings, USB first flash for a new S3 Mini
     (`flash_otgw.bat` / `./flash_otgw.sh`, factory mode), `/update` with the zip on
     a 2.0.0 unit, port 25238 opt-in, read Breaking changes + Known issues
   - how to report (here or a GitHub issue, the capture upload zip)
   After sending, read the channel back and check the text arrived intact.

### Phase 7: Close out

1. Tracking task: Final Summary names the evidence per AC (build lines, bench
   numbers, release URL, SHA match, post IDs). Waived ACs stay unchecked, the waiver
   is named, and the open work moves to a follow-up task. Then Done.
2. Commit + push the backlog files (docs-only).

## Post-publication corrections

Assets are immutable after publish; the body is not.
- Fix the release-notes file on `dev`, commit, push, then
  `gh release edit v<semver>-beta.N --notes-file docs/releases/RELEASE_NOTES_<v>.md`.
- A broken asset cannot be replaced: ship beta.N+1.

## Known traps

1. **Immutable releases**: no asset upload, replace or delete after publish. Hence draft first.
2. **A deleted release burns the tag name forever.** Bump to the next beta number and say so in the commit message.
3. **`--latest=false` is required** on every 2.x beta (dev is the default branch).
4. **Moving HEAD** (a peer push, a rebase) after the build means the post script refuses that zip: rebuild, or post from a worktree at the tag commit.
5. **Stale narrative at the tag** is permanent in the tagged tree: finish Phase 1 before Phase 2.
6. **`bin/bump-prerelease.sh` cannot change the word** (alpha -> beta): use `autoinc-semver.py --prerelease`.

## Important rules

- Never push to `main` or `release/2.x` from this flow. Never force-push.
- Build and evaluator gates are mandatory; a red gate means no tag.
- Do not flash the PIC. Clear any test HTTP password before ending.
- Never print the webhook URL.
