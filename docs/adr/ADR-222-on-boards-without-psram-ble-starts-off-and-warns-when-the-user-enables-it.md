---
id: "ADR-222"
title: "On boards without PSRAM BLE starts off and warns when the user enables it"
status: "Accepted"
date: "2026-11-08"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-169"
superseded_by: null
topics:
  - "ble"
  - "psram"
  - "settings defaults"
  - "web ui consent"
aliases:
  - "BLE no-PSRAM default"
  - "BLE stability warning"
  - "bBleRiskAck"
components:
  - "applyBleNoPsramDefault"
  - "maybeShowBleConsent"
  - "bleNeedsRiskAck"
symbols:
  - "applyBleNoPsramDefault"
  - "g_sawBleEnableKey"
  - "bleNeedsRiskAck"
  - "bleBlePaused"
  - "maybeShowBleConsent"
  - "bBleEnable"
  - "bBleRiskAck"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-222 On boards without PSRAM BLE starts off and warns when the user enables it

## Status

Accepted, 2026-11-08.
`tests/test_ble_nopsram_default.py` and `tests/webui/ble-nopsram-warning.test.mjs`.

**Decision Maker:** User: Robert van den Breemen (TASK-1693, 2026-11-08). Proposed by the agent; the
maintainer accepts it, the agent never does.

## Status History

```yaml
status_history:
  - date: 2026-11-08
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal (TASK-1693, maintainer decision recorded in the task notes)
    changed_via: adr-kit
  - date: 2026-11-08
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Robert accepted in session (AskUserQuestion, 2026-11-08): BLE off by default on no-PSRAM boards, warning only when the user enables it; TASK-1693"
    changed_via: adr-kit lifecycle
  - date: 2026-11-08
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Robert accepted ADR-222 (2026-11-08, TASK-1693): BLE off by default on no-PSRAM boards, warning only on enable; the bleActive() gate and memory rationale of ADR-169 stay"
    changed_via: adr-kit lifecycle
```

## Context

The 2.0.0 firmware runs one image on ESP32-S3 (Espressif microcontroller) boards with and without PSRAM (pseudo-static random-access
memory, an external memory chip). The BLE (Bluetooth Low Energy) sensor scanner takes roughly 64 KB (kilobytes) of
internal DRAM (dynamic random-access memory), the same pool the async web server needs (ADR-169 Context; ADR-147). ADR-169 made BLE on by
default everywhere (`SATtypes.h:607`, `bBleEnable = true`) and, on a board without PSRAM, kept the
scanner dormant until the user accepted a stability risk (`bBleRiskAck`, `SATble.ino:625`
`bleActive()`). The web UI (user interface) asked for that acceptance with a modal dialog that `fetchBle()` opened by
itself whenever BLE was on, there was no PSRAM and no ack (pre-change `v2.js` `fetchBle()`), so a
fresh no-PSRAM unit showed the dialog on its first visit to the Sensors page, before the user had
done anything with BLE.

Tester tjfs asked to warn only when the user turns BLE on and to keep it off at boot (#alpha-testing
2026-11-07, message 1568600646901305449). Sergeant D suggested hiding BLE on no-PSRAM boards entirely
(message 1568601041937764453). Robert chose the first on 2026-11-08 (TASK-1693 notes).

## Decision

**User Decision:** on a board without PSRAM, BLE is off by default and no dialog appears after boot
or when a page opens; the stability warning appears once, when the user switches BLE on, and only an
accepted warning stores BLE on together with the ack.

Concretely:

1. **Default.** A fresh install without PSRAM gets `bBleEnable = false` before its first settings
   write (`settingStuff.ino:567`, `applyBleNoPsramDefault()` at `:546-555`). A settings file without
   the `SATbleenable` key (for example one restored from 1.x) gets the same default after the parse,
   detected by the sentinel `g_sawBleEnableKey` and persisted by the deferred flush (`:658-663`). A
   stored `SATbleenable` always wins, so existing 2.x installs keep their value. PSRAM boards keep
   the struct default (on).
2. **Warning on enable.** When the user switches BLE on and saves, and the board has no PSRAM and no
   ack, the web UI takes `satbleenable` out of that save and opens the warning (`v2.js:2930`
   `bleEnabling`, `:2998` `bleNeedsRiskAck()`, `:3009` `maybeShowBleConsent()`). "Enable anyway"
   stores `satbleenable=true`, then `satbleriskack=true`, one call after the other (ADR-165);
   "Keep BLE off" stores nothing.
