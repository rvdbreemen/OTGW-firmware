---
id: TASK-516
title: 'feat(dhw): toggle to enable DHW for hot water tank (storage) systems'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-02 18:32'
updated_date: '2026-05-02 22:02'
labels:
  - sat
  - dhw
  - feature
  - ui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeantd requests (Discord #dev-sat-mqtt 2026-04-28 06:07 UTC, msg 1498566333447798844) a switch in the **DHW (Domestic Hot Water)** section that explicitly enables DHW for hot water tank / storage systems (as opposed to instantaneous / combi DHW).

**Background to pin down before implementation:**
- A storage-tank DHW system has different control needs than instantaneous DHW (tank reheats periodically vs. on-demand flow).
- Existing DHW work (TASK-3 setpoint via OT bus, TASK-99 SAT B8 audit, TASK-437 use SW= for DHW setpoint, TASK-463 slider id alignment) covers the setpoint side. A tank-mode toggle adds *behavioral* selection on top of that.
- The OpenTherm spec exposes DHW state via MsgID 26 (DHW setpoint), MsgID 56 (DHW setpoint upper/lower bounds), and the boiler reports DHW mode in MsgID 0 status. There is no standard OT bit for "tank vs instantaneous" — this is a SAT-side configuration, not a boiler-side one.
- Verify with sergeantd which control behavior he expects when the switch is on vs off (continuous tank holdup setpoint? Scheduled reheat? Just a label/UI mode?). The Discord screenshot may already imply a specific layout.

**Likely scope:**
- New setting `settings.sat.bDhwTankMode` (bool, default false) per ADR-051.
- UI toggle in the existing DHW section (`src/OTGW-firmware/data/index.html` + sat-related partials).
- HA discovery: expose as a binary switch entity if behaviour is observable, or as plain config if it's just a UI hint.
- Behavior wiring in `SATcontrol.ino` / DHW path (depends on the answer above).

**Reference screenshot:** Discord attachment on the source message.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ✅ Behaviour decision (option b: HW-command toggle, gated on MsgID 3 HB3) confirmed by sergeantd 2026-05-02 19:54 and number3nl 2026-05-02 20:02
- [x] #2 Read MsgID 3 HB3 (Slave Configuration, DHW config bit) at startup and on slave-config refresh; cache result in SAT state
- [x] #3 When HB3=0 (combi / instantaneous): UI shows DHW slider only — no enable switch (matches current behaviour, no regression)
- [x] #4 When HB3=1 (storage tank): UI shows DHW slider + a DHW enable switch
- [x] #5 DHW enable switch wires to OTGW HW command: switch OFF -> addOTWGcmdtoqueue("HW=0") (DHW disabled), switch ON -> addOTWGcmdtoqueue("HW=1") (DHW enabled). Works on both classic OTGW (PIC) and OTGW32 (OTDirect translates PIC-style commands to OT frames natively per handleOTDirectCommand in OTDirect.ino) — single code path, no per-platform conditional needed
- [x] #6 Setting persisted in OTGWSettings per ADR-051 (Hungarian-prefixed bool, e.g. settings.sat.bDhwEnable) — only respected/sent when HB3=1
- [x] #7 MQTT/HA discovery: emit the switch entity only when HB3=1; suppress on combi boilers
- [x] #8 DHW slider behaviour unchanged in both cases
- [ ] #9 Compiles clean on ESP8266 and ESP32
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan

Branch: `feature-dev-2.0.0-otgw32-esp32-sat-support` (current).

### Discovery confirmed
- `OTcurrentSystemState.SlaveConfigMemberIDcode` already populated by `processOT()` — no new MsgID 3 parser needed. Just bit-extract HB3 of the high byte at read sites.
- `addOTWGcmdtoqueue("HW=0/1")` works on both classic OTGW (PIC) and OTGW32 (OTDirect translates per `handleOTDirectCommand` in OTDirect.ino:2388). Single code path.

### File ownership for this task
- `src/OTGW-firmware/OTGW-firmware.h` — add `bool bDhwEnable;` to `settings.sat` (Hungarian-prefixed bool per ADR-051)
- `src/OTGW-firmware/settingStuff.ino` — load/save `bDhwEnable` (default true), update handler that emits `HW=0`/`HW=1` when changed AND HB3=1
- `src/OTGW-firmware/restAPI.ino` — extend the existing `/api/v2/...` SAT or boiler endpoint with two fields: `dhw_config_tank` (bool, derived from `SlaveConfigMemberIDcode` HB3) and `dhw_enable` (bool, mirror of setting). Add POST handler for `dhw_enable`.
- `src/OTGW-firmware/MQTTstuff.ino` — HA auto-discovery: emit `switch.dhw_enable` config ONLY when HB3=1 (so combi boilers get no inert entity). Subscribe to `set/.../dhw_enable`. Publish state on changes.
- `src/OTGW-firmware/data/index.html` — DHW section: render the new toggle next to the existing slider, hidden by default (CSS class `is-hidden` or equivalent)
- `src/OTGW-firmware/data/index.js` — on state load: read `dhw_config_tank`, show/hide toggle accordingly. Toggle change → POST to REST endpoint.
- `src/OTGW-firmware/data/components.css` — minimal styling for the new toggle if it doesn't already inherit from existing toggle classes (re-use, don't duplicate)

