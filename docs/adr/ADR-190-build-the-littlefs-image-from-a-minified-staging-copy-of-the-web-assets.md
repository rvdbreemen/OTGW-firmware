---
id: "ADR-190"
title: "Build the LittleFS image from a minified staging copy of the web assets"
status: "Accepted"
date: "2026-10-15"
binding: false
gate: "tests/test_build_staging.py"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-166"
  - "ADR-191"
  - "ADR-192"
  - "ADR-194"
topics:
  - "littlefs"
  - "web-assets"
  - "minify"
  - "build-pipeline"
aliases:
  - "minified littlefs"
  - "data staging"
  - "asset minification"
  - "no gzip"
components:
  - "LittleFS staging step in build.py (stage_data_dir, prepare_filesystem_staging)"
symbols:
  - "stage_data_dir"
  - "prepare_filesystem_staging"
  - "STAGING_DIR"
  - "STAGING_EXCLUDE"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-190 Build the LittleFS image from a minified staging copy of the web assets

## Status

Accepted, 2026-10-15.

**Decision Maker:** User: Robert van den Breemen (maintainer). He rejected gzip
and asked for a minified image that keeps every file now in `data/`.

## Status History

```yaml
status_history:
  - date: 2026-10-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-166
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-191
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-192
    changed_via: adr-kit lifecycle
  - date: 2026-10-23
    status: Accepted
    changed_by: "Agent: Claude (adr-generator, TASK-1303)"
    reason: Related to ADR-194 (narrowing, Proposed)
    changed_via: adr-kit lifecycle
```

## Context

The web UI ships as files on the LittleFS (Little File System) partition of the
ESP32-S3 (Espressif dual-core microcontroller) boards. On the 4 MB boards that
partition is 1.5 MiB, and the bench unit reports it at least 95% full: the
shipped assets need a 1460 KiB image, leaving about 75 KiB for settings, logs
and LittleFS copy-on-write. Epic TASK-1223 (WiFi firmware OTA, over-the-air
update, via a safeboot factory partition) needs the filesystem to shrink to
768K (spike TASK-1222, `docs/research/2026-10-14-spike-1222-safeboot-ota-4mb.md`).

Gzip would solve the size, but the maintainer rejected it twice: the directive
is recorded at `src/OTGW-firmware/FSexplorer.ino:83` ("no build-time gzip") and
was confirmed again on 2026-10-14. Files must stay plain text and be served as
they are. Most of the asset bytes are whitespace and comments in JavaScript
(JS) and CSS (Cascading Style Sheets): `v2.js` alone holds 690 comment lines
of 4486.

## Decision

`build.py` builds the LittleFS image from a staging copy of
`src/OTGW-firmware/data/` in which the `.js` and `.css` files are minified;
`data/` itself is never modified.

The staging step (`build.py:787` `stage_data_dir`, called from `build.py:853`
`prepare_filesystem_staging`) copies `data/` to `.pio/data-staging/`, removes
`v2.css` (it is only the input of the generated `v2-bundle.css`; no page loads
it), and minifies `.js` with rjsmin 1.3.0 and `.css` with rcssmin 1.3.0, both
pinned in `requirements-build.txt`. Every other file (HTML, PIC `.hex`, `.json`,
`.log`, `.ini`, fonts, images) is copied byte for byte. The PlatformIO build
reads the staging dir through `PLATFORMIO_DATA_DIR` (`build.py:2152`); the
arduino-cli path passes it to `mklittlefs -c`. A minifier error stops the build
with exit code 1; `--no-minify` builds an unminified image for debugging. The
build prints the per-file savings and the smallest LittleFS image that holds the
staged content.

## Decision Contract

### Must

- Build every LittleFS image from the staging directory, never from `data/`.
- Keep files the firmware reads itself, and all binaries and HTML, byte-identical.
- Fail the build when minifying fails; never fall back silently to unminified files.
- Keep the minifier versions pinned in `requirements-build.txt`.

### Must Not

- Write minified or otherwise transformed files into `src/OTGW-firmware/data/`.
- Gzip or otherwise compress-encode any asset (maintainer directive).
- Drop a file from the image other than those listed in `STAGING_EXCLUDE`.

### Exceptions