3. **No dialog on its own.** `fetchBle()` no longer opens the dialog. An install from before this
   change that has BLE on without the ack keeps BLE paused (`bleActive()` unchanged) and shows a
   hint with a "Read the warning" button in the BLE card (`v2.js:3085`, `bleBlePaused()` at `:2994`).
4. The firmware gate `bleActive() = bBleEnable && (psramFound() || bBleRiskAck)` is unchanged.

## Decision Contract

### Must

- On a board without PSRAM, apply the BLE-off default to a fresh install before the first
  `writeSettings()` and to a settings file without the `SATbleenable` key.
- Keep a stored `SATbleenable` value as it is.
- Show the stability warning only after a user action (switching BLE on, or the BLE card button).
- On acceptance store `satbleenable` before `satbleriskack`, sequentially.

### Must Not

- Must not open the BLE warning from a poll, a page load or a boot.
- Must not store `satbleenable=true` on a no-PSRAM board before the user accepted the warning.
- Must not change the PSRAM-board default or show the warning there.
- Must not change `bleActive()`'s gate.

### Exceptions

- None.

### Verification

- `python tests/test_ble_nopsram_default.py`: compiles the real `applyBleNoPsramDefault()` and checks
  the `readSettings()`/`updateSetting()` wiring; the pre-change `settingStuff.ino` fails 4/4.
- `node --test tests/webui/ble-nopsram-warning.test.mjs`: drives the real v2 page; the pre-change page
  opens the modal on its own and stores BLE before consent (tests 1-3 fail), the change passes 4/4.

## Alternatives Considered

- **Keep ADR-169 as is (dialog whenever BLE is on, no PSRAM, no ack).** Rejected by Robert: a fresh
  unit shows a dialog about a feature the user never touched, on the first page visit.
- **Hide BLE on boards without PSRAM (Sergeant D's suggestion).** Rejected by Robert: a user who
  accepts the risk (for example with few web clients) loses BLE sensors altogether; the warning
  keeps the choice with the user.
- **Off by default but keep the modal for existing dormant installs.** Rejected: it keeps the
  unprompted dialog for exactly the users who already saw it; a non-modal hint gives the same path
  without interrupting.

## Consequences

**Positive:**

- A fresh no-PSRAM unit boots with BLE off: it keeps the ~64 KB internal DRAM for the web server
  until the user opts in, and no dialog appears without a user action.
- The warning appears at the moment the user makes the choice, so it reads as a decision, not as an
  interruption.

**Negative:**

- A no-PSRAM user who wants BLE sensors must now switch BLE on and accept the warning; before, they
  only accepted the dialog. Mitigation: the settings help and the BLE card hint name the step.
- Existing installs with BLE on and no ack stay paused silently except for the BLE card hint.
  Mitigation: the hint has a button that opens the same warning.
- The default depends on `psramFound()` at the moment the settings are first written; a board whose
  PSRAM fails to initialise once would get BLE off. Mitigation: the user can switch it on with the
  warning; nothing is lost.

## Open Questions

- None.

## Related Decisions

- **ADR-169 (PSRAM-Aware BLE Default with a No-PSRAM Instability-Consent Gate)**: this ADR proposes to
  supersede it for the default and for when the consent is asked; its gate (`bleActive()`) and its
  memory rationale stay. The `supersedes`/`superseded_by` links are set when the maintainer accepts
  this ADR.
- **ADR-165 (Optimal request parallelism)**: the two settings calls on acceptance run one after the
  other.
- **ADR-147 (ESP32-S3 platform limits for concurrent web serving)**: the DRAM headroom this protects.

## References

- TASK-1693 (`backlog/tasks/task-1693 - BLE-on-boards-without-PSRAM-...md`), Robert's decision in the
  Implementation Notes, 2026-11-08.
- `src/OTGW-firmware/settingStuff.ino:546-555, 567, 658-663`; `src/OTGW-firmware/SATble.ino:625`;
  `src/OTGW-firmware/SATtypes.h:607-608`; `src/OTGW-firmware/data/v2.js:2930, 2994, 2998, 3009, 3085`.
- #alpha-testing messages 1568600646901305449 (tjfs) and 1568601041937764453 (Sergeant D),
  2026-11-07.
