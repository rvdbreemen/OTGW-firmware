---
id: TASK-1176
title: >-
  POST /api/v2/mqtt/republish resets loop-owned MQTT publish state from the
  async_tcp task; queue the request and apply the reset in loop()
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-30 08:45'
updated_date: '2026-09-30 09:20'
labels:
  - bug
  - mqtt
  - rest
  - concurrency
dependencies: []
priority: low
ordinal: 299000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OBSERVED DEFECT
Found by code inspection on dev a2a58ecf9; the traced files are unchanged since a26b19134.
- The handler for POST /api/v2/mqtt/republish resets the OT-value publish trackers and the four status force flags directly on the async_tcp task.
- processOT() on the loop task reads and clears the same state. Nothing synchronizes the two.
- This is a data race under the C++ memory model.
- With the current task pinning, the only lasting effect is a lost force flag. After a republish request, hvac_mode or hvac_action can stay unrepublished for up to 300 s.

CORRECTION TO THE ORIGINAL CLAIM
- The shared state lives in OTGW-Core.ino, not MQTTstuff.ino.
- MQTTstuff.ino holds only the two other callers: HA online (:817) and reconnect after a long offline (:1325).
- Both of those run on loopTask. espMqttClient is constructed with UseInternalTask::NO (MQTTstuff.ino:207) and is pumped only by `MQTTclient.loop();` (MQTTstuff.ino:1079).

MECHANISM
1. The writer runs on async_tcp.
   - Chain: FSexplorer.ino:238 registers processAPI for /api (server.on, HTTP_ANY) -> processAPI (restAPI.ino:2691) -> route `{ kRouteMqtt, handleMqtt }` (:2546) -> `r.handler(words, wc, method, originalURI);` (:2786) -> handleMqtt (:2061) -> `requestMQTTRepublishAll();` (:2088). No lock is taken.
   - ESPAsyncWebServer 3.11.0 calls the handler synchronously from the AsyncClient onData callback (WebRequest.cpp:79-83, :860; WebHandlers.cpp:309-311).
   - AsyncTCP 3.4.10 dispatches that callback from its "async_tcp" service task (AsyncTCP.cpp:293, :320-332, :382).
   - The code says the same at restAPI.ino:2716-2717: "Safe under ESPAsyncWebServer's single-task (async_tcp) handler serialization".
2. What the writer changes.
   - requestMQTTRepublishAll (OTGW-Core.ino:1935-1939) calls resetMqttTrackedState (:391-413). That sets mqttlastsent[256], the status and VH bit/byte timers, and the ASF/RBP/RO timers to TRACKED_TIME_UNSEEN.
   - It also calls requestMQTTStatusRepublish (:1941-1947). That sets mqttForceNext{Master,Slave}{,VH}StatusPublish = true (the flags are defined at :245-248).
3. The reader/writer runs on loopTask.
   - Chain: loop() -> `drainOTFrameQueue();` (OTGW-firmware.ino:1085, its only caller) -> processOT (OTGW-Core.ino:571, :4852).
   - processOT takes `OTStateLock stateLock;` (:4864). handleMqtt takes no lock, so nothing orders the two sides.
   - Each force flag is read and later cleared: master `const bool forcePublish = shouldForceMasterStatusPublish();` (:2305) -> `mqttForceNextMasterStatusPublish = false;` (:2324); slave :2349 -> :2368; Status VH :2435 -> :2454 and :2475 -> :2494.
4. Scheduling.
   - async_tcp runs at priority 10 on core 1: platformio.ini:62 `-DCONFIG_ASYNC_TCP_RUNNING_CORE=1` and :70 `-DCONFIG_ASYNC_TCP_PRIORITY=10`. All three envs inherit these through ${env.build_flags} (:138, :238, :281).
   - loopTask runs at priority 1 on core 1: arduino-esp32 3.3.5 cores/esp32/main.cpp:113 `xTaskCreateUniversal(loopTask, ..., NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);`, with esp32s3 qio_qspi sdkconfig.h:482 `#define CONFIG_ARDUINO_RUNNING_CORE 1`.
   - So the REST reset preempts the loop at an arbitrary instruction and runs to completion. The loop never sees a half-done reset.
   - That no-torn-state bound depends on the current single-core pinning.

