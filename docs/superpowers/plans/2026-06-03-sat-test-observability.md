# SAT Test Observability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give a human a readable, transition-only account of what the SAT controller / simulator is doing, on both the Telnet log and the Web UI live-log, from a single narration source.

**Architecture:** One narration helper (`satNarrate`) formats each line once and fans it to two sinks — `DebugTln` (Telnet) and `sendEventToWebSocket('S', ...)` (Web UI live-log). Narration fires only on state transitions. The Web UI gets a client-side "SAT only" filter that reuses the existing log-filter pipeline.

**Tech Stack:** Arduino C/C++ (single translation unit, ESP8266/ESP32-S3), vanilla JS Web UI (LittleFS assets), Playwright + system Chrome for the UI test, `python build.py` + `python evaluate.py` for firmware gates.

**Spec:** `docs/superpowers/specs/2026-06-03-sat-test-observability-design.md` · **Backlog:** TASK-815 · **Branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support` (2.0.0 only).

**Conventions that constrain this work:**
- No `String` in SAT/MQTT/REST hot paths (ADR-004) — `char[]` + `snprintf_P`.
- All string literals in `PSTR`/`F` (PROGMEM).
- No raw `#ifdef ESP8266/ESP32` outside the abstraction layer — not needed here (helper is platform-neutral).
- Firmware has no unit-test harness (single Arduino TU); firmware verification = `python build.py` clean on both envs + `python evaluate.py --quick` green + manual simulation observation. The Web UI filter logic gets a real Playwright test.
- Every firmware/data change needs a prerelease bump (`bin/bump-prerelease.sh`) before commit.
- `sendEventToWebSocket(char prefix, const char* msg, int len=-1)` is `static` in `OTGW-Core.ino` but visible across the whole TU (all `.ino` concatenate into one `.cpp`), so any `.ino` may call it.
- `SATDebugTln`/`SATDebugTf` are gated by `state.debug.bSAT`. Narration must be **ungated** (always-on), so it uses `DebugTln`/`Debugf` directly, NOT the `SATDebug*` macros.

---

## File Structure

- `src/OTGW-firmware/SATcontrol.ino` — define `satNarrate_P` / `satNarratef_P`; add narration at sim-start, window open/close, control-mode change, safety trip, scenario inject, real-boiler-detected.
- `src/OTGW-firmware/SATcycles.ino` — add narration at the flame on/off + cycle-verdict chokepoint (`satCycleOnFlameChange`), covering both simulated and real boilers.
- `src/OTGW-firmware/OTGW-firmware.h` — prototypes for the two narration helpers so `SATcycles.ino` can call them.
- `src/OTGW-firmware/data/index.html` — "SAT only" toggle in the log toolbar.
- `src/OTGW-firmware/data/index.js` — `satOnlyFilter` flag + predicate in `updateFilteredBuffer()`; wire the toggle; persist with existing log prefs.
- `src/OTGW-firmware/data/components.css` — optional severity coloring for `S` lines.
- `tests/webui/sat-log-filter.test.mjs` (scratch-run, not shipped) — Playwright test for the "SAT only" filter.

---

## Task 1: Narration helper (dual-sink)

**Files:**
- Modify: `src/OTGW-firmware/OTGW-firmware.h` (add prototypes near other SAT prototypes, e.g. after the `satCycleOnFlameChange` declaration at line 339)
- Modify: `src/OTGW-firmware/SATcontrol.ino` (add helper after the `SATDebug*` macros at lines 20-22)

- [ ] **Step 1: Add prototypes to `OTGW-firmware.h`**

After line 339 (`void satCycleOnFlameChange(bool flameOn);`) add:

```cpp
// SAT test-observability narration (TASK-815): one source, two sinks
// (Telnet via DebugTln + Web UI live-log via sendEventToWebSocket('S',...)).
void satNarrate_P(PGM_P msg_P);
void satNarratef_P(PGM_P fmt_P, ...);
```

- [ ] **Step 2: Implement the helper in `SATcontrol.ino`**

Immediately after the `SATDebugf` macro (line 22) add:

