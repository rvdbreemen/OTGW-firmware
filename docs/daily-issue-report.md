# Daily Issue Report — 2026-05-20

## Summary

- **GitHub**: 1 issue updated in the last 24 hours (since 2026-05-19T00:00:00Z).
- **Tweakers**: Feed unavailable — `gathering.tweakers.net` blocked by network egress policy in this environment.
- **Discord**: MCP server not configured in this execution environment.

---

## GitHub Issues

### Issue #575 — "Update Firmware" button not visible after updating to v1.5.0

- **URL**: https://github.com/rvdbreemen/OTGW-firmware/issues/575
- **Reporter**: rkuijer
- **Source**: GitHub
- **Classification**: Bug report
- **Last updated**: 2026-05-19T18:45:44Z
- **Labels**: bug, good first issue, question

**Summary**: After upgrading to v1.5.0, the "Update Firmware" button on the FSExplorer page is invisible. The reporter (using a Microsoft Surface Pro 9 — a touch-capable laptop) traced the root cause to a CSS media query:

```css
@media (pointer: coarse) and (hover: none) {
  .desktop-only { display: none; }
}
```

The Surface Pro 9 satisfies `pointer: coarse` when used in tablet mode, causing the button's wrapping `<span class="desktop-only">` to be hidden even on a full PC. The fix is to remove `desktop-only` from the Update Firmware form, or use a viewport-width-based rule instead of a pointer/hover heuristic.

**Status**: rvdbreemen confirmed the bug on 2026-05-19 and announced a fix will be released in the next beta.

---

## Tweakers Forum

Unavailable — outbound HTTP to `gathering.tweakers.net` is blocked by network egress policy in this execution environment. No Tweakers posts could be checked.

---

## Discord

MCP server not configured in this execution environment. No Discord posts checked.
