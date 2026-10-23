---
id: "ADR-194"
title: "Serve static text assets without an upload role from firmware flash"
status: "Accepted"
date: "2026-10-23"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-139"
  - "ADR-190"
topics:
  - "littlefs"
  - "web-assets"
  - "progmem"
  - "flash-budget"
  - "etag"
aliases:
  - "assets in firmware flash"
  - "progmem assets"
  - "settings help json"
  - "built-in replay fixture"
  - "littlefs margin"
components:
  - "serveProgmemAsset (firmware-flash asset route with a firmware-hash ETag)"
  - "built-in replay fixture fallback (OTGW_SIM_FIXTURE)"
symbols:
  - "serveProgmemAsset"
  - "SETTINGS_HELP_JSON"
  - "OTGW_SIM_FIXTURE"
  - "reopenOTGWSimulationFile"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-194 Serve static text assets without an upload role from firmware flash

## Status

Accepted, 2026-10-23.
(continuous integration) gate in `evaluate.py` or `tests/`; the decision rule is
applied at PR (pull request) review. It narrows ADR-190 and ADR-139 for two text
assets; it does not reverse either of them. The image stays minified, nothing is
gzipped, and every other file stays in `data/` and the LittleFS image.

**Decision Maker:** User: Robert van den Breemen (maintainer) chose this lever on
2026-10-22 in the TASK-1294 session. This record was drafted afterwards
(TASK-1303) and still needs his acceptance.

## Status History

```yaml
status_history:
  - date: 2026-10-23
    status: Proposed
    changed_by: "Agent: Claude (adr-generator, TASK-1303)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-23
    status: Proposed
    changed_by: "Agent: Claude (adr-generator, TASK-1303)"
    reason: Related to ADR-190 (narrowed by this ADR)
    changed_via: adr-kit lifecycle
  - date: 2026-10-23
    status: Proposed
    changed_by: "Agent: Claude (adr-generator, TASK-1303)"
    reason: Related to ADR-139 (narrowed by this ADR)
    changed_via: adr-kit lifecycle
  - date: 2026-10-23
    status: Accepted
    changed_by: Robert van den Breemen
    reason: "Maintainer decision 2026-10-23 (session): accepted"
    changed_via: adr-kit lifecycle
```

## Context

The 4 MB ESP32-S3 boards have a 768 KiB LittleFS (Little File System) partition
since the safeboot layout (ADR-191). The build checks a budget of staged assets
plus a 96 KiB runtime reserve (`build.py:858` `FS_RUNTIME_RESERVE_KIB = 96`,
message at `build.py:899`). After TASK-1281 added `settings-help.json` (18,300 B,
5 LittleFS blocks of 4 KiB) the build reported, quoted from TASK-1294:

- before: `LittleFS budget OK: 664 KiB assets + 96 KiB reserve <= 768 KiB partition`,
  a margin of 8 KiB.

That 664 KiB came from a local tree that also held `data/otgw_simulation.log`
(12,716 B, 4 blocks). The file was gitignored (`*.log`), so CI and release images
never carried it (TASK-1294 implementation note, 2026-10-22). For a release image
the margin was therefore about 24 KiB (derived: 664 minus the 16 KiB of the log),
still too small for the next web asset.

ADR-190 builds the image from a minified staging copy and keeps every file in
`data/`, with "no gzip" as a maintainer directive. ADR-139 keeps the assets as
plain files on LittleFS (no gzip, no `.gz` siblings). Its Alternative B rejected
embedding the assets as PROGMEM (program memory, the flash-resident data of the
firmware image) buffers on two grounds: the UI assets are user-updatable through
FSexplorer, and that design was gzipped. Neither ground applies to a plain-text
asset that users have no reason to upload, which is the gap this ADR fills.

## Decision

Static text assets that users never need to upload may be compiled into the
firmware image and served or read from PROGMEM, instead of shipping in the
LittleFS image; a built-in default is allowed when a user override lives on
LittleFS and wins. Every other asset stays on LittleFS, minified per ADR-190 and
never gzipped.

