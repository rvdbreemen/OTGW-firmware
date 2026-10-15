# Classic vs v2 Web UI gap audit (TASK-1223.11)

**Summary.** 60 user-visible features of the classic bundle were compared against the v2 bundle on branch `dev` (2026-10-15).
29 are already covered by v2. 31 are gaps (15 missing in v2, 14 partial, 2 not applicable).
Proposal: **port 19**, **drop on purpose 12**. "Drop" is a proposal for the maintainer, not a decision.
One firmware change is a hard blocker for deleting the classic files: `FSexplorer.ino:184` keys the root route on `/index.html` (section 5).

Method: static read of `src/OTGW-firmware/data/{index.html,index.js,graph.js,sat.js,v2.html,v2.js}` plus `restAPI.ino`, `FSexplorer.ino`, `webSocketStuff.ino`, `updateServerHtml.h`, `evaluate.py`, `build.py`. Every endpoint string was extracted with grep (`fetch(`, `/api/`, `v2/...`, `location.href`, `/pic?`, `/ws`) and every dynamic URL builder (`satPost`, `blePost`, `bleAction`, `otgw/command/${cmd}`) was resolved by hand. No build, no flash, no browser run.

**Evidence tags.** `V` = verified by reading the code at the cited lines. `I` = inferred from code paths, not traced end to end. Nothing in this report was checked on hardware or in a browser.

---

## 1. Inventory per bundle

### 1.1 Pages, tabs and views

| Area | Classic | v2 |
|---|---|---|
| Top nav | Home, SAT, Settings, Advanced dropdown (PIC firmware, Webhook, Debug Information, File system contents) `index.html:157-173` | Home, SAT, Monitor, Settings, Advanced, Diagnose (only when the PIC runs diagnose firmware) `v2.html:134-141` |
| Home | OT-Direct status panel, OT monitor value table, log section with tabs Log / Statistics / OT Support / Graph `index.html:178-423` | Three home views: System view, At a glance, Mission control `v2.html:145-409` |
| Monitor | (inside Home) | Log, Statistics, OT Support, Graph, Connection `v2.html:522-740` |
| SAT | Views Thermostat / Expert / Diagnostics `index.html:461-465`, separate SAT Settings page `index.html:691-700` | Layers Thermostat / Control / Technical with SAT settings embedded `v2.html:418-519` |
| Settings | Generic list from GET settings with fixed-IP editor and WiFi scan `index.js:6778-7063` | Category rail + cards from GET settings `v2.js:1747-1925` |
| Advanced | PIC firmware page, Debug Information, Webhook page, link to /FSexplorer | PIC firmware, Debug Information, File System (in-page), System actions `v2.html:763-853` |
| Other | `design.html` design showcase | Diagnose page `v2.html:859-892`, first-run and SAT onboarding wizards `v2.js:3841-4252` |

### 1.2 Settings fields

Both bundles build the settings form generically from `GET /api/v2/settings` (`index.js:6778`, `v2.js:1747-1756`) (V). Every key the firmware emits therefore appears in both UIs. Differences:

- Classic hides the seven `ui_*` preference keys and the webhook keys from the Settings page (`index.js:6242-6255`) and edits them on their own controls (log/graph toggles, Webhook page).
- v2 hides `ui_usev2`, `ui_onboarded`, `sat_onboarded` (`v2.js:1711-1716`) and shows the other `ui_*` keys as plain editable rows, but never reads them back to set its own chips (V: no reader outside `SET_META` / `ENUM_OPTS`).
- All 79 SAT keys the classic SAT Settings page edits are present in `GET /api/v2/settings` (V, diff of `index.js:3832-4027` against `restAPI.ino:3757+`).
- `satblenameprefix` and `satblenamefilteringest` are posted by classic (`index.js:4637`, `index.js:4650`) but are neither emitted by GET nor in the POST whitelist `knownSettings` (`restAPI.ino:4200`, enforced at `restAPI.ino:4258`). The classic save therefore gets a 400 (V in code, not run).

### 1.3 WebSocket usage

