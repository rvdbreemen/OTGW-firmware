# Daily Issue Report — 2026-05-17

## Summary

- **GitHub**: 1 new bug report since last check (2026-05-08). No issues updated in the strict last-24-hour window (since 2026-05-16).
- **Tweakers**: Feed unavailable — host blocked by network policy in this execution environment.
- **Discord**: MCP server not configured in this execution environment.

---

## New Issues Found

### 1. GitHub #575 — "Update Firmware" button hidden on touch-capable PCs

- **Source**: GitHub — https://github.com/rvdbreemen/OTGW-firmware/issues/575
- **Reporter**: rkuijer
- **Classification**: Bug report
- **Created**: 2026-05-15 · **Updated**: 2026-05-15
- **Labels**: bug, good first issue, question

**Summary**:
After upgrading to v1.5.0, the "Update Firmware" button on the FSExplorer page disappears on a Microsoft Surface Pro 9 (PC) in both Chrome and Firefox. The reporter traced the root cause to a CSS media query that hides elements with class `desktop-only` on any device that reports `pointer: coarse` and `hover: none` — which the Surface Pro 9 satisfies even when used as a laptop. Removing `desktop-only` from the button container via DevTools instantly reveals the button.

**Maintainer response (rvdbreemen)**:
> "Ahhh my bad. I didn't notice the surface pro remark. That should have worked. Will go and put on the backlog to fix this."

**Backlog**: Created **TASK-615** — Fix: Update Firmware button hidden on touch-capable PCs in FSExplorer (GitHub #575)

---

## Issues Reviewed but Not Actioned

### GitHub #154 — "[Request] Suggestions to improve security"

- **Source**: GitHub — https://github.com/rvdbreemen/OTGW-firmware/issues/154
- **Reporter**: 0crap (original, 2022)
- **Classification**: Feature request — not relevant for daily bug triage
- **Updated**: 2026-05-09 (metadata/label change only; last comment was 2024-07-20)
- **Decision**: No new user content, no backlog action needed.

---

## Sources Not Available

| Source | Status | Reason |
|--------|--------|--------|
| Tweakers RSS | Blocked | `gathering.tweakers.net` not in network allowlist |
| Discord #beta-testing | Not available | Discord MCP not configured in this session |
| Discord #english-support | Not available | Discord MCP not configured in this session |
| Discord #nederlandse-ondersteuning | Not available | Discord MCP not configured in this session |
| Discord #devs-esp-firmware | Not available | Discord MCP not configured in this session |

---

## Timestamps Updated

- Last check: 2026-05-17T00:00:00Z (all sources)
