# Comprehensive Review Closure Summary

Branch: `feature-dev-2.0.0-otgw32-esp32-sat-support`
Review window: `ace21a48..83e0a72e` (HEAD at time of writing)
Source review: `.full-review/05-final-report.md`
Followup plan: `.full-review/06-followup-plan.md`

## 1. Executive summary

The session work that landed `759e47f8..fa5ef3c5` (NimBLE port, BLE HA
auto-discovery, OTDirect TT/TC PIC-parity state machine, evaluate.py
PROGMEM gate fix) was reviewed across five phases (1A code quality,
1B architecture, 2A security, 2B performance, 3A testing, 3B docs,
3C ADR audit, 4A best practices, 4B CI/CD) by eleven parallel agent
runs. The review surfaced ~95 findings: **0 Critical, 15 High, 31
Medium, 46 Low**. All 15 Highs were closed during the review itself
in five fix-batch commits (TASK-493 through TASK-497). Mediums were
grouped into four follow-up batches (A through D); Batches A, B, and
C have shipped (TASK-498/499/500). Batch D (TASK-501, operational)
and Batch E (TASK-502, Lows polish sweep) are not yet landed. Two
real-issue Mediums (2B-M3 queue depth, 4A-M2 dispatch table) were
explicitly deferred with rationale.

## 2. Findings by severity, with disposition

### Critical: 0

The review explicitly found no Critical findings in any phase. The
project's binding-rules surface (PROGMEM, no-String, no-ArduinoJson,
serial-reserved, no-HTTPS, queue-only PIC commands, ADR-051 / 077 /
088 / 089 / 090) all held under audit.

### High: 15 — all closed during review

| ID | Title | Closing commit |
|---|---|---|
| 1A-H1 | `bDiscoveryPublished` flipped true on transient publish failure | `55fd0cf6` (TASK-493) |
| 1B-H1 | `platformio.ini` reformat erased build rationale + bumped esptoolpy pin | `55fd0cf6` + `ccee66ed` (TASK-493/495) |
| 2A-H1 | `setRemoteOverride()` float-narrowing UB on out-of-range input | `55fd0cf6` (TASK-493); generalised at all 11 sites in `39bbd2fe` (TASK-497) |
| 2B-H1 | `feedWatchDog()` only on success path of BLE HA-discovery streaming primitives | `55fd0cf6` (TASK-493) |
| 1B-M2 / 2A-M1 / 2B-M2 | `_bleSensors[]` cross-task race (one defect surfaced from three angles) | `55fd0cf6` (TASK-493), portMUX pattern formalised in `39bbd2fe` (TASK-497) |
| 3A-H1 | Zero executable tests for the four numerical contract fixes | `23e4b4e2` (TASK-496) |
| 3B-H1 | Per-MAC BLE state and discovery topics undocumented in MQTT contract | `ccee66ed` (TASK-495) |
| 3B-H2 | WebUI `iBleInterval` tooltip mislabelled "scan rate" after TASK-494 | `ccee66ed` (TASK-495) |
| 3B-H3 | CHANGELOG `[Unreleased]` missed all 8 session commits | `ccee66ed` (TASK-495) |
| 3C-1 | ADR-090 sub-rule 4 foresight clause fired (FreeRTOS cross-task) | `ccee66ed` (TASK-495) |
| 3C-2 | ADR-092 prose still claimed "Periodic-scan model retained" | `ccee66ed` (TASK-495) |
| 3C-3 | ADR-068 `OT_CMD_QUEUE_SIZE = 8` drift + uncovered coalesce semantics | `ccee66ed` (TASK-495) |
| 4B-H1 | `evaluate.yml` PR trigger missed `feature-dev-*` | `39bbd2fe` (TASK-497) |
| 4B-H2 | No CI compile gate for either ESP8266 or ESP32 target | `39bbd2fe` (TASK-497) |
| 4B-H3 | `MERGED_APP_SIZE` drift + stale `partitions.csv` duplicate | `39bbd2fe` (TASK-497) |

### Medium: 31

