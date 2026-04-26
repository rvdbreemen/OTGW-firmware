---
id: TASK-429
title: >-
  Write ADR (candidate C from discovery): Re-entrancy guard pattern for shared
  scratch buffers under ESP8266 cooperative multitasking
status: To Do
assignee: []
created_date: '2026-04-26 09:19'
labels:
  - adr
  - memory
  - documentation
  - re-entrancy
dependencies: []
references:
  - src/OTGW-firmware/MQTTstuff.ino
  - docs/adr/ADR-004-static-buffer-allocation.md
  - docs/adr/ADR-053-large-feature-buffer-static-allocation.md
  - docs/adr/ADR-044-global-state-header-definition-pattern.md
  - docs/adr/ADR-007-timer-based-task-scheduling.md
  - docs/adr/ADR-080-binding-adr-rules-must-have-a-ci-gate.md
  - docs/adr/ADR-088-mqtt-status-burst-windowing-and-cooldown.md
  - docs/adr/ADR-089-heap-tier-machine-contract.md
  - evaluate.py
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Discovered as **candidate C** in the ADR discovery pass on 2026-04-25. Candidates A and B became ADR-088 (status-burst windowing, commit ccba9076) and ADR-089 (heap tier-machine, commit 3f0fdc02) on 2026-04-26. C remains.

This task is fully briefed so a future session can pick it up cold. Read this description, the linked source lines, and the cross-referenced ADRs before starting.

## Why this is a candidate

ESP8266 runs Arduino cooperatively (single-threaded), but `doBackgroundTasks()` is re-entrant: it is called from inside `doAutoConfigure()`'s file-reading loop at `MQTTstuff.ino:1055`, causing `handleOTGW()` to fire mid-autoconfigure. Several places declare file-scope scratch buffers shared across re-entrant call paths. Without an explicit acquisition contract the second entrant clobbers the first entrant's state mid-use. The ADR-040 saga in MEMORY.md documents one variant: 1616 bytes of local buffers on the 4 KB CONT stack caused stack overflow under re-entrancy.

The codebase has at least two implementations of the guard pattern, and they are not the same shape. There may be more elsewhere. The contract that future scratch-buffer authors must follow is currently undocumented.

## Two known instances (verified 2026-04-26)

**Instance 1 (RAII session lock)**: `MQTTstuff.ino:75-89` declares `MQTTAutoConfigSessionLock`, an RAII guard around the file-static `mqttAutoConfigInProgress` flag (`MQTTstuff.ino:73`). Constructor sets the flag if free; destructor clears it iff the constructor took it. Copy/move deleted. Caller checks `guard.locked`; if false, the path was already in progress and the second entrant must early-return.

**Instance 2 (inUse-flag scratch struct)**: `MQTTstuff.ino:57-74` declares `mqttAutoCfgScratch` with `sMsg[1200]`, `sTopic[200]`, plus an `inUse` flag. The acquire helper hands out pointers and sets `inUse = true`; the release helper clears it. First-acquire wins; subsequent acquires see WARN.

Both patterns aim at the same problem (shared mutable state under re-entrancy) but use different shapes. Whether to standardise on one or document both is an open question.

## Pre-flight checks before writing the ADR

1. **Confirm pattern recurrence**: grep for similar shared-buffer + guard patterns beyond `MQTTstuff.ino`. Suggested searches:
   - `static bool .*InProgress` (RAII flag candidates)
   - `\.inUse\b` (inUse flag candidates)
   - `static char .*\[\d+\]` at file scope in *.ino files (potential unguarded shared scratch)

   If only 2 instances exist, the pattern is borderline between guideline-level documentation and binding pattern-level ADR. If 4+, it is firmly pattern-level.

2. **Re-read MEMORY.md ADR-040 entry**: documents the historical motivation. Reference concretely.

3. **Confirm gap vs related ADRs**:
   - **ADR-004** (Static Buffer Allocation): covers WHY static, not HOW re-entrancy-safe.
   - **ADR-053** (Large Feature Buffer Static Allocation): covers file-static declaration of large buffers, not acquisition contract.
   - **ADR-044** (Global State extern Declaration): covers declaration pattern, not concurrent access.
   - **ADR-007** (Timer-Based Scheduling): source of re-entrancy via callbacks, but does not specify scratch behaviour.

   ADR is justified iff none covers the *acquisition contract* under re-entrancy.

4. **Verify ADR number**: `ls docs/adr/ADR-090*` (or whatever is next sequential at pickup time; ADR-090 expected free as of 2026-04-26).

## Initial sub-rule sketch (validate against discovery)

1. **Acquisition contract** [pattern-level binding, candidate]: every file-scope mutable scratch buffer or shared mutable global accessed across re-entrant call paths must declare an explicit acquisition contract (RAII guard with deleted copy/move, or inUse-flag with paired acquire/release).

2. **Fail-safe on re-entrant acquisition** [pattern-level binding, candidate]: when acquisition fails, the second entrant must early-return or log + skip. Must not silently overwrite the first entrant's state.

3. **Release symmetry** [pattern-level binding for inUse, automatic for RAII]: inUse-flag patterns require every successful acquire to be paired with release on every exit path. RAII enforces this automatically. ADR should call out preferred pattern (RAII for new code).

