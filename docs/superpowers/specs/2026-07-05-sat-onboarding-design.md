# SAT onboarding wizard — design

**Date:** 2026-07-05
**Branch:** `dev` (2.0.0 ESP32-S3 async)
**Status:** Design — awaiting implementation plan
**Origin:** #alpha-testing dialog (2026-07-04), number3nl + sergeantd + crashevans

## Problem

Enabling SAT today flips a single `satenabled` toggle and drops the user into a
wall of ~46 raw SAT settings. Field testers asked for a guided setup instead.
sergeantd's steer: "match the config flow you get when you first set up SAT in
Home Assistant — we already use those questions." crashevans proposed a richer
commissioning assistant (data-source health, a control ladder). Robert
(number3nl): build the SAT onboarding that fires the moment a user enables SAT.

## Ground truth: the HA SAT config flow

Source: `other-projects/SAT-releases-thermo-nova/custom_components/sat/config_flow.py`
(the actual Alexwijn SAT HA integration, read 2026-07-05). The initial-setup
steps, stripped of HA-side gateway-connection plumbing the OTGW doesn't need:

| HA step | Question | On-device firmware key |
|---|---|---|
| `heating_system` | Radiators / Heat Pump / Underfloor | `satsystem` (0 auto/1 rad/2 UFH) + `satsource` |
| (appliance, via TASK-997) | Gas boiler / Heat pump | `satsource` (0 auto/1 gas/2 HP/3 hybrid) |
| `sensors` | inside (room) temp; outside temp; humidity (opt) | `satsensorarea0`; OT-bus Toutside or `satweather*`; `sathumidity*` |
| `automatic_gains` | auto-tune PID? yes/no | `satautogains` |
| `pid_controller` | P / I / D (only when auto off) | **no raw P/I/D on device** → `satcoefficient` (curve slope) + `satautogains` |
| `manufacturer` | boiler brand (auto-detected from OT member-id, editable) | `satmanufacturer` (0 auto…18 other) |

Every question has an existing firmware setting. The v2 UI also already ships a
full wizard engine (`startOnboarding`, TASK-997): full-screen overlay, screen
state-machine, choice-cards, live test-connection, `post()` → `/api/v2/settings`,
persist-once flag. **This feature is content + a trigger, not new plumbing.**

Two firmware-vs-HA parity caveats, carried into the design deliberately:
- The firmware exposes no raw P/I/D. "Full HA parity" for the tuning step means
  the on-device analog: Automatic (`satautogains`) vs Manual (`satcoefficient`
  slope + `satautogains`). We do **not** invent P/I/D fields.
- HA's `heating_system` folds "Heat Pump" into the system enum; on-device the
  heat-pump/gas/hybrid distinction lives in `satsource`, and `satsystem` only
  carries Radiators/Underfloor. The wizard's system screen writes both keys.

## Decisions (locked with maintainer, 2026-07-05)

1. **Trigger:** on `satenabled` false→on transition, once ever
   (new `sat_onboarded` persist flag). Plus two more entry points below.
2. **Depth:** full HA parity — all steps incl. manufacturer + tuning branch.
3. **Sources health screen:** yes, included (crashevans' idea; cheap because
   `/api/v2/health` already exposes link ages).
