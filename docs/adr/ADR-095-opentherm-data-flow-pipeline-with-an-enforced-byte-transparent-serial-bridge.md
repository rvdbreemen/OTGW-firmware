---
id: "ADR-095"
title: "OpenTherm data flow pipeline with an enforced byte-transparent serial bridge"
status: "Accepted"
date: "2026-09-03"
binding: true
gate: "OTGW_PASSTHRU_CHUNK"
documents_shipped: true
verified_in:
  - "src/OTGW-firmware/OTGW-Core.ino"
supersedes:
  - "ADR-038"
superseded_by: null
topics:
  - "data-flow"
  - "serial-bridge"
  - "ser2net"
  - "fan-out"
  - "binary-transparency"
aliases:
  - "port 25238"
  - "OTmonitor bridge"
  - "serial to network bridge"
  - "OpenTherm message pipeline"
components:
  - "handleOTGW"
  - "processOT"
  - "OTGWstream"
  - "dispatchOTGWInputLine"
symbols:
  - "OTGW_PASSTHRU_CHUNK"
  - "HANDLE_OTGW_BYTES_PER_CALL"
  - "HANDLE_OTGW_LINES_PER_CALL"
context_scope: "selective"
format: "madr"
---

<!-- markdownlint-disable MD025 -->

# ADR-095 OpenTherm data flow pipeline with an enforced byte-transparent serial bridge

## Status

Accepted, 2026-09-03.

## Status History

