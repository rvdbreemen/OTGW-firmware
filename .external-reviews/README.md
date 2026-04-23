# External Reviews

This directory holds review outputs produced **outside** the current session's own review pipeline (`.full-review/`).

External reviews are kept strictly separate — they are **not** merged into the in-flight consolidated report. They are processed as independent input and can be cross-referenced from a dedicated analysis document, but the Phase 1-5 pipeline in `.full-review/` reflects only the current session's own findings.

## Current contents

- `HANDOFF-claude-review-c-codebase-303Qj.md` — full-codebase review (~24k LoC) from branch `claude/review-c-codebase-303Qj`, dated 2026-04-21. Scope differs from `.full-review/`: that review is whole-codebase, `.full-review/` is branch 1.4.1 diff vs dev.

## Processing policy

1. Do not fold external findings into `.full-review/0X-*.md` files.
2. If a cross-check / overlap analysis is desired, produce it as a **separate** document (e.g., `cross-check-<name>.md` in this directory).
3. Preserve the external reviewer's wording verbatim — do not paraphrase their findings.
4. When an external claim conflicts with current code, verify against the tree before taking it at face value.
