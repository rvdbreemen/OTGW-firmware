---
id: "ADR-258"
title: "A WiFi probe that cannot run for lack of memory counts as dead when the largest free block is under 4 KB"
status: "Accepted"
date: "2026-11-18"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-261"
topics:
  - "wifi"
  - "dead link"
  - "heap"
  - "recovery"
  - "restart"
aliases:
  - "starved heap probe"
  - "no echo buffer"
  - "heap not healthy skip"
  - "loop alive network dead"
components:
  - "wifiProbeStarvedDead"
symbols:
  - "wifiProbeStarvedDead"
  - "WIFI_STARVED_MAXBLOCK"
  - "wifiStarvedLogged"
  - "handleWifiProbeVerdict"
  - "wifiLinkProbeTick"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-258 A WiFi probe that cannot run for lack of memory counts as dead when the largest free block is under 4 KB

## Status

Accepted, 2026-11-18. Guideline-level (ADR-080): verified by the host test named below, no evaluate.py gate.

## Status History

```yaml
status_history:
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-18
    status: Accepted
    changed_by: "Agent: Claude"
    reason: "Robert van den Breemen accepted: DM 'Ja' (1572613449282756649, 14:04:24, reply to the accept question 1572613106318708824 naming ADR-258 and TASK-2026); option A chosen in DM 1572611211378565161"
    changed_via: adr-kit lifecycle
```

## Context

The WiFi dead-link supervisor (`networkStuff.ino`, TASK-1450, no ADR of its own) pings the gateway once a minute. After 8
dead probes in a row it forces a reconnect, after 8 more it restarts the unit, at most 3 times until the link has been
alive for an hour; ADR-211 makes that restart skip the config portal. A probe that cannot run for lack of memory is
skipped and not counted, so a short memory shortage cannot make the unit restart: no probe at all while the heap is not
healthy (`getHeapHealth() != HEAP_HEALTHY`, below 8192 B free), no probe without a pcb, and a probe whose echo buffers
could not be allocated.