| | Classic | v2 |
|---|---|---|
| Endpoint | `ws://<host>/ws` `index.js:1232` | `ws://<host>/ws` `v2.js:412` |
| Sent by the browser | nothing | nothing (V: no `.send(` in either bundle) |
| Keepalive `{"type":"keepalive"}` | ignored `index.js:1995` | ignored `v2.js:490` |
| PIC flash progress JSON `{"state":..,"flash_written":..}` (`OTGW-Core.ino:5825`, `:5885`) | consumed by `handleFlashMessage` `index.js:2000`, `index.js:8280` | dropped `v2.js:496`; v2 polls `pic/flash-status` instead |
| Status JSON `{"type":"status","msg":..}` (`OTDirect.ino:895` ff, `SATcontrol.ino:2000`, `SATcycles.ino:731`, `:772`, `:1270`) | parsed and pushed into the log buffer `index.js:2009-2023`, but `formatLogLine` has no `msg` field handling, so it renders as a near-empty line (I) | dropped `v2.js:496` |
| PIC diagnose output (STX marker) | not handled | `handleDiagnoseMessage` `v2.js:495`, `v2.js:3054` |
| OT frame text lines | log, statistics, graph | log, statistics, support map, home views, graph `v2.js:501-507` |

---

## 2. Gap table

Columns: feature, classic location, v2 status, proposed verdict, notes. Tag in the last column.

### 2.1 Header and global

| # | Feature | Classic location | v2 status | Verdict | Notes | Tag |
|---|---|---|---|---|---|---|
| 1 | Light/dark theme toggle | `index.html:119`, `theme-toggle.js` | Covered `v2.html:132`, `v2.js:107-120` (same `otgw-theme` key) | Already covered | | V |
| 2 | Switch to the other UI | `index.html:120-155` | `v2.html:131`, `v2.js:202-223` | Drop on purpose | No second UI to switch to. Remove both buttons and `ui_usev2`. | V |
| 3 | Network type and Wi-Fi strength indicator | `index.html:103-115`, `index.js:637` | Covered `v2.html:118-121`, `v2.js:332-390` | Already covered | | V |
| 4 | Device wall clock (device time, timezone, NTP state visible) | `index.html:118`, `index.js:4964-5070` (polls `device/time`) | Partial: v2 shows the browser clock `v2.js:226-232` | Port | A wrong NTP or timezone setting is invisible in v2. Cheap: one `device/time` read and a local tick, as classic does. | V |
| 5 | Live heap readout in the footer | `index.html:708`, `index.js:4944` | Partial: heap on the Debug page only `v2.js:2598` | Drop on purpose | Diagnostic value is kept on the Debug page; a permanent footer poll is not needed. | V |
| 6 | Simulation badge | `index.html:255`, `index.js:2370` | Covered `v2.html:129`, `v2.js:391` | Already covered | | V |
| 7 | Firmware/filesystem mismatch banner with link to /update | `index.html:176`, `index.js:3674-3699`; SAT copy `index.html:449`, `sat.js:89-119` | Missing (the one `mismatch` hit in v2 is the SAT setpoint health item `v2.js:3460`) | Port | App and filesystem are separate uploads on /update, so a mismatch stays possible after the classic UI is gone. | V |
| 8 | Gateway/monitor mode indicator (PIC) | `index.html:256-258`, `index.js:614` | Covered: connection map mode chip and System status `v2.js:1261-1263`, `v2.html:835` | Already covered | | V |
| 9 | WebSocket link status | `index.html:253-254` | Covered: connection strip and map `v2.js:1280-1282` | Already covered | | V |

### 2.2 Home

| # | Feature | Classic location | v2 status | Verdict | Notes | Tag |
|---|---|---|---|---|---|---|
| 10 | Full live value table (every `otgw/otmonitor` entry with unit and stale marking, polled every 2 s) | `index.js:5886-6116` | Partial: v2 shows a fixed set of about 12 values `v2.js:1133-1150`, seeded once from `otmonitor` with 9 keys `v2.js:2325-2338` | Port | Classic's table is the only place that lists every decoded value at once. v2's Statistics tab shows raw last values from frames, not the decoded otmonitor set. | V |
| 11 | Dallas sensor temperatures | `index.js:5960+` (rows from `otmonitor`, which emits Dallas values, `restAPI.ino:3032`) | Missing: Dallas temps only appear in discovery toasts | Port | Part of row 10. | V |
| 12 | S0 pulse counter live values (power kW, interval and total count) | `otmonitor` rows `restAPI.ino:3025-3029` rendered by `index.js:5960+` | Missing | Port | Part of row 10. | V |
| 13 | Edit Dallas sensor label inline | `index.js:6040-6055`, `index.js:9276-9440` (`sensors/labels` GET+POST) | Missing: v2 never calls `sensors/labels`; area pickers show raw addresses `v2.js:2274-2277` | Port | | V |
| 14 | OT-Direct status panel (mode, OT bus, thermostat, boiler, setback, step-up, schedule, active override count) | `index.html:188-228`, `index.js:2196-2245` (`otdirect/status`) | Missing: v2 never calls `otdirect/status`; on OT-Direct the mode chip reads `n/a` `v2.js:1262` and the Debug page omits the `otd*` fields of `device/info` (`restAPI.ino:3184-3204`, `v2.js:2582-2600`) | Port | See section 4 (TASK-1219). | V |
| 15 | OT-Direct overrides (SR/CR/RM/CM/UI/KI) | `index.html:229-244`, `index.js:2285-2362` | Covered: Monitor > Connection `v2.html:720-737`, `v2.js:1290-1337` | Already covered | Moved from Home to Monitor. | V |
| 16 | PS=1 (print summary) fallback: keep the value table alive by REST polling when the PIC sends no frames | `index.js:5056`, `index.js:5072-5094` (reads `psmode` from `device/time`) | Missing: v2 polls `otmonitor` only when the WebSocket is closed `v2.js:4473` | Port | In PS=1 the socket stays open but carries no frames, so v2 home views would freeze. Not run. | I |

