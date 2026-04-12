---
name: update-docs
description: Update all OTGW-firmware documentation in one efficient parallel workflow
disable-model-invocation: true
---

# /update-docs - Documentation Update Workflow

Update all project documentation in a single efficient pass. Can be invoked standalone or as part of a release.

## Usage

```
/update-docs                       # Standalone: scope from git changes
/update-docs --release 2.0.1       # Release mode: also generate release documents
/update-docs --scope full          # Force-update all docs regardless of git diff
```

## Writing style rules

- **Never use em dashes** anywhere. Use colons, periods, commas, or parentheses.
- **All release documents must be in English** (international audience).
- **Manual chapters**: English version always, Dutch version always.
- **No emojis** in technical documentation.
- **Concise and correct over long and impressive**: a short accurate sentence beats a verbose vague one.

---

## Phase 1: Scope Detection

Determine what has changed and which documentation is affected.

```bash
# Get previous release tag (authoritative baseline)
PREV_TAG=$(gh release view --json tagName --jq '.tagName' 2>/dev/null || git describe --tags --abbrev=0)

# List changed source files since that tag
git diff --name-only $PREV_TAG..HEAD
```

Categorize changes into subsystems using this mapping:

| Changed files match | Subsystem | Docs affected |
|---|---|---|
| `networkStuff.ino`, `EthernetESP32.ino` | Network | ch06/h06, c4-code-network.md |
| `MQTTstuff.ino`, `mqttha.cfg` | MQTT | ch04/h04, docs/api/MQTT.md, OpenAPI |
| `restAPI.ino` | REST API | ch09/h09, docs/api/openapi.yaml, docs/api/README.md |
| `OTGW-Core.ino`, `OTDirect.ino` | OpenTherm core | ch08/h08, c4-code-otgw-core.md |
| `SAT.ino` | SAT thermostat | ch05/h05, c4-component-smart-thermostat.md |
| `sensorStuff.ino` | Sensors | docs/api/DALLAS_SENSOR_LABELS_API.md |
| `settingStuff.ino`, `OTGW-firmware.h` | Settings/State | ch08/h08, c4-code-settings.md |
| `data/index*.html`, `data/*.js`, `data/*.css` | Web UI | ch03/h03, c4-code-web-assets.md |
| `platformio.ini`, `build.py`, `flash_esp.py` | Build system | ch08/h08 (developer guide) |
| `OTGW-firmware.ino` | Main/boot | ch01/h01 if feature-level |

Build the affected set. If `--scope full` or more than 6 subsystems changed, treat all docs as affected.

**Report to user**: list affected subsystems and which docs will be updated. Proceed without waiting for confirmation unless `--release` mode (release has its own checkpoint).

---

## Phase 2: Parallel Content Updates

Launch agents in background for all affected documentation areas simultaneously. Do NOT wait for one to finish before starting the next.

### 2A: Manual chapters (English + Dutch)

For each affected manual chapter, launch ONE agent that updates both the English and Dutch versions.

**Chapter to file mapping:**

| Chapter | EN file | NL file |
|---|---|---|
| 1 (Introduction/features) | `docs/manuals/en/ch01-introduction.md` | `docs/manuals/nl/h01-introductie.md` |
| 2 (Hardware/install) | `docs/manuals/en/ch02-hardware-setup.md` | `docs/manuals/nl/h02-hardware-installatie.md` |
| 3 (Web interface) | `docs/manuals/en/ch03-web-interface.md` | `docs/manuals/nl/h03-webinterface.md` |
| 4 (Home Assistant/MQTT) | `docs/manuals/en/ch04-home-assistant.md` | `docs/manuals/nl/h04-home-assistant.md` |
| 5 (SAT thermostat) | `docs/manuals/en/ch05-sat-thermostat.md` | `docs/manuals/nl/h05-sat-thermostaat.md` |
| 6 (Network) | `docs/manuals/en/ch06-network.md` | `docs/manuals/nl/h06-netwerk.md` |
| 7 (Troubleshooting) | `docs/manuals/en/ch07-troubleshooting.md` | `docs/manuals/nl/h07-probleemoplossing.md` |
| 8 (Developer guide) | `docs/manuals/en/ch08-developer-guide.md` | `docs/manuals/nl/h08-ontwikkelaarsgids.md` |
| 9 (API reference) | `docs/manuals/en/ch09-api-reference.md` | `docs/manuals/nl/h09-api-referentie.md` |
| 10 (Appendix) | `docs/manuals/en/ch10-appendix.md` | `docs/manuals/nl/h10-bijlagen.md` |

