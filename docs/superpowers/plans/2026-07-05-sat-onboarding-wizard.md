# SAT Onboarding Wizard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fire a guided SAT setup wizard when a user enables SAT in the v2 web UI, mirroring the Home Assistant SAT config-flow questions, writing only existing firmware settings plus one new persist flag.

**Architecture:** Client-side wizard in `v2.js`, a second instance of the proven TASK-997 `startOnboarding` overlay engine. Six screens, no sensor-source selection. One new firmware bool `sat_onboarded` (mirrors `ui_onboarded`, opposite migration default). Three client triggers funnel into one `startSatOnboarding()`.

**Tech Stack:** Arduino C/C++ (ESP32-S3 async firmware, single translation unit), vanilla browser JS (no framework, no build step for `v2.js`), LittleFS-served assets, REST `/api/v2/settings`.

**Design spec:** `docs/superpowers/specs/2026-07-05-sat-onboarding-design.md` (TASK-1012).

## Global Constraints

- **No unit-test harness exists** for `v2.js` or the firmware UI path. Verification = `python build.py --target esp32` (exit 0), `python evaluate.py --quick` (no new FAIL), and on-device walk (Playwright via the `otgw-device-verify` agent, light + dark, 360px). "Test" steps below are these gates, not pytest.
- **PROGMEM:** all string literals in firmware use `F()` / `PSTR()`; `addBool`/`writeJsonBoolKV` already handle this.
- **No String class in hot paths, no ArduinoJson** — not relevant here (flag is a bool; JSON is the existing manual `addBool`).
- **No new firmware settings** except `sat_onboarded`. Every wizard answer targets an existing key: `satsystem`, `satsource`, `satmanufacturer`, `satautogains`, `satcoefficient`, `satenabled`.
- **Version bump required:** any staged change under `src/OTGW-firmware/**` (incl. `data/v2.js`) must bump `_VERSION_PRERELEASE`. Use `bin/bump-prerelease.sh` then commit in one shot (do NOT hand-stage version.h alone).
- **Trigger safety:** on the `enable` path SAT is written `true` only at the wizard's Finish, never mid-flow. Abandoning the `enable` wizard leaves `satenabled=false`.
- **No source selection in the wizard:** never render a BLE / DS18B20 / thermostat / weather source picker. Sources are health-surfaced read-only only.
- **Tuning has no raw P/I/D:** Manual tuning = `satcoefficient` (curve slope) + `satautogains` only.
- **Do NOT implement from this plan yet.** Per maintainer: plan is pushed to remote for the design tool to pick up the wizard UI first.

---

### Task 1: Firmware `sat_onboarded` persist flag

Mirror `ui_onboarded` (TASK-997) at its four touch points, plus the POST whitelist. Migration default is `false` for every install (opposite of `ui_onboarded`), so existing SAT users get the wizard once.

**Files:**
- Modify: `src/OTGW-firmware/UItypes.h:28` (struct field)
- Modify: `src/OTGW-firmware/settingStuff.ino:290` (serialize) and `:846` (parse)
- Modify: `src/OTGW-firmware/restAPI.ino:3497` (settings dump) and `:3866` (POST whitelist)

**Interfaces:**
- Produces: settings key `sat_onboarded` (bool, default `false`), readable via `GET /api/v2/settings` and writable via `POST /api/v2/settings {"name":"sat_onboarded","value":true}`. Backing field `settings.ui.bSatOnboarded`.

- [ ] **Step 1: Add the struct field**

In `src/OTGW-firmware/UItypes.h`, directly under line 28 (`bool bOnboarded ...`):

```cpp
  bool bSatOnboarded    = false;   // TASK-1012: SAT onboarding wizard shown once; set true on finish (enable path) or dismiss (migrate path). Re-runnable.
```

- [ ] **Step 2: Serialize it in writeSettings()**

In `src/OTGW-firmware/settingStuff.ino`, immediately after line 290 (`writeJsonBoolKV(file, F("ui_onboarded"), settings.ui.bOnboarded, true);`):

```cpp
  writeJsonBoolKV(file, F("sat_onboarded"), settings.ui.bSatOnboarded, true);
```

- [ ] **Step 3: Parse it in the settings setter**

In `src/OTGW-firmware/settingStuff.ino`, immediately after the `ui_onboarded` parse case at line 846:

```cpp
  else if (strcasecmp_P(field, PSTR("sat_onboarded"))==0)   { settings.ui.bSatOnboarded = EVALBOOLEAN(newValue); }
```

