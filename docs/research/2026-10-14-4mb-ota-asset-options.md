# Options report: making room for safeboot OTA on 4MB boards without gzip

- Date: 2026-10-14
- Task: TASK-1222 (spike), epic TASK-1223
- Builds on: `2026-10-14-spike-1222-safeboot-ota-4mb.md`
- Constraint (maintainer, 2026-10-14): no gzipped web assets.

## 1. Summary

Safeboot OTA fits on 4MB without gzip if the classic web UI is dropped **and**
the v2 UI is minified at build time. That gives the same firmware headroom
(270,464 B) as the gzip option. Dropping the classic UI alone also fits, but it
leaves the combo firmware only 73,856 B (3.6%) to grow.

Fixed numbers: usable app + filesystem flash is 3968 KiB; the combo app is
2,023,296 B; the safeboot updater needs a 960K factory partition (946,327 B).

## 2. Measured LittleFS minimum per asset set

Method: `mklittlefs -b 4096 -p 256`, smallest image size that builds, 4 KiB
steps. All variants uncompressed.

| Asset set | What changes | Content | LittleFS minimum |
|---|---|---|---|
| Today | both UIs, 43 files | 1,378,195 B | 1460 KiB |
| Drop classic UI | remove index.html, index.js, graph.js, sat.js, sat-slider.js, theme-toggle.js, echarts-theme.js, design.html | 793,515 B | 868 KiB |
| + bundle-only v2 CSS | do not ship v2.css (the browser loads v2-bundle.css) | 733,864 B | 808 KiB |
| + minify v2 (rjsmin/rcssmin, as built by TASK-1224) | v2.js 299,825 to 208,052 B; CSS minified with rcssmin | | **668 KiB** |
| (alternative: esbuild full minify) | v2.js 299,825 to 180,725 B; v2-bundle.css, components.css, ds-tokens.css 152,201 to 102,528 B | 565,091 B | 644 KiB |
| + system fonts | drop the 3 self-hosted woff2 fonts (69 KB) | about 496 KB | 564 KiB |
| Reference: gzip browser assets | rejected, shown for comparison | 504,911 B | 604 KiB |

Kept in every variant: FSexplorer.html with components.css and ds-tokens.css,
the PIC firmware .hex files, the simulation log, and the small images.

## 3. What fits

Factory (safeboot) 960K at 0x10000 in every layout. The app slot must be a
multiple of 64K. Runtime room = LittleFS size minus the measured minimum, for
settings.ini, logs and LittleFS copy-on-write. Today the bench has about 75 KiB
of runtime room.

| Option | App slot (ota_0) | Combo headroom | LittleFS | Runtime room |
|---|---|---|---|---|
| Drop classic UI | 2048K | 73,856 B (3.6%) | 960K | 152K |
| Drop classic UI, tighter FS | 2112K | 139,392 B (6.9%) | 896K | 88K |
| Drop classic + minify v2 | **2240K** | **270,464 B (13.4%)** | 768K | 100K (rjsmin; 124K with esbuild) |
| Drop classic + minify + system fonts | 2240K | 270,464 B | 768K | 204K |
| Reference: gzip | 2240K | 270,464 B | 768K | 164K |

## 4. Options with pros and cons

### Option 0: Keep today's layout (firmware updates stay USB-only)

Pros
- No work, no migration, no risk.
- Both web UIs stay available.

Cons
- Firmware OTA stays impossible on every 4MB board (ADR-166, TASK-959).
- Every firmware update needs a laptop and a USB cable at the unit.
- Today's LittleFS is already at least 95% full; that problem remains.

### Option 1: Drop the classic UI only

Pros
- Simplest change that fits: delete files, no new build step.
- One UI to maintain, test and document.
- Files on the device stay readable as written.

Cons
- Combo headroom is only 73,856 B (3.6%); a few features will fill it, then the
  firmware no longer fits and the layout must change again (another USB
  migration).
- Users who still run the classic UI lose it; needs an ADR and an announcement.
- Removes `settings.ui.bUseV2` and the index.html fallback in FSexplorer.ino;
  open classic-UI work (for example TASK-1219) is dropped.

### Option 2: Drop the classic UI and minify the v2 UI (recommended)

Pros
- Same headroom as gzip (270,464 B) without gzip: files stay plain text and
  are served exactly as today, so the no-gzip rule holds.
- Minify runs in build.py, like the existing v2-bundle.css generation; the
  readable source stays in git.
- 124K runtime room, more than the bench has today.

Cons
- Everything from Option 1 about losing the classic UI.
- Minified code is hard to read in the browser on the device; debugging needs
  the source from git (or a source map kept off the device).
- Adds a build dependency (esbuild or similar) and a build step that can fail.
- Not yet proven: the minified v2.js has not been run on a device. esbuild
  renames local variables, so a full v2 UI check on the bench is required.

### Option 3: Option 2 plus system fonts

Pros
- 80 KiB extra runtime room (204K).
- Fewer requests at page load (three font fetches fewer under the 2-request cap).

