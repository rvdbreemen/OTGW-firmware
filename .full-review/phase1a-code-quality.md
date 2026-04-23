# Phase 1A: Code Quality Findings

Review target: branch `1.4.1` vs `dev`, 14 commits, ~20 source files. Focus: heap-pressure reduction, cumulative heap diagnostics, MQTT discovery verification, time-boundary dispatcher refactor.

## Summary

- Critical: 0
- High: 4
- Medium: 9
- Low: 7
- Dead-code candidates: 14

The branch is in solid shape: no crash-class issues found, the platform constraints are respected, and the refactor goals (ADR-064 single-caller, retained discovery verification) are mostly well-executed. The main risks are in (a) incomplete coverage of the Status-burst quiesce for VH (ventilation) publishers, (b) NTP/time helpers generating unnecessary allocations per minute, and (c) a handful of redundant state fields and stale comments introduced by the refactor.

## Findings

### [HIGH] VH (ventilation) status publishers bypass the Status-burst quiesce

- **File:line**: `src/OTGW-firmware/OTGW-Core.ino:1688-1697` (`publishMasterStatusVHState`) and `1721-1732` (`publishSlaveStatusVHState`)
- **Issue**: The whole point of `beginStatusBurst`/`endStatusBurst`/`incrementStatusBurstPublishCount` (TASK-342 + TASK-347) is to suppress the MQTT discovery drip during any 20ms Status-frame fanout. The non-VH master/slave paths (lines 1568/1581, 1609/1623) correctly wrap themselves. The VH variants do not:

  ```cpp
  OTcurrentSystemState.MasterStatusVH = valueHB;
  mqttForceNextMasterStatusVHPublish = false;
  {
    OTPublishGate gate(publishCombined);
    sendMQTTData(F("status_vh_master"), statusText);    // no beginStatusBurst()
  }
  publishStatusVHBitMQTT(0, "vh_ventilation_enabled", ...);
  // ... 3 more bits
  // no endStatusBurst()
  ```

  `publishStatusVHBitMQTT` at line 1500 is also missing the `incrementStatusBurstPublishCount()` call that its non-VH sibling got at line 1496.
- **Why it matters**: Boilers with a ventilation/heat recovery unit produce the same 9-publish fanout on every Status-VH frame. Under the stated ~3s Status cadence, this is exactly the scenario the cooldown was designed to prevent — yet VH boilers are not covered. Users with this hardware keep the pre-TASK-342 drip collisions.
- **Fix**: Mirror the non-VH pattern. Wrap the VH send + bit publishes in `beginStatusBurst()`/`endStatusBurst()`, and add the `incrementStatusBurstPublishCount()` call in `publishStatusVHBitMQTT`:

  ```cpp
  // publishMasterStatusVHState
  beginStatusBurst();
  {
    OTPublishGate gate(publishCombined);
    if (publishCombined) incrementStatusBurstPublishCount();
    sendMQTTData(F("status_vh_master"), statusText);
  }
  publishStatusVHBitMQTT(0, ...);
  // ... (unchanged)
  endStatusBurst();

  // publishStatusVHBitMQTT (line 1500)
  void publishStatusVHBitMQTT(...) {
    const bool allowPublish = shouldPublishStatusVHBit(...);
    logMQTTStatusBitDecision(...);
    OTPublishGate gate(allowPublish);
    if (allowPublish) incrementStatusBurstPublishCount();   // ADD
    publishMQTTOnOff(topic, newVal);
  }
  ```

