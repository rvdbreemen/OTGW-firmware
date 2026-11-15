---
id: "ADR-246"
title: "When the reboot log does not fit the filesystem-OTA RTC snapshot, keep its newest whole lines (amends ADR-233)"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "filesystem OTA"
  - "RTC snapshot"
  - "reboot log"
  - "crash diagnostics"
aliases:
  - "fsOtaRtcFill"
  - "RTC snapshot budget"
  - "reboot_log.txt"
components:
  - "fsOtaRtcFill"
symbols:
  - "fsOtaRtcFill"
  - "FS_OTA_RTC_DATA_BYTES"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-246 When the reboot log does not fit the filesystem-OTA RTC snapshot, keep its newest whole lines (amends ADR-233)

## Status

Accepted, 2026-11-15. Amends ADR-233 in one rule (how the reboot log is stored when it does not fit); ADR-233 stays
Accepted and unchanged otherwise. Guideline-level per ADR-080: verified by the host test named below, no evaluate.py
gate.

Decision Maker: Robert van den Breemen (maintainer), Discord DM 2026-11-15, relayed by the coordinator: "A and accept
246" (message 1571505912303059000) on the TASK-1931 question 1571505314443034924, which named this ADR and its
acceptance. Recorded by Agent (ba).

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer chose A and accepted ADR-246 for TASK-1931 (Discord DM 1571505912303059000 on question 1571505314443034924); recorded by Agent (ba)
    changed_via: adr-kit lifecycle
```

## Context

ADR-233 keeps `settings.ini`, `reboot_count.txt` and `reboot_log.txt` in an RTC (real-time clock domain) slow-memory
snapshot of 7424 B of data (`FS_OTA_RTC_DATA_BYTES`) during a filesystem OTA (over-the-air update), so a reset in the
middle of the write does not lose them. It stores the files in that order, "each whole or not at all". Next to a
5.7 KB `settings.ini` about 1.7 KB is left, and a full reboot log of 20 lines is 1.9 KB to 4.8 KB
(`REBOOT_LOG_FILE_MAX`, `src/OTGW-firmware/OTGW-firmware.h`). 09 on `.61` (beta.264, 2026-11-15): "[OTA]
/reboot_log.txt (1920 B) does not fit the RTC snapshot (1755 B free): not protected during this filesystem write".
The log is then only protected by the heap keep/restore path, which a crash during the write loses, while after
exactly that crash the log is the diagnostic that is needed. The RTC slow memory has 140 B left (ADR-233 Open
Questions), so the budget cannot grow.

The log is written newest-first: `updateRebootLog()` (`src/OTGW-firmware/helperStuff.ino`) writes the new line, then
copies the older lines below it. Its newest whole lines are therefore a prefix of the file that ends with a CRLF
(carriage return, line feed).

## Decision

When `reboot_log.txt` does not fit the free part of the RTC snapshot, `fsOtaRtcFill()` stores the longest prefix
of it that fits and ends with a CRLF, never a partial line; `settings.ini` and the counter keep their place and their
rule (whole or not at all) from ADR-233.

## Decision Contract

### Must

- Fill the slots in the ADR-233 order: `settings.ini`, then the counter, then the log.
- For the log only: when the file is larger than the free budget, read the free budget, keep the bytes up to and
  including the last CRLF, zero the rest of the read bytes, and log on telnet how much was kept.
- Keep nothing of the log when the free budget holds no complete line.

### Must Not

- Must not store a partial line of the log.
- Must not cut `settings.ini` or the counter: their rule stays whole or not at all.

### Exceptions

- None.

### Verification

- `test/host/test_fs_ota_rtc_snapshot.py` (real `fsOtaRtcSnapshot` block): T11 (5700 B `settings.ini`, a 3200 B log
  of 20 x 160 B lines) restores settings, counter and the log's newest 1600 B of whole lines; T14 (lines of 60 to
  239 B, the budget ends inside a line) restores only the whole lines (1686 B of 2830 B); T12 (the `.63` sizes) still
  restores all three whole. Mutant MB8 (the log skipped whole, the rule before this ADR) fails exactly T11 and T14.
- Bench: a reset during a filesystem OTA on a unit with a near-full budget brings the newest reboot-log lines back
  (TASK-1931 AC3).

## Alternatives Considered

- **Keep the log whole or not at all (ADR-233 as it is).** Rejected by the maintainer: the whole log is lost in a
  crash during the write, which is the case the snapshot exists for.
- **Store the oldest lines that fit.** Rejected: the newest lines say why the unit crashed; the oldest are least
  relevant.
- **Grow the snapshot.** Not possible: 140 B of RTC slow memory are left.
- **Store the log before `settings.ini`.** Rejected: `settings.ini` restores the user's configuration, the priority
  of ADR-233.

## Consequences

**Positive:**

- After a crash during a filesystem write the newest reboot-log lines come back (1600 B of 3200 B in the worst host
  case) instead of none.
- No extra memory: the same buffer, one backward scan for the last CRLF.

**Negative:**

- The older part of the log is lost in that crash case. Mitigation: the heap keep/restore path still carries the
  whole log through a filesystem OTA that completes; only the crash case keeps the cut log.
- A log line that alone is larger than the free budget leaves the log out entirely. Mitigation: the telnet line
  says 0 B kept; with the ADR-233 budget about 1.7 KB stay free and a line is at most 239 B.

## Open Questions

- [x] Maintainer choice for TASK-1931: A (newest whole lines) or B (leave it). Answered A on 2026-11-15 (see Status).

## Related Decisions

- **ADR-233 (Keep settings.ini, the reboot counter and the reboot log in an RTC snapshot during a filesystem OTA)**:
  amended by this ADR for the reboot log only; ADR-233 stays Accepted and the rest of it stands unchanged.

## References

- TASK-1931 (backlog).
- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h`, `fsOtaRtcFill()`.
- `src/OTGW-firmware/helperStuff.ino`, `updateRebootLog()` (newest-first order).
- `test/host/test_fs_ota_rtc_snapshot.py`, `test/host/test_fs_ota_rtc_snapshot.cpp` (T11, T12, T14, MB8).
