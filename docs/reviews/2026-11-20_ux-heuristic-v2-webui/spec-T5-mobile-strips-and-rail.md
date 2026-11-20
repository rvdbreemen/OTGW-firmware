# Spec T5: scroll hints at 390 px and the Settings rail (TASK-2960, review F8 F9)

Author: otgw-ux-designer, 2026-11-20. Builder: otgw-webui. Web UI only (`v2.css`, `v2.js`, `v2.html`). Code read at origin/dev b3d14f76a.

## 1. Horizontal strips without a scroll hint (F8)

**Today.** These strips scroll sideways, but nothing shows that more items sit off-screen:

| Strip | Rule | Where it cuts off at 390 px |
|---|---|---|
| Main nav `.switcher` | `v2.css:107-108` | "Advan…" |
| SAT views `.sat-views` | `v2.css:767` | fits at 390 px today; same pattern, so same treatment |
| Monitor / Advanced tabs `.subtabs` | `v2.css:321` | depends on language; NL and DE are longer |
| Settings rail below 900 px `.set-rail` | `v2.css:535` | "MQT…" |
| Home chips `.mobile-strip` | `v2.css:223` | Pressure chip cut off |

**New, one shared behaviour** for all five, through one class `.hscroll` added next to the existing class (no rule is removed):

- **Edge fade:** a 24 px gradient on the side where content is hidden, made with `mask-image: linear-gradient(...)`. A mask works on any background (the dark nav, a light page), so no colour per theme is needed. Three states, set by a small script as a `data-overflow` attribute: `end` (more on the right, fade right), `both`, `start` (fade left), or none.
- **The script:** one function `syncHScroll(el)` that compares `scrollLeft`, `clientWidth` and `scrollWidth` (1 px tolerance). It runs on `scroll` (passive listener), on `resize`, and after any render that changes the strip's content (language switch, rail rebuild, page change).
- **Active item into view:** after a page or view change, the active item is scrolled into view: `el.scrollIntoView({block:'nearest', inline:'nearest'})`. For users who prefer reduced motion this is instant; otherwise smooth.
- **No arrows or buttons:** the fade plus a cut-off label is the familiar pattern on phones, and arrow buttons would add tab stops (spec T3 keeps the Tab order lean).

CSS sketch:

```
.hscroll[data-overflow="end"]  { mask-image: linear-gradient(to right, #000 calc(100% - 24px), transparent) }
.hscroll[data-overflow="start"]{ mask-image: linear-gradient(to left,  #000 calc(100% - 24px), transparent) }
.hscroll[data-overflow="both"] { mask-image: linear-gradient(to right, transparent, #000 24px, #000 calc(100% - 24px), transparent) }
```

Add the `-webkit-mask-image` twin for older Safari. `.hscroll` is a new class, so it needs this selector in `v2.css` (ADR-091 class drift gate).

## 2. Settings rail at 1400 px (F9)

**Today.** The rail column is 215 px (`.set-layout`, `v2.css:426`). Each item is built in `v2.js:4697-4703` as one text node "icon + title" plus a REBOOT badge (Network only) and a count. Because the icon and the title are one text node in a flex row, the line breaks inside it. "📶 Network" puts the icon on its own line when the badge takes the space, and "OpenTherm Gateway" and "SAT — Smart Thermostat" wrap to two lines.

**New item structure:**

```
<button type="button" class="rail-item" data-cat="network" aria-current="true|absent">
  <span class="ri-ic" aria-hidden="true">📶</span>
  <span class="ri-lbl">Network</span>
  <span class="ri-reboot" title="Changes apply after a reboot" aria-label="needs a reboot"></span>   <- Network only
  <span class="cnt">12</span>
</button>
```

