# UX review: the first five minutes of a new user (README and release notes), 2026-11-20

Reviewer: otgw-ux-designer. Task: TASK-2969. Suggested by the coordinator. Triage: the coordinator; owners are likely otgw-docs (texts) and BlueFlame (release-note style). Nothing here is a decision.

## Scope and method

- **Who:** a new visitor who has an OpenTherm Gateway (most likely with the ESP8266 module) and lands on the GitHub repository page. The question is what that person reads in the first five minutes, and whether they end up with the right firmware and a working unit.
- **What:** `README.md` on the default branch, the GitHub release list, and the newest release notes (`docs/releases/RELEASE_NOTES_2.0.0-beta.401.md`, which the README links). Read at origin/dev 335e16db7 on 2026-11-20.
- **Facts checked:** the default branch and the latest release via `gh repo view` (`defaultBranchRef: dev`, `latestRelease: v1.7.9`, published 2026-11-04), and the release list via `gh release list` (the six newest entries are 2.0.0 betas, marked Pre-release). Settings names against `v2.js` and the firmware.
- Severity 0 to 4, effort S/M/L, as in the other reviews.

## Summary

| ID | Where | Heuristic | Sev | Effort | Evidence |
|---|---|---|---|---|---|
| R1 | Repository landing | H2 match, H4 consistency | 3 | S | `README.md:1-9`; default branch `dev`; latest release v1.7.9 |
| R2 | README Quick Start | H5 error prevention | 3 | S | `README.md:148-160` |
| R3 | README Quick Start, MQTT | H2 accuracy | 2 | S | `README.md:171`, `:199`; manual ch03:114; `v2.js:4193-4205` |
| R4 | Release notes | H8 minimalism | 3 | M | `RELEASE_NOTES_2.0.0-beta.401.md:7-36` |
| R5 | README order | H8, H6 | 2 | S | SAT section `README.md:90-146` before Quick Start `:148` |
| R6 | README SAT links | H2 | 2 | S | `README.md:126`, `:143-144` link `backlog/docs/` |
| R7 | README Quick Start, details | H2 accuracy | 1 | S | `README.md:163`, `:167` |

## Findings

### R1. The landing page speaks 2.0.0 beta, the "latest" badge says 1.7.9, and the 1.x path is a detour (H2, H4, sev 3)

