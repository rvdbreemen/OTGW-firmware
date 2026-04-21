# Phase 1B: Architecture Findings

## Summary

- Critical: 1 | High: 3 | Medium: 5 | Low: 4

The branch is, on the whole, **architecturally coherent and in keeping with the codebase's documented conventions**. The two new ADRs are serious design documents, not rubber stamps, and the implementation faithfully follows them. The CRITICAL finding is not about code-level coupling or a broken boundary; it is about ADR integrity: both proposed ADRs explicitly name companion ADRs (ADR-077, ADR-078, ADR-080) that do not exist in `docs/adr/`, which undermines the traceability the ADR system is supposed to provide. The remaining findings are refinements, not structural problems.

Note: the scope document describes both new ADRs as "Proposed", but the files on disk carry `Status: Accepted`. Per the project's CLAUDE.md, an Accepted ADR must have been explicitly approved by the user; if that approval is not yet in hand, the status should be reverted to Proposed before merge. I flag this under HIGH rather than CRITICAL because it is a governance/process concern that the author can resolve with a single edit.

## ADR-062 Assessment

ADR-062 (retained discovery verification) is a **strong ADR overall** and one of the better-written ones in the repo. Specific strengths:

- **Context is concrete**, not abstract. It names five real failure modes for Gap A (mosquitto without `persistence true`, broker crash on volatile FS, manual `mosquitto_pub -r -n`, backup/restore, infrastructure migration) and a separate observability Gap B. This is the right level of specificity for an ADR.
- **Tuned-parameter table** with rationale per row (RX buffer size, window duration, heap thresholds) is exemplary. It tells a future reviewer not just *what* 1024 bytes is chosen but *why* 768 was rejected (~5% false-positive rate).
- **Coarseness section** is an honest trade-off declaration: per-topic tracking was considered and rejected with a cost argument (~32 bytes extra bitmap or complex reverse lookup).
- **Orphan non-deletion rationale** makes a real security/safety argument (auto-deleting on a misconfigured nodeId could wipe a neighbour's configs). This is the kind of "what we chose not to do and why" that separates a design decision from a feature description.
- **Preconditions at start** are listed explicitly and match the code 1:1 in `MQTTstuff.ino:217-226`. Implementation fidelity is high.
- **Binding rules** on `mqtt_configuratie.cpp` stream helpers are stated, and the CI-gate expectation is named.

Weaknesses:

- **Missing ADR references**: the "Related" section cites ADR-077, ADR-078, ADR-080, all of which do not exist (highest ADR before this one is ADR-061). See CRITICAL finding below.
- **The enumerated stream-helper list in the binding rule is incomplete**: the ADR binds `streamSensorDiscovery`, `streamBinarySensorDiscovery`, `streamClimateDiscovery`, `streamNumberDiscovery` — but `streamDallasSensorDiscovery` (mqtt_configuratie.cpp:2057) also calls `incPublishedTopicCount()` at line 2176 and is equally load-bearing for the counter. The rule text should enumerate all five, or phrase the rule as "every stream\*Discovery helper".
- **Thread-of-control claim not fully defended**: "Buffer-size restoration is strictly ordered: unsubscribe → setBufferSize(384)" is stated. The code follows this order in `endDiscoveryVerification`, but the `tickDiscoveryVerification` disconnect fast-path resets the buffer **without** calling unsubscribe (defensible since the client is dead), and the ADR would be stronger if it explicitly addressed that branch.
- **First-use accounting**: on a fresh boot, the first daily dispatch will see `dayChanged()==true` (sentinel `-1 != today`), which will attempt `startDiscoveryVerification()`. The ADR's preconditions (NTP sync, uptime>3600, heap threshold) will gate this out, but the interaction with ADR-064's boot-time "all helpers return true on first tick" behaviour deserves a sentence in Consequences.

Overall: Accept with minor edits. The substantive architectural thinking is sound.

## ADR-064 Assessment

ADR-064 (time-boundary single-caller contract) is **also well-written**, arguably tighter than ADR-062 because the scope is smaller.

Strengths:

- **The call-site census table at lines 23-26** is exactly the right evidence to cite: it shows the four helpers, their current call sites, and their downstream consumers. That table *is* the argument for the rule.
- **Silent-failure characterisation** ("The failure mode is silent. No compile error, no runtime assert. The only signal is one of the features stopped working in the field, hours to days after the change landed") is honest and calibrates severity well.
- **Alternatives-considered section** is **substantively better than most in this repo**: three alternatives (per-consumer local-static, multi-subscriber event bus, convert to non-consuming) are each named and rejected with a one-sentence reason. This is textbook ADR form.
- **CI gate is actually implemented** (`evaluate.py:check_time_boundary_single_caller`) and is resilient to comment mentions (`stripped.startswith(("//", "*", "/*"))`) — a detail that shows the author thought about the counter-examples.
- **The rule aligns with existing code patterns**: `DECLARE_TIMER_SEC` + `DUE()` is correctly called out in the guidance table as the right hammer for sub-minute granularity, preserving the consume-on-read rule only for the specific helpers that need it.

Weaknesses:

- **"Exactly one call site" is slightly overstated** given the comment-line skip in the checker. What the checker actually enforces is "exactly one call site on a non-comment line that does not match the `bool X(){` definition regex". That is the right implementation but the rule text says "the entire firmware", which a reader could take to include headers, tests, or documentation strings. Minor wording polish.
- **Boot-time behaviour is not discussed**: on the very first `doTaskMinuteChanged` after boot, all three of `hourChanged/dayChanged/yearChanged` return `true` (`lastX == -1`). This means `runNightlyRestartCheck`, `sendMQTTheapdiag`, and the daily discovery verify all fire within the first minute of boot. Each is gated by its own preconditions (uptime, heap, NTP), so nothing breaks — but the ADR should acknowledge that the dispatcher's first tick is a "fire everything" event, not just the normal sub-minute transitions. This is an observability concern: MQTT heap-diag messages can appear within ~60 seconds of boot, which may confuse a user expecting the first one at the next wall-clock hour.
- **`(void)yearFlag;` anti-pattern in the dispatcher** (OTGW-firmware.ino:324): the comment says "silence unused-warning until a yearly consumer lands". The whole point of the dispatcher is that the consumer list *is* the documentation — capturing an unused flag is fine, but the comment would be cleaner as an empty `if (yearFlag) { /* none yet */ }` block, matching the pattern of the hourly/daily blocks. Low-severity stylistic.
- **Related section lists ADR-080 and `Plan file: C:\Users\rvdbr\.claude\plans\expressive-growing-yao.md`** — the plan file reference is a local path that leaks onto a shared repo. See MEDIUM finding.

Overall: Accept with minor edits.

## Findings

### [CRITICAL] New ADRs reference non-existent companion ADRs (ADR-077, ADR-078, ADR-080)

- **Scope**: `docs/adr/ADR-062-retained-discovery-verification.md`, `docs/adr/ADR-064-time-boundary-single-caller-contract.md`
- **Issue**: Both ADRs cite ADR-080 ("binding ADR rules must have CI gate"), ADR-062 cites ADR-077 ("streaming MQTT HA discovery") and ADR-078 ("MQTT sub-command dispatch tables"), but none of ADR-077, ADR-078, or ADR-080 exist in `docs/adr/`. The highest pre-existing ADR is ADR-061. A `ls docs/adr | grep -E "07[78]|080"` returns zero matches.
- **Architectural impact**: Medium-to-High blast radius. The ADR system is the firmware's primary architectural decision record. When a new ADR says "per ADR-080 meta-rule, these binding rules need a CI-gate entry" and ADR-080 does not exist, three problems follow: (1) the reader cannot verify the claim; (2) future PRs that want to cite ADR-080 themselves will propagate the ghost; (3) the CI-gate claim becomes unverifiable policy — one of the two gates ADR-062 mentions (`check_discovery_counter_instrumented`, `check_publishedtopic_counter_reset`) is in fact **not implemented** in `evaluate.py`, which matches the pattern of a loose obligation rather than an enforced one.
- **Recommendation**: Either (a) write the missing ADRs before merging this branch, (b) rewrite the "Related" sections to cite only ADRs that exist (e.g., refer to ADR-050 "centralized API route dispatch" in place of ADR-078, since that is the actual predecessor), or (c) remove the ADR-NNN citations and replace with direct code references. Option (b) is cheapest and sufficient. Under no circumstance should a branch land with ADRs citing vapour.

### [HIGH] Status field shows "Accepted" but scope document says "Proposed"

- **Scope**: `docs/adr/ADR-062-retained-discovery-verification.md` line 5, `docs/adr/ADR-064-time-boundary-single-caller-contract.md` line 5
- **Issue**: Both ADR files carry `## Status\n\nAccepted` on disk, but the Phase 1B scope document (`00-scope.md` line 10) describes them as `(Proposed)`. Per the project's CLAUDE.md, a Proposed ADR becomes Accepted only after the user explicitly approves — and Accepted ADRs are immutable except for a Supersede status bump.
- **Architectural impact**: Process integrity. If these ADRs have not actually been user-approved, flipping to Accepted bypasses the checkpoint protocol and makes the text immutable before it was meant to be. Given the other findings below (CI gate gap, boot-time dispatch, orphan-count saturation), the text still needs edits — which Accepted status now forbids.
- **Recommendation**: Revert `Status: Accepted` to `Status: Proposed` in both files unless the user explicitly confirms acceptance. Then iterate on the edits from the other findings. Only flip to Accepted when the user signs off.

### [HIGH] ADR-062 binding rule claims two CI gates; only one is implemented — and it's ADR-064's, not ADR-062's

- **Scope**: `evaluate.py`, `docs/adr/ADR-062-retained-discovery-verification.md` lines 110-115
- **Issue**: ADR-062 says: *"Per ADR-080 meta-rule, these binding rules need a CI-gate entry in `evaluate.py`: `check_discovery_counter_instrumented` [and] `check_publishedtopic_counter_reset`. These gates land in TASK-349."* The diff adds a CI check in `evaluate.py`, but that check is `check_time_boundary_single_caller` — the ADR-064 gate. Neither of the two ADR-062 gates is implemented. `grep -n "check_discovery_counter_instrumented\|check_publishedtopic_counter_reset\|incPublishedTopicCount" evaluate.py` returns zero matches.
- **Architectural impact**: The binding rule becomes unenforced. If someone adds a new `streamSomeNewDiscovery` helper in `mqtt_configuratie.cpp` six months from now and forgets `incPublishedTopicCount()`, `state.discovery.iPublishedTopicCount` silently undercounts, causing every verify run to falsely report "missing configs" and triggering a full re-announce of 80 topics every day. This is exactly the kind of slow-bleed behaviour the ADR-062 Consequences section warns against. The worst part is the symptom only shows up on brokers where retained verification is actually triggered, and the debug signal (`iLastMissingCount > 0` every run) is buried in a diagnostic JSON.
- **Recommendation**: Either (a) implement both gates as promised, using a pattern similar to the ADR-064 gate — a simple regex scan that every `bool stream*Discovery(` function in `mqtt_configuratie.cpp` contains exactly one `incPublishedTopicCount()` call after the last `endPublish`, and that `clearMQTTConfigDone` assigns `iPublishedTopicCount = 0`; or (b) downgrade the ADR to remove the gate promise, and document the rule only as a maintenance-review obligation. Option (a) is strictly better given the five existing stream helpers and the likelihood of adding more.

### [HIGH] God-object creep in `MQTTstuff.ino` is meaningful on this branch

- **Scope**: `src/OTGW-firmware/MQTTstuff.ino`
- **Issue**: The file grew by +379 lines on this branch. It now contains: MQTT connect/reconnect, publish helpers, discovery drip (with bitmaps, adaptive timer, pending state), **Status-burst begin/end/cooldown state machine**, **discovery verification state machine**, **retained-config callback filter**, and **heap diagnostics publisher**. Five of those seven concerns touch global static state with subtle ownership rules (`statusBurstActive`, `statusBurstPublishCount`, `burstCooldownUntilMs`, `verifyActive`, `verifyBufferResized`, `verifyPrefixLen`, `verifyNodeLen`, `verifyWildcard[128]`, plus pre-existing `mqttAutoCfgScratch` and the two bitmaps). The re-entrancy contract between `doBackgroundTasks()`, `handleMQTTcallback`, `tickDiscoveryVerification`, and `loopMQTTDiscovery` is now held together by placement comments and author discipline rather than by scope.
- **Architectural impact**: Maintenance risk. A module that is one include from `OTGW-firmware.h` and one callback target for a PubSubClient hosting two independent state machines, plus a third (Status burst) that interacts with both — every future change in here will need the reviewer to hold all three machines in their head. This is the pattern that caused the MQTT auto-config stack overflow (recorded in MEMORY.md under "ADR-040 Bug Fix").
- **Recommendation**: Extract the discovery-verification state machine into a separate translation unit `mqtt_discovery_verify.cpp/.h` (similar to how `mqtt_configuratie.cpp` is separated). The extraction is clean because the state is already file-local statics, the only external symbols are `MQTTclient`, `NodeId`, `settings.mqtt.sHaprefix`, and the `state.discovery` struct — all trivially accessible via `extern` per ADR-044. The same could be done for the Status-burst quiesce block (separate TU, three global functions). This is a non-trivial refactor and could live in a follow-up task; but do not add a *fourth* state machine to this file without extracting first.

### [MEDIUM] `mqtt_configuratie.cpp` binding enumeration is incomplete

- **Scope**: `docs/adr/ADR-062-retained-discovery-verification.md` line 107, `src/OTGW-firmware/mqtt_configuratie.cpp:2057`
- **Issue**: ADR-062 enumerates "`streamSensorDiscovery`, `streamBinarySensorDiscovery`, `streamClimateDiscovery`, `streamNumberDiscovery`" as helpers that MUST call `incPublishedTopicCount()`. But `streamDallasSensorDiscovery` (line 2057) also publishes retained discovery configs and does call `incPublishedTopicCount()` at line 2176. The rule as written silently exempts Dallas sensor discovery from the contract.
- **Architectural impact**: Low-medium. The actual code is correct — all five helpers instrument the counter. But the ADR's enumeration is the canonical rule source, and a future dev reading only the ADR might think Dallas is exempt, remove the call during a refactor, and silently break the counter. If the CI gate from the HIGH finding above is implemented, it will catch this; without it, the ADR text is the only guard.
- **Recommendation**: Change the ADR wording to "every `stream*Discovery` helper in `mqtt_configuratie.cpp`" and add a parenthetical enumeration of the five current helpers. No code change.

### [MEDIUM] Heap-diag JSON buffer can truncate under implausible but possible saturation

- **Scope**: `src/OTGW-firmware/MQTTstuff.ino` `sendMQTTheapdiag` (line ~1068+), `char json[384]`
- **Issue**: The JSON template has 17 fields; at full 10-digit saturation of every `%lu` counter, the rendered string reaches 464 bytes, which snprintf_P will truncate at 383 bytes plus NUL. Realistically, `free_heap` and `max_block` will never exceed 5 digits on ESP8266, and most counters stay small, so the practical size is ~220 bytes. But the architectural principle of static buffer sizing says the buffer should accommodate the worst case the code can produce, not the worst case the author *expects* to occur.
- **Architectural impact**: Low runtime risk (saturated counters would require years of uptime); but the pattern "a static buffer sized for expected output, not maximum output" is a correctness trap this codebase has been burned by (the 1616-byte-stack publishToSourceTopic crash recorded in MEMORY.md). Do not set precedent.
- **Recommendation**: Bump `char json[384]` to `char json[512]` to cover the full worst case with headroom. Alternative: drop the rarely-interesting `_total` counters from the hourly publish and keep only the currently-moving ones (`free_heap`, `max_block`, `frag_pct`, `disc_last_missing`, `disc_last_orphan`). Either is fine; the former is simpler.

### [MEDIUM] Boot-time first-minute dispatch fires all hourly/daily tasks at once

- **Scope**: `src/OTGW-firmware/OTGW-firmware.ino` `doTaskMinuteChanged` lines 294-324
- **Issue**: On the first invocation after boot, `hourChanged()`, `dayChanged()`, and `yearChanged()` all return true because their static `lastX` sentinels are `-1`. This fires: `sendtimecommand` with both daily and yearly flags set (SR=21 and SR=22), `runNightlyRestartCheck` (gated out by `uptime<=3600`), `sendMQTTheapdiag` (fires as soon as MQTT is connected), and `startDiscoveryVerification` (gated by its own preconditions). Only the inner preconditions prevent bizarre behaviour.
- **Architectural impact**: Low functional risk (the inner guards all hold), but it's a latent issue: the ADR-064 "consume-on-read" semantics are explicit, but the boot-time "every helper fires true" behaviour is not documented. If a future hourly or daily consumer is added *without* its own uptime/heap preconditions, it will fire on boot-minute-1 — which in most cases is not what an "hourly" task intends.
- **Recommendation**: Either (a) document the boot-time behaviour in ADR-064's Consequences section, or (b) seed the static `lastX` values on first call by conditioning on a "has this function run before" flag, so boot's first tick consumes the sentinel transition without firing the consumers. Option (a) is simpler and matches the philosophy of "expose the mechanism, let the caller decide"; option (b) is cleaner semantics but changes the consume-on-read contract subtly.

### [MEDIUM] Discovery wildcard subscription crosses an abstraction boundary into PubSubClient internals

- **Scope**: `src/OTGW-firmware/MQTTstuff.ino` `startDiscoveryVerification` — `MQTTclient.setBufferSize(1024)` then `MQTTclient.subscribe(...)` then the raw callback filter in `handleMQTTcallback` (line 625+)
- **Issue**: The verify-window callback filter is layered onto the **same** `handleMQTTcallback` that dispatches command messages. The filter is ordered first via an `if (verifyActive && verifyPrefixLen > 0)` early-return. That works, but it couples two unrelated flows: the command-topic command dispatcher and the discovery-verify counter. A bug in one now has the attention surface of both — e.g., if a future tweak to the verify filter accidentally falls through to the command dispatcher with a retained config topic, the firmware will try to interpret a JSON-embedded discovery config as an OT command.
- **Architectural impact**: Medium. This is the kind of coupling that makes the HIGH "god-object creep" finding worse. When discovery-verify is extracted to its own TU per that recommendation, the callback should be split too: PubSubClient supports only one callback, but the `handleMQTTcallback` can trivially route to `handleDiscoveryVerifyCallback` when `verifyActive`, with a single clean dispatch at the top of the callback.
- **Recommendation**: Extract the verify-window filter block (MQTTstuff.ino:625-657) into a separate function `handleDiscoveryVerifyMessage(topic, length) -> bool` that returns true if the message was consumed, and call it at the top of `handleMQTTcallback`. This is cheap, atomic, and prepares cleanly for the module extraction in the HIGH finding.

### [MEDIUM] Plan-file path leaks local filesystem into repo

- **Scope**: `docs/adr/ADR-062-retained-discovery-verification.md` line 131, `docs/adr/ADR-064-time-boundary-single-caller-contract.md` line 135
- **Issue**: Both ADRs end with `Plan file: C:\Users\rvdbr\.claude\plans\expressive-growing-yao.md`. This is a machine-local path on the author's Windows system that is checked into a public repo.
- **Architectural impact**: None on the code. Low for documentation hygiene: the reference is useless to anyone else and leaks the author's user name.
- **Recommendation**: Remove both lines, or replace with "Planning: internal notes" if the ADR system has no mechanism for linking plans.

### [LOW] REST `/api/v2/discovery` GET endpoint duplicates precondition logic against the start function

- **Scope**: `src/OTGW-firmware/restAPI.ino:498-502`
- **Issue**: The POST `/api/v2/discovery/verify` handler pre-checks `state.mqtt.bConnected`, `ESP.getFreeHeap() < 6000`, `isDiscoveryVerificationActive()`, and `countPendingDiscoveryIds() > 0`, returning custom error codes for each. Then it calls `startDiscoveryVerification()`, which re-runs all the same checks internally.
- **Architectural impact**: Low. The duplication gives the REST API better error granularity (503 vs 409 vs 503 with different messages) than `startDiscoveryVerification()` can express with a single `bool` return. This is acceptable; the cost is two sources of truth for the precondition list, which can drift if `startDiscoveryVerification` gains a new precondition and the REST handler doesn't.
- **Recommendation**: Change `startDiscoveryVerification()` to return an `enum class VerifyStartResult { Ok, NotConnected, LowHeap, AlreadyActive, DripInProgress, Flashing, Truncated, BufferFailed, SubscribeFailed }` and have the REST handler translate each to the appropriate HTTP code. Alternatively, accept the duplication and add a comment on both sites linking them. Not urgent.

### [LOW] `sendMQTTheapdiag` topic does not use `settings.mqtt.sTopTopic` prefix convention

- **Scope**: `src/OTGW-firmware/MQTTstuff.ino` `sendMQTTheapdiag`, `sendMQTTData(F("otgw-firmware/stats/heap"), ...)`
- **Issue**: The topic `otgw-firmware/stats/heap` is hardcoded. Looking at other publishers, `sendMQTTData` probably prepends `sTopTopic` (default "OTGW"), so the actual topic becomes `OTGW/otgw-firmware/stats/heap`. That's consistent with other `otgw-firmware/...` topics in the codebase, but it's double-branded ("OTGW/otgw-firmware/..."). If the user changes `sTopTopic`, the heap diag topic silently changes too.
- **Architectural impact**: Low. Matches existing convention for this codebase.
- **Recommendation**: None strictly required. If cleaning up for the 1.4.1 release, consider making the sub-topic `stats/heap` without the `otgw-firmware/` segment, since the top-level namespace is already configurable.

### [LOW] `DiscoverySection` has 3-byte padding comment but no static_assert

- **Scope**: `src/OTGW-firmware/OTGW-firmware.h:260-269`
- **Issue**: The struct comment says `// 3 bytes padding`. ADR-051 says Hungarian prefixes and sized fields; no static_assert on the layout is a minor gap if the padding matters for anything.
- **Architectural impact**: None. The state struct is not serialised to flash (per ADR-051 `state` is transient), so padding is invisible.
- **Recommendation**: Drop the padding comment. It suggests the padding is load-bearing when it isn't.

### [LOW] `incPublishedTopicCount()` shim justified by ADR-044 but not commented inline in the called-from code

- **Scope**: `src/OTGW-firmware/mqtt_configuratie.cpp:1679`, `src/OTGW-firmware/MQTTstuff.ino:175-178`
- **Issue**: The shim's purpose (bridge between TUs per ADR-044) is commented at the declaration in both files. The declaration comment in `MQTTstuff.ino:175` says "ADR-044 shim". Good. The definition is clear. No code issue; just flagging that this pattern is worth preserving as a template for future cross-TU state access.
- **Architectural impact**: None — positive pattern confirmation.
- **Recommendation**: None.

## Critical issues for Phase 2 context

Security and performance review should specifically revisit:

1. **Discovery verify DoS surface**: the node-segment length cap (`VERIFICATION_MAX_NODE_SEGMENT_LEN = 64`) is present and correct, but a hostile broker could still flood the callback with `<haprefix>/+/<shortNodeId>/#` messages at line rate for 15 seconds. The 1024-byte RX buffer plus lwIP pbufs can cause heap pressure even when each message is parsed cheaply. Phase 2 should quantify: how many inbound retained messages per second can the ESP8266 sustain at heap ~6000 before the `getFreeHeap() < 4500` abort triggers?
2. **REST `/api/v2/discovery/republish` amplification**: a single authenticated POST triggers `markAllMQTTConfigPending`, queuing 80 MQTT publishes over 2 minutes at 2s/each. Phase 2 should verify this cannot be used as an internal DoS (e.g., rapid-fire POSTs while drip is already in progress).
3. **Heap threshold tuning risk**: `HEAP_LOW_THRESHOLD` was lowered 6144→5120 and `HEAP_WARNING_THRESHOLD` 4096→3072. Combined with the new discovery verify window's `MIN_HEAP_START=6000` / `MIN_HEAP_ABORT=4500`, the margins between tiers are now thin. Phase 2 should model the worst-case concurrent-allocation path (Status-burst + WS client + discovery drip + verify window open) and verify no combination drops below `HEAP_CRITICAL_THRESHOLD=1536`.
4. **`isStatusBurstActive()` timeout self-heal at 500ms**: if `endStatusBurst` is never reached due to an exception or early return path, the burst flag auto-clears at 500ms. Phase 2 should audit the actual call paths in `publishMasterStatusState` / `publishSlaveStatusState` (OTGW-Core.ino:1563+) for any early returns after `beginStatusBurst` that bypass `endStatusBurst` — e.g., if a `publishStatusBitMQTT` call ever returns early in a way not anticipated here.
5. **First-minute dispatch of `sendMQTTheapdiag` on boot** (MEDIUM finding above) publishes a retained JSON to `otgw-firmware/stats/heap` within ~60 seconds of boot with near-zero counters. Phase 2 should check this is intentional (it is useful for "boot recovery detection") or if it should wait for the first *real* hour boundary.
