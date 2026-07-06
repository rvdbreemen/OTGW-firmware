# ADR-167 Retire the ESP8266-Era Heap Tier Machine and Per-Consumer Gating on the ESP32-S3-Only Dev Branch

## Status

Proposed. Date: 2026-07-06.

This ADR documents a decision to remove code in a separate follow-up
implementation task. It does NOT flip to Accepted here: acceptance
(including whether the evidence below is sufficient, or whether a longer
soak is required first) is the maintainer's own determination, made later.
See "Evidence Limitations" in Context.

## Status History

status_history:
  - date: 2026-07-06
    status: Proposed
    changed_by: Agent (adr-generator, on maintainer instruction)
    reason: Documents the maintainer-authorized decision to retire ADR-089/ADR-121 heap-tier gating on the ESP32-S3-only dev branch, backed by TASK-956 soak evidence. Removal itself is deferred to a Phase-3 follow-up task (TASK-956 AC#4, not yet created). Status intentionally stays Proposed pending the maintainer's own accept-readiness review.
    changed_via: adr-kit

## Context

`dev` is the ESP32-S3-only 2.0.0 async/FreeRTOS line. ESP8266 support was
dropped from this branch on 2026-06-20 and now lives solely on the
`otgw-1.x.x` maintenance branch with its own ADR numbering. Two Accepted
ADRs on `dev` still carry heap-protection machinery whose stated
justification is entirely ESP8266-specific:

- **ADR-089 (Heap Tier Machine Contract,** Accepted 2026-04-26**)** defines
  a 4-state heap health tier (HEALTHY / LOW / WARNING / CRITICAL) over
  `platformFreeHeap()`, with thresholds `HEAP_CRITICAL_THRESHOLD = 1536`,
  `HEAP_WARNING_THRESHOLD = 3072`, `HEAP_LOW_THRESHOLD = 5120` bytes
  (`helperStuff.ino:853-855`), fragmentation-aware promotion via
  `HEAP_FRAG_PROMOTE_MAXBLOCK = 1536` consulting `platformMaxFreeBlock()`
  (`helperStuff.ino:890`), and three tier-entry counters
  (`iEnteredLowCount`, `iEnteredWarningCount`, `iEnteredCriticalCount`,
  `helperStuff.ino:917-919`). ADR-089's own Context section grounds the
  re-tuned thresholds in "the Crashevans field log" (TASK-344) and states
  they are "tied to the ESP8266 baseline," and its own Trade-offs section
  already flagged the ESP32 mismatch verbatim: *"ESP32 builds compile
  against the same constants but operate against different heap
  behaviour... A future amendment can branch on `#ifdef ESP32` once ESP32
  field data justifies a different ladder."* The finding in this ADR is
  stronger than that trade-off anticipated: ESP32-S3 does not need a
  different ladder, it does not need the ladder at all.
- **ADR-121 (Per-Consumer Heap Gating for the WebSocket Live-Log vs MQTT
  Publish,** Accepted 2026-06-02, binding amendment to ADR-089**)** splits
  the WebSocket live-log gate (`canSendWebSocket()`) and the MQTT publish
  gate (`canPublishMQTT()`) into independent threshold ladders, so that
  relaxing the MQTT side (its AC#8) could never re-loosen the WS side. Its
  own Consequences section already predicted the ESP32-S3 asymmetry
  verbatim: *"The ESP32-S3 (OTGW32) path now also runs the gate. On ESP32
  the abundant heap means `getHeapHealth()` rarely leaves `HEAPHEALTHY`, so
  the gate is near-always open and the behavioural change there is
  negligible; this is a noted asymmetry, not a regression."*

Both ADRs are binding per **ADR-080** (binding pattern-level ADRs must ship
with a CI gate in `evaluate.py`): ADR-089 is guarded by
`check_heap_tier_thresholds_ordered`, `check_heap_fragmentation_promotion`,
and `check_heap_tier_entry_counters`; ADR-121 is guarded by
`check_per_consumer_heap_gate`. ADR-089 in turn amends **ADR-030** (Heap
Memory Monitoring and Emergency Recovery, the original 4-state model), and
both depend on **ADR-061**'s platform-abstraction shims
(`platformFreeHeap()`, `platformMaxFreeBlock()`) for their readings.

TASK-956 ran two soak tests to gather the ESP32-S3 field data ADR-089's
Trade-offs section said would be needed to revisit the ladder:

**Data point 1 (2026-06-30, OTGW32 @192.168.88.39, alpha.286).** Cumulative
since-boot counters over a 6.7h uptime spanning two soak windows, including
a 30-minute 8-worker REST flood plus 960-connection WebSocket churn. This
load profile is EXTREME synthetic stress, not representative of field
traffic. Result: `entered_low=0`, `entered_warn=1`, `entered_crit=0`,
`drip_slowmode=0`, `ws_drops=0`, `mqtt_drops=0`. The tier machine engaged
exactly once, at WARN, and never dropped or throttled a consumer even under
this pathological load.

**Data point 2 (2026-07-06, esp32-classic bench @192.168.88.64,
alpha.331+a82bfda, dedicated bench unit).** A full 10-hour soak driven by
`scripts/heap_soak_driver.py`, a REPRESENTATIVE (non-adversarial) load:
dashboard-style API polling every 20s, occasional full asset reload, plus
the firmware's own onboard OTGW serial-simulation replay for continuous OT
traffic. The telnet `z` counter reset was issued from a healthy heap at run
start (satisfying TASK-956 AC#1/#2). Documented deviation: no MQTT
discovery republish traffic and no `sat_boiler_emulator` traffic, because
this bench unit is PIC/Classic hardware, not OTDirect. Wall-clock span was
9.998 hours, including a 127.7-minute gap where the board was physically
unplugged for unrelated hardware testing then reconnected (a reboot, not a
heap fault; the run resumed and completed to the original 10h deadline).
Results across the entire run, both sides of the reboot gap, tracked in the
test's own ndjson log independent of the device's own reboot-reset
counters:

- `hd_min_max_block` (largest contiguous free block watermark) never
  dropped below **10,740 bytes** at any point in 10 hours, comfortably
  above both the 8192-byte emergency threshold and the 1536-byte
  fragmentation-promotion threshold ADR-089 defines.
- `drip_slowmode`, `ws_drops`, `mqtt_drops`, `enter_low`, `enter_warning`,
  `enter_critical`: all zero for the full 10 hours, both before and after
  the reboot gap. The tier machine never intervened once.
- `hd_rest_503`: 29 over the run, expected ADR-165 REST backpressure
  shedding under the driver's own concurrent asset-reload bursts, unrelated
  to heap pressure. `hd_webfile_503`: 0.
- One blemish, disclosed honestly: `hd_max_loop_gap_ms` hit 1027ms at one
  point, but this coincides with unrelated live PIC-firmware-flash testing
  (TASK-972) performed on the SAME physical board during the soak window;
  PIC flash attempts hold the UART and block other loop work for hundreds
  of ms by design. This reading is contaminated by that overlap, not
  load-induced, and is not a heap symptom.

**Evidence limitations, stated openly:**

- 10 hours falls short of TASK-956's own originally-suggested 24-72 hour
  soak window.
- The representative-load leg (2026-07-06 run) did not include MQTT
  discovery republish traffic, a real production pattern that stresses the
  heap differently (larger burst allocations).
- **This ADR does not yet claim to satisfy all four adr-kit verification
  gates (Completeness / Evidence / Clarity / Consistency) required for
  Proposed to Accepted. Whether the evidence above is sufficient to accept
  is the maintainer's determination, not this drafting step's.**

## Decision

On the `dev` branch, retire the ADR-089 heap tier machine and the ADR-121
per-consumer gating in a separate Phase-3 implementation task (TASK-956
AC#4, not yet created or scoped by this ADR), because both are ESP8266-era
protective overhead that the evidence above shows is unnecessary on
ESP32-S3's heap allocator.

This ADR documents the decision and the evidence. It does **not** perform
the removal, and it does not create or scope the Phase-3 task beyond noting
it is TASK-956 AC#4. The Phase-3 task's scope, at minimum, must:

1. Remove (or make ESP32-S3-inert) the tier-machine logic in
   `getHeapHealth()`, the fragmentation-promotion path, and the tier-entry
   counters (ADR-089 sub-rules 1 to 3).
2. Remove (or make inert) the per-consumer WS/MQTT threshold split (ADR-121
   Option B).
3. Retire the four now-meaningless `evaluate.py` gates:
   `check_heap_tier_thresholds_ordered`, `check_heap_fragmentation_promotion`,
   `check_heap_tier_entry_counters`, `check_per_consumer_heap_gate`.
4. Preserve raw heap observability (`platformFreeHeap()`,
   `platformMaxFreeBlock()` per ADR-061) even without the gating behaviour,
   per the mitigation in Consequences below.

## Alternatives Considered

### Alternative A: Keep ADR-089/ADR-121 as-is on dev (do nothing)

Leave the tier machine and per-consumer gating running unchanged.

Rejected. The gating machinery is dead weight on ESP32-S3 hardware per the
evidence above: extra branches, counters, and tier-transition logic execute
on every heap check with no observed benefit across two different load
profiles (a pathological synthetic-extreme run and a 10-hour representative
run). It is also confusing for maintainers reading dev-branch code to see
ESP8266-motivated protective machinery on a branch that dropped ESP8266
entirely on 2026-06-20.

### Alternative B: Amend ADR-089 in place with an `#ifdef ESP32` branch that raises thresholds

Follow through on the trade-off ADR-089 itself floated: keep the tier
machine but give ESP32(-S3) its own, more lenient threshold ladder.

Rejected. `dev` is ESP32-S3-only now; there is no other platform on this
branch to branch against, so a conditional-threshold amendment solves a
multi-platform problem that no longer exists on this line. Removal is
simpler than a threshold rework that still carries machinery nothing on
this branch needs.

### Alternative C: Run a longer soak (24-72h as originally scoped) before drafting any ADR

Defer any ADR until the full soak window TASK-956 originally specified is
complete.

Deferred, not rejected outright: the maintainer explicitly reviewed the two
data points in hand and authorized drafting this superseding ADR now, while
keeping Status: Proposed rather than Accepted specifically so the
acceptance decision, including whether more soak time is needed first,
remains his to make later. Drafting now under Proposed status captures the
decision record without prematurely claiming sufficiency.

## Consequences

**Benefits**

- Simpler code path on `dev` once Phase-3 lands: fewer branches, no unused
  tier machine, no confusing ESP8266-motivated thresholds on an ESP8266-free
  branch.
- Removes four CI gates in `evaluate.py` that were guarding constants no
  longer meaningful on this branch.
- Frees the WS/MQTT publish paths from gate checks that, per the evidence
  above, never tripped on ESP32-S3 hardware across more than 10 combined
  soak-hours under two different load profiles.

**Trade-offs**

- Removes a safety net. If a future ESP32-S3 board variant, or firmware
  growth over time, increases memory pressure in ways not seen in these two
  soak runs, there is no preventive gate to fall back on until one is
  redesigned.
- The evidence base is not exhaustive: 10 hours (short of the 24-72h
  originally scoped) and no MQTT discovery republish traffic in the
  representative-load leg.

**Risks and mitigations**

- *Risk*: an unforeseen heap-pressure scenario after removal causes a crash
  instead of graceful degradation. *Mitigation*: the Phase-3 follow-up task
  must keep (or reinstate) raw heap monitoring/telemetry
  (`platformFreeHeap()` / `platformMaxFreeBlock()` per ADR-061) even if the
  tier-machine/gating behaviour is removed, so a future maintainer retains
  visibility without the automatic throttling.
- *Risk*: this ADR is accepted prematurely without enough soak evidence.
  *Mitigation*: this ADR stays Proposed; the 24-72h soak shortfall and the
  missing MQTT-discovery-republish traffic are disclosed openly above, and
  acceptance is explicitly deferred to the maintainer's own review.

## Related Decisions

- **ADR-089 (Heap Tier Machine Contract)**: superseded by this ADR's
  decision (removal deferred to the Phase-3 follow-up task; ADR-089's text
  is preserved per the immutability rule, not edited).
- **ADR-121 (Per-Consumer Heap Gating for the WebSocket Live-Log vs MQTT
  Publish)**: superseded by this ADR's decision, same terms as ADR-089.
- **ADR-030 (Heap Memory Monitoring and Emergency Recovery)**: the original
  4-state heap model that ADR-089 amended. This ADR does not touch ADR-030
  directly; it acts on ADR-089's amendment layer.
- **ADR-061 (Unified ESP8266/ESP32 Platform Abstraction)**: the Phase-3
  follow-up must preserve the `platformFreeHeap()` / `platformMaxFreeBlock()`
  shims this ADR depends on, per this project's platform-abstraction rules.
- **ADR-080 (Binding ADR Rules Must Have a CI Gate)**: removing the binding
  rules means the Phase-3 follow-up task must also retire the four named
  `evaluate.py` gate functions listed in Decision.

## References

- TASK-956 (the soak-test task; AC#4 is the not-yet-created Phase-3 removal
  follow-up).
- `scripts/heap_soak_driver.py` (representative-load soak driver used for
  the 2026-07-06 run).
- `evaluate.py` gate functions to retire in Phase-3:
  `check_heap_tier_thresholds_ordered`, `check_heap_fragmentation_promotion`,
  `check_heap_tier_entry_counters`, `check_per_consumer_heap_gate`.
- Soak run 1: OTGW32 @192.168.88.39, alpha.286, 2026-06-30, 6.7h uptime,
  8-worker REST flood + 960-connection WS churn (synthetic-extreme).
- Soak run 2: esp32-classic bench @192.168.88.64, alpha.331+a82bfda,
  2026-07-06, 9.998h wall-clock (representative load, one unrelated
  127.7-minute reboot gap mid-run).

Enforcement section omitted: this ADR documents a decision to remove
enforcement machinery in a future task. As Proposed, it carries no code
surface of its own to enforce; the actual removal, and any retirement of
the `evaluate.py` gates, is the Phase-3 follow-up task's job, not this
ADR's.
