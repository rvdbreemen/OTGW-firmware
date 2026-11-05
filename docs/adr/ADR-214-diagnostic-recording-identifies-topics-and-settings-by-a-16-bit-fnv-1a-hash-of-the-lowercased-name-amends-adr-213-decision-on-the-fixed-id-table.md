---
id: "ADR-214"
title: "Diagnostic recording identifies topics and settings by a 16-bit FNV-1a hash of the lowercased name (amends ADR-213 decision on the fixed id table)"
status: "Accepted"
date: "2026-11-05"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "diagnostics"
  - "recording"
  - "record format"
  - "topic ids"
aliases:
  - "diagnostic recording id hash"
  - "recording topic id"
components:
  - "recording id hash"
  - "recording record format amendments"
symbols:
  - "diagNameId"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-214 Diagnostic recording identifies topics and settings by a 16-bit FNV-1a hash of the lowercased name (amends ADR-213 decision on the fixed id table)

## Status

Accepted, 2026-11-05.

## Status History

```yaml
status_history:
  - date: 2026-11-05
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-05
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Accepted by Robert in the session 2026-11-05; matches the TASK-1665 implementation and his hash decision
    changed_via: adr-kit lifecycle
```

## Context

ADR-213 decision 2 says topic ids "come from a fixed table on the device", so the ring holds no
dictionary records. TASK-1665 implemented step 1 (`src/OTGW-firmware/diagRecorderStuff.h`) and
found that a real table is expensive and is not needed for the property ADR-213 wants: an id that
is the same on every boot and every build of one format version, so every 4 KiB block decodes on
its own.

Evidence:

- The recording budget measured about 260 distinct MQTT (Message Queuing Telemetry Transport) topics in the field run
  (`docs/diagnostics/recording-budget-1470.md`). Setting records need an id per settings.ini key too.
- A PROGMEM (program memory in flash) table of those names costs about 4 to 5 KB (kilobytes) of flash out of 143 KB of app headroom, and
  every new topic or setting needs a table edit, or its records carry no usable id.
- A 16-bit id over about 260 names has about 260 * 259 / 2 / 65536 = 0.5 expected colliding pairs.
- The decoder and the repeat caches key on the id alone, so a collision never corrupts a value;
  it only makes the name of two ids ambiguous on the host.

Robert chose the hash on 2026-11-05 (interpretation 1 of the TASK-1665 report).

TASK-1665 also made four choices where ADR-213 was silent or ambiguous. They are recorded here so
that the host decoder (TASK-1667) and the download header (TASK-1666) build on the same text.

## Decision

1. **Ids are a hash, not a table.** The 16-bit id of an MQTT topic suffix or a setting field name is
   FNV-1a (32 bit) over the ASCII (American Standard Code for Information Interchange)-lowercased name, folded as `(h >> 16) ^ (h & 0xFFFF)`. The value
   0xFFFF is reserved for an empty cache slot, so a hash of 0xFFFF becomes 0xFFFE. The function is
   `diagNameId()` and is fixed by the format version.
2. **Names travel in the file header.** The download header (ADR-213 decision 2) carries the names
   the firmware knows (topic suffixes and setting names) with their ids, so a decoder needs no
   external list. Records whose name is unknown to the header stay decodable by id.
3. **Collisions are checked on the host.** A host check hashes the real topic list and the real
   settings list of the firmware under test and fails on any two different names with the same id.
   On a collision the fix is a format version bump with a different fold, never a silent shared id.
   This check is the gate for this decision (TASK-1666 for the header list, TASK-1667 for the
   decoder side).
4. **Repeat records have one type per source.** OT (OpenTherm) frames use types 1 to 5 (T, B, R, A, E) and OT
   repeats use types 6 to 10, so the repeat payload is the MsgID alone as ADR-213 sizes it, and the
   decoder still knows the source.
5. **Text fields are `len u8 + bytes`, 255 means cut.** A length of 0 to 254 is the whole text. A
   length of 255 means the text was cut and 254 bytes follow. This replaces the ADR-213 wording
   "max 255, flag when cut", which leaves no room for the flag in a u8.