### [HIGH] Comment in `doTaskMinuteChanged` falsely claims NTP and uptime preconditions are enforced by `startDiscoveryVerification`

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:316-319`
- **Issue**:

  ```cpp
  // Daily MQTT discovery verification. Opt-in via settings.mqtt.bDiscoveryAutoVerify
  // (default true). Preconditions (NTP sync, uptime>3600, heap>=6000, no pending
  // drip, MQTT connected) are enforced inside startDiscoveryVerification(), so
  // this call is unconditional here and startup-safe.
  if (settings.mqtt.bDiscoveryAutoVerify) startDiscoveryVerification();
  ```

  `startDiscoveryVerification()` (MQTTstuff.ino:212-261) checks `verifyActive`, `state.mqtt.bConnected`, `isFlashing()`, pending IDs, `getFreeHeap() < 6000`, and `getMaxFreeBlockSize()`. It does **not** check NTP sync nor uptime>3600. The comment lies.
- **Why it matters**: On first boot, `dayChanged()` returns `true` the first time it is called (`lastday = -1`). Combined with a quickly-completed drip and an MQTT reconnect, a verify window can start within the first few minutes of boot. The pending-IDs guard usually saves us, but if the drip finishes before the first `minuteChanged()` tick, a startup verify runs with `iPublishedTopicCount` that may not yet include late-arriving ADR-040 per-source topics. Also, the comment is a trap for the next maintainer: they may "rely" on preconditions that do not exist.
- **Fix**: Either add the missing guards in `startDiscoveryVerification()` (preferred — the comment already reflects the intended contract) or correct the comment. The first option:

  ```cpp
  bool startDiscoveryVerification() {
    if (verifyActive) return false;
    if (!state.mqtt.bConnected) return false;
    if (isFlashing()) return false;
    if (state.uptime.iSeconds < 3600) return false;                     // ADD
    if (!isNTPtimeSet()) return false;                                  // ADD
    if (countPendingDiscoveryIds() > 0) return false;
    ...
  }
  ```

### [HIGH] REST `/verify` endpoint hard-codes heap threshold instead of referencing the constant

- **File:line**: `src/OTGW-firmware/restAPI.ino:499`
- **Issue**:

  ```cpp
  if (ESP.getFreeHeap() < 6000) { sendApiError(503, F("Heap too low for verify")); return; }
  ```

  The same value lives as `VERIFICATION_MIN_HEAP_START = 6000` at MQTTstuff.ino:192 and as the comment's documented value in `startDiscoveryVerification()`. The REST handler also duplicates the other guards (`isDiscoveryVerificationActive`, `countPendingDiscoveryIds`) which are already in `startDiscoveryVerification()`.
- **Why it matters**: Two sources of truth. If `VERIFICATION_MIN_HEAP_START` is tuned to 7000 after field testing, the REST endpoint silently allows starts at 6500 that would later be refused by `startDiscoveryVerification` (returning the generic "refused (see telnet log)" 503 instead of the specific "Heap too low for verify" 503). That's a diagnostic downgrade.
- **Fix**: Expose the constant and reference it. If you want nicer error messages than the boolean return, expose a small enum/reason code instead:

  ```cpp
  // MQTTstuff.ino (already a constexpr, just needs to be visible)
  // Either move to MQTTstuff.h, or add an extern-declared accessor.
  extern uint32_t getVerificationMinHeapStart();

  // restAPI.ino
  if (ESP.getFreeHeap() < getVerificationMinHeapStart()) {
    sendApiError(503, F("Heap too low for verify")); return;
  }
  ```

  Ideal: drop the REST precondition checks entirely, call `startDiscoveryVerification()`, and map the `false` return to a generic 503 — the duplication is the real smell here.

### [HIGH] Stale comment on hourly heap-diag publish path

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:1071-1074`
- **Issue**:

  ```cpp
  /*
  Publish cumulative heap-pressure and drop diagnostics as a single retained JSON
  blob to otgw-firmware/stats/heap. Called from the hourly tick (doTaskEvery60s
  gated by hourChanged) — NOT piggybacked on the 5-minute loop to keep traffic low.
  */
  ```

  After ADR-064 / TASK-350, `sendMQTTheapdiag()` is called from `doTaskMinuteChanged` under the `if (hourFlag)` block, NOT from `doTaskEvery60s`. The comment is wrong.
- **Why it matters**: Comments like this are how the next bug investigation starts. If someone chases an unexpected publish cadence, following this comment leads them to the wrong call site. Same pattern as the RAM/flash domain mismatch rule: stale documentation beats missing documentation to the bottom.
- **Fix**:

  ```cpp
  /*
  Publish cumulative heap-pressure and drop diagnostics as a single retained JSON
  blob to otgw-firmware/stats/heap. Called once per wall-clock hour from
  doTaskMinuteChanged() under the hourChanged() flag (ADR-064) — NOT piggybacked
  on the 5-minute loop to keep traffic low.
  */
  ```

### [MEDIUM] `getHeapHealth()` tier-transition counters can inflate on boundary chatter

- **File:line**: `src/OTGW-firmware/helperStuff.ino:722-758`
- **Issue**: `getHeapHealth()` is called many times per loop iteration by `canSendWebSocket()` and `canPublishMQTT()`. The new transition counters only increment "into a stricter tier" (`level > lastLevel`), which is correct for upward transitions — but the `lastLevel` static is global across all callers. If heap oscillates around a threshold (say around `HEAP_LOW_THRESHOLD = 5120`), every crossing from HEALTHY to LOW increments `iEnteredLowCount`. Recovery to HEALTHY resets `lastLevel` but is not counted. So under heap chatter the counter grows rapidly even though there is no sustained pressure event.
- **Why it matters**: Telemetry is meant to answer "how often does this boiler enter pressure zones" — not "how often does the free-heap integer wobble across a constant". The published counters will be interpreted as severity indicators, and field reports will look more alarming than reality.
- **Fix**: Add hysteresis: only count a transition if it's been stable for N ms / N calls. Minimum viable:

  ```cpp
  static HeapHealthLevel lastLevel = HEAP_HEALTHY;
  static uint32_t lastTransitionMs = 0;
  // ... compute level ...
  const uint32_t now = millis();
  if (level != lastLevel && level > lastLevel && (now - lastTransitionMs) > 1000) {
    // count transition ...
    lastTransitionMs = now;
  }
  lastLevel = level;
  ```

  Or simpler: debounce at 500ms, same idea. Either works — the goal is to avoid counting the same pressure event multiple times.

