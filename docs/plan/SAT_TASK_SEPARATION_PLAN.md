# SAT Task Separation + Core 0 Task Family — Master Plan

**Status:** Proposal — Phase 1 measurement work pending user start-go. Scope and parent-ADR timing confirmed by user 2026-05-28.
**Scope:** `feature-dev-2.0.0-otgw32-esp32-sat-support` worktree only. Dev (1.5.x) has no SAT files and is unaffected.
**Confirmed scope:** SAT + weather + webhook (full Core 0 task family pattern), parent ADR drafted alongside SAT child ADR with status Proposed, both Accepted together after Phase 5 SAT soak.
**Author context:** Drafted from a discussion about moving SAT off the cooperative loop and onto a FreeRTOS task pinned to Core 0 on ESP32-S3 (OTGW32). The user flagged the central concern: doing this ties SAT (and the sibling tasks) to dual-core, i.e. structurally to ESP32-S3.

---

## 0. FreeRTOS scheduling model on ESP32-S3

This section is the architectural baseline the rest of the plan rests on. It is *not* an implementation detail — it determines whether the "tasks on Core 0" framing is honest. Read this section before §1.

### 0.1 Preemption semantics

The ESP32-S3 has two Xtensa LX7 cores. Wi-Fi/BT system tasks and (in this firmware) the NimBLE host task already run on Core 0. The Arduino `loop()` / `doBackgroundTasks()` runs on Core 1 by default. We propose adding three application tasks pinned to Core 0.

Core 0 runs all its tasks via FreeRTOS preemptive scheduling:

- A task **blocked on I/O** (TCP read inside `http.GET()`, `vTaskDelay`, queue/notify wait, semaphore take) consumes **zero CPU**. FreeRTOS immediately runs the next ready task.
- A task **wakes** the moment its wait condition is satisfied (network bytes arrived, sleep expired, queue has an item).
- Two tasks both wanting CPU at the same instant → **higher priority wins**; equal priority → round-robin time slice (default 1 ms tick).
- A task that never yields can starve lower-priority peers. Avoidable by always blocking on I/O or `vTaskDelay` between iterations.

### 0.2 Active-fraction estimates per task

Each application task spends almost all its lifetime blocked, not running:

| Task | CPU-active fraction | Where time is spent |
|---|---|---|
| Weather fetch | ~0.01% | 15-min `vTaskDelay`, then 1–5 s of TCP I/O (mostly blocked) + brief parsing |
| Webhook dispatch | ~0% steady | Blocked on `ulTaskNotifyTake`; wakes only on status-bit change |
| SAT controller | ~1–5% | 1–10 s `vTaskDelay`, then 5–100 ms PID/curve compute |

Probability of two being CPU-bound at the same instant is low. When it happens, FreeRTOS resolves it via priority/time-slicing without anyone explicitly blocking anyone else.

### 0.3 Contrast with the cooperative loop

The substantive improvement: today, when `http.GET()` runs in the loop, **every other piece of firmware on Core 1** is frozen for the duration. `webhook.ino:201` literally documents this with the comment *"Synchronous call into HTTPClient; setTimeout below caps the main-loop stall"*. The 3-second timeout cap is a damage-control measure for a freeze that cannot be eliminated under the cooperative model.

On a Core 0 task, `http.GET()` blocks **only the calling task**. The CPU runs system tasks (Wi-Fi/BLE) and our peer application tasks during the network wait. The freeze is gone, not just bounded.

### 0.4 Recommended priority table

| Task | Priority | Rationale |
|---|---|---|
| Wi-Fi system | ~22 (SDK default) | OS responsibility, do not touch |
| NimBLE host | ~10 (library default) | OS responsibility, do not touch |
| Webhook | 4 | Fire promptly when an event arrives |
| SAT controller | 3 | Predictable cadence, not latency-critical |
| Weather | 2 | Once per 15 min, can wait |
| IDLE Core 0 | 0 | Feeds watchdog |

Priorities are part of the new parent ADR's Decision section (§15). They are tuneable in Phase 5 soak; the table above is the starting point.

### 0.5 Two explicit non-promises

- **Tasks are not on separate CPUs.** Two simultaneously-CPU-bound tasks still serialize on Core 0.
- **Discipline is required to avoid starvation.** A pathological busy-loop without yields would starve peers. Every iteration of every task in this plan ends with a blocking wait (`vTaskDelay`, `ulTaskNotifyTake`, or `http.GET()` itself which blocks on TCP). The reviewer should verify this on every PR.

---

## 1. TL;DR — what you're deciding

**Do we lift the blocking-network subsystems off the cooperative `doBackgroundTasks()` loop and run them as dedicated FreeRTOS tasks pinned to Core 0?**

Concretely, three subsystems:

- **SAT controller** (8101 lines across 9 files) — heating-controller logic, currently on the loop, latency-coupled to the OT-direct scheduler via shared loop time.
- **Weather fetch** (`SATweather.ino`, 737 lines) — three synchronous `http.GET()` sites that block the entire loop during HTTP round-trips.
- **Webhook dispatch** (`webhook.ino`, 365 lines) — synchronous `http.POST/GET` capped at 3 s by a workaround timeout, fronted by the ADR-048 state machine that exists *only* because there is no task model.

If yes, all three subsystems become structurally ESP32-S3-only (the FreeRTOS-task model has no ESP8266 analogue). You'd be burning the bridge to "SAT/weather/webhook on ESP8266" — a bridge that for SAT is currently *theoretically* open in the source but not a tested or shipped configuration, and for weather/webhook is open in *both* directions on ESP8266 but at the cost of the workarounds those files already contain.