TASK-1294 (commit `6ba4a5165`, "Ship the settings help texts and the replay
fixture inside the firmware") applied this to exactly two files, both removed
from `data/`:

- `settings-help.json` lives in `src/OTGW-firmware/settingsHelp.h` as
  `SETTINGS_HELP_JSON` (a raw string literal, the source of truth). The route
  `/settings-help.json` (`FSexplorer.ino:222`) calls
  `serveProgmemAsset()` (`FSexplorer.ino:130`), which sets
  `ETag: "<_VERSION_GITHASH>-<path>"` and `Cache-Control: no-cache` (the ADR-163
  value), answers 304 on a matching `If-None-Match`, and streams the body from
  flash with `webSendPStream` (no heap copy).
- `otgw_simulation.log` became the built-in replay fixture `OTGW_SIM_FIXTURE` in
  `src/OTGW-firmware/otgwSimulationFixture.h`, read through `pgm_read_byte`
  (`OTGW-Core.ino:3970-3979`). `reopenOTGWSimulationFile()`
  (`OTGW-Core.ino:4053-4062`) still opens an uploaded `/otgw_simulation.log`
  first and falls back to the fixture only when no such file exists.

## Decision Contract

### Must

- Move an asset into firmware flash only when all three hold:
  (a) users have no upload role for it; a built-in default is allowed when a
  user override lives on LittleFS and takes precedence over it;
  (b) it is text whose content changes with the firmware anyway;
  (c) an HTTP-served asset goes through `serveProgmemAsset()` (ETag = firmware
  git hash plus path, `Cache-Control: no-cache`, 304 on match), and an internally
  read asset goes through a PROGMEM reader such as `pgm_read_byte`.
- Keep the PROGMEM copy as the single source of truth and remove the file from
  `data/` in the same change.

### Must Not

- Move the web UI shell, `v2.js`, `v2-bundle.css`, FSexplorer or the fonts into
  firmware flash: they stay on LittleFS (ADR-139, ADR-192).
- Gzip or otherwise compress-encode an asset, in firmware or on LittleFS
  (ADR-190 directive, unchanged).
- Copy a PROGMEM asset to the heap to serve it.

### Exceptions

- None.

### Verification

- PR review against the three-part rule above; no automated gate.
- Bench evidence recorded in TASK-1294: `/settings-help.json` 200 with
  `ETag "7064e18-/settings-help.json"`, then 304 with 0 B on `If-None-Match`.
- `tests/webui/settings-help.test.mjs` parses the JSON out of `settingsHelp.h`;
  `tests/test_sim_fixture_builtin.py` fails when the fixture header drifts from
  `tests/fixtures/otgw_simulation.builtin.log` (source-level checks).

## Alternatives Considered

- **Gzip the staged web assets and serve the `.gz` sibling.** Would save the most
  (`v2.js` about 243 KB minified to an estimated 60 KB). Rejected: it reverses the
  standing maintainer directive in ADR-190 and ADR-139 D ("no gzip, no .gz
  siblings"), and the maintainer did not choose it on 2026-10-22.
- **Drop the shipped PIC hex files (about 116 KB).** The firmware can fetch them
  on demand from otgw.tclcode.com (`OTGW-Core.ino:6226`). Not chosen by the
  maintainer on 2026-10-22 (TASK-1294 notes record the choice, not a reason);
  it would also make PIC flashing depend on that external server.
- **Drop the self-hosted fonts (about 69 KB).** Rejected by the maintainer on
  2026-10-22; the fonts stay on LittleFS with their immutable cache policy.
- **Do nothing.** Rejected: an 8 KiB local margin (about 24 KiB for release
  images) means the next web asset of any size fails the ADR-190 budget gate.

## Consequences

**Positive:**

- LittleFS budget after TASK-1294, quoted from its green build:
  `LittleFS budget OK: 628 KiB assets + 96 KiB reserve <= 768 KiB`, a 44 KiB
  margin. Saving on a release image: the help JSON only, 20 KiB (5 blocks).
- Every unit, including a fresh release unit, now has a replay source: the
  built-in fixture. An uploaded `/otgw_simulation.log` still wins.
- The help text and the firmware that reads its keys always ship together, so
  they cannot drift apart after a firmware-only or filesystem-only update.

**Negative:**

- Help text changes need a firmware update, and the help JSON can no longer be
  replaced through FSexplorer. Accepted because users had no reason to edit it.
- Flash cost. Measured on 2026-10-23 from the headers: `SETTINGS_HELP_JSON` body
  18,398 B (it was 18,300 B at `6ba4a5165`; `298c6608c` changed it since) and
  the fixture 12,716 B in 1,156 lines (size of the canonical copy). Quoted from
  the TASK-1294 build: app flash went from 89.3% to 90.6% (2,049,071 of
  2,293,760 B before, +29.5 KB including the new code). Not re-measured by a
  build for this ADR.
- API contract change: `POST /api/v2/simulate/start` no longer answers 409 when
  no file exists, and the simulate status gains `"source":"file"|"builtin"`
  (`restAPI.ino:367-382`).
- A dirty rebuild at the same git hash keeps the same ETag, so a browser can keep
  a stale help JSON until the next commit. Mitigation: release builds come from
  a commit; the filesystem-hash ETag of ADR-139 has the same limit.

## Open Questions

- [x] Does the maintainer accept the three-part rule as the standing criterion for
  future moves into firmware flash?
  — **Answered 2026-10-23 by Robert van den Breemen:** Yes: the three-part rule (no upload role, changes with the firmware, PROGMEM path with a firmware-hash ETag or a PROGMEM reader) is the standing criterion for moving an asset into firmware flash. Maintainer decision 2026-10-23.

## Related Decisions

- **ADR-190 (LittleFS image from a minified staging copy)**: narrowed. Two text
  assets leave `data/`; staging, minify and the no-gzip rule stay as they are.
- **ADR-139 (ETag cache-busting standard)**: narrowed. Its Alternative B rejected
  PROGMEM for user-updatable, gzipped UI assets; this ADR moves only non-UI,
  non-uploaded plain text and keeps the ETag plus revalidation contract.
- **ADR-163 (Cache-Control no-cache)**: `serveProgmemAsset()` uses the same
  `no-cache` value.
- **ADR-192 (ship only the v2 web UI)**: the v2 UI fetches `/settings-help.json`;
  its shell and assets stay on LittleFS.
- **ADR-191 (safeboot OTA on 4 MB boards)**: the source of the 768 KiB partition.
- **ADR-080 (binding ADR rules need a CI gate)**: this ADR is guideline-level.

## References

- TASK-1294 (implementation, measurements, maintainer decision), TASK-1281 (help
  texts), TASK-1303 (this record).
- Commit `6ba4a5165` (TASK-1294), commit `298c6608c` (later help JSON edit).
- `src/OTGW-firmware/FSexplorer.ino:121-142` (`serveProgmemAsset`), `:222` (route).
- `src/OTGW-firmware/settingsHelp.h`, `src/OTGW-firmware/otgwSimulationFixture.h`.
- `src/OTGW-firmware/OTGW-Core.ino:3961-3979`, `:4053-4062`, `:6226`.
- `src/OTGW-firmware/restAPI.ino:367-382`.
- `build.py:858`, `build.py:899` (LittleFS budget gate).
- `tests/webui/settings-help.test.mjs`, `tests/test_sim_fixture_builtin.py`,
  `tests/fixtures/otgw_simulation.builtin.log`.

No Enforcement section: guideline-level per ADR-080. A declarative adr-judge
block would act as the CI gate this ADR does not claim; the rule is checked at PR
review.