IMPACT
- Lost hvac_mode force:
  - If the reset lands between :2305 and :2324, the clear at :2324 overwrites the handler's `true`.
  - hvac_mode republishes only on force, on a change, or on the heartbeat: `if (forcePublish || mode != mqttLastHvacMode || hvacHeartbeatDue(mqttLastHvacModeSent)) {` (:2266), with HVAC_HEARTBEAT_INTERVAL_SEC = 300 (:2244).
  - resetMqttTrackedState does not reset its cache, mqttLastHvacMode/Sent (:2239-2246).
  - Result: hvac_mode is not re-sent until it changes or 300 s pass.
- Lost hvac_action force: the same happens for hvac_action through the slave window (:2349 -> :2368, :2288, :2387).
- Everything else heals itself:
  - Tracker-gated topics see TRACKED_TIME_UNSEEN and publish as first-seen on the next frame (:1975, :2055/:2066, :2093/:2103).
  - A confirm*Slot write that lands after the reset (:1908-1933) only stamps a slot published in that same frame.
  - A lost VH force changes nothing, because the whole VH fan-out is tracker-gated (:2465-2468, :2512-2517).
  - mqttPending*Slot is not reset, and no crash path was found.
- Likelihood:
  - The window is a few instructions per status frame. It is longer only when the telnet MQTT-gate debug inside it is on (:2307-2322).
  - The natural hit rate per POST is very small. That is an estimate, not a measurement.

TRIGGER
- Any LAN client that POSTs /api/v2/mqtt/republish while MQTT is connected (restAPI.ino:2065).
- At most one accepted call per 60 s (:2071-2086).
- Auth applies only when an admin password is set (checkHttpAuth, restAPI.ino:219, called at :2765-2766).
- The route is unconditional: esp32, esp32-classic and esp32-combo, in both PIC and OTDirect mode.
- The web UI never calls it (no reference under src/OTGW-firmware/data). It is documented for automation at docs/api/README.md:508 and docs/api/openapi.yaml:1658.
- Prior sighting: the TASK-1059 notes say "Noted but out of scope: restAPI.ino:1993 calls requestMQTTRepublishAll() from the ESPAsyncWebServer handler, which DOES run on async_tcp". That line is now :2088.
- No test or evaluate.py gate covers this path.

1.x STATUS: NOT AFFECTED, NO PORT
- wt-otgw-1.x.x restAPI.ino:771 also calls requestMQTTRepublishAll(), but 1.x is single-threaded.
- httpServer.handleClient() (OTGW-firmware.ino:489) runs after handleOTGW() (:480), one after the other, in doBackgroundTasks.
- The only re-entry is delayms() (helperStuff.ino:1296-1302). It is reached only through blinkLED from setup (OTGW-firmware.ino:168, :211) and from a telnet key (handleDebug.ino:260), never inside processOT.

OUT OF SCOPE: SAME CLASS, CONSEQUENCES NOT TRACED, SEPARATE TASKS
(a) Telnet debug keys run on async_tcp. AsyncSimpleTelnet is in callback mode (networkStuff.ino:626), dispatches from onData (AsyncSimpleTelnet.h:317-319, :401-406), and its loop() is a no-op (:130).
   - Key 'r' -> startMQTT() -> handleMQTT() -> MQTTclient.loop() (handleDebug.ino:393-395, MQTTstuff.ino:592, :1079) pumps the MQTT engine off the loop task while MQTT is disconnected.
   - The comment at handleDebug.ino:494-496 is stale.
(b) Two discovery endpoints change loop-owned discovery state from async_tcp:
   - POST /api/v2/discovery/republish calls markAllMQTTConfigPending (restAPI.ino:2042 -> MQTTstuff.ino:2029);
   - /discovery/verify calls startDiscoveryVerification (restAPI.ino:2004).
