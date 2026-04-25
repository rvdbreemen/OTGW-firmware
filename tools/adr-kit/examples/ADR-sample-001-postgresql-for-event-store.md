# ADR-001 PostgreSQL for the event store

> **Sample ADR.** This is a worked example illustrating an ADR that passes all four verification gates (Completeness, Evidence, Clarity, Consistency). Use it as a reference for the level of detail expected. Real ADRs in your project replace this content with your own decision.

## Status

Accepted. Date: 2026-04-25.

## Context

The new audit-trail subsystem needs a durable event store. Producers append immutable events at a peak rate of about 800 events per second; consumers replay events for projection rebuilds and run point-in-time queries against arbitrary time ranges. The team already operates two PostgreSQL clusters for the transactional workload, so any choice that adds a new database technology incurs a known operations cost (on-call rotation, backup tooling, monitoring runbooks).

The audit subsystem must satisfy three constraints:

- **Durability**: events are the source of truth for compliance reporting. Loss of an acknowledged event is a P1 incident.
- **Replay throughput**: a full replay of 90 days of events must complete in under 30 minutes for the disaster-recovery rehearsal.
- **Query flexibility**: compliance queries are ad-hoc, often joining events with reference data already in the operational PostgreSQL cluster.

## Decision

The event store uses PostgreSQL 16 with a partitioned `events` table, append-only writes via a single insert path, and `pg_logical` for downstream consumers. We do not introduce a dedicated event-store technology (Kafka, EventStoreDB).

The append path uses a server-side function with `INSERT ... RETURNING id` and a UUIDv7 primary key. Partitions are time-based (one per month). Replay reads from a covering index on `(stream_id, event_seq)` with a serializable read snapshot.

## Alternatives Considered

### Alternative A: Kafka

Append throughput would be higher (we measured 12,000 events per second on a 3-broker cluster in a spike test). Replay from earliest offset is fast and well-tooled.

Rejected. Kafka introduces a third stateful technology to operate and a new on-call rotation that the team does not currently staff. The 800 events per second peak is well within PostgreSQL's published append rate (the team measured 4,200 events per second sustained on the existing cluster's WAL throughput; see SRE-204 in the internal doc set). The query-flexibility constraint (ad-hoc joins with operational data) is not a fit for Kafka without an additional sink, which compounds the operations cost.

### Alternative B: EventStoreDB

Purpose-built for event sourcing with first-class stream and projection primitives. Replay is the native happy path.

Rejected. Smaller operational community than PostgreSQL; the team has zero in-production experience. The compliance-query constraint requires joining events with operational reference data that lives in PostgreSQL anyway, so an EventStoreDB choice forces a sink-and-join pattern. Compounded with the cold-start operational cost, the trade is not favourable.

### Alternative C: Stay on the existing PostgreSQL cluster (no separate event store)

Cheapest option. No new schema, no new tables, no new partitioning.

Rejected. The audit-trail workload is append-heavy and time-partitioned; mixing it with the existing transactional schema risks autovacuum storms during partition turnover and creates contention on the WAL. We measured a 12% latency regression on the operational `orders` table in a load test that ran the audit workload alongside (see PERF-118). A dedicated cluster (still PostgreSQL, but its own instance) avoids the contention while keeping the technology stack at one item.

## Consequences

**Benefits**

- One database technology to operate. The team's existing PostgreSQL runbooks (backup, restore, monitoring, on-call escalation) apply unchanged.
- Cross-database joins between events and operational reference data are local to the same engine, simplifying compliance queries.
- Disaster-recovery rehearsals reuse the existing point-in-time-recovery procedure; no new procedure to validate.
- UUIDv7 keys give time-ordered IDs for free, which makes replay-by-time-range a covering-index scan.

**Trade-offs**

- Sustained append rate is bounded by the cluster's WAL throughput. Headroom over 800 events per second peak is roughly 5x today; that headroom shrinks if the workload grows by an order of magnitude. Re-evaluation point: when 7-day rolling peak exceeds 3,000 events per second.
- Time-based partitioning requires a partition-rotation job. We accept the operational cost of that job (one cron entry, one alert rule) for the simplicity of pruning old partitions in O(1) instead of running a delete loop.

**Risks and mitigations**

- *Risk*: WAL contention with the operational cluster if we are tempted to consolidate later. *Mitigation*: this ADR commits to a separate PostgreSQL instance for the event store. Any consolidation requires a superseding ADR with new measurements.
- *Risk*: a future workload spike pushes us past the 3,000 events per second re-evaluation point during a long weekend. *Mitigation*: monitor 1-minute peak append rate, page on sustained rate above 2,500 events per second over 5 minutes, and document the upgrade path in the runbook (read replica for replay, then evaluate Kafka if write rate is the bottleneck).

## Related Decisions

- **ADR-014 (Single PostgreSQL operations runbook)**: this decision relies on that runbook applying to the new instance unchanged.
- **ADR-027 (Compliance query patterns)**: depends on this decision; specifies the join shapes between events and reference data.

## References

- Internal performance doc PERF-118 (mixed-workload latency regression measurements).
- Internal SRE doc SRE-204 (sustained append-rate measurement on the existing cluster).
- PostgreSQL 16 partitioning documentation: https://www.postgresql.org/docs/16/ddl-partitioning.html.
- UUIDv7 draft: https://datatracker.ietf.org/doc/draft-ietf-uuidrev-rfc4122bis/.