- `--no-minify` builds an unminified image for debugging; it still uses the staging copy.

### Verification

- `python tests/test_build_staging.py`: source untouched, non-browser files
  byte-identical, browser assets smaller, only `STAGING_EXCLUDE` dropped,
  failures exit non-zero, and no CSS comment that closes early.
- Build log line `Building FS image from '.pio\data-staging'` from `pio run -t buildfs`.

## Alternatives Considered

- **Gzip the browser assets.** Smallest result (604 KiB image). Rejected by the
  maintainer (`FSexplorer.ino:83`); it also changes how the firmware serves files
  (Content-Encoding handling).
- **esbuild (Go-based bundler, run through npx/Node.js).** Its full minify gives
  a 23 KB smaller `v2.js` (180,725 B vs rjsmin 208,052 B). Rejected because it
  adds a Node.js dependency to a Python-only build; rjsmin/rcssmin install through
  the existing `requirements-build.txt` hook in `build.bat`/`build.sh`.
- **Minify in place in `data/` after a backup copy.** Rejected: an interrupted
  build could leave minified files in the source tree, and git already is the backup.
- **Do nothing.** Rejected: the filesystem is already at least 95% full and the
  safeboot OTA layout cannot fit.

## Consequences

**Positive:**

- LittleFS minimum image drops from 1460 KiB to 1080 KiB with both UIs still
  shipped: 332,441 bytes saved over 10 files (`v2.js` 299,825 to 208,052 B,
  `index.js` 392,995 to 253,779 B). Measured with `mklittlefs -b 4096 -p 256`.
- About 456 KiB runtime room on today's 1.5 MiB partition instead of about 75 KiB.
- Fewer bytes per page load at the 2-request serving cap (ADR-165).
- The source in git stays readable and is never rewritten by a build.

**Negative:**

- Minified files are hard to read in the browser's developer tools on the
  device. Mitigation: the readable source is in git; `--no-minify` builds a
  debug image.
- Two new build dependencies. Mitigation: pinned versions, installed by the
  build wrappers; a missing minifier stops the build with a clear message.
- A minifier bug could change behaviour. Mitigation: on 2026-10-14 the rjsmin
  output of all 7 JS files parsed (`node --check`) and was sha256-identical to
  the original after esbuild normalisation; rcssmin output differed only in
  whitespace and `.10` vs `.1`. A headless-browser check showed the same CSS
  declaration count before and after minify.
- A CSS comment that closes early is harmless whitespace in the source but can
  swallow following rules once lines are joined. Found and fixed in
  `ds-tokens.css` (TASK-1225); `tests/test_build_staging.py` now fails on it.

## Open Questions

- [x] Bench validation of the minified image (TASK-1224 AC7): both UIs, FSexplorer, settings round-trip and PIC hex hashes.
  Resolved 2026-10-15 on the esp32-combo bench unit, comparing an unminified
  baseline with the minified image:
  - Both images had 0 page errors.
  - The v2 UI had 0 console errors.
  - FSexplorer lists the files, without v2.css.
  - Settings round-trip and settings.ini restore both passed (0 of 182 keys differ).
  - The PIC hex sha256 values match the source.
  - The device serves v2.js at 208,052 B.

  The only failed requests were the known 503s from the file-serve gate (ADR-147), which also showed on the baseline.

## Related Decisions

- **ADR-166 (single app slot OTA policy)**: this ADR is a precondition for the
  safeboot layout that will amend it (TASK-1223.1).
- **ADR-165 (two concurrent API/file requests)**: smaller files shorten each
  request slot.
- **ADR-139 (ETag cache busting)**: unchanged; the ETag is still computed from the served file.

## References

- TASK-1224 (implementation), TASK-1225 (CSS comment bug), TASK-1222 (spike), TASK-1223 (epic).
- `build.py:771` (`STAGING_DIR`), `build.py:787`, `build.py:828`, `build.py:853`, `build.py:2152`.
- `requirements-build.txt`, `tests/test_build_staging.py`.
- `docs/research/2026-10-14-4mb-ota-asset-options.md` (size options and measurements).
- rjsmin and rcssmin: <https://github.com/ndparker/rjsmin>, <https://github.com/ndparker/rcssmin> (Apache-2.0).
