---
# METADATA
Document Title: Fixes Applied — Review 2026-03-20
Review Date: 2026-03-20 05:08:11 UTC
Branch: copilot/review-codewijzigingen-sinds-laatste-release
Status: COMPLETE
---

# Fixes Applied from Review

This document lists which review findings have been addressed in this PR branch.

## Fixed in this PR

### K1 — `state` local shadowing (`OTGW-firmware.ino`)
**Status: FIXED**

Renamed local `WifiPortalResetState state` → `WifiPortalResetState portalState` in all
three affected functions:
- `readWifiPortalResetState(WifiPortalResetState &portalState)`
- `writeWifiPortalResetState(const WifiPortalResetState &portalState)`
- `clearWifiPortalResetState()` — local var renamed
- `shouldForceWifiConfigPortal()` — local var renamed throughout

### K2 — `TRACKED_TIME_UNSEEN` sentinel clarification (`OTGW-Core.ino`)
**Status: CLARIFIED (was not a real bug)**

After deeper analysis: `currentTrackedSeconds()` returns `(millis()/1000) % 65535`,
which produces values in `[0, 65534]`. The sentinel `0xFFFF = 65535` is thus **never**
produced, confirming the original code is correct. A detailed comment was added
explaining the invariant to prevent future confusion.

### K3 — `readLatestCrashLog()` calls `LittleFS.begin()` (`helperStuff.ino`)
**Status: FIXED**

Replaced unconditional `LittleFS.begin()` with a guard against the existing
`LittleFSmounted` flag. This prevents re-mounting an already-mounted filesystem
and guards against use during OTA operations.

### K4 — `writeJsonStringKV()` uses global `cMsg` (`settingStuff.ino`)
**Status: FIXED**

`writeJsonStringKV()` now explicitly uses `escapeJsonStringTo(value, cMsg, ...)`,
with `cMsg` serving as the dedicated JSON-escape scratch buffer for this helper.
The review notes and comments were updated so the documented behavior matches the
actual implementation.

### K5 — `expandedPayload[384]` on stack in `sendWebhookPost()` (`webhook.ino`)
**Status: FIXED**

`sendWebhookPost()` no longer allocates a local `expandedPayload[384]` buffer on
the stack. It now builds the expanded webhook payload using the shared global
`cMsg` scratch buffer instead, aligning webhook formatting with the project-wide
static-buffer pattern. Any further webhook buffering or reentrancy refinements
are tracked separately from this K5 issue.

### I2 — `handleCommandSubmit()` missing alphabetic prefix check (`restAPI.ino`)
**Status: FIXED**

Added `isalpha()` checks on `cmdStr[0]` and `cmdStr[1]` before queuing. Rejects
commands with non-letter prefixes (e.g., null bytes, control characters) with
HTTP 400.

## Documented Only (Not Fixed in This PR)

### I1 — Lazy `new` for MQTT autoconfig buffers
Documented in REVIEW.md. Requires ADR update. Architectural change out of scope.

### I3 — `OTGWState state` / `OTGWSettings settings` defined in header
Documented in REVIEW.md. All other globals are also defined in the header
(established codebase pattern). Fixing I3 alone would be inconsistent; a complete
globals-to-.ino migration requires a dedicated ADR and PR.

### I4 — Timers initialized with pre-`readSettings()` defaults
Documented in REVIEW.md. Existing behavior since before v1.2.0; not a regression.

### I5 — Webhook 1-second timeout
Documented in REVIEW.md. Acceptable for local LAN; configurable timeout is a
follow-up feature request.

### I6 — TrackingStateInitializer static constructor
Documented in REVIEW.md. Low risk; no debug calls in constructor path.

### R5 — `parseJsonKVLine()` missing `\uXXXX` support
Documented in REVIEW.md. Low risk for embedded settings use case.

### R6 — Section map with hardcoded line numbers
Documented in REVIEW.md. Maintenance issue; no functional impact.