4. **No volatile required on ESP8266** [guideline-level]: cooperative model means scratch guards do not need `volatile` (no ISR enters the path). ESP32 with FreeRTOS may differ.

5. **Buffer sizing** [out of scope]: ADR-053 covers it. Cross-reference only.

## Alternatives to consider

(a) **Mutex / critical section**: rejected, same reasoning as ADR-088 alternative (b). ESP8266 cooperative; mutex inverts the model and deadlocks under re-entrancy via `doBackgroundTasks()`.

(b) **Per-caller stack buffers**: rejected per ADR-040 saga (1616 bytes blew the 4 KB CONT stack). File-static is the right home; the question is the contract on top.

(c) **One canonical RAII template `ScratchGuard<T>`**: candidate. Trade-off: template adds compile-time complexity for a 2-3-instance pattern. ADR should document as "consider for refactor" not mandate.

## Open decision points (resolve before writing)

**Q1**: How many instances exist? If only 2, is the ADR really pattern-level or guideline-level documentation? (ADR-080 governance.)
**Q2**: RAII vs inUse: mandate one as preferred for new code, or document both as acceptable?
**Q3**: CI gate feasibility — possible gates (RAII shape detection, inUse symmetry check, file-scope scratch buffer comment marker requirement) are all complex. If no clean gate emerges, label sub-rules guideline-level and defend per ADR-080.
**Q4**: ADR number at pickup time (ADR-090 expected).
**Q5**: README placement: Memory Management (where ADR-053 lives) or System Architecture (where ADR-007 lives). Lean Memory Management.

## Process notes (mirror ADR-088 / ADR-089)

Run pre-flight, refine sub-rules from findings, surface plan + Qs to user and **wait for approval**, then invoke `adr-generator` subagent (use ADR-088 / ADR-089 prompts as templates), spot-check every cited identifier (the ADR-089 subagent fabricated `iStatusBurstSkipCount` which did not exist), land any gates with strict signature regex pattern, add cross-refs, update README, verify gates via direct Python invocation, single descriptive commit.

## Cross-references

- Discovery shortlist: TASK-426 (cand A), TASK-428 (cand B).
- Sibling ADRs: ADR-088 (commit ccba9076), ADR-089 (commit 3f0fdc02).
- README hygiene follow-up: TASK-427 (still To Do; do NOT bundle).
- MEMORY.md sections: "ADR-040 Bug Fix (publishToSourceTopic)" and "Scratch Buffer Usage Pattern".
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Pre-flight discovery scan completed: documented all file-scope scratch-buffer + guard pattern instances across the codebase (not just MQTTstuff.ino). At least the two known instances (MQTTAutoConfigSessionLock at MQTTstuff.ino:75-89, mqttAutoCfgScratch.inUse at MQTTstuff.ino:57-74) are confirmed; any additional instances are listed with file:line and pattern shape (RAII vs inUse-flag vs other)
- [ ] #2 Gap analysis confirms ADR-004 / ADR-053 / ADR-044 / ADR-007 do NOT cover the acquisition contract under re-entrancy. If any of them does, this task is closed as no-op or converted to an amendment task
- [ ] #3 If only 2 instances exist project-wide, the ADR is explicitly labelled guideline-level (per ADR-080) or the decision to make it pattern-level is justified with concrete future-author risk
- [ ] #4 ADR file docs/adr/ADR-NNN-re-entrancy-guard-pattern-shared-scratch-buffers.md exists with Status: Accepted, next sequential number verified free at pickup time
- [ ] #5 Decision section: one declarative sentence + sub-rules with explicit pattern-level / guideline-level annotations per ADR-080. Each binding sub-rule names a CI gate or is explicitly labelled guideline-level with reasoning
- [ ] #6 Three alternatives considered with concrete rejection reasoning: (a) mutex/critical section, (b) per-caller stack buffers, (c) canonical RAII template helper for all guards
- [ ] #7 Related Decisions cross-links ADR-004, ADR-007, ADR-044, ADR-053 with explicit gap statement; cross-links ADR-040 saga from MEMORY.md as historical motivation
- [ ] #8 If new CI gates are added: they pass on current source via direct Python invocation; gate functions use the strict signature regex (\(\s*\)\s*\{) pattern learned from ADR-088
- [ ] #9 ADR-053 receives a single-line cross-reference in its Related Decisions section pointing at ADR-NNN; ADR-053 decision text untouched per immutability rule
- [ ] #10 README updated with ADR-NNN entry under chosen category (Memory Management preferred); per scope clarification, no category counts updated and no other index drift addressed (TASK-427 still owns the broader refresh)
- [ ] #11 All four /adr-kit:adr verification gates pass: Completeness, Evidence, Clarity, Consistency
- [ ] #12 Spot-check of ADR text: every named identifier (function, struct, constant, counter) is verified to exist in the source. The ADR-089 generation produced one fabricated identifier (iStatusBurstSkipCount); apply the same vigilance here
- [ ] #13 Commit and push as a single descriptive commit; commit body explains the acquisition contract and the rejected alternatives concisely
<!-- AC:END -->
