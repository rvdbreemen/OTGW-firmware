---
id: "ADR-177"
title: "Keep the PIC byte stream verbatim from the serial task to every consumer"
status: "Proposed"
date: "2026-09-05"
binding: true
gate: "adr-judge declarative rules on the raw-path files"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "madr"
topics:
  - pic-serial
  - byte-transparency
  - diagnose-firmware
aliases:
  - raw passthru
  - otRawQueue
  - byte-transparent bridge
components:
  - OTGW-Core raw PIC byte path
  - otRawQueue producer and consumer
symbols:
  - drainOTRawQueue
  - otRawQueue
  - OT_RAW_CHUNK_MAX
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-177 Keep the PIC byte stream verbatim from the serial task to every consumer

## Status

Proposed, 2026-09-05.

## Status History

```yaml
status_history:
  - date: 2026-09-05
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context and Problem Statement

The PIC can be loaded with the diagnostic firmware instead of the gateway
firmware. That firmware speaks an interactive text menu rather than the
OpenTherm protocol, and it declares its prompt as `da "Enter test number: \032"`,
where `\032` is an end-of-string sentinel that is never transmitted. The prompt
therefore arrives with no line terminator at all.

Anything that assembles PIC output into lines before forwarding it will hold that
prompt forever, waiting for a newline the PIC will never send. The user sees the
menu but not the question, and a screen meant to be interactive looks hung. This
is not hypothetical: it is why `OTGW-Core.h:671-673` documents the sentinel, and
why TASK-1111 built a raw byte path that copies each byte before line assembly
and hands whole chunks across the task boundary in `drainOTRawQueue()`
(`OTGW-Core.ino:596-635`, `:491-498`).

So the mechanism exists and is correct today. What does not exist is anything
that keeps it correct. `evaluate.py::check_pic_uart_task_owns_serial` gates *who*
may write the UART, naming `picSerialDrainOnce`, `picSerialPumpUpgrade` and
`picSerialFlushRx` as the only owners. Nothing gates *what survives the trip*. A
future edit that trims trailing whitespace, appends a newline "for readability",
or passes the chunk through a `char *` string API would compile, pass every gate,
and silently break the diagnose screen in a way that reproduces only on a PIC
running diagnostic firmware.

The 1.x line reached the same conclusion the expensive way and wrote it down as
its own ADR-095. That number is already taken here by an unrelated decision, so
the rule is currently unrecorded on this branch.

## Decision Drivers

* The failure is invisible in review: adding a newline looks like a tidy-up.
* It is also invisible in testing, unless the test runs against a diagnose PIC.
* The raw path has exactly one producer and one consumer today, so a narrow rule
  covers it without constraining unrelated line-oriented code in the same file.
* A decision that no gate checks is a comment, and comments do not survive refactors.

## Considered Options

* **Option A** — state the rule and enforce it with declarative regex rules over
  the raw-path file.
* **Option B** — state the rule and mark it `llm_judge: true`, letting a model
  read the diff semantically.
* **Option C** — write nothing, and rely on the existing comments plus review.

## Decision Outcome

Chosen option: **Option A**, because the damaging edits share a small, precise
syntactic signature. The raw payload travels as a `(pointer, length)` pair, and
every corruption seen so far involves handing that pointer to an API that assumes
a NUL-terminated string, or concatenating a terminator onto it. That is a regex,
not a judgement call, so it costs nothing per commit and cannot be talked around.

Option B was rejected on cost and determinism, not on capability: `llm_judge` is
billed per ADR per commit, and this rule does not need semantic reading. Option C
is the status quo that left the rule unwritten on this branch.

### Confirmation

`bin/adr-judge` blocks a staged diff that adds a string-API call on the raw
payload in the file named below. The positive behaviour is confirmed on hardware:
a diagnose PIC's prompt line, which carries no terminator, must appear on the
diagnose screen rather than being withheld.

## Decision Contract

### Must

* Copy PIC bytes on the raw path verbatim, as a `(pointer, length)` pair.
* Preserve length exactly. A chunk of N bytes reaches every consumer as N bytes.
* Let a consumer add framing *around* the payload, such as a WebSocket frame
  prefix, provided the payload itself is untouched.

### Must Not

* Assemble the raw path into lines, or wait for a terminator before forwarding.
* Synthesise or append `\r` or `\n`.
* Trim, pad, case-fold or otherwise normalise the payload.
* Treat the payload as a NUL-terminated string.

### Exceptions

* The line-assembled path that produces OpenTherm frames is unaffected. This
  decision governs the raw path only.
* A consumer whose transport cannot carry arbitrary bytes may encode **its own
  copy** at the edge, provided the queued payload is untouched and the encoding
  is documented at the call site. There is exactly one today:
  `forwardDiagnoseChunk()` drops non-printables other than CR and LF before
  handing the text to `sendLogToWebSocket()`, which takes a NUL-terminated
  string and would otherwise truncate at the first zero byte. Note what this
  exception does **not** license: it is a lossy render for a text channel, so a
  consumer that needs the true bytes must read them from the queue, not from
  the WebSocket.

### Verification

* `bin/adr-judge` declarative rules, see the Enforcement block below.
* `evaluate.py::check_pic_uart_task_owns_serial` remains the complementary gate on
  UART ownership.

## Consequences

### Positive

* The diagnose screen keeps working, including the terminator-less prompt.
* The rule is checked at commit time rather than remembered.
* Any future consumer of the raw stream inherits the guarantee for free.

### Negative

* A regex gate is syntactic, so it catches the known shapes rather than all
  possible corruption. A rename of the payload variable would need the rule
  updated with it, so the Enforcement block is part of the contract rather than
  decoration.
* It adds one more thing that can block a commit for a reason the author did not
  expect. Mitigated by the rule naming the payload explicitly, so a false positive
  is obvious on sight.

## Pros and Cons of the Options

### Option A

* Good, because the failure shapes are syntactic and cheap to detect.
* Good, because it costs nothing per commit.
* Bad, because it is tied to the payload's variable name.

### Option B

* Good, because it reads intent rather than syntax.
* Bad, because it is billed per ADR per commit for a rule that does not need it.

### Option C

* Good, because it costs nothing to write.
* Bad, because it is the state that left this rule unrecorded on this branch.

## Open Questions

* None.

## Related Decisions

* ADR-130 governs what may run in PIC-task context versus loop context. This
  decision constrains the payload that crosses that boundary, not the crossing.
* The 1.x line records the same rule as its own ADR-095. Numbering is independent
  per branch; the decision is meant to be coherent across both.

## References

* `src/OTGW-firmware/OTGW-Core.h:671-673` — the `\032` sentinel, and why the
  prompt carries no terminator.
* `src/OTGW-firmware/OTGW-Core.ino:596-635` — the raw chunk producer.
* `src/OTGW-firmware/OTGW-Core.ino:491-498` — `drainOTRawQueue()`, the consumer.
* `evaluate.py::check_pic_uart_task_owns_serial` — the complementary ownership gate.
* https://otgw.tclcode.com/diagnose.html — the diagnostic firmware and its menu.

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "\\b(strlen|strlcpy|strlcat|strncat|strcpy|strcat|strcmp|strncmp|strstr|snprintf|sprintf|printf)\\s*\\([^;]*\\braw\\.bytes\\b",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "ADR-177: the raw PIC payload is a (pointer, length) pair, not a string. Passing raw.bytes to a NUL-terminated string API truncates at the first zero byte and breaks byte transparency."
    },
    {
      "pattern": "\\braw\\.bytes\\b[^;]*(\\\\r|\\\\n)",
      "path_glob": "src/OTGW-firmware/OTGW-Core.ino",
      "message": "ADR-177: never synthesise CR or LF on the raw PIC path. The diagnose prompt carries no terminator by design, and adding one changes what the consumer sees."
    }
  ],
  "forbid_import": [],
  "require_pattern": []
}
```
