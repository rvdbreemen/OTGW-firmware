---
id: "ADR-221"
title: "Language packs may be gzipped on LittleFS (amends ADR-194 for language packs only)"
status: "Accepted"
date: "2026-11-08"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "i18n"
  - "gzip"
  - "littlefs"
  - "language-pack"
aliases:
  - "gzipped language packs"
components:
  - "language pack storage"
symbols: []
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-221 Language packs may be gzipped on LittleFS (amends ADR-194 for language packs only)

## Status

Accepted, 2026-11-08.

## Status History

```yaml
status_history:
  - date: 2026-11-08
    status: Proposed
    changed_by: Agent (Claude, TASK-1689)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-08
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Consistency fix ADR-219 vs ADR-194; accepted by Robert in session
    changed_via: adr-kit lifecycle
```

## Context

ADR-219 (Accepted 2026-11-08) ships the Dutch (NL) and German (DE) language packs "gzipped on LittleFS". ADR-194 (Accepted) says every LittleFS asset other than PROGMEM text stays "never gzipped", and its Decision Contract forbids gzip or any other compression of an asset "in firmware or on LittleFS" (ADR-190 directive). ADR-215 later allowed exactly one gzipped file per web page (`v2.html.gz`, `FSexplorer.html.gz`) and did not mention other files. ADR-219 did not name this conflict; a verifier in the TASK-1689 language research found it.

Measured on 2026-11-08 on unit .61 (`GET /api/listfiles`, beta.142): LittleFS 786,432 B total, 387,072 B used, 399,360 B free. The pages take 171,973 B (`v2.html.gz`) and 6,017 B (`FSexplorer.html.gz`). Estimated pack size (TASK-1689 research, not measured): 13-16 KB gzip or about 40-50 KB plain per language, so both forms fit.

## Decision

**User Decision (Robert, 2026-11-08):** the web UI language packs (`i18n/<lang>.json`, ADR-219/ADR-220) are stored on LittleFS as gzip files (`<lang>.json.gz`) and served with `Content-Encoding: gzip`, as ADR-215 does for the pages; for every other asset ADR-194's no-gzip rule stands.

The browser decompresses the pack, so the device never inflates it. Gzip is chosen for the bytes over Wi-Fi and the LittleFS share, not because the space would otherwise run out (see the measurement above).

## Decision Contract

### Must

- Serve each gzipped pack with `Content-Encoding: gzip` and an ETag, like the ADR-215 pages.
- Build the `.gz` packs in `build.py` from the `i18n/` sources; never commit `.gz` files.

### Must Not

- Gzip any other LittleFS asset under this ADR.
- Inflate a pack on the device.

### Exceptions

- None.

### Verification

- A build test that the LittleFS staging contains `nl.json.gz` and `de.json.gz` and no other new `.gz` file; a web UI test that the real `t()` loads a gzipped pack served with `Content-Encoding: gzip`.

## Alternatives Considered

- **Store the packs uncompressed.** Keeps ADR-194 untouched and fits (about 80-100 KB for two languages against 399,360 B free), but sends about three times the bytes to every browser. Rejected by Robert.
- **Inline the packs into `v2.html.gz`.** One file, but every user downloads every language. Rejected.

## Consequences

**Positive:**

- About 26-32 KB of LittleFS for NL and DE (estimate) instead of about 80-100 KB, and smaller transfers to the browser.

**Negative:**

- A second gzip exception next to ADR-215 makes ADR-194's rule narrower. Mitigation: the exception names language packs only, and the build test fails on any other new `.gz`.

## Related Decisions

- **ADR-194 (Static text assets from firmware flash)**: amended for language packs only.
- **ADR-215 (One gzipped file per page)**: the same serving pattern.
- **ADR-219 (Web UI languages)** and **ADR-220 (i18next v4 pack format)**: the packs this ADR stores.

## References

- TASK-1689 notes; `docs/research/2026-11-07-web-ui-languages-research.md`.
- LittleFS measurement: `GET http://192.168.88.61/api/listfiles`, 2026-11-08, beta.142+4f7fd77.
