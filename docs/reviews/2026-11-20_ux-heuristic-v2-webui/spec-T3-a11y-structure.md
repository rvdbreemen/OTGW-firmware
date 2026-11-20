# Spec T3: accessibility structure (TASK-2958, review F18 F19 F20 F21)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. Web UI only (`src/OTGW-firmware/data/` and its tests). Code read at origin/dev b3d14f76a.

## 1. Landmarks, headings and the skip link (F19, WCAG 2.4.1 level A, 1.3.1)

**Today** (`v2.html`):
- The main navigation is `<div class="switcher">` (`:62`), not a `<nav>`. The only `<nav>` in the page is the Settings rail (`:766`), so the one navigation landmark a screen reader finds is the wrong one.
- The content sits in `<div class="wrap">` (`:70`); there is no `<main>`.
- The product name is `<span class="big">OTGW firmware</span>` (`:28`); there is no `<h1>`. Pages have no page-level heading; Home has a decorative `<h2>` "OTGW // MISSION CONTROL" inside one view (`:311`).
- No skip link.

**New structure:**

```
<body>
  <a class="skip-link" href="#main">Skip to content</a>        <- first focusable element
  <div class="headerrow">
    <h1 class="big">OTGW firmware <span class="sub">· v2</span></h1>   <- was <span>
    ...
  </div>
  <nav class="switcher" aria-label="Main">                       <- was <div>
    <button class="seg" data-page="home" aria-current="page">…   <- see section 2
  </nav>
  <main class="wrap" id="main" tabindex="-1">                    <- was <div>
    <div id="page-home" class="page active" aria-labelledby="pgh-home">
      <h2 id="pgh-home" class="sr-only">Home</h2>
      …
```

- **Skip link:** visually hidden until focused, then shown top-left above the header (`position:fixed; top:8px; left:8px; z-index` above the header), styled as a button with the focus ring of section 4. Activating it moves focus to `<main>` (`tabindex="-1"` makes that possible without adding a tab stop).
- **`<h1>`:** one per document, the product name. CSS keeps the current look; reset the browser's h1 margin and font-size to the `.big` values.
- **Page headings:** each `.page` gets an `<h2>` with the page name, as the first child, with a new `.sr-only` class (none exists yet in `v2.css` or `ds-tokens.css`). The nav already shows the page name visually, so the heading only serves screen readers. Text from the existing nav keys (`nav.home`, `nav.sat`, `nav.monitor`, `nav.settings`, `nav.advanced`, `h_home.nav_diagnose`), so no new translations are needed.
- **The Settings rail** `<nav>` gets `aria-label="Settings sections"`, so the two navigation landmarks have distinct names.
- **Existing `<h2>`/`<h3>`/`<h4>` inside pages** stay. The card titles are `<h4>`, so a page reads h2 > h4. Changing card levels is out of scope; the headings list still works.

New CSS (design-system, ADR-091 gate):

```
.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}
.skip-link{position:absolute;left:-9999px}
.skip-link:focus{position:fixed;left:8px;top:8px;z-index:100; /* button look from the design system */}
```

Copy: `core.skip_to_content` "Skip to content" (new key), `core.nav_main` "Main" and `core.nav_settings_sections` "Settings sections" for the two aria-labels (new keys).

## 2. Active state and names (F18, WCAG 4.1.2, 1.3.1)

Four groups show "which one is active" by colour and underline only. One pattern for all of them:

| Group | Elements | Kind | Attribute on the active one | Today |
|---|---|---|---|---|
| Main nav | `.seg` (`v2.html:63-68`), toggled in `showPage()` (`v2.js:1986-1989`) | navigation between pages | `aria-current="page"` | none |
| SAT views | `.satview-btn` (`v2.html:346-348`), toggled at `v2.js:7696` and `:7748` | view switch inside a page | `aria-current="true"` | none |
| Monitor tabs | `#page-monitor .subtab` (`v2.html:486-491`), toggled in `showTab()` (`v2.js:2043-2047`) | view switch | `aria-current="true"` | none |
| Advanced tabs | `#advTabs .subtab` (`v2.html:807-…`, `data-adv`) | view switch | `aria-current="true"` | none |

- Remove the attribute from the others (`removeAttribute`, not `"false"`), in the same place where `classList.toggle('active', …)` runs today.
- The theme buttons keep `aria-pressed` (`v2.html:57-58`): they are toggles, which is the right pattern for them.

**Why not the ARIA tabs pattern** (`role=tablist`/`tab`/`tabpanel`, `aria-selected`), which the review suggested for the SAT views: a correct tabs pattern also needs a roving tabindex and arrow-key handling. Without those, `role=tab` makes things worse, because screen-reader users then expect arrow keys that do nothing. `aria-current` on plain buttons is announced ("current page", "current"), keeps the Tab order as it is, and is three lines per group. If otgw-webui wants the full tabs pattern later, that is a separate task with its own keyboard test.

**The SAT switch name.** Today the checkbox sits inside `<label class="sat-switch"><span id="satEnableLbl">Enabled</span><input …></label>` (`v2.html:342`), so its accessible name is the state text. It is announced "Disabled, checkbox, not checked", a double negative. New:
- `aria-label="SAT"` on `#satEnable` (key `core.sat_switch_name` "SAT"). The aria-label wins over the wrapping label for the accessible name.
- `#satEnableLbl` gets `aria-hidden="true"`: the checked state already says on or off, and the visible text stays for sighted users. During a takeover it reads "Off · takeover active" (spec T2).

