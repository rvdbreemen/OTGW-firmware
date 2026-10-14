---
id: TASK-1224
title: Build a minified LittleFS image from a staging copy of data/ (no gzip)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-14 19:41'
updated_date: '2026-10-14 22:06'
labels:
  - build
  - webui
  - esp32
milestone: m-2
dependencies: []
references:
  - docs/research/2026-10-14-4mb-ota-asset-options.md
  - build.py
priority: medium
ordinal: 354000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Goal: build.py produces a LittleFS image with minified web assets, while src/OTGW-firmware/data/ stays the untouched, readable source. No gzip (maintainer directive, FSexplorer.ino:83): every file in the image stays plain text and is served exactly as today.

Why: measured 2026-10-14 (docs/research/2026-10-14-4mb-ota-asset-options.md). Minify cuts v2.js 299,825 -> 204 KB (whitespace+syntax) or 181 KB (full), the three CSS files 152,201 -> 102,528 B. It frees room on today's >=95% full 1.5 MiB LittleFS and is a precondition for the safeboot OTA layout in epic TASK-1223 (LittleFS 768K, see TASK-1223.6).

Out of scope: dropping the classic UI (separate maintainer decision, needs its own ADR); rewriting v2.js; gzip; changing how the firmware serves files.

Approach: build.py copies data/ to a per-target staging dir under .pio/, minifies the browser assets in the staging copy only, and points the filesystem build at the staging dir (PLATFORMIO_DATA_DIR for pio buildfs; the same dir for the mklittlefs path). The source is never modified, so no backup copy of data/ is needed: git already holds it, and the build cannot overwrite it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py builds the LittleFS image from a staging copy of src/OTGW-firmware/data/; after a full build, git status shows no change under data/ (and generated files such as v2-bundle.css stay as today)
- [x] #2 Files the firmware reads itself (.hex, .ver, .json, .log, .ini, version.hash, transfer.dat) and binaries (.woff2, .png, .ico) are byte-identical in the image (sha256 compare in a test)
- [x] #3 v2.css is left out of the image (the browser only loads v2-bundle.css); no other file is dropped
- [x] #4 A minify failure fails the build with a non-zero exit and a clear message (no silent fallback to unminified files); build.py --no-minify builds an unminified image for debugging
- [x] #5 build.py prints a size report: per minified file before/after bytes, total content bytes, and the mklittlefs minimum image size
- [x] #6 Host tests (stdlib unittest, tests/) cover the staging step: source untouched, non-browser files byte-identical, minified files smaller and still parse
- [x] #7 On the bench (combo, COM4) with the minified image: v2 UI and classic UI load with no console errors, live data updates, a settings save round-trips, FSexplorer lists and downloads files, and /pic hex files match the source sha256
- [x] #8 ADR via adr-kit for the build-tooling change (minify step + new dependency), referencing the no-gzip directive and ADR-166/TASK-1223
- [x] #9 Browser assets (.js, .css) are minified in the staging copy only; HTML stays byte-identical (amended 2026-10-14: the HTML comment gain is ~5 KB, not worth the risk); tool and version are pinned and recorded
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Tool choice (first, measured): compare a Python-only minifier (rjsmin + rcssmin, installed in the build.bat venv, no Node) with esbuild (pinned, needs Node/npx). Measure v2.js/CSS output size and LittleFS minimum for both. Default to the Python-only tool unless esbuild saves meaningfully more (esbuild measured: whitespace+syntax 204 KB, full 181 KB). Record the choice in the ADR.
2. build.py: new stage_data_dir(target) -> .pio/data-staging/<env>/. Copy data/ (after generate_v2_css_bundle), drop v2.css, minify .js/.css (and conservative .html comment strip) in place in the staging copy, verify every other file byte-identical.
3. Point the filesystem build at the staging dir: PLATFORMIO_DATA_DIR env for build_filesystem_pio(); the -c argument for the mklittlefs path in build_filesystem(). Keep the TASK-337 artifact checks.
4. --no-minify flag: stage without minify (copy only), for debugging.
5. Size report: per-file before/after, totals, mklittlefs minimum (same bisect as the TASK-1222 spike).
6. Host tests in tests/ (stdlib unittest).
7. build.bat build, flash fs to the bench (settings backup first: a fs write resets settings), verify with the otgw-device-verify agent: both UIs, FSexplorer, settings round-trip, /pic sha256.
8. ADR via adr-kit, docs (build guide), evaluator green, version bump (data assets change what ships).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-14 tool research (CLAUDE.md "reuse open source first"):
- Candidates: esbuild 0.24.0 (Go, via npx/Node) and rjsmin 1.3.0 + rcssmin 1.3.0 (Python, C-accelerated, Apache-2.0, pip). Both mature, maintained.
- Size on v2.js: esbuild whitespace-only 207,107 B, +syntax 203,602 B, full (renames locals) 180,725 B; rjsmin 208,052 B. CSS total (bundle+components+tokens): esbuild 102,528 B; rcssmin 102,668 B.
- Equivalence: rjsmin output of all 7 JS files parses (node --check) and its esbuild-normalised form is byte-identical to the esbuild-normalised original (sha256 equal) -> no semantic change. rcssmin differs only in whitespace after colons/commas and .10 vs .1 in custom properties (equivalent).
- Choice: rjsmin/rcssmin. Python-only, installs through the existing requirements-build.txt hook of build.bat/build.sh, no Node dependency. Cost: ~27 KB more v2.js than esbuild full minify.
- HTML: left byte-identical (AC amended); v2.html comments are ~5.3 KB (an earlier 44 KB figure was a greedy-regex measuring error).
- Found while testing: ds-tokens.css comment closed early and dropped --hot (pre-existing bug, now TASK-1225); joined by a minifier it would also drop --cold/--dhw/--gauge-track. Fixed in source first; test_build_staging.py guards it.

