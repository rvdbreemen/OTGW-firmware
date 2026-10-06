---
id: TASK-1175
title: >-
  PIC banner callback fwreportinfo() writes OTGWState and publishes MQTT from
  the PIC UART task (ADR-130 seam violation)
status: To Do
assignee:
  - '@claude'
created_date: '2026-09-30 08:45'
updated_date: '2026-10-06 12:51'
labels:
  - bug
  - pic
  - adr-130
  - concurrency
dependencies: []
priority: low
ordinal: 298000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Defect (dev only, PIC mode). The OTGWSerial firmware-banner callback fwreportinfo() runs on the "picSerial" FreeRTOS task, not on the loop. In that task context it writes OTGWState, logs to telnet, builds String temporaries and publishes up to 10 MQTT topics. ADR-130:89-90 forbids exactly this: "The task does byte I/O **only**: no parse, no `processOT()`, no MQTT/WebSocket, no `OTGWState` write." ADR-130:140-144 also assumes the version readout stays loop-side. Code state: dev a2a58ecf9.

MECHANISM
1. OTGWSerial::read() runs matchBanner() on every byte. src/libraries/OTGWSerial/OTGWSerial.cpp:905-907: "retval = HardwareSerial::read(); if (retval >= 0) { matchBanner(retval); }".
2. matchBanner() (OTGWSerial.cpp:1042-1066) calls "_firmwareFunc(firmware, fwversion);" (:1052-1053) when a banner from :96-98 plus a version is followed by whitespace. It is a streaming matcher, so it also fires inside a "PR: A=OpenTherm Gateway 6.6" reply.
3. _firmwareFunc is fwreportinfo. Its only registration is in detectPIC(): src/OTGW-firmware/OTGW-Core.ino:998 "OTGWSerial.registerFirmwareCallback(fwreportinfo);".
4. At runtime the only reader is picSerialDrainOnce() (OTGW-Core.ino:697-800; :720 "uint8_t outByte = OTGWSerial.read();").
   - It is called by picSerialTaskBody (:846-858) on the task created at :882-883: "platformTaskCreatePinned(picSerialTaskBody, "picSerial", 4096, nullptr, 1);".
   - The file's own contract (:684-686) says this code does "NO network/MQTT/WebSocket I/O and NO OTGWState writes".