Closed:

| ID | Title | Closing commit |
|---|---|---|
| 4A-M1 | C-style cast chain in f8.8 conversion → canonical `floatToF88` helper at 11 sites | `39bbd2fe` (TASK-497) |
| 1A-M2 | `lastThermostatVal = 0xFFFF` sentinel collision with valid f8.8 value | `90608866` (TASK-498) |
| 1B-M3 | New BLE helpers broke `satBLE*` naming convention | `90608866` (TASK-498) |
| 2A-M3 | BTHome v2 parser bailed on first unknown object 0x00 (packet-id) | `90608866` (TASK-498) |
| 3A-M1 | TASK-494 coalesce-by-MsgID had no host test | `90608866` (TASK-498) |
| 4B-M2 | `otCmdQueueHighWater` only visible via telnet | `90608866` (TASK-498) |
| 4A-M2 | `SATBLEScanCallbacks` not marked `final` | `90608866` (TASK-498) |
| 1A-M5 | BLE state-topic format duplicated in two places | `4d7bfaa0` (TASK-499) |
| 1B-M1 | ADR-077 bounded-payload single-publish exception not codified | `4d7bfaa0` (TASK-499) |
| 3B-M1 | ADR-092 missing TASK-497 portMUX cross-link | `4d7bfaa0` (TASK-499) |
| 3B-M2 | `c4-code-otdirect.md` / `c4-code-sat.md` stale (BLEDevice.h, 7-vs-8 entry override-table, pre-TASK-466 mapping) | `4d7bfaa0` (TASK-499) |
| 3B-M3 | MQTTstuff.ino block comment claimed "two-pass shape ADR-077 requires" | `4d7bfaa0` (TASK-499) |
| 1A-M1 | `setRemoteOverride()` ghost override on queue-full (state set before enqueue) | `83e0a72e` (TASK-500) |
| 1A-M3 | TT=/TC= negative-value clear semantics — documented as not-a-bug per gateway.asm parity | `83e0a72e` (TASK-500) |
| 2A-M2 | Default-allow MAC filter trust model — documented in MANUAL.md (option c) | `83e0a72e` (TASK-500) |
| 2B-M1 | 32-publish first-scan burst — `delay(0)` between sensors | `83e0a72e` (TASK-500) |
| 3A-M2 | MAC-filter bypass scenarios untested | `83e0a72e` (TASK-500) |
| 3A-M3 | BLE byte-layout parsers (ATC/pvvx, BTHome v2) untested | `83e0a72e` (TASK-500) |
| 3A-M4 | Encrypted-BTHome-flag rejection untested | `83e0a72e` (TASK-500) |
| 3A-M5 | OTDirect coalesce semantics not host-tested for non-MsgID-16 cases | `83e0a72e` (TASK-500) |
| 3A-M5 | Function contract on `bleSensorPublishHaDiscovery` bool-return semantics undocumented | `4d7bfaa0` (TASK-499 doc sweep) |

Deferred:

| ID | Title | Rationale |
|---|---|---|
| 2B-M3 | `OT_CMD_QUEUE_SIZE = 12` borderline for new TT/TC + CS/C2 + SAT stack | Defer until `otCmdQueueHighWater` field-data is collected; coalesce-by-MsgID (TASK-494) reduces effective depth substantially |
| 4A-M2 follow | TT/TC dispatch table per ADR-078 | Larger architectural sweep; no-op this round |
| 3A-M4 | `00-scope.md` evaluate.py framing inconsistency | Process annotation only; convention clarification deferred to next review template revision |
| 4B-M1 | "Safe OTA rollback" claim contradicts single-`ota_0` partition reality | Batch D (TASK-501) — not yet landed |
| 4B-M3 | No dependency-vulnerability scan on `lib_deps` | Batch D (TASK-501) — not yet landed |
| 4B-M4 | No SHA256SUMS published with releases | Batch D (TASK-501) — not yet landed |
| 4B-M5 | No formal hardware-verification log template | Batch D (TASK-501) — not yet landed |
| 3B-M4 | SAT C4 component diagram does not surface cross-task BLE model | Documentation-only; deferred to next C4 sweep |
| 3B-M5 | SAT REST `/api/v2/sat/status` schema in ch09 omits `ble_*` fields | Documentation-only; deferred to next manuals sweep |

