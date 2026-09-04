---
id: TASK-1089
title: >-
  OTA recovery is unreachable on a heap-latched device (independent entry
  point?)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-25 18:53'
updated_date: '2026-09-04 06:42'
labels:
  - bug
  - adr-required
dependencies: []
priority: medium
ordinal: 188000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while resolving TASK-1039. The flash-upload handlers are reached through the same gated call as ordinary serving: OTGW-firmware.ino runs handleEsp/PicFlashBackgroundTasks in an if/else-if/else on bESPactive/bPICactive, and those flags are set by the upload handler, which is itself only reachable through canServeHttp(). The comment at helperStuff.ino claims the flash-upload handlers are NOT gated; that overstates the guarantee.

Consequence: a device whose HTTP gate has engaged cannot be recovered over the air. TASK-1039's reaper stops the gate latching shut, but it does not change this: it releases the pending connection rather than serving it, so an upload POST arriving during a gated window is closed rather than accepted.

This is deliberately NOT folded into ADR-091. That decision records one rule, that a refusal must not suppress its own cleanup path. Privileging OTA under heap pressure is a different promise with different trade-offs (you want that path favoured, not bounded), and merging them would blur both.

Needs its own ADR before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Whether OTA needs an entry point independent of the HTTP heap gate is decided and recorded in its own ADR
- [x] #2 A reboot can be triggered from the telnet console, which stays reachable when canServeHttp() is refusing, and it routes through the deferred-reboot mechanism rather than calling ESP.restart() inline
- [x] #3 Verified on the bench: the command appears in the console help, triggers the reboot with its reason logged before it happens, and the device comes back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-25: ADR-092 drafted (Proposed) covering this. Recommends Option A, a telnet reboot command, over giving OTA its own entry point below the gate.

Reasoning, all source-verified: bESPactive is set in exactly one place, _handleUploadStart at OTGW-ModUpdateServer-impl.h:286, which is an upload callback of an ordinary HTTP route registered at :151 on httpServer. Routes on httpServer run only from handleClient(), which canServeHttp() gates. So the loop comment claiming the flash handlers are not gated is true of a flash in progress and false of starting one.

Telnet survives the gate: debugTelnet.loop() runs before it, and ADR-079 keeps telnet clients connected during an incident by design. So an operator can already reach a gated device; what they cannot do is act, because handleDebug.ino has no reboot command and no ESP.restart call anywhere in it. The nearest thing, 'r' at :179, only reconnects WiFi and only when WiFi is already down.

Option B (an OTA entry point below the gate) was rejected: it needs a body path avoiding the parser's unchecked 2100-byte contiguous allocation, which exceeds HTTP_SERVE_MIN_MAXBLOCK, so it means a second upload implementation or a raw-stream reader on the exact path where failure bricks a device.

Option C (auto-reboot on sustained CRITICAL) was rejected as a first step and kept as a later option. TASK-1037's leak was diagnosable only because the device stayed up long enough to be captured; a reboot loop would have destroyed that evidence.

Blocked on maintainer acceptance of ADR-092. Two open questions in it, both for the maintainer: whether the command needs a confirmation keystroke against the single-character convention, and whether the same action should also exist over MQTT.

2026-08-25: ADR-092 Accepted after grilling, so implementation is unblocked. Two settled points that bind the work:

Single key, no confirmation, consistent with every other telnet command. handleDebugChar reads one character and dispatches immediately, and the console has no confirmation pattern anywhere, so a two-key sequence would be its only exception. The mistype risk is bounded: a reboot preserves settings, the device returns in seconds, and the deferred-reboot mechanism refuses to fire while isFlashing(), which is the one case where an accidental restart would do real harm.

Telnet only. MQTT is a separate decision, tracked separately. Note the reason changed during grilling: the draft argued MQTT is unreachable under heap pressure, which is only half true. canPublishMQTT() gates publishing, but handleMQTT() runs in the same loop branch as telnet and before the HTTP gate, so an inbound command would most likely still arrive. The real argument is scope, since an MQTT reboot is a new external effect on a channel shared with a broker.

