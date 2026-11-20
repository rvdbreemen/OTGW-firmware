# Spec T1: contrast tokens per context (TASK-2956, review F1 F2 F16 F17)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. Files: `src/OTGW-firmware/data/ds-tokens.css` and `v2.css` only. Code read at origin/dev b3d14f76a.

Every ratio below uses the WCAG 2.x relative-luminance formula, computed in Python from the hex values. The composite backgrounds (`color-mix(... N%, transparent)`) were computed over the surface they sit on. Text needs >= 4.5:1 (WCAG 1.4.3); none of the texts here is "large" (all are below 18.66 px bold).

## 1. How the colours are wired today

- `ds-tokens.css` defines the palette per theme (`:root, [data-theme="light"]` from line 45, `[data-theme="dark"]` from line 170).
- `v2.css:6-19` maps it to the UI names: `--accent: var(--accent-primary)` (light #0288a0), `--nav: #383838` (the header in **both** themes), `--muted: var(--fg-4)`.
- `v2.css:25` overrides dark: `--accent: var(--brand-cyan)` (#00bffe), not the generic dark `--accent-primary` #4a90e2.
- `--zone-warn` = `--status-warn` (light #ff9800, dark #ffa726). `ds-tokens.css:101/215` already has `--status-warn-text` (light #9c5700, dark #f0b35d), but nothing in `src/` uses it.
- `.sat-pill` is white on `--hot` while heating and white on `--tile-off` when idle or off (`v2.css:763-764`). `.sat-tile .tag` is `--hot` on a 16 % `--hot` tint (`v2.css:825`).
- `v2.css:68` already fixes one case in dark: `.lang-menu li a.active{color:#0d2026}` ("white on the dark-theme cyan is below 4.5:1"). This spec generalises that fix.

## 2. Measured today, and the proposal

| # | Context | Theme | Today: fg on bg | Ratio | Proposal: fg on bg | Ratio |
|---|---|---|---|---|---|---|
| A1 | Accent text on a surface (links, active SAT view tab, "Raw data") | light | #0288a0 on #ffffff | 4.17 | #016f83 on #ffffff | 5.83 |
| A2 | same, on the page background | light | #0288a0 on #f1f7f8 | 3.86 | #016f83 on #f1f7f8 | 5.38 |
| A3 | same, on the faint row tint (`--brand-faint`) | light | #0288a0 on #f0fbff | 3.96 | #016f83 on #f0fbff | 5.54 |
| A4 | Accent text on a surface | dark | #00bffe on #2c2c2e | 6.56 | unchanged | 6.56 |
| A5 | same, on the page background | dark | #00bffe on #1a1a1a | 8.19 | unchanged | 8.19 |
| A6 | same, on the alt surface | dark | #00bffe on #3a3a3c | 5.34 | unchanged | 5.34 |
| B1 | Active main-nav item on the header | light | #0288a0 on #383838 | **2.81** | #00bffe on #383838 | 5.52 |
| B2 | same | dark | #00bffe on #383838 | 5.52 | unchanged | 5.52 |
| C1 | White text on an accent fill (active Light button, primary buttons, active preset) | light | #ffffff on #0288a0 | 4.17 | #ffffff on #016f83 | 5.83 |
| C2 | Text on an accent fill (active Dark button, primary buttons, active preset) | dark | #ffffff on #00bffe | **2.12** | #0d2026 on #00bffe | 7.90 |
| D1 | Warn text on the warn banner (16 % tint over the page) | light | #ff9800 on #f3e8d0 | **1.77** | #8a4b00 on #f3e8d0 | 5.59 |
| D2 | Warn text on a surface (reboot badge, dirty dot, pill) | light | #ff9800 on #ffffff | **2.16** | #8a4b00 on #ffffff | 6.80 |
| D2b | Warn text on the 20 % tint (`.dir-tb`, `v2.css:369`), worst case over the page | light | #ff9800 on #f4e4c6 | **1.72** | #8a4b00 on #f4e4c6 | 5.43 |
| D3 | Warn text on the warn banner | dark | #ffa726 on #3f311c | 6.48 | #f0b35d on #3f311c | 6.78 |
| D4 | Warn text on the 20 % tint over a dark panel (worst case in use) | dark | #ffa726 on #56452c | 4.73 | #f0b35d on #56452c | 4.95 |
| E1 | SAT pill "Heating" | light | #ffffff on #ff5a3c | **3.10** | #ffffff on #c43e1f | 5.18 |
| E2 | SAT pill "Heating" | dark | #ffffff on #ff6a4d | **2.83** | #1c1c1e on #ff6a4d | 6.01 |
| E3 | SAT pill "Idle" / "Off" | light | #ffffff on #9e9e9e | **2.68** | #ffffff on #6b6f73 | 5.06 |
| E4 | SAT pill "Idle" / "Off" | dark | #ffffff on #54565a | 7.35 | unchanged | 7.35 |
| F1 | Source tag "SAT → boiler" (16 % `--hot` tint over the tile) | light | #ff5a3c on #ffe5e0 | **2.59** | #b3261e on #ffe5e0 | 5.46 |
| F2 | same | dark | #ff6a4d on #4e3633 | **3.91** | #ffab99 on #4e3633 | 6.07 |

Bold = fails today. Rows E1, E2 and F2 are new: the review measured only the light Off pill and the light tag.

**Correction to review F17:** the tag today measures 2.59:1, not 2.13:1. The browser scan read the `color-mix()` background wrongly as #d6d6d6; the review's caveat ("pixel check first") was right. The fix and the AC do not change.

The narrowest pass is D4 (4.95, dark, 20 % tint over a panel). Tints stronger than 20 % would push it below 4.5; keep warn tints at 16-20 %.

## 3. Tokens: what is new, what changes

**Recommended: one accent per theme, plus three small tokens.**

| Token | File | Light | Dark | Status | Used by |
|---|---|---|---|---|---|
| `--accent` | `v2.css:16` | `#016f83` (was `var(--accent-primary)` #0288a0) | `var(--brand-cyan)` #00bffe (unchanged, `v2.css:25`) | **changed (light)** | the 22 text rules and 15 fill rules that use `--accent` today |
| `--nav-accent` | `v2.css` `:root` | `var(--brand-cyan)` #00bffe | same | **new** | `.seg.active` (`v2.css:115`): `border-bottom-color` and `color` |
| `--on-accent` | `v2.css` `:root` / dark override | `#ffffff` | `#0d2026` | **new** | every rule with `background:var(--accent)` and `color:#fff`: `v2.css:43, 67, 177, 351, 519, 752, 795, 804, 880, 892`; replaces the one-off fix at `:68` |
| `--status-warn-text` | `ds-tokens.css:101/215` | `#8a4b00` (was #9c5700) | `#f0b35d` (unchanged) | **exists, light value changed, now used** | the 6 rules with `color:var(--zone-warn)`: `v2.css:285, 369, 478, 491, 583, 670`. Today no file other than `ds-tokens.css` uses this token (grep over `src/`), so the change is safe. #9c5700 would pass the 16 % banner (4.57) but fail the 20 % tint over the page (4.44). |
| `--hot-fill` | `ds-tokens.css` | `#c43e1f` | `var(--hot)` #ff6a4d | **new** | `.sat-pill` background while heating (`v2.css:763`) |
| `--on-hot` | `ds-tokens.css` | `#ffffff` | `#1c1c1e` | **new** | `.sat-pill` text (`v2.css:763`) |
| `--pill-off` | `v2.css` `:root` / dark override | `var(--fg-3)` #6b6f73 | `#54565a` (= dark `--tile-off`) | **new** | `.sat-pill.idle, .sat-pill.off` (`v2.css:764`); `--tile-off` itself stays, the LEDs keep it |
| `--hot-text` | `ds-tokens.css` | `#b3261e` | `#ffab99` | **new** | `.sat-tile .tag` text (`v2.css:825`); the tint keeps `--hot` |

Unchanged on purpose: `--accent-primary` (other consumers of `ds-tokens.css`), `--brand-cyan`, `--hot` (chart lines and the flow arrow, which are graphics and need 3:1, not 4.5:1), `--zone-warn` (borders, tints and icons keep the bright orange), `--tile-off`.

**Why change `--accent` instead of adding a separate text token.** In light, every use of `--accent` behind or as text fails (A1-A3, C1). A separate `--accent-text` would leave 37 rules to sort into "text" and "decoration" by hand, and new code would pick the wrong one. One darker accent fixes all of them at once. The cost is a slightly darker cyan for decorative fills too (the progress bar, the switch, the chart legend): #016f83 instead of #0288a0. The dark theme does not change.

**Alternative (if the maintainer wants #0288a0 kept for decoration):** keep `--accent` and add `--accent-strong` (light #016f83, dark #00bffe) for text and text-bearing fills. Same ratios, but every one of the 37 rules must be classified, and the ADR-091 gate cannot catch a wrong pick (see section 4).

## 4. ADR-091 consequence

The ADR-091 gate (`check_design_system_drift` in `evaluate.py`) checks that every class emitted by HTML or JS has a selector in `data/*.css`. It does **not** check token values. So:

- This spec adds no class, and the gate stays green by construction. Run `python evaluate.py --quick` anyway: it must show no new FAIL.
- What the gate cannot see (a wrong token on a text rule) is covered by the contrast check of section 5 instead.
- Tests that pin colours: `tests/webui/sat-chart-actual-flow.test.mjs:70` asserts `stroke="#00bffe"` for the modulation area. It probably runs with a stubbed palette; run the whole `tests/webui` suite and check that file in particular after the change.

## 5. Acceptance (replaces the ACs of TASK-2956 where they differ)

1. The tokens of section 3 exist with the values given; `--accent` light is #016f83 (or, if the maintainer picks the alternative, `--accent-strong` is used by every text and text-bearing fill rule).
2. Every row of section 2 measures at least the ratio in the Proposal column, checked with `mock-contrast.mjs` (light and dark) and, for the composite backgrounds (D1, D3, F1, F2), by pixel sampling on a screenshot; 0 FAIL lines in both themes on Home, SAT (three views) and Settings.
3. The pill is checked in all three states (heating, idle, off) in both themes; the heating state needs a status fixture with a firing `boiler_status`.
4. `v2.css:68` (`.lang-menu li a.active{color:#0d2026}`) is replaced by `--on-accent`, with no visual change.
5. `python evaluate.py --quick` shows no new FAIL (ADR-091 class drift included), and the full `tests/webui` suite passes.
6. Before/after screenshots of Home, SAT Control and Settings in light and dark show only the intended colour changes.