Note: NO `g_sawKey`-style presence detection — unlike `ui_onboarded`, we deliberately default `false` on migration so existing SAT users are prompted once.

- [ ] **Step 4: Expose it in the settings dump**

In `src/OTGW-firmware/restAPI.ino`, immediately after line 3497 (`addBool(F("ui_onboarded"), settings.ui.bOnboarded, "b");`):

```cpp
  addBool(F("sat_onboarded"), settings.ui.bSatOnboarded, "b");
```

- [ ] **Step 5: Add to the POST whitelist**

In `src/OTGW-firmware/restAPI.ino` line 3866, the writable-keys array currently contains `"ui_onboarded"`. Add `"sat_onboarded"` in alphabetical position (it sorts before the `sat*` block — place it just before `"satautogains"`/first `sat` entry, or wherever the array's sort order dictates). Match the existing quoting and trailing comma style exactly.

- [ ] **Step 6: Build**

Run: `python build.py --target esp32`
Expected: exit 0, firmware + filesystem built. Confirm the per-env SUCCESS line (build.py can exit 0 even on a per-env failure — grep the log for `esp32 ... SUCCESS`).

- [ ] **Step 7: Evaluate**

Run: `python evaluate.py --quick`
Expected: exit 0 (or WARN only, no new FAIL).

- [ ] **Step 8: On-device verify (via otgw-device-verify agent)**

Flash the bench ESP32-S3, then:
- `GET /api/v2/settings` → `sat_onboarded` present, value `false`.
- `POST /api/v2/settings {"name":"sat_onboarded","value":true}` → 200; re-GET shows `true`; reboot; re-GET still `true` (persisted to LittleFS).
- `POST ... {"value":false}` → resets to `false`.

- [ ] **Step 9: Bump + commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/UItypes.h src/OTGW-firmware/settingStuff.ino src/OTGW-firmware/restAPI.ino
git commit -m "feat(sat): add sat_onboarded persist flag (mirrors ui_onboarded)"
```

(`bump-prerelease.sh` stages the version banners itself; the `git add` above stages the three source edits.)

---

### Task 2: SAT wizard engine `startSatOnboarding()` in v2.js

Clone the `startOnboarding` pattern (v2.js:3388) into a new `startSatOnboarding()`. Six screens, no source pickers. This is the baseline implementation; the design tool may refine the visual markup, but the state machine, key-writes, and commit semantics below are final.

**Files:**
- Modify: `src/OTGW-firmware/data/v2.js` (add `startSatOnboarding()` + `maybeShowSatOnboarding()` near the existing `startOnboarding` at 3388)

**Interfaces:**
- Consumes: `setData` (global settings map, each `{value}`), `APIGW` (API base), `fetchSettings()`, `showPage()`, `bleToast()`.
- Produces: `startSatOnboarding()` (opens overlay), `maybeShowSatOnboarding(reason)` where `reason ∈ {'enable','migrate'}` (gated launcher). Both used by Task 3.

- [ ] **Step 1: Add the gated launcher and engine scaffold**

Insert after `startOnboarding`'s closing brace (after v2.js:3524). Scaffold mirrors the existing engine (backdrop `#satOnbBack`, `CARD`/`PRIM`/`GHOST`/`IN`/`LBL` style consts copied verbatim from `startOnboarding`, `post()`, `close()`, `render()`, click delegation):

```js
  var _satOnbShown = false;
  // reason: 'enable' (satenabled just flipped on) or 'migrate' (existing SAT user, first SAT-page visit)
  function maybeShowSatOnboarding(reason) {
    if (_satOnbShown || document.getElementById('satOnbBack')) return;
    var ob = setData.sat_onboarded;
    if (ob && ('' + ob.value) === 'true') return;   // already onboarded
    _satOnbShown = true; startSatOnboarding(reason);
  }
  function startSatOnboarding(reason) {
    var migrate = (reason === 'migrate');
    var st = {
      screen: 'welcome',
      system: (setData.satsystem && ('' + setData.satsystem.value)) || '0',   // 0 auto/1 rad/2 UFH
      source: (setData.satsource && ('' + setData.satsource.value)) || '1',   // 1 gas/2 HP/3 hybrid (pre-filled from TASK-997)
      mfr:    (setData.satmanufacturer && ('' + setData.satmanufacturer.value)) || '0',
      tuning: 'auto',
      coeff:  (setData.satcoefficient && setData.satcoefficient.value) || '1.4',
      gains:  (setData.satautogains && setData.satautogains.value) || '1.0'
    };
    var back = document.createElement('div'); back.id = 'satOnbBack';
    back.setAttribute('style', 'position:fixed;inset:0;z-index:200;background:var(--bg);display:flex;flex-direction:column;overflow:auto');
    document.body.appendChild(back);
    // ---- style consts: copy CARD, PRIM, GHOST, IN, LBL verbatim from startOnboarding (v2.js:3402-3406) ----
    function esc(s){ return (''+(s==null?'':s)).replace(/"/g,'&quot;').replace(/</g,'&lt;'); }
    function post(name,value){ return fetch(APIGW+'v2/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:name,value:value})}); }
    function close(){ if(back.parentNode) back.parentNode.removeChild(back); }
```

- [ ] **Step 2: Add the commit function (final key-write semantics)**

```js
    function commit(then) {
      var w = [
        post('satsystem', st.system),
        post('satsource', st.source),
        post('satmanufacturer', st.mfr)
      ];
      if (st.tuning === 'manual') {
        w.push(post('satcoefficient', st.coeff), post('satautogains', st.gains));
      } else {
        w.push(post('satautogains', '1.0'));   // Automatic: reset gains multiplier, leave coefficient default
      }
      w.push(post('satenabled', true));         // SAT enabled ONLY here, at Finish
      Promise.all(w).then(function(){ return post('sat_onboarded', true); }).then(then, then);
    }
    // migrate path: dismissing without finishing still marks onboarded (SAT already runs)
    function dismiss() {
      if (migrate) { post('sat_onboarded', true).then(function(){}, function(){}); }
      close();
    }
```

- [ ] **Step 3: Add the render() screen switch**

Render six screens. Header + progress bar copied from `startOnboarding` (v2.js:3452-3461) with title "SAT setup" and 5 numbered steps (welcome is unnumbered). Screens:

```js
    function choiceCard(field, val, cur, title, desc) {   // reuse startOnboarding's choiceCard shape
      var sel = (''+cur) === (''+val);
      return '<button data-act="pick-'+field+'-'+val+'" style="flex:1;min-width:200px;text-align:left;border-radius:12px;padding:16px;cursor:pointer;font-family:inherit;'+
        (sel?'background:color-mix(in srgb,var(--accent) 12%,var(--panel));border:2px solid var(--accent)':'background:var(--bg);border:1.5px solid var(--border)')+'">'+
        '<div style="font-weight:700;font-size:15px;color:var(--text)">'+title+'</div>'+
        '<p style="font:400 11.5px/1.5 inherit;color:var(--muted);margin:9px 0 0">'+desc+'</p></button>';
    }
    function render() {
      var h = '';
      // header + progress bar here (clone v2.js:3452-3461, label "SAT setup")
      if (st.screen === 'welcome') {
        // "Let's tune SAT for your system." Note SAT stays disabled until Finish. Button data-act="begin".
      } else if (st.screen === 'system') {
        // three choiceCards. Radiators -> satsystem=1,satsource=1 ; Underfloor -> satsystem=2,satsource=1 ; Heat pump -> satsystem=0,satsource=2
        // Continue -> data-act="to-mfr". Back -> data-act="back-welcome".
      } else if (st.screen === 'mfr') {
        // <select id="satOnbMfr"> populated from the satmanufacturer enum (v2.js:2094). Default st.mfr.
        // Continue -> data-act="to-tuning". Back -> data-act="back-system".
      } else if (st.screen === 'tuning') {
        // two choiceCards: Automatic (recommended) vs Manual. If Manual: reveal
        //   coefficient slider (satcoefficient 0.1-5.0) id="satOnbCoeff" and gains input (satautogains 0.1-10) id="satOnbGains".
        // Continue -> data-act="to-health". Back -> data-act="back-mfr".
      } else if (st.screen === 'health') {
        // read-only checklist (Step 4 below fills it). Continue -> data-act="to-done". Back -> data-act="back-tuning".
      } else if (st.screen === 'done') {
        // summary card (system, tuning, health snapshot). Button data-act="go".
      }
      back.innerHTML = h;
    }
```

Provide full markup for each screen by copying the corresponding `startOnboarding` screen block (welcome ← 3463-3468, choice screen ← 3469-3474, select/inputs ← 3480-3492, done ← 3496-3504) and substituting the SAT copy and `data-act` names above. No new CSS classes — reuse the inline style consts.

- [ ] **Step 4: Add the Sources-health screen data wiring**

On entering the `health` screen, fetch health + sat status and render a green/amber checklist. No inputs.

```js
    function loadHealth() {
      Promise.all([
        fetch(APIGW+'v2/health').then(function(r){return r.ok?r.json():{};}).catch(function(){return {};}),
        fetch(APIGW+'v2/sat/status').then(function(r){return r.ok?r.json():{};}).catch(function(){return {};})
      ]).then(function(res){
        var hlt = res[0]||{}, sat = res[1]||{};
        // rows: OT bus online (hlt.otgwconnected), Room temp (sat.roomtemp != null),
        //       Outside temp (sat.outsidetemp != null), Flow temp, Return temp (if present),
        //       MQTT connected (hlt.mqttconnected). Green check / amber warn per row.
        // When a critical row (room temp) is amber, show a line linking to the Sensors settings page.
        var el = back.querySelector('#satOnbHealth'); if (el) el.innerHTML = /* rows */ '';
      });
    }
```

Call `loadHealth()` from the click handler when transitioning to the `health` screen.

- [ ] **Step 5: Add the click delegation**

Clone `startOnboarding`'s `back.addEventListener('click', ...)` (v2.js:3509-3522), wiring: `begin`→system; `pick-satsystem-*` / `pick-satsource-*` set st and re-render; `to-mfr`/`back-*` navigation; `pick-tuning-auto|manual` set `st.tuning`; `to-health` reads the manual inputs into `st.coeff`/`st.gains` then `loadHealth()`; `to-done`; `go`→`commit(function(){ close(); fetchSettings(); showPage('sat'); })`. The overlay's implicit dismiss (Back from welcome / an explicit Close affordance) calls `dismiss()`.

- [ ] **Step 6: Build + evaluate**

Run: `python build.py --target esp32` → exit 0 (rebuilds LittleFS with the new v2.js).
Run: `python evaluate.py --quick` → no new FAIL.

- [ ] **Step 7: Bump + commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/data/v2.js
git commit -m "feat(sat): SAT onboarding wizard engine (6 screens, no source pickers)"
```

---

### Task 3: Three triggers + two re-run buttons

Wire the launcher into the three entry points and add the manual re-run affordances.

**Files:**
- Modify: `src/OTGW-firmware/data/v2.js` — `saveSettings()` (1824), `satPageStart()` (2801), the settings `data-act` click handler (near the TASK-997 `action === 'onboarding'` case, v2.js:2714), and `renderSettings()` (1645) for the Advanced>System button.
- Modify: `src/OTGW-firmware/data/v2.html` — SAT-page re-run button (if not injected by JS).

**Interfaces:**
- Consumes: `maybeShowSatOnboarding`, `startSatOnboarding` from Task 2.

- [ ] **Step 1: Enable trigger in saveSettings()**

In `saveSettings()` (v2.js:1824), capture the pre-save value and detect the false→on transition after the save chain resolves. Modify the function:

```js
  function saveSettings() {
    var keys = Object.keys(setDirty);
    if (!keys.length) return;
    // TASK-1012: detect satenabled false->on to launch the SAT wizard after save
    var satEnabling = ('satenabled' in setDirty) &&
      ('' + setDirty.satenabled) === 'true' &&
      !(setData.satenabled && ('' + setData.satenabled.value) === 'true');
    var chain = Promise.resolve();
    // ... existing per-key POST chain unchanged ...
    chain.then(function () {
      setDirty = {};
      /* existing toast */
      renderSettings();
      if (satEnabling) maybeShowSatOnboarding('enable');   // sat_onboarded gate inside
    }).catch(/* existing */);
  }
```

- [ ] **Step 2: Migrate trigger in satPageStart()**

In `satPageStart()` (v2.js:2801), on first invocation, if SAT is enabled and not yet onboarded, launch with reason `'migrate'`:

```js
  function satPageStart() {
    // TASK-1012: existing SAT users see the wizard once on first SAT-page visit
    if (setData.satenabled && ('' + setData.satenabled.value) === 'true') {
      maybeShowSatOnboarding('migrate');
    }
    // ... existing satPageStart body ...
  }
```

`maybeShowSatOnboarding` self-gates on `sat_onboarded` and the `_satOnbShown` latch, so this is safe to call every SAT-page entry.

- [ ] **Step 3: Manual re-run — SAT page button**

Add a "Re-run SAT setup" button to the SAT page header. In the SAT-page render (or `v2.html` SAT section), add `<button data-act="sat-onboarding">Re-run SAT setup</button>`. In the SAT-page/global `data-act` click handler, add:

```js
    if (act === 'sat-onboarding') { _satOnbShown = false; startSatOnboarding('rerun'); return; }
```

(`'rerun'` behaves like a fresh run but SAT is already on; `commit` re-writes keys and sets `sat_onboarded=true`.)

- [ ] **Step 4: Manual re-run — Advanced > System button**

In `renderSettings()` (v2.js:1645), in the Advanced/System group next to TASK-997's re-run (`action === 'onboarding'`, handler at v2.js:2714), add a sibling button `data-act="sat-onboarding"`. The handler from Step 3 covers it.

- [ ] **Step 5: Build + evaluate**

Run: `python build.py --target esp32` → exit 0.
Run: `python evaluate.py --quick` → no new FAIL.

- [ ] **Step 6: Bump + commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/data/v2.js src/OTGW-firmware/data/v2.html
git commit -m "feat(sat): wire SAT onboarding triggers (enable/migrate/re-run) + buttons"
```

---

### Task 4: On-device end-to-end verification

**Files:** none (verification only).

- [ ] **Step 1: Fresh-enable walk (via otgw-device-verify agent)**

On a device with `sat_onboarded=false`, `satenabled=false`: toggle SAT on + Save → wizard opens at Welcome. Walk all 6 screens. Finish → confirm `GET /api/v2/settings` shows `satenabled=true`, `sat_onboarded=true`, and `satsystem/satsource/satmanufacturer/satautogains` match the choices. Reload page → wizard does NOT reappear.

- [ ] **Step 2: Abandon walk**

Reset `sat_onboarded=false`, `satenabled=false`. Enable SAT + Save → wizard opens → close it without finishing → confirm `satenabled=false` (SAT NOT enabled) and `sat_onboarded=false` (wizard reappears next attempt).

- [ ] **Step 3: Migrate walk**

Set `satenabled=true`, `sat_onboarded=false` directly via POST. Navigate to SAT page → wizard opens once (`migrate`). Dismiss → confirm `sat_onboarded=true` and `satenabled` still `true` (SAT keeps running). Re-visit SAT page → no wizard.

- [ ] **Step 4: Theme + width**

Repeat Step 1 in dark theme and at 360px width. Confirm no horizontal overflow, 44px tap targets, overlay legible in both themes.

- [ ] **Step 5: Mark TASK-1012 ACs**

```bash
backlog task edit 1012 --check-ac 1 --check-ac 2 --check-ac 3 --check-ac 4 --check-ac 5 --check-ac 6 --check-ac 7
```

(Check each AC as its verification passes; do not batch-check unverified ACs.)

---

## Self-Review

**Spec coverage:**
- Trigger (enable/migrate/re-run) → Task 3 ✓
- 6 screens, no source pickers → Task 2 ✓
- Every answer writes existing key + new `sat_onboarded` bool → Task 1 + Task 2 `commit()` ✓
- Migration once for existing users → Task 1 default `false` + Task 3 Step 2 ✓
- Re-run in both places → Task 3 Steps 3-4 ✓
- Sources health read-only → Task 2 Step 4 ✓
- Build/eval/on-device gates → every task's verify steps + Task 4 ✓
- Deferred control-ladder → out of scope, separate task (spec) ✓

**Type consistency:** `maybeShowSatOnboarding(reason)` / `startSatOnboarding(reason)` signatures match across Tasks 2-3. `commit()` writes exactly the keys named in Task 1's whitelist. `st.system`/`st.source`/`st.mfr`/`st.coeff`/`st.gains`/`st.tuning` used consistently in Task 2. Setting key `sat_onboarded` spelled identically in firmware (Task 1) and JS (Tasks 2-3).

**Placeholder note:** Task 2 Steps 3-4 intentionally reference "clone screen block from `startOnboarding` line N" rather than re-pasting ~60 lines of near-identical inline markup — the source lines are cited exactly and the copy/`data-act`/key substitutions are fully specified. The visual markup is deliberately left as the design tool's refinement surface (maintainer directive: this plan is picked up by the design tool before implementation), while the state machine, key-writes, and commit semantics are final and complete.