### Low: 46 — Batch E (TASK-502) not yet landed

The polish sweep has not landed at HEAD `83e0a72e`. All 46 Lows
remain queued for Batch E execution per the followup plan: cumulative
naming polish, comment refresh, RAII over manual `portENTER/EXIT_CRITICAL`,
named OT-frame accessors, SemVer pin tightening, idiom cleanup. None
are bugs, none block 2.0.0 merge.

## 3. Architectural amendments

| ADR | Status | Change |
|---|---|---|
| ADR-068 | Amended in `ccee66ed` (TASK-495) | OT_CMD_QUEUE_SIZE 8 → 12; added Coalesce-by-MsgID subsection covering position-preserving replacement and high-water counter |
| ADR-077 | Amended in `4d7bfaa0` (TASK-499 1B-M1) | Bounded-payload single-publish exception clause: helpers ≤ 1 KB may use single-buffer + one chunk if banner comment justifies the bound; two-pass MEASURE/WRITE remains mandatory for unbounded payloads |
| ADR-090 | Amended in `ccee66ed` (TASK-495 / 3C-1, then cross-linked in TASK-499 3B-M1) | Cross-task race coverage: sub-rule 4 ("no volatile required") scoped to cooperative-loop re-entrancy paths only; FreeRTOS cross-task case (NimBLE host task ↔ Arduino loop task) named with `_bleSensorsMux` portMUX as canonical exemplar |
| ADR-092 | Amended in `ccee66ed` (TASK-495 / 3C-2) and `4d7bfaa0` (TASK-499 3B-M1) | "Periodic-scan model retained" replaced with continuous-scan model (TASK-494 fact); cross-link to ADR-090 sub-rule 4 amendment recording the cross-task pattern |

No new ADRs were created. ADR-093 (queue model) and ADR-094
(FreeRTOS cross-task pattern) were proposed in 3C but the chosen
path was Option C: amend in place rather than mint new ADRs. The
pattern-level rule from ADR-080 was honoured by labelling the
cross-task exemplar as guideline-level (single instance, below the
recurrence bar for binding pattern-level enforcement).

## 4. Behavioural changes

User-visible firmware behaviour delta from this review pass:

- **BLE scanner** (TASK-487): Bluedroid → NimBLE-Arduino 2.x. ~400 KB
  flash freed. No more 3-second blocking scan in `loop()`. Continuous
  scan model (TASK-494): scan starts once at `satBLEInit()` and runs
  forever; `iBleInterval` repurposed from scan-rate to publish/state
  update cadence. The 30-second startup blackout window is gone.
- **BLE Home Assistant discovery** (TASK-488): per-MAC discovery topic
  family `<HaPrefix>/sensor/<uniqueId>_ble_<mac>_<kind>/config` for
  every BLE sensor in range, four entities per MAC (temperature,
  humidity, battery, RSSI). Per-MAC state topic family
  `sat/ble/<mac>/{temp,rh,bat,rssi}` published every `iBleInterval`.
  One-shot retain-config per slot; recycled-slot reset (TASK-489)
  triggers a fresh discovery for the new MAC.
- **OTDirect TT/TC PIC-parity** (TASK-466): `TT=` and `TC=` now drive
  a state machine that mirrors the PIC `gateway.asm` reference: TT
  auto-clears on thermostat program shift, TC persists until `TC=0`,
  both push a clearing `MsgID 100 = 0` frame on clear. `MsgID 100`
  flag-byte synthesis added (`0x0001` for constant override, `0x0002`
  for temporary).