There are three sub-paths if the answer is yes (see §3); the recommended one (A) is to make the ESP8266 cut explicit via a `HAS_SAT` capability flag in `boards.h`, mirroring `HAS_DIRECT_OT` / `HAS_OLED_CAPABLE`. Weather and webhook stay compiled on both targets but with the cooperative path retained for ESP8266 (Option A applies to SAT only — see §3).

---

## 2. Why do this at all

Substantive reasons, in priority order:

1. **Latency isolation for the OT-direct scheduler.** When SAT's once-per-cycle work fires (PID + autotune step + weather fetch + MQTT discovery burst), it rides the cooperative loop and can delay the OT scheduler's next dispatch. Today this is masked by ADR-068's 200 ms jitter margin on the 800 ms status interval. Moving SAT off the loop removes that source of jitter, potentially allowing the status interval to return toward spec (1000 ms) and recover ~25% of bus time for data polling.
2. **Cooperative-loop stalls from blocking HTTP eliminated structurally.** `webhook.ino:201` admits the stall in a comment. `SATweather.ino` has three synchronous `http.GET()` sites. Each runs for whatever the network takes. On Core 0 tasks, these freezes affect only the calling task, not the whole firmware.
3. **BLE + SAT colocation on Core 0.** NimBLE's host task already runs on Core 0; `_bleSensorsMux` (ADR-090 amendment, 2026-04-30) crosses BLE→loop today. With SAT on Core 0, the BLE→SAT path becomes in-core (cheap), and a single new cross-core boundary is introduced in the *other* direction: SAT → OT command queue.
4. **Architectural modularization.** ADR-070 already did most of the structural homework for SAT (file-static private state with accessor-function discipline, two-tier public/private state model). Promoting SAT to a task crystallizes the existing interface contract.
5. **ADR-048 supersession.** Webhook's three-state machine (`WH_IDLE` → `WH_PENDING` → `WH_RETRY_WAIT`) exists *only* because the cooperative loop cannot tolerate synchronous HTTP. With a task, the state machine collapses to linear code (~100 LOC removed). The retry policy from ADR-057 is unchanged in intent; only its execution context moves.
6. **A documented architectural pattern that scales.** Once "Core 0 hosts blocking-network tasks" is named in a parent ADR, new outbound-HTTP features (energy-price fetch, remote-log push, anything else) have a documented home. No more ad-hoc state-machine workarounds.
7. **`SATweather.ino` HTTP-fetch isolation.** The weather fetch uses `String` and can block for hundreds of ms to seconds. On the cooperative loop, that's a real stall. As a task it's an irrelevant local slowdown — and as ADR-070 §4 admits, the `String` usage there is permitted only because the call is rare. With a task, even the rare argument is moot.

**What it does NOT solve:**

- Does *not* fix OT-direct scheduler jitter on its own (would require putting OT-direct on its own task too — explicitly out of scope here).
- Does *not* address CPU contention with Wi-Fi/MQTT bursts — those stay on Core 1 / the cooperative loop.
- Does *not* unlock any new *features*. Purely structural.

If none of the reasons above resonate, the right answer is **no**. The cooperative model has shipped successfully; this proposal does not exist to chase a hypothetical problem.

---

## 3. The dual-core lock-in — the central trade-off

**Current state of platform coupling** (verified against source 2026-05-28):

### 3.1 SAT

| File | Top-level platform guard | Notes |
|---|---|---|
| `SATcontrol.ino` (4038 lines) | None | Compiles on both. Internal `#if defined(ESP32)` guards around BLE paths at lines 975, 1948, 2559, 3042. |
| `SATpid.ino` | None | Pure PID math — platform-agnostic. |
| `SATcycles.ino` | None | Cycle tracker — platform-agnostic. |
| `SATweather.ino` | None | HTTPClient — works on both. |
| `SATble.ino` (870 lines) | Effectively ESP32-only (NimBLE) | Already a cross-core boundary via `_bleSensorsMux`. |
| `SATmqttPublish.cpp/h` | None | Platform-agnostic. |
| `SATtypes.h` | None | Headers. |
| `boards.h` | **No `HAS_SAT` flag exists** | Always compiled in on both targets. |
| `OTGW-firmware.ino:236` | `initSAT()` called unconditionally | |
| `OTGW-firmware.ino:643` | `satControlLoop()` called unconditionally | Timer-guarded internally per ADR-070. |

Today, the ESP8266 build *compiles SAT in*. Whether it *runs correctly under realistic load* is the question ADR-070 anticipated but never proved.

### 3.2 Weather

Weather is invoked through `SATweather.ino` and is structurally part of SAT today. If SAT is cut from ESP8266, weather follows automatically. If we keep SAT on ESP8266 (Option B), weather's HTTP fetch remains on the cooperative loop on that platform — i.e. one platform pays the workaround tax, the other doesn't.

### 3.3 Webhook

Webhook is platform-independent in `webhook.ino` — runs on both targets today and is unrelated to SAT. The ADR-048 state machine works on both platforms. If webhook becomes a Core 0 task on ESP32, **the ESP8266 build must retain the cooperative state machine** (no FreeRTOS on ESP8266). That means a dual implementation: `#if defined(ESP32) { task version } #else { state machine version }`. Net: webhook code grows a `#if` boundary even though the ESP32 version's body shrinks.

### 3.4 Three options for handling the cut

