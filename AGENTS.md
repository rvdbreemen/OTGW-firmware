
<!-- PROJECT VERSIONING GUIDELINES START -->
# Project Versioning

When asked to bump a prerelease version, keep the prerelease tag numeric and increment that number by one.
For release-relevant work, bump the prerelease number once for each relevant commit or coherent change set that should be reflected in the prerelease version.

Examples:

- `beta.13` becomes `beta.14`
- `alpha.3` becomes `alpha.4`
- `rc.1` becomes `rc.2`

Do not change the prerelease channel name unless the user explicitly asks for that. Update all matching version strings in the firmware version metadata so `_VERSION_PRERELEASE`, `_SEMVER_FULL`, `_SEMVER_NOBUILD`, and `_VERSION` stay consistent. If there are multiple relevant commits, apply the bumps cumulatively.

<!-- PROJECT VERSIONING GUIDELINES END -->

<!-- PROJECT BUILD GUIDELINES START -->
# Project Build Rules

When building release or validation artifacts, never build firmware-only. Any firmware build must also build the filesystem image in the same validation pass so firmware and LittleFS assets stay in sync.

Prefer the project build command that produces both artifacts. If using `build.py`, run the combined/default build or explicitly include both firmware and filesystem targets instead of `--firmware` alone.

<!-- PROJECT BUILD GUIDELINES END -->

<!-- BACKLOG.MD GUIDELINES START -->
<!-- DO NOT regenerate this block via `backlog agents --update-instructions`. -->
<!-- The full Backlog CLI reference is intentionally extracted to -->
<!-- .claude/backlog-cli-reference.md to keep this file focused. -->

# Backlog.md task management

All task operations go through the **`backlog` CLI** — never edit task files directly. The CLI owns file naming, frontmatter, AC indexing, and Git tracking; bypassing it desynchronises the project.

Full CLI reference: read `.claude/backlog-cli-reference.md` before any backlog operation.
<!-- BACKLOG.MD GUIDELINES END -->
