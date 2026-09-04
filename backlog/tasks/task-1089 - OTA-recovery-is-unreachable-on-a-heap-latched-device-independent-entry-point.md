---
id: TASK-1089
title: >-
  OTA recovery is unreachable on a heap-latched device (independent entry
  point?)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-25 18:53'
updated_date: '2026-09-04 06:41'
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
- [ ] #2 A reboot can be triggered from the telnet console, which stays reachable when canServeHttp() is refusing, and it routes through the deferred-reboot mechanism rather than calling ESP.restart() inline
- [ ] #3 Verified on the bench: the command appears in the console help, triggers the reboot with its reason logged before it happens, and the device comes back
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
<!-- SECTION:NOTES:END -->
