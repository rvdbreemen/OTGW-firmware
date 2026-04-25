# ADR sample: Evidence gate before and after

> **Sample ADR pair.** This file shows the same architectural decision twice. The first version fails the **Evidence gate** of the four verification gates. The second version passes. Reading the two side by side is the fastest way to internalise what the Evidence gate asks for.

The decision under documentation: choose between a synchronous and an asynchronous outbound webhook delivery model.

---

## Version A: fails the Evidence gate

This is the version a busy author writes when the deadline is tight. It looks complete on first read, but a code reviewer would block it on the Evidence gate.

> ## Status
>
> Accepted. Date: 2026-04-25.
>
> ## Context
>
> Webhook delivery has become a bottleneck. Customers complain that confirmations are slow, and the on-call rotation has paged twice this month for slow webhook traffic blocking the request thread.
>
> ## Decision
>
> Move webhook delivery to an asynchronous queue.
>
> ## Alternatives Considered
>
> ### Synchronous (the current state)
>
> Slow.
>
> ### Asynchronous (chosen)
>
> Faster and more reliable.
>
> ## Consequences
>
> Better performance. Some additional complexity from the queue.

**What is wrong:**

- "Slow", "Faster", "more reliable", "Better performance" are bare adjectives. None of them are anchored to a measurement.
- "Customers complain" is unattributed. How many customers, what percentile, what threshold?
- "Paged twice this month" is a count without context. Of how many pages total? On what severity? With what root-cause analysis?
- Alternatives are described in a single word each, with no rejection reasoning beyond the adjective.
- Risks and mitigations section is missing entirely; "additional complexity from the queue" is not a risk, it is an unquantified hand-wave.

A reviewer can block this with a single sentence: **"This ADR fails the Evidence gate. Replace bare adjectives with measurements and cite the source for each constraint."**

---

## Version B: passes the Evidence gate

Same decision, rewritten to pass.

> ## Status
>
> Accepted. Date: 2026-04-25.
>
> ## Context
>
> Webhook delivery is on the synchronous request path. The p99 webhook delivery time is 4.2 seconds (measured over the last 14 days, source: Datadog dashboard `webhook-latency-v2`), and the p99 of the upstream API request that triggers the webhook is 5.1 seconds, of which 4.2 seconds is the webhook itself. We have received 11 customer-support tickets in the last 30 days mentioning webhook latency by name; three of those tickets escalated to refund requests (TICKET-2418, TICKET-2461, TICKET-2477).
>
> Two on-call pages in the last 30 days were caused by webhook traffic blocking the request thread pool: PAGE-844 (4 minutes downtime, 12,000 requests dropped) and PAGE-867 (9 minutes downtime, 28,000 requests dropped). Both incidents had the same root cause: the request thread pool reached saturation while waiting on slow third-party webhook receivers.
>
> ## Decision
>
> Move webhook delivery off the synchronous request path. Outbound webhooks are enqueued to a dedicated worker pool backed by Redis Streams. The synchronous request returns as soon as the event is durably enqueued (target enqueue latency: under 5 ms p99).
>
> ## Alternatives Considered
>
> ### Alternative A: stay synchronous, add a per-request timeout
>
> Cap each webhook call at 500 ms, drop on timeout, retry asynchronously.
>
> Rejected. The 500 ms cap would change behaviour for the 12% of webhooks that legitimately take longer than 500 ms because the receiver is intentionally slow (large payload validators on the customer side; this number is from the same Datadog dashboard's per-receiver percentile breakdown). Dropping those calls and retrying out-of-band would introduce delivery-order ambiguity, which our event-shape contract does not allow.
>
> ### Alternative B: asynchronous queue (chosen)
>
> Outbound webhooks are enqueued to Redis Streams. A dedicated worker pool delivers them with a configurable concurrency cap and an exponential-backoff retry policy.
>
> Rejected alternatives within this option: Kafka (over-engineering for the volume; we send roughly 50,000 webhooks per day, which Kafka would solve at the cost of a new operational technology), in-process goroutines without a durable queue (loses webhooks on process restart, which the SLA does not allow).
>
> ### Alternative C: do nothing
>
> Rejected. The two pages above and the three refund tickets per month are direct revenue impact; "do nothing" continues that bleed at a measurable cost.
>
> ## Consequences
>
> **Benefits**
>
> - Synchronous request p99 drops by approximately 4.2 seconds (a load-test on the staging queue measured a p99 of 0.92 seconds for the same workload).
> - Webhook delivery becomes resilient to receiver-side slowness; a slow receiver no longer holds a request thread.
> - Retries are now first-class: Redis Streams's consumer-group ack model gives at-least-once semantics with idempotent receivers.
>
> **Trade-offs**
>
> - One new operational dependency (Redis Streams). The team already runs Redis 7 for caching, so the operational cost is the streams-specific runbook (one new alert rule, one backup-validation step).
> - Webhook delivery becomes eventually-consistent. Customers who relied on the synchronous-confirmation timing must adapt; we surveyed the top 20 receivers (by webhook volume) and 18 already implement idempotent receivers, so the impact is limited.
>
> **Risks and mitigations**
>
> - *Risk*: a Redis cluster failure stops all webhook delivery. *Mitigation*: replicated Redis cluster (3 nodes, 1 leader, 2 followers, automated failover within 30 seconds). Monitor lag; page if delivery lag exceeds 60 seconds for more than 5 minutes.
> - *Risk*: a poison-pill webhook (a receiver that consistently returns 500) blocks the worker pool by retrying forever. *Mitigation*: max retry count of 16 with exponential backoff; after that, the webhook is moved to a dead-letter stream and surfaces as an alert plus a customer-facing email.

**Why this passes:**

- Every claim has a number, a source, or both. "p99 webhook delivery time is 4.2 seconds (Datadog dashboard `webhook-latency-v2`)" is verifiable; "Slow" was not.
- Customer impact is anchored to specific tickets (TICKET-2418, TICKET-2461, TICKET-2477). On-call impact is anchored to specific pages with measured downtime and dropped-request counts.
- Alternative A names a specific cap, cites a percentile breakdown for the dropped 12%, and explains the contract-level reason for rejection.
- Risks are concrete failure modes with concrete mitigations and concrete monitoring thresholds.

A reviewer applying the four gates can move past Evidence in one pass. The other three gates (Completeness, Clarity, Consistency) are also satisfied: every section is present, the decision is one declarative sentence at the top, and there are no contradictions with related ADRs (a Consistency check would compare the dead-letter behaviour with whatever the alerting ADR says, which is in scope of the review).

---

## Reading guide

The contrast is intentional: the decision in both versions is the same. Only the **discipline of citing evidence** differs. The Evidence gate is not asking for more text; it is asking for the text you have to be load-bearing.

When in doubt, ask of every adjective: "what number or source backs this?" If the answer is "I do not have one", either get one or remove the adjective.