```cpp
// --- SAT test-observability narration (TASK-815) ---
// Emits one plain-language line to BOTH sinks so Telnet and the Web UI live-log
// never drift. Always-on (NOT gated by state.debug.bSAT) but transition-only at
// the call sites, so volume stays low. Web UI lines carry prefix 'S'.
// sendEventToWebSocket() is a file-static in OTGW-Core.ino, visible across the TU.
#define SAT_NARRATE_BUF 96
void satNarrate_P(PGM_P msg_P) {
  if (!msg_P) return;
  char buf[SAT_NARRATE_BUF];
  // Copy the PROGMEM string into RAM once, then hand the same buffer to both sinks.
  snprintf_P(buf, sizeof(buf), PSTR("%s"), msg_P);
  DebugTf(PSTR("SAT: %s\r\n"), buf);          // Telnet sink
  sendEventToWebSocket('S', buf);             // Web UI live-log sink (prefix 'S')
}
void satNarratef_P(PGM_P fmt_P, ...) {
  if (!fmt_P) return;
  char buf[SAT_NARRATE_BUF];
  va_list args;
  va_start(args, fmt_P);
  vsnprintf_P(buf, sizeof(buf), fmt_P, args);
  va_end(args);
  DebugTf(PSTR("SAT: %s\r\n"), buf);          // Telnet sink
  sendEventToWebSocket('S', buf);             // Web UI live-log sink (prefix 'S')
}
```

- [ ] **Step 3: Build firmware to verify it compiles**

Run: `python build.py --firmware`
Expected: exit 0; grep the log for both `esp32 ... SUCCESS`/`.bin` and `esp8266 ... .bin` (build.py masks per-env failures — confirm both envs produced a `.ino.bin`).

- [ ] **Step 4: Run the evaluator**

Run: `python evaluate.py --quick`
Expected: `✗ Failed: 0`; no new PROGMEM / ADR-004 violations.

- [ ] **Step 5: Commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/OTGW-firmware.h src/OTGW-firmware/SATcontrol.ino src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
git commit -m "feat(sat): add dual-sink test-observability narration helper (TASK-815)"
```
(Note: `bump-prerelease.sh` touches ~42 files with version strings; stage `src/OTGW-firmware` wholesale only after confirming via `git diff --ignore-cr-at-eol --numstat -- src/OTGW-firmware | awk '$1>2||$2>2'` that only your two files + `version.h` have substantive changes.)

---

## Task 2: Flame + cycle narration (the chokepoint)

`satCycleOnFlameChange(bool flameOn)` (SATcycles.ino:507) is called for BOTH simulated (SATcontrol.ino:3413/3420) and real (SATcontrol.ino:4164) flame transitions, so narrating here covers both with no duplication. The cycle verdict is computed in the flame-OFF branch at SATcycles.ino:578 (`cls`).

**Files:**
- Modify: `src/OTGW-firmware/SATcycles.ino` (flame-on branch ~507-555; flame-off + classify branch ~556-604)

- [ ] **Step 1: Narrate flame ON**

In `satCycleOnFlameChange`, inside the `flameOn == true` branch (after the existing per-cycle reset around line 533, where `_cycle_flameOnStartMs` is set), add:

```cpp
  satNarratef_P(PSTR("Flame lit — flow %.0f°C, setpoint %.0f°C"),
                satGetFlowTemp(), state.sat.fFinalSetpoint);
```

- [ ] **Step 2: Narrate flame OFF + cycle verdict**

In the flame-OFF branch, immediately after line 580 (`_cycleRecord(cls, durationSec, _cycle_maxFlowTemp, _cycle_overshootSec);`) add:

```cpp
    // Map the cycle class enum to a plain word for the observer.
    static const char *const _satCycleWord[] = { "none", "GOOD", "overshoot",
                                                 "underheat", "short", "uncertain" };
    const char *verdict = ((uint8_t)cls < (sizeof(_satCycleWord)/sizeof(_satCycleWord[0])))
                          ? _satCycleWord[(uint8_t)cls] : "?";
    satNarratef_P(PSTR("Flame off — cycle %s, %0.0fs on, peak flow %.0f°C"),
                  verdict, durationSec, _cycle_maxFlowTemp);
```

> NOTE: confirm the `SATCycleClass` enum ordering matches `_satCycleWord` by reading the enum in `SATtypes.h` before building. The Web UI `CYCLE_LABELS` array (`sat.js`) is `['None','Good','Overshoot','Underheat','Short','Uncertain']` — mirror that order. If the firmware enum differs, reorder `_satCycleWord` to match the enum, not the JS.

- [ ] **Step 3: Build + evaluate**

Run: `python build.py --firmware` (expect exit 0, both env `.bin`) then `python evaluate.py --quick` (expect 0 failures).

- [ ] **Step 4: Commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/SATcycles.ino src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
git commit -m "feat(sat): narrate flame on/off and cycle verdict (TASK-815)"
```

