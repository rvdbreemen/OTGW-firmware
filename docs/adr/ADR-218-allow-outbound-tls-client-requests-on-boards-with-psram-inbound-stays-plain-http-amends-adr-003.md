---
id: "ADR-218"
title: "Allow outbound TLS client requests on boards with PSRAM, inbound stays plain HTTP (amends ADR-003)"
status: "Proposed"
date: "2026-11-08"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "tls"
  - "https-client"
  - "psram"
  - "mbedtls"
  - "ca-bundle"
aliases:
  - "outbound https"
  - "TLS client"
components:
  - "outbound TLS client capability"
symbols: []
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-218 Allow outbound TLS client requests on boards with PSRAM, inbound stays plain HTTP (amends ADR-003)

## Status

Proposed, 2026-11-08.

## Status History

```yaml
status_history:
  - date: 2026-11-08
    status: Proposed
    changed_by: Agent (Claude, TASK-1689)
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

ADR-003 (2018, ESP8266 era) rules out HTTPS and WSS (WebSocket Secure) for the gateway: the device serves plain HTTP on a trusted LAN. Robert asked on 2026-11-07 to revisit that for the ESP32-S3 line (TASK-1689). This ADR covers only **outbound** TLS (Transport Layer Security): the gateway as an HTTPS client. Inbound web, REST and WebSocket stay plain HTTP as ADR-003 says.

Proof of concept, 2026-11-07 (branch poc-tls-1689, not merged; `tlsPocStuff.ino`, one-shot task with 12 KB stack, `NetworkClientSecure`, GET to raw.githubusercontent.com and cdn.jsdelivr.net):

- The prebuilt Arduino-ESP32 3.3.5 libraries set `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC` and `MBEDTLS_SSL_MAX_CONTENT_LEN 16384`. `mbedtls_ssl_setup()` allocates an input and an output record buffer of about 16.7 KB each, each one contiguous block, before any byte goes over the network.
- The largest free internal block on both bench units was 31,732 B (free internal about 53 KB, fragmented). Without help every handshake failed with `-0x7F00` (SSL alloc failed) after 25-110 ms: .63 9/9, .61 (no PSRAM) 5/5.
- `esp_config.h` maps the mbedTLS allocator to a function, so `mbedtls_platform_set_calloc_free()` can redirect it at runtime. With blocks of 1 KB or more sent to PSRAM (fallback internal): .63 pin-mode 3/3, CA-bundle 2/2, jsDelivr 2/2, all HTTP 200; connect + handshake 706-823 ms; internal low point 23.5-27.6 KB; PSRAM peak about 50-54 KB; task stack used about 4.4 of 12 KB. 20 back-to-back fetches: internal free constant at 37,524 B, PSRAM stable.
- Flash: the TLS client code adds about 96 KB; the full ESP-IDF CA bundle another 69 KB (app slot 2,293,760 B, already about 93% used, GitHub #694). A mini bundle of 14 roots from IDF 5.5.1 `cacrt_all.pem` is 5,427 B and validates all six target hosts (commit 1dd2007, `scripts/tls/`, `tests/test_tls_mini_bundle.py` 7/7; anchors ISRG Root X1/X2, USERTrust RSA/ECC).

**What would use it today: nothing.** Checked 2026-11-08 in `src/OTGW-firmware`: the outbound `HTTPClient` users are the PIC firmware check and download (`OTGW-Core.ino:7017-7063`, plain HTTP) and the webhook (`webhook.ino:224`, `http://` only). The web UI language packs of TASK-1689 are fetched by the **browser** from jsDelivr (an `http://` page may load `https://` resources), so they need no TLS in the firmware. Candidate future users: HTTPS-only webhook targets (ntfy.sh, Pushover, Discord, IFTTT), the PIC firmware site if it ever goes HTTPS-only, a firmware-side update check.

## Decision

**User Decision (Robert, 2026-11-07), parked 2026-11-08:** when a firmware feature needs an HTTPS request, the gateway may act as a TLS client only on boards with PSRAM, detected at runtime, with the mbedTLS allocator redirected to PSRAM and a mini CA bundle of about 15 roots; on boards without PSRAM that feature uses a reasonable fallback or is disabled with an explanation in the UI. Inbound HTTP/WS stays as ADR-003 says.

This ADR stays **Proposed** until a concrete feature asks for it (most likely HTTPS webhooks). Accepting it before then would spend about 100 KB of a nearly full app slot on code nothing calls (YAGNI).

## Decision Contract

### Must

- Run TLS client work off the async_tcp task (ADR-137), with its own task stack of at least 8 KB.
- Detect PSRAM at runtime; install the PSRAM allocator redirect before the first handshake.
- Trust only the mini CA bundle built by `scripts/tls/` from the pinned IDF list.

### Must Not

- Serve HTTPS or WSS inbound.
- Attempt a handshake on a board without PSRAM.
- Disable certificate verification.

### Exceptions

- None.

### Verification

- A host test of the PSRAM capability gate and the fallback per feature; a bench fetch on a PSRAM board and a refusal on a no-PSRAM board, as in the PoC.

## Alternatives Considered

- **Keep ADR-003 as is (no TLS at all).** Chosen for now in practice: nothing needs it. Loses HTTPS webhooks for good, hence this record.
- **TLS on every board, internal memory only.** Fails: the two 16.7 KB record buffers do not fit the 31,732 B largest block (measured, -0x7F00 on 14/14 runs).
- **Smaller record buffers (`SSL_IN/OUT_CONTENT_LEN 4096`) or `MBEDTLS_DYNAMIC_BUFFER`.** Needs self-built IDF libraries (pioarduino `custom_sdkconfig`), a heavier build, and the server must honour max-fragment-length or `record_size_limit`; not tested against GitHub or jsDelivr.
- **Full ESP-IDF CA bundle.** 69 KB of flash for about 140 roots where 4 are in use; rejected by Robert for the mini bundle.

## Consequences

**Positive:**

- A measured, ready recipe when HTTPS webhooks or another fetch is asked for: PSRAM redirect, mini bundle, 0.7-0.8 s per connect.

**Negative:**

- About 96 KB code plus 5.4 KB roots in the app slot once implemented (#694 headroom). Mitigation: only when a feature needs it.
- Two feature levels (PSRAM / no PSRAM) to document and test. Mitigation: one runtime capability flag and a UI explanation per feature.
- Roots expire or CDNs change chains. Mitigation: the mini-bundle test checks the anchors per host; 10 of 14 roots are headroom.

## Open Questions

- [ ] Robert: which feature triggers acceptance (HTTPS webhooks first?), and does it then become its own task.

## Related Decisions

- **ADR-003 (HTTP-Only Network Architecture)**: amended for outbound client requests only; its inbound rule stays.
- **ADR-137 (Defer outbound HTTP from async handlers)**: TLS work runs in the same deferred, non-async_tcp context.

## References

- TASK-1689 notes and `docs/research/2026-11-07-web-ui-languages-research.md` (update note on the ADR-003 revisit).
- PoC branch poc-tls-1689 (local), images kept in `wt-tlspoc-keep`; mini bundle commit 1dd2007 (lane-1689tls).
- `src/OTGW-firmware/OTGW-Core.ino:7017-7063`, `src/OTGW-firmware/webhook.ino:224` (current outbound HTTP users).
- Mbed TLS `MBEDTLS_SSL_MAX_CONTENT_LEN`: https://mbed-tls.readthedocs.io/en/latest/kb/how-to/reduce-mbedtls-memory-and-storage-footprint/