On .61 (OTGW Classic with an S3 module without PSRAM (pseudo-static RAM, external memory), BLE (Bluetooth Low Energy) switched on, beta.370, 2026-11-18) the heap was starved from
13:41 (free 7-13 KB, largest block 2-5 KB), the unit had short reachable windows until about 13:50 and was then
unreachable (no ping, no web page, no telnet) until a USB (Universal Serial Bus) reset at 14:03:10. The reboot log has no entry between the OTA (over-the-air)
at 13:38:51 and that USB reset: nothing restarted the unit (TASK-2025, TASK-2026; 09's bench report).

Nothing could, from the code:

- the heap emergency (ADR-107, `emergencyHeapRecovery()`, helperStuff.ino) drops web sockets, port 25238 and the
  discovery queue, and never restarts;
- the supervisor skipped every probe (heap not healthy, or no buffer), and before that the short reachable windows reset
  its count of dead probes in a row;
- the task watchdog and the 0x26 hardware watchdog only watch the loop task (`esp_task_wdt_add(NULL)`, `feedWatchDog()`
  in OTGW-Core.ino), and the loop most likely kept running (a blocked loop task would have tripped the task watchdog
  within 13 min; inferred, not measured).

TASK-2025 removes the known trigger (BLE on a board without PSRAM). The gap stays for any other heap exhaustion.

## Decision

A probe that cannot run for lack of memory counts as a dead probe, instead of a skip, when the dead-link check is armed
and the largest free block (`platformMaxFreeBlock()`) is below `WIFI_STARVED_MAXBLOCK` (4096 B); the existing supervisor
path then handles it unchanged.

User Decision: Robert van den Breemen chose this option (A) on 2026-11-18 13:55:30, in DM 1572611211378565161, a reply to
the coordinator's question 1572602711843803158 that set out options A, B and C. His answer, verbatim: "A".

- The three memory skips are covered: "heap not healthy", "no pcb", "no echo buffer". `wifiProbeStarvedDead(why)` counts
  the probe through `handleWifiProbeVerdict(false)` and logs once, until a probe is answered again:
  `WiFi: gateway probe not possible (<why>), heap starved (largest block N B): counted as dead`.
- Not armed (no gateway answered since boot) or a largest block of 4 KB or more: the probe is skipped and not counted,
  as before.
- The rest is the existing path: 8 dead in a row, a forced reconnect, 8 more, a restart with the reason
  "WiFi: connected but no traffic", capped at 3 restarts, then reconnects only. From the moment the unit is fully dead
  the restart comes after about 16-17 minutes (8 + 8 probes at one per minute).

## Decision Contract

### Must

- Count a memory skip as a dead probe only when the check is armed (`wifiArmedGateway != 0`) and
  `platformMaxFreeBlock() < WIFI_STARVED_MAXBLOCK`.
- Reach the restart only through the existing supervisor path (`handleWifiProbeVerdict()`): its 8-probe threshold, the
  forced reconnect first, the witness rule (MQTT, REST or port 25238 traffic means the LAN (local area network) works) and the cap of 3.

### Must Not

- Restart directly on a heap number alone; ADR-107's recovery actions stay as they are (no restart there).
- Count a memory skip as dead while the largest free block is 4 KB or more (a busy but healthy heap).

### Exceptions

- None.

### Verification

- `tests/test_wifi_deadlink.py`: the real state machine on the host; scenarios `starved` (heap `LOW`, largest block 2 KB,
  link dead) and `starved_nopbuf` (no echo buffer, largest block 2 KB) end in a forced reconnect and a restart (red on
  the code before this ADR); the existing `heap` scenario (heap `LOW`, largest block large) stays silent.
- Bench: the prerelease-only telnet key `H` (`handleDebug.ino`, `heapHoldTick()`) starves the heap on purpose; expected
  telnet lines: 8 x 'counted as dead', 'forcing a reconnect', 8 more, the restart, and its reason in the reboot log.

## Alternatives Considered

- **B: a heap-floor restart (largest block or free heap below a floor for some minutes restarts the unit).** It also covers
  the phase with short reachable windows, but it restarts on a heap number alone, also while the network works, needs its
  own cap against a restart loop on a unit that starves again right after boot, and its thresholds need field data. It
  would amend ADR-107 with a restart action. Rejected by Robert's choice; kept as a follow-up if a field unit shows a long
  flapping phase without recovery.
- **C: nothing beyond TASK-2025.** The known trigger is gone, but the next heap exhaustion again needs a hard reset by the
  user. Rejected.
- **Count "8 of the last 10" instead of "8 in a row" (an option to A).** It would also catch the phase with short reachable
  windows (on .61, 13:41-13:50), but it changes the supervisor for flapping routers too. Deferred: not part of this
  decision; it needs its own field evidence.

## Consequences

**Positive:**

- A unit whose loop runs but whose network is starved by heap exhaustion recovers by itself, about 16-17 minutes after it
  became fully unreachable, through a path that is already capped and already skips the portal (ADR-211).
- Two independent signals have to agree (no probe possible and a starved heap), so a healthy unit is not restarted: the
  bench values were a largest block of 2-5 KB while starved and 28-31 KB when healthy.

**Negative:**

- The 4 KB floor is a judgement, not a measured boundary; mitigation: the wide margin above and the cap of 3 restarts.
- The phase with short reachable windows is not covered (the count of dead probes in a row resets); see the deferred
  alternative.
- A heap that is already starved before the first answered ping after boot is not covered (the check is not armed).
- The forced reconnect before the restart does not help against a starved heap; it costs 8 minutes but keeps one path for
  all dead-link causes.

## Open Questions

- [x] Which option closes the gap: A, B or C? — **Answered 2026-11-18 by Robert van den Breemen:** A (DM 1572611211378565161).

## Related Decisions

- **ADR-211 (Skip the WiFi config portal on the boot after a WiFi supervisor restart)**: applies unchanged to the restart
  this ADR can lead to.
- **ADR-107 (Emergency heap recovery actions)**: untouched; the heap emergency still does not restart.
- **ADR-167 (Heap tier gating removal)**: untouched; no new heap-tier gating.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level; its check is the host test above.

## References

- TASK-2026 (this decision), TASK-2025 (the BLE trigger on boards without PSRAM), TASK-1450 (the dead-link supervisor).
- `src/OTGW-firmware/networkStuff.ino`: `wifiProbeStarvedDead()`, `WIFI_STARVED_MAXBLOCK`, `handleWifiProbeVerdict()`,
  `wifiLinkProbeTick()`.
- `src/OTGW-firmware/helperStuff.ino`: `emergencyHeapRecovery()` (ADR-107, no restart).
- `src/OTGW-firmware/OTGW-Core.ino`: the task watchdog subscription and `feedWatchDog()`.
- `tests/test_wifi_deadlink.py`: scenarios `starved`, `starved_nopbuf`, `heap`.
- Bench report (09, .61, 2026-11-18): reboot log entries 13:38:51 (OTA) and 14:03:10 (USB reset), nothing in between.