---

## Task 3: Remaining SAT transition narration

**Files:**
- Modify: `src/OTGW-firmware/SATcontrol.ino` (sim start 3378; window 903/909; mode change ~1955; safety set 4234 & 4552; safety clear 1725; real-boiler ~1220; scenario branches 3604+)

- [ ] **Step 1: Simulation start** — at SATcontrol.ino:3378, replace the gated line
`SATDebugTln(F("SAT SIM: simulation started"));`
with (keep the debug line too, add narration after it):

```cpp
    SATDebugTln(F("SAT SIM: simulation started"));
    satNarrate_P(PSTR("Simulation started — synthetic boiler, no bus traffic"));
```

- [ ] **Step 2: Window open / close** — at SATcontrol.ino:903 (`state.sat.bWindowOpen = true;`) add directly after:

```cpp
    satNarrate_P(PSTR("Window open detected — heating paused"));
```
and at line 909 (`state.sat.bWindowOpen = false;`) add directly after:

```cpp
    satNarrate_P(PSTR("Window closed — heating resumes"));
```

- [ ] **Step 3: Control-mode change** — at SATcontrol.ino ~1955 (right after the `SATDebugTf("SAT: control mode ...")` line), add:

```cpp
    satNarratef_P(PSTR("Control mode → %s"),
                  (state.sat.eControlMode == SAT_MODE_PWM)        ? "PWM"
                  : (state.sat.eControlMode == SAT_MODE_CONTINUOUS) ? "Continuous"
                  :                                                   "Off");
```

- [ ] **Step 4: Safety trip set** — at SATcontrol.ino:4234 and 4552, directly after each `state.sat.bSafetyTripped = true;` add:

```cpp
      satNarrate_P(PSTR("SAFETY TRIPPED — SAT regulation halted"));
```

- [ ] **Step 5: Safety clear** — at SATcontrol.ino:1725, directly after `state.sat.bSafetyTripped = false;` add:

```cpp
    satNarrate_P(PSTR("Safety cleared — SAT may resume"));
```

- [ ] **Step 6: Real boiler detected** — in `satForceDisableSimulation` (~SATcontrol.ino:1204-1221), after the existing `sendEventToWebSocket('!', PSTR("SAT-SIM: boiler appeared, simulation off"));` add the narration prefix-'S' line so it shows under the SAT-only filter too:

```cpp
  satNarrate_P(PSTR("Real boiler detected on bus — simulation disabled"));
```

- [ ] **Step 7: Scenario injection** — in `satSimInjectEvent` (SATcontrol.ino:3598+), add one narration line per successful branch, immediately before each `return true;`. For the four primary events:

```cpp
  // window_open branch (after line 3611):
    satNarratef_P(PSTR("Scenario: window-open x%.1f for %lds"), m, (long)(durMs/1000UL));
  // solar_gain branch (after line 3622):
    satNarratef_P(PSTR("Scenario: solar gain %.2f°C/min for %lds"), g, (long)(durMs/1000UL));
  // sensor_noise branch (in the else, after line 3638):
    satNarratef_P(PSTR("Scenario: sensor noise +/-%.2f°C for %lds"), a, (long)(durMs/1000UL));
  // dhw_demand branch (after the expiry is set):
    satNarrate_P(PSTR("Scenario: DHW draw simulated"));
```

- [ ] **Step 8: Build + evaluate**

Run: `python build.py --firmware` (exit 0, both env `.bin`) then `python evaluate.py --quick` (0 failures).

