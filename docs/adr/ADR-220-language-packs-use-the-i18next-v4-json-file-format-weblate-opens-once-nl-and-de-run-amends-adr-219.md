---
id: "ADR-220"
title: "Language packs use the i18next v4 JSON file format; Weblate opens once NL and DE run (amends ADR-219)"
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
  - "language-pack-format"
  - "weblate"
aliases:
  - "i18next v4 JSON"
  - "translation file format"
components:
  - "language pack file format"
symbols:
  - "t()"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-220 Language packs use the i18next v4 JSON file format; Weblate opens once NL and DE run (amends ADR-219)

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
    reason: Robert's file-format and Weblate decisions 2026-11-08; accepted in session
    changed_via: adr-kit lifecycle
```

## Context

ADR-219 (Accepted 2026-11-08) chose a `t()` helper with flat dotted keys, `{name}` placeholders and `key_one`/`key_other` plurals, and Hosted Weblate only when more languages come. Robert then asked which standard the packs follow and whether ordinary users can edit them without git.

Hosted Weblate (libre plan, free for public open-source projects up to 160,000 strings; the project needs about 2,520) lets users translate in a web page and opens GitHub pull requests. Its formats include plain `json` (flat, no plural support) and `i18nextv4` (CLDR plural forms, `{{name}}` interpolation). Sources: https://docs.weblate.org/en/latest/formats/json.html , https://weblate.org/en/hosting/ . The `i18n-check` tool (lingualdev, MIT) also understands i18next syntax for missing-key and placeholder checks. With ADR-219's `{name}` syntax, plurals would be plain separate strings in Weblate, without plural rules in its editor.

## Decision

**User Decision (Robert, 2026-11-08):** the language pack files follow the i18next v4 JSON format, read by the project's own small `t()` helper (no i18next library, ADR-219 stands); Hosted Weblate is opened for ordinary users once NL and DE run.

- File format: i18next v4 JSON. Keys nest by `.` (for example `nav.settings` is `{"nav":{"settings":"..."}}`), placeholders are `{{name}}`, plurals use the CLDR suffixes `_one`, `_other` (and `_few`, `_many` where a language needs them), chosen with `Intl.PluralRules`.
- Runtime: unchanged from ADR-219: a `t(key, params)` helper in v2.js, English per-key fallback, no library.
- Workflow: NL and DE first as AI drafts reviewed by Robert (NL) and a German-speaking tester (DE), in `i18n/<lang>.json` in the repo. When both run, a Hosted Weblate project (libre plan, maintainer account) is opened with the `i18nextv4` format, the "remove blank strings" add-on and merge-commit (not squash) merges of its pull requests.

This amends ADR-219 on the placeholder syntax, the key nesting and the Weblate timing only.

## Decision Contract

### Must

- Write every pack and the English source in i18next v4 JSON (`{{name}}`, CLDR plural suffixes).
- Keep the packs outside `src/` (`i18n/`), so Weblate pull requests need no firmware bump.

### Must Not

- Ship the i18next library or any other i18n library in the web UI bundle.
- Squash-merge Weblate pull requests.

### Exceptions

- None.

### Verification

- The pack-coverage test parses every `i18n/*.json` as i18next v4 JSON and fails on a missing `{{placeholder}}` or a plural form that `Intl.PluralRules` needs for that language.

## Alternatives Considered

- **Plain JSON with `{name}` (ADR-219 as written).** Simplest, but plural forms lose their meaning in Weblate's editor and i18next tooling does not recognise the placeholders. Rejected by Robert.
- **The i18next library itself.** Same format, but about 15 KB gzip in the bundle (ADR-219 research). Rejected.
- **Weblate from the first translation.** Needs the account and project before there is anything to review. Deferred until NL and DE run.

## Consequences

**Positive:**

- Ordinary users can later translate in a browser; the format needs no conversion for Weblate or i18n-check.
- The runtime stays a few hundred bytes (estimate), as in ADR-219.

**Negative:**

- The `t()` helper must resolve nested keys and the CLDR suffixes itself (a few more lines). Mitigation: covered by the web UI test of TASK-1689 AC2.
- A Weblate account and project become maintenance work once opened. Mitigation: only after NL and DE run; English stays the source in the repo.

## Related Decisions

- **ADR-219 (Translate the web UI with a t() helper)**: amended on placeholder syntax, key nesting and Weblate timing; everything else stands.

## References

- Weblate JSON formats: https://docs.weblate.org/en/latest/formats/json.html
- i18next JSON v4 format: https://www.i18next.com/misc/json-format
- `docs/research/2026-11-07-web-ui-languages-research.md` section 5 (TASK-1689).
