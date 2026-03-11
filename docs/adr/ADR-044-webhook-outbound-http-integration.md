# ADR-044: Webhook Outbound HTTP Integration

**Status:** Accepted  
**Date:** 2026-03-02

## Context

Home automation platforms (Home Assistant, Domoticz, OpenHAB, Node-RED) and smart-home devices (Shelly, Tasmota) often expose HTTP endpoints that can be called to trigger automations or control devices. The OTGW already publishes boiler state to MQTT (ADR-006), but MQTT requires a broker and a subscription. Some users have simpler setups — a Shelly relay, an HA webhook automation, a Node-RED flow — where a plain HTTP call is easier to integrate than configuring an MQTT subscription.

### Problem

There was no way for the OTGW to proactively call external HTTP endpoints when a boiler state changes. Users had to poll the REST API or subscribe to MQTT, both of which require client-side infrastructure.

### Constraints

- **HTTP only:** TLS (BearSSL) consumes 20–30 KB of heap — over 50% of available RAM — making HTTPS impractical on ESP8266 (ADR-003, ADR-001).
- **Local-network only:** The device is a local-network appliance (ADR-032); outbound calls to public internet addresses contradict the security model.
- **Single-core cooperative scheduling:** A blocking HTTP call must not freeze the main loop. A 3-second timeout and `yield()` calls are required (ADR-007).
- **Memory:** Static buffers required; no heap allocation for URL, payload, or template expansion (ADR-004).
- **Trigger mechanism:** StatusFlags bit layout is already established by ADR-022 (GPIO output bit-flag control) and should be reused for consistency.

## Decision

**Implement a configurable outbound HTTP webhook that fires once on each rising or falling edge of a selected OpenTherm StatusFlags bit.**

Key design choices:

1. **Method selection by payload presence:** If `WebhookPayload` is empty, send HTTP GET (compatible with Shelly Gen1, Domoticz JSON API). If `WebhookPayload` is set, send HTTP POST with `Content-Type` from `WebhookContentType` (compatible with Shelly Gen2, HA, OpenHAB, Node-RED).

2. **Template expansion:** `{variable}` placeholders in the payload template are expanded to live OpenTherm values at send time (e.g. `{tboiler}`, `{state}`, `{tr}`, `{flameon}`). No heap allocation: a single `static char expandedPayload[201]` buffer is reused.

3. **Local-only URL enforcement:** `isLocalUrl()` validates every URL before the HTTP call:
   - Scheme must be `http://` (never `https://`)
   - If the host is a bare IPv4 address, it must be RFC1918 (10.x, 172.16–31.x, 192.168.x) or link-local (169.254.x)
   - Hostnames (letters/digits/dashes/dots) pass without IP-range check — local DNS on a trusted LAN is acceptable (ADR-032)
   - Loopback (127.x) is rejected to prevent feedback loops

4. **Edge-only trigger:** The webhook fires once on ON→OFF or OFF→ON transition. `webhookInitialized` suppresses the first call (boot-time false trigger prevention).

5. **Six settings:** `WebhookEnabled`, `WebhookURLon`, `WebhookURLoff`, `WebhookTriggerBit` (0–15, defaults to bit 1 = slave CH mode), `WebhookPayload`, `WebhookContentType`.

6. **REST test endpoint:** `POST /api/v2/webhook/test?state=on|off` fires the webhook unconditionally for configuration testing.

## Alternatives Considered

### Alternative 1: MQTT-only integration
**Pros:**
- Already implemented (ADR-006)
- Bidirectional; supports multiple subscribers

**Cons:**
- Requires MQTT broker (Mosquitto, HA Mosquitto add-on)
- Shelly and similar devices cannot subscribe to MQTT without extra firmware
- Adds overhead for users who only want simple local HTTP triggers

**Why not chosen:** MQTT is the right solution for rich integrations, but too heavyweight for simple relay-on/relay-off scenarios.