4. **Re-run entry:** both — a button on the SAT page **and** in Advanced > System
   (next to TASK-997's "Re-run first-time setup").
5. **Migration:** existing SAT users (`satenabled` already true, no
   `sat_onboarded` key) are shown the wizard **once** on their next SAT-page
   visit, so they benefit from the guided sources/health check.
6. **No source selection in the wizard:** onboarding never binds a sensor input
   (BLE, DS18B20, thermostat-over-MQTT, weather). Those live in their own settings;
   the wizard only shows source *health* read-only. Deliberate deviation from HA's
   `sensors` step.
7. **Tuning has no raw P/I/D:** confirmed with maintainer — firmware exposes none,
   so Manual tuning = `satcoefficient` (curve slope) + `satautogains` only.
8. **Deferred (explicit):** crashevans' Off→Observe→Advise→Assist→Control ladder
   needs SAT run-modes the firmware does not have. Captured as a separate backlog
   task, not built here.

## Trigger logic (client-side, `v2.js`)

Three entry points, all funnel into one `startSatOnboarding()`:

- **Fresh enable** — in the settings-render/toggle handler, when `satenabled`
  goes false→on, call `maybeShowSatOnboarding('enable')` if `sat_onboarded` is
  falsy.
- **Migration** — on first navigation to the SAT page, if `satenabled` is true
  AND `sat_onboarded` is falsy, call `maybeShowSatOnboarding('migrate')`. Because
  the flag defaults false on upgraded installs (see firmware section), every
  existing SAT-on user hits this exactly once.
- **Manual re-run** — SAT-page button and Advanced>System button call
  `startSatOnboarding()` directly.

**Abandon semantics (safety-critical):**
- `enable` path: SAT stays **disabled** until the user reaches Finish. Closing
  the overlay leaves `satenabled=false` and `sat_onboarded=false` → wizard
  reappears next attempt. No control writes happen mid-wizard.
- `migrate` path: SAT was already running fine. Closing the overlay sets
  `sat_onboarded=true` (opt-out) so the migration prompt shows only once; SAT
  keeps running unchanged.

## Screens (full-screen overlay, HA-parity, on-device-adapted)

**No sensor-source selection happens in the wizard** (maintainer decision,
2026-07-05). The wizard never asks the user to pick a room sensor, outside-temp
source, or any input (BLE / DS18B20 / thermostat-over-MQTT / weather). Those are
configured in their own dedicated settings (Sensors page, SAT weather Detect
Location, etc.). The wizard only *surfaces whether sources are healthy*
(read-only, screen 5) and points the user at where to configure a missing one.
This is a deliberate deviation from HA's `sensors` step: SAT commissioning here
covers the control parameters, not sensor wiring.

1. **Welcome** — "Let's tune SAT for your system." States SAT stays disabled
   until Finish.
2. **Heating system** — Radiators / Underfloor / Heat pump choice-cards. Writes
   `satsystem` (+ `satsource` when Heat pump). Pre-fill `satsource` from the
   TASK-997 answer so gas/HP is not asked twice.
3. **Manufacturer** — `satmanufacturer` dropdown, defaulted to the OT member-id
   auto-detect, editable. Mirrors HA's `manufacturer` step.
4. **Tuning** — Automatic (default; `satautogains`=1.0, keep `satcoefficient`
   default) vs Manual (reveal `satcoefficient` slope slider + `satautogains`).
   On-device analog of HA's automatic_gains → pid_controller branch. Firmware has
   no raw P/I/D (confirmed with maintainer), so Manual = curve slope + auto-gains,
   not P/I/D fields.
5. **Sources health** — read-only checklist from `/api/v2/health` + `sat/status`:
   OT bus online · Room temp available · Outside temp available · Flow temp ·
   Return temp (if supported) · MQTT connected. Green/amber. When a critical source
   is missing, warns and links to the relevant settings page — but does **not** let
   the user select or bind a source here.
6. **Done** — summary card (system, tuning, source-health snapshot). `commit()`
   writes every collected key **plus `satenabled=true` and `sat_onboarded=true`**,
   closes the overlay, refreshes the SAT page.

## Firmware change (minimal — mirrors `ui_onboarded` exactly)

Add a `sat_onboarded` bool to `OTGWSettings`, same 4-touch pattern as
`ui_onboarded` (TASK-997) but with the **opposite** migration default:

1. Field in the settings struct (`settings.ui.bSatOnboarded` or `settings.sat.*`),
   default `false`.
2. Serialize in `settingStuff.ino` `writeSettings()` (`writeJsonBoolKV`).
3. Parse case in the settings setter (`EVALBOOLEAN`).
4. Expose in `restAPI.ino` settings dump (`addBool`).

**Migration default is `false` for every install — fresh and upgraded alike.**
Unlike `ui_onboarded` (which marks existing installs as already-onboarded so the
first-time wizard never re-appears), we deliberately want existing SAT users to
see the SAT wizard once. Defaulting the flag false and letting the client trigger
logic gate it (`satenabled` true + `sat_onboarded` false → prompt once on SAT
page) achieves that with no `g_sawKey`-style presence detection. No other firmware
settings are added — every wizard answer targets an existing key.

## Files touched

- `src/OTGW-firmware/data/v2.js` — `startSatOnboarding()` (6 screens, no source
  pickers) + the three trigger hooks + two re-run buttons (SAT page,
  Advanced>System).
- `src/OTGW-firmware/data/v2.html` / `v2.css` — re-run button(s), any wizard-only
  styles not covered by the shared overlay CSS.
- `src/OTGW-firmware/settingStuff.ino` — `sat_onboarded` write + parse + migration.
- `src/OTGW-firmware/restAPI.ino` — `sat_onboarded` in the settings dump.
- `src/OTGW-firmware/*types.h` — the struct field.
- Prerelease bump (touches `src/**` → version policy requires it).

## Out of scope (separate tasks)

- Off→Observe→Advise→Assist→Control SAT run-mode ladder (needs firmware modes).
- Any new SAT control logic — the wizard only writes existing settings.

## Verification

- On-device: fresh device → enable SAT → wizard walks all 8 screens → Finish
  writes keys, SAT active, `sat_onboarded=true`, wizard never reappears.
- Abandon on `enable` path → SAT stays off, wizard reappears.
- Existing SAT user (migrated) → wizard once on SAT-page visit → dismiss keeps
  SAT running and never reappears.
- `python build.py --target esp32` green; `python evaluate.py --quick` no new
  failures; light + dark themes; 360px width.