- **OTDirect queue diagnostics**: queue coalesce-by-MsgID (TASK-494)
  reduces effective depth in the common case (same-MsgID enqueue
  replaces in place rather than appending). High-water counter
  exposed via REST `/api/v2/otdirect/status` JSON (TASK-498 / 4B-M2).
  Ghost-override fix (TASK-500 / 1A-M1): `setRemoteOverride()` now
  enqueues MsgID 16 first and only commits state on success — honours
  the owner's "queue is the channel, no side-channels" principle.
- **Numerical contract**: f8.8 conversion now goes through a single
  canonical `floatToF88` helper with sign-extend + clamp at 11 call
  sites (TASK-497 / 4A-M1). Closes the latent UB at every site that
  converts a float to OT f8.8.
- **BLE parsing robustness**: BTHome v2 parser now handles object ID
  0x00 (packet-id, 1-byte payload) by skipping rather than bailing
  (TASK-498 / 2A-M3); previously sensors emitting packet-id before
  temperature dropped silently.
- **CI surface**: PR-time compile gate added for both ESP8266 and
  ESP32 (TASK-497 / 4B-H2). `evaluate.yml` and the OT v4.2 spec audit
  now trigger on `feature-dev-*` / `feature-*` branches (TASK-497 /
  4B-H1). Partition-size constant (`MERGED_APP_SIZE`) corrected to
  match `partitions_otgw_esp32.csv` (TASK-497 / 4B-H3).

## 5. Deferred items with rationale

- **2B-M3 (queue depth review)**: deferred until field telemetry from
  the new `otCmdQueueHighWater` REST/MQTT exposure (Batch A close)
  is available. Coalesce-by-MsgID materially reduces effective depth;
  bumping `OT_CMD_QUEUE_SIZE` without data would violate the owner
  binding "queue is THE channel / fix mechanics first" principle.
- **4A-M2 follow (TT/TC dispatch table per ADR-078)**: larger
  architectural sweep, not appropriate inside the review's
  fix-during-review window. ADR-078 dispatch tables already exist in
  MQTTstuff for sub-command dispatch; the OTDirect command surface
  has 30+ commands and would benefit from the same shape, but YAGNI
  triggers when a third command in the same category lands.
- **4B-M1 (OTA rollback claim)**: pending Batch D (TASK-501). Choice
  is binary — either match doc to reality (single `ota_0` slot, USB
  recovery only) or adopt 8 MB flash variant with dual-slot layout.
  Owner decision required.
- **4B-M3..M5 (dep-vuln scan, SHA256SUMS, hardware-verify template)**:
  pending Batch D (TASK-501). Each is independent CI/release
  tooling; not blocking 2.0.0.
- **All 46 Lows (Batch E / TASK-502)**: polish sweep, no bugs. Cherry
  pick the trivial ones (~20 of 46) when next stable cadence allows;
  carry the rest to next major work cycle.

## 6. Lessons for next review iteration

- **Tier-1 patterns landed during review become binding via CI gate**.
  The 2B-H1 watchdog-feed pattern, the heap-tier discovery pattern,
  and the cross-task race pattern were all surfaced during the
  review. ADR-088 (status-burst windowing + cooldown) and ADR-089
  (heap tier machine) were already binding via `evaluate.py` gates;
  the cross-task race was added to ADR-090 as guideline-level rather
  than minting a new pattern-level ADR (single instance, below the
  ADR-080 binding bar).
- **Cooperative re-entry and FreeRTOS cross-task are different
  classes**. ADR-090's original scope was cooperative ESP8266
  re-entry into shared scratch buffers (`doBackgroundTasks` re-entered
  from inside `doAutoConfigure`). The NimBLE host task on ESP32-S3
  introduces a structurally distinct concurrency class. The amendment
  makes the distinction explicit so the next reviewer is not confused
  by the two patterns sharing one ADR home.
- **Multi-agent execution validated as faster than serial fixes**.
  Eleven parallel agent runs, fifteen Highs closed in five fix-batch
  commits during the review itself. The "review-while-active"
  workflow paid for itself in one session. Drift between ADR text
  and shipped code is only visible when someone deliberately compares
  record to reality.