### 2.3 Monitor: log, statistics, support, graph

| # | Feature | Classic location | v2 status | Verdict | Notes | Tag |
|---|---|---|---|---|---|---|
| 17 | Live log with search, auto-scroll, timestamps, clear, download | `index.html:274-316`, `index.js:2829-3020` | Covered `v2.html:532-560`, `v2.js:820-874` (v2 adds Pause) | Already covered | | V |
| 18 | SAT-only filter | `index.html:286`, `index.js:2626` | Covered `v2.html:541`, `v2.js:825` | Already covered | | V |
| 19 | Auto-download every 15 min | `index.html:281-284`, `index.js:3358` | Covered `v2.html:542`, `v2.js:2479-2481` | Already covered | | V |
| 20 | Stream log to a local file | `index.html:295-298`, `index.js:3112-3310` | Covered `v2.html:543`, `v2.js:2483` | Already covered | | V |
| 21 | Log depth and capture mode (2000 lines default, capture mode up to 1M lines sized to storage quota) | `index.html:291-294`, `index.js:1265-1383` | Partial: fixed 600-line buffer `v2.js:821`; Download saves only those 600 lines | Port | Support asks users for logs; 600 lines is a few minutes of bus traffic. A larger buffer or a capture toggle is enough. | V |
| 22 | Log buffer survives a page reload (localStorage) | `index.js:1414-1590`, `index.js:1623` | Missing | Drop on purpose | Stream-to-file covers long captures without storage quota risk. | V |
| 23 | Send raw OTGW command | `index.html:301-305`, `index.js:3040` (POST `otgw/commands`) | Covered `v2.html:548-553`, `v2.js:2986-3013` | Already covered | | V |
| 24 | Statistics table with sort | `index.html:319-341`, `index.js:8905-8987` | Covered `v2.html:562-572`, `v2.js:2249-2258` | Already covered | | V |
| 25 | Resizable, remembered table column widths | `index.js:8410-8714` | Missing | Drop on purpose | Cosmetic; about 300 lines of code. | V |
| 26 | Boiler-unsupported list and active gateway overrides | `index.html:342-359`, `index.js:9043-9142` | Covered `v2.html:573-581`, `v2.js:2500-2520` | Already covered | | V |
| 27 | OT Support map from persisted bitmaps (thermostat read/write, boiler acked/unsupported, survives reboot via `/ot-thermo.json`, `/ot-boiler.json`) | `index.html:362-384`, `index.js:8988-9042` (`otgw/ot-support`) | Partial: v2 builds the map only from frames seen since page load, and only knows "seen from thermostat / boiler" `v2.js:744-785` | Port | Feed the v2 matrix from `otgw/ot-support`; keep the live overlay. | V |
| 28 | Graph content: 10 series (flame, DHW mode, CH mode, modulation, control setpoint, boiler, return, room setpoint, room, outside) plus Dallas sensors, 24 h buffer | `graph.js:25`, `graph.js:100-109`, `graph.js:277-340` | Partial: 4 series (flow, return, setpoint, modulation), 1800 samples taken per frame change `v2.js:515-519`, so the 24 h chip cannot hold 24 h on a busy bus | Port | Port the missing series and a time-based buffer. Classic loads ECharts from cdn.jsdelivr.net `index.html:27`, so the classic graph and SAT charts need internet; v2 has no external dependency. | V |
| 29 | Graph PNG and CSV export, auto-save | `index.html:403-417` | Covered `v2.html:605-608`, `v2.js:2447-2472` (v2 auto-saves every 5 min, classic every 15 min) | Already covered | | V |
| 30 | Graph time windows 10 min to 24 h (8 choices) | `index.html:390-400` | Partial: 4 choices `v2.html:602-603`, `v2.js:4451-4460` | Drop on purpose | Four windows cover the use. | V |
| 31 | Graph disconnect/reconnect markers | `graph.js:33`, `graph.js:781-830` | Missing | Drop on purpose | Nice-to-have; the connection map shows link state live. | V |
| 32 | Auto-disable of the live log on small screens or behind a proxy | `index.js:1714-1829` | Not present in v2 | Drop on purpose | v2 was designed phone-first. | I |

