---
id: TASK-1146
title: >-
  Fix: port 25238 discards the first command after connect, breaking the HA
  opentherm_gw integration
status: Done
assignee:
  - '@claude'
created_date: '2026-09-21 18:58'
updated_date: '2026-09-24 16:41'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/685'
priority: high
ordinal: 225000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Home Assistant's built-in opentherm_gw integration (pyotgw 2.2.3) cannot connect to port 25238 on 1.7.5: it fails with cannot_connect because the first command it sends is thrown away. Reported by petrister in GitHub #685 with a pyotgw debug capture and a confirmed root cause.

pyotgw writes PS=0 about 20 ms after the TCP handshake and waits for 'PS: 0'. The server only accepts a client when loop() runs _acceptNewClients(), so by the time SimpleTelnet::_connectClient() runs, that command is already sitting in the receive buffer. _connectClient() calls _drainClient(), which discards every pending RX byte, so handleOTGW() never sees the command and the PIC never replies. Raw T/B frames still stream in on the same connection, which is why the port looks alive. A client that waits ~1 s after connecting before writing is always answered; writing immediately is answered only sometimes.

The drain exists to flush telnet negotiation, which is meaningful for the debug console on port 23 but harmful for the port-25238 passthrough, where clients speak the raw OTGW serial protocol.

Confirmed in code: src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:421 (_connectClient) and :570-572 (_drainClient discard loop), pinned submodule cc4c88e.

Scope note: the fix lands inside the vendored SimpleTelnet submodule. There is no firmware-side escape, because the drain sits in _connectClient and startOTGWstream() has no hook to skip it. This needs maintainer approval for a vendored-library change, and landing it means a commit in the SimpleTelnet repo plus a submodule pointer bump here. The same defect exists on the 2.0.0 line (dev worktree, SimpleTelnet pin 123106f, same call at SimpleTelnet_impl.tpp:281), so a sibling task is expected there.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A client that writes a command immediately after the TCP connect on port 25238 gets its reply; the command is no longer discarded
- [x] #2 Home Assistant's opentherm_gw integration completes setup against socket://<ip>:25238 without cannot_connect
- [x] #3 The telnet debug console on port 23 still discards telnet negotiation on connect (no regression)
- [x] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [x] #5 petrister confirms on a beta build that the HA integration connects
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. SimpleTelnet repo: branch from cc4c88e, backport the _drainClient() half of upstream a909731 (delete the inbound-discard loop, keep the outbound flush). Skip the _releaseSlot()/_flushTx() half - it lives in AsyncSimpleTelnet.h, which does not exist at cc4c88e. Commit and push.
2. Bump the 1.x submodule pointer to the new commit, commit in the superproject.
3. Verify: python build.py --firmware exits 0; python evaluate.py --quick shows no new failures.
4. On-device on the bench 1.x ESP8266 (192.168.88.68) via web OTA: a socket client that writes immediately after connect gets its reply on 25238; port 23 console still behaves.
5. Ask petrister on GH #685 to confirm the HA opentherm_gw integration connects.
6. 2.0.0: verification only. Append a note to TASK-1145 that the adopted commit also closes #685; do not touch its AC list.

Rejected alternative: bump 1.x straight to origin/main. cc4c88e is an ancestor so it is mechanically clean, but the delta is the library 2.0.0 rewrite (+3261/-674, new SimpleTelnetCore.h, async transport, RFC 854 negotiation). Two verified blockers: (a) upstream defaults to NEG_REFUSE which strips IAC, breaking byte transparency on the raw 25238 port - 2.0.0 compensates with setTelnetNegotiation(NEG_OFF) at OTGW-Core.ino:5563, so a 1.x bump would need the same; (b) ESP8266 Core 2.7.4 compatibility of the rewrite is unverified, and cc4c88e exists specifically for dual-target WiFiServer accept()/available().

