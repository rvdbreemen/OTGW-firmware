---
id: "ADR-180"
title: "Keep the Arduino String class out of hot-path source files"
status: "Accepted"
date: "2026-10-01"
binding: false
gate: null
documents_shipped: true
verified_in:
  - "evaluate.py:HOT_PATH_PREFIXES"
supersedes: []
superseded_by: null
topics:
  - "heap"
  - "string-class"
  - "coding-standards"
  - "memory"
aliases:
  - "no String in hot paths"
  - "String class"
  - "heap fragmentation"
  - "char buffers"
components:
  - "Hot-path String rule (HOT_PATH_PREFIXES and the String check in check_coding_standards)"
symbols:
  - "HOT_PATH_PREFIXES"
  - "_STRING_DECL_RE"
  - "is_hot_path_file"
  - "check_coding_standards"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-180 Keep the Arduino String class out of hot-path source files

## Status

Accepted, 2026-10-01.

Guideline-level per ADR-080: `evaluate.py` reports each violation as WARN with
its call site and does not fail the build.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-10-01, chose
the guideline level over a binding rule with a burn-down task.

## Status History

```yaml
status_history:
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Documents the hot-path String check that evaluate.py already runs
    changed_via: adr-kit lifecycle
  - date: 2026-10-01
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer accepted in session 2026-10-01 as guideline-level, after the acceptance packet
    changed_via: adr-kit lifecycle
```

## Context

The firmware has a long-standing rule against Arduino `String` objects in
hot-path source files: the SAT (smart thermostat) subsystem, MQTT (Message
Queuing Telemetry Transport) publishing, the REST API, the OpenTherm core and
the direct OpenTherm driver. Three places state or enforce it, and all three cite
ADR-004:

- CLAUDE.md, Critical Coding Rules: "No String class in hot paths (ADR-004)".
- AGENTS.md, String usage: "areas covered by ADR-004", followed by the five
  areas above.
- `evaluate.py`: the `HOT_PATH_PREFIXES` tuple (`evaluate.py:44`) and the result
  label "String Class in Hot Path (ADR-004)" in `check_coding_standards()`
  (`evaluate.py:2386`).

ADR-004 (Static Buffer Allocation Strategy) is Superseded by ADR-053 (Large
Feature Buffer Static Allocation), and ADR-053 does not restate the rule. The
rule therefore cites a decision that is no longer in force. TASK-1183 found
this, and on 2026-09-30 the maintainer decided that ADR-004 stays off the binding
list and that the rule gets an Accepted home of its own (TASK-1187).

The reason for the rule is in ADR-004 (lines 30 and 41). An Arduino `String`
allocates on the heap and reallocates as it grows. Code that runs per
OpenTherm message, per MQTT publish or per REST request does that thousands of
times a day, and the fragmentation it leaves shrinks the largest free block the
firmware can still allocate. A fixed `char[]` buffer filled with `strlcpy()` or
`snprintf_P()` allocates nothing.

Measured on 2026-10-01 at dev commit 9d6407d31: the check scans the `.ino` and
`.cpp` files whose names start with `SAT`, `MQTTstuff`, `restAPI`, `OTGW-Core` or
`OTDirect` for `String` declarations (`_STRING_DECL_RE`, `evaluate.py:51`;
reference forms such as `const String&` are skipped, because they do not
allocate). It reports 10 usages as WARN:

- 7 in `OTGW-Core.ino` (lines 6039 to 6201): the firmware update check for the
  gateway's PIC (Microchip PIC microcontroller), `checkforupdatepic()`, and the
  selection of its firmware file.
- 2 in `restAPI.ino` (`:521`, `:946`): request arguments read through
  `argCompat()`, which returns a `String`.
- 1 in `restAPI.ino` (`:4364`): URI escaping in an error response.

All ten run on demand. None of them runs per message.

## Decision

New code in hot-path source files does not declare `String` objects. Hot-path
source files are the `.ino` and `.cpp` files whose names start with `SAT`,
`MQTTstuff`, `restAPI`, `OTGW-Core` or `OTDirect`. They use fixed `char[]`
buffers filled with `strlcpy()` and `snprintf_P()` instead. `String` stays
acceptable in setup code and in one-off paths in other files.

