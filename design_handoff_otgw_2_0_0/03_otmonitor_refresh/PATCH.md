# Patch 03 — OTMonitor home page refresh

## Why

The home page (`.otmontable` / `.otmonrow`) currently uses light-blue
(`#add8e6`) card fills. After Patch 02 the SAT panel uses neutral
cards; the home page needs to follow the same vocabulary or the two
pages read as different products.

## What changes

1. Add `data/otmonitor.css` (file in this folder).
2. In `data/index.html`, link it AFTER `index_dark.css` (and after
   `sat.css` from Patch 02 — order doesn't matter between these two,
   they don't overlap):

   ```html
   <link rel="stylesheet" href="ds-tokens.css">
   <link rel="stylesheet" href="index.css">
   <link rel="stylesheet" href="index_dark.css">
   <link rel="stylesheet" href="sat.css">
   <link rel="stylesheet" href="otmonitor.css">     <!-- NEW -->
   ```

3. The boolean indicators (Flame / CH active / DHW active / Fault)
   need a `data-state` attribute so the CSS can color them:

   ```html
   <!-- before -->
   <div class="otmonrow"><span class="label">Flame</span>
        <span class="value bool-on">●</span></div>

   <!-- after -->
   <div class="otmonrow is-bool" data-state="on">
     <span class="label">Flame</span>
     <span class="value"></span>
   </div>
   ```

   Add `is-fault` to the Fault row's class list so it uses
   `--status-error` instead of `--status-ok` when on. The render code
   in `OTMonitor.jsx` (or whichever partial emits these rows in the
   current 2.0.0 build) needs the corresponding update.

## Verification

- Home page cards: white (light) or `#2c2c2e` (dark), no blue tint.
- Flame on → green dot with glow. Fault on → red dot with glow.
  Toggle theme — both swap to their dark-theme variants.
- Numbers stay aligned column-to-column when values change (tabular
  numerals are now applied globally by `ds-tokens.css`).

## Rollback

Delete `data/otmonitor.css`, remove the `<link>`, revert the
`data-state` markup change. The old `.otmonrow` rules in `index.css`
are untouched, so the home page falls back to the previous look.
