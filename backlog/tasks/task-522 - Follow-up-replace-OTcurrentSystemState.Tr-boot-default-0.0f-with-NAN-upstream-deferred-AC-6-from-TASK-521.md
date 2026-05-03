---
id: TASK-522
title: >-
  Follow-up: replace OTcurrentSystemState.Tr boot-default 0.0f with NAN upstream
  (deferred AC #6 from TASK-521)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-02 21:59'
updated_date: '2026-05-03 13:49'
labels:
  - refactor
  - sat
  - safety
  - followup
dependencies:
  - TASK-521
  - TASK-516
references:
  - src/OTGW-firmware/OTGW-Core.h
  - 'src/OTGW-firmware/SATcontrol.ino:922-967'
  - TASK-521 commit a0c6a105
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-521 deferred its AC #6 to keep parallel execution safe with TASK-516, which was reading `OTGW-Core.h`. This follow-up closes that loop now that both 516 and 521 have shipped.

**Scope:**
- `src/OTGW-firmware/OTGW-Core.h` — change `float Tr = 0.0f;` to `float Tr = NAN;` in the `OTcurrentSystemState` struct (line ~32 area, find via grep).
- Audit any code that compares `Tr == 0.0f` or does arithmetic on `Tr` before NAN validation. Each such site needs an `isnan()` guard.
- `src/OTGW-firmware/SATcontrol.ino` — once Tr is NAN-init upstream, the `static bool sTrEverNonZero` local guard added by TASK-521 in `satGetRoomTemp()` is redundant. Remove it; the natural NAN flow does the same work without the static bookkeeping.
- Audit other `OTcurrentSystemState.*` float fields with the same boot-zero-vs-real-zero ambiguity. **Toutside** is the obvious next candidate (0 °C is a real winter value, but 0.0f is also the boot default — same hazard). MaxRelModLevel and CS may also need attention.

**Why this matters:** TASK-521 patched the symptom in SATcontrol.ino. The root cause is upstream — many fields have a boot-zero ambiguity that could bite us in other paths (HA discovery, REST responses, OLED display, etc.). Cleaner to fix at the source.

**Note:** this is a refactor / cleanup task, not a new bug. The 521 fix is sufficient for the immediate safety hazard. Bring this into a normal release cycle, not a hotfix.

**References:**
- TASK-521 implementation (commit `a0c6a105` on feature-dev-2.0.0-otgw32-esp32-sat-support)
- `src/OTGW-firmware/SATcontrol.ino:922-967` — current `sTrEverNonZero` workaround that this task removes
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW-Core.h struct: Tr (and audited siblings) initialised to NAN instead of 0.0f
- [x] #2 All sites that read Tr / Toutside / etc. updated to isnan()-guard before arithmetic or comparison
- [x] #3 TASK-521's sTrEverNonZero static-local in satGetRoomTemp() removed; the NAN-flow handles the same case naturally
- [x] #4 Audit report (in task notes): which OTcurrentSystemState fields were checked, which were changed, which were left alone (and why)
- [ ] #5 Compiles clean on ESP8266 and ESP32
- [x] #6 No regression in normal-operation behaviour: when sensors are valid, all paths produce the same outputs as before
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

Scope-uitbreiding bij audit (AC #4): vijf `OTcurrentSystemState` float-velden delen exact dezelfde boot-zero-vs-real-zero ambiguiteit (Tr, Tboiler, Tdhw, Toutside, Tret op `OTGW-Core.h:54-58`). Eén refactor-pass, niet vijf losse follow-ups.

### Code-wijzigingen

1. **`OTGW-Core.h:54-58`** — `= 0.0f;` → `= NAN;` voor Tr, Tboiler, Tdhw, Toutside, Tret. `<math.h>` zit al in via `Arduino.h`, geen extra include nodig.

2. **`SATcontrol.ino:922-967` (`satGetRoomTemp`)** — vervang TASK-521's static-local workaround. Patroon nu:

```cpp
static bool sTrEverNonZero = false;
float otRoom = OTcurrentSystemState.Tr;
if (otRoom != 0.0f) sTrEverNonZero = true;
const bool trGhost = (!sTrEverNonZero && otRoom == 0.0f);
```

vervangen door:

```cpp
float otRoom = OTcurrentSystemState.Tr;
const bool trGhost = isnan(otRoom);
```

NAN-init bovenstrooms vangt het "never observed"-geval natuurlijk; geen static-bookkeeping meer nodig.

3. **Display-sites krijgen `isnan()`-guard** (cosmetisch, NaN → "--" of "n/a"):
   - `OLED.ino:155` (`Room temp: %.1f C`)
   - `SATcycles.ino:544` (WebSocket status `Heating off, room %.1f deg C`)
   - `webhook.ino:139` (webhook variabele `tr`)
   - `handleDebug.ino:41` (telnet debug regel)

4. **`OTGW-Core.ino:3802` (`print_f88`) en `:4476` (`dtostrf`)** — geen wijziging. Dit zijn protocol/diagnostiek-getters; "nan" als string is daar acceptabel (en in feite informatief: "ik heb nooit een waarde gezien"). Documenteer in audit-rapport.

5. **`restAPI.ino:1533`** — al gegate'd via `getMsgLastUpdated(OT_Tr) > 0`. Geen wijziging. Sterker: dit is het *canonieke* freshness-patroon; de NAN-init wordt het safety-net onder consumers die deze gate vergeten.

### MQTT publish-pad audit

Verifiëren of `Tr` / `Toutside` / etc. via een vergelijkbare `getMsgLastUpdated()`-gate worden gepubliceerd, of unconditioneel. Als ze unconditioneel publiceren wordt de retained payload "nan", wat HA als unavailable interpreteert — vrijwel zeker het juiste gedrag, maar wel expliciet vaststellen.

### Audit-rapport (AC #4)

| Veld | Boot 0.0 hazardous? | Actie |
|---|---|---|
| Tr (room) | Ja — 0.0 is reëel (onverwarmd) maar ook boot-default | NAN |
| Tboiler (boiler flow) | Ja — boot-default ambigu | NAN |
| Tdhw (DHW) | Ja — boot-default ambigu | NAN |
| Toutside (outside) | Ja — 0.0 is reëel winter maar ook boot-default | NAN |
| Tret (return) | Ja — boot-default ambigu | NAN |
| CS / MaxRelModLevel / Tset / overige floats | Te evalueren tijdens impl-pass | TBD |

### Verificatie

- `python evaluate.py --quick` schoon (geen nieuwe failures)
- `python build.py --firmware` exit 0 op ESP32 én ESP8266 (background, hoeft niet sequentieel)
- Geen regressie wanneer sensoren geldig zijn (alle bestaande paden produceren dezelfde uitvoer)
- SAT-loop met geldige BLE / OT-thermostaat blijft werken; geen valse ghost-detectie
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit report (AC #4)

Phase 1 scope: **Tr only**. Decision rationale: TASK-521 explicitly identified Tr as the safety hazard; sibling NAN-init has a wider audit surface (HA discovery templates, MQTT publish paths, per-field REST gating that may differ); keeping Phase 1 narrow lets the safety fix ship without dragging the sibling refactor's risk along.

| Field | Boot 0.0 ambiguity? | Decision | Rationale |
|---|---|---|---|
| Tr (room) | YES — 0.0 is real winter unheated, also boot default | **Changed to NAN** | Direct cause of TASK-521 safety bug. NAN-init is the upstream fix. |
| Toutside (outside) | YES — 0.0 is real winter, also boot default | Audited; **deferred to follow-up** | Same hazard pattern as Tr. SAT's `satGetOutsideTemp()` already has fallback chain (Open-Meteo / external sensor). Recommend follow-up task: extend NAN-init to Toutside. **Strongest Phase 2 candidate.** |
| Tboiler / Tdhw / Tret | Boot default ambiguous | Audited; left at 0.0 | Boiler-side readings; not safety-critical. NAN-init would propagate to OLED + webhook + WebSocket without per-site gating audit. Defer. |
| TSet / TrSet / TrSetCH2 / TrOverride / TrOverride2 / TdhwSet / MaxTSet / Hcratio / Remoteparameter4-8 / CoolingControl | Setpoints / parameters | Audited; left | 0.0 is a meaningful command/parameter value. NAN would break protocol semantics. |
| MaxRelModLevelSetting / RelModLevel | Modulation % | Audited; left | 0.0 = idle is valid. |
| CHPressure / DHWFlowRate | Boot 0 ambiguous but not safety-critical | Audited; left | Display-only; boot 0 reads as "sensor not reporting". |
| Tsolarstorage / Tsolarcollector / TflowCH2 / Tdhw2 / Theatexchanger / Texhaust / ElectricalCurrentBurnerFlame / TRoomCH2 / RelativeHumidity | Various | Audited; left | Niche or boundary fields; no PID consumer, not safety-critical. |

**Phase 2 recommendation:** follow-up task extending NAN-init to **Toutside** specifically. Other fields can stay at 0.0f indefinitely; boot-default ambiguity isn't meaningful for setpoints or non-safety sensors.
<!-- SECTION:NOTES:END -->