Bench 2026-10-15, combo .88.61, fs-only flash of build/OTGW-firmware-esp32-combo-2.0.0-alpha.420+dddc423.littlefs.bin (hash verified). The baseline is the unminified image (.tmp/bench-1224/baseline/); the after run is in .tmp/bench-1224/after/.
- Classic UI (ui_usev2=false on defaults): 0 page errors, live OT data shown. One 503 on a font, from the file-serve gate (ADR-147).
- settings.ini restored by upload + reboot: sha256 identical, 0 of 182 keys differ from the pre-flash settings.
- v2 UI: 0 console errors/warnings, 0 failed requests, --hot fixed.
- FSexplorer: 36 rows. v2.css absent, v2-bundle.css present. The components.css 503 is the same as in the baseline.
- Settings round-trip OK.
- PIC hex sha256 identical to source.
- The device serves v2.js 208,052 B and index.js 253,779 B, so it runs the minified build.
- Cosmetic: the classic page shows a firmware/filesystem hash mismatch banner, because data/version.hash carried the uncommitted build stamp. This is not caused by minify.
Fresh host runs 2026-10-15: test_build_staging 11 OK, test_build 9 OK, evaluate --quick 100% / 0 fail. ADR-190 strict lint PASS. Docs: BUILD.md and c4-code-web-assets.md updated.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py now builds the LittleFS image from .pio/data-staging, a copy of src/OTGW-firmware/data/. In that copy v2.css is left out, .js is minified with rjsmin 1.3.0 and .css with rcssmin 1.3.0 (pinned in requirements-build.txt). Every other file stays byte-identical, and data/ is never modified. A minifier error exits 1; --no-minify builds a debug image. The build prints a size report with the mklittlefs minimum. LittleFS minimum went from 1460 to 1080 KiB with both UIs (332,441 B saved). Without the classic UI it is 668 KiB, which fits the 768K safeboot layout (TASK-1223).

Evidence per AC:
1. build.bat log shows 'Building FS image from .pio\data-staging'. git status data/ shows only the TASK-1225 fix, the regenerated bundle and the build stamp.
2. Unpacked image equals staging (42 files). Hex, log, fonts and html have source sha. test_non_browser_files_byte_identical.
3. test_excluded_files_not_staged and test_nothing_else_dropped. v2.css is absent on the device.
4. The prepare tests exit 1 on RuntimeError and ImportError. The --no-minify build gave 1400 KiB.
5. Size report in .tmp/build-1224.log.
6. tests/test_build_staging.py, 11 OK.
7. Bench before/after: both UIs, FSexplorer, settings round-trip + restore, PIC sha (.tmp/bench-1224/).
8. ADR-190 (Proposed, strict lint pass). Acceptance waits on the maintainer.
9. rjsmin/rcssmin pinned. HTML byte-identical.

Found and fixed TASK-1225 along the way.
<!-- SECTION:FINAL_SUMMARY:END -->
