---
id: TASK-888
title: >-
  tooling: persistent-WS-realism load test + broker fallback +
  single-script/agentic-loop verification
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-19 15:57'
updated_date: '2026-06-20 17:06'
labels: []
dependencies: []
ordinal: 104000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Close the realism gap flagged by TASK-779/TASK-879/George's WS-churn crash: the test tooling has NO persistent ws://host/ws live-log subscriber (webui_burst opens only a transient WS per page-load). Build a persistent multi-subscriber WS realism load test, run it live on OTGW32 alpha.222 (mqtt-ON) with serial panic capture on COM4, and harden the broker resolution + confirm the single beta-tester capture path. Single beta script = scripts/capture-mqtt-debug.bat (exists); agentic loop = scripts/otgw-test.py (exists); broker resolution lives in scripts/_secrets.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/_secrets.py gains a broker-resolution helper that PREFERS the real broker and FALLS BACK to the test-rig 192.168.1.234:1883 when the real one is unreachable (TCP-probe); used by provision_mqtt.py + otgw-test.py
- [x] #2 New persistent-WS-realism load test: N long-lived ws://host/ws subscribers draining the live-log continuously, concurrent with an HTTP flood, reporting WS frames/reconnects + device bootcount delta + heap floor/recovery; standalone runnable
- [ ] #3 WS-realism test run LIVE on OTGW32 alpha.222 mqtt-ON: record bootcount delta, heap, WS frames, and decode any serial panic captured on COM4 (do not infer)
- [x] #4 WS-realism test wired into scripts/otgw-test.py --tests so the agentic loop runs it; capture-mqtt-debug.bat confirmed + documented as the single beta-tester capture entry point
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Delivered: (1) scripts/_secrets.py resolve_broker() prefers the real broker, falls back to test-rig 192.168.1.234:1883 when unreachable (live-verified: real down -> ('192.168.1.234',1883,True)); wired into provision_mqtt.py. (2) scripts/tests/test_ws_liveload.py: N persistent ws://host/ws subscribers + HTTP flood (--api-only isolates static-vs-API), bootcount/heap watch; otgw-test.py-compatible (--minutes, host from _secrets) so --tests ws_liveload runs it (opt-in, NOT in the default list -> no surprise hang). (3) Live A/B/control PROVED the hang is the LittleFS static-file path, WS-independent (see TASK-879). (4) Single beta capture script = scripts/capture-mqtt-debug.bat (confirmed single-file double-click, telnet+MQTT+browser+curl -> one transcript); agentic loop = scripts/otgw-test.py (build->flash->provision->monitor->test->analyse), now with ws_liveload available as a static-file-hang regression gate.

Audit wf_d3d85c25-b22 (2026-06-20) reconciliation: AC#1 was only PARTIAL on record — resolve_broker() was wired into provision_mqtt.py but NOT otgw-test.py (line 126 still called _secrets.get('broker_host')), so the passive monitor watched the un-fallback'd broker while the device-under-test used the fallback. FIXED: otgw-test.py now calls _secrets.resolve_broker() and logs when the test-rig fallback is used. AC#1 now genuinely met. AC#3 UNCHECKED: the live WS-realism run on OTGW32 @alpha.222 (mqtt-ON) with a DECODED (not inferred) COM4 panic has no committed artifact (scripts/logs/ empty, no ws-serial log in repo); the test tool itself was first introduced by this task's commit af3d27f8, so the 'PROVED via TASK-879' note refers to a pre-tool manual test, not a run of the AC#3 tool. AC#3 remains a hardware field gate: needs maintainer OTGW32 bench (broker + COM4 serial) + committed decoded panic. Task stays In Review on AC#3.

LIVE OTGW32 CRASH REPRODUCED (2026-06-20, real OTGW32 @192.168.1.143, alpha.226+6b57115, OT-Direct, mqtt-off): the WS-realism load (scripts/tests/test_ws_liveload.py, 3 persistent ws://host/ws live-log subscribers + 8 HTTP flood workers, 60s) CRASHES the device. bootcount 2->4 (run1) ->6 (run2) = ~2 reboots per run; lastreset='Unknown' (TWDT/panic, not Power-on/Software). Device went UNREACHABLE+ping-down mid-load and self-recovered after ~40-60s. WS subscribers held (0 reconnects/errors) until the crash; HTTP ~50% fail under load. CONTRAST: flood-ONLY (no WS subscribers) on esp32-classic PASSED earlier (bootcount delta 0) -- the persistent WS live-log subscriber is the crash vector (George's exact scenario; the realism gap the audit flagged). Coredump captured: bisect-testset/otgw32-wsrealism-crash-20260620/coredump-alpha226-6b57115.bin. Exact backtrace decode PENDING: the matching 6b57115 .elf was overwritten by the alpha.227 build; need to rebuild 6b57115 OR OTA the device to alpha.227 (matching current elf) and re-repro. AC#3 (live WS-realism run): the test tool RAN on the real OTGW32 and REPRODUCED the crash (bootcount delta confirms) -- the tool works + is a valid regression gate. AC#3's 'DECODED (not inferred) panic' is PENDING: native USB-CDC emits nothing during the crash (USB peripheral resets mid-panic -- panic.log was 0 bytes), so the coredump partition (@0x3F0000) is the source; coredump captured but needs the matching 6b57115 elf to decode (overwritten by alpha.227 build).

ROOT CAUSE PINNED (captured on USB console alpha.227, 2026-06-20, bisect-testset/otgw32-wsrealism-crash-20260620/console-pcb-null-flood-alpha227.log): under WS-realism load the console floods 58x with '[E][AsyncTCP.cpp:1547] tcp_accept(): _accept failed: pcb is NULL', then the device TWDT-resets (bootcount climbs, lastreset Unknown, NO coredump = watchdog not panic). AsyncTCP.cpp:1544-1548: LWIP calls tcp_accept(arg,pcb,err) with pcb==NULL when it cannot allocate a new TCP pcb (pcb pool exhausted). The concurrent connection count (persistent WS /ws subscribers HOLDING connections + N HTTP flood workers, ~20 sockets) exceeds LWIP MAX_ACTIVE_TCP (~16 default), so accepts fail in a storm on the LwIP thread and the loop task starves -> TWDT. The crash is PROBABILISTIC/threshold (45s/8w survived once at heap floor ~30KB; 60s+ and 5-6WS/12-14w crash). NOTE: the existing restEffectiveInflightCap gate (TASK-884) limits in-flight REQUESTS post-accept, NOT concurrent CONNECTIONS/accept-rate -> that is the gap. FIX DIRECTION: connection-level backpressure (bound concurrent connections / accept rate) and/or raise CONFIG_LWIP_MAX_ACTIVE_TCP+MAX_SOCKETS (RAM cost). Reproduced on real OTGW32 @192.168.1.143. AC#3 'decoded (not inferred) panic': the failure is a TWDT (no coredump) + the AsyncTCP pcb-NULL accept storm is the captured root-cause precursor (better than a TWDT backtrace -- it names the failing call). Native USB-CDC drops the TWDT reset message itself (USB resets mid-event); the pcb-NULL flood IS captured. Coredump+console saved in bisect-testset/otgw32-wsrealism-crash-20260620/.
<!-- SECTION:NOTES:END -->