- **What a visitor sees:** the title "OTGW-firmware v2.0.0" (`README.md:1`), next to a release badge that by default shows the newest non-prerelease, v1.7.9 (shields.io's default for `github/v/release`; the badge was not fetched, this is from its documented default). Then a disclaimer: 2.0.0 is a beta; "if you need a gateway that just works, stay on the 1.x line" (`:7-9`).
- **The detour:** the 1.x link goes to the `otgw-1.x.x` branch and to "the published releases" (`:9`). That release list opens with six 2.0.0 betas (Pre-release) above v1.7.9. A 1.x user must scroll past them and not pick a beta zip, which does not run on their ESP8266 anyway (the beta.401 notes say so, `:36`).
- **Why it matters:** most visitors own an ESP8266 gateway. Everything else on the page (hardware, Quick Start, MQTT) describes 2.0.0 on an ESP32-S3.
- **Proposal:** a two-way choice right under the title, before the disclaimer:

  > **Which firmware do you need?**
  > - Your gateway has a **Wemos D1 mini or NodeMCU (ESP8266)**: use the stable 1.x firmware → [latest stable release](https://github.com/rvdbreemen/OTGW-firmware/releases/latest) (v1.7.9) and its [README](https://github.com/rvdbreemen/OTGW-firmware/tree/otgw-1.x.x).
  > - Your gateway has a **LOLIN S3 Mini (ESP32-S3)**, or you want to try 2.0.0: read on. 2.0.0 is in beta.

  `/releases/latest` lands on v1.7.9 because GitHub skips prereleases there, so the 1.x user never sees a beta zip.
- **Proposed AC:** the README's first section after the badges sends ESP8266 users to `/releases/latest` and the 1.x README in one step; the 2.0.0 content follows below it.

### R2. Quick Start step 1 is a factory wipe, and a 1.x user's hardware cannot take it (H5, sev 3)

- **What:** step 1 tells the reader to run `flash_otgw.bat` / `./flash_otgw.sh` and adds "Without options this is a factory install: it erases the flash" (`README.md:150-160`). That is the CLI review's C1 (TASK-2963) seen from the README. Nothing before step 1 asks which module the reader has; the module swap from the ESP8266 is in another section (`:75-88`).
- **Proposal:** a step 0, "Check your hardware: 2.0.0 needs a LOLIN S3 Mini (ESP32-S3). Coming from an ESP8266? Swap the module first: see Migrating from ESP8266." Step 1 then says: "New unit or a clean start: `flash_otgw.bat` (erases WiFi and settings). Unit already on 2.0.0: update over WiFi from the web UI instead." Once C1 is built, step 1 matches the tool's menu.
- **Proposed AC:** Quick Start starts with a hardware check and names the erase before the command; the update path (WiFi) is offered next to the factory path.

### R3. "Enable HA Discovery" is a setting that 2.0.0 does not have (H2, sev 2)

- **What:** Quick Start step 4 says "Enable **HA Discovery**" (`README.md:171`), and the MQTT table lists "HA Discovery … checked" (`:199`); the manual has the same row (`docs/manuals/en/ch03-web-interface.md:114`). The v2 MQTT settings are Enable MQTT, Broker, Port, Username, Password, Top topic, Unique id, HA discovery prefix and the publishing options (`v2.js:4193-4205`). The firmware has no discovery on/off key either (only `mqtthaprefix`, `settingsHelp.h:35`). A new user looks for a checkbox that is not there.
- **Proposal:** step 4: "Go to Settings › MQTT, switch on **Enable MQTT** and fill in the broker. Home Assistant discovery is always on; the **HA discovery prefix** must match Home Assistant (default `homeassistant`)." Remove the "HA Discovery" row from the README table and from manual ch03:114 (otgw-docs).
- **Proposed AC:** README and manual name only settings that exist in the v2 Settings page, checked against `SET_META` in `v2.js`.

### R4. The release notes open with 25 bullets of test history (H8, sev 3)

- **What:** the beta.401 notes start with a disclaimer whose body is a list of test provenance per build range, from beta.77 to beta.401 (about 25 bullets, `RELEASE_NOTES_2.0.0-beta.401.md:7-36`). What changed for the reader starts at line 50. A user who opens the notes from the README (`:7`) or GitHub sees a wall of build numbers first. The file is 811 lines long, because every beta's section is carried along.
- **What a reader needs first** (in this order): what this release is and who should install it; the known issues; what changed since the last version they may have; how to install or update. The test record matters, but as evidence, not as the first thing.
- **Proposal:** a fixed head of at most about 15 lines:
  1. One sentence: what this beta is (and that it is a beta).
  2. "Install it if … / Stay on … if …" (two lines).
  3. Known issues (short list, or a link).
  4. "What changed since beta.N" (the newest section).
  5. How to update (WiFi, or USB for an old layout).

  Then the older "since beta.N" sections, and at the end an appendix "How this beta was tested", which holds today's provenance bullets unchanged. Style owner: BlueFlame, with the release skill (`/beta-release-2x`), so every next beta follows it.
- **Proposed AC:** the next beta's notes have the head above within the first ~15 lines, with the test provenance in an appendix at the end; the beta-release skill template is updated to match.

### R5. SAT comes before getting started (H8, H6, sev 2)

- **What:** the README runs: What's new, Features, Hardware, then 56 lines of SAT (`:90-146`), and only then Quick Start (`:148`). SAT is opt-in and off by default (`:26`). A new user has to scroll past an advanced feature before learning how to install.
- **Proposal:** order: the firmware choice (R1), Quick Start, Hardware and migration, Features, then SAT, MQTT and the rest. The "What's new" block can shrink to the highlights list, with the detail in the release notes.
- **Proposed AC:** Quick Start comes directly after the firmware choice; SAT follows the basic setup.

### R6. The SAT guides live in `backlog/docs/` (H2, sev 2)

- **What:** the README sends users to `backlog/docs/doc-3 - sat-integration-guide.md` (`:126`), `doc-2 - sat-opv-calibration.md` and `doc-4 - sat-preset-configuration.md` (`:143-144`). `backlog/` is the project's task store. Next to those guides it holds team files such as `sat-python-cpp-mapping.md` and `sat-future-plan.md`. The same rule the coordinator set for the banner applies: a user link should not land in project-internal space. Chapter 5 of the manual covers SAT for users.
- **Proposal:** link the manual chapter 5 sections instead, or move the three guides into `docs/guides/` (otgw-docs). Whether their content is current was not checked here.
- **Proposed AC:** no README link points into `backlog/`.

### R7. Two small mismatches in Quick Start (H2, sev 1)

- **Step 2** says the access point is `<hostname>-<mac>` (`:163`): correct, but abstract. The example `OTGW2-A4:CF:12:21:B4:F8` (as in the manual) tells a new user what to look for in the WiFi list. Same wording as the CLI review's C3.
- **Step 3** says "The dashboard shows the live OpenTherm log, temperatures, and boiler status" (`:167`). In the v2 web UI, Home shows the system view with temperatures and boiler status; the live log is under Monitor › Log.
- **Proposed AC:** step 2 has the example name; step 3 names Home and Monitor › Log as they are.

## What works well

- **The beta warning is honest and early** (`README.md:7`): it says what was and was not tested, and it tells people who need reliability to stay on 1.x.
- **Breaking changes are named before the upgrade** (`:33`), with a direct link to the list.
- **The migration from the ESP8266** has its own section and a step-by-step chapter in the manual (`:75-88`).
- **The release notes carry their own evidence**: every range of builds says how it was tested. The content is right; R4 is about where it sits.

## Not verified

- The rendered GitHub page and the badge image were not fetched; the badge value is from shields.io's documented default for `github/v/release` (prereleases excluded).
- The 1.x README on `otgw-1.x.x` and the v1.7.9 release notes were not reviewed; only the path to them.
- Whether the three `backlog/docs` SAT guides are current (R6).
- No user test: this is a heuristic walkthrough by one reviewer.