**Agent prompt template for a chapter update:**

> Read the current `[EN file]` and `[NL file]`. Read the git diff for the relevant source files: `git diff [PREV_TAG]..HEAD -- [source files]`. Update both files to reflect the changes accurately. Preserve all existing content that is still correct. The EN file must be in English, the NL file must be in Dutch. Technical terms stay in English in both. Keep the same `## Chapter N:` / `## Hoofdstuk N:` heading format. Write the updated files back.

### 2B: API documentation

If REST API or MQTT changed, launch a single agent to update all API docs in parallel:

**Files to update:**
- `docs/api/openapi.yaml` — OpenAPI 3.1 spec for all `/api/v2/` endpoints
- `docs/api/README.md` — REST API human-readable reference
- `docs/api/MQTT.md` — MQTT topics, payloads, retain flags
- `docs/api/WEBSOCKET_FLOW.md` — only if WebSocket behavior changed
- `docs/api/DALLAS_SENSOR_LABELS_API.md` — only if Dallas sensor API changed

**Agent instructions:**
> Read the current docs in `docs/api/`. Read the git diff: `git diff [PREV_TAG]..HEAD -- restAPI.ino MQTTstuff.ino`. Update the affected API docs to match the current implementation. For openapi.yaml: ensure every endpoint present in `kV2Routes[]` in `restAPI.ino` has a spec entry. Add new endpoints, remove removed ones, update changed response schemas. For MQTT.md: verify topic paths and payload formats match the current `MQTTstuff.ino` publish calls.

### 2C: C4 architecture docs (only if structural changes)

If new files, new components, or new dependencies were added:

**Trigger condition:** `git diff --name-only $PREV_TAG..HEAD | grep -E "\.ino$" | wc -l` is 3 or more, OR a new `.ino` file was added.

Update the relevant `docs/c4/c4-code-*.md` and `docs/c4/c4-component-*.md` files. Do not rewrite from scratch — targeted updates only.

---

## Phase 3: Release Documents (--release mode only)

Only execute this phase when invoked as `/update-docs --release <version>` or when called from `/release`.

### 3A: Gather changes and contributors

```bash
# All commits since previous release (exclude CI noise)
git log $PREV_TAG..HEAD --oneline | grep -v "CI: update version.h"
```

Categorize each commit: new feature, bug fix, internal improvement, breaking change.

Check `docs/adr/` for new or updated ADRs since `$PREV_TAG`.

**Contributors** (3 sources, run in parallel):

1. GitHub: `gh pr list --state merged --search "merged:>$PREV_DATE" --json author,title --jq '.[] | "\(.author.login): \(.title)"'`
2. Discord `#beta-testing` (channel ID `914498730001072149`): messages since `$PREV_DATE`, extract testers
3. Discord `#devs-esp-firmware` (channel ID `924989767966425158`): bug reports and contributors

Strip trailing digits from Discord usernames. Exclude bot IDs and maintainer `384411356616720384`.

### 3B: Generate release documents

**`RELEASE_NOTES_<version>.md`** (at repo root):

Follow the template in `docs/process/RELEASE_PROCESS.md`. Sections:
- Release summary (2-3 sentences)
- What's new (features, with subsystem grouping)
- Bug fixes
- Breaking changes (always explicit: either "none" or list them)
- Upgrade notes
- Known issues
- Contributors

**`RELEASE_GITHUB_<version>.md`** (at repo root):