(c) OTDirect REST command paths run processOT on async_tcp (OTDirect.ino:640, :2145, :2157) with an unbounded OTStateLock wait (OTGW-Core.ino:4864). That breaks the rule at OTGW-Core.h:622-626.
   - These lines never reach the status fan-out: isvalidotmsg rejects them (OTGW-Core.ino:4456-4457), so they take the branch at :5199 or :5206.
(d) Stale comments:
   - OTGW-Core.ino:4859-4861 says the consumer runs on the PIC task;
   - OTGW-Core.ino:1905-1907 says the confirm is called from sendMQTTData, but the real callers are :4434 and :5161.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 handleMqtt (restAPI.ino) no longer calls requestMQTTRepublishAll(). It calls queueMQTTRepublishAll(), which only raises a flag that loop() consumes. The comments at restAPI.ino:2059 and :2089 say the reset is queued for loop(). Static check: `grep -n "requestMQTTRepublishAll()" src/OTGW-firmware/*.ino` lists only the two MQTTstuff.ino callers (HA online, onMqttConnect) and the new consumer handlePendingMQTTRepublish() in OTGW-Core.ino. The consumer is called exactly once, from loop() in OTGW-firmware.ino immediately before drainOTFrameQueue(), and outside both #if HAS_PIC and the !isFlashing() block.
- [ ] #2 A curl transcript on the bench shows the API contract is unchanged: POST /api/v2/mqtt/republish returns 200 {"status":"republish_requested"}; a second POST within 60 s returns 429 with the retry seconds; with MQTT disconnected it returns 503 'MQTT not connected'; GET returns 405.
- [ ] #3 Old-vs-fix task-context proof. Both builds carry the same test-only log line at the entry of requestMQTTRepublishAll, which prints the calling task name (pcTaskGetName(nullptr)). A REST POST logs 'async_tcp' on OLD (dev before the fix) and 'loopTask' on FIX. Control: a homeassistant/status offline -> online cycle logs 'loopTask' on both builds and re-publishes hvac_mode at the next master status frame. Telnet transcript attached.
- [ ] #4 Old-vs-fix lost-update reproduction, following the repro plan. Both builds carry identical test-only instrumentation: a 'RACEWIN open' telnet marker, a 2 s stretch between the force-flag read (OTGW-Core.ino:2305) and the clear (:2324), and a lost-force counter. Run at least 10 marker-synchronized POSTs per build, with the replay as frame source. OLD: the counter increments on the synchronized trials, and at the next master status frame status_master is re-published but hvac_mode is not. FIX: the counter stays 0, and status_master and hvac_mode are both re-published at the next master status frame after every POST. Exclude trials where the 300 s hvac heartbeat could fire in the observation window. Attach capture transcripts for both builds.
- [x] #5 The committed fix contains none of the test-only instrumentation (task-name log, marker, stretch, counter): `git show <fix commit>` shows only the queue/consume change, the declarations and the comment updates.
- [x] #6 build.bat builds esp32, esp32-classic and esp32-combo fresh, shown by the SUCCESS lines and fresh firmware.bin and littlefs.bin timestamps. `python evaluate.py --quick` shows no new FAIL. The prerelease tag is bumped with bin/bump-prerelease.sh in the same commit.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from the backlog triage (workflow wf_1c568508-5f2) after an adversarial verification (workflow wf_4f2df5e8-285): 3 independent verifiers (trace / refute / impact) all voted real; judge severity low.

FIX PLAN:
1. OTGW-Core.ino: add a single-slot loop bridge next to requestMQTTRepublishAll() (:1935-1947).
   - Use house style: a static volatile bool, cleared before acting. Precedents: g_picRxOverrunPending (:605), g_rebootPending (helperStuff.ino:396), and the snapshot-and-clear in handlePendingPicHttp (:6072-6077).
   - Code:
     static volatile bool g_mqttRepublishAllPending = false;
     void queueMQTTRepublishAll() { g_mqttRepublishAllPending = true; }
     void handlePendingMQTTRepublish() {
       if (!g_mqttRepublishAllPending) return;
       g_mqttRepublishAllPending = false;  // clear first; a request raised after this line is served next pass
       requestMQTTRepublishAll();
     }
   - Comment it in the present tense: the REST handler runs on async_tcp; processOT() on the loop task owns the trackers and force flags; loop() applies the reset between frames.

