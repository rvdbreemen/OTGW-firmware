---
id: "ADR-097"
title: "Allow two writers on port 25238 with a byte-transparent write floor"
status: "Proposed"
date: "2026-09-26"
binding: false
gate: "OTGW_NET_WRITE_FLOOR"
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
  - "binary-transparency"
aliases:
  - "otmonitor port"
  - "two clients on 25238"
  - "command splicing"
  - "write floor"
components:
  - "OTGWstream inbound path (port 25238 to PIC)"
  - "SimpleTelnet per-slot read API"
symbols:
  - "OTGW_NET_WRITE_FLOOR"
  - "OTGWstream"
  - "availableFrom"
  - "readFrom"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-097 Allow two writers on port 25238 with a byte-transparent write floor

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
moment, their bytes were interleaved into one stream toward the PIC, so half of
one client's command could be concatenated with another's into a malformed
command. The release note said that one writer plus several readers was the way
back if people needed it.

People need it. On 2026-09-26 a user (iandury_, Discord
#nederlandse-ondersteuning) reverted to v1.7.4 because Domoticz on port 25238 and
OTmonitor no longer work side by side, and Schelte Bron reported a user whose
OTmonitor could not connect. Both tools send commands, so a second slot that only
reads would leave the second tool half working.

The bridge must stay byte transparent in both directions. ADR-095 (Accepted)
enforces this for the PIC-to-client direction with the `OTGW_PASSTHRU_CHUNK`
symbol. For the client-to-PIC direction the maintainer set the same requirement
on 2026-09-26: every byte a client sends reaches the PIC unchanged, in order,
without being added to, dropped, reframed or held for a line terminator. One
reason is PIC firmware upgrades: flashing the PIC through OTmonitor over port
25238 is discouraged but supported, and can work. OTmonitor, written by Schelte
Bron, is the only tool proven to upgrade the PIC this way (maintainer,
2026-09-26), so its upgrade traffic is the reference the design must carry. That traffic is binary, so it
contains CR bytes and every other byte value, and a single foreign byte inserted
into it can leave the PIC unbootable. The
current inbound path already does that for one client
(`src/OTGW-firmware/OTGW-Core.ino`, the `OTGWstream.available()` loop at about
line 4841): each byte is written to the PIC as it is read
(`OTGWSerial.write(outByte)`), and a parallel line buffer, `sWrite`, only
observes the stream to drive side effects when a carriage return (CR) passes
(overriding a queued command via `lastSer2netCmdMs`, resetting the gateway on
`GW=R`, tracking `PS=1` / `PS=0`). The loop is bounded by
`HANDLE_OTGW_LINES_PER_CALL` and must not yield, because its state is static.

The hardware is an ESP8266 with 15 to 17 KB of free heap measured on the bench
gateway (192.168.88.68, TASK-1163, 2026-09-24). Two slots is the configuration
that shipped until v1.7.5, so its memory cost is known from the field.

## Decision Drivers

* Two command-sending tools on one gateway is a real, reported use case, and one
  user downgraded to keep it.
* Client-to-PIC must be 100% byte transparent: no byte added, dropped, changed,
  reordered within a client, or delayed until a line terminator (maintainer,
  2026-09-26).
* Bytes from two clients must never interleave on the way to the PIC.
* The PIC-to-client direction must stay exactly as ADR-095 decided.
* Memory headroom on the ESP8266 is small, so the slot count must stay at a
  configuration with field evidence, and the design should not add buffers.

## Considered Options

* **Option A: keep one client** (the v1.7.5 status quo, TASK-1115).
* **Option B: one writer plus read-only readers.** Later clients receive the
  stream; their input is discarded.
* **Option C: two writers, each command forwarded as one complete line.** Buffer
  per client and forward on CR.
* **Option D: two writers with a write floor.** One client at a time holds the
  right to write; its bytes pass straight through; the other client's bytes wait
  unread in its own TCP socket until the floor is free.

## Decision Outcome

Chosen option: **Option D**, because it is the only option that lets two
command-sending tools share the port while keeping every byte transparent and
making interleaving impossible. Option C was the first proposal and was rejected
by the maintainer on 2026-09-26 because it reframes the client-to-PIC stream.

1. SimpleTelnet gains a per-slot read API, `availableFrom(idx)` and
   `readFrom(idx)`, plus a query for whether a slot is active. The existing
   `available()` and `read()` stay for other users of the library.
2. `OTGWstream` returns to two slots (`SimpleTelnet<2>`).
3. The inbound path grants a write floor to one slot at a time:
   * A slot takes the floor when the floor is free and it has a byte to send.
   * The holder's bytes are read and written to `OTGWSerial` one by one as they
     arrive, exactly as today: unchanged, in order, with no buffering or delay.
   * The other slot is not read at all while the floor is held. Its bytes stay in
     its own TCP receive buffer, so nothing is copied, reordered or dropped, and
     TCP flow control holds the sender back if it keeps sending.
   * The floor is released only on silence or disconnect, never on the content
     of the stream. A holder that has sent only printable ASCII, CR and LF since
     taking the floor releases it after `OTGW_NET_FLOOR_IDLE_MS` without a byte.
     Once the holder has sent any other byte value, the stream is treated as
     binary (a PIC firmware upgrade) and the floor is released only after
     `OTGW_NET_FLOOR_BINARY_IDLE_MS` without a byte, long enough to cover the
     bootloader's pauses between blocks. A CR never releases the floor, because
     a binary stream contains CR bytes. Release happens only between bytes.
   * The existing side effects keep observing the holder's stream as they do now.
   The rule is named by the symbol `OTGW_NET_WRITE_FLOOR` in the code.
4. The PIC-to-client direction does not change and stays governed by ADR-095.
5. The port's state (number of clients, their address, the last address that was
   refused) is published in `/api/v2/device/info` (TASK-1167 part 1).

