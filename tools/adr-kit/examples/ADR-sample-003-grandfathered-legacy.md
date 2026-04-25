# ADR-003 Use SHA-256 hashes for record dedup keys

<!-- adr-kit-lint: advisory -->

> Worked example of an ADR that predates the canonical adr-kit template.
> The legacy template uses different subsection names (Decision drivers,
> Pros and Cons, Risks and Mitigations) than the canonical seven. The
> per-ADR marker above tells `/adr-kit:lint` to treat all gate failures
> on this file as ADVISORY. Strict gating still applies to ADRs written
> after the project adopted adr-kit; this file remains untouched.

## Status

Accepted, 2023-08-14.

## Context

The ingest service stores roughly 12 million records per day, deduplicated
by a key field. The original key was a 16-byte UUID v4 captured at the
edge. Edge clients running on intermittent networks regenerated the UUID
on retry, producing duplicate records that downstream consumers had to
collapse later, costing 3 to 5 percent of pipeline time.

We need a deterministic key that produces the same value for the same
logical record regardless of how many times the edge client retries.

## Decision drivers

- Determinism across retries: same input bytes must produce the same key.
- Collision resistance: collisions cannot be tolerated; the key is the
  primary key on the storage table.
- Speed at edge: each record passes through a constrained device that
  has a 10 ms budget for hashing.
- Library availability in Go (server) and C (edge firmware).

## Decision

Use SHA-256 of the canonical record body as the dedup key, encoded as
the lower 16 bytes of the digest in big-endian order. Truncate to 16
bytes only on the storage column; keep the full 32 bytes in transit so
later auditing can re-derive the full hash.

## Pros and Cons

### Pros

- Deterministic across retries.
- SHA-256 has no known collision attack at the truncation boundary we
  use; the birthday-collision probability at 16 bytes is acceptable for
  a daily volume of 12 million records.
- Available in stdlib in both Go and C (mbedTLS on the edge device).

### Cons

- Edge measurement: SHA-256 takes 4.2 ms on the target microcontroller
  for an average 800-byte record (well within budget but not free).
- Truncation to 16 bytes is a permanent commitment; if collisions ever
  appear in production, lengthening the key is a migration.

## Risks and Mitigations

- **Risk**: future record bodies grow large enough to exceed the 10 ms
  hashing budget at the edge. **Mitigation**: monitor edge hash latency;
  if p99 exceeds 8 ms, switch to BLAKE2s (3x faster on this MCU).
- **Risk**: birthday collision at 16 bytes if daily volume grows by an
  order of magnitude. **Mitigation**: alarm when collision count in any
  hour exceeds zero; expand to 24 bytes via a migration.

## Related ADRs

- ADR-002 (record canonicalisation, in scope of this hash).
- Original ingest design doc: `docs/internal/ingest-2023-q3.md`.

---

*Lint note: this ADR is structured around the original 2023 template
that the project used before adopting adr-kit. Subsections like Decision
drivers, Pros and Cons, and Risks and Mitigations would normally fail
the Completeness gate's canonical-seven check. The `<!-- adr-kit-lint:
advisory -->` marker at the top of the file tells the lint skill to
report findings as ADVISORY rather than FAIL on this file. New ADRs in
the project follow the canonical template and lint strict; this one
remains as written.*