2. OTGW-Core.h: declare queueMQTTRepublishAll() and handlePendingMQTTRepublish() next to :524-525. Mark requestMQTTRepublishAll() as loop-task only.

3. restAPI.ino:2088: replace `requestMQTTRepublishAll();` with `queueMQTTRepublishAll();`.
   - Keep the cooldown stamp at :2089, reworded from "stamp only after work commits" to "stamp once the request is queued".
   - Keep the 200 republish_requested reply at :2091.
   - Update the header comment at :2058-2059.
   - The API contract does not change. The reply already says "requested" (openapi.yaml:1679), and the republish was always paced by bus arrival.

4. OTGW-firmware.ino loop(): call handlePendingMQTTRepublish() immediately before `drainOTFrameQueue();` (:1085).
   - That is loop() proper, so it never runs inside processOT() or the re-entrant doBackgroundTasks().
   - It sits outside #if HAS_PIC (:1071-1074), so no-PIC builds run it too, and outside the !isFlashing() block.
   - The next frames then see first-seen plus force.

5. Leave MQTTstuff.ino:817 and :1325 unchanged; they already run on loopTask.

6. Why volatile bool is enough:
   - There is one producer (async_tcp) and one consumer (loopTask), both pinned to core 1 (platformio.ini:62; sdkconfig CONFIG_ARDUINO_RUNNING_CORE 1).
   - A byte store cannot tear, and volatile stops the compiler caching the flag across loop() passes.
   - After the fix, only loopTask touches the trackers and force flags, so no other ordering is needed.
   - A cross-core placement of async_tcp would need std::atomic<bool>. Record that here in the task record, not as a code comment.

7. Rejected alternative: wrap the direct call in OTStateLock(OT_STATE_READ_LOCK_MS).
   - It would serialise correctly, because processOT holds the mutex for the whole frame (OTGW-Core.ino:4864).
   - But async_tcp could block up to 100 ms behind per-frame I/O (OTGW-Core.h:622-626, the TASK-879 concern), and it would need a 503 timeout path.
   - It would also stretch a lock documented for the decoded snapshot over publish state.

