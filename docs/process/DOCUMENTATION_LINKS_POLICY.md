# Documentation links policy

Use these canonical locations when adding or updating documentation links:

- **Guides**: `docs/guides/`
- **API docs**: `docs/api/`
- **Feature docs**: `docs/features/`
- **Process docs**: `docs/process/`
- **Current release notes**: `docs/releases/`
- **Archived release notes**: `docs/releases/archive/`
- **Other archived docs**: `docs/archive/`

## Linking rules

1. Prefer links to canonical files, not legacy paths kept for historical context.
2. In repository-root files (for example `README.md`), use root-relative paths like `docs/guides/FLASH_GUIDE.md`.
3. In nested docs, use correct relative paths from the current file location (`../` or `../../` as needed).
4. Link to `docs/releases/archive/` intentionally for historical versions (`v1.3.x` and older).
5. Keep release navigation up to date in `docs/releases/README.md` when adding or moving release notes.