- [ ] **Step 9: Commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/SATcontrol.ino src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
git commit -m "feat(sat): narrate sim start, window, mode, safety, scenarios (TASK-815)"
```

---

## Task 4: Web UI "SAT only" filter (test-first)

The existing log filter lives in `updateFilteredBuffer()` (index.js:2374). SAT lines arrive as `HH:MM:SS S <msg>` (timestamp + ` S ` prefix from `sendEventToWebSocket`). The toggle adds a predicate: when on, keep only lines whose formatted text matches `/^\d\d:\d\d:\d\d\s+S\s/`.

**Files:**
- Create: `tests/webui/sat-log-filter.test.mjs` (run from a scratch dir with playwright installed; not shipped in the LittleFS image)
- Modify: `src/OTGW-firmware/data/index.html` (log toolbar)
- Modify: `src/OTGW-firmware/data/index.js` (`satOnlyFilter` flag + predicate + wiring)
- Modify: `src/OTGW-firmware/data/components.css` (optional `S`-line color)

- [ ] **Step 1: Write the failing Playwright test**

Create `tests/webui/sat-log-filter.test.mjs` (mirrors the IP-octet harness: serves `data/` over http, drives system Chrome). It loads `index.html`, calls a small test seam that pushes known lines through the real filter, toggles "SAT only", and asserts only `S` lines remain.

```js
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path';
import { chromium } from 'playwright';
const DATA = process.env.OTGW_DATA_DIR, PORT = 8139;
const MIME = {'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json','.svg':'image/svg+xml','.ico':'image/x-icon','.png':'image/png'};
const srv = http.createServer((q,s)=>{let p=decodeURIComponent(new URL(q.url,`http://x:${PORT}`).pathname);if(p==='/')p='/index.html';const f=path.join(DATA,p);if(fs.existsSync(f)&&fs.statSync(f).isFile()){s.writeHead(200,{'content-type':MIME[path.extname(f)]||'application/octet-stream'});fs.createReadStream(f).pipe(s);}else{s.writeHead(404);s.end('nf');}});
await new Promise(r=>srv.listen(PORT,'127.0.0.1',r));
const b = await chromium.launch({channel:'chrome',headless:true});
const pg = await b.newPage();
await pg.route('**/api/**', r=>r.fulfill({status:200,contentType:'application/json',body:'{}'}));
await pg.goto(`http://127.0.0.1:${PORT}/index.html`,{waitUntil:'domcontentloaded'});
let ok=true; const log=(p,m)=>{console.log((p?'PASS':'FAIL')+': '+m); if(!p)ok=false;};
// Seam: seed the real buffer with mixed lines and run the real filter.
const res = await pg.evaluate(() => {
  otLogBuffer = [
    {data:'19:42:02 . Request Boiler R80000100 Status = Master'},
    {data:'19:42:05 S Flame lit — flow 22°C, setpoint 45°C'},
    {data:'19:42:06 B Boiler B40 RelModLevel = 78 %'},
    {data:'19:49:30 S Flame off — cycle GOOD, 410s on, peak flow 47°C'}
  ];
  window.satOnlyFilter = true;
  updateFilteredBuffer();
  return otLogFilteredBuffer.map(e=>e.data);
});
log(res.length===2, 'SAT-only keeps exactly the 2 S lines (got '+res.length+')');
log(res.every(l=>/^\d\d:\d\d:\d\d\s+S\s/.test(l)), 'all kept lines are S-prefixed');
await b.close(); srv.close();
console.log(ok?'RESULT: ALL PASS':'RESULT: FAILURES'); process.exitCode = ok?0:1;
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /c/Users/rvdbr/AppData/Local/Temp/otgw-iptest   # reuse the dir that already has playwright installed
OTGW_DATA_DIR="D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-2.0.0/src/OTGW-firmware/data" \
  node "D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware-2.0.0/tests/webui/sat-log-filter.test.mjs"
```
Expected: FAIL — `satOnlyFilter`/predicate not implemented, so `updateFilteredBuffer()` ignores it and returns 4 lines.

- [ ] **Step 3: Implement the filter flag + predicate in `index.js`**

Near `let searchTerm = '';` (index.js:928) add:

```javascript
let satOnlyFilter = false;   // "SAT only" toggle (TASK-815): keep only S-prefixed lines
const SAT_LINE_RE = /^\d\d:\d\d:\d\d\s+S\s/;   // "HH:MM:SS S ..." narration prefix
```

Replace `updateFilteredBuffer()` (index.js:2374-2383) with:

```javascript
function updateFilteredBuffer() {
  let base = otLogBuffer;
  if (satOnlyFilter) {
    base = base.filter(entry => SAT_LINE_RE.test(formatLogLine(entry.data)));
  }
  if (!searchTerm || searchTerm.trim() === '') {
    otLogFilteredBuffer = base;
  } else {
    const term = searchTerm.toLowerCase();
    otLogFilteredBuffer = base.filter(entry =>
      formatLogLine(entry.data).toLowerCase().includes(term)
    );
  }
}
```

(Exposing `satOnlyFilter` on `window` is automatic — top-level `let` in the page scope is reachable from `page.evaluate`. The test sets `window.satOnlyFilter`; mirror by reading the bare `satOnlyFilter` — they are the same binding at page scope. If the bundler scopes it, change the test to `satOnlyFilter = true` without `window.`.)

- [ ] **Step 4: Run the test to verify it passes**

Run the Step-2 command again. Expected: `PASS: SAT-only keeps exactly the 2 S lines` + `PASS: all kept lines are S-prefixed` + `RESULT: ALL PASS`.

- [ ] **Step 5: Add the toggle control to `index.html`**

In the OT-log toolbar (near the existing log search input — find `id="searchLog"` in index.html), add:

```html
<label class="ot-log-toggle"><input type="checkbox" id="satOnlyToggle"> SAT only</label>
```

- [ ] **Step 6: Wire the toggle in `index.js`**

Where the log-search listener is registered (near index.js:2681, the `searchTerm = e.target.value` handler), add a sibling listener:

```javascript
  const satToggle = document.getElementById('satOnlyToggle');
  if (satToggle) {
    satToggle.addEventListener('change', (e) => {
      satOnlyFilter = e.target.checked;
      updateFilteredBuffer();
      scheduleDisplayUpdate();
    });
  }