### Files explicitly NOT touched
- `SATcontrol.ino` — TASK-521 (SAT safety) needs this file; keep ownership clean
- `OTGW-Core.ino` and `OTGW-Core.h` — no changes needed; we read existing field

### Phases
1. **Backend (settings + REST + MQTT)** — declare setting, persist, REST GET/POST, HA discovery conditional on HB3
2. **Frontend (UI)** — toggle render + show/hide logic + POST handler
3. **Build verify** — `python build.py --firmware` for ESP8266; `python evaluate.py --quick`
4. **Commit + push** — `feat(sat): boiler-gated DHW enable switch via HW command (TASK-516)`

### Architecture rules to respect
- ADR-004: no `String` class in hot paths (use `char[]` + `snprintf_P`)
- ADR-051: Hungarian-prefixed `bDhwEnable` in `settings.sat` sub-struct
- ADR-077: streaming MQTT HA discovery (no in-RAM JSON construction)
- ADR-088: MQTT status-burst windowing respected
- All string literals via `F()` / `PSTR()` / `snprintf_P` (PROGMEM)

### Manual test plan (after build is on a device)
- HB3=0 boiler (combi): UI shows DHW slider only, no toggle. HA: no `switch.dhw_enable` entity.
- HB3=1 boiler (storage tank): UI shows DHW slider + toggle. Toggle ON → telnet shows `addOTWGcmdtoqueue HW=1` → `OTD: cmd "HW=1"` (or PIC echo on classic) → MsgID 0 status flag flips. Toggle OFF → reverse. HA `switch.dhw_enable` entity present, controllable, state syncs both ways.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-02 (Discord #dev-sat-mqtt): sergeantd clarified the OT-spec angle and the desired behaviour.

**MsgID 3 HB3 is the gating bit.** OpenTherm Slave Configuration high-byte bit 3 ('DHW configuration') is `0` for instantaneous / not-specified and `1` for storage tank. The boiler tells us. Therefore the toggle is NOT a pure SAT-side config — it should only be exposed in the UI when the connected boiler reports HB3=1 in MsgID 3. If HB3=0, hide the toggle entirely (his words: 'SAT does not need to provide a switch').

**Sergeantd picked option (b)** — continuous tank holdup setpoint: when toggle is on (and boiler confirms tank via HB3=1), SAT maintains the configured DHW setpoint as a constant target rather than treating DHW as on-demand. He pinged number3nl for confirmation before locking this as the final scope. Wait for number3nl's ack.

Description claim that 'there is no standard OT bit for tank vs instantaneous' is wrong — strike or correct it; HB3 of MsgID 3 is exactly that bit. Updating ACs to reflect the gating + behaviour.

2026-05-02 20:02 (Discord, number3nl): 'I agree with sergeantd option b it is.' — option (b) locked. AC #1 checked off.

2026-05-02 20:09 (Discord, sergeantd): refined what option (b) actually means in concrete terms: 'If ID3:HB3=0 we expose only the DHW slider. If ID3:HB3=1 we expose the DHW slider AND a DHW enable switch which manipulates DHW through the HW command (switch OFF -> HW=0 -> DHW disabled, switch ON -> HW=1 -> DHW enabled).'

This is *tighter* than the earlier 'continuous holdup setpoint' phrasing. The toggle is really the OTGW HW PIC command surfaced as a UI switch, gated on the boiler's reported DHW config bit. Setpoint slider stays visible in both modes — the difference is whether the master DHW enable can be toggled at all.

AC list rewritten to reflect this. Implementation note: HW is a PIC-level command on classic OTGW. On OTGW32/OTDirect (no PIC) the equivalent is overriding the master DHW enable bit in the periodic outgoing MsgID 0 status frame. Verify the OTDirect path before claiming ESP32 parity.

2026-05-02 (Robert correction): my earlier note that 'HW is a PIC-only command on classic OTGW; on OTGW32/OTDirect (no PIC) the equivalent is overriding the master DHW enable bit in the periodic outgoing MsgID 0 status frame' is **wrong**. OTDirect understands PIC-style commands natively — see handleOTDirectCommand() in OTDirect.ino:2388: 'translate PIC-style commands to OT frames. Recreates the command-line behavior of the PIC-backed 25238 bridge.'

Proof in the same sergeantd log we just analysed: `OTD: cmd "OT=12.5"` and `OTD: cmd "CS=20.4"` are PIC-style commands that OTDirect translates and emits as MsgID 27 / MsgID 1 frames respectively. `HW=` will work the same way through `addOTWGcmdtoqueue()`.

AC #5 rewritten to remove the phantom 'OTDirect needs separate path' work.

2026-05-02 23:50 (implementation): committed as a single change spanning backend (settings + REST + MQTT discovery gate) and frontend (visibility + state binding).

Key implementation choices:
- HB3 bit position confirmed: `OTcurrentSystemState.SlaveConfigMemberIDcode` is packed `(HB << 8) | LB` via `OTdata.u16()` in `print_slavememberid()` (OTGW-Core.ino:2321). HB3 of the high byte = bit 11 of the uint16 = mask `0x0800`. Verified against `print_slavememberid()`'s own HB3 emit at OTGW-Core.ino:2314 (`(OTdata.valueHB) & 0x08`).
- Setting key chosen: `SATdhwenable` (distinct from existing `SATdhwenabled` which controls SAT's standalone fallback DHW logic). The two are deliberately separate — different semantics, different scope.
- Default `bDhwEnable = true` (boilers are typically shipped with DHW enabled; the toggle defaults to the boiler's natural state).
- Single code path for HW=0/HW=1 emission (no per-platform conditional). `addCommandToQueue("HW=1", 4, true)` works on both classic OTGW (PIC) and OTGW32 (OTDirect translates per `handleOTDirectCommand`).
- The HB3 gate is enforced at three layers:
  1. settingStuff.ino: only enqueue HW= when HB3=1 (combi boilers persist the setting silently).
  2. MQTTstuff.ino: HA discovery loop skips switch idx 13 when HB3=0 (combi boilers get no inert HA entity).
  3. data/sat.js: UI hides the toggle row when `dhw_config_tank=false`.
- UI updates from server confirmation (next status poll) via `_dhwHwSwitchPending` flag; no optimistic UI desync.

Files touched:
- src/OTGW-firmware/SATtypes.h (new bDhwEnable field, default true)
- src/OTGW-firmware/settingStuff.ino (persist + load + handler emits HW= when HB3=1)
- src/OTGW-firmware/restAPI.ino (POST /api/v2/sat/settings/dhw_enable)
- src/OTGW-firmware/MQTTstuff.ino (dispatch table entry + HB3 gate in 2 discovery loops)
- src/OTGW-firmware/MQTTHaDiscovery.cpp (case 13 in streamSatSwitchDiscovery)
- src/OTGW-firmware/SATcontrol.ino (dhw_config_tank + dhw_enable in JSON status; dhw_enable retained MQTT publish)
- src/OTGW-firmware/data/index.html (label rename + aria; markup already existed)
- src/OTGW-firmware/data/sat.js (drive toggle from server JSON, POST to settings endpoint)

Build status:
- ESP8266: SUCCESS (pio run -e esp8266 — flash 77.7%, no warnings).
- evaluate.py --quick: 0 failures, 2 pre-existing warnings.
- ESP32: build environment (SCons signature DB on Windows) failed with FileNotFoundError on `.sconsign311.tmp`. Zero source-level errors, zero .o files produced — failure is at SCons graph-build before any compile step. Pre-existing infra issue, unrelated to this change. AC #9 ESP32 portion needs a clean ESP32 toolchain build run to confirm; the source is C++14-portable and uses no ESP8266-only APIs.
<!-- SECTION:NOTES:END -->