- **`.ri-lbl`:** `min-width:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap`, with the full title in its `title` attribute. Icon and label never break apart.
- **Reboot marker:** a small dot (8 px, warn colour `--status-warn-text` from spec T1) replaces the text badge in the rail. The full REBOOT badge stays in the category header and on the setting rows, where there is room. That keeps the information and frees about 60 px.
- **Shorter rail titles** (the category header keeps the long title):

  | id | Rail title | Category header (unchanged) |
  |---|---|---|
  | otgw | OpenTherm | OpenTherm Gateway |
  | sat | SAT | SAT — Smart Thermostat |
  | s0 | S0 counter | S0 Pulse Counter |

  New keys for the three short titles, next to the existing `settings.cat.<id>.title` (`catTitle()`, `v2.js`): `settings.cat.otgw.short`, `settings.cat.sat.short`, `settings.cat.s0.short`. A small `catShort(c)` falls back to `catTitle(c)` for the other categories.

### 2.1 A rail item is not a button (new finding, WCAG 2.1.1 level A)

`.rail-item` is a `<div>` with a click handler (`v2.js:4697-4701`). It has no tabindex and no key handler, so a keyboard user cannot reach or open a Settings category at all. This is more serious than the layout (level A, it blocks the task), and it is found here because the rail is rebuilt anyway.

- Fix: `<button type="button">` as above, with the existing click handler. A button gets focus, Enter and Space for free.
- The active item gets `aria-current="true"`, the same pattern as the other view switches in spec T3.
- Reset the button look in CSS (`border:0; background:none; font:inherit; text-align:left; width:100%`), so nothing changes visually.

**Moved to TASK-2958** (coordinator decision 2026-11-20; TASK-2958 raised to high): this section is built and accepted there, not in TASK-2960.

### 2.2 The panel next to the rail

With one category shown, `.set-cards{columns:300px}` (`v2.css:457`) puts a category with one or two cards into the first 300 px column of a 1000 px area (System measured about 370 px). New: when the shown category has at most 2 cards and no search is active, the cards render in one column of up to 760 px (`columns:auto; max-width:760px`), set by a class on `.set-cards` (`set-cards--few`, needs a selector for ADR-091). With search active, or with 3 or more cards, the columns stay as they are.

## 3. Answer to the review's open question (F9)

*Is the Settings rail SAT group the same set as "SAT settings" at the bottom of SAT > Control?* **One source, two renderings.** Both read `setData` (one `/api/v2/settings` answer) and select the keys with `catFor(k) === 'sat'`: the Settings page through `keysForCat()` (`v2.js:4637-4643`), the SAT page through `renderSatSettings()`, which groups them into `SAT_L2`/`SAT_L3` and leaves out `SAT_NOT_IN_FORMS`. So a value changed on one page is the same value on the other.

The consistency risk that remains is the grouping, not the data: the same setting sits in a different group on each page, and the Settings count (90) includes keys the SAT page does not show. That is a design question, not a defect, and it is out of scope here. A possible later step: the Settings SAT category gets one line at the top, "These settings are also on the SAT page, grouped by task: [Open SAT › Control]".

## 4. Acceptance (replaces the ACs of TASK-2960 where they differ)

1. At 390 px the five strips of section 1 show the edge fade on the side with hidden content (both themes), the fade follows scrolling, and after a page or view change the active item is in view; screenshots of the main nav and the Settings rail at 390 px before and after.
2. At 1400 px every rail item keeps icon and label on one line; long titles end in an ellipsis with the full title as `title`; Network shows the reboot dot instead of the text badge; the three short titles are used.
3. (Moved to TASK-2958: rail items as `<button>`, section 2.1.)
4. A category with at most 2 cards renders in one column of up to 760 px when no search is active.
5. Section 3 answers the open question; no change to how the two pages share `setData`.
6. New classes (`.hscroll`, `.ri-ic`, `.ri-lbl`, `.ri-reboot`, `.set-cards--few`) have selectors; `python evaluate.py --quick` shows no new FAIL; new EN keys go through the i18n sync and `tests/webui/i18n-*.test.mjs` pass.
