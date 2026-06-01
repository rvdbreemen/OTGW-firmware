# ADR-090 Re-entrancy Guard Pattern for Shared Scratch Buffers

## Status

Accepted (guideline-level per ADR-080: 2 instances in 1 file is below the recurrence bar for binding pattern-level enforcement; no CI gate). Date: 2026-04-26.

## Context

OTGW-firmware runs cooperatively single-threaded on ESP8266 (and on ESP32 in the 2.0.0 line). There is no preemption and no ISR enters MQTT or autoconfigure paths. There is, however, one well-understood re-entrancy mechanism: `doBackgroundTasks()` is called from inside `doAutoConfigure()`'s file-reading loop at `MQTTstuff.ino:1055`. That call yields control back to `handleOTGW()` and `processOT()` mid-autoconfigure, which means functions reachable from both the autoconfigure path and the OT message path can re-enter through their own front door while a previous invocation is still mid-flight.

Several functions in `MQTTstuff.ino` keep mutable scratch state in file-scope or function-local `static` storage, because heap allocation is forbidden in hot paths (ADR-053) and stack allocation of large buffers blew the 4 KB CONT stack in the ADR-040 saga (publishToSourceTopic originally allocated 1616 bytes on the stack). Static storage solves the memory question; it does not solve the acquisition question. Two instances in `MQTTstuff.ino` already encode acquisition contracts of different shapes, and a third instance added without a contract would silently corrupt the first entrant's state. This ADR names the pattern, points at the two exemplars, and tells future authors which shape to copy.

## Decision

File-scope or function-local-static mutable scratch state that is shared across re-entrant call paths in the cooperative ESP8266 model declares an explicit acquisition contract: either an RAII guard with deleted copy/move (preferred for new code), or a paired acquire/release flag with manual symmetry on every exit path.

1. **Acquisition contract required** [guideline-level]
   Any file-scope mutable buffer or function-local `static` mutable state shared across re-entrant call paths declares either an RAII guard or an inUse-flag pattern. Inventing a third shape requires explicit justification in code review.

2. **RAII preferred for new code** [guideline-level]
   The RAII shape (`struct X { X() { acquire; } ~X() { release; } X(const X&) = delete; X& operator=(const X&) = delete; };`) is preferred for new code because it enforces release symmetry through the language, whereas inUse-flag patterns require manual discipline that is fragile under refactoring. The inUse-flag shape remains acceptable when the function has bounded, easy-to-audit early-return paths.

3. **Fail-safe on contention** [guideline-level]
   When acquisition fails (lock already held, inUse already true), the second entrant returns or skips. It does not silently overwrite the first entrant's state. RAII guards expose `locked` so callers can check; inUse-flag callers check the flag explicitly before setting.

4. **No volatile required on ESP8266** [guideline-level]
   The cooperative single-threaded model means scratch guards do not need `volatile` because no ISR enters these paths. A future SAT/OTGW32 introduction of FreeRTOS-style threading would require revisiting this rule, but until then `volatile` is unnecessary noise.

   **Amendment 2026-04-30 (TASK-497)**: the foresight clause has fired
   on ESP32-S3 (OTGW32). The NimBLE 2.x scan callback runs on the BLE
   host task on core 0 while the Arduino loop task runs on core 1. The
   re-entrancy-guard pattern in this ADR is the right model for ESP8266
   cooperative re-entry but does not cover task-to-task data races.
   For the new cross-task case, use a `portMUX_TYPE` with the
   snapshot pattern (writer wraps the slot-update in
   `portENTER_CRITICAL` / `portEXIT_CRITICAL`; reader takes a stack
   copy under the same lock and processes outside it). Critical
   sections must stay short — one struct copy or one slot-update at
   most — so the BLE radio task is never blocked. See
   `src/OTGW-firmware/SATble.ino` (`_bleSensorsMux`) for the canonical
   exemplar. The cooperative re-entry rules above (sub-rules 1-3) and
   the exemplars in §A and §B remain authoritative for the cooperative
   case; they do not extend to task-to-task hazards.

5. **Two existing instances are exemplars** [guideline-level]
   New code should follow either the `MQTTAutoConfigSessionLock` shape or the `publishToSourceTopic` inUse-flag shape, not invent a third. If a third instance is added without choosing either, the next reviewer should ask why.

### Exemplar A: RAII session lock

`MQTTstuff.ino:75-89` declares `struct MQTTAutoConfigSessionLock` with deleted copy/move. The backing flag `mqttAutoConfigInProgress` is a file-static `bool` at line 73. Call-sites: `MQTTstuff.ino:1745` and `MQTTstuff.ino:1822`, both in the doAutoConfigure path. Caller pattern:

```cpp
MQTTAutoConfigSessionLock guard;
if (!guard.locked) return;  // another caller holds the session
// ... protected work ...
// implicit release on scope exit
```

### Exemplar B: function-local inUse flag

`publishToSourceTopic` at `MQTTstuff.ino:1538-1554` declares `static bool inUse = false;` at line 1543, protecting the function-local `static char sourceTopic[MQTT_TOPIC_MAX_LEN]` at line 1548. Manual acquire/release on lines 1545, 1547, 1550, and 1553. The re-entrancy comment at lines 1541-1542 documents the failure mode in plain terms: `sendMQTTData` may yield via `feedWatchDog`, allowing a second `processOT` call to overwrite the static buffer mid-publish.

## Alternatives Considered

### Alternative A: mutex or critical section

Rejected. ESP8266 is cooperative single-threaded and there is no second thread to mutually exclude against. A mutex would also deadlock under re-entrancy via `doBackgroundTasks()` on the second entry, because the same logical caller would block waiting for itself. The same reasoning rules out a mutex in ADR-088.

