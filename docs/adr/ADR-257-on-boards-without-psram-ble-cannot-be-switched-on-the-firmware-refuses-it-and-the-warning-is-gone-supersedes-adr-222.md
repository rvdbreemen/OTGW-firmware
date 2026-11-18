---
id: "ADR-257"
title: "On boards without PSRAM BLE cannot be switched on; the firmware refuses it and the warning is gone (supersedes ADR-222)"
status: "Accepted"
date: "2026-11-18"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-222"
superseded_by: null
related:
  - "ADR-169"
  - "ADR-222"
topics:
  - "ble"
  - "psram"
  - "heap"
  - "settings"
  - "web-ui"
aliases:
  - "BLE needs PSRAM"
  - "no BLE without PSRAM"
  - "bleEnableAllowed"
components:
  - "BLE scanner activation gate (SATble bleActive)"
  - "SATbleenable setter refusal (settingStuff bleEnableAllowed)"
symbols:
  - "bleActive"
  - "bleEnableAllowed"
  - "g_bleRefusedNoPsram"
  - "bleNoPsram"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-257 On boards without PSRAM BLE cannot be switched on; the firmware refuses it and the warning is gone (supersedes ADR-222)

## Status

Accepted, 2026-11-18.

**Decision Maker:** User: Robert van den Breemen (maintainer). Discord direct message 1572600155788546051,
2026-11-18 13:11:34, reply "A" to question 1572596080632139908 (see Decision).

## Status History

```yaml
status_history:
  - date: 2026-11-18
    status: Proposed
    changed_by: Agent (ba)
    reason: "Written by otgw-webui (Claude) on Robert's choice A, Discord DM 1572600155788546051 on question 1572596080632139908 (TASK-2025); acceptance is his"
    changed_via: adr-kit
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-169
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-222
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Accepted
    changed_by: Robert van den Breemen (decision, DM 1572612342850658579), recorded by Agent (ba)
    reason: "Accepted: decision Robert van den Breemen (DM 1572612342850658579, answer 'Ja' to question 1572605854773936209), recorded by Agent (ba); bench PASS by otgw-bench-tester on .61 (beta.375+ff73985)"
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Accepted
    changed_by: Robert van den Breemen (decision, DM 1572612342850658579), recorded by Agent (ba)
    reason: Superseded by ADR-257 (Robert's option A, DM 1572600155788546051; accepted DM 1572612342850658579)
    changed_via: adr-kit lifecycle
```

## Context

The 2.x firmware runs one image on ESP32-S3 boards with and without PSRAM (pseudo-static random-access memory, an
external memory chip). ADR-222 (TASK-1693) kept BLE (Bluetooth Low Energy) off by default on a board without PSRAM and
showed a stability warning when the user switched it on; "Enable anyway" stored `SATbleenable=true` and
`SATbleriskack=true`, and the scanner then ran, because the firmware gate was
`bleActive() = bBleEnable && (psramFound() || bBleRiskAck)` (`SATble.ino:624-626` before this ADR, identical in the
public v2.0.0-beta.360).

