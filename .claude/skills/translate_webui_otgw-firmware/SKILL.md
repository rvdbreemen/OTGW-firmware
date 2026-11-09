---
name: translate_webui_otgw-firmware
description: Add a new language to the OTGW-firmware v2 web UI, or bring an existing language up to date, with a heating glossary, two adversarial reviews and an editor pass. Input is the language (ISO 639-1 code, e.g. "fr", optionally "fr refresh" or "nl sync"). Web UI only; firmware, MQTT and API stay English. Use when Robert asks for a new web UI language, for missing translations after new UI text, or for a review of an existing pack.
---

# translate_webui_otgw-firmware

Turns one language into a reviewed, shipped web UI pack. Built from the TASK-1764 run of
2026-11-09 (English, Dutch, German, Greek; 1485 keys).

## Input

`/translate_webui_otgw-firmware <code> [add|sync|refresh]`

| mode | when | what happens |
|---|---|---|
| `add` (default for a new code) | the language is not in `I18N_LANGS` | wires the language (I18N_LANGS, menu entry, flag SVG, CSS rule) and translates every key |
| `sync` | new English UI text landed | translates only keys missing from the pack, drops keys English no longer has |
| `refresh` | the pack exists but reads poorly | re-translates on top of the existing pack, then reviews |

Ask once for the native name if you are not sure of it (e.g. Français, Español, Ελληνικά). Refuse
`add` for a code that already exists; suggest `sync` or `refresh`.

## Hard rules (lessons from TASK-1764)

1. **Web UI only** (Robert, 2026-11-09: "translation needs to be of the WebUI only", "leave backend
   firmware in English"). Touch only `src/OTGW-firmware/data/v2.html`, `v2.js`, `v2.css`,
   `data/flags/` and `i18n/`. Texts the firmware sends at runtime stay as received.
2. **No parallel webui tests.** Never pass several `tests/webui` files or the folder to one
   `node --test`: each test opens a headless Chrome and they leak (TASK-1765). One file per call,
   count `chrome.exe` (profiles `%TEMP%\otgw-cdp-*`) before and after. The workflow's agents are
   forbidden to run Chrome at all; the page tests and screenshots are yours, one at a time.
3. **Keep English as the default** with browser-language detection (TASK-1689). Keep product and
   protocol names, MsgIDs, commands and units in English.
4. Pattern is ADR-219/220/221 (t() helper, i18next v4 JSON, gzipped on LittleFS). Adding a
   language needs no new ADR.
5. Work in a lane worktree from `origin/dev` (`docs/guides/worktrees.md`); take the backlog task
   first (`backlog task edit <n> -s "In Progress" -a @claude`).

## Steps

1. **Task and lane.** Find or create the backlog task (`backlog search "<language> web UI"`).
   Create the worktree and branch from `origin/dev`.
2. **Run the workflow** (saved as `.claude/workflows/translate-webui-language.js`):
   ```
   Workflow({ name: 'translate-webui-language',
              args: { code: 'fr', name: 'French', nativeName: 'Français', mode: 'add',
                      wt: '<worktree path>', work: '<scratchpad>/i18n-<code>' } })
   ```
   Phases: Prepare (English source from `I18N_EN`, glossary column for the language in
   `i18n/glossary.md`, wiring for `add`), Translate (opus), Review (two adversarial reviewers:
   native tone + heating terminology; clarity for regular people + placeholders/plurals/length),
   Editor (judges every finding against the code, applies the accepted ones), Verify (static only).
3. **Read the editor's rejections yourself.** Reviewers can be wrong about what a setting does;
   the TASK-1764 editors rejected findings that contradicted the firmware (e.g. the setback
   temperature is the fail-safe flow setpoint, not an "away" temperature). Spot-check 20 strings
   yourself in the context of the page.
4. **Page tests, one file at a time** (Chrome count before/after):
   `node --test tests/webui/lang-menu.test.mjs`, then `i18n-lang`, then `i18n-no-hardcoded`.
   For `add`, extend `lang-menu.test.mjs` so the new entry and its flag file are asserted.
5. **Screenshots** with synthetic data: Home, Monitor, Settings, Advanced in the new language,
   desktop light and 375 px dark; one Chrome at a time. Look for overflow on buttons, tabs and tiles.
6. **LittleFS fit is a hard check**: report the pack's `.json.gz` size, `v2.html.gz` and the
   free space against the 768 KiB (0xC0000) LittleFS partition.
7. **Ship it** per CLAUDE.md: `bin/bump-prerelease.sh` (data/ is a firmware path), one commit with
   the task file staged, build esp32-combo with `build.bat`, OTA a bench unit (save
   `/api/v2/settings` first), 15-min soak, open the web UI on the device in the new language,
   push after the coordinator's go, `bash scripts/discord-post-build.sh -m "..."`, material to the
   comms agent (mention the language and the flag menu).
8. **Native reviewer.** Ask a native-speaking tester to read the pack (TASK-1724: a third
   language wanted a named native reviewer). File their corrections as a `refresh` run.

## Known exceptions kept English (TASK-1764)

- Monitor > Graph window chip "1 hour": the graph code reads the chip text back by regex.
- Mission control heading "· ws ● live".
- Runtime texts from the firmware/API, log lines, OT frame dumps.