5. fwreportinfo (OTGW-Core.ino:5780-5791) takes no OTStateLock. In task context it:
   - strlcpy's state.pic.sFwversion, sDeviceid and sType (:5782, :5785, :5788);
   - calls DebugTln/DebugTf/OTDebugTf (:5781-5789), which share _debugBOL's statics (debugStuff.ino:43-57);
   - builds Strings through processorToString() (sscanf) and firmwareToString(fw) (OTGWSerial.cpp:959-1000);
   - calls sendMQTTversioninfo() (:5790). That function (MQTTstuff.ino:1610-1633) makes 6 publishes, plus 4 otgw-pic/* when isPICEnabled().
   - Each publish goes through mqttPublishRaw() (MQTTstuff.ino:332-336). That takes espMqttClient's mutex with portMAX_DELAY (.pio/libdeps/esp32-combo/espMqttClient/src/Helpers.h:16, MqttClient.cpp:160) and then calls feedWatchDog().
   - On HAS_PIC_WATCHDOG boards (src/libraries/Platform/src/boards.h:170, :239), feedWatchDog() runs the 0x26 I2C feed (when the chip was detected) and the LED1 blink in task context (OTGW-Core.ino:1476-1486). TASK-1131 had removed exactly that work from the task (:691-696).
6. CI does not catch it.
   - evaluate.py::check_pic_uart_task_owns_serial (evaluate.py:3250) matches byte-I/O call sites only (:3282-3283).
   - Its docstring lists registerXxxCallback as a control method (:3262-3265).

TRIGGERS
Affected: esp32-classic, and esp32-combo booted in PIC mode (HAS_PIC=1, boards.h:169, :232). Not affected: esp32/OTGW32 (HAS_PIC=0, boards.h:100), and a combo in OTDirect, where the task parks (OTGW-Core.ino:871).

Every PIC-mode boot (near-certain):
- resetOTGW() (OTGW-firmware.ino:528) runs before startPICSerialTask() (:536).
- GW=R executes `reset` (other-projects/otgw-6.6/gateway.asm:4734-4738). The reset vector calls SelfProg first (:627-629), which waits up to 1 s for STX (selfprog.asm:19-20). Then GreetingStr is printed (gateway.asm:1322-1323). The task reads that banner.
- MQTT is normally not connected yet: handleMQTT is gated on bSetupComplete (OTGW-firmware.ino:949), and connect() is issued at MQTTstuff.ino:1159 on a later tick. So the boot publishes mostly return early. Unverified: whether the connect after setup can beat the banner.
- Unverified and timing-dependent: a banner left over from detectPIC()'s earlier reset may also be buffered and read by the task.

Runtime, with MQTT connected (the full 10-publish path runs on the task):
- MQTT resetgateway (MQTTstuff.ino:986);
- ser2net GW=R (OTGW-Core.ino:5427-5431);
- GW=R or PR=A through REST POST /api/v2/otgw/commands (restAPI.ino:729-738) or the MQTT command topic;
- the 60 s PR=A probe while sDeviceid is unknown (OTGW-firmware.ino:781-790);
- PIC watchdog or brown-out restarts;
- every diagnose-menu redraw (diagnose.asm:356 prints Banner inside MainLoop; :373 and :379 loop back).

Why the loop-side handler does not cover these:
- processOT's banner branch (OTGW-Core.ino:5247-5279) runs loop-side under OTStateLock (:4864).
- A PR=A reply, "PR: A=OpenTherm Gateway x" (gateway.asm:3695-3703, :5434-5435), takes the buf[2]==':' branch instead (:5199-5202). handlePRresponse then drops register A: "default: OTDebugTf(PSTR("handlePRresponse: unknown register [%c]\r\n"), reg); return;" (:1265-1267).
- The diagnose and interface banners miss the case-sensitive strstr at :5247.
- For these cases fwreportinfo is the only handler. The fix must move its work to the loop, not delete it.

IMPACT (no crash proven)
- The PIC task writes state.pic.* without OTStateLock. The loop (processOT) and async_tcp (REST snapshot, restAPI.ino:3064-3065) use the same fields.
- The loop's OTPublishGate flips the global mqttPublishAllowed (OTGW-Core.h:744-750, OTGW-Core.ino:5156-5164). Task publishes that land in that window are dropped at MQTTstuff.ino:1389/1434. do5minevent (OTGW-firmware.ino:895-897) and onMqttConnect (MQTTstuff.ino:1354) republish, so a drop heals within 5 minutes.
- The increment ++mqttSendSuccessCount (MQTTstuff.ino:1419, :1465) is not synchronised with the loop's ADR-104 slot commit (OTGW-Core.ino:5157-5163). This is unlikely, but it can commit a slot whose own publish failed.
- Telnet prefixes can interleave. The I2C 0x26 feed and the LED1 write run off-loop.
- The byte-I/O task can block on the espMqttClient mutex while MqttClient::loop() holds it across socket I/O (MqttClient.cpp:270-275). With a stalled socket, that lasts as long as the loop's own stall.
- The picSerial stack headroom under the publish chain has never been measured. One verifier estimated about 2.4 KB of 4 KB from ELF frame sizes; not re-verified.
- No library corruption and no deadlock found. espMqttClient, AsyncSimpleTelnet (AsyncSimpleTelnet.h:159) and Wire serialise internally. espMqttClient releases its mutex around callbacks (MqttClient.cpp:510-512, :550-558). The park wait in resetOTGW is bounded (OTGW-Core.ino:898-901).

No synchronous waiter depends on the task-side write:
- The state.pic.* readers are single reads: restAPI.ino:3114-3116, :3371, :3537-3567; OTGW-Core.ino:5962, :6082-6084, :6128-6172; FSexplorer.ino:391.
- fwupgradedone (OTGW-Core.ino:5667-5726) does not wait for the banner.
- Deferring the work to the loop adds at most one loop pass of latency.

1.x STATUS: not affected.
- otgw-1.x.x reads the UART in handleOTGW() on the single loop: wt-otgw-1.x.x/src/OTGW-firmware/OTGW-Core.ino:4830-4833 "outByte = OTGWSerial.read();". The callback is registered at :571; fwreportinfo is at :5302-5351.
- The 1.x callback also removes GW=R from cmdqueue (:5304-5313), self-heals bAvailable (TASK-1126, :5321-5332) and publishes only on change (TASK-1127, :5314-5319, :5341-5350).
- Porting those into dev's callback as-is would move a cmdqueue mutation onto the task. On dev they belong in the loop-side consumer this task creates.

History: the TASK-1111 notes (AC#3 scope) deferred this "separate pre-existing bug" to its own task. `backlog search fwreportinfo` and `backlog search matchBanner` find none.

FOLLOW-UPS (separate tasks; code reading, not bench-verified)
1. The 60 s PR=A probe cannot re-enable a PIC on dev.
   - bAvailable is assigned only at OTGW-Core.ino:1012 and :5251, and a PR=A reply never reaches :5251.
   - The probe stops once fwreportinfo fills sDeviceid (OTGW-firmware.ino:782-784). The comment at OTGW-firmware.ino:775 is therefore wrong.
   - Fix: port 1.x TASK-1126 into this task's loop consumer. Depends on this task.
2. Port 1.x TASK-1127 (the bannerChanged gate) into the consumer. Today every diagnose redraw republishes the version info.
3. processOT's banner branch duplicates the version write and publish (OTGW-Core.ino:5264-5270). Leave it alone in this task, because removing it has an ordering catch:
   - The consumer runs before processOT's bAvailable recovery, so its sendMQTTversioninfo() sees isPICEnabled()==false and skips otgw-pic/*.
   - Dropping processOT's publish would therefore lose those topics on DEGRADED to PIC recovery.
4. OTGWSerial.cpp:1056 writes fwversion[16] (:54) with no bound check, and _version_pos is a byte (OTGWSerial.h:201). This affects both lines.
5. Telnet debug keys run on async_tcp, not the loop.
   - AsyncSimpleTelnet::loop() is a no-op (AsyncSimpleTelnet.h:129-130); input is dispatched from _onData (:317-320, :401-406).
   - 'a' writes state.pic.* there (handleDebug.ino:354-358), and 'p' runs detectPIC() there.
   - The comment at handleDebug.ino:494-499 is stale.
6. Flash-completion handoff (plausible, unverified).
   - fwupgradedone clears bPICactive (OTGW-Core.ino:5692) before OTGWSerial::finishUpgrade deletes _upgrade (OTGWSerial.cpp:1083-1089).
   - The parked task re-checks every 20 ms (OTGW-Core.ino:849-852). Once unparked, its available() call runs upgradeEvent() (OTGWSerial.cpp:1095-1104) on a still-live _upgrade.
7. picSerialDrainOnce makes its own task-side DebugTf calls (OTGW-Core.ino:742, :764). Pre-existing and out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 fwreportinfo() in src/OTGW-firmware/OTGW-Core.ino only records that a banner arrived: a volatile flag, optionally with a counter. Its body contains none of the following: a state.* write; any Debug*, OTDebug* or MQTTDebug* call; a String or *ToString() call; a sendMQTT*, WebSocket or reportOTGWEvent call; feedWatchDog(); any cmdqueue access.
- [x] #2 A loop-side consumer called from drainOTFrameQueue() consumes the flag. It can sit inside reportPendingPICRxErrors(), which is called at OTGW-Core.ino:549. The consumer writes state.pic.sFwversion, sDeviceid and sType, emits the debug lines and calls sendMQTTversioninfo(). It clears the flag before applying, so a banner that completes in the meantime is handled on the next pass. processOT's banner branch (OTGW-Core.ino:5247-5279) is unchanged.
- [x] #3 Static old-vs-fix check. evaluate.py::check_pic_uart_task_owns_serial, or a sibling gate, resolves every function passed to OTGWSerial.registerFirmwareCallback(). It FAILs when that function's comment-stripped body matches the forbidden set from AC1. The gate FAILs on dev a2a58ecf9, naming fwreportinfo (OTGW-Core.ino:5780-5791), and PASSes on the fix. It does not inspect fwupgradestep, fwupgradedone or picSerialDrainOnce. tests/test_evaluate.py gains one failing and one passing fixture, and 'python tests/test_evaluate.py' passes.
- [ ] #4 Bench old-vs-fix check on a Classic PIC board running esp32-combo in PIC mode, with MQTT connected and the same task-name instrumentation in both builds. Triggers: 3x MQTT resetgateway=1 (at least 5 s apart) and 3x REST POST /api/v2/otgw/commands with body {'command':'PR=A'}. The OLD build (dev HEAD) logs the banner work on task picSerial; the FIX build logs it on loopTask. Both capture-mqtt-debug transcripts are attached and cited in the Final Summary.
- [ ] #5 PR=A-only parity in the FIX build. After the REST PR=A trigger, these carry the PIC's real values: /api/v2/device/info picfwversion, picdeviceid and picfwtype, and the broker's otgw-pic/version, otgw-pic/deviceid and otgw-pic/firmwaretype. This proves the deferral keeps version detection on the path processOT does not handle.
- [ ] #6 The picSerial task stack high-water mark is logged before and after the AC4 triggers in both builds. The OLD minimum is recorded in the task notes as the measured headroom. The FIX build shows no drop across the triggers.
- [x] #7 The comments at OTGW-Core.ino:1035-1036, OTGW-Core.ino:1181-1182 and OTGW-firmware.ino:775, and the evaluate.py docstring at :3262-3265, describe the actual flow: PR=A replies reach handlePRresponse, which ignores register A. The version is recorded through the banner callback and the loop consumer. The firmware callback executes inside OTGWSerial::read() on the PIC task.
- [x] #8 build.bat builds esp32-combo, esp32-classic and esp32 (HAS_PIC=0), each with a SUCCESS line and a fresh firmware.bin. 'python evaluate.py' reports no new FAIL. The prerelease tag is bumped with bin/bump-prerelease.sh in the same commit.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from the backlog triage (workflow wf_1c568508-5f2) after an adversarial verification (workflow wf_4f2df5e8-285): 3 independent verifiers (trace / refute / impact) all voted real; judge severity low.

FIX PLAN:
Approach: detect in the task, apply in the loop. reportPendingPICRxErrors() already splits the work this way (flags at OTGW-Core.ino:600-610, consumer at :809-838; ADR-130 "RX-error split").

1. Add the flag. In OTGW-Core.ino, in the #if HAS_PIC flag block next to g_picRawDropPending (:600-611), add `static volatile bool g_picBannerPending = false;`.

2. Shrink the callback. Reduce fwreportinfo (OTGW-Core.ino:5780-5791) to `(void)fw; (void)version; g_picBannerPending = true;`. Add a comment that it runs inside OTGWSerial::read() on the PIC task. Keep the name.

3. Add the loop-side handler. Add `static void applyPICBannerInfo()`, which does what processOT's banner branch already does loop-side (OTGW-Core.ino:5264-5270):
   - sFwversion from OTGWSerial.firmwareVersion() (OTGWSerial.cpp:951-953);
   - sDeviceid from processorToString();
   - sType from firmwareToString(), the no-argument overload (:972-974);
   - the three debug lines, then sendMQTTversioninfo().
   Optionally wrap the three strlcpy calls in a local OTStateLock scope. The lock is non-recursive (OTGW-Core.h:614-637), so never hold it across processOT.

4. Consume the flag. At the top of reportPendingPICRxErrors(), add `if (g_picBannerPending) { g_picBannerPending = false; applyPICBannerInfo(); }`.
   - reportPendingPICRxErrors() is called from drainOTFrameQueue() at :549. That runs only from loop() at OTGW-firmware.ino:1085, outside the doBackgroundTasks re-entrancy.
   - Clearing the flag first means a banner that completes meanwhile re-arms it.
   - Coalescing loses nothing, because the consumer reads the library's latest state.

5. Leave processOT's banner branch (:5247-5279) as is. See follow-up 3 in the description: an ordering catch loses the otgw-pic/* publish on DEGRADED to PIC recovery.

6. Correct the comments at OTGW-Core.ino:1035-1036, :1181-1182 and OTGW-firmware.ino:775. Do not change ADR-130: it is Accepted, and this fix brings the code in line with it.

7. Extend evaluate.py::check_pic_uart_task_owns_serial (:3250).
   - Collect every `OTGWSerial.registerFirmwareCallback(<fn>)` across the firmware sources.
   - Take <fn>'s body with the existing _fn_span() (:3295-3308) and strip comments.
   - FAIL on any reference to: state., sendMQTT, any Debug call, String or ToString(, feedWatchDog, WebSocket, reportOTGWEvent, cmdqueue or CmdQueue.
   - Scope the check to registerFirmwareCallback only. fwupgradestep and fwupgradedone (registered at OTGW-Core.ino:5851-5852) legitimately write state.flash.* and send WebSocket JSON. They fire from the upgrade state machine, which picSerialPumpUpgrade() drives loop-side while the task is parked (OTGW-Core.ino:5832-5837, OTGW-firmware.ino:929-935).
   - Leave picSerialDrainOnce's own DebugTf calls (:742, :764) alone.
   - Update the docstring at :3262-3265, and add FAIL and PASS fixtures to tests/test_evaluate.py.

8. Housekeeping:
   - run bin/bump-prerelease.sh in the same commit;
   - run `graphify update src`;
   - no new ADR, since this is a bug fix inside ADR-130.

Alternative considered: a depth-2 value-copy queue of {fw, version[16]} through platformQueueCreate/Send/Receive (src/libraries/Platform/src/platform_esp32.h:506-550).
- What it would buy: it removes a residual race. The loop could read the library's fwversion while a newer banner's version bytes are being written; OTGWSerial.cpp:1056 writes them before the NUL at :1048.
- Why the flag is the default instead:
  - processOT already reads the same statics loop-side (OTGW-Core.ino:5264-5268).
  - The race needs a complete second banner prefix (18+ chars, about 19 ms at 9600 baud) to arrive between the flag being set and consumed.
  - It self-corrects, because the newer banner re-arms the flag.
  - The flag needs less code and matches the existing pattern.
- When to switch: use the queue if a formally race-free handoff is wanted.

REPRO PLAN:
Nothing was run for this judgement: the session was read-only and a bench test was in progress.

A. STATIC OLD-VS-FIX (no hardware)
1. Implement the gate extension first.
2. Run `python evaluate.py --quick` on unmodified dev (a2a58ecf9). Expected: FAIL, naming fwreportinfo (OTGW-Core.ino:5780-5791).
3. Apply the fix and run it again. Expected: PASS.
4. Run `python tests/test_evaluate.py` with two fixtures:
   - failing: a callback body that writes state. or calls sendMQTT;
   - passing: a callback body that only sets a flag.

B. BENCH OLD-VS-FIX (classic-pic-bench)

Hardware and tooling:
- A Classic PCB with a live gateway PIC, plus the Classic-S3 bench board (COM8, USB flash only) running esp32-combo in PIC mode (esp32-classic also works).
- The bench MQTT broker.
- Telnet port 23, captured with scripts/capture-mqtt-debug.bat.
- The OTGW32 cannot reproduce this: HAS_PIC=0 there, and a combo in OTDirect mode parks the task.

Instrumentation (identical in OLD and FIX; test-only unless the maintainer keeps it):
- Two shims in src/libraries/Platform/src/platform_esp32.h. This is the only platform header on dev, so no ESP8266 stub is needed. ADR-120 forbids raw FreeRTOS calls in .ino files.
  - platformCurrentTaskName(), wrapping pcTaskGetName(nullptr);
  - platformTaskStackHighWater(PlatformTask), wrapping uxTaskGetStackHighWaterMark.
- A debug line "banner work on task=<name>" at the start of the banner handling:
  - OLD: at the top of fwreportinfo;
  - FIX: at the top of the loop consumer. In FIX the callback only bumps a volatile counter, which is printed loop-side. A Debug call in the callback would trip the new gate.
- A loop-side debug line with the picSerial high-water mark (g_picSerialTask), printed after each trigger.

Procedure per build:
1. Save the settings: `curl http://<ip>/api/v2/settings`.
2. Flash the app only: `flash_otgw.bat --board esp32-combo --update --app <bin>`. This keeps the settings; do not pass --fs.
3. Start the capture: `scripts\capture-mqtt-debug.bat -DeviceHost <ip> -BrokerHost <broker> -DurationSeconds 600 -Topic "<top>/value/<uid>/#"`.
4. Wait for MQTT to connect, then record the picSerial high-water mark.
5. Trigger (a): publish MQTT <top>/set/<uid>/resetgateway with payload 1, three times, at least 5 s apart (cooldown at MQTTstuff.ino:977-983).
6. Trigger (b): REST POST /api/v2/otgw/commands with {"command":"PR=A"}, three times.
   - Do not use telnet keys: they execute on async_tcp, and 'a' writes state.pic.* itself (handleDebug.ino:354-358).
   - Never flash the PIC.
7. After (b), read picfwversion, picdeviceid and picfwtype from /api/v2/device/info, and the retained otgw-pic/version, deviceid and firmwaretype at the broker.

Expected results:
- OLD: every trigger logs task=picSerial. The picSerial high-water mark drops after the first trigger with MQTT connected.
- FIX:
  - every trigger logs task=loopTask;
  - the callback counter rises;
  - the high-water mark shows no drop across the triggers;
  - the values after the PR=A-only trigger match the PIC.

Discriminator: the task name logged for the same trigger in OLD and FIX. Topic counts at the broker do not discriminate, because both builds publish.

Evidence: one transcript per build, named transcript-<host>-<id>-<hw>-<datetime>.txt, cited in the Final Summary.

Limits: a host test cannot reproduce the FreeRTOS task split. A harness that compiles OTGWSerial.cpp against stubs only proves that the library fires the callback synchronously on the reading thread. fwreportinfo lives in the single-TU sketch and cannot be linked into such a harness, so only the bench run proves the fix.

DISSENT / CORRECTIONS:
All three verifiers voted real, and none offered a checked refutation of the seam violation.

Verifier 2 refuted only the "two threads publishing corrupts the libraries" reading. espMqttClient (Helpers.h:16, MqttClient.cpp:160), AsyncSimpleTelnet (AsyncSimpleTelnet.h:159) and Wire all serialise internally. I accept that, and the task makes no such claim.

I also checked for a deadlock and found none:
- espMqttClient releases its mutex around onConnect and onMessage (MqttClient.cpp:510-512, :550-558).
- The park wait in resetOTGW() is bounded (OTGW-Core.ino:898-901).

SEVERITY: low.
- Verifier 1 said medium: an Accepted ADR is violated, the triggers are deterministic, and the byte-I/O task has an unbounded mutex wait.
- Verifiers 2 and 3 said low. I agree with them:
  - no crash path was found;
  - dropped publishes heal within 5 minutes (OTGW-firmware.ino:895-897, MQTTstuff.ino:1354);
  - the remaining races need narrow interleavings.
- It should rise if AC6 measures a near-exhausted picSerial stack, or if 1.x's cmdqueue-mutating fwreportinfo is ported before this fix lands.

CORRECTIONS TO INDIVIDUAL VERDICTS
- Verifier 3 said a dropped version publish "stays lost" for PR=A and diagnose banners. It does not: do5minevent and onMqttConnect republish it.
- Verifiers 1 and 2 proposed the telnet 'a' key as a trigger. That trigger is confounded:
  - telnet input runs on async_tcp (AsyncSimpleTelnet.h:129-130, :317-320, :401-406);
  - 'a' writes state.pic.* itself (handleDebug.ino:354-358).
  Verifier 3 was right to exclude it.
- Verifier 3 proposed editing ADR-130. It is Accepted and therefore immutable. The fix restores compliance with it, so no ADR change is needed.
- Verifier 2 called both boot banners certain and the boot publishes zero. Both points are timing-dependent and unverified: the banner from detectPIC's earlier reset, and whether MQTT connects after setup before the banner arrives. Verifier 1's wording holds.
- Verifiers 2 and 3 cited "banner +57ms" from the TASK-972 notes. That figure conflicts with the PIC source, which gives about 1 s:
  - gateway.asm:4738 (GW=R executes reset);
  - gateway.asm:627-629 (the reset vector calls SelfProg);
  - selfprog.asm:19-20 (SelfProg waits up to 1 s for STX).
  I did not rely on the 57 ms figure.
- Verifiers 1 and 2 included ESP8266 stubs in their instrumentation. dev has only src/libraries/Platform/src/platform_esp32.h.
- Line numbers have drifted since the verifiers' HEAD. The firmware sources are unchanged, but:
  - evaluate.py moved in 8874e410b: the gate is now at :3250, the docstring at :3262-3265, the regex at :3282-3283;
  - boards.h now lives at src/libraries/Platform/src/boards.h.
- I did not re-verify Verifier 3's stack estimate (about 2432 B of 4096 B).

2026-09-30 implementation (alpha.383), following the fix plan.
- AC#1: fwreportinfo() now only does (void)fw; (void)version; and, under #if HAS_PIC (the esp32 target has HAS_PIC=0 and never registers it), g_picBannerPending = true. The flag sits in the existing PIC flag block next to g_picRawDropPending.
- AC#2: applyPICBannerInfo() (loop-side, inside the #if HAS_PIC block) does the state.pic.sFwversion/sDeviceid/sType writes, the three debug lines and sendMQTTversioninfo(), reading the fields OTGWSerial already parsed (firmwareVersion(), processorToString(), firmwareToString()). reportPendingPICRxErrors() consumes the flag first thing, clearing it before applying. processOT's banner branch is unchanged. No OTStateLock was added: its documented contract is one writer site (processOT) over the decoded snapshot, which state.pic is not part of.
- AC#3: evaluate.py pic_task_callback_violations() (module level, called from check_pic_uart_task_owns_serial as the new ADR-130 result) resolves every registerFirmwareCallback(<fn>) across the firmware sources and FAILs on state., Debug*(, String, *ToString(, sendMQTT*, WebSocket, reportOTGWEvent*, feedWatchDog, cmdqueue/CmdQueue* in the comment-stripped body; upgrade callbacks are out of scope. On the real sources: OLD (HEAD before the fix) -> 6 violations (DebugTf(, DebugTln(, OTDebugTf(, ToString(, sendMQTTversioninfo, state.); FIX -> 0; the gate result 'PASS: [ADR-130] PIC-task firmware callback seam'. tests/test_evaluate.py TestPicTaskCallbackSeam (5 tests: pre-1175 body fails, flag-only body passes, cross-file definition, missing definition, upgrade callbacks out of scope): 71 tests OK.
- AC#7: comments corrected after checking the PIC source (other-projects/otgw-6.6/gateway.asm:5434-5435: PrintSettingA de 'A=' then GreetingStr 'OpenTherm Gateway '): the PR=A reply is 'PR: A=OpenTherm Gateway x.x', reaches handlePRresponse() (buf[2]==':') and is ignored as register A; the version is recorded through the firmware callback and the loop consumer; the unsolicited boot banner has no ':' at position 2 and reaches processOT()'s banner branch. Updated: getpicfwversion(), handlePRresponse() header and note, the processOT dispatch comment, OTGW-firmware.ino (the 60 s PR=A probe comment, which wrongly claimed the banner branch sets bAvailable on that path), and the evaluate.py docstring.
- AC#8: bin/bump-prerelease.sh alpha.382 -> alpha.383 in this commit; build.bat --target all: esp32 (HAS_PIC=0), esp32-classic and esp32-combo SUCCESS for firmware and filesystem (fresh 12:01-12:08, alpha.383+b2c418e, images under %LOCALAPPDATA%/OTGW-capture/img-alpha383-b2c418e); python evaluate.py (full) 81 passed / 4 warnings / 0 failed; tests/test_evaluate.py 71 OK. After the build only comments changed (verified with git diff); evaluate.py --quick afterwards 70 passed / 0 / 0.
OPEN: AC#4-#6 need the Classic-S3 PIC bench (task-name instrumentation, triggers, stack high-water mark).
Follow-up #1 of the description (the 60 s PR=A probe cannot set bAvailable) is filed as its own task.

2026-09-30: follow-up #1 is fixed in TASK-1179 (alpha.386): applyPICBannerInfo() now re-enables a PIC that boot detection missed (bAvailable = true, eMode = HW_MODE_PIC) before its version publish, so the 60 s PR=A probe recovers the PIC and otgw-pic/* is published. The processOT banner branch's board-mode persist was NOT copied: it would rewrite an S3 Mini Pro (mode 3) to 1 (TASK-1180).

2026-10-06: maintainer decision: wait for the Classic-S3 PIC rig (COM8, on its carrier); it is not attached now. Only the OTGW32 (COM4) is on the bench, which has no PIC.

2026-10-06: maintainer: there is no Classic-S3 board; stop waiting for it. Parked in To Do: the open ACs need a PIC on an ESP32-S3 Classic carrier, which the bench does not have (only the OTGW32, no PIC). Resume only when such a rig exists.
<!-- SECTION:NOTES:END -->