On 2026-11-18 otgw-bench-tester measured what that acceptance costs on .61 (no PSRAM, beta.370, TASK-2025).
`SATbleriskack` was already stored true; one REST (Representational State Transfer, the HTTP API) POST (an HTTP write request) `SATbleenable=true` started the passive scanner (4 sensors in
range, about 400 advertisements per 30 s), with no web UI open and no diagnostic recording running. From about 2 minutes
later the heap was starved: free 7 to 13 KB, largest free block 2 to 5 KB, minimum free 520 B (telnet: "free 8992 frag
44% minFree 804 maxBlk 5108"); the web server answered 503 and then nothing, MQTT (Message Queuing Telemetry Transport, the broker link) dropped 270 packets, and after about
10 minutes the unit hung completely, without any restart, until a USB (Universal Serial Bus) reset 13 minutes later. The reboot log shows one
boot over the whole episode. A user in the field would need a power cycle, and the unit would hang again whenever BLE is
on. The warning text of ADR-222 ("the Web UI can become unstable or unreachable under load") understates that.

Robert was asked (question 1572596080632139908) to choose between A (no BLE without PSRAM, refused in the firmware),
B (keep "Enable anyway", add a boot guard that switches BLE off after a crash) and C (cap the BLE memory on boards
without PSRAM, unmeasured). The question, verbatim as the coordinator relayed it (Dutch, elisions as relayed): "Vraag
TASK-2025 (HOOG, ook in publieke 360). Op een board zonder PSRAM vreet BLE de heap op (vrij 9 KB, grootste blok 2 KB) en
komt de unit in een herstart-loop; via REST is BLE niet meer uit te zetten, alleen USB helpt. [...] A) Geen BLE op
no-PSRAM: firmware weigert het (REST 400 met reden), een opgeslagen true wordt bij boot gewist, UI verbergt de
schakelaar met een korte uitleg (Sergeant D stelde dit 7 nov voor). [...] Advies: A." Its "herstart-loop" wording was
corrected afterwards in direct message 1572601402079973387: there was no restart, the unit hung. His reply, verbatim:
"A". Two details differ from the option text and were put to him (message 1572607116852596767): REST answers 409 (a
conflict with the board) instead of 400, and the switch is not hidden. On the second he answered, verbatim: "Niet
verbergen maar disablen. Geen PSRAM dan mag je de schakelaar niet aanraken." (message 1572613627528085586): the
switch stays visible and disabled.

## Decision

**User Decision:** on a board without PSRAM BLE cannot be switched on: the scanner never starts there, every write of
`SATbleenable=true` is refused, a stored `true` is cleared once, and the web UI says why instead of offering
"Enable anyway".

1. **Gate.** `bleActive() = bBleEnable && psramFound()` (`SATble.ino:627`). `bBleRiskAck` stays a setting that is
   read and written, but it no longer opens the gate.
2. **One setter.** Every write of `SATbleenable` goes through `bleEnableAllowed(psram, want)`
   (`settingStuff.ino:572`, block `[ble-nopsram-refuse]`), called from the setter (`:1321`). The boot parse of
   `settings.ini`, the REST settings POST and the MQTT `ble_enable` command all use that setter, so they get the same
   answer.
3. **Stored value.** A stored `SATbleenable=true` on a board without PSRAM is refused at the boot parse and written
   back as `false` once through the deferred flush (`settingStuff.ino:702`). This replaces ADR-222's "a stored
   SATbleenable always wins" for boards without PSRAM; PSRAM boards keep their stored value.
4. **REST answer.** `POST /api/v2/settings` with `satbleenable=true` on a board without PSRAM answers `409` with
   `{"error":"BLE needs PSRAM: not available on this board"}` before the setter runs (`restAPI.ino:4573`).
5. **Web UI.** The stability warning dialog, its "Enable anyway" and "Keep BLE off" buttons and the "BLE is paused"
   hint are removed. On a board without PSRAM the BLE switch stays visible but disabled (greyed out, the note as its
   title), set when the row is built and again when the BLE poll first reports no PSRAM (`bleLockSwitch()` in
   `data/v2.js`). Robert, direct message 1572613627528085586, verbatim: "Niet verbergen maar disablen. Geen PSRAM dan
   mag je de schakelaar niet aanraken." As a second line of defence a save that still carries `satbleenable=true` does
   not post it and shows a red toast "BLE needs PSRAM. ..."; the BLE card shows the same note. PSRAM boards switch BLE
   on as before.

## Decision Contract

### Must

- Keep the BLE scanner off on a board without PSRAM, whatever `SATbleenable` and `SATbleriskack` hold.
- Route every write of `SATbleenable` through `bleEnableAllowed()`.
- Clear a stored `SATbleenable=true` on a board without PSRAM once, through the deferred flush.
- Answer a REST request to switch BLE on without PSRAM with 409 and a reason.

### Must Not