```yaml
status_history:
  - date: 2026-09-03
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-09-03
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
  - date: 2026-09-03
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Supersedes ADR-038
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

The core purpose of this firmware is to receive OpenTherm messages from the
PIC (Peripheral Interface Controller) and fan them out to five consumers:
MQTT (Message Queuing Telemetry Transport), the WebSocket log,
the REST (Representational State Transfer) API, the OTmonitor desktop
application over TCP (Transmission Control Protocol) port 25238, and the telnet
debug console. ADR-038 decided that pipeline in February 2026 and documented it
accurately.

ADR-038 also specified, in its architecture diagram and again in its consumer
table, that the port 25238 consumer is fed by **raw byte forwarding** on **every
byte**. That clause was correct and is still correct. What ADR-038 did not do is
make it checkable: it carries `binding: false`, `gate: null` and an empty
`verified_in`, so nothing in the toolchain ever compared the shipped code against
it.

The implementation drifted. `dispatchOTGWInputLine()` became the sole writer to
`OTGWstream` and emitted `write(buf, len)` followed by a synthesised carriage
return and line feed, only once a complete line had been assembled. Three
consequences follow, none of which any gate could see:

1. Output that carries no line terminator never reached a connected client at
   all. It sat in the parser's line buffer indefinitely.
2. A carriage return, line feed or NUL byte inside a payload was consumed as a
   terminator and replaced by a synthesised pair, so the byte stream a client
   received was not the byte stream the PIC sent.
3. A line longer than the parser buffer was discarded whole, including for the
   network client, which has no stake in the parser's limits.

Schelte Bron, who writes the PIC firmware, reported the first consequence from
the field: text from the diagnose firmware only appeared once a full line with a
newline existed. The cause is visible in his source. The prompt is declared as
`da "Enter test number: \032"`, where `\032` is an end-of-string sentinel that is
never transmitted, so the prompt genuinely carries no newline and the PIC then
blocks waiting for input that the operator cannot see a request for.

This ADR therefore does two things: it carries the ADR-038 pipeline forward
unchanged, because that decision was right, and it restates the port 25238
contract as a binding requirement with a source anchor, so the same drift cannot
recur silently.

## Decision Drivers

* A decision that no gate checks is a comment, and this one demonstrably decayed
  into one over roughly six months without anybody noticing.
* Port 25238 exists for OTmonitor compatibility. OTmonitor speaks a raw serial
  protocol, so anything that reframes the byte stream breaks the reason the port
  exists.
* The PIC firmware is written by a third party and may emit any byte sequence.
  The bridge cannot assume line structure it does not control.
* The line buffer is needed by the OT parser and must keep working. Transparency
  must not cost message decoding.
* Per-byte network writes are not free on this platform: on the ESP8266 Arduino
  core a single-byte `WiFiClient::write()` becomes its own TCP segment.

## Considered Options

* **Option A: carry the pipeline forward, restore byte transparency, and make it
  enforceable.** Separate transport from parsing, forward every byte verbatim,
  and anchor the requirement to a named symbol the judge can check.
* **Option B: restore byte transparency without enforcement.** Fix the code, keep
  ADR-038 as it stands.
* **Option C: keep line-based forwarding and amend the record to match.** Declare
  the shipped behaviour correct and drop the raw-forwarding clause.
* **Option D: do nothing.**

## Decision Outcome

Chosen option: **Option A**, because the defect was never a disagreement about
the right behaviour. ADR-038 already specified byte forwarding and the code
simply stopped doing it. Fixing the code alone (Option B) repairs today's symptom
and leaves the same blind spot that allowed six months of drift; the cost of a
gate here is one named constant.

The pipeline decided by ADR-038 is carried forward unchanged: a synchronous
fan-out where the serial reader dispatches complete lines to `processOT()`, which
updates global state and pushes to every consumer in one call stack. Commands
from MQTT, REST and the Web UI still go through the deduplicating command queue,
while commands arriving on port 25238 still bypass it and reach the serial port
directly.

What changes is that transport and parsing are now separate concerns in the
serial reader. Every byte read from the PIC is forwarded to `OTGWstream` as it is
read, independent of the line assembly that continues to feed `processOT()`.

Coalescing is explicitly permitted and explicitly bounded: bytes may be gathered
in a small fixed buffer that is flushed when it fills and again before the reader
returns. Coalescing may never wait for a line terminator, and it may never
outlive a single call of the reader. This is a deliberate concession to the
per-byte TCP segment cost, not a licence to reintroduce line semantics.

### Confirmation

The gate symbol `OTGW_PASSTHRU_CHUNK` must be present in the serial reader, and
the Enforcement block below fails the commit if it disappears.

Behaviourally, a client on port 25238 must observe reads that do not end on a
line terminator. Measured on a live gateway running 1.7.5-beta.7: 69 of 98 reads
over 22 seconds ended mid-line, with fragments such as `R0`, `000`, `PR` and
`=16.` arriving as they came off the wire. The previous implementation could not
produce that observation, which makes it a positive test rather than an absence
of failure.

## Decision Contract

### Must

* Forward every byte read from the PIC serial port to `OTGWstream` verbatim,
  independent of any line assembly.
* Preserve carriage return, line feed, NUL and every byte above 0x7F exactly as
  received on the forwarding path.
* Flush any coalescing buffer before the serial reader returns, so no byte
  outlives the call that read it.
* Bound the serial drain by a byte cap as well as a line cap, because a payload
  without terminators no longer reaches the line cap.
* Keep feeding complete, NUL-terminated lines to `processOT()` for decoding.
* Give any producer that synthesises lines without passing through the serial
  reader, such as the simulation replay path, its own write to `OTGWstream`.

### Must Not

* Gate a forwarded byte on the arrival of a line terminator.
* Synthesise, translate or normalise a line terminator on the forwarding path.
* Withhold from the network client a line that the parser rejects for its own
  reasons, such as a buffer overflow.
* Hold coalesced bytes in state that survives the serial reader, which would let
  a re-entrant call inherit a partial chunk.

### Exceptions

* The OT-Direct bridge writes synthesised lines to port 25238 because it decodes
  frames itself and has no raw byte stream to forward. Byte transparency is
  undefined there and this contract does not apply to it.
* The telnet debug console on port 23 is a human-facing line protocol, not a data
  bridge, and is out of scope.

### Verification

* Source anchor: `OTGW_PASSTHRU_CHUNK` in `src/OTGW-firmware/OTGW-Core.ino`,
  checked by the Enforcement block below.
* Field check: connect a raw TCP client to port 25238 and confirm that reads
  arrive mid-line rather than one whole line per read.

## Consequences

### Positive

* Output that carries no line terminator reaches the client immediately, which
  is what the diagnose firmware needs and what OTmonitor assumed all along.
* Binary payloads survive the bridge, so the port can carry data that is not
  text without silent corruption.
* The requirement now has a gate, so the next refactor that reintroduces line
  buffering fails at commit time instead of shipping.
* A line the parser discards still reaches the client, which removes a class of
  invisible loss where the network consumer paid for a parser limit.

### Negative

* More TCP segments. Measured at idle OpenTherm traffic the stream went from
  roughly 1.3 to roughly 4.5 packets per second, because a flush now happens per
  reader tick rather than per line. At 9600 baud this is bounded and cheap, and
  the coalescing buffer only fills during a sustained burst.
* A client that assumed one read equals one line must now assemble lines itself.
  That assumption was never part of the contract, but code may have relied on it.
* Byte transparency of the pipe is not the same as exclusive ownership of the
  link. The firmware still injects its own traffic on the same serial port, so
  this decision does not by itself make a firmware upgrade driven over port 25238
  work. That remains out of scope here.

## Pros and Cons of the Options

### Option A: carry forward, restore, and enforce

* Good, because it keeps a decision that was already correct instead of
  relitigating it.
* Good, because the gate closes the specific failure mode that produced this
  defect, namely a true clause nobody checked.
* Bad, because it costs a supersession of a broad ADR to add one enforceable
  clause, and the successor has to restate the pipeline to stay self-contained.

### Option B: restore without enforcement

* Good, because it is the smallest change and ships the user-visible fix.
* Bad, because it leaves the blind spot intact. The same drift already happened
  once under exactly these conditions.

### Option C: keep line forwarding and amend the record

* Good, because it would make the record match the code with no code change.
* Bad, because it breaks the stated purpose of the port. It would also codify a
  behaviour that a third-party firmware author reported as a defect.

### Option D: do nothing

* Good, because nothing else can break.
* Bad, because a reported field defect stays unfixed and the record keeps
  describing behaviour the firmware does not have.

## Open Questions

* None.

## Related Decisions

* Supersedes ADR-038, whose pipeline decision is carried forward here unchanged
  and whose raw-forwarding clause is restated as a binding requirement.
* ADR-010 fixes port 25238 as the OTmonitor compatibility port and is unaffected.
* ADR-016 governs the command queue that inbound commands from MQTT, REST and the
  Web UI pass through, which port 25238 deliberately bypasses.

## References

* `src/OTGW-firmware/OTGW-Core.ino`, the serial reader and its passthrough
  buffer, plus `dispatchOTGWInputLine()` which no longer writes to the stream.
* `docs/adr/ADR-038-opentherm-data-flow-pipeline.md`, the superseded predecessor
  whose consumer table already read "Every byte (raw forwarding)".
* TASK-1109, the implementation, including the on-device measurement quoted under
  Confirmation.
* Field report by Schelte Bron, author of the PIC firmware, against the diagnose
  image, whose prompt is declared as `da "Enter test number: \032"`.

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "OTGW_PASSTHRU_CHUNK",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "The port 25238 bridge must forward every serial byte verbatim through the passthrough buffer (ADR-095). Removing OTGW_PASSTHRU_CHUNK reintroduces line-gated forwarding, which strands output that carries no newline."
    }
  ]
}
```
