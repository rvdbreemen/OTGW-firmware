---
id: "ADR-092"
title: "Keep a recovery route reachable when the HTTP heap gate refuses"
status: "Proposed"
date: "2026-08-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "madr"
topics:
  - "heap"
  - "ota"
  - "recovery"
  - "esp8266"
aliases:
  - "OTA unreachable under heap pressure"
  - "remote recovery route"
  - "telnet reboot command"
components:
  - "Recovery route under heap pressure"
symbols:
  - "canServeHttp"
  - "bESPactive"
  - "handleDebug"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-092 Keep a recovery route reachable when the HTTP heap gate refuses

## Status

Proposed, 2026-08-25.

## Status History

```yaml
status_history:
  - date: 2026-08-25
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context and Problem Statement

A device whose HTTP heap gate has engaged cannot be recovered remotely, and that includes the over-the-air (OTA) firmware update path. Every
route that could repair it runs behind the gate that the low-memory condition
just shut.

The chain, verified in source rather than assumed:

- `src/OTGW-firmware/OTGW-firmware.ino:424-448` runs the flash-upload handlers
  in an `if/else if/else` on `state.flash.bESPactive` / `bPICactive`. When
  either flag is set, that branch runs and the HTTP gate is genuinely bypassed.
- But `bESPactive` is set in exactly one place:
  `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h:286`, inside
  `_handleUploadStart()`. That is an upload callback of a route registered with
  `_server->on(path, HTTP_POST, ...)` (`:151`), an ordinary HTTP route on the same `httpServer`.
- Routes on `httpServer` only run from `httpServer.handleClient()`, and that
  call is gated by `canServeHttp()` (`OTGW-firmware.ino:444`).

So the in-code comment at `OTGW-firmware.ino:439-441`, "Flash-upload handlers
(handleEsp/PicFlashBackgroundTasks) are NOT gated", is true of a flash already
**in progress** and false of **starting** one. Once the gate is shut, an upload
can never begin, so the ungated continuation is unreachable.

ADR-091 did not change this and was not meant to. Its reaper releases pending
connections rather than serving them, so an upload request arriving during a gated
window is closed rather than accepted. That was the right call for the latch and
it leaves this gap exactly as it was.

The naive repair is closed off. Letting the ordinary pump run below the gate
would execute the multipart parser, which performs an unchecked contiguous
allocation of roughly 2100 bytes (`ESP8266WebServer/src/Parsing-impl.h:475`,
`HTTP_UPLOAD_BUFLEN` 2048 plus struct overhead). That is larger than
`HTTP_SERVE_MIN_MAXBLOCK` itself, so there is no block size below the gate at
which accepting an upload is safe.

What survives the gate is telnet. `debugTelnet.loop()` runs in the same block,
before the gate (`OTGW-firmware.ino:431`), and ADR-079 deliberately keeps telnet
clients connected during a heap incident so an operator can watch. An operator
can therefore still reach a gated device and read its state. What they cannot do
is act on it: the telnet command set in `src/OTGW-firmware/handleDebug.ino`
contains no reboot, and a search for `ESP.restart` / `ESP.reset` in that file
returns nothing. The one recovery-shaped command, `'r'` at `:179`, only
reconnects WiFi and only when WiFi is already down.

This is worth deciding now rather than when it bites, because the condition that
strands a device is exactly the condition in which someone wants to flash a fix
onto it.

## Decision Drivers

* A device that cannot be recovered remotely has to be recovered physically,
  which for a gateway wired to a boiler is a service call.
* Whatever route is added must be safe precisely when memory is scarce, which
  rules out anything that allocates in order to work.
* Telnet is already reachable in this state and already trusted: ADR-032 places
  the device on a trusted local network with no authentication, and ADR-079
  keeps telnet alive during an incident by design.
* A fresh boot resolves the condition outright. Whatever leaked or fragmented is
  gone, and the device comes back with its full heap.
* The firmware already reboots itself deliberately in other circumstances
  (`settings.bNightlyRestart`, `settings.iRestartHour`), so a remote reboot is
  not a new class of behaviour.

## Considered Options

* **Option A — Add a reboot command to the telnet console.** One command on a
  path that is already reachable and already ungated. The operator reboots the
  device and then flashes it normally, because a freshly booted device has a
  healthy heap and an open gate.
* **Option B — Give the OTA upload its own entry point below the gate.** Accept
  a firmware image through a route that does not run the multipart parser, so
  flashing works while the gate is shut.
* **Option C — Reboot automatically after sustained CRITICAL.** No operator
  involved: if the device stays critical for long enough, restart it.
* **Option D — Do nothing.** Accept that a gated device needs physical access.

## Decision Outcome

Chosen option: **Option A**.

It is the smallest change that removes the actual harm, and it is safe in the
state it has to work in: sending a reboot allocates nothing.

Option B was rejected on cost and on risk. It would need a body-consuming path
that avoids the 2100-byte parser allocation, which means either a second upload
implementation or a raw-stream reader, both of which are substantial new code on
the exact path where failure bricks the device. It also privileges OTA
specifically, when the general problem is that no *action* is reachable, not
that OTA is missing.

Option C was rejected as a first step, not on principle. An automatic reboot
hides the condition instead of surfacing it, and this firmware has just spent a
release cycle learning how much a silent recovery costs: TASK-1037's leak was
diagnosable only because the device stayed up long enough to be captured. A
reboot loop would have destroyed that evidence. It stays available as a later
decision if operators report that manual recovery is not enough.

## Decision Contract

### Must

* Keep the recovery route free of allocation, so it works in the state it exists
  for.
* Route the reboot through the existing deferred-reboot mechanism rather than
  calling `ESP.restart()` inline, so it cannot fire mid-flash. That mechanism
  already guards on `isFlashing()`.
* Log the reboot and its reason before it happens, so a field capture shows an
  operator-initiated restart rather than an unexplained gap.

### Must Not

* Reboot automatically on heap state. This decision adds an operator action, not
  a policy.
* Accept a firmware upload below the HTTP gate. The parser's unchecked 2100-byte
  contiguous allocation exceeds the gate threshold, so there is no safe band.
* Drop telnet during a heap incident in order to free memory. ADR-079 rejected
  that for its own reasons, and this decision now depends on telnet staying up.

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/handleDebug.ino` — the new command in the telnet command
  table.
