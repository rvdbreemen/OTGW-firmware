---
id: "ADR-097"
title: "Allow two writers on port 25238 by forwarding each command line to the PIC whole"
status: "Proposed"
date: "2026-09-26"
binding: false
gate: "OTGW_NET_LINE_ATOMIC"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "madr"
topics:
  - "serial-bridge"
  - "ser2net"
  - "port-25238"
  - "multi-client"
  - "command-framing"
aliases:
  - "otmonitor port"
  - "two clients on 25238"
  - "command splicing"
components:
  - "OTGWstream inbound path (port 25238 to PIC)"
  - "SimpleTelnet per-slot read API"
symbols:
  - "OTGW_NET_LINE_ATOMIC"
  - "OTGWstream"
  - "availableFrom"
  - "readFrom"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-097 Allow two writers on port 25238 by forwarding each command line to the PIC whole

## Status

Proposed, 2026-09-26.

## Status History

```yaml
status_history:
  - date: 2026-09-26
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context and Problem Statement

Port 25238 is the raw serial bridge to the PIC (Peripheral Interface
Controller, the microcontroller that runs Schelte Bron's OpenTherm gateway
firmware). OTmonitor, the Home Assistant `opentherm_gw` integration, Domoticz and
similar tools connect to it and both read the PIC's output and send it commands
such as `PR=A` or `TT=21.5`.

Until v1.7.5 the port accepted two clients. v1.7.5 (TASK-1115) reduced it to one,
because the bridge library, SimpleTelnet, has no per-client read identity:
`available()` and `read()` serve whichever slot holds data
(`src/OTGW-firmware/OTGW-Core.h:20-28`). With two clients writing at the same
moment, the bytes of both were interleaved into one stream toward the PIC, so
half of one client's command could be concatenated with another's into a
malformed command. The release note said that one writer plus several readers was
the way back if people needed it.

People need it. On 2026-09-26 a user (iandury_, Discord
#nederlandse-ondersteuning) reverted to v1.7.4 because Domoticz on port 25238 and
OTmonitor no longer work side by side, and Schelte Bron reported a user whose
OTmonitor could not connect. Both tools send commands, so a second slot that only
reads would leave the second tool half working.

The current inbound path (`src/OTGW-firmware/OTGW-Core.ino`, the
`OTGWstream.available()` loop at about line 4841) writes every byte it reads to
the PIC immediately (`OTGWSerial.write(outByte)`). A parallel line buffer,
`sWrite`, only drives side effects when a carriage return (CR) arrives:
removing a queued command that the network command overrides
(`lastSer2netCmdMs`), resetting the gateway on `GW=R`, and tracking `PS=1` /
`PS=0` print-summary mode. The loop is bounded by `HANDLE_OTGW_LINES_PER_CALL`
and must not yield, because its state is static.

ADR-095 (Accepted) requires the PIC-to-client direction to be byte transparent
and enforces that with the `OTGW_PASSTHRU_CHUNK` symbol. It says commands from
port 25238 bypass the command queue and go straight to the serial port. It does
not constrain how the client-to-PIC direction is framed. Flashing the PIC over
WiFi through OTmonitor is already forbidden in this project, because it can brick
the PIC.

The hardware is an ESP8266 with 15 to 17 KB of free heap measured on the bench
gateway (192.168.88.68, TASK-1163, 2026-09-24). Two slots is the configuration
that shipped until v1.7.5, so its memory cost is known from the field. Every
client receives the full PIC stream through its own TCP send buffer.

## Decision Drivers

* Two command-sending tools on one gateway is a real, reported use case, and one
  user downgraded to keep it.
* A command that reaches the PIC must be one client's complete command, never a
  mix of two.
* The PIC-to-client direction must stay exactly as ADR-095 decided.
* Memory headroom on the ESP8266 is small, so the slot count must stay at a
  configuration with field evidence.
* SimpleTelnet is the maintainer's own library, so a per-slot API is an option
  rather than a fork.

## Considered Options

* **Option A: keep one client** (the v1.7.5 status quo, TASK-1115).
* **Option B: one writer plus read-only readers.** The first client may send
  commands; later clients receive the stream and their input is discarded.
* **Option C: two writers, each command forwarded to the PIC as one complete
  line.**
* **Option D: three or more writers**, as option C with more slots.

## Decision Outcome

Chosen option: **Option C**, decided by the maintainer on 2026-09-26, because it
is the only option that lets two command-sending tools work side by side while
making interleaved commands impossible rather than unlikely.

1. SimpleTelnet gains a per-slot read API, `availableFrom(idx)` and
   `readFrom(idx)`, plus a query for whether a slot is active. The existing
   `available()` and `read()` stay for other users of the library.
2. `OTGWstream` returns to two slots (`SimpleTelnet<2>`).
3. The port 25238 inbound path keeps one line buffer per slot. A command is
   forwarded to `OTGWSerial` only when its CR arrives, as a single write of the
   complete line followed by CR. A line feed is skipped, as today. A line longer
   than the buffer is discarded whole and never forwarded in part. The side
   effects (queue override, `GW=R`, `PS=1` / `PS=0`) run per completed line,
   exactly as now. The symbol `OTGW_NET_LINE_ATOMIC` names this rule in the code.
4. The PIC-to-client direction does not change and stays governed by ADR-095.
5. The port's state (number of clients, their address, the last address that was
   refused) is published in `/api/v2/device/info` (TASK-1167 part 1).

### Confirmation

* On the bench gateway, two clients connected at once each send commands in a
  tight loop for several minutes. Every command the PIC answers matches one
  client's command exactly; the PIC reports no malformed command.
* A line sent without CR is not forwarded; a line longer than the buffer is not
  forwarded; neither affects the other client's next command.
* The Home Assistant `opentherm_gw` integration and a second tool connect at the
  same time and both work.
* Heap after two clients have streamed for 30 minutes stays at the level measured
  with one client, within normal variation.

## Decision Contract

### Must

* Forward a network command to the PIC only once its line is complete, in one
  write, from one client's buffer.
* Keep one line buffer per slot, so one client's partial line can never reach
  another client's command.
* Discard, never truncate-and-forward, a line that overflows its buffer.
* Keep the PIC-to-client passthrough of ADR-095 unchanged.

### Must Not

* Forward network bytes to the PIC one at a time as they arrive.
* Share a single line buffer between slots.
* Raise the slot count above two without a new ADR that measures the heap cost.

### Exceptions

* None.

### Verification

* `OTGW_NET_LINE_ATOMIC` is present in `src/OTGW-firmware/OTGW-Core.ino`
  (Enforcement block below).
* The bench test in Confirmation, recorded in the implementing task.

## Consequences

### Positive

* Two tools that both send commands, for example Home Assistant and OTmonitor or
  Domoticz and OTmonitor, work on one gateway again.
* A partial command can no longer reach the PIC at all. Before v1.7.5 it could
  splice with another client's; before this decision a single client's partial
  line still reached the PIC byte by byte. This is stricter than both.
* The status fields show at a glance who holds the port, which answers "why can
  OTmonitor not connect" without a capture.

### Negative

* The client-to-PIC direction is now framed by lines. A tool that sends bytes
  without a CR, or a binary protocol over port 25238, no longer reaches the PIC.
  Mitigation: every known client sends CR-terminated commands, and PIC flashing
  over this port is already forbidden.
* A command without CR stays buffered until the client sends CR or disconnects,
  at which point it is dropped. Mitigation: that command could not have been
  processed correctly by the PIC either.
* Two tools can still send commands that contradict each other, for example two
  different setpoints. That is a configuration conflict between the tools, the
  same as before v1.7.5, and the firmware does not try to arbitrate it.
* One more line buffer of `MAX_BUFFER_WRITE` bytes, and a larger library API.
* Risk: heap pressure with two clients streaming under load. Mitigation: two
  slots is the configuration that shipped until v1.7.5, and the Confirmation
  step measures heap with both connected.

## Pros and Cons of the Options

### Option A: keep one client

* Good, because it needs no change and no new code.
* Bad, because the reported use case stays impossible and users downgrade.

### Option B: one writer plus read-only readers

* Good, because no second writer means no interleaving by construction.
* Bad, because both reported tools send commands, so the second one loses its
  commands and works only half.
* Bad, because which client is the writer depends on connection order, which the
  user does not control.

### Option C: two writers, complete lines only

* Good, because both tools work and interleaving becomes impossible.
* Good, because partial lines no longer reach the PIC even with one client.
* Bad, because the inbound direction is no longer byte transparent.
* Bad, because the library needs a per-slot read API.

### Option D: three or more writers

* Good, because it covers setups with more than two tools.
* Bad, because every slot adds a TCP send buffer that receives the full PIC
  stream, and nobody has asked for a third slot.

## Open Questions

* [ ] Should a buffered line without CR be dropped after a timeout rather than
  only on disconnect?

## Related Decisions

* **ADR-095 (OpenTherm data flow pipeline with an enforced byte-transparent
  serial bridge)**: complements it. ADR-095 governs the PIC-to-client direction
  and stays in force; this ADR governs the client-to-PIC direction.

## References

* `src/OTGW-firmware/OTGW-Core.h:20-28`: the single-slot declaration and the
  splicing rationale from TASK-1115.
* `src/OTGW-firmware/OTGW-Core.ino`: the `OTGWstream.available()` inbound loop,
  about line 4841.
* `src/libraries/SimpleTelnet/src/SimpleTelnet.h`: the current read API.
* TASK-1115 (one client on port 25238), TASK-1109 (byte transparency), TASK-1146
  (first command discarded after connect), TASK-1167 (this change).
* Discord #nederlandse-ondersteuning, 2026-09-25 and 2026-09-26: requests from
  Schelte Bron and iandury_.

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "OTGW_NET_LINE_ATOMIC",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "Commands from port 25238 must reach the PIC one complete line at a time from one client's buffer (ADR-097). Removing OTGW_NET_LINE_ATOMIC reopens the interleaving that made v1.7.5 drop to one client."
    }
  ]
}
```