6. **The repeat caches live outside the 4 KB ring.** They sit in the same allocation as the ring.
   On a no-PSRAM board they add 768 B (64 OT entries and 64 MQTT entries of 6 B), so the start rule
   needs free heap of at least 4096 + 768 + 5120 = 9984 B (ring and caches 4864 B, then the ADR-030
   5120 B rung). On a PSRAM (pseudo-static random-access memory) board they add 4608 B inside the PSRAM block. The ADR-213 start rule is
   unchanged: refuse when the free heap after the allocation would be below 5120 B.
7. **REST (Representational State Transfer) records carry a route id and the header carries the names.** The route id is the index in
   `kV2Routes[]`, which changes between builds, so the download header lists the route names with
   their ids. The method byte is 1 POST (HTTP write request), 2 PUT (HTTP replace request), 3 PATCH (HTTP partial update), 4 DELETE (HTTP delete request), 5 other.

## Decision Contract

### Must

- Compute record ids with `diagNameId()` and nothing else.
- Write the names and ids the firmware knows into the download header.
- Fail a host check when two different names of the real topic or settings list share an id.
- Count the repeat caches in the allocation the no-PSRAM start rule checks.

### Must Not

- Add dictionary records to the ring.
- Share one id between two names on purpose.
- Change `diagNameId()` or the type numbers without a new format version.

### Exceptions

- None.

### Verification

- `test/host/test_diag_recorder.py` includes the real `diagRecorderStuff.h` and covers the id
  function, repeat types, the 255 cut marker and the 9984 B start boundary (cases F1, F7, F9, F11).
- The collision check over the real name lists is owned by TASK-1666 and TASK-1667. Until it exists
  this ADR is guideline-level per ADR-080.

## Alternatives Considered

- **A PROGMEM id table (ADR-213 as written).** Exact ids, no collisions. Rejected: 4 to 5 KB of
  flash, and a table edit for every new topic or setting.
- **An escape record that carries the topic text inline.** No collisions, no table. Rejected: it
  repeats the text in the ring, which is the cost the ids exist to avoid.
- **A 32-bit hash.** No expected collisions at this size. Rejected: 2 more bytes on every MQTT
  record and every repeat, about 280 KB per day (5,780 publishes per hour times 24 times 2 B) on a 2.79 MB (megabytes) recording.
- **A runtime dictionary of first-seen topics.** Rejected in ADR-213: an overwritten block would
  orphan later ones, and the RAM (random-access memory) cost is too high on a no-PSRAM board.

## Consequences

**Positive:**

- No table in flash and nothing to maintain when a topic or setting is added.
- Ids are stable across boots and builds of one format version.
- A collision never corrupts a value or a repeat, because both decoder and encoder key on the id.

**Negative:**

- Two names can share an id (about 0.5 expected pairs over 260 names). Mitigation: the host
  collision check and the version bump rule.
- A name missing from the file header cannot be shown on the host. Mitigation: the header lists
  all names known to the firmware.
- MQTT value repeats compare a 32-bit hash of the value, so a false repeat has probability about
  2^-32 per comparison.
- The no-PSRAM start needs 9984 B free instead of 9216 B, so some tight units refuse more often.

## Open Questions

- [x] Keep the 16-bit FNV-1a (Fowler-Noll-Vo hash) hash instead of the ADR-213 fixed table. - **Answered 2026-11-05 by Robert van den Breemen:** Keep the 16-bit FNV-1a hash.

## Related Decisions

- **ADR-213:** amended in decision 2 (the fixed id table), the OT repeat payload, the text cut flag
  and the cache accounting. Its other decisions stand.
- **ADR-030:** the 5120 B rung of the start rule.
- **ADR-080:** guideline-level until the collision check exists.

## References

- `src/OTGW-firmware/diagRecorderStuff.h` (`diagNameId`, record types, start rule).
- `test/host/test_diag_recorder.cpp` and `test/host/test_diag_recorder.py`.
- `docs/diagnostics/recording-budget-1470.md`.
- Tasks: TASK-1665 (implementation), TASK-1666 (download header), TASK-1667 (host decoder).