### 2.4 Advanced: PIC, debug, files, system

| # | Feature | Classic location | v2 status | Verdict | Notes | Tag |
|---|---|---|---|---|---|---|
| 33 | PIC firmware list, flash, download newer, update check | `index.js:5095-5430`, `index.js:8046-8409` | Covered `v2.js:2622-2793` (v2 adds delete via `/pic?action=delete`, handled at `OTGW-Core.ino:6281`) | Already covered | | V |
| 34 | PIC flash progress | WS JSON + `flash/status` `index.js:8093` | Covered by polling `pic/flash-status` `v2.js:2768-2785` | Already covered | Different mechanism, same information. | V |
| 35 | PIC gateway settings (PR= values) | 15 decoded values incl. setpoint override, DHW override, GPIO functions and states, LED functions, tweaks, temp sensor, smart power `index.js:5448-5808` | Partial: 7 rows `v2.js:2730-2755` | Port | The data is already in `pic/settings`; only rendering is missing. | V |
| 36 | Debug Information: every `device/info` field (about 120, incl. `otd*`, `hd_*` heap diagnostics, `perf_*`, `disc_*`, `eth_current_*`) | `index.js:6117-6176` (generic loop) | Partial: curated list of about 30 fields `v2.js:2572-2620`, plus the raw `/api/v2/debug` dump, which has different content | Port | Add an "all other fields" group. The `hd_*` counters mirror the MQTT stats topics, so this gives them without a broker (I). | V |
| 37 | Crash/reboot log | `index.js:6177-6235` | Covered `v2.js:2551-2570` | Already covered | | V |
| 38 | Update firmware link | `index.js:3692` | Covered `v2.js:2962` | Already covered | | V |
| 39 | File system browser | link to `/FSexplorer` `index.js:3486` | Covered: in-page browser `v2.html:811-827`, `v2.js:2794-2948`, plus link | Already covered | | V |
| 41 | Browser-console helper `window.otgwDebug` (ws reconnect, logs, api, sendCmd, export) | `index.js:950-1220` | Missing | Drop on purpose | Developer tool; DevTools and the REST API cover it. | V |
| 42 | Design showcase page | `design.html` | n/a | Drop on purpose | Not user-facing. It loads `theme-toggle.js` and `sat-slider.js` (`design.html:624-625`), so delete it together with those files. | V |

### 2.5 Settings

| # | Feature | Classic location | v2 status | Verdict | Notes | Tag |
|---|---|---|---|---|---|---|
| 43 | Generic settings from GET, password masking, save | `index.js:6778-7063`, `index.js:7266-7375` | Covered `v2.js:1747-2031` | Already covered | | V |
| 44 | Fixed-IP editor: DHCP checkbox, per-octet validation, prefill from current DHCP lease | `index.js:6317-6678` | Partial: plain text inputs, no validation, no prefill `v2.js:1480-1489` | Port | A typo in a static IP locks the user out until AP reset. At least port the validation. | V |
| 45 | Confirm dialog before changing Board mode | `index.js:6953` | Missing: plain select `v2.js:2307` | Port | The classic text warns that a wrong mode leaves the gateway non-functional. One `confirm()`. | V |
| 46 | WiFi network scan | `index.js:9451-9546` (`network/scan`) | Missing | Drop on purpose | Informational only; SSID is read-only in both UIs and is set through the AP portal. | V |
| 47 | Reset Wi-Fi | `index.js:6932`, `index.js:9441` | Covered `v2.js:2974` | Already covered | | V |
| 48 | Webhook test call ON and OFF | `index.js:7101-7121` | Partial: ON only `v2.js:1932-1960` | Port | One extra button. | V |
| 49 | Device-side UI preferences applied on load (`ui_autoscroll`, `ui_timestamps`, `ui_capture`, `ui_autoscreenshot`, `ui_autodownloadlog`, `ui_autoexport`, `ui_graphtimewindow`) | `index.js:2846-2963`, `index.js:9143-9275` | Partial: v2 shows them as editable rows `v2.js:1532-1538` but never applies them | Drop on purpose | v2 keeps its own state per page. Either hide these rows or retire the keys (section 6). As is, editing them in v2 does nothing visible. | V |
| 50 | BLE roster: select, label, forget, rescan | `index.js:4390-4668` | Covered `v2.js:2033-2240` (v2 adds bind key) | Already covered | | V |
| 51 | BLE name-prefix filter and "restrict ingest" toggle | `index.js:4631-4656` | Missing | Port | Already broken in classic: the keys are not in `knownSettings` (`restAPI.ino:4200`), so POST returns 400 (V in code, not run). Port only together with a firmware fix (whitelist plus GET exposure). | V |
| 52 | Dallas or BLE sensor per SAT area | `index.js:4206-4389` (`sat/sensor-areas` PATCH, shows labels) | Covered via `satsensorarea0..3` pickers `v2.js:2266-2306` | Already covered | v2 shows Dallas addresses, not labels (row 13). | V |