### [MEDIUM] `sendMQTTheapdiag` sets `iLastPublishedEpoch` before the publish

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:1079-1108`
- **Issue**:

  ```cpp
  void sendMQTTheapdiag(){
    if (!settings.mqtt.bEnable) return;
    if (!state.mqtt.bConnected) return;
    state.heapdiag.iLastPublishedEpoch = (uint32_t)time(nullptr);   // set BEFORE publish
    char json[384];
    ...
    sendMQTTData(F("otgw-firmware/stats/heap"), json, true);
  }
  ```

  `sendMQTTData()` returns `void` and can drop the message silently (heap guard, `canPublishMQTT()` returning false). The epoch is recorded even if the payload never left the board.
- **Why it matters**: Cosmetic but confusing when correlating telemetry. "Last publish at 14:00" on the UI, but broker never saw it — debugging becomes harder. Combined with the fact that `iLastPublishedEpoch` is also currently dead on the consumer side (see Dead Code section), this is double noise.
- **Fix**: Either (1) change `sendMQTTData` to return a bool and set the epoch only on success, or (2) check preconditions inline:

  ```cpp
  void sendMQTTheapdiag(){
    if (!settings.mqtt.bEnable) return;
    if (!state.mqtt.bConnected) return;
    if (!canPublishMQTT()) return;            // match the gate sendMQTTData uses
    char json[384];
    ...
    sendMQTTData(F("otgw-firmware/stats/heap"), json, true);
    state.heapdiag.iLastPublishedEpoch = (uint32_t)time(nullptr);  // AFTER
  }
  ```

### [MEDIUM] `MQTT_DISCOVERY_HEAP_MIN` comment claims alignment but differs from `HEAP_WARNING_THRESHOLD`

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:45-50` vs `src/OTGW-firmware/helperStuff.ino:695`
- **Issue**:

  ```cpp
  // MQTTstuff.ino
  // Value aligned with HEAP_WARNING_THRESHOLD (3072) in canPublishMQTT()
  constexpr uint32_t MQTT_DISCOVERY_HEAP_MIN = 3000;

  // helperStuff.ino
  #define HEAP_WARNING_THRESHOLD    3072
  ```

  3000 ≠ 3072. The comment says "aligned with" but they are 72 bytes apart. Small, but the 72-byte band is exactly where the drip can start a publish that `canPublishMQTT()`'s WARNING-tier throttle will then fight against (it's a throttle, not a block).
- **Why it matters**: The two values should either be equal (and share a symbol) or the comment should describe the intentional offset. Right now a reader assumes they match and is confused when grep tells them otherwise.
- **Fix**: Either use the same symbol (preferred) or document the offset:

  ```cpp
  // helperStuff.ino is .ino so it compiles into the same TU; #define is visible here.
  constexpr uint32_t MQTT_DISCOVERY_HEAP_MIN = HEAP_WARNING_THRESHOLD;
  ```

  Or if there's a real reason to be below the threshold, state it: "set 72 bytes below WARNING so the drip's own allocation doesn't push the next caller past the threshold".

### [MEDIUM] Four `ZonedDateTime` allocations per minute in the time-boundary dispatcher

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:294-325`, `helperStuff.ino:467-515`
- **Issue**: Each helper (`minuteChanged`, `hourChanged`, `dayChanged`, `yearChanged`) independently constructs a `TimeZone` and a `ZonedDateTime`:

  ```cpp
  bool hourChanged(){
    static int8_t lasthour = -1;
    TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
    ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
    ...
  }
  ```

  Per minute the dispatcher does this 4 times (once from the loop's `minuteChanged()`, then 3 times inside `doTaskMinuteChanged`), and `sendtimecommand()` does it a 5th time. `createForZoneName` does string-based zone-db lookup; `ZonedDateTime::forUnixSeconds64` walks transition rules. Not free.
- **Why it matters**: Not a crash risk, but the refactor promised to consolidate calls. In practice it just moved them. On ESP8266 the cost of `createForZoneName` traversal is measurable (ADR-064 mentions the motivation was alignment, not CPU — but the CPU improvement was on the table and was not taken).
- **Fix**: Compute the `ZonedDateTime` once in `doTaskMinuteChanged`, pass its `.hour()/.day()/.year()` into the helpers (or change the helpers to accept the pre-computed values). Something like:

  ```cpp
  void doTaskMinuteChanged() {
    if (!isNTPtimeSet()) {
      // Still need to set lastXXX=-1 once time syncs, so fall back to the old path
      ...
      return;
    }
    TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
    ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);

    const bool hourFlag = hourChangedAt(myTime.hour());
    const bool dayFlag  = dayChangedAt(myTime.day());
    const bool yearFlag = yearChangedAt(myTime.year());
    sendtimecommand(myTime, dayFlag, yearFlag);
    ...
  }
  ```

  This is a refactor, not a one-liner — consider a follow-up task.

### [MEDIUM] Verify window heap-abort masks a real failure

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:313-319`
- **Issue**:

  ```cpp
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_ABORT) {
    verifyReceivedCount = expected;   // suppress false-missing republish
    DebugTln(F("[verify] heap-abort: closing window early"));
    endDiscoveryVerification();
    return;
  }
  ```

  Setting `verifyReceivedCount = expected` to avoid triggering a republish is pragmatic, but it falsifies the telemetry: `iLastMissingCount` will be 0 despite the fact that the run was aborted inconclusively. The UI will show "last verify: clean" when in fact the verify never completed.
