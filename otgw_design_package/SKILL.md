---
name: Firmware design-system handoff
description: |
  Generate a complete, self-contained design-system package for an
  embedded device's on-device web UI — one tokens file, one component
  library, a /design.html reference page, page-templates with
  sentinel-marked patch blocks, a Windows installer script (PS1 + .bat
  wrapper), and a handoff.md aimed at Claude Code for the markup edits
  and per-patch commits.
---

# Firmware design-system handoff (skill)

Use this skill when a user has an existing firmware repo with a
hand-rolled web UI (LittleFS / SPIFFS / similar partition, no build
step on the device) and wants the design layer rebuilt as a real,
token-driven design system that:

- ships as a drop-in package, not a refactor PR,
- backs up the existing `data/` before touching anything,
- leaves behavior untouched (no JS rewrites beyond minimum hooks),
- comes with a visual regression page hosted on the device itself,
- is applyable on Windows with a double-click.

## Inputs to gather (questions_v2 first)

Always ask these before writing anything:

1. **Repo + screen list.** Which firmware (path / repo URL), and which
   pages/screens does the UI have? Need: home, settings, log/monitor,
   one or more device-specific control panels, file explorer, etc.
2. **Existing CSS state.** Are there hand-rolled stylesheets per page,
   or one global CSS, and is there a dark-mode variant? Get the file
   names — they show up in the apply script's plan.
3. **Theme toggle mechanism.** `body.dark` class? `[data-theme]` attr?
   localStorage key? Match the existing convention so the toggle keeps
   working.
4. **Chart library.** ECharts? Chart.js? None? CDN or self-host? If
   CDN: which version is pinned. The skill emits a chart-theme
   helper that reads CSS vars regardless of library.
5. **Fonts.** Self-hosted WOFF2 (preferred for offline devices) or
   system stack? File-size budget if relevant — embedded partitions
   are usually 1-2 MB.
6. **Variations to expose as Tweaks.** Optional — usually no, this is
   production output, not a design exploration.
7. **Scope.** Just the design layer, or also a list of bugs/issues
   that need to be fixed alongside (e.g. "the X panel is broken").
8. **Branding constraints.** Which colors are "the brand"? What can
   be changed and what is sacred?
9. **Iconography.** Self-host PNG/SVG, icon font, or no icons?
10. **Apply target.** Windows / macOS / Linux for the script. Default
    to Windows since most firmware-flashing flows are Windows.

## Package structure to emit

```
<project_name>_design_package/
├── README.md
├── handoff.md
├── SKILL.md            <-- this file, for next time
├── apply.ps1
├── apply.bat
├── data/               <-- mirrors firmware data/ structure
│   ├── ds-tokens.css
│   ├── components.css
│   ├── theme-toggle.js
│   ├── <chart>-theme.js
│   ├── design.html
│   └── fonts/
└── templates/
    ├── index.html.template
    └── <each-page>.html.template
```

## Hard rules

- **One tokens file.** All color, type, space, radius, shadow, motion.
  Light + dark via the existing convention (`body.dark` AND
  `[data-theme="dark"]` for safety). No second source of truth.
- **components.css uses var(--*) only.** Zero raw hex outside
  ds-tokens.css. Search for `#[0-9a-f]` after writing — should match
  zero lines.
- **Match existing class names.** components.css must shadow the
  legacy CSS without markup changes for as many components as
  possible. The user removes the legacy CSS in the final patch, not
  the first.
- **Sentinels in templates.** Every patch block in `templates/*.template`
  is wrapped in `<!-- DS:NAME:BEGIN -->` … `<!-- DS:NAME:END -->`
  comments so the receiver knows exactly what to paste.
- **Idempotent JS hooks.** Use a `data-bound="1"` attribute on each
  bound element so re-running on a re-rendered DOM doesn't double-bind.
  Listen for a project-specific `<page>:rendered` event for re-binding.
- **Chart theme reads tokens.** Helper function calls
  `getComputedStyle(document.body).getPropertyValue('--…')` so theme
  swap is one `chart.setOption(theme(), true)` call.
- **Backup before copy.** apply.ps1 always writes a timestamped zip
  unless `-NoBackup`. Default mode is interactive (y/n/q per file);
  `-Force` skips prompts; `-WhatIf` is a dry run.
- **No git, no build, no flash.** Script copies files. handoff.md
  documents what's left for the receiver (Claude Code).
- **/design.html lives on the device.** Renders every component in
  light and dark side-by-side. Hosting it from the firmware means
  visual regression after each flash is one URL away.

## Output discipline

- ds-tokens.css opens with a comment block explaining the dual-selector
  convention and the apply order.
- components.css has a numbered table-of-contents at the top.
- Each template's leading comment names the file it patches and the
  scope (full replace vs. block replace vs. append).
- handoff.md is structured as Patch A/B/C/D/E with explicit commit
  messages. The receiver should be able to land each as a separate
  PR if they want.
- README.md has Quick Start, Apply Order, Smoke Test, Rollback,
  "What the script does/doesn't do" — in that order.
- apply.ps1 uses `[CmdletBinding(SupportsShouldProcess=$true)]` so
  `-WhatIf` works without extra code.

## Anti-patterns to avoid

- Self-hosting a chart library "for offline use" without checking
  with the user — it bloats the partition and is rarely worth it.
  Default to CDN, document the fallback.
- Hand-rolled icons. Use what's already in the firmware's `assets/`
  or text labels. Don't invent SVG icons.
- Adding routes/handlers to the firmware's HTTP server. The design
  layer is files-only; routes are out of scope.
- Aggressive markup refactors. If a class name already exists, match
  it. The receiver should be able to land the package without
  rewriting partials.

## When NOT to use this skill

- One-off styling tweaks (use a single PATCH.md instead).
- The user wants to explore design directions (use design_canvas).
- A from-scratch new firmware UI (build with the standard frontend
  design skill; this skill is for retrofitting an existing repo).
