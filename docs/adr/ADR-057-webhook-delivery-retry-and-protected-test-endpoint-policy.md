# ADR-057: Webhook Delivery, Retry, and Protected Test Endpoint Policy

**Status:** Proposed  
**Date:** 2026-03-21  
**Supersedes:** ADR-055

## Context

ADR-055 introduced outbound HTTP webhooks for local-network integrations such as Home Assistant, Shelly, Node-RED, OpenHAB, and Domoticz. Since then, the implemented behavior has moved beyond the original ADR in several important ways:

1. Delivery is no longer described accurately by a single blocking request model.
2. Webhook sending now uses a cooperative state-machine pattern with bounded retry behavior.
3. The HTTP timeout has been reduced to fit the local-LAN assumption better.
4. The webhook test endpoint is no longer effectively public when admin protection is configured.
5. The current operator-facing policy spans ADR-055, ADR-048, code, and feature docs.

A superseding ADR is needed so the documented webhook contract matches current implementation and current security posture.

## Decision

Define the webhook feature as a best-effort, local-network, edge-triggered outbound HTTP delivery mechanism with bounded retry and a protected test endpoint.

### 1. Trigger model

The webhook monitors one configured OpenTherm StatusFlags bit and fires on edge transitions:

- OFF to ON
- ON to OFF

The default trigger is bit 1, matching central-heating-active behavior.

The feature is edge-triggered, not periodic. It does not send on every loop iteration or every OpenTherm frame.

### 2. Delivery method selection

The HTTP method is selected from configuration:

- Empty payload: HTTP GET
- Non-empty payload: HTTP POST with configured content type

Payload templates expand supported `{variable}` placeholders using current OpenTherm state at send time.

### 3. Local-only outbound policy

Every outbound webhook attempt must pass local-network URL validation:

- Scheme must be `http://`
- Dotted IPv4 addresses must be RFC1918 or link-local
- Loopback `127.x.x.x` is rejected
- Public IPv4 addresses are rejected
- Hostnames are allowed within the trusted local-network model

Policy rejection is treated as a non-retryable outcome. The firmware logs the block and does not keep retrying a URL that violates the local-network policy.

This preserves the ADR-003 and ADR-032 deployment model: OTGW is a local-network HTTP appliance and must not become a public-webhook client.

### 4. Cooperative send and timeout policy

Webhook delivery uses the cooperative, non-blocking scheduling pattern established by ADR-048.

Per send attempt:

- `attemptSendWebhook()` performs one actual HTTP request
- Request timeout is 1000 ms
- Watchdog feeding and yields are preserved around the request path
- HTTP 2xx is considered success
- Any other HTTP result or transport failure is considered failure

A missing configured URL for the requested state is treated as "nothing to send" and is not retried.

### 5. Retry policy

The retry contract is:

- Initial send attempt when the state machine reaches pending state
- If the attempt fails, retry after 30 seconds
- Maximum 3 total attempts
- Retries only proceed when WiFi is connected
- After the final failure, the event is dropped and the machine returns to idle

This is best-effort delivery, not guaranteed delivery.

The implementation retains one pending transition at a time. It is not a queued event system and does not guarantee retention of every intermediate state change while a retry cycle is still unresolved.

### 6. Protected test endpoint

The webhook test endpoint is part of the admin boundary.

Contract:

- Route: `POST` or `PUT /api/v2/webhook/test?state=on|1|off|0`
- `state` is required
- Invalid or missing `state` returns HTTP 400
- Valid input executes the same configured webhook logic for the requested state

When protected admin endpoints are enabled:

- The test endpoint requires the admin password
- The test endpoint also passes the same same-origin check used by protected admin API routes

When protected admin endpoints are disabled:

- The test endpoint remains available under the baseline local-network model

### 7. Relationship to ADR-048

ADR-048 remains the mechanism-level decision for using a webhook state machine with retry on a cooperative single-core firmware loop.

This ADR supersedes ADR-055 as the policy-level contract for:

- What counts as a delivery attempt
- What gets retried
- What is non-retryable
- How the test endpoint is exposed
- How local-only policy and protected admin controls interact with webhook delivery

## Alternatives Considered

### Alternative 1: Keep ADR-055 unchanged

**Pros:**

- No new ADR required
- Preserves the original introduction of webhook support

**Cons:**

- Leaves the old blocking/non-retry framing in place
- Incorrectly describes the test endpoint as effectively open
- Does not define current retry behavior
- Does not capture the relationship with ADR-048

**Why not chosen:** ADR-055 no longer describes the implemented behavior precisely enough.

### Alternative 2: Make webhooks synchronous single-shot only

**Pros:**

- Simpler implementation story
- Easier to reason about per edge event

**Cons:**

- More main-loop blocking
- No resilience to brief target outages
- Regresses current implementation

**Why not chosen:** The firmware already moved to bounded retry and cooperative delivery for good reason.

### Alternative 3: Add queued guaranteed delivery for every event

**Pros:**

- Better retention of rapid consecutive edge changes
- Stronger delivery semantics

**Cons:**

- More RAM and state complexity
- More difficult failure and restart behavior
- Out of proportion to OTGW's best-effort local automation use case

**Why not chosen:** OTGW webhook delivery is intentionally lightweight and best-effort.

### Alternative 4: Allow public internet or HTTPS webhook targets

**Pros:**

- Direct integration with cloud webhook services
- Fewer relay components for users

**Cons:**

- Violates ADR-003 and ADR-032 deployment assumptions
- Increases heap pressure and complexity
- Encourages unsafe internet-facing behavior

**Why not chosen:** OTGW remains local-network-only for both inbound and outbound HTTP use.

## Consequences

### Positive

1. The documented webhook policy now matches the current delivery and retry behavior.
2. Retry behavior is bounded and understandable for operators.
3. Blocking risk is reduced by the 1-second timeout.
4. The test endpoint now aligns with the protected-admin boundary when enabled.
5. Local-only outbound policy remains explicit and defensible.

### Negative

1. Delivery is still best-effort, not guaranteed.
2. At most one pending transition is retained at a time.
3. A 30-second retry interval can delay eventual success.
4. Hostname-based local targets are trusted by LAN naming policy rather than by cryptographic identity.
5. The feature still depends on plaintext HTTP because HTTPS remains out of scope.

### Risks & Mitigation

**Risk 1:** Users expect guaranteed webhook delivery.  
**Mitigation:** This ADR explicitly documents best-effort semantics and bounded retry.

**Risk 2:** Users interpret the test endpoint as harmless even when it can trigger local actions.  
**Mitigation:** Keep it inside the protected admin boundary when admin protection is enabled.

**Risk 3:** Users try to target public internet services directly.  
**Mitigation:** Keep local-only URL validation mandatory and document use of local relays when public services are desired.

## Related Decisions

- **ADR-003:** HTTP-Only Network Architecture (No HTTPS)
- **ADR-007:** Timer-Based Task Scheduling
- **ADR-032:** No Authentication Pattern (Local Network Security Model)
- **ADR-048:** Non-Blocking Webhook State Machine with Retry
- **ADR-055:** Webhook Outbound HTTP Integration
- **ADR-056:** Protected Admin Endpoint Security and Secret-Handling Contract

## References

- `src/OTGW-firmware/webhook.ino`
- `src/OTGW-firmware/restAPI.ino`
- `docs/features/webhook.md`
- `docs/adr/ADR-048-nonblocking-webhook-state-machine.md`
- `RELEASE_NOTES_1.2.0.md`
- `RELEASE_GITHUB_1.2.0.md`