- **Build verification gate (TASK-497) caught the partition-size
  bug**. The 4B-H3 `MERGED_APP_SIZE` drift was a 1.875 MB vs 2.875 MB
  mismatch that would only surface at user-flash time. CI compile +
  partition-table audit catches it before tag.
- **Backlog hygiene compounds**. Each closing commit's body reads as
  a release note; `git log` + `backlog task view <id>` reconstructs
  the full chain of decisions. The CHANGELOG sweep (TASK-495 / 3B-H3)
  is what turns the internal trail into a user-facing one — keep
  that step in the next review template.

## 7. Hardware verification still required

The branch is clean for merge to `dev` once these owner-driven
hardware acceptance criteria land:

- **TASK-487 AC #6**: BLE NimBLE port — both BTHome v2 and ATC/pvvx
  sensor formats discovered on real OTGW32 hardware.
- **TASK-488**: Per-MAC HA discovery surfaces in HA UI; entities
  appear under correct device for each MAC.
- **TASK-494 AC #11**: Continuous BLE scan — sensors discovered
  within 30 s of boot (not the previous 30-s startup blackout
  window). `otCmdQueueHighWater` stays low under realistic load.
- **TASK-466 AC #12**: TT/TC override + auto-clear cycle — issue
  `TT=20`, observe thermostat echo with `honoredCount` increment;
  shift thermostat program; observe TT auto-clear; verify TC
  persistence under same scenario.
- **TASK-500 1A-M1 (queue stress)**: with the ghost-override fix in
  place, exercise queue-full behaviour by spamming TT=/TC= via MQTT
  and confirm the override-table never claims an override that has
  no frame in flight.
- **TASK-496 ACs 3 and 4**: host-side test execution. Owner runs
  `g++ -std=c++17 -Wall -Wextra tests/test_otdirect_override.cpp -o /tmp/test && /tmp/test`
  and `tests/test_ble_parsers.cpp` on a host with g++ available.

## 8. References

- Branch: `feature-dev-2.0.0-otgw32-esp32-sat-support`
- Followup plan: `.full-review/06-followup-plan.md`
- Final report: `.full-review/05-final-report.md`
- This summary: `.full-review/07-closure-summary.md`
- Phase reports: `.full-review/phase{1a,1b,2a,2b,3a,3b,3c,4a,4b}-*.md`

Closing commits in dependency order:

| SHA | Subject |
|---|---|
| `55fd0cf6` | fix(satble,otdirect,mqttstuff,platformio): five high-priority comprehensive-review follow-ups (TASK-493/494/495/496/497) |
| `805c6728` | fix(satble,otdirect): BLE continuous-scan + queue coalesce-by-MsgID + high-water diagnostic (TASK-494) |
| `ccee66ed` | docs(release,api,adr,webui): close Phase 3 doc + ADR drift Highs (TASK-495) |
| `23e4b4e2` | test(otdirect): host-side unit test for TT/TC numerical contracts (TASK-496, 3A-H1) |
| `39bbd2fe` | fix(ci,partitions,otdirect): close Phase 4 highs (TASK-497) |
| `6537b75a` | chore(review,backlog): comprehensive review Phase 5 final report + Done-flip TASK-497 |
| `90608866` | fix: Batch A quick-win Mediums (TASK-498) |
| `4d7bfaa0` | docs(adr,c4): Batch B doc/ADR drift consolidation (TASK-499) |
| `83e0a72e` | fix(otdirect,ble,tests): Batch C real-issue Mediums (TASK-500) |

ADRs amended this cycle: ADR-068, ADR-077, ADR-090, ADR-092. No new
ADRs minted; the FreeRTOS cross-task pattern is recorded at
guideline-level inside ADR-090 rather than promoted to a new
pattern-level ADR (single instance is below the ADR-080 binding bar).

Outstanding: Batch D (TASK-501, operational — OTA rollback doc fix,
dep-vuln scan, SHA256SUMS, hardware-verification log template) and
Batch E (TASK-502, Lows polish sweep). Neither blocks 2.0.0 merge to
`dev`.