- **Why it matters**: Telemetry should distinguish "clean pass" from "could not assess". A future field bug report saying "discovery verify says all OK but broker is missing topics" will be traced back to this line. Also, `iLastVerifyEpoch` is set in `endDiscoveryVerification` regardless, so the UI has no way to know this was a heap-abort.
- **Fix**: Introduce a status enum or a flag and publish it separately:

  ```cpp
  // OTGW-firmware.h, DiscoverySection
  enum class VerifyOutcome : uint8_t { UNKNOWN, CLEAN, MISSING, ABORTED_HEAP, ABORTED_DISCONNECT };
  VerifyOutcome eLastOutcome = VerifyOutcome::UNKNOWN;

  // MQTTstuff.ino
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_ABORT) {
    state.discovery.eLastOutcome = VerifyOutcome::ABORTED_HEAP;
    verifyActive = false;
    ...
    return;
  }
  ```

  Do not reuse `verifyReceivedCount` as a signal vehicle.

### [MEDIUM] `handleDiscovery` status endpoint has 9 variable-arg `snprintf_P` with a 320-byte stack buffer

- **File:line**: `src/OTGW-firmware/restAPI.ino:472-493`
- **Issue**: The GET status handler writes 9 values into `char msg[320]`. The worst-case length is hard to prove by inspection — `%lu` can produce up to 10 digits, plus punctuation. On my rough count the max serialized length is around 240 bytes, so 320 is safe but the margin is thin and there is no truncation check. `snprintf_P` returns a meaningful value if truncated.
- **Why it matters**: A future field (e.g. a new counter) added to this endpoint could silently exceed 320 and HTTP serves partial JSON — HA UI chokes on malformed JSON.
- **Fix**: Either size the buffer with a `static_assert`-style computation, or check the return:

  ```cpp
  char msg[320];
  int wrote = snprintf_P(msg, sizeof(msg), PSTR(...), ...);
  if (wrote <= 0 || (size_t)wrote >= sizeof(msg)) {
    sendApiError(500, F("Response truncated")); return;
  }
  ```

### [MEDIUM] `DebugTln(F("[verify] ..."))` logs are not runtime-gated

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:229, 240, 246, 258, 284, 288, 316`
- **Issue**: The discovery-drip logs use `MQTTDebugTf` (gated by `state.debug.bMQTT`). The new verify logs use raw `DebugTf`/`DebugTln`, so they print to telnet whether MQTT debug is enabled or not.
- **Why it matters**: Not critical — at most one verify per day or per manual trigger, so log volume is bounded. But it's inconsistent with the rest of the file's convention and hurts telnet-log readability when MQTT debug is off (users see verify noise without context).
- **Fix**: Use `MQTTDebugTf`/`MQTTDebugTln` for the same reasons the drip does. Keep the `refused` log on ERROR level (it's actionable) but gate the informational ones:

  ```cpp
  MQTTDebugTf(PSTR("[verify] started: wildcard=%s expected=%lu\r\n"), ...);
  MQTTDebugTf(PSTR("[verify] done: expected=%u received=%u orphans=%u missing=%u\r\n"), ...);
  DebugTln(F("[verify] missing configs detected, triggering markAllMQTTConfigPending"));  // keep
  ```

### [MEDIUM] Redundant `isStatusBurstActive()` check in `loopMQTTDiscovery`

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:1326-1333`
- **Issue**:

  ```cpp
  if (isStatusBurstActive()) {
    state.heapdiag.iDripActiveBurstSkipCount++;
    return;
  }
  if (isDripDeferred()) {              // also checks isStatusBurstActive() internally
    state.heapdiag.iDripCooldownSkipCount++;
    return;
  }
  ```

  `isDripDeferred()` is:

  ```cpp
  bool isDripDeferred() {
    if (isStatusBurstActive()) return true;      // dead path — already handled above
    if (burstCooldownUntilMs != 0 && (long)(millis() - burstCooldownUntilMs) < 0) return true;
    return false;
  }
  ```

  The caller has already returned from the burst-active case, so the first line of `isDripDeferred` never fires in this path. But `isDripDeferred` is also the public API — the header exposes it. So the internal check is defensive in general, but in `loopMQTTDiscovery` specifically, it's a double-test.