Safety verified at the 1.x pin rather than inherited from upstream: port 25238 runs streaming mode (_onInput == nullptr) so nothing filters it, which is correct for a raw port. Port 23 is setLineMode(false), so _handleCharInput passes every byte and the >=0x80 guard in _handleLineInput is never reached - upstream's _filterByte premise does not transfer. Checked the consumer instead: handleDebugChar()'s switch ends in 'default: break;' (handleDebug.ino:284), so stray IAC bytes are silently ignored.

Residual accepted: a telnet option byte could coincidentally equal a command char (all commands >=0x20); standard clients negotiate options <=39, colliding with none. The proper fix is upstream's IAC state machine, reachable only via the full bump.

Completion: AC #5 is not self-verifiable, so the task stays In Progress with that blocking AC named in the Final Summary once everything else is green.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
On-device verification on the bench 1.x ESP8266 (192.168.88.68, PIC gateway 6.8), firmware-only web OTA the XHR way.

Reproduced first on the OLD build (1.7.6-beta.4+014d380), which is what makes the after-figure mean something. Same script, four write delays after connect, 8 trials each:
- zero-delay 5/8 answered, 1ms 7/8, 5ms 8/8, 20ms 8/8
- the misses returned raw R00000000 frames, matching the report that the port looks alive while the command is gone
- an earlier attempt at a flat 20ms delay did NOT reproduce, because by then accept() has already run and drained an empty buffer. The race only opens when the bytes land before the first loop() pass.

After flashing the fix (+b6b3c98): 8/8 at every delay, 32/32 total, plus a later 10/10 immediate run. 42/42 with zero misses.

Port 23 regression check: banner delivered (1631 bytes) and the h command answered in three sessions, including two where a real telnet clients IAC DO/WILL burst was written at zero delay so it landed before accept. No spurious command fired, which is the handleDebugChar default: break path behaving as the plan predicted.

AC #2 verified with the real client rather than deferred: pyotgw 2.2.3, the exact version from the report, against socket://192.168.88.68:25238. 3/3 connects succeeded, no cannot_connect.

One honest caveat, not claimed as fixed: each 3-attempt pyotgw run logs exactly 2 "Timed out waiting for command: PS, value: 0" lines during init. The count is stable across runs, unlike the random misses of the discard bug, and it never prevents the connection. This bench unit has no boiler or thermostat attached (thermostatconnected false), which is a plausible cause. Out of scope for #685; flagging rather than burying it.

Device health after the runs: no crashlog, lastreset Software/System restart from the OTA only, bootcount stable at 2, heap ~18 KB, MQTT connected.

Wording drift on AC #3, flagged rather than silently accepted. It reads "the telnet debug console on port 23 still discards telnet negotiation on connect (no regression)". After this fix the console deliberately does NOT discard any more, so the literal text is false while the intent, no regression on port 23, is met and verified. Checked against the intent. If the wording matters for the record it should be reworded to "the telnet debug console on port 23 is unaffected (no regression)".

Resolved the pyotgw PS-timeout caveat rather than leaving it open. Probed the commands directly on the fixed build:
- PS=1 -> "PS: 1" followed by an all-zero summary line
- PS=0 -> "PS: 0"
- PR=A -> "PR: A=OpenTherm Gateway 6.8"

All three answer correctly. The summary is all zeros because this bench unit has no boiler and no thermostat, so pyotgw waits during init for values an empty bus never produces. Not a firmware defect and not a residue of the discard bug. No follow-up task opened; noted on GH #685 so the question is not left hanging.

Filesystem deliberately NOT flashed, recorded so nobody assumes a full-image verification happened. The fix commit touches CHANGELOG.md and the SimpleTelnet submodule only, zero files under src/OTGW-firmware/data/, so the LittleFS image is byte-identical in content to the one the unit already runs. Flashing it would also wipe /otgw_simulation.log, which the bench needs for silent-bus tests. Firmware-only OTA was the right scope here; a beta release still ships both, as the release notes always state.

