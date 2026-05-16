# Daily Issue Report — 2026-05-16

**Generated**: 2026-05-16  
**Period checked**: Last 24 hours (since 2026-05-15T00:00:00Z)

## Sources Checked

| Source | Status | Result |
|--------|--------|--------|
| GitHub Issues | ✅ Scanned | 1 new issue updated in last 24h |
| Tweakers forum | ❌ Unreachable | Host blocked by network policy |
| Discord | ❌ Unavailable | Discord MCP not configured in this session |

## Findings

### Bug Reports

---

#### [#575] "Update Firmware" button not visible after updating to v1.5.0

- **Source**: GitHub Issues
- **Reporter**: rkuijer
- **URL**: https://github.com/rvdbreemen/OTGW-firmware/issues/575
- **Created**: 2026-05-15T12:33:16Z
- **Updated**: 2026-05-15T16:14:19Z
- **Labels**: bug, good first issue, question
- **Classification**: Bug report
- **Comments**: 3

**Summary**: After upgrading to v1.5.0, the "Update Firmware" button on the FSExplorer page is no longer visible on a Microsoft Surface Pro 9 (touch device) in both Chrome and Firefox.

**Root cause (identified by reporter)**: The button's container `<div>` has `class="desktop-only"`. The FSExplorer CSS contains a media query `@media (pointer: coarse) and (hover: none) { .desktop-only { display: none; } }` which hides all `.desktop-only` elements on touch devices. The Surface Pro 9 triggers this rule, causing the Update Firmware button to vanish.

**Suggested fixes**:
1. Remove `desktop-only` class from the Update Firmware form wrapper (so it shows on all devices).
2. Change the media query to hide based on viewport width instead of pointer/hover type.
3. Introduce a more specific CSS class for elements that are truly touch-incompatible.

**HTML/CSS evidence**: Removing the `class="desktop-only"` attribute via DevTools immediately restores the button visibility, confirming the diagnosis.

---

## Notes

- **Tweakers forum**: The RSS endpoint at `gathering.tweakers.net` is not reachable from this execution environment due to network allowlist policy. Tweakers posts could not be scanned.
- **Discord**: No Discord MCP server is configured for this session; Discord `#beta-testing` channel was not scanned.