```

- [ ] **Step 7: Optional severity color for S lines (`components.css`)**

Append near the `.ot-log-content` block:

```css
/* TASK-815: SAT narration lines stand out in the mixed log stream. */
.ot-log-content .sat-line { color: var(--brand-cyan, #7ee0c0); }
```
(Only wire this if `renderLogDisplay` tags lines with a class; if it renders plain `textContent`, skip the CSS — the prefix + toggle are sufficient. Do NOT switch the log render to `innerHTML` just for color; the container is `white-space: pre` plain text by contract.)

- [ ] **Step 8: Commit**

```bash
bin/bump-prerelease.sh
git add src/OTGW-firmware/data/index.html src/OTGW-firmware/data/index.js src/OTGW-firmware/data/components.css src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
git commit -m "feat(webui): SAT-only filter toggle for the live-log (TASK-815)"
```

---

## Task 5: Full build, verify, ship

- [ ] **Step 1: Full build (firmware + filesystem)**

Run: `python build.py`
Expected: exit 0; confirm both `OTGW-firmware-esp32-...littlefs.bin` and `OTGW-firmware-esp8266-...littlefs.bin` are produced (do NOT pipe through `tail` — keep the full log to read per-env lines).

- [ ] **Step 2: Evaluator**

Run: `python evaluate.py --quick`
Expected: `✗ Failed: 0`.

- [ ] **Step 3: Re-run the Playwright filter test against the final assets**

Run the Task-4 Step-2 command. Expected: `RESULT: ALL PASS`.

- [ ] **Step 4: Manual simulation smoke (hardware/Telnet — record result)**

Enable simulation (`mosquitto_pub ... sat/simulation -m ON` or REST), connect Telnet (port 23) and open the Web UI log. Confirm narration lines appear at transitions (sim start, flame on/off, a cycle verdict, a scenario inject, window open) on BOTH Telnet and the web log, the "SAT only" toggle filters the web log to `S` lines, and the log stays quiet between transitions. This step is field-verified; note the outcome in the task.

- [ ] **Step 5: Finalize the backlog task**

```bash
backlog task edit 815 --check-ac 1 --check-ac 2 --check-ac 3 --check-ac 4 --check-ac 5 --check-ac 6 \
  --final-summary "Dual-sink satNarrate helper; transition-only narration at flame/cycle chokepoint + sim start/window/mode/safety/scenario/real-boiler; Web UI 'SAT only' toggle (Playwright-verified); build + evaluator green." \
  -s Done
git push origin feature-dev-2.0.0-otgw32-esp32-sat-support
```

---

## Self-Review notes (author)

- **Spec coverage:** dual-sink helper (Task 1) ✓; transition-only call sites incl. flame/cycle/scenario/window/mode/safety/sim/real-boiler (Tasks 2-3) ✓; "SAT only" filter (Task 4) ✓; no String / PSTR / no new REST-state (Task 1 code) ✓; Playwright filter test (Task 4) ✓; build+evaluate+bump (every task) ✓.
- **Open confirmations the implementer MUST do (flagged inline, not placeholders):** (a) `SATCycleClass` enum order vs `_satCycleWord` (Task 2 Step 2); (b) whether `renderLogDisplay` tags lines with a class before wiring the optional CSS (Task 4 Step 7); (c) exact toolbar anchor for the toggle (`id="searchLog"` neighbourhood, Task 4 Step 5).
- **Type consistency:** helper names `satNarrate_P` / `satNarratef_P`, flag `satOnlyFilter`, regex `SAT_LINE_RE` used consistently across tasks.
