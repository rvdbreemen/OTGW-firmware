# Phase 2A: Security Findings

## Summary
- Critical: 0 | High: 0 | Medium: 2 | Low: 3 | Informational: 2

## Threat model calibration

This device is an ESP8266 firmware on a **LAN-only, NAT-isolated** network. There is no port-forwarding, no reverse proxy, no public internet reachability. The ADR explicitly mandates HTTP/WS only. Severity is calibrated against three realistic threat actors:

1. **The MQTT broker itself** (misconfigured or compromised Mosquitto/EMQX/cloud broker). This is the primary trust boundary — HA discovery is an intended publish/subscribe channel and brokers do get misconfigured (two HA instances, shared prefixes, stuck retained messages). Real findings against this path cap at **Medium/High** depending on impact.
2. **A LAN-side device already compromised by a separate attack.** This actor has already achieved LAN access by definition; anything they can do to the OTGW is strictly less severe than the compromise they already own. Capped at **Medium**.
3. **The user themselves misclicking.** Not a security finding; logged as UX/safety if relevant.

External internet attackers are **out of scope**. Plaintext HTTP/MQTT on LAN is **not a finding** (ADR design). Lack of auth on GET endpoints is **not a finding** (by design; the settings-gated admin password covers POST/PUT per ADR-054). OWASP Top 10 categories for web apps — SQLi, XSS, CSRF against public users, session mgmt, crypto — are not meaningfully applicable here and have been intentionally excluded from this review unless a deviation was found.

Phase 1A flagged a LOW on "verify callback topic filter has no NUL-termination bound". That has been re-verified against PubSubClient internals (`libraries/PubSubClient/src/PubSubClient.cpp:399`, `this->buffer[llen+2+tl] = 0;`) — the library guarantees the topic pointer is NUL-terminated before calling the user callback. That finding is downgraded to **verified clean** here.

## Context

This branch's new attack surface, evaluated against the model above:
- Two new **POST** endpoints (`/api/v2/discovery/verify`, `/api/v2/discovery/republish`) — both gated by `checkHttpAuth()` at `restAPI.ino:615-618`, identical to every other v2 mutation. Good.
- One new **GET** endpoint (`/api/v2/discovery`) — unauth by design, returns only counter state and the `auto_verify` settings bit. No secrets.
- One new **telnet debug char** (`V`) in `handleDebug.ino:77-83` — triggers `startDiscoveryVerification()`. Telnet is pre-existing unauth LAN surface; this adds a button that does strictly less than what `F` (force discovery) already does.
- One new **MQTT callback filter** (`MQTTstuff.ino:629-656`) — runs during the 15s verify window, intercepts retained discovery configs from broker. Reached only via broker messages; counters are `uint16_t`; heap-aborts at 4500 bytes; callback is O(prefix+node) with a `VERIFICATION_MAX_NODE_SEGMENT_LEN=64` cap explicitly added as a DoS guard.
- **Transient RX buffer realloc** to 1024 bytes during verify, restored on close. Pre-checked against `getMaxFreeBlockSize()` to avoid realloc-on-fragmented-heap; abort path restores on disconnect.

No new library dependencies. No platformio.ini or library.json changes. Supply chain surface unchanged.

## Findings

### [MEDIUM] Rapid-fire `/api/v2/discovery/republish` can indefinitely defer drip completion
- **Attack surface**: authenticated LAN client (post-auth), or LAN device that has cracked/reused the admin HTTP password.
- **File:line**: `src/OTGW-firmware/restAPI.ino:512-524`
- **Issue**: `markAllMQTTConfigPending()` is idempotent — multiple calls just re-seed the ~80-bit pending bitmap. The endpoint has no rate limit. A loop POSTing every 2 seconds will re-queue all discovery IDs faster than the drip (2s cadence normal, 10s slow) can drain them. Net effect: the discovery queue is never empty, `countPendingDiscoveryIds() > 0` stays true permanently, which in turn **blocks `startDiscoveryVerification()`** (precondition at `MQTTstuff.ino:216`). So the primary use of this endpoint can permanently lock out the verify endpoint. Also keeps lwIP pbuf pressure elevated.
- **Attack scenario**: a LAN actor who has already obtained the admin password runs `while true; curl -u admin:pw -X POST http://otgw/api/v2/discovery/republish; sleep 1; done`. OTGW stays in permanent discovery-drip mode. No crash, no data loss, but heap stays near the LOW tier and verify never runs. Realistic only post-auth-compromise — capped at Medium.
- **CWE**: CWE-770 (allocation without limits / rate-limiting)
- **Fix (cheap)**: add a cooldown timer tracking the last invocation epoch and reject subsequent POSTs for e.g. 60 seconds with 429:
  ```cpp
  // inside handleDiscovery republish branch
  static unsigned long lastRepublishMs = 0;
  constexpr unsigned long REPUBLISH_COOLDOWN_MS = 60000UL;
  if (lastRepublishMs != 0 && (millis() - lastRepublishMs) < REPUBLISH_COOLDOWN_MS) {
    sendApiError(429, F("Republish cooldown active, retry in 60s"));
    return;
  }
  lastRepublishMs = millis();
  ```
  Alternative: reject with 409 when `countPendingDiscoveryIds() > 0` (symmetrical with `/verify` precondition at line 501).

