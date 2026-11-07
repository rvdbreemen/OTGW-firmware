---
id: "ADR-215"
title: "Serve each web page as one gzipped file with an ETag (partly supersedes ADR-139, ADR-190 and ADR-192)"
status: "Accepted"
date: "2026-11-06"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-139"
  - "ADR-147"
  - "ADR-165"
  - "ADR-188"
  - "ADR-190"
  - "ADR-192"
  - "ADR-212"
  - "ADR-216"
topics:
  - "web-ui"
  - "web-server"
  - "gzip"
  - "littlefs"
  - "caching"
aliases:
  - "single gzipped page"
  - "inline web assets"
  - "gzip web ui"
components:
  - "scripts/webui_pages.py (page inlining and gzip)"
  - "build.py stage_data_dir (filesystem staging)"
  - "scripts/platformio_webui.py (update page header)"
  - "FSexplorer.ino serveGzipPage"
  - "webServerCompat.h webSendGzipPage"
symbols:
  - "serveGzipPage"
  - "webSendGzipPage"
  - "sendIndex"
  - "sendFSexplorerPage"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-215 Serve each web page as one gzipped file with an ETag (partly supersedes ADR-139, ADR-190 and ADR-192)

## Status

Accepted, 2026-11-06.

Maintainer decision, 2026-11-06 (Robert, TASK-1674): accept this direction although it reverses the plain-text rule of ADR-190 and the no-gzip rule of ADR-139, once the review's must-fix items were in: the plain-page fallback for a filesystem from before this ADR, the relation to ADR-212, and a host test of the serving path.

Guideline-level (ADR-080) with a test gate: `tests/test_webui_pages.py` and
`tests/test_build_staging.py`, and `tests/test_web_file_scheduling.py` for the serving path.

This ADR replaces only the parts of ADR-139, ADR-190 and ADR-192 listed under
"Partial supersession" below. adr-kit has no partial-supersession form (see
ADR-203), so the front-matter `supersedes` list stays empty and those ADRs are
not edited by this proposal. When this ADR is accepted, run
`adr relate ADR-215 --to ADR-139`, `--to ADR-190` and `--to ADR-192`; do not
run `adr supersede`. The link to ADR-212 is already in place (`adr relate`).

## Status History

```yaml
status_history:
  - date: 2026-11-05
    status: Proposed
    changed_by: "Contributor: André Brait"
    reason: Initial proposal
    changed_via: manual
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-212
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-139
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-147
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-165
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-188
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-190
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-192
    changed_via: adr-kit lifecycle
  - date: 2026-11-06
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted the direction on 2026-11-06 (TASK-1674) on condition of the review must-fixes, which landed with this change
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-216
    changed_via: adr-kit lifecycle
```

## Context

The v2 web UI needed five files on every first load: `v2-bundle.css`, `v2.js`
and three WOFF2 fonts, next to `v2.html`. The server streams at most two files
at a time (ADR-165) and answers 503 to the rest, and browsers do not retry a 503
for a stylesheet, script or font. The UI therefore loaded its files one after
another from JavaScript, with retries (TASK-978, TASK-989, TASK-1643).

The measurements in PR #697 (one ESP32-S3 Combo unit, Chrome, Firefox and WebKit)
compared that loader, native parallel loading with a server-side queue, and one
page with everything inlined:

| Variant | Cold load, one tab | File 503s, two tabs | Min. free heap, two tabs |
|---|---:|---:|---:|
| beta.94 JavaScript loader | 2.9–4.2 s | 60 (5 of 10 tabs incomplete) | 16.4 KB |
| Native loading with a file queue | 2.9–3.5 s | 39 | 6.0 KB |
| One gzipped file | 1.1–1.4 s | 0 | 26.1 KB |
| One uncompressed file | 2.9–4.0 s | 0 | 26.0 KB |

One file needs one file stream per tab, which removes the 503s and the heap dip
of two concurrent streams; gzip is what makes it fast. A reload with the ETag
gets 304 and transfers only headers (about 0.25–0.35 s).