- **Why it matters**: Minor. The real problem is that the counters rely on call-order coincidence: if someone reorders the checks, the diagnostic split between "burst" and "cooldown" silently wrong-assigns all skips to cooldown.
- **Fix**: Collapse into one decision with an explicit reason:

  ```cpp
  enum class DripSkipReason { NONE, BURST, COOLDOWN };
  DripSkipReason why = dripSkipReason();   // { if active, BURST; else if cooldown-open, COOLDOWN; else NONE }
  if (why == DripSkipReason::BURST)    { state.heapdiag.iDripActiveBurstSkipCount++; return; }
  if (why == DripSkipReason::COOLDOWN) { state.heapdiag.iDripCooldownSkipCount++; return; }
  ```

  Or at minimum, inline the cooldown test directly in `loopMQTTDiscovery` and kill the public `isDripDeferred()` (I haven't found any other caller — see dead-code section).

### [MEDIUM] `runNightlyRestartCheck` loses the `minute() == 0` guard

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:269-282`
- **Issue**: The original code was:

  ```cpp
  if (myTime.hour() == settings.iRestartHour && myTime.minute() == 0) { ... ESP.restart(); }
  ```

  The refactor removed the `minute() == 0` guard, relying on `hourChanged()` to fire only once per hour. This is *probably* correct: `hourChanged()` consumes-on-read, and the dispatcher is the sole caller. But:
  - On first boot, `hourChanged()` returns `true` regardless of actual minute. The `uptime.iSeconds > 3600` guard prevents restart, so safe.
  - If NTP syncs between xx:30 and xx:59 and `hourChanged()` happens to fire then, the restart could trigger mid-hour. The `uptime.iSeconds > 3600` guard still protects, but only after first hour of operation.
  - After >1 hour uptime, if an NTP leap or clock jump causes `hourChanged()` to fire again within the same hour, the restart fires at an unexpected time.
- **Why it matters**: The change in behavior is subtle and is not documented. If a user configures `iRestartHour = 3`, they expect the box to restart at 03:00, not at 03:47.
- **Fix**: Preserve the minute guard — it's defense in depth for NTP anomalies:

  ```cpp
  static void runNightlyRestartCheck() {
    if (!settings.bNightlyRestart) return;
    if (!settings.ntp.bEnable) return;
    if (state.uptime.iSeconds <= 3600) return;
    const int64_t now_sec = time(nullptr);
    if (now_sec <= 946684800) return;
    TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
    ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now_sec, myTz);
    if (myTime.hour() != settings.iRestartHour) return;
    if (myTime.minute() > 5) return;               // ADD — loose guard for NTP jitter
    DebugTf(PSTR("Nightly restart triggered at %02d:%02d (uptime=%lu s)\r\n"),
            myTime.hour(), myTime.minute(), (unsigned long)state.uptime.iSeconds);
    delay(200);
    ESP.restart();
  }
  ```

### [LOW] Stale comment in main loop about drip interval

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:409`
- **Issue**:

  ```cpp
  loopMQTTDiscovery();              // async MQTT discovery drip (self-timed, 3s interval)
  ```

  The interval was changed to 2s normal / 10s slow (MQTTstuff.ino:1297-1298). Comment says 3s.
- **Fix**:

  ```cpp
  loopMQTTDiscovery();              // async MQTT discovery drip (self-timed, 2s normal / 10s slow)
  ```

### [LOW] `MQTT_DISCOVERY_HEAP_MIN` comment block still references "ADR-077"

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:46`
- **Issue**: "Streaming HA discovery (ADR-077)" — there is no ADR-077 in the branch or `dev`. The `docs/adr/` listings include ADR-062 and ADR-064 as newly proposed; the highest existing ADR before this branch is ADR-051. A search confirms no ADR-077 exists.

  ```bash
  $ ls docs/adr | grep -E 'ADR-0[67]'
  ADR-062-retained-discovery-verification.md
  ADR-064-time-boundary-single-caller-contract.md
  ```
- **Fix**: Either create ADR-077 (probably intended to document the streaming HA discovery migration from the earlier ADR-040 / ADR-044 work), or correct the reference:

  ```cpp
  // Streaming HA discovery (ADR-044 buffer design + ADR-040 per-source topics)
  // only needs ~200 bytes per chunk...
  ```

### [LOW] `(void)yearFlag` is a workaround, not a design

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:324`
- **Issue**:

  ```cpp
  // Yearly consumers: none currently beyond sendtimecommand's SR=22 which is
  // driven by yearFlag above.
  (void)yearFlag;  // silence unused-warning until a yearly consumer lands
  ```

  `yearFlag` is already passed to `sendtimecommand(dayFlag, yearFlag)` on line 304 — so it IS used. The `(void)yearFlag` is therefore dead code (not actually silencing any warning, since the variable IS read).
- **Fix**: Remove the `(void)yearFlag;` line and the accompanying comment, OR — if the intent is "reserve this slot for future yearly-only consumers" — use a more honest marker:

  ```cpp
  // Yearly consumers: SR=22 is sent inside sendtimecommand() when yearFlag is set.
  // No yearly-only work currently. Add a `if (yearFlag) { ... }` block here if needed.
  ```

