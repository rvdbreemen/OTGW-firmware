---
id: TASK-620
title: >-
  feat-2.0.0: SessionStart hook to pre-provision ESP build toolchain for web
  sessions
status: Done
assignee:
  - '@claude'
created_date: '2026-05-17 21:32'
updated_date: '2026-05-17 21:38'
labels:
  - tooling
  - feat-2.0.0
dependencies: []
ordinal: 37000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Claude Code on the web sessions cannot run python build.py --firmware: the environment egress proxy blocks the arduino-cli / PlatformIO toolchain download (HTTP 403), so build-gated tasks cannot be self-verified. Add an idempotent, web-only SessionStart hook that provisions and caches the build toolchain once (effective after the toolchain hosts are allowlisted in the environment network policy), without breaking session start when the download is blocked.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A web-only, idempotent, non-interactive SessionStart hook script exists under .claude/hooks/ and never fails session start (exit 0) even when the toolchain download is blocked
- [x] #2 The hook is registered as an ADDITIONAL SessionStart entry in .claude/settings.json with the existing Cozempic and .wolf SessionStart hooks preserved intact
- [x] #3 Offline-verifiable behaviour validated in-container: hook runs clean and emits the documented allowlist guidance on network block; evaluate.py --quick still green
- [x] #4 Network-dependent toolchain provisioning documented as requiring the host allowlist (downloads.arduino.cc, github.com, objects.githubusercontent.com, arduino.esp8266.com) — known env limitation, not a hook defect
- [x] #5 Committed and pushed to feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Write .claude/hooks/session-start-build-toolchain.sh: web-only ($CLAUDE_CODE_REMOTE), idempotent (HOME marker), runs build.py --firmware once, traps failure -> logs allowlist guidance, exit 0
2. Merge a 3rd SessionStart entry into existing .claude/settings.json (preserve Cozempic + .wolf)
3. Validate offline: run hook (expect graceful network-block message), run evaluate.py --quick (green)
4. Final summary; close; commit --no-gpg-sign + push to feature-dev-2.0.0
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Validated in-container: Path A (local, CLAUDE_CODE_REMOTE unset) exit 0 no-op; Path B (marker present) prints cached-skip exit 0; Path C (web, no marker) invokes build.py, PlatformIO installs from PyPI then espressif8266@2.6.3 fails with "Host not in allowlist" -> hook prints allowlist guidance and exits 0 (HOOK_EXIT=0). settings.json remains valid JSON with 3 SessionStart entries (Cozempic + .wolf preserved byte-intact). evaluate.py --quick green (59 pass, only pre-existing out-of-scope PROGMEM FAIL).

Backend note: 2.0.0 build.py defaults to PlatformIO (espressif8266@2.6.3) so it additionally needs the PlatformIO registry hosts (dl/api.registry.platformio.org, github.com, objects.githubusercontent.com); dev arduino-cli backend needs downloads.arduino.cc + arduino.esp8266.com.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a web-only SessionStart hook that pre-provisions and caches the ESP firmware build toolchain so Claude Code on the web sessions can run python build.py --firmware and self-verify build-gated tasks.

Changes:
- .claude/hooks/session-start-build-toolchain.sh: runs only when CLAUDE_CODE_REMOTE=true; idempotent via a $HOME/.cache marker (container state is cached after the hook, so only the first web session pays the provisioning cost); invokes build.py --firmware once; on failure logs precise allowlist guidance and ALWAYS exits 0 so it can never block session start.
- .claude/settings.json: appended as a THIRD SessionStart entry (timeout 600s); the existing Cozempic guard and .wolf session-start entries are preserved untouched (targeted edit, not a JSON rewrite).
- backlog TASK-620.

Important limitation (by design, not a defect): a SessionStart hook runs inside the same sandboxed container under the same egress policy, so it does NOT bypass the proxy. It only provisions+caches once the toolchain hosts are allowlisted in the environment network policy. Verified empirically: PlatformIO itself installs (PyPI is permitted) but espressif8266@2.6.3 fails with "Host not in allowlist". Required hosts — PlatformIO backend (2.0.0 default): dl.registry.platformio.org, api.registry.platformio.org, github.com, objects.githubusercontent.com; arduino-cli backend (dev default): downloads.arduino.cc, github.com, objects.githubusercontent.com, arduino.esp8266.com.

Validation: hook Paths A/B/C all exit 0 with correct behaviour; settings.json valid (3 SessionStart entries); evaluate.py --quick green vs baseline. The toolchain-download path cannot be validated in-container until the hosts are allowlisted (expected).

Scope: tooling/config only (.claude + backlog), no firmware touched, no version bump per policy. Landed on the 2.0.0 line only (the branch with signing-bypass authorization in this session); porting the identical hook to dev is a follow-up once dev signing is unblocked.
<!-- SECTION:FINAL_SUMMARY:END -->