ADR-190 and ADR-139 kept the filesystem plain text because the maintainer had
rejected gzip: files on LittleFS were meant to stay readable and editable through
FSexplorer. In practice developers change the web UI with scripts or agents, not
by uploading single files by hand, so a build step that produces the page is the
normal workflow.

The firmware update page (`/update`) lives in app flash so it works whatever the
state of LittleFS (TASK-1283). It was already one self-contained page, stored as
about 53 KB of plain text.

## Decision

1. **One file per page.** `scripts/webui_pages.py` inlines every local
   `<link rel="stylesheet">` as `<style>`, every local `<script src>` as an inline
   `<script>`, and every CSS `url()` (the fonts) as a `data:` URI. The pages are
   `v2.html` and `FSexplorer.html`; their sources stay as separate, readable files
   in `src/OTGW-firmware/data/`.
2. **gzip at build time.** `build.py` minifies the CSS and JavaScript in the
   staging copy (ADR-190), builds each page, gzips it reproducibly (fixed mtime,
   level 9) as `v2.html.gz` and `FSexplorer.html.gz`, and removes the inlined
   inputs from the image. The filesystem image holds no separate CSS, JavaScript
   or font files.
3. **Serve with Content-Encoding and ETag.** `/`, `/index`, `/index.html` and
   `/v2.html` serve `/v2.html.gz`; `/FSexplorer` and `/FSexplorer.html` serve
   `/FSexplorer.html.gz`. Each response carries `Content-Encoding: gzip`,
   `Cache-Control: no-cache` and `ETag "<filesystem hash>-<path>"`; a matching
   `If-None-Match` gets 304 with no body (ADR-139). The handlers name the `.gz`
   file directly, so the library does not add a second `Content-Encoding`
   (the TASK-433 bug).
4. **Update pages in app flash, gzipped.** The update page and its form-POST
   success page move to `src/OTGW-firmware/webui/`. A PlatformIO pre-script
   (`scripts/platformio_webui.py`, also called by `build.py` for the arduino-cli
   path) gzips them into the generated, git-ignored `updateServerHtml.h`.
   `webSendGzipPage()` streams them from flash with `Content-Encoding: gzip`;
   `GET /update` gets `ETag "<firmware hash>-/update"` and 304 on a match.
5. **Recovery stays independent.** FSexplorer carries only its own stylesheet,
   inlined into its own file, so it works when the v2 page is missing. When
   `FSexplorer.html.gz` is missing too, the PROGMEM upload prompt remains.
6. **Live boards.** `scripts/ota_flash.py --webui` builds the same pages from
   `data/` and uploads them through `/upload`, without flashing an image.

The safeboot updater (a separate image, ADR-191) is not changed.

## Partial supersession