### 2.6 SAT

| # | Feature | Classic location | v2 status | Verdict | Notes | Tag |
|---|---|---|---|---|---|---|
| 53 | SAT dashboard: enable, target +/-, presets, cycle mode, tiles, history, control/PID/PWM/smart/external/health/simulation panels, raw JSON | `index.html:444-690`, `sat.js:159-420`, `sat.js:833-898` | Covered `v2.html:418-519`, `v2.js:3149-3672` | Already covered | | V |
| 54 | SAT settings page | `index.js:3832-4862` | Covered on the SAT page `v2.js:1856-1877` (TASK-1211) | Already covered | | V |
| 55 | Heating curve chart | `sat.js:492-717` | Covered `v2.js:3507-3550` | Already covered | | V |
| 56 | PV boost status | `index.js:3794` | Covered `v2.js:3345` | Already covered | | V |
| 57 | Calibration markers: add by clicking the curve, delete | `sat.js:1385-1440` (POST/DELETE `sat/markers`) | Partial: list only `v2.js:3251-3262`, `v2.js:3551-3564` | Port | Without add/delete the marker feature is read-only in v2. | V |
| 58 | DHW setpoint slider | `sat.js:1175-1234`: bounds from boiler MsgID 48, sends transient `SW=<n>` via `otgw/command/` | Partial: fixed 40-65 range `v2.html:459`, posts the persisted `settings/dhw_setpoint` `v2.js:3640` | Port | Port the MsgID 48 bounds. The semantics differ (transient command vs persisted SAT setting); the maintainer should confirm v2's choice is intended. | V |
| 59 | DHW enable switch shown only with a storage tank | `sat.js:1154-1173` (MsgID 3) | Covered via `dhw_config_tank` in `sat/status` `v2.js:3437-3438` | Already covered | | V |
| 60 | Weather card and Detect location | `sat.js:899-984` | Covered `v2.js:3491`, `v2.js:3588-3603` | Already covered | Classic also sets `SATweatherenable=1`; v2 deliberately does not (`v2.js:3585-3587`). | V |
| 61 | Weather auto-prefill of location on first visit, and the OpenWeatherMap key wizard (`sat/weather/needs-setup`) | `sat.js:985-1128` | Missing | Drop on purpose | Location and API key are editable settings, and v2 has Detect location. The wizard uses `window.prompt` and validates the key against openweathermap.org from the browser. | V |

### 2.7 Counts

60 rows in total (rows 1 to 61; number 40 is unused because the Reboot button is a v2-only control, listed under section 3).

| Verdict | Count | Rows |
|---|---|---|
| Already covered | 29 | 1, 3, 6, 8, 9, 15, 17, 18, 19, 20, 23, 24, 26, 29, 33, 34, 37, 38, 39, 43, 47, 50, 52, 53, 54, 55, 56, 59, 60 |
| Port | 19 | 4, 7, 10, 11, 12, 13, 14, 16, 21, 27, 28, 35, 36, 44, 45, 48, 51, 57, 58 |
| Drop on purpose | 12 | 2, 5, 22, 25, 30, 31, 32, 41, 42, 46, 49, 61 |

The 31 gaps (port + drop) split by v2 status:

- Missing in v2 (15): 7, 11, 12, 13, 14, 16, 22, 25, 31, 32, 41, 45, 46, 51, 61.
- Partial in v2 (14): 4, 5, 10, 21, 27, 28, 30, 35, 36, 44, 48, 49, 57, 58.
- Not applicable to v2 (2): 2 (UI switch), 42 (design showcase).

---

## 3. REST and route usage: classic vs v2

"After removal" means no remaining UI calls it. Every endpoint below is documented in `docs/api/openapi.yaml` (V: grep), so HA, Telegraf or scripts may depend on it. **No deletion is proposed.**