* `src/OTGW-firmware/helperStuff.ino` — the deferred-reboot mechanism it calls.

## Consequences

### Positive

* A stranded device can be recovered without physical access: reboot over
  telnet, then flash normally once it is back with a healthy heap.
* Nothing about the gate, the thresholds or the upload path changes, so the
  change cannot regress the fragmentation behaviour TASK-841 addressed.
* The command is useful outside this scenario. There is currently no way to
  restart the device remotely at all except by waiting for the nightly restart.

### Negative

* **It requires a human.** The device does not rescue itself, so an unattended
  installation stays stranded until someone notices. That is deliberate, but it
  is a real limitation and Option C exists precisely for it.
* **It assumes telnet is reachable**, which is true while WiFi is up and the
  loop still runs. A device that has also lost WiFi is not helped by this, and
  neither is one whose loop has stopped.
* Any client on the trusted network can reboot the gateway. Consistent with
  ADR-032's trust model, but it is a new remote effect and worth stating rather
  than discovering.

### Neutral

* The OTA path is unchanged. This decision routes around the gap rather than
  closing it, and Option B remains available if a case for it appears.

## Open Questions

- [ ] Should the reboot command require a confirmation keystroke? Every other
      telnet command is a single character with an immediate effect, so a
      confirmation would break that convention, but rebooting a boiler gateway
      by mistyping one letter is a different order of consequence from toggling
      a debug flag.
- [ ] Should the same action be exposed over the message-queue telemetry transport (MQTT) as well? Home Assistant users
      are more likely to have MQTT than a telnet client to hand, and a command
      topic already exists. Against: MQTT is precisely what a heap-pressured
      device may have stopped servicing, so it is the less reliable of the two.

## Related Decisions

* **ADR-091 (A heap refusal must not suppress the cleanup path it depends on)**:
  the gate this decision routes around. ADR-091 made the gate able to reopen;
  it did not make a shut gate recoverable, which is this gap.
* **ADR-079 (Emergency Heap Recovery Actions)**: keeps telnet connected during a
  heap incident. This decision depends on that and must not be read as licence
  to revisit it.
* **ADR-032 (No Authentication on the Local Network)**: the trust model under
  which an unauthenticated telnet command may reboot the device.

## References

* TASK-1089 — the finding this decision resolves.
* TASK-1039 — the latch fix that surfaced it.
* `src/OTGW-firmware/OTGW-firmware.ino:424-448` — the gated loop block and the
  flash-active branch.
* `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h:286` — the single place
  `bESPactive` is set, inside an upload callback of a gated route.
* `src/OTGW-firmware/OTGW-ModUpdateServer-impl.h:151` — that route's
  registration on `httpServer`.
* `src/OTGW-firmware/handleDebug.ino:179` — the `'r'` command, the closest thing
  to a recovery action that exists today.
* `ESP8266WebServer/src/Parsing-impl.h:475` — the unchecked 2100-byte allocation
  that rules out Option B's simple form.
