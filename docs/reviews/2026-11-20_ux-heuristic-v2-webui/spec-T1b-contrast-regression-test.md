# Spec T1b: contrast regression test (TASK-2956, extends spec T1)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. One new test file, `tests/webui/contrast-tokens.test.mjs`; no change to the web UI itself. It turns the measurements of spec T1 into a gate, so a later token or CSS change that breaks contrast goes red.

## 1. Two layers

| Layer | What it checks | Why |
|---|---|---|
| **A. Token pairs** | every row of spec T1 section 2 (19 rows, light and dark): the foreground and background of that context, resolved by the real CSS in the browser, against its required ratio | deterministic; catches a token change even when no element in the mock happens to show that state |
| **B. Rendered scan** | every visible text element on Home, SAT (three views) and Settings, light and dark, plus the state fixtures of section 4 | catches a rule that uses the wrong token, which layer A cannot see (the ADR-091 gate does not check tokens either) |

Both layers run in one Chrome through `startPage()` from `tests/webui/lib/page-harness.mjs`, as `mission-control-follows-theme.test.mjs` does.

## 2. Resolving colours correctly

The review's scanner (`mock-contrast.mjs`) misread a `color-mix(…, transparent)` background: `getComputedStyle` returns `color(srgb r g b / a)` for it, which a simple `rgb()` parser gets wrong (the tag measured 2.13 instead of 2.59). The test must not repeat that:

- **Normalise every colour through a canvas:** `ctx.fillStyle = cssColor; ctx.fillRect(0,0,1,1); getImageData(0,0,1,1)` gives the sRGB bytes and alpha for any CSS colour syntax (`rgb()`, `color(srgb …)`, `color-mix()` results). Use a fresh 1×1 canvas per colour (cleared, so alpha reads correctly).
- **Background:** walk up from the element, collect every background colour with alpha > 0 until the first opaque one, then composite them bottom-up over that opaque colour (alpha blending in sRGB, which is how `color-mix(in srgb …)` mixes). If no ancestor is opaque, composite over the page background `--bg`.
- **Foreground with alpha:** composite over the computed background.
- **Elements over images or gradients** (`background-image` not `none` on the element or an ancestor before the opaque one): skip, and list them in the test output as "not measured", so the gap is visible.
- **Ratio:** WCAG 2.x relative luminance. Large text (>= 24 px, or >= 18.66 px with weight >= 700) needs 3:1, all other text 4.5:1.

## 3. Layer A: token pairs

A table in the test, one entry per row of spec T1 section 2, for example:

```js
// [row, theme, fg, bg, min]  fg/bg: a CSS colour expression resolved on a probe element
['A2', 'light', 'var(--accent)',            'var(--bg)',                                              4.5],
['B1', 'light', 'var(--nav-accent)',        'var(--nav)',                                             4.5],
['C2', 'dark',  'var(--on-accent)',         'var(--accent)',                                          4.5],
['D2b','light', 'var(--status-warn-text)',  'color-mix(in srgb, var(--zone-warn) 20%, var(--bg))',    4.5],
['D4', 'dark',  'var(--status-warn-text)',  'color-mix(in srgb, var(--zone-warn) 20%, var(--panel))', 4.5],
['E2', 'dark',  'var(--on-hot)',            'var(--hot-fill)',                                        4.5],
['F1', 'light', 'var(--hot-text)',          'color-mix(in srgb, var(--hot) 16%, var(--panel))',       4.5],
```

- Each expression is set on a probe `<div>` (`style.color` / `style.backgroundColor`) under `<html data-theme="…">`, read back with `getComputedStyle`, and normalised through the canvas (section 2).
- **Missing token = explicit failure.** Before measuring, check every custom property the row names with `getComputedStyle(document.documentElement).getPropertyValue('--name')`. If it is empty, fail the row as "token --name not defined". An undefined `var()` would otherwise fall back to an inherited colour and give a meaningless ratio.
- **Tight passes get a margin check too:** D4 (4.95) and A2 (5.38) are the closest to the line. The test asserts the 4.5 minimum, and also prints the measured ratio for every row, so a review sees the margin shrink before it fails.
- The table rows are the spec's rows: when spec T1 changes, the table changes in the same commit.