8. Process:
   - Backlog task first.
   - No ADR: this is a bug fix inside the ADR-137 queue-then-loop-worker pattern.
   - Bump with bin/bump-prerelease.sh, since the change touches src/OTGW-firmware/**.
   - Run build.bat (all three envs), then evaluate.py --quick.
   - No 1.x port.
   - Adjacent issues (a)-(d) in the description get their own tasks.

REPRO PLAN:
Nothing was run for this verdict: the brief was read-only and the bench is busy.

Why a plain run is not enough: the natural window is a few instructions long. An unmodified old-vs-fix run would almost always pass on OLD, which proves nothing. Use identical test-only instrumentation in both builds and never commit it.

HARDWARE AND TOOLS
- OTGW32 bench: ESP32-S3 on COM4, MAC 10:20:BA:21:B4:F8, running its usual env (esp32-combo by default).
- The bench MQTT broker, telnet port 23, curl, mosquitto_sub and scripts/capture-mqtt-debug.bat.
- The classic-S3 bench works too; the path is board-independent.

BUILDS
- OLD = dev at the commit before the fix, plus patch T.
- FIX = the fix commit, plus the same patch T.
- Patch T:
  (1) At the entry of requestMQTTRepublishAll(): print 'RACE republish on <task>' using pcTaskGetName(nullptr).
  (2) In publishMasterStatusState(), between the byte gate (OTGW-Core.ino:2306) and the clear (:2324):
      - print a 'RACEWIN open' line;
      - delay(2000);
      - then if (!forcePublish && mqttForceNextMasterStatusPublish), increment a static raceLostForce counter and print 'RACE lost force #n'.
- Flash app-only with flash_otgw.bat --update --app <bin> so settings survive.

WHY THE MARKER WORKS (checked in source)
- Debug* macros write straight to debugTelnet (debugStuff.h:69-81).
- AsyncSimpleTelnet::write() flushes from the calling task: _flushTx -> c->add() + c->send() (AsyncSimpleTelnet.h:462-476, :507-526). So the marker leaves the device before loopTask enters delay().
- handleMqtt takes no OTStateLock, so the POST runs while processOT holds the lock during the stretch.
- The 2 s stretch is far below the 30 s loop TWDT.

FRAME SOURCE
- POST /api/v2/simulate/start.
- Since TASK-1071 the replay runs loop-side on any transport: handleOTReplay (OTGW-Core.ino:5330-5346), called at OTGW-firmware.ino:978; see also restAPI.ino:687-696.
- It plays one line per 750 ms (debugStuff.h:59).
- data/otgw_simulation.log (1156 lines) holds 39 T08000800 / B40000800 msgId-0 pairs. A master status frame therefore arrives about every 22 s.
- hvac_mode and hvac_action never change on their own in this fixture, so only a force, first-seen or the 300 s heartbeat publishes them.
- sendMQTTData has no simulation gate (MQTTstuff.ino:1386-1421).

PRE-FLIGHT
- mosquitto_sub -v -t '<top>/value/<node>/#' shows status_master and hvac_mode arriving from the replay.
- Telnet shows 'RACEWIN open' once per master status frame.
- No marker means no status frames. That is a stall, not a negative result.

STEP A (AC 3): TASK CONTEXT
- Send an unsynchronized curl -X POST http://<ip>/api/v2/mqtt/republish.
- Expected: OLD logs 'RACE republish on async_tcp'; FIX logs 'loopTask'.
- Control: publish 'offline' then 'online' to homeassistant/status. Both builds log 'loopTask', and hvac_mode re-publishes at the next master status frame.

STEP B (AC 4): LOST UPDATE
- A small driver (Python telnet reader plus HTTP POST) waits for 'RACEWIN open' and POSTs /api/v2/mqtt/republish at once, well inside the 2 s stretch. It records the HTTP code and the time.
- At the next marker, about 22 s later, it checks whether status_master and hvac_mode arrive within 5 s.
- It waits at least 61 s for the cooldown, then repeats: 10 trials per build.
- Exclude trials where the last hvac_mode publish is 300 s or more before that next marker, because the heartbeat could fire.
- Expected OLD: 'RACE lost force' on each synchronized trial. At the next master frame status_master is re-published (first-seen after the reset) but hvac_mode is not. hvac_action still re-publishes, because the slave force was not in the window.
- Expected FIX: no 'RACE lost force', and status_master plus hvac_mode are both re-published at the next master frame after every POST.
- Optional: repeat with the stretch in publishSlaveStatusState (:2349 -> :2368) and watch hvac_action.

CONTROL (supports severity low)
- Run the same builds without the stretch, with unsynchronized POSTs every 61 s.
- Expected: no lost force in either build.

RUN CONDITIONS
- Close the web UI: the stretch holds OTStateLock, so otmonitor reads return 503.
- Send no OTDirect command REST calls during the run. They take OTStateLock unbounded on async_tcp and would stall the POST.
- Keep the telnet MQTT-gate debug off.
- Record with scripts/capture-mqtt-debug.bat as transcript-<host>-REPUBLISH-<hw>-<datetime>.txt.

WHY NOT A HOST TEST
- A host harness cannot show the preemption.
- A harness that re-implements the gates proves nothing about the real code (maintainer rule).

DISSENT / CORRECTIONS:
No verifier dissented. All three lenses (trace, refute, impact) returned real, high confidence, severity low, and none offered a concrete refutation.

I re-checked every decisive line myself:
- restAPI.ino:2088: a direct call with no lock.
- The dispatch onto async_tcp: FSexplorer.ino:238, restAPI.ino:2786 and :2716-2717, plus the ESPAsyncWebServer 3.11.0 and AsyncTCP 3.4.10 call chain.
- The pinning: platformio.ini:62/:70, main.cpp:113, sdkconfig.h:482.
- The state and its windows: OTGW-Core.ino:245-248, :391-413, :1935-1947, :2305/:2324 and :2349/:2368.
- The hvac cache sits outside the reset: :2239-2246, :2266, :2288.
- The single loop-side consumer: OTGW-firmware.ino:1085.
- No other lock serialises REST against the loop. otStateMutex is the only FreeRTOS mutex in the firmware sources, and handleMqtt does not take it.

Points I resolved between the verifiers:
(1) File location. All three corrected the claim: the state is in OTGW-Core.ino, not MQTTstuff.ino.
(2) Replay on OTDirect. Verifier 1 wrote that /simulate is inert on OTDirect, which matches an older project memory note.
   - The code shows TASK-1071 moved the replay loop-side for any transport: OTGW-Core.ino:5330-5346, OTGW-firmware.ino:978, restAPI.ino:687-696.
   - So verifier 3 is right, the OTGW32 bench can drive the repro, and that memory note is stale.
(3) Repro method. Verifier 2 proposed a statistical run: a 50 ms stretch, a ~1 s cooldown, and about 5% expected misses. Verifier 3 proposed a marker-synchronized run plus a host harness built from extracted source spans.
   - I chose the synchronized run after checking two things. The telnet marker leaves the device while loopTask is stretched (AsyncSimpleTelnet::write flushes from the calling task, AsyncSimpleTelnet.h:462-476 and :507-526). sendMQTTData has no simulation gate (MQTTstuff.ino:1386-1421).
   - I dropped the host harness because it cannot show the preemption.
(4) Flag type. Verifiers 1 and 3 proposed volatile bool (house style); verifier 2 proposed std::atomic exchange.
   - I chose volatile bool. It is valid because both tasks are pinned to core 1.
(5) Rejected alternative. All three rejected putting OTStateLock in the handler, because it blocks async_tcp behind per-frame I/O.

The likelihood figures are verifier estimates, not measurements: about 1e-7 to 1e-5 per POST naturally, and about 5% with a 50 ms stretch.

The adjacent findings are recorded as out of scope. Their dispatch context is verified, but their consequences are not traced:
- the telnet 'r' key;
- the discovery/republish and discovery/verify endpoints;
- the unbounded OTStateLock on the OTDirect REST processOT path;
- the stale comments.

2026-09-30 implementation (alpha.380), following the fix plan.
- OTGW-Core.ino: static volatile bool g_mqttRepublishAllPending, queueMQTTRepublishAll() (raise) and handlePendingMQTTRepublish() (snapshot-and-clear, then requestMQTTRepublishAll()), next to requestMQTTRepublishAll(). OTGW-Core.h declares both and marks requestMQTTRepublishAll() loop-task only.
- restAPI.ino handleMqtt: requestMQTTRepublishAll() -> queueMQTTRepublishAll(); cooldown stamp kept ('stamp once the request is queued'); header comment says the reset is queued for loop(). API contract unchanged (200 republish_requested / 429 / 503 / 405).
- OTGW-firmware.ino loop(): handlePendingMQTTRepublish() immediately before drainOTFrameQueue(), after doBackgroundTasks(), outside #if HAS_PIC and outside the !isFlashing() block.
AC#1 static check: grep 'requestMQTTRepublishAll()' src/OTGW-firmware/*.ino -> MQTTstuff.ino:817 (HA online), MQTTstuff.ino:1325 (onMqttConnect), OTGW-Core.ino:1938 (definition), OTGW-Core.ino:1960 (the consumer). handlePendingMQTTRepublish() is called once, OTGW-firmware.ino:1085.
AC#5: the commit carries no test-only instrumentation (no task-name log, marker, stretch or counter).
AC#6: bin/bump-prerelease.sh alpha.379 -> alpha.380 in this commit; build.bat --target all: esp32, esp32-classic and esp32-combo SUCCESS for firmware and filesystem, fresh .ino.bin/.littlefs.bin 11:12-11:19 (alpha.380+d57d9e2, images saved under %LOCALAPPDATA%/OTGW-capture/img-alpha380-d57d9e2); evaluate.py --quick 69 passed, 0 warnings, 0 failed.
OPEN: AC#2 (curl contract on the bench), AC#3/#4 (old-vs-fix task-context and lost-update proofs, which need identical test-only instrumented builds of OLD and FIX plus the OTGW32 bench on WiFi).
<!-- SECTION:NOTES:END -->
