# Agentic setup plates

Generators for the two reference wireframes and the overview poster used by `docs/process/agentic-setup.md`, its visual version
`agentic-setup.html` and the image brief `docs/process/agentic-setup-image-brief.md` (TASK-1984).

| Plate | Script | Output |
|---|---|---|
| A, the whole agentic workshop | `plate_a.py` | `docs/process/images/agentic-setup-plate-a.svg` / `.png` |
| B, BlueFlame Bot and its channels | `plate_b.py` | `docs/process/images/agentic-setup-plate-b.svg` / `.png` |
| Overview, architecture at a glance (2560 x 1440, English only) | `plate_overview.py` | `docs/process/images/agentic-setup-overview.svg` / `.png`, plus downloads `-2x.png` (5120 x 2880) and `.pdf` (vector, fonts embedded) |

Light theme, English labels (the scripts also hold a Dutch label set: pass `nl`). Palette and symbols live in
`svgkit.py`; the labels and coordinates live in the plate scripts.

```bash
python3 tools/agentic-plates/plate_a.py docs/process/images/agentic-setup-plate-a.svg en
python3 tools/agentic-plates/plate_b.py docs/process/images/agentic-setup-plate-b.svg en
python3 tools/agentic-plates/plate_overview.py docs/process/images/agentic-setup-overview.svg
# PNG renders (Node with Playwright and Chromium; fonts come from Google Fonts)
NODE_PATH=$(npm root -g) node tools/agentic-plates/render.js docs/process/images/agentic-setup-plate-a.svg docs/process/images/agentic-setup-plate-a.png 1
NODE_PATH=$(npm root -g) node tools/agentic-plates/render.js docs/process/images/agentic-setup-plate-b.svg docs/process/images/agentic-setup-plate-b.png 1
NODE_PATH=$(npm root -g) node tools/agentic-plates/render.js docs/process/images/agentic-setup-overview.svg docs/process/images/agentic-setup-overview.png 1
# Downloads of the overview plate: high-resolution PNG and a one-page vector PDF
NODE_PATH=$(npm root -g) node tools/agentic-plates/render.js docs/process/images/agentic-setup-overview.svg docs/process/images/agentic-setup-overview-2x.png 2
NODE_PATH=$(npm root -g) node tools/agentic-plates/render_pdf.js docs/process/images/agentic-setup-overview.svg docs/process/images/agentic-setup-overview.pdf
```

Run the scripts from the repository root (they import `svgkit` from their own folder). When the architecture
changes, update the labels here first, then the image brief, then regenerate the plates (and the overview downloads) and look at the PNGs.