## 4. Layer B: rendered scan and state fixtures

Pages and views: Home, SAT Thermostat, SAT Control, SAT Technical, Settings; light and dark; width 1400. Mock `api` answers, so every text that matters is on screen:

| Fixture | API answer | Makes visible |
|---|---|---|
| SAT heating | `/v2/sat/status`: `{ enabled: true, active: true, boiler_status: "heating", target_temp: 20, room_temp: 19.5 }` | the Heating pill (rows E1, E2) |
| SAT idle | same with `boiler_status: "idle"` | the Idle pill (E3, E4) |
| SAT off | `{ enabled: false }` | the Off pill |
| SAT source tag | any SAT status with `final_setpoint` set | the "SAT → boiler" tag (F1, F2) |
| Beta banner | `/v2/device/info` with `fwversion: "2.0.0-beta.1"` (`isPrereleaseFw()`, `v2.js:6939-6941`, read at `:6961`), and no `otgw-v2-satbeta-dismissed` in sessionStorage | the warn banner (D1, D3) |
| Reboot badge | a settings answer with one setting that needs a reboot (as in `tests/webui/settings-reboot-badge.test.mjs`) | the warn badge (D2) |

- **Assertion:** every measured text element meets its minimum, except disabled controls (`:disabled` or `aria-disabled="true"`, exempt under WCAG 1.4.3) and elements listed as "not measured" (section 2).
- **Output on failure:** one line per failing element, with the page/view, theme, selector, text (first 30 characters), fg, bg, ratio and minimum, so otgw-webui can find the rule without rerunning anything.
- **Deduplicate** by (text, fg, bg) per view, as `mock-contrast.mjs` does, so a list of 50 identical links reports once.

## 5. Proving it works (red, then green)

- **Red:** run the test on the code from before TASK-2956 (`OTGW_DATA_DIR` pointed at a copy of `src/OTGW-firmware/data` from origin/dev before the token change). Expected:
  - Layer A fails on A1, A2, A3, C1 (old light `--accent` #0288a0) and D2b (old `--status-warn-text` #9c5700: 4.44). It reports "token not defined" for B1, C2, E1, E2 and F1, F2 (`--nav-accent`, `--on-accent`, `--hot-fill`, `--on-hot`, `--hot-text` do not exist yet).
  - Layer A does **not** fail D1, D2 and E3 on the old code: `--status-warn-text` #9c5700 already passes the banner and the surface, and E3's proposed `--pill-off` is also missing (so it reports "token not defined" too). The old failures there come from the rules, which layer B catches.
  - Layer B lists at least the active theme buttons, the active nav item, the warn banner and badge, the SAT pill in all three states and the tag.
  - Paste that output into the task notes.
- **Green:** the same test on the TASK-2956 change passes with 0 failures in both layers.
- **Mutation check:** set `--status-warn-text` back to #9c5700 in a scratch copy. Layer A must fail on D2b (4.44). This proves the narrow rows are really checked.

## 6. Runtime and flakiness

- One Chrome, both themes switched in the same page (`data-theme` attribute plus a re-render), so the test takes about as long as one other webui browser test.
- Not in the `scripts/run_tests.py` flaky list (no timing dependency): it waits for each view with `h.until(...)` on a concrete element, not on a sleep.

## 7. Acceptance (added to TASK-2956)

1. `tests/webui/contrast-tokens.test.mjs` exists with layer A (every row of spec T1 section 2) and layer B (the pages, themes and fixtures of section 4), with colours normalised through a canvas as in section 2.
2. Red on the pre-TASK-2956 data (the failing rows of section 5 listed in the task notes), green on the change, and the D2b mutation check fails as described.
3. It runs in `python scripts/run_tests.py --webui` and is not on the flaky list.