### [MEDIUM] Verify-window callback fall-through on broker-crafted non-discovery topics matching wildcard
- **Attack surface**: the MQTT broker itself (legitimate trust boundary; but a misconfigured broker or one shared with an attacker-controlled publisher).
- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:629-656`
- **Issue**: the verify filter routes to `return` ONLY when the topic matches the 3-segment structure `<haprefix>/<component>/<nodeId>/...`. If the broker publishes a retained message on a topic that:
  1. Starts with `<haprefix>/` (matches line 631)
  2. Contains exactly one `/` after the prefix (slash1 non-null at line 634, slash2 NULL at line 637)

  …the filter falls through to the regular command dispatcher below (lines 658+). This is a narrow case — the subscribe wildcard `<haprefix>/+/<nodeId>/#` should only match topics with the right structure, so the broker must either replay topics from the retained store that don't match the subscribe pattern, or the MQTT server must implement wildcard filtering incorrectly. Mosquitto/EMQX do not do this. However, the code defense is "trust broker delivers only matches" which is weaker than "validate structure and drop unknowns".
- **Attack scenario**: an attacker with write access to the broker (not the OTGW's threat model's biggest concern, but the documented trust boundary) publishes a retained message on `homeassistant/junk_with_command_payload` where the payload happens to parse as an OT command. During the 15s verify window it falls through to the command-topic dispatcher at line 692+. Whether this actually reaches the PIC depends on the topic parsing that follows. Realistic impact: **discovery verify counters are wrong AND a crafted topic can sneak a command**. Real crash risk is low — the downstream command parser has its own guards — but the architectural intent (filter fully routes these) is violated.
- **CWE**: CWE-20 (Improper input validation)
- **Fix**: make the filter return-on-prefix-match even if substructure doesn't parse — the intent is "anything on haprefix/ is not a command topic":
  ```cpp
  if (verifyActive && verifyPrefixLen > 0) {
    const char *prefix = CSTR(settings.mqtt.sHaprefix);
    if (strncmp(topic, prefix, verifyPrefixLen) == 0 && topic[verifyPrefixLen] == '/') {
      // This topic came from our verify subscription; count what we can,
      // but never fall through to the command dispatcher regardless of
      // whether substructure parses.
      const char *rest   = topic + verifyPrefixLen + 1;
      const char *slash1 = strchr(rest, '/');
      if (slash1) {
        const char *nodeStart = slash1 + 1;
        const char *slash2    = strchr(nodeStart, '/');
        if (slash2) {
          const size_t nodeLen = (size_t)(slash2 - nodeStart);
          if (nodeLen > VERIFICATION_MAX_NODE_SEGMENT_LEN) {
            verifyOrphanCount++;
          } else if (nodeLen == verifyNodeLen && strncmp(nodeStart, NodeId, nodeLen) == 0) {
            verifyReceivedCount++;
          } else {
            verifyOrphanCount++;
          }
        } else {
          verifyOrphanCount++;  // malformed but still under our prefix
        }
      } else {
        verifyOrphanCount++;    // malformed but still under our prefix
      }
      return;   // ALWAYS consume haprefix-matching topics during verify
    }
  }
  ```
  This is the minimum change. A belt-and-braces alternative is to also verify the topic ends in `/config` (HA discovery convention), but that couples more tightly to HA's URI scheme.