### [LOW] Status-burst cooldown could overlap next burst (acknowledged by comment, but not mitigated)

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:94-99`
- **Issue**: The comment warns:

  ```
  CAUTION: at 10 seconds, the cooldown can overlap consecutive Status-frames
  (Crashevans log shows ~3s cadence). That means under heavy Status traffic
  the drip can stall. Tunable via STATUS_BURST_COOLDOWN_MS.
  ```

  A 10s cooldown vs. ~3s burst cadence means the drip can be permanently deferred under heavy OT traffic. The author documented the trade-off but shipped 10000ms. At a typical 140-ID discovery count × 10s = 23 min until the drip can run uninterrupted — realistically it can't, so `iDripCooldownSkipCount` will grow steadily without discovery progressing.
- **Why it matters**: The comment tells the future maintainer to lower the value if they see the counter grow. But the field is exactly where this will be observed, and no automatic backoff exists.
- **Fix**: Add an automatic fallback: if `iDripCooldownSkipCount` outpaces drip progress for N minutes, shorten the cooldown or bypass it:

  ```cpp
  // Adaptive cooldown: if we've been skipping for too long without progress,
  // shorten the effective cooldown.
  if ((long)(now - lastDripProgressMs) > 60000) {
    burstCooldownUntilMs = 0;  // bypass this round
  }
  ```

  Or — simpler — ship with 3000-5000ms default and only go higher when field evidence says so.

### [LOW] `copyMQTTPayloadToBuffer` / verify filter: no validation that topic is NUL-terminated

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:629-656`
- **Issue**: PubSubClient's callback passes `topic` as `char*`. The library contract is that it's NUL-terminated, but under heap exhaustion PubSubClient's RX buffer handling has been known to truncate. The verify filter calls `strncmp`/`strchr` on `topic` without a length bound. For retained-discovery wildcards this is fine in practice; under malicious or broken broker behaviour it's a weak-but-real concern.
- **Fix**: Belt-and-braces: cap the topic length before strchr walking:

  ```cpp
  // Bound topic scan to the PubSubClient RX buffer (1024 during verify, 384 otherwise).
  const size_t topicMax = 200;  // matches MQTT_TOPIC_MAX_LEN
  for (size_t i = 0; i < topicMax; i++) {
    if (topic[i] == '\0') { /* ok */ break; }
    if (i + 1 == topicMax) { verifyOrphanCount++; return; }  // too long
  }
  // ... existing strncmp logic ...
  ```

### [LOW] `MQTT_DISCOVERY_HEAP_MIN` is no longer a gate below the drip's own logic

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:1322`
- **Issue**:

  ```cpp
  if (ESP.getFreeHeap() < MQTT_DISCOVERY_HEAP_MIN) return;
  ```

  With `MQTT_DISCOVERY_HEAP_MIN = 3000` and `HEAP_WARNING_THRESHOLD = 3072`, this gate fires only 72 bytes below the warning threshold. The adaptive interval (heap-pressure detection at line 1308) already kicks in at `HEAP_LOW = 5120` — so by the time the drip reaches this heap check, the timer is already on 10s slow-mode. In effect, the heap-min gate is rarely hit. Not a bug, but noise: two heap checks with near-identical semantics.
- **Fix**: Either remove the direct heap-min check and rely on `getHeapHealth()` / `isDripDeferred()` for all throttling, or promote it to a meaningful threshold (e.g. `HEAP_CRITICAL_THRESHOLD`).

## Dead Code & Cleanup Candidates

Cross-checked each candidate with `grep -rn` across the `src/` tree. "Introduced" = added by this branch's diff. "Pre-existed" = not touched by the diff but orphaned by the refactor.

### [HIGH/introduced] `state.discovery.bVerificationActive` is write-only

- **File:line**: `src/OTGW-firmware/OTGW-firmware.h:267`, written at `MQTTstuff.ino:256, 267, 307`
- **Snippet**:

  ```cpp
  // header
  bool     bVerificationActive      = false; // active verify window indicator (observable via REST)
  ```

  Grep confirms zero reads. The REST `/api/v2/discovery` endpoint (`restAPI.ino:481`) reads `isDiscoveryVerificationActive()`, which returns the static `verifyActive` (MQTTstuff.ino:331), not the state field.
- **Cleanup**: Either drop the state field (the static-local is the single source of truth), or switch `isDiscoveryVerificationActive()` to read the state field (but that's a layering regression — the state field would become a mirror of the static). Simpler path:

  ```cpp
  // OTGW-firmware.h: remove the field
  struct DiscoverySection {
    uint32_t iLastVerifyEpoch = 0;
    ...
    // bVerificationActive removed — use isDiscoveryVerificationActive()
  };

  // MQTTstuff.ino: remove the three writes at 256, 267, 307
  ```

### [HIGH/introduced] `state.heapdiag.iLastPublishedEpoch` is write-only

- **File:line**: `src/OTGW-firmware/OTGW-firmware.h:280`, written at `MQTTstuff.ino:1082`
- **Snippet**:

  ```cpp
  uint32_t iLastPublishedEpoch      = 0; // unix-epoch of last sendMQTTheapdiag publish
  ```

  No reader — not in devinfoV2 (restAPI.ino:880+), not in the heap-diag topic (it's the timestamp of its own publish — circular), not in index.js. Pure bookkeeping without a consumer.
- **Cleanup**:

  ```cpp
  // OTGW-firmware.h: drop the field.
  // MQTTstuff.ino:1082: drop the assignment.
  ```

  Or, if the intent is to expose "when did we last publish diag", add it to `sendDeviceInfoV2`:

  ```cpp
  sendJsonMapEntry(F("hd_last_published_epoch"), state.heapdiag.iLastPublishedEpoch);
  ```

  Pick one. Right now it's noise.

### [HIGH/introduced] `endDiscoveryVerification` is in the public header but only called internally

- **File:line**: `src/OTGW-firmware/OTGW-firmware.h:132`, definition at `MQTTstuff.ino:264`
- **Snippet**:

  ```cpp
  // header
  void     endDiscoveryVerification();
  ```

  All 3 callers are inside MQTTstuff.ino (`tickDiscoveryVerification`). No external translation unit calls it.
- **Cleanup**: Demote to `static` inside the .ino file:

  ```cpp
  // MQTTstuff.ino (remove from header, add static)
  static void endDiscoveryVerification() { ... }
  ```

  Same treatment for `tickDiscoveryVerification` (only used in `handleMQTT`, same TU).

### [MEDIUM/introduced] `isDripDeferred()` is public but only called once in its own TU

- **File:line**: `src/OTGW-firmware/OTGW-firmware.h:117`, call at `MQTTstuff.ino:1330`
- **Snippet**: The header exposes it to other TUs. Grep shows a single caller (loopMQTTDiscovery in the same file).
- **Cleanup**: Make `static`. Combined with the "Redundant isStatusBurstActive() check in loopMQTTDiscovery" finding above, consider inlining entirely.

### [MEDIUM/introduced] `(void)yearFlag;` after `sendtimecommand(dayFlag, yearFlag)`

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:324`
- **Snippet**:

  ```cpp
  // Yearly consumers: none currently beyond sendtimecommand's SR=22 which is
  // driven by yearFlag above.
  (void)yearFlag;  // silence unused-warning until a yearly consumer lands
  ```

  `yearFlag` was already consumed two lines earlier — the `(void)` cast is a no-op.
