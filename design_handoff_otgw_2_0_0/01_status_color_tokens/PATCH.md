# Patch 01 — Status color tokens

## Why

The 2.0.0 branch has at least three different greens (`#4caf50`,
`#27ae60`, `#2e7d32`) and two reds (`#f44336`, `#e74c3c`) all mapped to
the same semantic role ("OK" / "error"). This patch consolidates them
into five `--status-*` token pairs (one foreground, one background) in
`ds-tokens.css`, with body.dark overrides for the dark theme.

## What changes

Replace `data/ds-tokens.css` with the file in this folder. The font
@font-face block is unchanged — only the `:root {…}` and a new
`body.dark {…}` block at the bottom are added.

## Apply

```sh
cp 01_status_color_tokens/ds-tokens.css \
   <repo>/src/OTGW-firmware/data/ds-tokens.css
```

## Then sweep callers (in the same commit)

Find-and-replace across `data/index.css` and `data/index_dark.css`:

| Find (any occurrence)             | Replace with                  |
|-----------------------------------|-------------------------------|
| `background: #4caf50;`            | `background: var(--status-ok);` |
| `background: #27ae60;`            | `background: var(--status-ok);` |
| `background: #4CAF50`             | `background: var(--status-ok)` |
| `box-shadow: 0 0 8px #4caf50;`    | `box-shadow: 0 0 8px var(--status-ok-glow);` |
| `background: #f44336;`            | `background: var(--status-error);` |
| `background: #e74c3c;`            | `background: var(--status-error);` |
| `background: #ff9800;`            | `background: var(--status-warn);` |
| `background: #2196f3;`            | `background: var(--status-info);` |
| `background: #9e9e9e;`            | `background: var(--status-neutral);` |

Do **not** sweep blindly — leave the literal hex in:

- the **simulation purple** badge (`#4a148c` / `#e1bee7`) — distinct
  semantic role
- the **modulation / boiler-water gradients** (literal hex in graphs)
- inline SVG fill attributes inside `index.html`

When in doubt, search for the hex in `data/index.css` and `data/index_dark.css`
**only** — leave `index.html`, `index.js`, `graph.js`, `sat.js` alone.
The JS files reference colors via ECharts theme strings, not raw CSS.

## Why scope so tight

The status tokens are only consumed via CSS today. Burning them into
JS makes the next refactor harder. If you later want JS-driven badge
colors, expose them via `getComputedStyle(document.body).getPropertyValue('--status-ok')`
and we'll have one source of truth.

## Verification

After replacing the file, in DevTools console:

```js
getComputedStyle(document.body).getPropertyValue('--status-ok').trim()
// "#2e7d32" in light, "#66bb6a" in dark
```

Toggle dark mode — the value should swap. Every `ws-connected`,
`sat-health-ok`, `btn-active` should track.

## Rollback

`git checkout HEAD -- data/ds-tokens.css data/index.css data/index_dark.css`