| Method + path | Classic caller | v2 caller | Possibly UI-dead after removal |
|---|---|---|---|
| GET `/api/v2/health` | `index.js:1035` (debug helper) | `v2.js:1224`, `v2.js:4091` | no |
| GET/POST `/api/v2/settings` | `index.js:897` ff, `sat.js:886` ff, `index.html:136` | `v2.js:211`, `v2.js:1748` ff | no |
| GET `/api/v2/device/info` | `index.js:762` ff | `v2.js:2532`, `v2.js:3901` | no |
| GET `/api/v2/device/time` | `index.js:5014` | none | **yes** (unless row 4 or 16 is ported) |
| GET `/api/v2/device/crashlog` | `index.js:6225` | `v2.js:2555` | no |
| GET `/api/v2/filesystem/hash-check` | `index.js:3675` | none | **yes** (unless row 7 is ported) |
| GET `/api/v2/firmware/files` | `index.js:5114`, `index.js:5404` | `v2.js:2653` | no |
| GET `/api/v2/flash/status` | `index.js:8095` | none (uses `pic/flash-status`) | **yes** |
| GET `/api/v2/pic/update-check` | `index.js:5350` | `v2.js:2708`, `v2.js:2716` | no |
| GET `/api/v2/pic/settings` | `index.js:5696` | `v2.js:2734` | no |
| GET `/api/v2/pic/flash-status` | none | `v2.js:2770` | no |
| GET `/pic?action=upgrade` and `refresh` | `index.js:5246`, `index.js:8246` | `v2.js:2763`, `v2.js:2788` | no |
| GET `/api/v2/network/scan` | `index.js:9508` | none | **yes** |
| GET `/api/v2/sensors` | `index.js:4262` | `v2.js:2268`, `v2.js:3807` | no |
| GET/POST `/api/v2/sensors/labels` | `index.js:171`, `index.js:4268`, `index.js:9392-9409` | none | **yes** (unless row 13 is ported) |
| GET `/api/v2/otgw/otmonitor` | `index.js:5890` (2 s poll) | `v2.js:2326` (seed, then every 20 s only while WS is down) | no |
| POST `/api/v2/otgw/commands` | `index.js:3071` | `v2.js:3001` | no |
| POST `/api/v2/otgw/command/{cmd}` (compat alias) | `index.js:1042` (debug helper), `sat.js:1144` (DHW `SW=`) | none | **yes** |
| GET `/api/v2/otgw/messages/3`, `/48` | `sat.js:1155`, `sat.js:1176` | none | **yes** (unless row 58 bounds are ported) |
| GET `/api/v2/otgw/ot-support` | `index.js:8993` | none | **yes** (unless row 27 is ported) |
| GET `/api/v2/otgw/overrides` | `index.js:9047` | `v2.js:1409`, `v2.js:2501` | no |
| GET `/api/v2/otgw/boiler-support` | `index.js:9093` | `v2.js:2512` | no |
| GET `/api/v2/otdirect/status` | `index.js:2208` | none | **yes** (unless row 14 is ported) |
| GET/POST `/api/v2/otdirect/overrides` | `index.js:2286`, `:2344`, `:2355` | `v2.js:1293`, `v2.js:1322` | no |
| GET `/api/v2/sat/status` | `index.js:3797`, `sat.js:160` | `v2.js:2341`, `v2.js:3212` | no |
| POST `/api/v2/sat/{target,preset,mode,enable}` | `sat.js:834-878` | `v2.js:3206` ff, `v2.js:2383` | no |
| POST `/api/v2/sat/settings/dhw_enable` | `sat.js:1238` | `v2.js:3647` | no |
| GET `/api/v2/sat/weather` | `sat.js:900`, `:988`, `:1077` | `v2.js:2357`, `v2.js:3245` | no |
| GET `/api/v2/sat/weather/needs-setup` | `sat.js:1040` | none | **yes** |
| GET `/api/v2/sat/markers` | `sat.js:1290` | `v2.js:3252` | no |
| POST `/api/v2/sat/markers`, DELETE `/api/v2/sat/markers/{id}` | `sat.js:1399`, `sat.js:1386` | none | **yes** (unless row 57 is ported) |
| GET/PATCH `/api/v2/sat/sensor-areas` | `index.js:4274`, `index.js:4376` | none (v2 uses `satsensorarea0..3` settings) | **yes** |
| GET `/api/v2/sat/ble/discovery` | `index.js:4282`, `index.js:4391` | `v2.js:2040`, `v2.js:3779` | no |
| POST `/api/v2/sat/ble/{select,label,forget}` | `index.js:4594-4623` | `v2.js:2096-2109` | no |
| POST `/api/v2/sat/ble/rescan` | `index.js:4658` | `v2.js:2133` | no |
| POST `/api/v2/webhook/test` | `index.js:7104` (on and off) | `v2.js:1953` (on only) | no |
| GET `/FSexplorer` | `index.js:3486` | `v2.html:826` | no |
| GET `/update` | `index.js:3692`, `sat.js:109` | `v2.js:2962` | no |
| GET `/ResetWireless` | `index.js:9443` | `v2.js:2974` | no |