### [LOW] `verifyActive = true` without rollback on `state.discovery.bVerificationActive` / `iVerifyRunCount` drift if later path fails
- **Attack surface**: the broker — specifically, a broker that accepts SUBSCRIBE but immediately drops the socket before any PUBLISH arrives.
- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:252-257`
- **Issue**: the happy path increments `iVerifyRunCount++` and sets `bVerificationActive=true` before the first tick. If the broker then disconnects, `tickDiscoveryVerification()` at line 296-307 cleans up `verifyActive` and `bVerificationActive` but does NOT roll back the increment. Over time, broker flappiness inflates the "verify runs" counter without corresponding useful work. Not a security issue in the strict sense — it's a telemetry-inflation issue that degrades observability.
- **Attack scenario**: a flaky broker drops the connection during every verify window. The `disc_verify_runs` counter grows without bound, misleading the user that verification is happening successfully when it is not.
- **CWE**: CWE-778 (insufficient logging) — arguably; or N/A for pure telemetry.
- **Fix**: increment `iVerifyRunCount` in `endDiscoveryVerification()` (on real close) instead of `startDiscoveryVerification()`. Current Phase 1A MEDIUM finding ("Verify-window heap-abort masks failure as a clean pass") is the related telemetry issue — fix both together.

### [LOW] `handleDebugChar('V')` is reachable from unauth telnet
- **Attack surface**: LAN post-auth (telnet is plaintext port 23, no password).
- **File:line**: `src/OTGW-firmware/handleDebug.ino:77-83`
- **Issue**: telnet debug is unauth by existing design — every command character is a pre-existing LAN surface. `V` adds one more: it triggers `startDiscoveryVerification()`, which goes through the same preconditions as the REST POST. So `V` adds strictly nothing a LAN attacker couldn't already do via `F` (force-reconfigure-all, which is strictly more disruptive).
- **Attack scenario**: a LAN actor connects to telnet and presses `V` in a loop. Because `startDiscoveryVerification()` returns `false` when already active, the loop just flaps "refused" messages into the log until the 15s window expires.
- **CWE**: N/A — by-design unauth surface.
- **Fix**: no change required. This is flagged only for completeness.

### [LOW] `VERIFICATION_MIN_HEAP_START = 6000` / `_ABORT = 4500` are magic numbers duplicated in REST endpoint
- **Attack surface**: internal consistency — not directly exploitable.
- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:188-189` and `src/OTGW-firmware/restAPI.ino:499`
- **Issue**: `restAPI.ino:499` hardcodes `6000` instead of referencing `VERIFICATION_MIN_HEAP_START`. Phase 1A already flagged this as HIGH under Code Quality ("Two sources of truth"). From a security angle this is latent: if the heap guard threshold is lowered in MQTTstuff without updating restAPI, the REST endpoint will reject when the internal function would have accepted, or vice versa. Not exploitable in isolation, but can mask a tuning mistake.
- **Fix**: expose the constant via the header or an inline getter and use it in both places. See Phase 1A 1A-HIGH-3 for the full recommendation.

### [INFORMATIONAL] Hostile broker can flood verify window with retained messages
- **Attack surface**: the broker (trust boundary).
- **File:line**: `src/OTGW-firmware/MQTTstuff.ino:310-328` (tick), `629-656` (callback filter)
- **Analysis**: during the 15s window, a hostile broker can publish unlimited retained messages matching `<haprefix>/+/<nodeId>/#`. PubSubClient processes **one** inbound packet per `loop()` call (verified at `libraries/PubSubClient/src/PubSubClient.cpp:387-418`), so at the main-loop rate of ~500-1000 Hz the device is not starved of cooperative time — `feedWatchDog()` still fires. Counters are `uint16_t` but `verifyReceivedCount >= expected` triggers an early-close at `MQTTstuff.ino:323-325`, so a flood-of-matches actually **shortens** the window. A flood of orphans (nodes other than ours) runs the window to 15s but allocates nothing new — the 1024-byte RX buffer is reused per message. The `VERIFICATION_MIN_HEAP_ABORT=4500` gate provides a safety cut-off. Overall, the window is self-terminating and bounded. Not a finding.

