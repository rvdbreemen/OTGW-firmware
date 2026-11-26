---
name: beta-release-2x
description: Publish a 2.x (ESP32-S3) beta prerelease from dev: set _VERSION_PRERELEASE to beta.N, roll the CHANGELOG, build esp32-combo, OTA bench check, annotated tag, draft-first GitHub prerelease, announce in #beta-testing
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
- **OTGW32 may be named publicly** (Robert, 2026-11-22): call it a prototype board. The
  main public hardware is still the OTGW Classic with an S3 Mini.

## Process

### Phase 0: Preconditions and coordination

1. **CHECKPOINT 1: hourly slots** (Robert 2026-11-25 22:50; replaces the four-slot rule of 22:20 and the 12-hour 08:00/20:00 rule of
   2026-11-02): a public beta may go out at any full hour from 07:00 to 23:00, at most one beta per hour, and only with a candidate that
   finished its 60 min soak; an hour without a soaked candidate is skipped. Every firmware task keeps its own
   `2.0.0-beta.N` bump and its #alpha-testing zip (alpha track); only the daily release gets a
   GitHub prerelease and a #beta-testing post. Candidate: origin/dev HEAD at compose time (T-75, see
   below), published as the release build that ran at least 1 h on the bench unit (.63) with MQTT
   connected, without a restart (bootcount = flash + 2, no crashlog, heap not trending down). Robert 2026-11-03: "Soak terugbrengen naar zinvol minimum. Beta
   testers zijn er ook om problemen te ontdekken." (was 8 h).
   **Soak levels (Robert, 2026-11-25; first a tiered version, corrected by him the same evening at 22:00).** Three levels:
   15 min per TASK on the bench (the developer's per-task bench run); **60 min on the public beta release build** (this skill; the
   soak end is the publish moment); and for the FINAL release a long run of more than 24 h (see `/release-2x`). There is no shorter
   sanity soak for a beta release build and no separate "hours on .61" level.
   - Why 60 min and not less: the run must pass uptime 3600 s, because the hourly block (`OTGW-firmware.ino:949`, uptime above 3600 s plus
     NTP) runs only then; a leak in such a block killed 1.x units at 82 min. The old reason "1 h covers the safeboot rollback window"
     is wrong: the health gate confirms the app after 30 s (`OTGW-firmware.ino:1206`).
   - **Attribution** (Robert 2026-11-25 22:50: bundling is the default; this replaces "one MQTT/TCP/heap cause per build"): one commit per
     task, behaviour changes behind runtime flags, so a failed soak can be traced to one commit. Every candidate has a
     halving fallback: drop one commit, rebuild, soak again.
   No soak waiver: a slot whose candidate has not finished its 60 min soak waits until it has. Every flash
   of the bench unit restarts the soak, so the last flash before a slot is at least 1 h before it. Skip also when nothing user-visible changed or the candidate failed its soak or a bench
   check. Hotfix exception: a public beta that breaks or crashes units may get its fix out at
   once. No per-release question within this rule; still ask (by Discord DM via the coordinator, one question
   each; never an interactive CLI question, TASK-1829) for anything new to weigh: a new waiver, a failed gate, a breaking change. The
   soak/macOS waiver (TASK-1544) carries over and is stated in every notes file.
   The release docs (notes bundling every beta.N since the previous public beta, CHANGELOG rollover, README link; Phase 1/2) go INSIDE the
   candidate commit, before the build (Robert 2026-11-25 22:20), so the soaked binary is the published binary: no docs-only release commit
   on top of the candidate, no second build and no second soak. The tag goes on that candidate commit. Non-destructive 15 min task checks
   run during the 60 min soak; destructive ones run first, before the soak starts.
   **Who starts it** (Robert 2026-11-14, standing permission: "Ik wil dat je de release van een beta
   gewoon uitvoert"): the coordinator invokes this skill itself at T-75 (compose + push the release commit), and
   publishes at T once the release build passed its soak; Robert does not have to type it. Stable releases (`/release-2x`, `/release`) keep the
   per-action yes.
   **Compose shortly before, publish at the slot** (Robert 2026-11-15: "de release kort van
   tevoren samenstelt. Zodat elke user de nieuwst mogelijke release krijgt. Geen vertraging na de
   soak van een 1 uur."). The soak runs on the RELEASE BUILD itself, so the soak end is the
   publish moment. Timeline for slot T (any full hour 07:00-23:00). When the candidate is not built yet, follow the steps below. When the candidate was
   built and soaked earlier, with its release docs already inside the commit, publish at the first slot after its 60 min soak passed (tag, draft,
   verify, publish, posts; no rebuild):
   - all day: the notes draft and the Discord post draft are kept current after every push
     (one owner, the comms session), so composing takes minutes, not an hour;
   - **T-75: compose** Phase 1 + 2 together with the newest firmware that passed its own bench, as the candidate commit on top of
     origin/dev HEAD at that moment, and **push the candidate commit to `dev` at once**. No freeze: the release is that commit, not
     "HEAD at T". Pushes after it simply land in the next release.
   - **T-70 to T-62: build** that release commit (Phase 3.1, from a worktree at the commit if
     the tree moved on).
   - **T-62: flash** the release build on the bench unit (.63) and run the Phase 3 checks: this
     flash starts the 1 h soak (soak end = T-2 at the latest).
   - **T: soak verdict, then at once** tag THE RELEASE COMMIT (not HEAD; `git tag -a ... <commit>`),
     draft, verify, publish, posts (Phase 4.2-6). No rebuild after the soak: the soaked binary is
     the published binary (sha256 of the bench artifact = the release asset).
   If the build or the bench check fails, publish at the earliest time a 1 h soak of the fixed
   build can finish, not at the next slot; skip the slot only if the fix needs new firmware.
   **No dev freeze** (Robert 2026-11-15: "Waarom de lange freeze periodes"). Only the bench unit
   is held, from the flash until the verdict. Phase 4.1's push is the T-75 push above; the
   "origin/dev must still be the parent" check and the "beta.N tag out" release message to peers
   fall away.
2. Run from the main dev tree, on `dev`: `git rev-parse --abbrev-ref HEAD` prints `dev`.
3. `git fetch origin && git status -sb`: local `dev` equals `origin/dev`. When the main tree is
   behind with local edits that conflict (`.wolf/*`, a peer's task file): save them first
   (`git diff -- <files> > "$SCRATCH/maintree-local-edits.patch"`), `git checkout -- <files>`,
   then `git reset --keep origin/dev`. A local commit that `git cherry -v origin/dev dev` marks
   with `-` is already upstream and may go. Name the patch path in the session report.
4. **No push freeze.** Tell the peers the release commit id once it is pushed (T-75), so the
   comms session knows where the release ends. Later pushes do not touch the release: it is
   tagged on that commit and built from it (a worktree at the commit when the tree moved on).
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
   **Every update goes into the notes and the CHANGELOG, not only the highlights** (Robert
   2026-11-14: "Zorg ervoor dat alle updates in de releasenotes staan. Niet alleen de top 4."):
   one line per user- or tester-visible change, with its TASK number. Cross-check: the task IDs in
   `git log --format='%h %s' <prev-tag>..origin/dev -- src/ scripts/ build.py platformio.ini`
   against the IDs in the new section; every ID that is left out is named in the tracking task
   with its reason (internal only, CI only, no behaviour change).
   **Embargo:** a fix under embargo (e.g. TASK-1934 until v1.7.10) gets one neutral line and
   nothing next to it that hints at the issue (no paths, no example requests, no "security").
4. README: beta disclaimer and highlights match the build. Docs-only commits may be
   pushed at any time.
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
   - **Normal slot release (candidate already at beta.N in `version.h`):** no bump. The release
     docs (notes, CHANGELOG rollover, README, task file) are part of the candidate commit (since 2026-11-25 22:20) and the tag carries the
     candidate's number (beta.237, beta.250). Commit as `docs(release): v<semver>-beta.N release
     notes, CHANGELOG rollover and README (TASK-NNN)`.
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
   On Windows run `build.bat` by full path from PowerShell (`cmd.exe /c "<repo>uild.bat"`):
   `./build.sh` in Git Bash can stop at once with "Python 3 not found" (2026-11-14). Kill
   `llama-server` and keep the 2 s guard running from start to BUILD END; check commit charge
   first. A build that outlives the 10-minute foreground limit moves to a background shell: keep
   the session busy until it ends, then check the log, not the exit code. Afterwards restore the
   build stamp (`git checkout -- src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash`).
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
   **MQTT preflight and PASS criterion** (Robert, DM 1575388089852108851, 2026-11-26): before the soak clock starts the unit reports `mqttconnected=true`
   against a broker the bench owns (`otgw-bench-mqtt`, started per run, removed afterwards); the production NAS broker is not a soak dependency. If it is false
   at the start, switch to the bench broker then (save `/api/v2/settings` first, restore after, record the switch in the board file). During the soak at least
   99 % of the health samples must show `mqttconnected` true; a lower share fails the soak. Details: `docs/guides/agent-build-and-bench.md` (Soak preflight).
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
   **Body limit:** GitHub refuses a body over 125000 characters (HTTP 422 "body is too long"),
   and the cumulative notes file passes that (beta.250: 163 KB). Then build the body from the file:
   the header up to and including "What changed since <previous beta>", a short "Earlier changes"
   section that links the full file at the tag
   (`https://github.com/rvdbreemen/OTGW-firmware/blob/v<semver>-beta.N/docs/releases/RELEASE_NOTES_<semver>-beta.N.md`),
   then every section from "SAT is experimental and opt-in" to the end. Check its size
   (`wc -c`) and that it has no em dash, and pass it with `--notes-file "$SCRATCH/body.md"`.

### Phase 5: Publish and verify

1. A draft's URL is `releases/tag/untagged-<id>`. Publish with:
   `gh release edit untagged-<id> --draft=false --prerelease --latest=false --tag v<semver>-beta.N`
   **Every edit of a draft (body, assets) gives it a NEW `untagged-<id>` URL**, and the API then
   reports `tag_name` as that untagged name; the old URL returns "release not found" (beta.401,
   2026-11-20). After any edit, publish by release id instead, which also sets tag and target
   explicitly:
   ```bash
   RID=$(gh api repos/rvdbreemen/OTGW-firmware/releases --jq '.[] | select(.draft and .name|startswith("v<semver>-beta.N")) | .id')
   gh api -X PATCH repos/rvdbreemen/OTGW-firmware/releases/$RID -f tag_name=v<semver>-beta.N \
     -f target_commitish=<release-commit-sha> -F draft=false -F prerelease=true -f make_latest=false
   ```
   When you correct a phrase in the body, check it occurs once: the cumulative notes repeat older
   disclaimer lines, and a global replace also rewrites the history of earlier betas.
2. Verify:
   ```bash
   gh release view v<semver>-beta.N --json isDraft,isPrerelease,tagName --jq .
   gh api repos/rvdbreemen/OTGW-firmware/releases/latest --jq .tag_name   # still the 1.x stable
   curl -sL -o "$TMP/dl.zip" "https://github.com/rvdbreemen/OTGW-firmware/releases/download/v<semver>-beta.N/<zip name with + as %2B>"
   sha256sum "$TMP/dl.zip"                                                # equals SHA256SUMS
   ```
3. Message the peers: "beta.N tag out", dev is free. The bench unit stays on the release build
   overnight unless the next slot needs a newer candidate soaked on it (the next slot then skips
   when nothing newer had its hour there).

### Phase 6: Publish the zip and announce

1. **#alpha-testing: zip post or note** (every tag is published once, TASK-1809).
   First look the key up in the machine-wide post ledger:
   ```bash
   P="${LOCALAPPDATA:-$HOME/.local/share}/OTGW-capture"
   grep -x "<semver>-beta.N" "$P/published-tags.txt"; grep -P "\t<semver>-beta.N\t" "$P/published-posts.tsv"
   ```
   - **Key found** (the normal case: the release commit is docs-only on top of the
     candidate, so it carries the candidate's beta.N, and the candidate's developer
     already posted that zip on the alpha track): do NOT run the post script. It
     refuses with "prerelease ... was already published", in any phase, before or
     after the GitHub publish; the refusal comes from this local ledger, not from
     GitHub (beta.174, beta.224 and beta.237 all hit or skipped it). Instead the
     coordinator posts a short #alpha-testing note: "beta.N is now the public beta",
     the release link, and the message id of the earlier zip post from
     `published-posts.tsv`. The firmware in that zip equals the release build except
     the githash string (the release commit changes only docs).
   - **Key not found** (the release commit carries its own bump, e.g. the first beta
     after the alphas): post the zip as below. The post script refuses unless the zip
     githash is HEAD. If HEAD moved after the tag, post from a temp worktree at the tag:
   ```bash
   git worktree add --detach ../wt-beta-post <tag-commit>
   OTGW_REPO="$(cd ../wt-beta-post && pwd)" bash scripts/discord-post-build.sh \
     -m "<short note + release link>" "$PWD/build/<flash zip>"
   git worktree remove ../wt-beta-post && git worktree prune
   ```
   Add the row to `.claude/alpha-build-ledger.local.md` (for a note: "zip already
   posted as alpha beta.N" and both message ids).
2. **#beta-testing announcement** (sent without showing it first under the hourly-slot rule
   above; show it first only when it announces something new to weigh). One post per release; when two betas go out within 3 h, they may be
   combined in one message, newest release link first.
   Channel `914498730001072149`, via `mcp__discord-mcp__send_message` (the webhook
   only reaches #alpha-testing). If the MCP is down, post through the Discord REST API with the
   bot token from the user environment (`DISCORD_TOKEN`, header `User-Agent: DiscordBot (local, 1.0)`,
   never print the token) and read the message back by its id.
   **Every change since the previous #beta-testing release post** (Robert 2026-11-15: "de
   releases notes op discord altijd alle wijzigingen sinds laatste discord publicatie laten
   zien"). Not highlights: the full list. Scope = every beta.N since the previous public beta
   that was announced in #beta-testing (find that post: read the channel back for the last
   bot message with a `releases/tag/v2.0.0-beta.` link; a skipped slot means its betas are
   still unannounced and belong in this post). Source = the "What changed since" section of
   the notes file plus the CHANGELOG `[<semver>-beta.N]` section; one plain-language line per
   user- or tester-visible change with its TASK number, grouped by beta.N or by theme;
   internal-only or CI-only changes may be left out but are counted ("plus N internal
   changes"). Cross-check: every TASK id in the CHANGELOG section is in the post or in that count.
   Discord caps a message at 2000 characters: split into numbered parts ("1/3", "2/3", ...),
   each under 2000, sent in order; part 1 carries the title line and the release link, the
   last part the disclaimer, before-you-flash and how-to-report blocks.
   Content, for testers coming from 1.x:
   - title line with the version, the release link, and "changes since beta.<previous announced>"
   - what 2.x is (ESP32-S3, S3 Mini replaces the Wemos, PIC stays, combo zip)
   - the full change list as above, SAT experimental and off by default
   - BREAKING changes since the previous announced beta, each on its own line
   - the beta disclaimer: what was not tested, stay on 1.x if you need stability,
     going back to 1.x means the Wemos back in
   - before you flash: save settings, USB first flash for a new S3 Mini
     (`flash_otgw.bat` / `./flash_otgw.sh`, factory mode), `/update` with the zip on
     a 2.0.0 unit, port 25238 opt-in, read Breaking changes + Known issues
   - how to report (here or a GitHub issue, the capture upload zip)
   After sending, read the channel back and check the text arrived intact (every part, in order).
   Record the message ids of all parts in the tracking task; the next release starts its
   change list from this post.

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
6. **"prerelease ... was already published"** from the post script means the beta.N key is in the local post ledger (Phase 6.1), not that GitHub refused anything. Moving the post before the publish does not help; post the note instead (TASK-1809).
7. **`bin/bump-prerelease.sh` cannot change the word** (alpha -> beta): use `autoinc-semver.py --prerelease`.
8. **Release body over 125000 characters** is refused at draft time: use the trimmed body of Phase 4.4.
9. **Tag the release commit, not HEAD**: with no freeze, HEAD has usually moved on by T. `git tag -a v<semver>-beta.N <release commit>` and check `git rev-parse --short "v<semver>-beta.N^{}"` equals it.

## Important rules

- Never push to `main` or `release/2.x` from this flow. Never force-push.
- Build and evaluator gates are mandatory; a red gate means no tag.
- Do not flash the PIC. Clear any test HTTP password before ending.
- Never print the webhook URL.