Cons
- Visual change: the UI falls back to system fonts, which differ per OS.
- The extra room is not needed for the layout; only useful if logs grow.

### Option 4: Gzip browser assets (reference only)

Pros
- Keeps both UIs.

Cons
- Rejected by the maintainer (FSexplorer.ino:83, "no build-time gzip").
- Changes how the firmware serves files (Content-Encoding handling).

### Option 5: Wait for an 8MB module

Pros
- Native A/B OTA with two full app slots: a failed upload never destroys the
  running firmware.
- No updater app to build or maintain.

Cons
- Needs a new board revision; the existing 4MB fleet never benefits.
- Unknown timeline.

## 5. Common to options 1 to 3

- One USB migration per field unit (partition table and LittleFS offset);
  settings are lost unless backed up.
- With a single app slot, a failed or interrupted upload destroys the old
  firmware. The unit stays recoverable over WiFi through safeboot, but does not
  act as a gateway until a good image is uploaded.
- Needs a new ADR that amends ADR-166, plus one for retiring the classic UI.

## 6. Not yet measured

- Correction: an earlier version of this report said v2.html is mostly
  comments and that stripping them would save 30 to 40 KB. Measured, its
  comments are about 5 KB. TASK-1224 keeps HTML byte-identical.
- Whether the minified v2 UI works end to end on the device: see section 8.

## 7. Optimization plan for the v2 UI (no gzip)

Remove what the browser never loads, minify at build time, keep the readable
source in git; rewriting code comes last and is probably not needed. Steps 1 to
5 together give the measured 644 KiB with esbuild, or 668 KiB with the rjsmin
build that was implemented (section 8); steps 6 to 8 should save roughly
another 80 KiB with no visible change (estimate).

| # | Step | Gain | Risk | Status |
|---|---|---|---|---|
| 1 | Stop shipping the classic UI files | 1460 to 868 KiB | Functional: classic UI users lose it | measured |
| 2 | Stop shipping v2.css (already inside v2-bundle.css) | 60 KiB | None | measured |
| 3 | Minify v2.js, whitespace + syntax only | 300 to 204 KB | Very low: no names renamed | measured |
| 4 | Full minify of v2.js (rename local names) | another 23 KB, to 181 KB | Low | measured |
| 5 | Minify CSS (bundle, components, tokens) | 152 to 103 KB | Very low | measured |
| 6 | Replace components.css (67 KB, 212 selectors; FSexplorer uses 5 classes) | about 40 to 60 KB | Low | estimate |
| 7 | Strip comments and indentation from v2.html | about 10 KB | Very low | estimate |
| 8 | Subset the fonts to Latin (pyftsubset) instead of dropping them | about 30 KB | Low | estimate |

How to build it:

- In build.py, the same way generate_v2_css_bundle() works today: readable
  source in git, minified output in a build staging folder for the LittleFS
  image. Files stay plain text.
- Pin the minifier to one version (implemented with rjsmin/rcssmin, see
  section 8); a failed minify fails the build instead of
  silently shipping large files.
- v2.js loads as a classic script, not a module, so esbuild renames no globals
  and no properties; inline handlers keep working. Still to be proven on the
  bench.
- Point FSexplorer at v2-bundle.css or give it a 2 KB stylesheet of its own.
- A budget gate in build.py (TASK-1223.6) checks the image against the
  partition with a fixed runtime margin.

Not worth it: rewriting or deduplicating v2.js (high effort, unknown gain;
minify already removes the 690 comment lines of 4486), or adopting a bundler
such as Vite.

Open questions:

- Are pic16f88/gateway-4.3.hex (23.5 KB) and otgw_simulation.log (12.7 KB)
  needed in the release image?
- Steps 2 to 5 help today's full filesystem too, independent of the classic UI
  decision.

## 8. Implemented: TASK-1224 (ADR-190, Proposed)

Steps 2, 3 and 5 are built into build.py, for both UIs, without dropping the
classic UI:

- build.py copies `data/` to `.pio/data-staging/`, leaves out v2.css, minifies
  `.js` with rjsmin 1.3.0 and `.css` with rcssmin 1.3.0 (pinned in
  `requirements-build.txt`), and builds the LittleFS image from that copy.
  `data/` is never modified. HTML, `.hex`, `.json`, `.log`, `.ini`, fonts and
  images stay byte-identical.
- rjsmin and rcssmin were chosen over esbuild: they are Python-only (no Node.js
  in the build). The price is 23 KB on v2.js (208,052 B vs 180,725 B), so the
  "drop classic + minify" variant measures 668 KiB instead of 644 KiB.
- Measured, both UIs still shipped: LittleFS minimum 1460 to **1080 KiB**,
  332,441 B saved over 10 files. Today's 1.5 MiB partition goes from about
  75 KiB to about 456 KiB runtime room.
- `--no-minify` builds an unminified debug image; a minifier error fails the
  build.
- This work also found TASK-1225: a comment in ds-tokens.css that closed early
  dropped the `--hot` colour token in the browser. Fixed in the source.
