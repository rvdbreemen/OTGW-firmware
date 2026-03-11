# Release Generation Guide

This guide provides the exact steps (and AI prompts) required to reliably generate the artifacts, notes, and documentation changes for every new release of the OTGW-firmware.

When preparing a release, the maintainer or AI Copilot should perform these exact tasks to prepare the repository. 

## 1. Gather the List of Commits
Extract all commits made since the previous version to ensure all notable features, bug fixes, and memory adjustments are discovered.
```bash
git log <last-version-tag>..HEAD --oneline
```
*Note: Ignore commits explicitly starting with `CI: update version.h` or trivial branch merges.*

## 2. Generate Full Release Notes
Create a new file named `RELEASE_NOTES_<version>.md` in the root folder.
Identify and categorize changes into:
- 🚀 **New Features**: Any new functionality added to the firmware, WebUI, MQTT discovery, or hardware manipulation.
- 🐛 **Bug Fixes**: Any issue mitigated. 
- 🧠 **Memory and Performance Improvements**: Any optimizations regarding heap allocations, refactoring of structures (like switching from `String` to static buffer).
- ⚠️ **Breaking Changes**: Very explicitly describe if any MQTT topic structure varied, if endpoints were removed (or marked 410 Gone), or if configuration parameters require a migration path. 
*Always declare "There are **no breaking changes**" if there are truly none.*

## 3. Extract the Github Release Note Summary
Create a new file named `RELEASE_GITHUB_<version>.md` in the root folder.
This must be a stripped-down, punchy summary of the full release notes suitable to be directly pasted into the GitHub Release UI window. 
- Keep the introduction short.
- Use a bolded bulleted format.
- Avoid citing too many deep technical refactors unless it profoundly affects stability.
- Explicitly link to the full `RELEASE_NOTES_<version>.md` at the bottom.

## 4. Record Breaking Changes Globally
Append any explicit breaking changes to the global `docs/BREAKING_CHANGES.md` log. This ensures users jumping across multiple versions have a chronological warning path.
- Add a new Markdown header (`## 🛑 vX.X.X`) at the **top** of the list.
- Provide a concise description of the breaking mechanism.
- If there are none, still create the header and declare: "There are **no breaking changes** in `vX.X.X`."

## 5. Update the Root README.md
You must adapt the main `README.md` to point toward the new stable release.
1. Move the old "🚀 What's new in vX.X.X" down into the historical stack, downgrading it to "## What was new in vX.X.X".
2. Create a fresh "## 🚀 What's New in v<new-version>" directly under the opening banner. Copy the highlight bullets from your release notes.
3. Traverse down to the **Release History Table** at the bottom of the `README.md` and insert a new row into the table containing a summarized description of the release.

## 6. Execute Pre-release Checks
Follow the `docs/RELEASE_PROCESS.md` standard checklist before pushing these changed documents. 
- Ensure all `-beta` strings inside the `version.h` and the new released docs have been removed.
- Run `python evaluate.py` to confirm everything is pristine. 

---
### 🤖 Example Copilot Prompt for Automating this
> *"You are an AI assistant. I want you to prepare the release for **v1.4.0**. The previous release was **v1.3.0**. First, run a git log comparison to capture all commits since **v1.3.0**. Second, generate the `RELEASE_NOTES_1.4.0.md` capturing all features, fixes and performance improvements. Third, create `RELEASE_GITHUB_1.4.0.md`. Fourth, prepend any breaking changes to `docs/BREAKING_CHANGES.md`. Finally, parse `README.md`, demote the older feature list and promote the new 1.4.0 highlights, adding the new version row into the version table. Ensure no `-beta` nomenclature remains."*