### Confirmation

* Byte transparency: a client sends a stream containing every byte value 0-255
  (with the PIC in a harmless state or the serial side captured); the bytes on
  the serial side are identical to the bytes sent, in the same order.
* No interleaving: two clients send commands in a tight loop at the same time for
  several minutes; every command the PIC answers matches one client's command
  exactly, and the PIC reports no malformed command.
* A client that holds the floor releases it after `OTGW_NET_FLOOR_IDLE_MS` of
  silence, and the other client's command then goes through intact.
* A binary stream with pauses shorter than `OTGW_NET_FLOOR_BINARY_IDLE_MS`, and
  containing CR bytes, keeps the floor from start to end while the second client
  sends commands in a loop: the serial side receives the binary stream with no
  foreign byte inside it. This is tested with a byte-pattern stream and the
  serial side captured, never with a real PIC upgrade.
* The Home Assistant `opentherm_gw` integration and a second tool connect at the
  same time and both work.
* Heap after two clients have streamed for 30 minutes stays at the level measured
  with one client, within normal variation.

## Decision Contract

### Must

* Forward every byte from the floor holder to the PIC as it is read, unchanged
  and in order.
* Read from only one slot at a time while the floor is held; leave the other
  slot's bytes unread in its socket.
* Release the floor only between bytes, and only on silence
  (`OTGW_NET_FLOOR_IDLE_MS`, or `OTGW_NET_FLOOR_BINARY_IDLE_MS` once the holder
  has sent a non-text byte) or on disconnect.
* Keep the PIC-to-client passthrough of ADR-095 unchanged.

### Must Not

* Buffer, reframe, strip, add or delay client bytes on their way to the PIC.
* Wait for a line terminator before forwarding a byte.
* Release the floor because of a byte value, CR included.
* Read two slots in the same pass while the floor is held.
* Raise the slot count above two without a new ADR that measures the heap cost.

### Exceptions

* None.

### Verification

* `OTGW_NET_WRITE_FLOOR` is present in `src/OTGW-firmware/OTGW-Core.ino`
  (Enforcement block below).
* The bench tests in Confirmation, recorded in the implementing task.

## Consequences

### Positive

* Two tools that both send commands, for example Home Assistant and OTmonitor or
  Domoticz and OTmonitor, work on one gateway again.