- **Cleanup**: Delete the line.

### [MEDIUM/introduced] Stale comment on `sendMQTTheapdiag` location

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:1073-1074`
- **Snippet**: "Called from the hourly tick (doTaskEvery60s gated by hourChanged)" — actually called from `doTaskMinuteChanged` under `if (hourFlag)`. See HIGH finding above.
- **Cleanup**: Fix the comment (patch in HIGH finding section).

### [MEDIUM/introduced] `ADR-077` reference does not exist

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:46`
- **Snippet**: "Streaming HA discovery (ADR-077) only needs ~200 bytes per chunk"
- **Cleanup**: Either create the ADR or correct the reference. See LOW finding above.

### [MEDIUM/pre-existed] `setMQTTConfigPending` is file-scope but PROGMEM bitmap covers 256 IDs — half the namespace is dead

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:1255-1260`
- **Snippet**:

  ```cpp
  void setMQTTConfigPending(const uint8_t MSGid)
  {
    uint8_t group = (MSGid >> 5) & 0x07;
    uint8_t bit   = MSGid & 0x1F;
    bitSet(MQTTautoCfgPendingMap[group], bit);
  }
  ```

  This is pre-existing and out of scope for this branch, but `markAllMQTTConfigPending` at line 1269 iterates `for (uint16_t i = 0; i < 256; i++)` and only sets bits where `sIdx != MQTT_HA_INDEX_NONE || bIdx != MQTT_HA_INDEX_NONE`. In practice, only ~50 of the 256 OT IDs have discovery definitions. The other 206 iterations are pure overhead. Not introduced here, but this branch adds more paths that iterate the full bitmap (countPendingDiscoveryIds: MQTTstuff.ino:202-208 iterates all 256 bits always). Worth flagging as a latent tech-debt item.
- **Cleanup**: Cache the count of bits set (O(1) instead of O(256)) as a side-effect of setMQTTConfigPending / bitClear in the drip loop. Out of scope for 1.4.1 but this branch multiplied its call sites — add to follow-up backlog.

### [MEDIUM/introduced] Redundant heap precondition duplication in REST handler

- **File:line**: `src/OTGW-firmware/restAPI.ino:498-502`
- **Snippet**:

  ```cpp
  if (!state.mqtt.bConnected) { sendApiError(503, F("MQTT not connected")); return; }
  if (ESP.getFreeHeap() < 6000) { sendApiError(503, F("Heap too low for verify")); return; }
  if (isDiscoveryVerificationActive()) { sendApiError(409, F("Verification already active")); return; }
  if (countPendingDiscoveryIds() > 0) { sendApiError(409, F("Discovery drip in progress")); return; }
  if (!startDiscoveryVerification()) { sendApiError(503, F("Verification start refused (see telnet log)")); return; }
  ```

  Each of those conditions is re-checked inside `startDiscoveryVerification()`. The only reason to duplicate them is to return nicer error messages. That's a valid UX reason, but it creates two sources of truth (see HIGH finding on hard-coded 6000).
- **Cleanup**: Replace all pre-checks with a single call and return codes:

  ```cpp
  switch (startDiscoveryVerificationWithReason()) {
    case VerifyStartOk:           break;
    case VerifyStartMqttDown:     sendApiError(503, F("MQTT not connected")); return;
    case VerifyStartHeapLow:      sendApiError(503, F("Heap too low for verify")); return;
    case VerifyStartAlreadyActive:sendApiError(409, F("Verification already active")); return;
    case VerifyStartDripBusy:     sendApiError(409, F("Discovery drip in progress")); return;
    case VerifyStartFlashBusy:    sendApiError(503, F("Flash operation in progress")); return;
    case VerifyStartFragmented:   sendApiError(503, F("Heap fragmented")); return;
    case VerifyStartSubscribeFailed: sendApiError(503, F("Subscribe failed")); return;
  }
  ```

### [LOW/pre-existed] `mqttAutoConfigInProgress` + `MQTTAutoConfigSessionLock` duplicate coverage

- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:60-76`
- **Snippet**: The raw bool `mqttAutoConfigInProgress` is tested, the `MQTTAutoConfigSessionLock` RAII wrapper sets/clears it. Not introduced here but this branch doesn't clean it up either. Two symbols doing one job.
- **Cleanup**: Move the flag inside the RAII struct as a `static bool` member (same semantics), delete the file-scope bool. Or rename one to make the relationship obvious.

