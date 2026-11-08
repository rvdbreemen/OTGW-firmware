---
id: "ADR-219"
title: "Translate the web UI with a t() helper, English built in, NL and DE packs on LittleFS"
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
  - "web-ui-language"
  - "translation"
  - "language-pack"
aliases:
  - "internationalisation"
  - "web UI translation"
  - "taalpakket"
components:
  - "web UI translation mechanism"
symbols:
  - "t()"
  - "applyI18n"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-219 Translate the web UI with a t() helper, English built in, NL and DE packs on LittleFS

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
    reason: Robert's four language decisions 2026-11-08; accepted in session
    changed_via: adr-kit lifecycle
```

## Context

The v2 web UI (ADR-192) is English only. Robert wants it multilingual, starting with Dutch (NL) and German (DE), without large flash growth, and asked whether packs can come from GitHub (TASK-1689, 2026-11-07). The research report `docs/research/2026-11-07-web-ui-languages-research.md` (six angles, each re-checked by a verifier) found:

- Firmware projects that ship web UI languages keep them in the web layer: EMS-ESP32 (12 locales, browser language, per-browser override), OpenDTU (en/de/fr bundled, other languages as runtime packs, English per key). Tasmota translates at compile time (one image per language) and decided never to translate MQTT (Message Queuing Telemetry Transport) or JSON. WLED (an open-source LED controller firmware) has no translations; a GitHub-fetch PR (#4274) was closed. None fetches packs from GitHub at runtime in a released build.
- Libraries that work without a bundler (v2.js is inlined by `scripts/webui_pages.py`, ADR-215) cost 1-20 KB gz (i18next 27.0.0: 15,249 B gz); a `t()` helper with `Intl.PluralRules` costs an estimated 0.3-0.8 KB.
- The device cannot fetch from GitHub or jsDelivr itself: both answer HTTP with a 301 to HTTPS, and outbound TLS is parked in ADR-218. The browser can: jsDelivr serves CORS (Cross-Origin Resource Sharing) `*`, a fetch with an SRI (Subresource Integrity) hash works cross-origin, and an `http://` page may fetch `https://` resources.
- Packs that live outside the feature workflow decay (OpenDTU-OnBattery: 1,261 English keys, French 879). A missing-key check is needed.

Size: about 630 strings including the settings help (`settingsHelp.h`, 17,683 B raw, 6,766 B gz); estimated 13-16 KB gz per language on LittleFS (estimate, not measured).

## Decision

**User Decision (Robert, 2026-11-08):** the web UI is translated in the browser by a small `t(key, params)` helper in v2.js with English built in as the per-key fallback; NL and DE ship as gzipped packs on LittleFS, other languages may later be fetched by the browser from jsDelivr pinned to the firmware tag with SRI; the language follows the browser with a per-browser override; only the web UI (including the settings help) is translated.

- **Mechanism:** flat dotted keys (`nav.settings`, `help.<setting>`), `{name}` placeholders, plurals as `key_one`/`key_other` through `Intl.PluralRules`. HTML uses `data-i18n` (and `-title`, `-placeholder`) attributes with one `applyI18n(root)` pass after each render; JS-built text calls `t()`. No library.
- **Packs:** `i18n/<lang>.json` at the repo root (outside `src/`, so pack edits need no firmware bump); `build.py` gzips NL and DE into the LittleFS image. A missing key falls back to English for that key only.
- **Language choice:** `navigator.languages`, first supported base tag, else English; an override in the UI stored in `localStorage` key `otgw.lang` (per browser; no device setting, no REST change).
- **Scope:** all visible web UI text and the settings help. Not translated: MQTT topics, Home Assistant entity names, REST field names, telnet and logs, the update and safeboot pages (contracts and support stay English, as Tasmota decided).
- **Translation:** English in the repo is the source. On every change an AI draft of NL and DE is made; Robert reviews NL, a German-speaking tester reviews DE. A test fails when a pack misses a key or a placeholder. Hosted Weblate (libre plan) only when more languages are added.

## Decision Contract

### Must

- Keep English complete in the repo and use it per key when a pack lacks the key.
- Gate every pack with a test for missing keys and placeholder mismatches.
- Pin any remote pack URL to the firmware tag and check it with an SRI hash built into the firmware.

### Must Not

- Translate MQTT topics, Home Assistant entity names, REST fields, telnet output or logs.
- Add an i18n library to the web UI bundle.
- Let the device itself download packs (no TLS, ADR-003/ADR-218).

### Exceptions

- None.

### Verification

- A web UI test that runs the real v2.js helper: an unsupported browser language gives English, a supported one gives the pack, a missing key falls back per key (TASK-1689 AC2).
- A pack-coverage test over `i18n/*.json` against the English key set.

## Alternatives Considered

- **An i18n library (i18next, typesafe-i18n).** 1-20 KB gz for features two languages do not need; typesafe-i18n needs a bundler. Rejected.
- **Compile-time languages (Tasmota).** One image per language multiplies the release assets and the bench work. Rejected.
- **All packs from jsDelivr.** Zero bytes on the device, but no offline use and the Home Assistant ingress CSP (Content Security Policy) for such a fetch is untested. Rejected for NL/DE, kept for later languages.
- **Language as a device setting.** One language for everyone and a settings contract change; it ignores the browser. Rejected by Robert.
- **Hosted Weblate from day one.** Needs accounts and volunteers before there is a need. Deferred.

## Consequences

**Positive:**

- NL and DE work offline from the device, with no app-slot growth beyond the helper (about 0.3-0.8 KB, estimate).
- Contracts (MQTT, REST, Home Assistant) are unchanged, so integrations and support logs stay comparable.

**Negative:**

- About 26-32 KB gz of LittleFS for two languages (estimate). Mitigation: measure in the first implementation step against the 768 KB LittleFS partition.
- Every UI text change needs a key and a review in two languages. Mitigation: the coverage test and the per-key English fallback keep a missing translation visible but harmless.
- The settings help moves into the packs as `help.<setting>` keys; the PROGMEM copy (ADR-194) stays for English until the move is measured.

## Open Questions

- [x] Robert: accept this ADR (TASK-1689 AC1). — **Answered 2026-11-08 by Robert van den Breemen:** Yes, accepted (Robert, session 2026-11-08, after the acceptance packet).

## Related Decisions

- **ADR-192 (Ship only the v2 web UI)**: this ADR adds languages to that UI.
- **ADR-194 (Static text assets from firmware flash)**: the settings help is such an asset today.
- **ADR-215 (One gzipped file per page)**: the helper lives inside the inlined v2 bundle; packs are separate gzipped files.
- **ADR-218 (Outbound TLS, parked)**: the reason the device does not fetch packs itself.

## References

- `docs/research/2026-11-07-web-ui-languages-research.md` (TASK-1689).
- EMS-ESP32 i18n: https://github.com/emsesp/EMS-ESP32 ; OpenDTU locales: https://github.com/tbnobody/OpenDTU ; Tasmota language decision: https://github.com/arendst/Tasmota
- Mozilla Developer Network (MDN) `Intl.PluralRules`: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/PluralRules ; W3C SRI: https://www.w3.org/TR/SRI/