* Client-to-PIC stays byte transparent: a single client sees exactly today's
  behaviour, byte for byte and with the same latency.
* Interleaving becomes impossible rather than unlikely, without any extra buffer.
* The status fields show who holds the port, which answers "why can OTmonitor
  not connect" without a capture.

### Negative

* While one client holds the floor, the other client's command waits until the
  holder has been silent for `OTGW_NET_FLOOR_IDLE_MS`. Mitigation: commands are
  short, and the idle timeout is small.
* During a PIC firmware upgrade the other client cannot write at all, for the
  whole transfer plus `OTGW_NET_FLOOR_BINARY_IDLE_MS`. That is the intent: its
  commands would otherwise land inside the upgrade. Its output side still
  receives the PIC's bytes, which during an upgrade are bootloader replies.
* The text/binary distinction is a scheduling rule, not a filter: it never
  changes a byte. Risk: a text client that sends a stray non-text byte holds the
  floor for the long timeout once. Mitigation: that costs the other client up to
  `OTGW_NET_FLOOR_BINARY_IDLE_MS` of delay, nothing more.
* Risk: a bootloader pause longer than `OTGW_NET_FLOOR_BINARY_IDLE_MS` lets the
  other client write mid-upgrade. Mitigation: the value is set with margin above
  the longest pause measured in a real OTmonitor upgrade capture; flashing over
  the network stays discouraged in the documentation.
* The values of both timeouts are set by measurement during implementation (see
  Open Questions).
* Two tools can still send commands that contradict each other, for example two
  different setpoints. That is a configuration conflict between the tools, the
  same as before v1.7.5, and the firmware does not arbitrate it.
* The library API grows by a per-slot read.
* Risk: heap pressure with two clients streaming under load. Mitigation: two
  slots is the configuration that shipped until v1.7.5, and the Confirmation step
  measures heap with both connected.

## Pros and Cons of the Options

### Option A: keep one client

* Good, because it needs no change.
* Bad, because the reported use case stays impossible and users downgrade.

### Option B: one writer plus read-only readers

* Good, because a single writer cannot interleave.
* Bad, because both reported tools send commands, so the second one loses its
  commands; which client writes depends on connection order.

### Option C: two writers, complete lines only

* Good, because both tools work and interleaving is impossible.
* Bad, because it reframes the client-to-PIC stream: bytes wait for CR, and data
  without CR never reaches the PIC, which breaks a PIC firmware upgrade. Rejected
  by the maintainer: client-to-PIC must be 100% byte transparent.

### Option D: two writers with a write floor

* Good, because both tools work, interleaving is impossible, and every byte stays
  transparent with no added buffer.
* Good, because a binary upgrade stream keeps exclusive access to the PIC.
* Bad, because a waiting client's command is delayed while the other holds the
  floor, and two timeouts have to be chosen by measurement.

## Open Questions

* [ ] What value should `OTGW_NET_FLOOR_IDLE_MS` have? Proposal: start at 100 ms
  and set it from the gap measured between bytes of one command from OTmonitor and
  the Home Assistant integration.
* [ ] What value should `OTGW_NET_FLOOR_BINARY_IDLE_MS` have? Proposal: start at
  3000 ms and set it with margin above the longest pause in an OTmonitor upgrade
  capture (OTmonitor is the only proven network upgrade tool), taken without
  flashing a PIC on this project's bench, for example by asking Schelte Bron for
  the bootloader's worst-case block timing.

## Related Decisions

* **ADR-095 (OpenTherm data flow pipeline with an enforced byte-transparent
  serial bridge)**: complements it. ADR-095 governs the PIC-to-client direction
  and stays in force; this ADR applies the same transparency to the
  client-to-PIC direction with two clients.

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
      "pattern": "OTGW_NET_WRITE_FLOOR",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "Port 25238 must give one client at a time the right to write and forward its bytes verbatim (ADR-097). Removing OTGW_NET_WRITE_FLOOR reopens the interleaving that made v1.7.5 drop to one client."
    }
  ]
}
```