### [LOW/introduced] Comment in `loopMQTTDiscovery` references "3s interval" that is gone

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:409`
- **Snippet**: Already covered under LOW findings. Repeated here because it's specifically dead-comment rot.

### [LOW/introduced] `state.heapdiag.iLastPublishedEpoch` is in the UI translateFields? (check)

- **File:line**: Checked — not present in `data/index.js`. So the write-only field is also not exposed anywhere. Reinforces the "drop it" recommendation.

### [LOW/introduced] Duplicate "// ADR-064: single caller" comments on every dispatcher call

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:295, 297, 299, 407`
- **Snippet**:

  ```cpp
  // ADR-064: single caller
  const bool hourFlag = hourChanged();
  // ADR-064: single caller
  const bool dayFlag  = dayChanged();
  // ADR-064: single caller
  const bool yearFlag = yearChanged();
  ...
  // ADR-064: single caller
  if (minuteChanged())              doTaskMinuteChanged();
  ```

  The long header comment at lines 285-293 already explains the rule. Four repeats of the same tag-line is noise that will rot as soon as someone touches the code.
- **Cleanup**: Delete the per-line `// ADR-064: single caller` comments. Keep the block header. The CI check in `evaluate.py::check_time_boundary_single_caller` is the real enforcement mechanism — the comments add nothing.

### [LOW/introduced] `HEAP_FRAG_PROMOTE_MAXBLOCK` is a `#define`, not a `constexpr`

- **File:line**: `src/OTGW-firmware/helperStuff.ino:722`
- **Snippet**:

  ```cpp
  #define HEAP_FRAG_PROMOTE_MAXBLOCK  1536   // maxBlock below this while freeHeap in LOW → promote to WARNING
  ```

  The rest of the branch uses `constexpr` for type-safe compile-time constants (MQTTstuff.ino passim). This one is `#define` for no reason — likely muscle memory.
- **Cleanup**:

  ```cpp
  constexpr uint32_t HEAP_FRAG_PROMOTE_MAXBLOCK = 1536;
  ```

### [LOW/pre-existed] `Log heap statistics every minute for monitoring` comment now misleading

- **File:line**: `src/OTGW-firmware/OTGW-firmware.ino:258-259`
- **Snippet**:

  ```cpp
  // Log heap statistics every minute for monitoring
  logHeapStats();
  ```

  After the refactor, `doTaskEvery60s` runs on a 60s boot-relative timer; the new `doTaskMinuteChanged` handles wall-clock alignment. The "every minute" is technically "every 60s" which was true before and after, so not strictly stale — but the neighbouring comment at 261-263 now discusses the `doTaskMinuteChanged` relationship. Tighten for consistency.
- **Cleanup**: Not strictly dead, but a comment-rot opportunity while touching this block.

## Critical issues for Phase 2 context

Items Phase 2 (security/performance) and Phase 3 (testing) should re-examine:

1. **`setBufferSize(1024)` during verify window** (MQTTstuff.ino:239) — resizes PubSubClient's buffer via `realloc`. Phase 2 should verify umm_malloc fragmentation impact over multi-week uptime, especially if the verify runs daily. The `getMaxFreeBlockSize() < 1280` precheck at line 221 helps but does not prevent fragmentation accumulation.
2. **Verify window topic filter (MQTTstuff.ino:629-656)** — fast-path in the MQTT callback, runs on every retained message. No bounds check on `topic` length; `strchr` could in theory walk a malformed topic. See LOW finding on NUL-termination.
3. **`isDripDeferred()` + cooldown default of 10s** — under field Status-frame cadence of ~3s, the drip can stall permanently. Phase 2 should validate against the referenced Crashevans log data whether the current tuning ever actually allows discovery progress.
4. **VH publishers not wrapped in Status-burst quiesce** (HIGH finding) — Phase 2 should confirm the heap-pressure reduction claim holds for VH-equipped boilers too.
5. **`getHeapHealth()` transition counter inflation** (MEDIUM finding) — Phase 2 telemetry interpretation must account for this; field reports will mislead debugging otherwise.
6. **REST endpoint `sendDeviceInfoV2` growth** (restAPI.ino:879+) — this branch added 15 new JSON map entries. Phase 2 should verify the cumulative JSON size vs. HTTP chunk-stream assumptions in the frontend.
7. **`sendMQTTheapdiag` json[384] buffer** — current worst-case serialization is ~260 bytes. One more counter and this exceeds the buffer. Add the truncation check.
8. **NTP `ZonedDateTime` per-minute allocation cost** (MEDIUM finding) — may not crash but Phase 2 should measure the CPU footprint now that 4+ constructions happen per minute in the dispatcher.

Overall assessment: the branch is safe to merge pending the HIGH findings being addressed. The dead-code cleanup is mostly noise reduction, not stability risk. The one real functional gap is the VH Status-burst wrapping.