Concise GitHub release body. Sections:
- Short intro (1 sentence)
- Highlights (bullet list, max 8 items)
- Bug fixes (bullet list)
- Upgrade notes (only if needed)
- Thank you (shoutout to most active contributor + bullet list of others + Discord invite link)

**`docs/BREAKING_CHANGES.md`** update:

Prepend a new version section. Explicitly state whether there are breaking changes or not.

**`README.md`** update:

- Demote current "What's New in v<prev>" to "What was new in v<prev>"
- Add new "What's New in v<version>" section with 4-6 bullet highlights

---

## Phase 4: Docs Folder Cleanup

Always run this phase, regardless of mode. It is fast and idempotent.

### 4A: Archive old release notes

Release notes older than 2 major versions ago belong in an archive. Move files from `docs/releases/` if there are more than 10 release note files. Strategy:

```bash
# Files to potentially archive (older versions, not current or last 2 releases)
ls docs/releases/RELEASE_NOTES_*.md | sort | head -n -4  # keep 4 newest
ls docs/releases/RELEASE_GITHUB_*.md | sort | head -n -4
```

Move archived files to `docs/releases/archive/` (create if needed).

### 4B: Move misplaced files

Identify and move misplaced release documents from the repo root to `docs/releases/`:

```bash
# Release notes/github release files at repo root that belong in docs/releases/
ls RELEASE_NOTES_*.md RELEASE_GITHUB_*.md GITHUB_RELEASE_*.md 2>/dev/null
```

Exception: the CURRENT release's documents (`RELEASE_NOTES_<version>.md`, `RELEASE_GITHUB_<version>.md`) stay at root during the release phase for easy reference, then move after publication.

### 4C: Reviews folder organization

If `docs/reviews/` has more than 15 subdirectories, group older ones by quarter:

```bash
ls docs/reviews/ | head -n -10  # directories to potentially group
```

Group format: `docs/reviews/archive/2026-Q1/` etc. Move only directories older than 90 days. Preserve the 10 most recent as-is.

### 4D: Verify docs/archive

Ensure `docs/archive/MQTT_old.md` is in `docs/archive/` (it is). Check for any other clearly outdated files at `docs/` root level that should be archived:

```bash
ls docs/*.md | grep -v "BREAKING_CHANGES\|upgrade-from"
```

Files that are version-specific or clearly superseded go to `docs/archive/`.

---

## Phase 5: Verify and Report

After all agents complete and cleanup is done:

1. List all files modified in this run: `git diff --name-only`
2. Summarize what was updated per category
3. Report any docs that could not be auto-updated (e.g., architectural decision required)
4. If in `--release` mode: present the full release documents (RELEASE_NOTES, RELEASE_GITHUB, README What's New) for user review before committing

**In standalone mode (`/update-docs` without `--release`):**
- Commit all documentation changes directly: `git add docs/ README.md CHANGELOG.md && git commit -m "docs: update documentation for recent changes"`
- Push to current branch

**In release mode:**
- Do NOT commit yet — the release skill handles the commit as part of Phase 5 execution

---

## Integration with /release

The `/release` skill calls this workflow in its Phase 4. When called from `/release`:
- Skip Phase 5 commit (release skill commits everything together)
- The release skill's CHECKPOINT 1 serves as the review gate for release documents
- Pass `--release <version>` so Phase 3 runs and generates the release documents

---

## Important rules

- **Parallel first**: always launch multiple background agents simultaneously for independent doc areas
- **Read before writing**: every agent must read the current version of the file before updating it
- **Scope discipline**: only update what actually changed — do not rewrite docs for unchanged subsystems
- **Preserve structure**: keep existing heading levels, table formats, and file organization intact
- **Accuracy over completeness**: a correct partial update is better than a comprehensive inaccurate one
- **Dutch chapters must be in Dutch**: never let English creep into NL files except for technical terms
- **OpenAPI spec must match implementation**: always verify endpoints in spec against `kV2Routes[]` in `restAPI.ino`
