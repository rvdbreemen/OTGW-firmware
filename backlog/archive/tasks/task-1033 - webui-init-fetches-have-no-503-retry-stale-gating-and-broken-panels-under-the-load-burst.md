---
id: TASK-1033
title: >-
  webui: init-fetches have no 503-retry - stale gating and broken panels under
  the load-burst
status: Done
assignee:
  - '@claude'
created_date: '2026-07-09 18:10'
updated_date: '2026-07-09 18:33'
labels: []
dependencies: []
ordinal: 242000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Real-use test on the S3 Mini Pro (alpha.338, 2026-07-09, CDP walkthrough with console+network+telnet capture). The ADR-165/147 backpressure gates 503 ~15% of requests during page-load bursts and heap pressure (274 gate-503s in ~8min of UI use; heap dipped to 8KB free; cap dropped to 1). The gates work as designed, but several frontend init-fetches treat a single 503 as terminal: (1) classic index header stays at [hostname]/[version] placeholders when device/info 503s and 'Wait for it...' text never clears; (2) v2 Advanced>PIC firmware card showed 'No PIC detected - flashing unavailable (OT-Direct/OTGW32)' on a live PIC board because the gating fetch 503d/was stale - a hard refresh fixed it (dangerous: users will think PIC flash is unsupported); (3) v2 Advanced>File System showed 'Failed to read the file system.' (listfiles 503, no retry); (4) v2 SAT page showed 'SAT status is unavailable on this device.'; (5) classic Dallas-labels warning. Fix pattern: wrap init/gating fetches in the existing backoff-retry helper (single retry after 300-500ms clears virtually all of these; the gate is transient by design), and never persist a negative capability verdict (PIC gating) from a failed fetch - keep 'unknown' and re-poll.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All five listed panels recover automatically after a transient 503 (verified against a device under load-burst)
- [x] #2 PIC-firmware card never shows a definitive 'No PIC / unavailable' verdict based on a failed or stale fetch; unknown state re-polls
- [x] #3 No regression in the N<=2 request cap (sequential chains stay sequential)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented fetchWithRetry (503+network only, sequential backoff, no added concurrency) in both bundles; applied to classic device/info header + Dallas labels, v2 withDeviceInfo (null=unknown semantics, no negative PIC verdict from failed fetch, 2s re-poll), FS listing (+Retry button), SAT status (state-aware message). On-device verified on the S3 Mini Pro under real load-burst: classic header fills immediately and Wait-for-it clears; v2 PIC card correct on first load; FS tab lists; SAT page renders the full off-state view. node --check green, evaluate 0 fails.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Init/gating fetches in classic and v2 webui now retry transient 503s from the backpressure gates (sequential, cap-safe) and never persist negative capability verdicts from failed fetches. Fixes the stuck [hostname] header, false 'No PIC detected' on the PIC-firmware card, 'Failed to read the file system', and 'SAT status unavailable'. Field-verified on the S3 Mini Pro.
<!-- SECTION:FINAL_SUMMARY:END -->