## 3. Disabled controls look disabled (F20, H1, H5)

**Today.** `applySatLock()` (`v2.js:4785-4791`) disables the dial buttons, the presets and other SAT controls while SAT is off. Only `.fs-btn:disabled` has a disabled style (`v2.css:751`), so `.stepbtn` (dial − and +), `.sat-preset` and `.tbtn` (Detect location) keep full opacity and `cursor:pointer` (`mock-disabled-sat-control.txt`: 9 buttons).

**New:**
- One shared rule for every button in the v2 UI:

  ```
  button:disabled { opacity:.5; cursor:not-allowed; box-shadow:none; transform:none }
  ```

  It generalises the `.fs-btn:disabled` rule (`v2.css:751`, same values), so the Settings buttons do not change. WCAG 1.4.3 exempts inactive components from the contrast minimum; .5 still keeps the label legible.
- A hint in the dial card while SAT is off: "SAT is off: switch it on (top right) to change the target." Key `tools.sat_dial_off_hint` (new). Same `.sat-hint` style as the existing hints; hidden while SAT is on. The SAT settings block already shows the same message through `settings.sat_off_note`.

## 4. One focus ring (F21, H4; 2.4.7 already passes)

**Today.** Most controls get the browser's default ring (`outline-style:auto`); four places have a designed `2px solid var(--accent)` ring (`v2.css:54`, `:66`, `:449`, `:496`). `var(--accent)` fails 3:1 on the dark header in light (2.81:1, see TASK-2956).

**New:** one token and one rule.

```
:root { --focus-ring: #1e88e5 }          /* same value in light and dark */
:focus-visible { outline:2px solid var(--focus-ring); outline-offset:2px }
```

- The four existing ring rules are removed, so the single rule applies everywhere. Two of them use an inset offset (`-2px`: language menu items `v2.css:66`, group summaries `:449`); keep those offsets as overrides without their own colour, so the ring is not cut off at the edge of their container.
- **Why #1e88e5:** a ring must reach 3:1 against what is next to it (WCAG 1.4.11). The header stays #383838 in both themes, and the pages are light or dark, so the colour must pass against all of them. Measured (WCAG formula, computed in Python):

  | Background | #383838 header | #f1f7f8 light page | #ffffff | #1c1c1e dark page | #2c2c2e dark panel |
  |---|---|---|---|---|---|
  | #1e88e5 | 3.19 | 3.40 | 3.68 | 4.63 | 3.79 |
  | #0288a0 (accent, light) | 2.81 | 3.86 | 4.17 | 4.08 | 3.34 |
  | #00bffe (accent, dark) | 5.52 | 1.96 | 2.12 | 8.01 | 6.56 |

  Neither accent passes everywhere; #1e88e5 does, with the smallest margin on the header (3.19). An orange (#e65100) also passes, but would read as a warning.
- The token belongs in `ds-tokens.css`, under the ADR-091 drift gate. If TASK-2956 introduces a header-accent token, check that #1e88e5 still passes 3:1 against the active nav item's underline; a ring next to the underline is fine at 3.19 on the header itself.

## 5. Keyboard and screen-reader behaviour after the change

Tab order on any page: Skip to content → connection summary → language → Light → Dark → main nav (5 or 6) → page content. Activating the skip link puts focus on `<main>`; the next Tab goes to the first control of the active page.

A screen-reader user hears: the page title "OTGW firmware" as heading level 1, the "Main" navigation with "Home, current page", and per page a level 2 heading with the page name. On the SAT page: "SAT, switch/checkbox, checked" and "Control, current".

## 6. Acceptance (replaces the ACs of TASK-2958 where they differ)

1. The first Tab on any page focuses a visible "Skip to content" link; Enter moves focus into `<main id="main">`; the Tab-walk script (`OTGW-field-data/.../tools/mock-kbd-sat.mjs` or a test copy) shows it as stop 1.
2. The document has exactly one `<h1>` (the product name), one `<main>`, a `<nav aria-label="Main">` for the page switcher, and each `.page` starts with an `<h2>` (visually hidden) with the page name.
3. After every page change exactly one `.seg` has `aria-current="page"`; after every view change exactly one SAT view button, one Monitor tab and one Advanced tab in its group has `aria-current="true"`. A browser test checks all four groups.
4. `#satEnable`'s accessible name is "SAT" in both states, and `#satEnableLbl` is `aria-hidden`.
5. With SAT off, every disabled button (dial −/+, presets, Detect location) renders with `opacity:.5` and `cursor:not-allowed`, and the dial card shows the off hint; screenshots in light and dark.
6. One `:focus-visible` rule with `--focus-ring: #1e88e5` covers buttons, links, summaries and inputs; the Tab walk shows `outline-style: solid` on every stop; the old four ring rules are gone; ADR-091 drift gate green.
7. New EN keys (`core.skip_to_content`, `core.nav_main`, `core.nav_settings_sections`, `core.sat_switch_name`, `tools.sat_dial_off_hint`); NL and DE through the i18n sync; `tests/webui/i18n-*.test.mjs` pass.
8. No visual change for sighted users other than the disabled style, the off hint, the skip link on focus and the focus ring colour (before/after screenshots of Home, SAT and Settings in both themes).