### Alternative 2: WebSocket push events
**Pros:**
- Already exists (ADR-005); clients connected to port 81 already receive OT frames

**Cons:**
- Requires a WebSocket client to be permanently connected
- No standard support in automation platforms as a server-push receiver

**Why not chosen:** The source side (OTGW) already pushes events; the problem is the consumer side has no WebSocket listener in most setups.

### Alternative 3: Full outbound HTTPS support
**Pros:**
- Would allow direct calls to Discord, cloud APIs, public webhooks

**Cons:**
- BearSSL on ESP8266 requires 20–30 KB heap (>50% of available RAM); combined with TLS session state can cause out-of-memory crashes (ADR-003, ADR-001)
- Violates the "local-network appliance" security model (ADR-032)
- Users with public internet targets should use a local relay (Node-RED, HA automation)

**Why not chosen:** Memory cost is prohibitive; ADR-003 explicitly excludes HTTPS.

### Alternative 4: Send webhook on every OpenTherm cycle (not just on change)
**Pros:**
- Simpler implementation (no state tracking)
- Consumers always have fresh data

**Cons:**
- OpenTherm frames arrive every ~1 second; 86,400 HTTP requests/day would overload local devices (Shelly, HA)
- Blocking HTTP calls at 1 Hz would saturate the main loop

**Why not chosen:** Edge-triggering (on change) reduces load by orders of magnitude and matches the typical automation pattern (start action when heating turns on, stop action when it turns off).

## Consequences

**Positive:**
- Enables zero-MQTT integrations with Shelly, Domoticz, OpenHAB, HA webhooks, Node-RED
- Reuses existing StatusFlags bit layout (ADR-022) for consistent configuration
- Template expansion allows rich payloads from a single call
- Local-only URL enforcement preserves the security model (ADR-003, ADR-032)

**Negative:**
- Blocking HTTP calls (up to 3 seconds on failure) introduce latency spikes in the main loop
- Only one trigger bit can be monitored; multiple simultaneous conditions require separate MQTT subscriptions
- No retry on failure: a missed webhook (WiFi glitch, target unreachable) is silently dropped

**Risks and Mitigation:**
- *Main loop stall:* Mitigated by 3-second `http.setTimeout()` and `yield()` calls around request/response; watchdog is fed after the call (ADR-011).
- *Template injection / URL injection:* Template expansion only replaces known variable names; unknown `{placeholders}` are passed through as literal text. URL validation rejects non-local targets before any connection is attempted.
- *Config portal exposure:* The webhook test endpoint (`/api/v2/webhook/test`) is unauthenticated, consistent with the rest of the API (ADR-032). It can only call URLs that already pass `isLocalUrl()`.

## Related Decisions

- **ADR-003:** HTTP-Only Network Architecture — prohibits HTTPS, enforced by `isLocalUrl()`
- **ADR-004:** Static Buffer Allocation — `expandedPayload` is a `static char[201]`
- **ADR-006:** MQTT Integration Pattern — complementary push integration (MQTT vs HTTP)
- **ADR-007:** Timer-Based Task Scheduling — webhook evaluation runs in the main `loop()`
- **ADR-011:** External Hardware Watchdog — watchdog fed after potentially slow HTTP call
- **ADR-022:** GPIO Output Bit-Flag Control — StatusFlags bit layout reused for trigger selection
- **ADR-032:** No Authentication / Local Network Security — local-only URL policy applied to outbound calls

## References

- Implementation: `src/OTGW-firmware/webhook.ino`
- Settings: `src/OTGW-firmware/OTGW-firmware.h` (lines 254–263)
- Settings persistence: `src/OTGW-firmware/settingStuff.ino` (lines 228–233, 533–540)
- Feature documentation: `docs/features/webhook.md`
- REST test endpoint: `src/OTGW-firmware/restAPI.ino` (`/api/v2/webhook/test`)
- Introduced in: v1.2.0, merged via `dev-1.2.0-stable-version-adding-webhook`