### [INFORMATIONAL] REST POST endpoints inherit the existing unauth-when-empty-password behavior
- **Attack surface**: LAN post-compromise (if the user intentionally disables auth by leaving password empty, anyone on LAN can POST).
- **File:line**: `src/OTGW-firmware/restAPI.ino:110-126` (`checkHttpAuth`)
- **Analysis**: when `settings.sHTTPpasswd[0] == '\0'`, `checkHttpAuth()` returns `true` immediately. The new `/api/v2/discovery/verify` and `/republish` endpoints inherit this behavior — consistent with every other v2 POST endpoint (e.g., `/api/v2/otgw/...` command injection has the same model). This is documented user intent: the user has explicitly opted out of auth. Not a finding against this branch.

## Surfaces verified clean

The following dimensions were examined and found clean (no new issues introduced by this branch):

1. **Authentication/authorization consistency** — The new `/api/v2/discovery` routes go through the centralized `checkHttpAuth()` (POST/PUT) or are intentionally unauth (GET) at `restAPI.ino:615-618`. The pattern matches every other v2 route.
2. **CSRF protection on POSTs** — `checkHttpAuth()` invokes `isSameOriginRequest()` for authenticated requests; the new endpoints inherit this unchanged.
3. **MQTT topic NUL-termination bounds** — PubSubClient guarantees topic NUL-termination at `libraries/PubSubClient/src/PubSubClient.cpp:399` before dispatching the callback. `strncmp`/`strchr` at `MQTTstuff.ino:631-637` are therefore safe. Phase 1A LOW finding downgraded.
4. **PROGMEM pointer safety on new code** — Verify filter uses `strncmp` (not `strncmp_P`) on RAM-domain `topic` and `CSTR(settings.mqtt.sHaprefix)`; the `NodeId` symbol is also RAM. Domains match.
5. **Stack-buffer sizes for new `snprintf_P` calls** — `handleDiscovery` GET: 320 bytes vs worst-case JSON ~250 bytes; `/verify` response: 128 bytes vs ~90 bytes; `/republish` response: 96 bytes vs ~45 bytes; `sendMQTTheapdiag`: 384 bytes vs ~465 bytes worst-case with all 17 counters maxed (flagged in Phase 1A — add safety margin).
6. **Secrets in logs** — new `[verify]` debug traces log the wildcard topic (contains `sHaprefix` + `NodeId`, both non-secret) and counters only. No broker credentials, no payload content, no user identifiers.
7. **Supply-chain / dependencies** — no `platformio.ini`, `library.json`, or `libraries/*` changes on this branch. PubSubClient unchanged.
8. **Integer wraparound on new counters** — `iPublishedTopicCount`, `iVerifyRunCount`, `iRepublishTriggeredCount`, and the `heapdiag` counters are `uint32_t`. At current event rates (hundreds/day at most) they need millennia to wrap. `iLastMissingCount` / `iLastOrphanCount` are `uint16_t`, but they hold the PER-WINDOW count (not cumulative), so they are bounded by the number of retained configs under `<haprefix>/` — on realistic HA deployments a few hundred at most.
9. **Re-entrancy of `handleMQTTcallback`** — the new verify-filter block uses only file-local statics (`verifyActive`, `verifyPrefixLen`, `verifyNodeLen`, `verifyReceivedCount`, `verifyOrphanCount`). No shared buffer with `mqttAutoCfgScratch` or `ot_log_buffer`. The callback cannot be re-entered (single-threaded cooperative model + `MQTTclient.loop()` processes one packet per call).
10. **Status-burst flag exposure** — `isStatusBurstActive()` has a self-healing 500ms timeout (`MQTTstuff.ino:119-125`). Even if `endStatusBurst()` is skipped by an exception path, the flag clears on its own. No long-term stuck state.
11. **OWASP Top 10 categories explicitly scoped out** — SQLi (no DB), XSS (no reflected user content in UI), session management (no sessions), CSRF against browsers (POST is CSRF-checked; GET is read-only), SSRF (no outbound fetches driven by user input), open redirects (no redirect logic), cryptography (none in scope — LAN + VPN trust model per ADR), TLS (HTTP/WS only by design).

## Quick security merge-readiness

**Security alone does not block the merge.** No Critical or High. The two Mediums are:
- Rate limit `/republish` (small, cheap, recommended before merge).
- Verify-callback fall-through guard (small, recommended before merge — the right fix is a 5-line restructure).

Both are realistic-impact Low in practice, marked Medium because they touch the broker-trust boundary which is the device's real security edge.

The branch does not introduce new cryptographic risk, new authentication logic, or new unauth external surface. It does extend two existing surfaces (REST v2 and MQTT callback), but both are gated appropriately for the LAN threat model.
