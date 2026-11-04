---
id: TASK-993
title: >-
  investigate: attached telnet debug client starves HTTP serving under
  concurrent load
status: Done
assignee: []
created_date: '2026-07-03 04:34'
updated_date: '2026-07-09 21:24'
labels: []
dependencies: []
ordinal: 205000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed on bench S3 (TASK-989 load testing, 2026-07-03): with a telnet debug client connected, a concurrent HTTP ramp times out on EVERY request (even conc=1, 8s timeouts, zero 503s reaching the wire); detach telnet and the identical ramp returns fast 200/503/RST mixes. Hypothesis: debug writes to the attached telnet client from async/handler context block or queue-saturate the shared async serving path (AsyncSimpleTelnet shares AsyncTCP). Needs isolation: DebugTf volume vs telnet client TCP window, and whether WS suffers the same.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce with minimal case: telnet attached + conc=4 ramp -> quantify timeout rate vs detached
- [x] #2 Identify the blocking call path (instrument or code-read AsyncSimpleTelnet write path from async context)
- [x] #3 Fix or bound it (non-blocking telnet writes / drop-on-full) with A/B evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 drain verify — NOT REPRODUCIBLE on the current build (alpha.341); the AsyncSimpleTelnet write path is now non-blocking + drop-on-full, which is exactly the AC#3 fix. AC#2 code-read: write() -> _pushEscaped() -> ring push() which 'returns the number actually stored (drops overflow)' (AsyncSimpleTelnet.h:79/473); _flushTx() breaks on c->space()==0 (line 504) and c->add()==0 (line 510) leaving data in the bounded ring — it NEVER blocks the AsyncTCP task, so a slow/full telnet client cannot starve HTTP. AC#1+#3 A/B on the bench Pro (.74): HTTP ramp conc=4, 24 requests, 6s timeout. DETACHED: 21x200 / 3x503 (ADR-165 backpressure) / 0 timeouts. TELNET ATTACHED (client genuinely draining a steady debug stream — captured 53,920 bytes / 95 processOT+REST lines during the window): 22x200 / 2x503 / 0 timeouts. Essentially identical; the 2026-07-03 'every request times out with telnet attached' symptom is gone. Fixed by the AsyncSimpleTelnet submodule evolution (bounded drop-on-full TX ring, pointer bumped alpha.335).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Telnet-attached HTTP starvation (observed 2026-07-03) is resolved: the AsyncSimpleTelnet TX path is now a bounded, drop-on-full ring that never blocks the shared AsyncTCP task. A/B on the bench Pro shows HTTP serves identically with a telnet client attached (0 timeouts either way) vs the original 'every request times out'. Verified, no code change needed — the submodule fix already shipped.
<!-- SECTION:FINAL_SUMMARY:END -->