The rule is guideline-level per ADR-080. `evaluate.py` reports every `String`
declaration in a hot-path file as WARN with its call site, and the build does not
fail on it. The 10 usages listed in the Context are known debt in on-demand
paths; removing them is optional cleanup, not a precondition.

`evaluate.py`, CLAUDE.md and AGENTS.md cite this ADR instead of ADR-004.

## Decision Contract

### Must

- Use `char[]` buffers with `strlcpy()` or `snprintf_P()` for new text handling
  in hot-path source files.
- Keep `evaluate.py` reporting hot-path `String` declarations with their call
  sites.
- Cite this ADR, not ADR-004, wherever the rule is stated.

### Must Not

- Declare a `String` object in a per-message path of a hot-path source file:
  OpenTherm message handling, MQTT publishing, or the handling of a polled REST
  request.

### Exceptions

- A library call that returns only a `String`, such as a request argument read
  through `argCompat()`, may be held in a short-lived local in a request handler
  and copied into a fixed buffer.
- `const String&` and other reference forms do not allocate and fall outside the
  rule.

### Verification

- `python evaluate.py`: the "String Class in Hot Path (ADR-180)" result lists
  every `String` declaration in a hot-path source file, with its call site.
- The Enforcement block below keeps `HOT_PATH_PREFIXES` in `evaluate.py`.

## Alternatives Considered

- **Binding, with WARN now and FAIL after a burn-down** (the path ADR-091
  took): `binding: true` with the `evaluate.py` check as its gate, a follow-up
  task that removes the 10 usages, and then a check that fails the build. Not
  chosen in this draft: the 10 usages run on demand, so the FAIL would police
  code that does not fragment the heap in practice, and naming hot paths by file
  prefix is coarse. This remains the maintainer's choice (Open Questions).
- **Binding, with an immediate FAIL**: every `evaluate.py` run fails until the 10
  usages are rewritten, which blocks unrelated work.
- **Cite ADR-053 instead**: ADR-053 does not state the rule, so the citation
  would still point at a decision that does not contain it.
- **Drop the rule**: the reason it exists, heap fragmentation from per-message
  allocation, still holds.

## Consequences

**Positive:**

- The rule cites an Accepted decision again: a reader who follows the citation
  finds the rule, its file list and its reason.
- Nothing changes in the build: the check stays WARN.

**Negative:**

- A WARN can be ignored, so a new hot-path `String` is caught only when someone
  reads the `evaluate.py` output. Mitigation: the WARN names each call site, and
  CLAUDE.md lists the rule under Critical Coding Rules.
- The 10 known usages stay until someone chooses to remove them.

## Open Questions

- [x] Guideline-level as drafted (the `evaluate.py` check reports WARN), or binding (`binding: true`, the `evaluate.py` check as its gate, WARN until the 10 known usages are removed, then FAIL)? — **Answered 2026-10-01 by User: Robert van den Breemen:** Guideline-level, as drafted. The maintainer chose it on 2026-10-01 after seeing that all ten current usages run on demand; evaluate.py keeps reporting WARN with call sites and no burn-down task is opened.

## Related Decisions

- **ADR-004 (Static Buffer Allocation Strategy, Superseded)**: the original home
  of the rule and of its reason.
- **ADR-053 (Large Feature Buffer Static Allocation)**: the successor of
  ADR-004; it does not restate the rule.
- **ADR-080 (Binding ADR rules must have a CI gate)**: this ADR is guideline-level
  under it.

## References

- `evaluate.py:44` (`HOT_PATH_PREFIXES`), `:51` (`_STRING_DECL_RE`), `:224`
  (`is_hot_path_file()`), `:2382` to `:2395` (the hot-path String result in
  `check_coding_standards()`).
- `docs/adr/ADR-004-static-buffer-allocation.md`, lines 30 and 41.
- CLAUDE.md, "No String class in hot paths"; AGENTS.md, "String usage".
- TASK-1183 (where the stale citation was found) and TASK-1187 (this decision).

## Enforcement

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [
    {
      "pattern": "HOT_PATH_PREFIXES",
      "path_glob": "evaluate.py",
      "message": "evaluate.py must keep the hot-path file list that the String check uses (ADR-180)."
    }
  ],
  "llm_judge": false,
  "llm_judge_reason": "The rule is guideline-level; its only mechanical surface is the evaluate.py check, which this require_pattern keeps in place."
}
```