| Option | SAT on ESP8266 | Weather on ESP8266 | Webhook on ESP8266 | Notes |
|---|---|---|---|---|
| **A — Recommended (confirmed)** | Cut via `HAS_SAT=0` | Cut (follows SAT) | Cooperative version retained | SAT/weather become structurally OTGW32-only; webhook becomes dual-implementation. Mirrors `HAS_DIRECT_OT` / `HAS_OLED_CAPABLE` pattern. |
| B | Dual cooperative+task | Dual | Dual | All three subsystems dual-implementation. Preserves theoretical ESP8266 capability but doubles maintenance for SAT and weather where it was never proven anyway. |
| C | Refactor to accessor boundaries only, no task | No change | No change | Captures modularization but not the latency/stall benefits. Risk of "never come back to Phase 4". |

**Confirmed: Option A.** The user has explicitly accepted the SAT-on-ESP8266 lock-in. Weather follows SAT into the cut (since it's structurally part of SAT). Webhook gets a dual implementation because it has a working ESP8266 path that's worth preserving.

---

## 4. Proposed architecture (SAT)

A single `satControlTask` pinned to Core 0 on ESP32-S3. Priority 3 (per §0.4). Woken either on periodic tick (`vTaskDelay(pdMS_TO_TICKS(1000))`) or on notify from BLE/OT producers.

### 4.1 Interface contract

| Direction | Mechanism | Location of new code | Risk |
|---|---|---|---|
| **In: OT boiler cache** (`otBoilerCache[128]` in `OTDirect.ino:268`) | New `portMUX_TYPE _otBoilerCacheMux`. Snapshot pattern: SAT copies 8 fields under lock, processes outside. | `OTDirect.ino` (writer-side critical sections), `SATcontrol.ino` (snapshot accessor) | Low. Critical sections <5 µs each. |
| **In: BLE sensor cache** | Existing `_bleSensorsMux` (`SATble.ino`, ADR-090 amendment) | None | Zero. Already shipping. |
| **In: settings** (`settings.sat.*`) | Snapshot to local `SATSettingsView` on task wake | New helper in `SATtypes.h` | Low. |
| **In: outdoor temp & external indoor** | New `_satExternalInputsMux` for ~3 loop-writer / SAT-reader fields | `SATcontrol.ino` | Low. |
| **Out: OT commands** | Existing `otCmdEnqueue()` (`OTDirect.ino`, ADR-068 fan-in) wrapped in `portENTER_CRITICAL`/`EXIT_CRITICAL` around head/tail+coalesce | `OTDirect.ino` — ~5 lines | Low. <2 µs critical. |
| **Out: `state.sat.*`** | New `portMUX_TYPE _satStateMux`; writers acquire, readers take stack copy under same lock | `OTGW-firmware.h` (declaration), all access sites | Medium. ~30–50 sites to audit. |
| **Out: MQTT publish** | SAT pushes deltas to small SPSC ring; loop's MQTT slot drains it | `SATmqttPublish.cpp/h` redesign | Low–Medium. ADR-111 already separates SAT MQTT timing. |
| **Internal `_module_` statics** | Untouched. SAT task is sole accessor. | None | Zero. |

### 4.2 What stays unchanged

- **OT-direct scheduler** stays on the cooperative loop. ADR-068's framing remains valid.
- **ADR-090 sub-rule 4** stays valid for cooperative-loop-only paths.
- **All other firmware code** (MQTT, REST, OTA, WebSocket, ser2net, OLED) unaffected.
- **Frame bridge (ADR-087), mode enum (ADR-064), `otCmdEnqueue` coalesce semantics (ADR-068)** preserved.

### 4.3 Boot ordering

In `OTGW-firmware.ino:setup()`, existing `initSAT()` at line 236 splits into:

1. `satInitDataStructures()` — runs in `setup()`. Initializes muxes, publish ring, accessors. No task created yet.
2. `satStartTask()` — runs *after* `state.bSetupComplete = true` (line 212), gated on `isOTDirectEnabled()`. Calls `xTaskCreatePinnedToCore(satControlTaskFn, "sat_ctrl", SAT_TASK_STACK_BYTES, NULL, /*priority=*/3, &satTaskHandle, /*coreId=*/0)`.

The existing call to `satControlLoop()` from `doBackgroundTasks():643` is removed.

### 4.4 Task stack sizing

Start at **8 KB** on ESP32-S3 (we are not on ESP8266's 4 KB CONT stack). Instrument with `uxTaskGetStackHighWaterMark()` for the entire Phase 5 beta. Settle to actual high-water + 50% headroom in Phase 6.

The two HTTP `String` allocations in `SATweather.ino` are the most stack-hungry path; instrument specifically.

### 4.5 Watchdog

SAT task calls `vTaskDelay(pdMS_TO_TICKS(1000))` between iterations. Yields to per-core IDLE task, which feeds WDT. No need to disable IDLE WDT on Core 0.

---

## 5. Affected files

| File | Nature of change | LOC delta |
|---|---|---|
| `src/OTGW-firmware/boards.h` | Add `HAS_SAT` flag (Option A) | +6 |
| `src/OTGW-firmware/OTGW-firmware.h` | Add `_satStateMux` / `_otBoilerCacheMux` / `_weatherDataMux` / `_satExternalInputsMux` declarations; `SATSettingsView` struct | +30 |
| `src/OTGW-firmware/OTGW-firmware.ino` | Remove `satControlLoop()` from `doBackgroundTasks()`; add task start after `bSetupComplete`; same for weather + webhook tasks | ±20 |
| `src/OTGW-firmware/OTDirect.ino` | Add `_otBoilerCacheMux`; wrap cache updates and `otCmdEnqueue` head/tail in critical sections | +40 |
| `src/OTGW-firmware/SATcontrol.ino` | Replace direct reads with snapshot accessors; add task-loop wrapper; rename `satControlLoop()` to `satControlIteration()` | ~150 touched, +50 net |
| `src/OTGW-firmware/SATmqttPublish.cpp/h` | Replace direct publish calls with SPSC ring; loop-side drain function | ±80 |
| `src/OTGW-firmware/SATble.ino` | Minor: remove any direct `state.sat.*` writes if present | minor |
| `src/OTGW-firmware/SATweather.ino` | Task wrapper around existing fetch logic; replace `_weather_forecastTemp[24]` direct access with snapshot accessor under `_weatherDataMux` | +60, −20 |
| `src/OTGW-firmware/webhook.ino` | **Remove** ADR-048 state machine for ESP32; replace with linear task body. Keep `#if defined(ESP32)` task version + retained cooperative version for ESP8266. | +40, −140 (net **−100**) |
| `src/OTGW-firmware/MQTTstuff.ino`, `restAPI.ino` | All `state.sat.*` reads wrapped in snapshot-copy pattern | ~30 sites |
| `platformio.ini` | Optionally `-DHAS_SAT=1` for `otgw32` env | minor |
| `docs/adr/ADR-NNN-core0-task-family-blocking-network-io.md` | **NEW** parent ADR | new file |
| `docs/adr/ADR-NNN-sat-task-separation-on-core-0.md` | **NEW** SAT child ADR | new file |
| `docs/adr/ADR-NNN-weather-task-separation-on-core-0.md` | **NEW** weather child ADR | new file |
| `docs/adr/ADR-NNN-webhook-task-separation-on-core-0.md` | **NEW** webhook child ADR (supersedes ADR-048) | new file |
| `docs/adr/ADR-048-...` | Status change: Accepted → **Superseded by ADR-NNN** | status-line only |
| `docs/adr/ADR-055-...`, `ADR-057-...` | Amendment: webhook policy unchanged, execution context moves | minor |
| `docs/adr/ADR-070-...` | Amendment: ESP8266 framing historical; weather buffer ownership | minor |
| `docs/adr/ADR-085-...` | Amendment: lifecycle subsection | minor |
| `docs/adr/ADR-090-...` | Amendment: extend cross-task exemplars with new muxes | minor |
| `docs/adr/ADR-111-...` | Amendment: publish path now via ring + loop drain | minor |

**Total impact:** ~250 LOC net (SAT alone) + ~100 LOC net (weather) + **−100 LOC net** (webhook, due to state machine removal) ≈ **~250 LOC net total** across ~12 source files; 4 new ADRs; 6 ADR amendments; 1 ADR supersession.

---

## 6. Phased delivery

Phases 1–5 cover SAT (the proving ground). Phases 7–8 are sibling cycles for weather and webhook after SAT soaks.

| Phase | Scope | Done when | Reversibility |
|---|---|---|---|
| **1. Measurement** | Add `millis()` brackets around `satControlLoop()` iterations and the autotune step. Add HTTPClient-call timing for weather and webhook. Ship a beta. | Real data on SAT compute burst (open question #1) and HTTP-stall durations (validates the "structural improvement" claim). | Fully reversible — pure instrumentation. |
| **2. ESP8266 gating** | Introduce `HAS_SAT` flag (Option A). Gate SAT call sites. Confirm ESP8266 firmware builds & runs identically. ESP32-S3 still cooperative. | `python build.py` produces both binaries. Evaluator green. No regression. | Fully reversible — `HAS_SAT=1` on ESP8266 brings code back. |
| **3. Interface contract (SAT, no task yet)** | Introduce the three SAT-related portMUXes, wrap `otCmdEnqueue`, replace `state.sat.*` direct access with snapshot accessors, switch SAT MQTT publish to ring + loop drain. SAT still called from `doBackgroundTasks()`. | All access wrapped. `otCmdEnqueue` critical-section-safe. Ring publishing equivalent on the wire. Evaluator green. | Fully reversible — muxes are no-ops while SAT stays on loop. |
| **4. SAT task promotion** | Add `satControlTask`, `xTaskCreatePinnedToCore` on Core 0. Remove call from `doBackgroundTasks()`. Add stack high-water instrumentation. | Task runs continuously. Stack high-water stable. OT scheduler latency improves measurably vs Phase 1. | Partially reversible — backout = revert one commit. |
| **5. SAT soak + parent ADR acceptance** | Ship as beta. Monitor BGTRACE, stack high-water, OT scheduler jitter, MQTT publish lag, WDT resets. Two beta cycles minimum. At end: promote parent ADR + SAT child ADR from Proposed to Accepted. | No regressions over two beta cycles. Stack high-water settled. Parent ADR Accepted. | N/A — observation + documentation. |
| **6. ADR amendments** | Apply amendments to ADR-070/-085/-090/-111. Update ADR-068 ("status interval could now safely return to spec" — separate ADR if changed). | All amendments merged. | N/A — documentation. |
| **7. Weather task (sibling cycle)** | Lift weather fetch into `weatherFetchTask` on Core 0. Notify SAT task on fresh data. Single child ADR drafted and Accepted. | Task runs. Weather data delivered to SAT through snapshot accessor. No regression on `state.sat.*` weather fields. | Partially reversible. |
| **8. Webhook task (sibling cycle)** | Lift webhook into `webhookDispatchTask` on Core 0. Remove ADR-048 state machine for ESP32 path; retain for ESP8266. Single child ADR drafted, Accepted; ADR-048 status flipped to Superseded. | Task runs. Webhook events fire from loop-side producer queue. Net code change is negative. Webhook end-to-end verified in field. | Partially reversible. |

Phase 7 and 8 are sequenced serially after Phase 5/6 specifically because each subsystem needs its own beta cycle for "did anything break?" attribution.

---

## 7. Open questions to answer in Phase 1

1. **What's SAT's worst-case compute burst today?** Specifically the autotune step (ADR-085). If <50 ms, the "OT latency isolation" case is real but small. If >200 ms, the case is strong. Phase 1 instrumentation answers this.
2. **`SATweather.ino`'s `HTTPClient` — does it work cleanly from a non-loop task?** ESP-IDF's WiFi stack is happy from any task, but Arduino's `HTTPClient` wrapper sometimes assumes loop-context. Verify in a small prototype before Phase 4. If broken, the fix is to push HTTP fetch results into the loop via a queue and have the loop perform the HTTPS call — but that re-couples the worst-case-blocking case we wanted to isolate.
3. **Are settings ever written from the loop while SAT is mid-cycle?** If yes (e.g., REST API setpoint change during a long autotune step), we need snapshot-on-wake or a settings-dirty event. Verify via grep over `settings.sat.*` writers.
4. **Does ADR-085's smart-autotune state machine assume strict ordering between SAT decisions and OT-side responses?** If the autotune needs to see a specific OT MsgID response before advancing, cross-task latency (notify wake-up = ~1 ms typical) becomes a correctness question. Verify via reading `SATcontrol.ino` autotune block.
5. **How long does the worst-case webhook HTTP round-trip take in field conditions?** Justifies the "structural improvement" framing if observed durations are >>10 ms (likely true for any real LAN target). Phase 1 measures this.

**Stop-the-line:** Negative answer to #2 (HTTPClient unsafe off-loop) or #4 (autotune needs strict OT-frame ordering) would stop the project at Phase 3 — we'd keep the interface contract benefits and abandon Phase 4–8.

---

## 8. ADR consequences

### New ADRs

| ADR | Title | Status at draft | Status at Phase 5 |
|---|---|---|---|
| ADR-NNN (parent) | Core 0 Task Family for Blocking Network I/O on ESP32-S3 | Proposed | Accepted |
| ADR-NNN (SAT) | SAT Controller as Separately-Tasked Subsystem on Core 0 | Proposed | Accepted |
| ADR-NNN (weather) | Weather Fetch as Core 0 Task | (drafted in Phase 7) | Accepted at Phase 7 end |
| ADR-NNN (webhook) | Webhook Dispatch as Core 0 Task (supersedes ADR-048) | (drafted in Phase 8) | Accepted at Phase 8 end |

### Supersession

| ADR | Action |
|---|---|
| ADR-048 (nonblocking webhook state machine) | Status: Accepted → **Superseded by ADR-NNN** (webhook child). Body preserved as historical record per project ADR rules. |

### Amendments

| ADR | Amendment |
|---|---|
| ADR-055 (webhook outbound HTTP) | Policy unchanged; execution context moves to Core 0 task. |
| ADR-057 (webhook delivery retry) | Retry policy unchanged; runs inside task loop instead of via `WH_RETRY_WAIT` state. |
| ADR-070 (SAT memory) | §"ESP32-S3 task model" amendment. ESP8266 constraints in §Context become historical context. Add stack-sizing constants. Weather buffer ownership clarified. |
| ADR-085 (SAT integration) | §"Lifecycle on OTGW32" — boot ordering, task handle, shutdown, settings-changed propagation. |
| ADR-090 (re-entrancy guards) | Extend §Amendment 2026-04-30 with new cross-task exemplars: `_otBoilerCacheMux`, `_satStateMux`, `_weatherDataMux`, `_satExternalInputsMux`, `otCmdEnqueue` critical section. Cooperative re-entry rules unchanged. |
| ADR-111 (SAT MQTT publish-on-change) | Publish path now via SPSC ring + loop-side drain. Jittered-heartbeat policy unchanged. |

### ADRs explicitly NOT touched (intentional)

- **ADR-064** (OT-direct mode architecture) — unchanged. OT-direct stays on cooperative loop.
- **ADR-068** (OT-direct schedule tuning) — unchanged in Phases 3–4. *Possibly* eligible for follow-up after Phase 5 if jitter measurements warrant returning to spec 1000 ms — separate decision.
- **ADR-087** (frame bridge) — unchanged.
- **ADR-016** (OpenTherm command queue) — unchanged in spirit; only implementation gets a critical section.

---

## 9. Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Stack overflow on `satControlTask` | Medium at first ship, Low after Phase 5 | High (panic + reboot) | Start at 8 KB; high-water instrumentation in Phase 4; settle in Phase 6. |
| `HTTPClient` misbehaves on non-loop task | Low–Medium | Medium (weather/webhook broken) | Verify in Phase 1 prototype. Fallback: keep HTTP on loop, push result via queue. |
| Cross-task race on `state.sat.*` field we forgot to mux | Medium | Medium (stale/torn MQTT value) | Phase 3 audit; portMUX wraps the entire struct, not individual fields. |
| Settings change mid-cycle yields half-applied state | Low | Low (one cycle wrong, self-correcting) | Snapshot-on-wake. Documented in SAT child ADR. |
| Autotune state machine ordering-dependent on OT replies | Low (designed as polled state machine) | High if real (incorrect autotune) | Open question #4. If real, abandon Phase 4, ship Phase 3 only. |
| Beta soak surfaces WDT resets on Core 0 | Low | High (visible regression) | `vTaskDelay` between iterations + IDLE_TASK feeds WDT. If still fails: disable IDLE WDT on Core 0. |
| "Ship Phase 3 / 5 and never come back to Phase 7/8" | Medium (scheduling risk, not technical) | Low–Medium (paid the audit cost, didn't unlock the benefit) | Phase boundaries enforce decision points; user re-confirms commit to Phase 7/8 after Phase 5 soak. |
| Webhook dual-implementation drift between ESP8266 (state machine) and ESP32 (task) | Medium long-term | Medium (silent behavior divergence) | Phase 8 child ADR documents both paths explicitly; reviewer enforces parity. |
| ESP8266 build still attempts to compile weather as part of SAT after SAT is gated out | Low | Low (link error caught at CI) | `HAS_SAT` flag gates `SATweather.ino` inclusion in `platformio.ini build_src_filter`. |

---

## 10. Estimated cost

| Item | Effort (calendar) |
|---|---|
| Phase 1 (instrumentation + measurement) | 0.5 day code + 1 beta cycle |
| Phase 2 (`HAS_SAT` gating) | 0.5 day code + 1 beta cycle |
| Phase 3 (SAT interface contract, no task) | 1.5 days code (audit-heavy) + 1 beta cycle |
| Phase 4 (SAT task promotion + stack instrumentation) | 1 day code + 1 beta cycle |
| Phase 5 (SAT soak + parent ADR accept) | 0 days code + 2 beta cycles + 0.5 day docs |
| Phase 6 (ADR amendments) | 1 day docs |
| Phase 7 (weather task + child ADR) | 0.5 day code + 1 beta cycle + 0.5 day docs |
| Phase 8 (webhook task + child ADR + ADR-048 supersession) | 1.5 days code + 1 beta cycle + 0.5 day docs |
| **Total active development** | **~6.5 days** |
| **Total calendar (including beta cycles)** | **~10–12 weeks** |

Assumes Phase 1 doesn't kill the project (open questions #2 / #4 acceptable).

---

## 11. Cross-worktree consequences

- **`dev` branch (1.5.x, ESP8266):** No impact. Dev has zero SAT files. Webhook on dev keeps its existing ADR-048 state machine unchanged. No port required.
- **`feature-dev-2.0.0-otgw32-esp32-sat-support` branch:** All work here.
- **No master plan / two-task split required** (the cross-worktree pattern from `CLAUDE.md`). This is single-tree work end-to-end.

---

## 12. Decisions confirmed and remaining

### Confirmed by user 2026-05-28

| # | Question | Confirmed answer |
|---|---|---|
| 1 | Scope | **SAT + weather + webhook (full Core 0 task family)** |
| 4 | Parent ADR timing | **Draft now alongside SAT child ADR (Proposed); both Accepted together after Phase 5 SAT soak; weather and webhook child ADRs follow as sibling cycles citing the already-Accepted parent** |

### Remaining decisions (defer to Phase 1 start)

| # | Question | Options | Recommended |
|---|---|---|---|
| 2 | ESP8266 strategy | A: `HAS_SAT` flag, SAT cut from ESP8266 (with weather following) / B: dual implementation / C: refactor accessors only no task | **A** — mirrors `HAS_DIRECT_OT` / `HAS_OLED_CAPABLE`. Webhook gets a small dual-implementation `#if defined(ESP32)` boundary since it works on ESP8266 today. |
| 3 | Phasing | Bundled / sibling cycles after SAT | **Sibling cycles** — proves the pattern with SAT (hardest case) before extending. Per-subsystem beta gives clean "what broke?" attribution. |
| 5 | Stop-the-line criteria | Default: open questions #2 (HTTPClient off-loop) and #4 (autotune ordering) / propose alternatives | **Default** — these are the two questions whose negative answers genuinely kill Phase 4. |
| 6 | Backlog task structure | One umbrella + per-phase / Phase 1 only first | **Phase 1 only first** — minimum-commitment, re-decide for real after data. |
| 7 | Where to commit this plan doc | Push to `feature-dev-2.0.0-...` as docs-only commit / keep local until decision settled | **Push as docs-only** — `CLAUDE.md` allows docs-only commits to this branch without build/eval gates. Makes the plan reviewable in PR form. |

The recommended path through the remaining five: **2(A) → 3(sibling cycles) → 5(default) → 6(Phase 1 only) → 7(push as docs)**. This is the minimum-commitment / maximum-reversibility variant.

---

## 13. References

- ADR-016 — OpenTherm command queue
- ADR-048 — Non-blocking webhook state machine (supersession candidate)
- ADR-055 — Webhook outbound HTTP integration (amendment candidate)
- ADR-057 — Webhook delivery retry & protected test endpoint policy (amendment candidate)
- ADR-061 — Unified ESP8266/ESP32 platform abstraction
- ADR-063 — OTGW32 hardware support (the `HAS_*` capability flag pattern this proposal mirrors)
- ADR-064 — OT-direct operating mode architecture
- ADR-068 — OT-direct schedule tuning constants (the 200 ms jitter margin)
- ADR-070 — SAT memory allocation (foundation; subject to amendment)
- ADR-085 — SAT smart autotune integration (subject to amendment)
- ADR-087 — Frame bridge pattern
- ADR-090 — Re-entrancy guard pattern, with cross-task amendment of 2026-04-30
- ADR-111 — SAT MQTT publish-on-change (subject to amendment)
- `docs/plan/OTGW32_INTEGRATION_PLAN.md` — parent OTGW32 plan
- `docs/plan/HAL_2_0_0_DESIGN_PLAN.md` — sibling concern; check for HAL boundary intersect before Phase 3

---

## 14. Sibling tasks: weather and webhook

### 14.1 Weather (`weatherFetchTask`)

**Pinned to:** Core 0, priority 2.
**Wake cadence:** `vTaskDelay(pdMS_TO_TICKS(WEATHER_POLL_INTERVAL_MS))` — default 15 min, configurable. Also wakes on REST `/api/v2/weather/refresh` notify.
**Body:** Sequential: open `HTTPClient`, `http.GET()`, stream-parse via existing `weatherParseStream()`, populate forecast buffer under `_weatherDataMux`, close client, sleep.

**Interface contract:**

| Direction | Mechanism |
|---|---|
| In: refresh trigger | `vTaskDelay` timer **or** `xTaskNotifyGive` from REST handler |
| In: settings (provider URL, API key, location) | Snapshot at task wake |
| Out: forecast buffer (`_weather_forecastTemp[24]`, count, last-updated timestamp) | Wrapped in `_weatherDataMux`; SAT task reads via snapshot accessor `weatherGetForecastSnapshot()` |
| Out: error/status (for REST/UI) | Two atomic-int fields under same mux: last-HTTP-code, last-fetch-ms |

**ADR-070 §4 amendment:** The "permitted String exception" rationale ("low-frequency one-off") becomes moot — the String stays inside the task's local scope, never crosses a task boundary, can be discarded mid-iteration without affecting any other code path.

**No state machine to remove** — weather is already a "do it, sleep, do it again" pattern; the task body is just the existing fetch function with a `while(true) { fetch(); vTaskDelay(...); }` wrapper.

**Cost: ~0.5 dev days code + rides on SAT's beta cycles.**

### 14.2 Webhook (`webhookDispatchTask`)

**Pinned to:** Core 0, priority 4 (highest of the three — webhook events should fire promptly).
**Wake cadence:** Blocked on `xQueueReceive(webhookEventQueue, ...)` — wakes on each enqueued event.
**Body:** Linear retry loop:

```cpp
// CONCEPTUAL — not for implementation in this turn
while (true) {
  WebhookEvent ev;
  xQueueReceive(webhookEventQueue, &ev, portMAX_DELAY);
  for (int attempt = 0; attempt < WEBHOOK_MAX_RETRIES; attempt++) {
    if (sendWebhookSync(ev) == HTTP_OK) break;
    vTaskDelay(pdMS_TO_TICKS(WEBHOOK_RETRY_INTERVAL_MS));
  }
}
```

**Interface contract:**

| Direction | Mechanism |
|---|---|
| In: webhook events | `xQueueSendToBack(webhookEventQueue, &ev, 0)` from `evalWebhook()` on the loop |
| In: settings (URL, payload template) | Snapshot at event dequeue |
| Out: none | Fire-and-forget; only writes are to status counters under existing webhook stats struct, treated atomically (single writer) |

**ADR-048 supersession:** The `WH_IDLE` / `WH_PENDING` / `WH_RETRY_WAIT` state machine is *deleted* on the ESP32 path. The retry policy from ADR-057 is preserved inside the linear retry loop body — same intent, simpler code.

**Dual-implementation note:** The cooperative state machine is retained for ESP8266 via `#if defined(ESP32) ... #else ... #endif`. Parity is the maintainer's responsibility; the webhook child ADR documents both paths.

**Queue sizing:** 4 entries. Webhook events are status-bit changes — at most one pending at a time in practice. A 4-slot queue absorbs a burst (e.g., flame on → flame off → flame on within retry window) without policy change.

**Cost: ~1.5 dev days code + 1 dedicated beta cycle (webhook is user-visible; needs attribution).**

### 14.3 What SAT, weather, and webhook share

- All three are pinned to Core 0.
- All three are part of the same parent ADR (§15).
- All three use the same boot-ordering pattern: `xInitDataStructures()` in `setup()` first, `xStartTask()` after `bSetupComplete = true`.
- All three follow the same priority hierarchy (§0.4).
- All three are gated behind ESP32 detection (SAT via `HAS_SAT`, weather follows SAT, webhook via `#if defined(ESP32)`).

---

## 15. Parent ADR sketch — "Core 0 Task Family for Blocking Network I/O on ESP32-S3"

This is a sketch — not the final ADR file. Actual `docs/adr/ADR-NNN-core0-task-family-blocking-network-io.md` is drafted at Phase 3 start; Accepted at Phase 5 end.

### Status
Proposed → Accepted after Phase 5 SAT soak validates the pattern in production.

### Context

The OTGW firmware uses a cooperative single-threaded model where all background work is serialized through `doBackgroundTasks()` on Core 1 (Arduino default). This works well for non-blocking polled protocol work (MQTT publish/subscribe, REST handlers, OT-direct scheduler, OLED updates, ser2net). It works poorly for **synchronous outbound network I/O**, because a single `http.GET()` call freezes every other piece of firmware on Core 1 for the duration of the TCP round-trip — typically tens to thousands of milliseconds.

Today the codebase contains three subsystems that pay this tax:

- **Webhook** (`webhook.ino`): synchronous HTTP POST/GET to a local-network endpoint when an OT status bit changes. ADR-048 introduced a three-state state machine (`WH_IDLE` / `WH_PENDING` / `WH_RETRY_WAIT`) specifically to cap the cooperative-loop stall to 3 seconds; the rejected-alternatives section of ADR-048 explicitly says "Ticker callbacks run in interrupt context… would need to set a flag and process in the main loop anyway — which is exactly what the state machine does, more explicitly." The state machine *is* the workaround.
- **Weather** (`SATweather.ino`): three synchronous `http.GET()` sites for forecast retrieval (Open-Meteo, OpenWeatherMap). ADR-070 §4 admits the `String` usage there is permitted "only because the call is rare" — i.e. the same workaround-by-rarity pattern.
- **SAT** (`SATcontrol.ino` and siblings): no direct network calls, but its compute burst (PID + heating-curve + autotune step) shares the cooperative loop with the OT-direct scheduler, contributing to the 200 ms jitter margin ADR-068 maintains.

The ESP32-S3 has two cores. Core 0 already hosts the system Wi-Fi/BT tasks and (in this firmware) the NimBLE host task. Application work could be added to Core 0 as FreeRTOS tasks without conflicting with the Arduino-loop / OT-scheduler latency model on Core 1.

### Decision

**On ESP32-S3 (OTGW32 builds, `HAS_DIRECT_OT == 1`), blocking-network outbound calls (HTTP/HTTPS) live on dedicated FreeRTOS tasks pinned to Core 0. The cooperative loop on Core 1 owns non-blocking polled protocol/UI work only.**

Specifics:

1. **Task hosting**: Each blocking-network subsystem (SAT, weather, webhook, future subsystems of similar shape) runs in its own `xTaskCreatePinnedToCore(..., coreId=0)` task.
2. **Priority hierarchy**: Wi-Fi system (~22) > NimBLE host (~10) > webhook (4) > SAT (3) > weather (2) > IDLE (0). Application priorities are tuneable in Phase 5.
3. **Inter-task communication**: Producers on the cooperative loop enqueue events into `QueueHandle_t` / notify the task via `xTaskNotifyGive`. Consumers on Core 1 read task-produced data via portMUX-protected snapshot pattern (matching ADR-090's `_bleSensorsMux` exemplar).
4. **Boot ordering**: `xInitDataStructures()` in `setup()`; `xStartTask()` after `state.bSetupComplete = true`.
5. **Watchdog**: Every task body ends with a blocking wait (`vTaskDelay`, `xQueueReceive`, blocking I/O). The per-core IDLE task feeds the WDT during waits.
6. **ESP8266 fallback**: Subsystems retain their cooperative implementations on ESP8266 builds (`#if defined(ESP32) { task path } #else { cooperative path } #endif`). For SAT specifically, the cooperative path is removed entirely via `HAS_SAT=0` on ESP8266 (Option A in §3.4).

### Alternatives considered

1. **Status quo: keep cooperative + per-call state machines/workarounds.** Rejected. Every new outbound-HTTP feature pays the workaround tax. ADR-048 is the existence proof. The cost compounds.
2. **Preemption on Core 1 (single-core with FreeRTOS tasks).** Rejected. Conflicts with the OT-direct scheduler latency model — a preempted scheduler iteration could miss the 1000 ms boiler keepalive even with ADR-068's 200 ms margin. Two cores let us isolate the network-blocking work entirely from the OT-direct path.
3. **Move OT-direct scheduler to its own task, leave SAT/weather/webhook on the loop.** Rejected. OT-direct is the most concurrency-hostile subsystem in the firmware (large state space, latency-coupled to the OpenTherm protocol, file-static buffers everywhere). Moving the *least* network-coupled work first inverts the cost/benefit; the right surgical target is the blocking network calls, not the scheduler.
4. **Use Arduino's `AsyncWebClient` library for non-blocking HTTP.** Rejected. Adds a new dependency; semantics are callback-based, which would still require integration into the cooperative loop. Doesn't help SAT (which isn't network-blocking) and doesn't address the architectural pattern.

### Consequences

**Benefits:**

- Cooperative-loop stalls from HTTP eliminated structurally. No more "main-loop stall cap" comments.
- ADR-048 superseded; webhook code shrinks by ~100 LOC.
- Architectural pattern named — future blocking-network features have a documented home.
- ADR-068 jitter margin becomes optional rather than essential (potential follow-up to return OT status interval toward spec).

**Trade-offs:**

- Three new FreeRTOS tasks (~24 KB total stack on ESP32-S3 — rounding error on 512 KB SRAM).
- New cross-task boundaries via portMUX (4 new muxes for SAT subsystem; 1 for weather).
- Webhook gets a dual implementation between ESP32 (task) and ESP8266 (cooperative state machine); reviewer must enforce parity.
- ADR-090 sub-rule 4 ("no volatile required on cooperative paths") gains more cross-task counter-exemplars; the rule itself is unchanged but the exemplar set grows.

**Risks:**

- HTTPClient may misbehave when called from a non-loop task on Arduino-ESP32 (verify in Phase 1 prototype).
- Pathological busy-loop without yields in a future task could starve peers (reviewer enforcement).
- Stack sizing requires empirical measurement; initial 8 KB may be over or under.

### Related

- **Child ADRs:**
  - SAT child ADR — SAT Controller as Separately-Tasked Subsystem on Core 0
  - Weather child ADR — Weather Fetch as Core 0 Task
  - Webhook child ADR — Webhook Dispatch as Core 0 Task (Supersedes ADR-048)
- **Superseded:** ADR-048
- **Amended:** ADR-055, ADR-057, ADR-070, ADR-085, ADR-090, ADR-111
- **Foundation:** ADR-061 (platform abstraction), ADR-063 (OTGW32 hardware, `HAS_*` flag pattern)
- **Unaffected (intentional):** ADR-016, ADR-064, ADR-068, ADR-087