- Offer a way to accept a risk and run BLE on a board without PSRAM.
- Change BLE behaviour or defaults on boards with PSRAM.

### Exceptions

- None.

### Verification

- `tests/test_ble_nopsram_refuse.py` compiles the real `bleActive()` and `bleEnableAllowed()`: without PSRAM the gate is
  false for every enable/ack combination (the old gate fails with "no PSRAM, enable=1 ack=1 starts BLE"), and checks the
  setter, the boot clear with `settingsDirty`, and the 409 before the setter (red on origin/dev 0e967cb71).
- `tests/webui/ble-nopsram-warning.test.mjs` drives the real `v2.html`/`v2.js`: the switch disabled with the reason as
  its title, the card note, the red toast with no POST, no warning dialog, and the switch usable on a PSRAM board. `tests/webui/settings-error-toast.test.mjs`
  covers the refused save in `saveSettings()`.
- `tests/test_ble_nopsram_default.py` (ADR-222's default for a fresh install) stays green.
- Bench: .61 (no PSRAM) with a stored `SATbleenable=true` and `SATbleriskack=true` boots with BLE off, rewrites
  `settings.ini`, answers the POST with 409, and keeps its heap; a PSRAM board still runs BLE.

## Alternatives Considered

- **B: keep "Enable anyway" and add a boot guard** that clears `SATbleenable` after a crash or low-heap restart soon
  after the BLE start. Rejected: on .61 the unit hung without any restart, so there was no next boot for a guard to act
  on; and a user who accepts still loses the gateway until a power cycle.
- **C: cap the BLE memory without PSRAM** (a smaller NimBLE host, fewer scan results). Rejected for now: ADR-185 already
  trimmed the host, most of the footprint is the controller, the heap fell over about 2 minutes (so something grows with
  the advertisements, not yet identified, TASK-2025 AC2), and nothing was measured that shows a cap would hold.
- **Keep ADR-222 and only rewrite the warning.** Rejected: an option that is known to hang the unit should not be offered.

## Consequences

**Positive:**

- The BLE starvation and hang of TASK-2025 cannot happen on a board without PSRAM: the scanner never starts.
- A unit in the field that has BLE stored on is fixed by updating: the first boot of a build with this ADR clears it.
- REST, MQTT and the web UI give the same answer, and a REST client is told why.

**Negative:**

- No BLE sensors on boards without PSRAM at all, also for a user who had it working at low traffic. No such user is
  known.
- A unit already on v2.0.0-beta.360 or earlier with BLE on still needs the workaround until it is updated: switch BLE
  off over REST while it answers, then reset or power-cycle.
- `SATbleriskack` stays in `settings.ini` without an effect.
- Why the heap-emergency restart and the task watchdog did not recover the hung unit is not answered here (TASK-2026).

## Open Questions

- None.

## Related Decisions

- **ADR-222 (On boards without PSRAM BLE starts off and warns when the user enables it):** superseded. Its default for a
  fresh install (BLE off without PSRAM) stays; its warning-and-accept path and "a stored SATbleenable always wins" go.
- **ADR-169 (BLE on by default with a no-PSRAM risk acknowledgement):** the acknowledgement it introduced no longer
  opens the gate.
- **ADR-185 (NimBLE observer-only host trims):** relevant to alternative C.

## References

- TASK-2025 (backlog): 09's measurements on .61, the code reading of v2.0.0-beta.360 and origin/dev, the options.
- TASK-2026 (backlog): the missing self-recovery of the hung unit.
- `src/OTGW-firmware/SATble.ino:627` (`bleActive()`), `src/OTGW-firmware/settingStuff.ino:572` (`bleEnableAllowed()`),
  `:702` (boot clear), `:1321` (setter), `src/OTGW-firmware/restAPI.ino:4573` (409),
  `src/OTGW-firmware/data/v2.js:5017`, `:5042`, `:5081`, `:5133` (web UI).
- Discord direct messages 1572596080632139908 (question) and 1572600155788546051 (Robert's reply "A").