### Alternative B: per-caller stack buffers (allocate on the stack each call)

Rejected. The ADR-040 saga (documented in MEMORY.md) showed that stack allocation of large buffers caused 4 KB CONT stack overflow: `publishToSourceTopic` originally allocated 1616 bytes on the stack (sourceTopic[200] + sensorId[64] + discoveryTopic[200] + discoveryMsg[1152]). File-static is the right home; the question this ADR answers is the contract on top.

### Alternative C: one canonical RAII template `ScratchGuard<T>` for all guards

Considered, not mandated. With only 2 instances today the template overhead exceeds the consolidation benefit. The right time to consolidate is when a third instance appears and the shape is well-understood, not now.

## Consequences

**Benefits**

- Future authors of file-scope or function-local-static scratch state have a single referable name (ADR-090) for the pattern. PR comments and code reviews can cite it instead of re-explaining the `doBackgroundTasks` re-entrancy mechanism each time.
- The two existing shapes are documented as exemplars, so a new instance can be matched to one of them without inventing a third shape.
- Honest guideline-level labelling per ADR-080 means no false claim of CI enforcement: reviewers know they own the enforcement, not the build.
- The volatile-not-required note prevents future authors from defensively decorating scratch guards with `volatile` and adding noise.

**Trade-offs**

- Guideline-level means a new direct publisher that bypasses both shapes will not fail any build; only a careful review will catch it. The two existing instances are the only enforcement: a new third pattern variant gets a code-review eyeball, nothing more.
- The "RAII preferred for new code" guidance is a soft preference. A future author with strong reasons for an inUse-flag pattern (very small function, audit-friendly) is still in compliance.
- A future SAT/OTGW32 introduction of FreeRTOS-style threading invalidates sub-rule 4 silently. The ADR would need an amendment if that happens.

**Risks and mitigations**

- *Risk*: a third instance is added without checking ADR-090. *Mitigation*: code review on `MQTTstuff.ino` (and adjacent files that gain shared scratch state) is the enforcement layer. The two existing exemplars are right there for the reviewer to compare against.
- *Risk*: `publishToSourceTopic` gains a fifth return path and the author forgets the manual `inUse = false`. *Mitigation*: this is the exact reason sub-rule 2 prefers RAII for new code. Existing callers stay as they are; a future refactor of `publishToSourceTopic` to RAII is a candidate but not mandated by this ADR.
- *Risk*: documentation drift if `MQTTAutoConfigSessionLock` or `publishToSourceTopic` get refactored away (mirroring what happened to the original `mqttAutoCfgScratch` referenced in older MEMORY.md entries). *Mitigation*: the ADR cites file:line, which is the standard staleness-detection mechanism in this corpus. Reviewers landing such a refactor should update this ADR's References section and the corresponding MEMORY.md entry.

## Related Decisions

- **ADR-004** (Static Buffer Allocation Strategy): covers WHY static allocation. This ADR adds the acquisition contract on top of static allocation when the buffer is shared across re-entrant paths.
- **ADR-007** (Timer-Based Task Scheduling): not the source of re-entrancy in this codebase (timers do not re-enter the protected paths). The actual re-entrancy mechanism is `doBackgroundTasks()` via `doAutoConfigure()`'s file-reading loop at `MQTTstuff.ino:1055`. Cross-referenced for completeness.
- **ADR-044** (Global State extern Declaration): covers the declaration pattern for cross-translation-unit globals; ADR-090 covers the access contract under re-entrancy. Orthogonal and complementary.
- **ADR-053** (Large Feature Buffer Static Allocation): closest neighbour. ADR-053 covers WHY the buffer is file-static; ADR-090 covers HOW the buffer is acquired and released safely under re-entrancy.
- **ADR-080** (Binding ADR Rules Must Have a CI Gate): governance. This ADR is explicitly guideline-level per ADR-080's escape hatch for low-recurrence patterns: 2 instances in 1 file does not meet the threshold for binding pattern-level enforcement.
- **ADR-088** (MQTT Status Burst Windowing): unrelated mechanism but adjacent in the codebase. ADR-088 deals with timing coordination; ADR-090 deals with state acquisition. Both live in `MQTTstuff.ino`.
- **ADR-089** (Heap Tier Machine Contract): unrelated mechanism. Cross-referenced for navigation only.

## References

- Source for Exemplar A: `src/OTGW-firmware/MQTTstuff.ino:73` (backing flag `mqttAutoConfigInProgress`), `:75-89` (struct `MQTTAutoConfigSessionLock`), `:1745` and `:1822` (call-sites in doAutoConfigure path).
- Source for Exemplar B: `src/OTGW-firmware/MQTTstuff.ino:1538-1554` (function `publishToSourceTopic`), `:1543` (`static bool inUse`), `:1548` (`static char sourceTopic[MQTT_TOPIC_MAX_LEN]`), `:1545`, `:1547`, `:1550`, `:1553` (acquire/release sites), `:1541-1542` (re-entrancy explanatory comment).
- Re-entrancy mechanism: `src/OTGW-firmware/MQTTstuff.ino:1055` (`doBackgroundTasks()` call inside `doAutoConfigure()`'s file-reading loop). Documented in CLAUDE.md "Architecture rules" and MEMORY.md "Key Architecture Facts".
- Historical motivation: MEMORY.md "ADR-040 Bug Fix (publishToSourceTopic)" section documents the original 1616-byte stack overflow that motivated moving heavy buffers to file-static. The original `mqttAutoCfgScratch` struct that pre-dated `MQTTAutoConfigSessionLock` no longer exists in source; older docs and comments referencing it are historical context only.
- Discovery context: TASK-429 (this ADR's parent task), TASK-426 (ADR-088, candidate A from same discovery pass), TASK-428 (ADR-089, candidate B).