2026-09-22: petrister replied on GH #685. He will flash the beta once published, re-enable port 25238 and add the opentherm_gw integration on HA 2026.8.3, then report back. AC #5 stays blocked until a beta carrying 3ef40d692 is actually released.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixes the port 25238 defect behind GH #685: the first command a client sent after connecting was discarded, so Home Assistants opentherm_gw integration failed with cannot_connect.

## Cause

SimpleTelnet::_drainClient() read and threw away every pending inbound byte at accept, justified as flushing telnet negotiation. The server only accepts a client on the next loop() pass, so a client that pipelines a command with connect() had that command waiting in the receive buffer when the drain ran, and lost it. pyotgw writes PS=0 about 20 ms after the TCP handshake and waits for PS: 0, which never came, while raw OpenTherm frames kept streaming on the same connection and made the port look healthy.

The loop was unconditional and could not tell an IAC sequence from payload. Port 25238 runs in streaming mode (_onInput == nullptr), so _processInput() never runs there and the drain was the only thing touching those bytes, on a port that speaks no telnet at all.

## Change

One change, in the vendored SimpleTelnet submodule: _drainClient() now flushes outbound only. Backport of upstream a909731 onto the 1.x line, which predates the librarys 2.0.0 rewrite. Only the _drainClient half applies; the _releaseSlot/_flushTx half lives in AsyncSimpleTelnet.h, which does not exist on this line.

- SimpleTelnet: branch fix/no-discard-at-accept-1x, commit a297bf4, pushed.
- Firmware: submodule bump plus CHANGELOG, commit 3ef40d692, pushed to origin/otgw-1.x.x.

Rejected the alternative of bumping straight to origin/main. cc4c88e is an ancestor so it is mechanically clean, but the delta is the 2.0.0 rewrite (+3261/-674). Two verified blockers: upstream defaults to NEG_REFUSE which strips IAC and would break byte transparency on the raw port (2.0.0 compensates with setTelnetNegotiation(NEG_OFF) at OTGW-Core.ino:5563), and ESP8266 Core 2.7.4 compatibility of the rewrite is unverified.

## Verification

Bug reproduced on the old build first, then re-tested after flashing, same script:

| write delay | before (+014d380) | after (+b6b3c98) |
|---|---|---|
| zero-delay | 5/8 answered | 8/8 |
| 1 ms | 7/8 | 8/8 |
| 5 ms | 8/8 | 8/8 |
| 20 ms | 8/8 | 8/8 |

Plus a later 10/10 immediate run: 42/42 with zero misses after the fix.

pyotgw 2.2.3, the exact version from the report, connects 3/3 with no cannot_connect. Port 23 unaffected: banner and h command answered in three sessions, two of them with a real telnet clients IAC burst written at zero delay so it landed before accept; no spurious command fired.

Build green (1.7.6-beta.4+b6b3c98, firmware and filesystem). Evaluator 38 checks, 0 failures.

## Risks and follow-ups

- Accepted residual: a telnet option byte could coincidentally equal a console command char (all commands are >= 0x20). Standard clients negotiate options <= 39, colliding with none. The proper answer is upstreams IAC state machine, reachable only via the full bump.
- Unrelated observation, not fixed and not claimed: each 3-attempt pyotgw run logs exactly 2 PS command timeouts during init. Stable count, never blocks the connection, and this bench unit has no boiler or thermostat attached.
- The 2.0.0 line already carries this fix via its own adoption of a909731; no sibling task was needed.

## Blocking AC

AC #5 requires petrister to confirm on a beta build. Not self-verifiable, so the task stays In Progress until he reports back on GH #685.

Field confirmation (AC #5): petrister on GH #685, 2026-09-23 18:13 UTC, on v1.7.6-beta.4 with HA 2026.8.3 / pyotgw 2.2.3: the opentherm_gw config flow completed on the first attempt, 187 entities loaded, live climate values, no errors in the HA log. Hardware: Nodo OTGW, PIC gateway 6.8, Intergas Kombi Kompakt HRE 28/24 A, Honeywell T87M. Issue closed by the maintainer 2026-09-24.
<!-- SECTION:FINAL_SUMMARY:END -->