v2-only features, for completeness: a Reboot button (`v2.js:2966`; classic has none, V: no `/ReBoot` call in `index.js`). v2-only calls: `/api/v2/simulate`, `/api/v2/debug`, `/api/v2/filesystem/files`, `/upload`, `/ReBoot`, `/api/v2/otgw/diagnose`, `/api/v2/sat/target`, `/api/v2/sat/externaltemp`, `/api/v2/sat/ble/bindkey`, `/pic?action=delete`.

---

## 4. OT-Direct status on load (TASK-1219)

Code-level answer, **unverified on hardware**.

- The TASK-1219 bug (poll never starts because `otDirectAvailable` is still false when `showMainPage()` runs, `index.js:3724` vs `index.js:2196-2198`) does not exist in v2. v2 has no such gate: `fetchConn()` runs 400 ms after load and every 15 s (`v2.js:4411-4412`) (V).
- But v2 also has **no OT-Direct status panel**. It never calls `GET /api/v2/otdirect/status` (V: grep). On an OT-Direct board v2 shows:
  - OT bus link up/down from `health.otgwconnected` (`v2.js:1236-1244`);
  - the mode chip as `n/a`, because `otgwmode` is absent there (`v2.js:1262`);
  - the OT-Direct overrides editor in Monitor > Connection (`v2.js:1256-1260`).
- Not shown anywhere in v2: OT-Direct mode (gateway/monitor/bypass/master/loopback), thermostat presence, boiler active, setback, step-up, schedule active/total, override count. All of these are already in `device/info` (`restAPI.ino:3184-3204`) and in `otdirect/status`, but `renderDebugGroups` does not list them (`v2.js:2582-2600`) (V).

**Conclusion.** TASK-1219 can be closed as superseded only if row 14 is ported to v2 or deliberately dropped. Removing the classic UI without a port removes OT-Direct status visibility from the web UI.

---

## 5. Firmware changes required before the classic files are deleted

These are blockers or gates, not cleanups.

| Location | What happens if the classic files are deleted unchanged | Tag |
|---|---|---|
| `FSexplorer.ino:184-197` | `if (!LittleFS.exists("/index.html"))` registers `sendFSexplorerFallback` on `/`, `/index`, `/index.html`. With index.html gone, the root serves FSexplorer, not v2. Must key on `/v2.html` and serve v2 for all three paths (keeps bookmarks working). | V |
| `FSexplorer.ino:594` | Directory requests ending in `/` default to `index.html`. | V |
| `updateServerHtml.h:179`, `:318`, `:499`, `:530`, `:539` | After an OTA the page redirects to `/`. Fine once the root serves v2; broken if the line above is not fixed. | V |
| `evaluate.py:2377-2395` (`check_poll_window_coupling`) | Reads `data/index.js` for `OTMONITOR_POLL_MS` / `DEVTIME_POLL_MS`. Without the file it degrades to WARN and stops guarding ADR-172/173. | V |
| `restAPI.ino:2609-2651` (ADR-172 windows) | The 1500 ms / 4000 ms windows are documented as 75% of classic's 2 s and 5 s polls. v2 does not poll `device/time` and polls `otmonitor` only as a WS fallback every 20 s. The limiter still protects against Telegraf and scripts, so keep it, but its sizing rationale and the coupling gate need a new anchor (v2 constants or a fixed budget). | V |
| Tests referencing classic files | `tests/test_static_integrity.py`, `tests/test_webui_asset_versioning.py`, `tests/webui/*.test.mjs` (paced-poller, sat-area-sensor-mapping, sat-log-filter, sat-settings-layout, sat-status-pill, tab-restore), `tests/webui_label_coverage.spec.js`, `tests/webui_settings_layout.spec.js`, `scripts/tests/{static_integrity,refresh_storm,webui_burst,test_webserver,test_ws_liveload}.py`, `scripts/capture-mqtt-debug.{bat,sh}` | I (file-level grep only, not read) |

---

## 6. Firmware and asset code that only exists for the classic UI

All V unless marked.

