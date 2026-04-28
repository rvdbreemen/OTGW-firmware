---
id: TASK-472
title: 'fix(webui): restore brand cyan in design-system tokens'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-28 17:25'
updated_date: '2026-04-28 17:29'
labels:
  - webui
  - design-system
  - tokens
dependencies:
  - TASK-467
references:
  - src/OTGW-firmware/data/ds-tokens.css
  - otgw_design_package/AUDIT-2026-04-28.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Restore the vibrant cyan brand identity in `ds-tokens.css` light-mode `:root { ... }` block. The design-package author intentionally desaturated brand colours; this task reverts that decision per project visual identity (matches `dev` branch baseline: `#00bffe` nav strip, `#e6ffff` page background, `lightblue` buttons).

## Context

`ds-tokens.css:51-58` currently has:
```
--brand-cyan:        #cfe9ee;   /* "almost gray" */
--brand-cyan-soft:   #f1f7f8;
--brand-lightblue:   #d6d8da;
```

Per audit `otgw_design_package/AUDIT-2026-04-28.md` and verification against `git show dev:src/OTGW-firmware/data/index.css` (line 20: `html { background: #e6ffff }`, line 84: `.nav-container { background: #00bffe }`), these are wrong values for the OTGW brand. Dark-mode in the same file already has `#00bffe` correctly — the desaturation only affected light-mode.

## Scope

Edit `ds-tokens.css` light-mode `:root { ... }` block ONLY (approximately lines 51-110). Do NOT touch dark-mode block (lines ~140+) or any other file.

Replace:
- `--brand-cyan: #00bffe`
- `--brand-cyan-soft: #e6ffff`
- `--brand-lightblue: #add8e6` (CSS named `lightblue`)
- `--brand-sky: #87ceeb` (CSS named `skyblue`)
- `--brand-faint: #f0fbff`
- `--accent-primary: #0288a0` (slate-cyan, harmonises with vivid brand)
- `--accent-hover: #015a70`

Update the comment at line 51-53 from "desaturated" to reflect vivid-brand intent.

## Acceptance Criteria
<!-- AC:BEGIN -->
- AC1: `--brand-cyan` is `#00bffe` (matches `git show dev:src/OTGW-firmware/data/index.css` line 84)
- AC2: `--brand-cyan-soft` is `#e6ffff` (matches dev line 20 page-bg)
- AC3: `--brand-lightblue` is the CSS named colour `lightblue` (#add8e6)
- AC4: Dark-mode block is **unchanged** (already had correct values)
- AC5: After flash + browser load: header strip is vibrant cyan, page background is light mintcyaan, buttons are lightblue
- AC6: `/design.html` reference page validates new colours in all components without console errors

## Out of scope

- Class additions or layout changes (those are TASK-472)
- Density tweaks (those are TASK-473)
- Touching components.css at all (those are TASK-472/473/474)
<!-- SECTION:DESCRIPTION:END -->

- [x] #1 --brand-cyan is #00bffe (matches dev branch index.css:84)
- [x] #2 --brand-cyan-soft is #e6ffff (matches dev branch index.css:20 page-bg)
- [x] #3 --brand-lightblue is the CSS named colour lightblue (#add8e6)
- [x] #4 Dark-mode :root block is unchanged
- [ ] #5 After flash: header strip vibrant cyan, page-bg mintcyaan, buttons lightblue
- [ ] #6 /design.html reference page renders new colours in all components without console errors
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementer-agent klaar (28s). Edits: ds-tokens.css regels 51-61. Light-mode brand vars hersteld naar canonical waarden (--brand-cyan #00bffe, --brand-cyan-soft #e6ffff, --brand-lightblue #add8e6 = lightblue, --brand-sky #87ceeb, --brand-faint #f0fbff, --accent-primary #0288a0, --accent-hover #015a70). Commentaarblok geherschrev naar 'vivid OTGW cyan'. Dark-mode block onaangetast. AC1-4 verifieerbaar in source; AC5-6 (browser smoke) wachten op flash.
<!-- SECTION:NOTES:END -->