- **ADR-139:** the clauses that rule out gzip and `.gz` files (the maintainer
  directive and Alternative B's second ground). The ETag contract stays: one ETag
  per served file, `no-cache`, 304 on a match.
- **ADR-190:** "files must stay plain text and be served as they are". The
  minified staging copy stays; the pages are gzipped after minifying.
- **ADR-212:** the page-side native loading (the browser fetching the stylesheet,
  script and fonts as separate files, and the `window.otgwAssetLoad` diagnostics)
  and its test gate `tests/webui/native-asset-load.test.mjs`, which this change
  removes. ADR-212's server-side bounded file queue stays in place as a safety net
  for every remaining file response.
- **ADR-192:** the asset list (`v2-bundle.css`, `fsexplorer.css` and the font
  files as separately served files, `ds-tokens.css` as the input of
  `v2-bundle.css`). `/` still serves the v2 UI and still falls back to FSexplorer.

## Decision Contract

### Must

- Ship each page in `webui_pages.PAGES` as one gzipped file, with no local
  stylesheet, script or font reference left in it.
- Serve every page with `Content-Encoding: gzip` and an ETag, and answer a
  matching `If-None-Match` with 304 and no body.
- Fail the build when a page references a local file that does not exist.
- Keep FSexplorer self-contained: it must not depend on any v2 file.

### Must Not

- Serve a `.gz` file through a path where the library also adds
  `Content-Encoding` (a `.gz` sibling of a requested plain name).
- Commit the generated `updateServerHtml.h`.

### Exceptions

- `favicon.ico` stays a separate file: browsers request it on their own.

### Verification

- `tests/test_webui_pages.py`: inlining, missing-file failure, script-end escaping,
  the real pages without local references, the PROGMEM header round-trip.
- `tests/test_build_staging.py`: the staging copy holds the gzipped pages and no
  inlined inputs, and passes the LittleFS budget.
- `tests/test_web_file_scheduling.py`: compiles the real `serveGzipPage`,
  `sendIndex`, `handleFile` and `webSendGzipPage`: ETag `<hash>-<path>`, 304 on a
  matching `If-None-Match`, `Content-Encoding: gzip` exactly once, the plain-page
  fallback and the upload-prompt gate. `--mutant` shows each check fails on its defect.
- Bench: first load 200 with the ETag, reload 304, stale ETag 200; no file 503s
  with two tabs.

## Alternatives Considered

- **Keep separate files and only queue requests on the server (ADR-212,
  PR #697).** Native loading worked for one tab but two concurrent file streams
  lowered the largest free block to 7,668 bytes, below the 8,192-byte guard.
  ADR-212's queue stays underneath this decision as a safety net; its page-side
  native loading and `window.otgwAssetLoad` diagnostics are replaced by the
  single page.
- **One uncompressed file.** Same heap and 503 results as gzip, but the page is
  466 KB instead of 171 KB and loads no faster than today.
- **Gzip each file separately (GitHub #692 as first proposed).** Smaller
  transfers, but still five files and the same concurrency limits.
- **Move the pages into app flash (EMS-ESP32 style).** Rejected: the app
  partition is about 93% full, and the UI would no longer update with the
  filesystem image.

## Consequences

- First loads take about 1.1–1.4 s instead of 2.9–4.2 s, and the page has no
  file 503s with two tabs.
- The filesystem image is smaller: the gzipped pages replace about 450 KB of
  separate files. The update pages take about 17 KB of app flash instead of 53 KB.
- Any change to CSS, JavaScript or fonts changes the whole page, so a reload after
  an update downloads it again (about 170 KB) instead of only the changed file.
- The files on the device are no longer readable in FSexplorer; the readable
  sources are in the repository.
- Uploading a page with `--webui` does not change the filesystem hash, so the
  ETag stays the same: reload with the cache bypassed after a push.
- **Mixed firmware and filesystem.** The two images can be updated one at a time,
  in either order. New firmware on a filesystem from before this change has the
  plain `/v2.html` and `/FSexplorer.html` and no `.gz` files: `/` serves the plain
  `/v2.html` (without `Content-Encoding`, so the UI still works, now with its
  separate `/v2.js` and `/v2-bundle.css` coming from the filesystem), the recovery
  page serves the plain `/FSexplorer.html`, and files such as `/settings.ini`
  keep being served; the upload prompt appears only when neither form of the
  recovery page exists. New filesystem under old firmware: the old firmware looks for the plain
  `/v2.html` and `/FSexplorer.html`, which the new image no longer holds, so `/`
  answers 404 and `/FSexplorer.html` shows the old upload prompt, until the
  firmware is updated too. `/update` lives in app flash and keeps working, so the
  firmware can always be updated from the browser. Update the firmware first, then
  the filesystem (the order `ota_flash.py` uses), or both in one go.
- The page loaders are gone (the sequential JavaScript loader and ADR-212's
  native-loading diagnostics). A failed page load shows the browser's error page,
  as for any other single-file page.

## Related Decisions

ADR-139 (ETag), ADR-147 and ADR-165 (file-serving limits), ADR-188 (connection
cap), ADR-190 (minified staging), ADR-191 (safeboot, unchanged), ADR-192 (v2 only),
ADR-212 (server-side file queue stays, page-side native loading replaced).

## References

- GitHub #692 (gzip proposal), PR #697 (measurements, comments 6660042343 and
  6670567252).