Implementation per the Decision Contract: route through the existing deferred-reboot mechanism rather than calling ESP.restart() inline, and log the reboot and its reason first so a field capture shows an operator-initiated restart rather than an unexplained gap.

2026-08-25: ADR-092 Accepted. Telnet reboot only. The MQTT variant is closed, not deferred: a follow-up task was opened and dropped the same day because the maintainer saw no reason to spend time on a second channel for an action that already has one. ADR-092's answered open question was corrected to say so, with maintainer authorisation recorded in its Status History. Remaining work here is AC#2/AC#3, which ADR-092 reshapes: no upload is accepted below the gate (Option B was rejected), so AC#2 becomes 'a single-key telnet command reboots a gated device through the existing deferred-reboot mechanism' and AC#3 verifies that on the bench.

2026-09-04: implemented ADR-092 Option A and replaced the two stale acceptance criteria.

ACs #2 and #3 as written described Option B, giving OTA its own entry point below the heap gate. ADR-092 answered AC #1 with a NO on exactly that option: it would need a body-consuming path avoiding the 2100-byte parser allocation, which means a second upload implementation or a raw-stream reader, both substantial new code on the path where failure bricks the device. Checking criteria that describe a rejected design would have been false, so they were removed and replaced with criteria for the option that was actually accepted. AC #1 is unchanged and still checked.

Implementation: handleDebug.ino gains 'R', which calls requestDeferredReboot("telnet operator request"). Uppercase because lowercase 'r' is the WiFi and MQTT reconnect immediately above it. Deferred rather than inline per the Decision Contract: loop() calls performDeferredReboot() only when !isFlashing(), so a reboot requested mid-flash waits rather than bricking the device, and it fires outside the console callback so the acknowledgement has left the socket. requestDeferredReboot() stores a const char* and logs; it allocates nothing, which is the property that matters on a starved heap.

Bench verification on 192.168.88.68 running 1.7.6-beta.1+24eb2ef:
  h -> "R) Reboot the ESP (deferred; waits if a flash is running)"
  R -> handleDebugC(211): Reboot requested from the telnet console
       requestDefer(499): [reboot] deferred request: "telnet operator request" heap=17328 minHeap=16832 maxBlk=16424 frag=6 flashing=0
       performDefer(506): [reboot] performing deferred reboot after 3ms defer
       doRestart(639): [reboot] doRestart("telnet operator request") begin
       doRestart(654): [reboot] calling ESP.restart() after 2019ms total
Device came back and answered /api/v2/device/info.

One correction made before shipping: the first draft of the comment said doTaskEvery1s() services the deferred reboot. It is loop() (OTGW-firmware.ino:531), which also means the reboot fires at full loop rate rather than up to a second later.

Not verified, and deliberately not claimed: that the command works while the gate is actually engaged. Holding a device below the contiguous-block threshold on demand is not something this bench can do reliably. The argument rests on debugTelnet.loop() running before the gate in doBackgroundTasks() and on requestDeferredReboot() allocating nothing, both source-verified.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A stranded gateway can now be recovered without physical access.

Everything served on httpServer runs from handleClient(), which canServeHttp() withholds under heap pressure, so a device whose gate had engaged could be neither flashed nor rebooted over HTTP. Telnet stays reachable by design (ADR-079) and its loop runs before the gate, but the console had no way to act: no reboot command and no ESP.restart call anywhere in handleDebug.ino.

The telnet console gains 'R', which requests a deferred reboot. It routes through the existing mechanism rather than calling ESP.restart() inline, so a reboot asked for mid-flash waits instead of bricking the device, and it allocates nothing, which is the property that matters in the state it exists for.

This implements ADR-092 Option A. Option B, giving OTA its own entry point below the gate, was rejected in that ADR on cost and risk, so the two acceptance criteria describing it were replaced rather than checked.

Verified on the bench: the command appears in the help, logs its reason before acting, reboots 3 ms after the request, and the device comes back. Full log in the implementation notes.
<!-- SECTION:FINAL_SUMMARY:END -->