| Item | Location | Note |
|---|---|---|
| `sendIndex` v2/classic branch | `FSexplorer.ino:150-165` (branch at 160-164) | Serve v2.html unconditionally. |
| `settings.ui.bUseV2` / `ui_usev2` | `UItypes.h:27`, `restAPI.ino:3853`, `restAPI.ino:4225`, `settingStuff.ino:290`, `settingStuff.ino:848` | Plus the v2 side: `v2.html:131`, `v2.js:202-223`, `v2.js:1709-1711`. |
| Seven `ui_*` preference keys with no reader in v2 | `UItypes.h:21-30` (`bAutoScroll`, `bShowTimestamp`, `bCaptureMode`, `bAutoScreenshot`, `bAutoDownloadLog`, `bAutoExport`, `iGraphTimeWindow`), `restAPI.ino:3847-3852`, `restAPI.ino:3856`, `restAPI.ino:4224-4225`, `settingStuff.ino:284-289`, `settingStuff.ino:293`, `settingStuff.ino:842-853` | Keep `bOnboarded` and `bSatOnboarded` (v2 reads them, `v2.js:3849`, `v2.js:4006`). |
| Classic asset routes | `FSexplorer.ino:202-207` (`/index.js`, `/graph.js`, `/sat.js`, `/sat-slider.js`, `/echarts-theme.js`, `/theme-toggle.js`) | Keep `/components.css` and `/ds-tokens.css` (`FSexplorer.ino:208-209`): `FSexplorer.html:17`, `:33` loads both. |
| `/v2.css` route | `FSexplorer.ino:213` | Already dead: `build.py:774` excludes `v2.css` from the image. The comment at `FSexplorer.ino:214-217` is stale. |
| Unified flash status endpoint | `restAPI.ino:3680-3698` (`sendFlashStatus`), route `restAPI.ino:509-516` | Only classic calls it; documented, so keep, but it is UI-dead. |
| WebSocket flash-progress JSON | `OTGW-Core.ino:5825-5830`, `OTGW-Core.ino:5885-5889` | Only consumer is `index.js:8280`. Each broadcast is a per-client heap allocation (`OTGW-Core.ino:5857` comment). |
| WebSocket `{"type":"status"}` events | `OTDirect.ino:895`, `:1408`, `:1497`, `:2512-2604`; `SATcontrol.ino:2000`; `SATcycles.ino:731`, `:772`, `:1270` | v2 drops all JSON frames (`v2.js:496`). Classic stores them but shows a near-empty line (I). No UI consumer either way. |
| Protected-file lists naming classic files | `FSexplorer.html:225-235`, `v2.js:2803-2805` | Cosmetic. |
| Data files used only by classic | `update.png`, `system_update.png` (only `index.js`) | Free LittleFS bytes for the 768K budget. |
| Data files with no consumer today | `download-to-storage-drive.png`, `refresh-page-option.png`, `transfer.dat` | No reference in `src/` (I: grep of html/js/ino/h/cpp only). |
| `design.html` | `data/design.html` | Needs `theme-toggle.js`, `sat-slider.js`, `components.css`, `ds-tokens.css`. Delete with the classic set. |
| Stale comments | `restAPI.ino:2609-2613` (names `data/index.js` poll constants), `build.py:730` | Update with the removal. |

---

## 7. Side findings (outside the gap question)

1. **v2 1-Wire discovery toast never fires.** `pollDallasDiscovery` reads `j.sensors.dallas[]` (`v2.js:3810`), but `GET /api/v2/sensors` emits `sensors.devices{<addr>:{temp,epoch}}` (`restAPI.ino:449-458`). The early return at `v2.js:3810` therefore always triggers. V in code, not run.
2. **Classic BLE name filter save fails.** See row 51: `satblenameprefix` / `satblenamefilteringest` are parsed by `settingStuff.ino:1135-1136` but missing from `knownSettings` (`restAPI.ino:4200`), so `postSettings` returns 400 at `restAPI.ino:4258`. V in code, not run.
3. **Classic charts need internet.** ECharts comes from `cdn.jsdelivr.net` (`index.html:27`). On a LAN without internet the classic graph and SAT charts do not render; v2 has no external script dependency. This supports dropping, not porting, the ECharts implementation.

---

## 8. What was not checked

- Nothing was run in a browser or on a device. Every "covered" verdict means the control and its API call exist in v2 code, not that they behave identically.
- `graph.js` series logic was read at the definition level (`graph.js:25-340`), not traced per update path.
- Test and script files in section 5 were matched by filename grep only.
- `.wolf/` and graphify updates were skipped because this task allowed exactly one new file.
