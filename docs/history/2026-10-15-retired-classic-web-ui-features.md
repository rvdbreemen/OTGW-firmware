# Retired classic Web UI features (2.0.0 line)

| | |
|---|---|
| Date | 2026-10-15 |
| Decision maker | Robert van den Breemen (maintainer) |
| Related | TASK-1223.11 (gap audit), TASK-1223.12 (remove the classic UI), ADR-191 (WiFi firmware OTA on 4MB boards through a safeboot partition) |
| Gap audit | `docs/research/2026-10-15-classic-vs-v2-gap-audit.md` |
| Last commit that still holds the classic UI | `3aa1ef927` (`3aa1ef927c45c44abb0c4c7699165a3985c08f4c`), branch `dev` |

**Why this document exists.** The classic Web UI is being removed from the 2.0.0 line so the v2-only filesystem image fits the smaller 768K LittleFS partition of the safeboot layout (ADR-191, TASK-1223.12). The gap audit listed 31 classic features that v2 lacks. The maintainer decided to port 19 of them to v2 and to drop the other 12. This document records the 12 dropped features in enough detail that any of them can be rebuilt later without digging through git history.

**Recovering the source.** Every file and line number in this document refers to commit `3aa1ef927`. To read a file as it was:

```bash
git show 3aa1ef927:src/OTGW-firmware/data/index.js   > index.js
git show 3aa1ef927:src/OTGW-firmware/data/index.html > index.html
git show 3aa1ef927:src/OTGW-firmware/data/graph.js   > graph.js
git show 3aa1ef927:src/OTGW-firmware/data/sat.js     > sat.js
git show 3aa1ef927:src/OTGW-firmware/data/design.html > design.html
git show 3aa1ef927:src/OTGW-firmware/FSexplorer.ino  > FSexplorer.ino
```

**Evidence tags.** Unless marked otherwise, every claim was verified by reading the code at the cited lines on 2026-10-15. A claim marked **(inferred)** was reasoned from code paths or browser behaviour but not traced end to end. Nothing in this document was run on hardware or in a browser.

**Scope words used below.**

- **Removed by TASK-1223.12**: in that task's description or acceptance criteria.
- **Candidate for removal**: proposed in gap audit section 6, not yet in a task's acceptance criteria.
- **Kept, UI-dead**: the endpoint stays (gap audit section 3 proposes no endpoint deletion, and `docs/api/openapi.yaml` documents it), but no UI calls it once the classic UI is gone.

---

## Summary table

Effort column: **estimate**, in working hours for one developer who knows the v2 bundle, excluding review and bench testing.

| # | Feature (gap-audit row) | Classic files | Firmware pieces | Endpoints | Rebuild effort (estimate) |
|---|---|---|---|---|---|
| 1 | UI switch and `ui_usev2` (row 2) | `index.html:120-156`, `v2.html:131`, `v2.js:202-223`, `v2.js:4343-4344`, `index.js:7732`, `:7940` | `UItypes.h:27`, `FSexplorer.ino:150-165`, `:184-197`, `:98-110`, `restAPI.ino:3853`, `:4225`, `settingStuff.ino:290`, `:848` | POST `/api/v2/settings` | 1 to 2 h (only meaningful if a second UI ever returns) |
| 2 | Heap footer (row 5) | `index.html:708`, `index.js:357`, `:498-502`, `:1187-1188`, `:4944-4950`, `:5049-5051` | `restAPI.ino:3704-3754` (`sendDeviceTimeV2`), rate limit `restAPI.ino:2650` | GET `/api/v2/device/time` | 1 h |
| 3 | Log persistence across reloads (row 22) | `index.js:1252-1259`, `:1414-1590`, `:1623-1638`, `:1641-1688`, `:2596`, `:2820-2826`, `:3592` | none | none (localStorage only) | 3 to 4 h |
| 4 | Resizable table columns (row 25) | `index.js:8401-8705`, `:1818-1827`, `:3453-3454`, `components.css:556-575` | none | none (localStorage only) | 4 to 6 h |
| 5 | 8-option graph window list (row 30) | `index.html:390-400`, `graph.js:25`, `:151-163`, `:402-407`, `:962-964` | `ui_graphtimewindow` (see feature 11) | none | under 1 h for the list; depends on port row 28 for a buffer that can hold 12 h or 24 h |
| 6 | Graph disconnect markers (row 31) | `graph.js:33`, `:781-808`, `:823-849`, `index.js:1931-1932`, `:1949-1950` | none | WebSocket `/ws` (open/close events only) | 2 h |
| 7 | Auto-hide log on small screens (row 32) | `index.js:13`, `:1714-1827`, `:1840-1856`, `:3600-3605`, `components.css:1902-1906` | none | WebSocket `/ws` (not opened) | 1 to 2 h |
| 8 | `window.otgwDebug` console helper (row 41) | `index.js:827-1152`, `:1242`, `:1641-1688` | none | GET `device/info`, `settings`, `health`, any GET; POST `otgw/command/{cmd}` | 2 h |
| 9 | `design.html` showcase (row 42) | `design.html` (627 lines), `theme-toggle.js`, `sat-slider.js` | none (served by the catch-all `FSexplorer.ino:343`, `:585-596`) | GET `/design.html` | 4 to 8 h to recreate for v2 |
| 10 | WiFi scan (row 46) | `index.js:7049-7054`, `:9447-9546` | `restAPI.ino:2458-2552` (`handleNetwork`), route `:2566`, `:2592` | GET `/api/v2/network/scan` | 2 h |
| 11 | `ui_*` display preferences (row 49) | `index.js:2836-2965`, `:3391-3395`, `:6242-6249`, `:9143-9266`, `graph.js:172-197` | `UItypes.h:20-30`, `restAPI.ino:3847-3856`, `:4224-4225`, `settingStuff.ino:284-293`, `:842-854` | GET/POST `/api/v2/settings` | 3 h |
| 12 | Weather location auto-fill and OWM key wizard (row 61) | `sat.js:755-771`, `:899-1128` | `restAPI.ino:1562-1590` (needs-setup branch), `SATweather.ino:85-86`, `:297-301`, `:560-600` | GET `sat/weather`, GET `sat/weather/needs-setup`, POST `settings`, external GET `api.openweathermap.org` | 3 h (plus a firmware fix, see below) |

---

## 1. UI switch (classic and v2) and `settings.ui.bUseV2`

**Gap-audit row 2.** Scope: removed by TASK-1223.12 (button, flag, `sendIndex` branch).

### What it did

The device served one of two complete Web UIs at `/`. The classic header had a button **"Try New UI"** next to the theme toggle. The v2 header had a button **"Classic UI"** (`v2.html:131`). Clicking either one stored the choice on the device and reloaded the page, so every browser that opened the device afterwards got the same UI. Both shells also stayed reachable directly at `/index.html` and `/v2.html`.

A second route to the same flag: the classic Settings page listed `ui_usev2` as a normal checkbox labelled "Use v2 Web UI" (label `index.js:7732`, help text `index.js:7940`). The classic `hiddenSettings` list (`index.js:6242-6262`) does not hide it. v2 hides it from its settings list (`SET_HIDE`, `v2.js:1711`).

### How it was built

**Firmware.**

- Flag: `bool bUseV2 = false;` in `UISection` (`UItypes.h:27`), instance `settings.ui.bUseV2`.
- Persisted in `settings.ini` as `ui_usev2` (`settingStuff.ino:290` write, `settingStuff.ino:848` parse with `EVALBOOLEAN`).
- Exposed in GET `/api/v2/settings` as `{"value":<bool>,"type":"b"}` (`restAPI.ino:3853`), writable through the POST whitelist `knownSettings` (`restAPI.ino:4225`).
- Root routing (`FSexplorer.ino:184-197`): when `/index.html` exists, `/`, `/index` and `/index.html` all go to `sendIndex`; otherwise they serve `FSexplorer.html` as a recovery page.
- `sendIndex` (`FSexplorer.ino:150-165`):

```cpp
static void sendIndex(AsyncWebServerRequest *request) {
  webBeginRequest(request);
  if (!checkHttpAuth()) return;   // ADR-056: ask for the password before any API call
  // TASK-908: device-wide default UI selector.
  if (settings.ui.bUseV2 && LittleFS.exists("/v2.html")) {
    serveVersionedAsset("/v2.html", F("text/html; charset=UTF-8"));
  } else {
    serveVersionedAsset("/index.html", F("text/html; charset=UTF-8"));
  }
}
```

- ETag fix tied to this switch (`FSexplorer.ino:98-110`): the ETag is `"<fsHash>-<path>"`, not the bare filesystem hash. The comment explains why: with a bare hash, `/` returned the same validator for both shells, the browser received a false 304 after a switch, and kept showing the old shell until a hard reload (TASK-960).

**Classic button** (`index.html:120` button, inline script `index.html:122-156`). It POSTs the flag and reloads only after a confirmed 200. The retry logic exists because a 503 from the REST backpressure gate used to drop the switch silently (TASK-960):

```js
var url = location.protocol + '//' + location.host + '/api/v2/settings';
var attempt = 0;
function trySet() {
  fetch(url, {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name: 'ui_usev2', value: 'true' })
  }).then(function (r) {
    if (r.ok) { location.href = '/'; }
    else if (++attempt <= 8) { setTimeout(trySet, 250 * attempt); }
    else { b.textContent = orig; b.title = 'Device busy, tap to retry'; }
  }).catch(function () {
    if (++attempt <= 8) { setTimeout(trySet, 250 * attempt); }
    else { b.textContent = orig; b.title = 'Device busy, tap to retry'; }
  });
}
```

**v2 button** (`gotoClassic`, `v2.js:202-223`, wired at `v2.js:4343-4344`): the same loop with `value: 'false'`. On final failure it gives no feedback (no "tap to retry" text).

**Data flow.** Click, then POST `/api/v2/settings` `{"name":"ui_usev2","value":"true"|"false"}`, then `postSettings` (`restAPI.ino:4239`) to `updateSetting`, which sets the RAM flag at once and schedules a debounced flash write (2 s timer). Then `location.href = '/'`, then `sendIndex` serves the other shell.

**Edge cases.**

- Backoff: 8 retries at 250 ms x attempt, about 9 s in total.
- The flag is device-wide, not per browser. One user switching changes it for everyone.
- The value is posted as a JSON string `"true"`; `extractJsonField` (`jsonStuff.ino:882-943`) accepts strings and bare tokens alike, and `EVALBOOLEAN` (`OTGW-firmware.h:151`) accepts `true`, `on` and `1`.

### Why it was dropped

There is no second UI to switch to. TASK-1223.12 removes the flag, both buttons and the `sendIndex` branch; the root always serves `v2.html`. TASK-1223.12 states that an old `ui_usev2` line in `settings.ini` is ignored on read, so no migration is needed. That claim was not checked here **(inferred from the task text)**.

### What a rebuild would need

- Still there after removal: POST `/api/v2/settings`, `serveVersionedAsset`, and the path-specific ETag.
- Removed with it: `bUseV2`, the `ui_usev2` GET/POST/persist lines, the `sendIndex` branch, both buttons.
- Gap audit section 5 names a hard prerequisite of the removal itself: `FSexplorer.ino:184` keys the root route on `/index.html`. Without that file the root serves FSexplorer, not v2, unless the check is moved to `/v2.html`.

---

## 2. Heap footer

**Gap-audit row 5.** Scope: removed with the classic files.

### What it did

Bottom-right of every classic page, before the copyright line: `Heap: <free> / <maxblock> B`, for example `Heap: 61234 / 31732 B`. Tooltip: "Free heap memory / largest contiguous free block, in bytes" (`index.html:708`). When the gateway rate-limited the poll, the text was marked stale and the tooltip changed to the pacing reason.

### How it was built

**Data source.** GET `/api/v2/device/time`, handler `sendDeviceTimeV2` (`restAPI.ino:3704-3754`). Response shape:

```json
{"devtime":{"dateTime":"2026-10-15 07:14:20","epoch":1792041260,"message":"",
  "psmode":false,"otgwsimulation":false,"freeheap":61234,"maxfreeblock":31732,
  "psram_found":0,"psram_size":0,"psram_free":0,"internal_free":61234,
  "internal_maxblk":31732,"networkmode":"wifi","ipaddress":"192.168.1.10",
  "wifiquality":80}}
```

(Field list from the handler; the values are illustrative.) `freeheap` = `platformFreeHeap()`, `maxfreeblock` = `platformMaxFreeBlock()`.

**Polling.** One shared poller drives the clock, the status message, PS mode, the network indicator and the heap readout:

- `DEVTIME_POLL_MS = 5000` (`index.js:357`).
- `devTimePoller = makePacedPoller({periodMs: DEVTIME_POLL_MS, run: ..., onStale: v => setRegionStale(['#theTime','#heap-info'], v, PACED_REASON)})` (`index.js:498-502`).
- `makePacedPoller` (`index.js:379-445`, ADR-173) is a self-rescheduling `setTimeout`. On a 429 it reschedules at a random offset inside a full period, trusting `Retry-After` up to 4 periods. After 3 refusals it marks the region stale. From 4 refusals on it backs off to 2x, then 4x the period.
- `refreshDevTime` (`index.js:5005-5029`) skips a request within `DEVTIME_MIN_INTERVAL_MS = 4000` of the previous one (ADR-184), because page switches call it next to the poller.
- Firmware side: GCRA budget `{4000 ms window, burst 2}` for `device/time` (`restAPI.ino:2650`, ADR-172). The window is sized at 75% of the classic 5 s poll, and `evaluate.py::check_poll_window_coupling` reads `DEVTIME_POLL_MS` from `index.js` to keep the two in step.

**Rendering** (`index.js:4944-4950`, fed from `applyDevTime`, `index.js:5049-5051`):

```js
function updateHeapDisplay() {
  const el = document.getElementById('heap-info');
  if (!el || currentFreeHeap === null) return;
  el.textContent = `Heap: ${currentFreeHeap} / ${currentMaxFreeBlock} B`;
}
// in applyDevTime(json):
if (devtime.freeheap !== undefined)    currentFreeHeap = devtime.freeheap;
if (devtime.maxfreeblock !== undefined) currentMaxFreeBlock = devtime.maxfreeblock;
updateHeapDisplay();
```

`setRegionStale` (`index.js:466-488`) parks the element's own `title` in `data-stale-title`, sets `data-stale` (styled in `components.css`), and restores the title when the data is fresh again.

### Why it was dropped

The diagnostic value stays on the v2 Debug page (`v2.js:2599`: free heap, max free block, fragmentation, read from `device/info` when the page opens). A permanent 5 s poll from every open browser costs device budget for a number most users never read.

### What a rebuild would need

- `device/time` is **kept**. It also stays UI-live if port rows 4 (device clock) or 16 (PS=1 fallback) land, since both read it. If neither lands, it becomes **kept, UI-dead**.
- `/api/v2/health` and `/api/v2/device/info` also carry heap fields **(inferred for health: v2 polls it every 15 s, `v2.js:4411-4412` per the gap audit; field list not checked)**.
- A v2 rebuild should piggyback on a poll v2 already runs, not add a new one. If a new poll is added, the ADR-172 window and `check_poll_window_coupling` need a new anchor (gap audit section 5).

---

## 3. Log persistence across page reloads

**Gap-audit row 22.** Scope: removed with `index.js`.

### What it did

The classic live OpenTherm log (Home, Log tab) survived a page reload, a browser restart or a tab close. On load, the last saved buffer reappeared before new WebSocket lines arrived. The log display preferences (auto-scroll, timestamps, capture mode, search text) came back too.

### How it was built

**Storage.**

| localStorage key | Content |
|---|---|
| `otgw_log_buffer` | JSON array of log entries `{time: "HH:MM:SS.uuuuuu", data: {<parsed WS JSON line>}}` (entry shape from `addLogLine`, `index.js:2552-2563`) |
| `otgw_log_prefs` | `{autoScroll, showTimestamps, captureMode, searchTerm, savedAt}` |

Constants (`index.js:1252-1259`): `PERSISTENCE_INTERVAL_MS = 30000`, `DEBOUNCE_SAVE_MS = 2000`.

**Save triggers.**

1. `debouncedSave()` (`index.js:1557-1572`) is called for every log line from `addLogLine` (`index.js:2596`). It restarts a 2 s timer each time, with a 1 s rate limit.
2. `startPersistenceTimer()` (`index.js:1574-1590`): a 30 s `setInterval` fallback. Started from the WebSocket `onopen` (`index.js:1928`).
3. `pagehide` and `beforeunload` call `persistOTLogBufferForUnload()` (`index.js:1623-1638`).

Because the debounce restarts on every line, on a busy bus it may never fire; the 30 s timer and the unload handlers then do the saving **(inferred: depends on the frame rate, roughly one frame per second on a typical OT bus)**.

**Save logic** (`saveDataToLocalStorage`, `index.js:1414-1478`), abridged:

```js
const availableBytes = storageQuotaMB * 1024 * 1024 * STORAGE_SAFETY_MARGIN; // 0.8
let logsToSave = otLogBuffer;
// estimate size from 50 random samples
...
if (estimatedSize > availableBytes) {
  const maxEntries = Math.floor(availableBytes / (estimatedSize / logsToSave.length));
  logsToSave = otLogBuffer.slice(-maxEntries);
}
localStorage.setItem(PERSISTENCE_KEY_LOGS, JSON.stringify(logsToSave));
localStorage.setItem(PERSISTENCE_KEY_PREFS, JSON.stringify({
  autoScroll, showTimestamps, captureMode, searchTerm, savedAt: new Date().toISOString() }));
} catch (e) {
  if (e.name === 'QuotaExceededError' || e.code === 22) {
    const halfEntries = otLogBuffer.slice(-Math.floor(otLogBuffer.length / 2));
    localStorage.setItem(PERSISTENCE_KEY_LOGS, JSON.stringify(halfEntries));
  }
}
```

**Restore** (`restoreDataFromLocalStorage`, `index.js:1485-1544`) is called from `initMainPage` (`index.js:3592`). It replaces `otLogBuffer`, rebuilds the filtered buffer, schedules a redraw, and 100 ms later syncs the three checkboxes and the search field. On a parse error it deletes both keys.

**Clear.** The Clear button asks for confirmation and calls `clearStoredData()` (`index.js:2900-2913`, `:1546-1555`). `clearLogBuffer()` (`index.js:2820-2826`) removes only the log key.

**Debug hook.** `window.otgwPersistence = {save, restore, clear, info}` (`index.js:1641-1688`); `info()` prints buffer, JS heap and storage figures.

### Edge cases and bugs

- **Quota mismatch (inferred).** `storageQuotaMB` comes from `navigator.storage.estimate()` (`index.js:1265-1288`), which reports the whole origin quota (often hundreds of MB or more). localStorage itself is capped at about 5 MB per origin in current browsers. So the pre-trim rarely triggers, and the real limit is hit as `QuotaExceededError`.
- **Half-size retry drops prefs.** The retry path saves only the log key, not the prefs key.
- **Order conflict.** `restoreDataFromLocalStorage` runs first, then `loadPersistentUI` (`index.js:3595`) fetches the `ui_*` settings and overwrites `autoScroll`, `showTimestamps` and `captureMode` from the device (feature 11). The device value wins.
- **Main-thread cost.** Each save stringifies the whole buffer (5k to 10k lines in normal mode, `calculateOptimalMaxLines`, `index.js:1336-1366`) on the main thread.

### Why it was dropped

Stream-to-file (covered in v2, gap-audit row 20) handles long captures without a storage quota risk. v2 keeps a fixed 600-line buffer (`v2.js:821`); the larger buffer is port row 21.

### What a rebuild would need

No firmware or endpoint. Reuse the v2 log buffer (`logBuf`, `v2.js:821`) and save it on `pagehide` plus a 30 s interval. Do not save per line. Size by entry count, not by `navigator.storage.estimate()`, and catch `QuotaExceededError`. If port row 21 lands with a much larger buffer, store only the newest N lines or use IndexedDB **(suggestion, not a decision)**.

---

## 4. Resizable, remembered table columns

**Gap-audit row 25.** Scope: removed with `index.js` and the `.col-resizer` rules in `components.css`.

### What it did

On Home, the Statistics table and the OT Support table had a 6 px drag handle on the right edge of each header cell, except the last one. Dragging resized the column; the widths were remembered per browser. Without stored widths, columns were auto-fitted to their content.

### How it was built

**Configuration** (`index.js:8401-8405`):

```js
var OT_TABLE_COL_CONFIGS = {
  otStatsTable:   { storageKey: 'otStatsColWidths.v4',   minWidth: 30 },
  otSupportTable: { storageKey: 'otSupportColWidths.v3', minWidth: 44 }
};
var OT_TABLE_COL_FIT_PADDING = 28;
```

**Storage.** localStorage keys `otStatsColWidths.v4` and `otSupportColWidths.v3`. Each holds a JSON array of CSS widths per column, for example `["62px","48px","",...]`. The `.vN` suffix was bumped whenever the column set changed, so stale widths were discarded.

**Mechanism** (`index.js:8410-8705`):

- `ensureResizableTableColgroup` adds or trims a `<colgroup>` so there is one `<col>` per `<th>`. Widths are set on `<col>` elements, and the table width is set to the sum (`setTableLayoutWidth`), so the table scrolls sideways rather than squeezing.
- `fitTableColumnsToContent` measures every cell with a shared offscreen `<canvas>` 2D context (`measureText` with the cell's computed font) plus padding, border and 28 px slack. The widest cell per column wins. Called after the stats table renders (`index.js:8956`) and after the support table renders (`index.js:9004`, `:9030`), but only when no stored widths exist.
- Drag (`onTableResizeDown`, `index.js:8650-8678`, with move and up handlers above it):

```js
function onTableResizeMove(e) {
  if (!otTableResizeState) return;
  var newWidth = otTableResizeState.startWidth + (e.clientX - otTableResizeState.startX);
  if (newWidth < otTableResizeState.minWidth) newWidth = otTableResizeState.minWidth;
  var cols = ensureResizableTableColgroup(otTableResizeState.table);
  var maxWidth = cols ? getResizeMaxColWidth(otTableResizeState.table, cols,
                   otTableResizeState.colIndex, otTableResizeState.minWidth) : null;
  if (Number.isFinite(maxWidth) && newWidth > maxWidth) newWidth = maxWidth;
  otTableResizeState.col.style.width = newWidth + 'px';
  updateTableLayoutWidth(otTableResizeState.table);
  e.preventDefault();
}
function onTableResizeUp() {
  ...
  document.body.classList.remove('col-resizing');
  document.removeEventListener('mousemove', onTableResizeMove);
  document.removeEventListener('mouseup', onTableResizeUp);
  saveTableColWidths(otTableResizeState.table);   // localStorage write on release
  otTableResizeState = null;
}
```

- `getResizeMaxColWidth` caps a column so that the table never exceeds the container (or viewport) width.
- Setup: `initResizableTableColumns` (`index.js:8680-8697`) appends a `div.col-resizer` to every header except the last, guarded by `data-resizers-init="1"`. Called from `initMainPage` (`index.js:3453-3454`). A debounced (200 ms) window `resize` handler re-applies the layout (`index.js:1818-1827`).
- CSS (`components.css:556-575`): `.ot-stats-table th .col-resizer {position:absolute; right:0; width:6px; height:100%; cursor:col-resize}`, hover and `.dragging` background, and `body.col-resizing * {cursor:col-resize !important; user-select:none !important}` during the drag.

### Edge cases

- Mouse events only (`mousedown`, `mousemove`, `mouseup`). No touch or pointer events, so it did nothing on phones and tablets.
- The handle `click` handler stops propagation, so a drag does not trigger the header's sort click.
- The last column cannot be resized.

### Why it was dropped

Cosmetic, about 300 lines of code, and v2 was designed phone-first, where a mouse-only drag does not work anyway.

### What a rebuild would need

No firmware or endpoint. Port the colgroup and measure logic to the v2 statistics table (`v2.html:562-572`) and use Pointer Events (`pointerdown`, `setPointerCapture`) so touch works. Keep a versioned storage key.

---

## 5. The 8-option graph time-window list

**Gap-audit row 30.** Scope: removed with `index.html` and `graph.js`.

### What it did

On the classic Graph tab a `<select>` offered 10 min, 30 min, 1 h, 2 h, 4 h, 6 h, 12 h and 24 h. The x-axis slid to show that window. The choice was stored on the device as `ui_graphtimewindow`, so it came back on every browser (see feature 11).

### How it was built

- Markup (`index.html:390-400`): `<select id="graphTimeWindow">` with values `10, 30, 60, 120, 240, 360, 720, 1440` (minutes), `60` selected.
- Read once at init (`graph.js:151-163`) and on change through `setTimeWindow` (`graph.js:402-407`):

```js
setTimeWindow: function(minutes) {
    if (!minutes || isNaN(minutes)) return;
    this.timeWindow = minutes * 60 * 1000;
    this.updateChart();
},
getMinTime: function() {
    return (new Date()).getTime() - this.timeWindow;
},
```

- `updateChart` (`graph.js:966` onward) sets `xAxis.min = getMinTime()` on all 5 grids every 2 s (`UPDATE_INTERVAL_MS = 2000`, `graph.js:13`).
- The buffer behind it: `maxPoints: 864000` per series (`graph.js:25`, commented as "24h of data at 10 msgs/sec"), trimmed with `shift()` (`graph.js:954-957`). So 24 h could actually be held.
- Persistence: `setupPersistentUIListeners` saves `ui_graphtimewindow` on change (`index.js:9165-9171`); `loadPersistentUI` sets the select and calls `OTGraph.setTimeWindow` on load (`index.js:9253-9262`).
- The CSV export uses the same window (`graph.js:450`).

### Why it was dropped

The four v2 windows (10 min, 1 h, 4 h, 24 h: `v2.html:602-603`, handler `v2.js:4451-4460`) cover the use.

### What a rebuild would need

- Adding chips to v2 is trivial: the handler parses the number and unit from the chip text.
- **Depends on port row 28.** v2 keeps `SAMPLES_MAX = 1800` samples, taken per frame change (`v2.js:515-519`), so even the existing 24 h chip cannot hold 24 h on a busy bus. Longer windows only make sense once row 28 brings a time-based buffer.
- v2's settings-page enum for `ui_graphtimewindow` lists 10, 30, 60, 240 and 1440 (`v2.js:2319`), which differs from both the classic list and the v2 chips. A device that stored `120`, `360` or `720` from classic shows a value outside that enum **(inferred: how the v2 select renders an unknown value was not checked)**.

---

## 6. Graph disconnect and reconnect markers

**Gap-audit row 31.** Scope: removed with `graph.js`.

### What it did

When the live WebSocket dropped, a red dashed vertical line appeared on the classic graph; when it came back, a green dashed line. Gaps in the curves could then be read as "the browser lost the link", not "the boiler stopped".

### How it was built

- State: `disconnectMarkers: []` of `{time: <ms>, type: 'disconnect'|'reconnect'}` (`graph.js:33`).
- Producers (`graph.js:823-849`), called from the WebSocket handlers: `onopen` calls `OTGraph.recordReconnect()` (`index.js:1931-1932`), `onclose` calls `OTGraph.recordDisconnect()` (`index.js:1949-1950`).

```js
recordDisconnect: function() {
    var now = new Date().getTime();
    this.disconnectMarkers.push({ time: now, type: 'disconnect' });
    var cutoff = now - (24 * 3600 * 1000);           // keep 24 h of markers
    this.disconnectMarkers = this.disconnectMarkers.filter(function(m) { return m.time > cutoff; });
    if (this.chart) { this.updateOption(); }          // full option rebuild
},
// recordReconnect: identical with type 'reconnect'
```

- Rendering, inside `updateOption` (`graph.js:781-808`): series index 0 to 4 (the first series of each of the 5 grids) get an ECharts `markLine`:

```js
var markLineData = [];
this.disconnectMarkers.forEach(function(marker) {
    var isDisconnect = marker.type === 'disconnect';
    markLineData.push({
        xAxis: marker.time,
        lineStyle: { color: isDisconnect ? '#ff4444' : '#44ff44', type: 'dashed', width: 2 },
        label: { show: false }
    });
});
if (markLineData.length > 0) {
    seriesConfig.markLine = { silent: true, symbol: 'none', data: markLineData };
}
```

### Edge cases

- The very first `onopen` after page load also records a green "reconnect" marker, so every session starts with one green line.
- Each marker triggers a full `updateOption()` (rebuild of all series), not the incremental append path.
- Colours are hard-coded, not design tokens.
- Markers are not persisted; a reload loses them.

### Why it was dropped

Nice to have. The v2 connection map shows the link state live (gap audit).

### What a rebuild would need

No firmware or endpoint. v2 draws its graph as SVG polylines (`renderGraph`, `v2.js:1192` onward). A rebuild hooks v2's WebSocket `onopen` and `onclose` (`v2.js:878-880`), keeps a 24 h marker array, and draws one SVG `<line>` per marker that falls inside the window. Skip the first `onopen`.

---

## 7. Auto-hiding the live log on small screens and behind a proxy

**Gap-audit row 32.** Scope: removed with `index.js`. The gap audit tagged this row as inferred; it is verified in code here.

### What it did

On phones, on any window 768 px wide or narrower, and on pages loaded over HTTPS (a reverse proxy), the classic Home page switched the log section to **commands-only**: the Log, Statistics, OT Support and Graph tabs were hidden, only the raw-command input stayed, and the WebSocket was **not opened or was closed**. So it stopped the log, and with it the live statistics and graph, which are fed from the same socket. Resizing the window back above 768 px reconnected.

### How it was built

- Breakpoint: `MOBILE_BREAKPOINT_PX = 768` (`index.js:13`).
- Detection (`getOTLogDisplayState`, `index.js:1714-1736`):

```js
const isProxied = window.location.protocol === 'https:';      // ws:// is mixed content there
const isPhone = /iPhone|iPod/.test(navigator.userAgent) ||
               (/Android/.test(navigator.userAgent) && /Mobile/.test(navigator.userAgent));
const isSmallScreen = window.innerWidth <= MOBILE_BREAKPOINT_PX;
return { isProxied, isPhone, isSmallScreen, isPSmode,
         sectionDisabled: (isProxied || isPhone || isSmallScreen),
         wsDisabled:      (isProxied || isPhone || isSmallScreen) };
```

- Apply (`updateOTLogResponsiveState`, `index.js:1784-1815`): if `sectionDisabled`, call `setOTLogCommandsOnly(true)` and `disconnectOTLogWebSocket()`; otherwise clear commands-only and open the socket if it is closed.
- `setOTLogCommandsOnly` (`index.js:1756-1781`) adds class `commands-only` to `#otLogSection`, forces the Log panel active and resets `currentTab`.
- CSS (`components.css:1902-1906`): `.commands-only .ot-log-tabs, .commands-only #Statistics, .commands-only #Graph { display: none; }`.
- Guard in the connector (`initOTLogWebSocket`, `index.js:1840-1856`): if `wsDisabled && !force && !isFlashing`, it logs the reason to the console and returns without connecting. PIC flashing passes `force` so progress still streams.
- Triggers: `initMainPage` calls it once (`index.js:3606`); a window `resize` listener (debounced 200 ms) calls it again (`index.js:3600-3603`, `:1818-1827`); `applyPSmodeState` calls it on PS-mode changes (`index.js:5072-5094`).

### Edge cases

- No on-page explanation. `updateOTLogModeNotice` (`index.js:1739-1753`) only shows a text for PS=1 mode; the phone, small-screen and proxy reasons go to the console only.
- A desktop window narrowed to 768 px or less drops the socket, which also stops the graph and statistics.
- The HTTPS check is the only proxy protection in the classic UI. v2 has none (gap audit: "Not present in v2").

### Why it was dropped

v2 was designed phone-first and renders the log at any width.

### What a rebuild would need

No firmware. If the HTTPS case ever matters for v2 (a `ws://` socket from an `https://` page is blocked as mixed content), a rebuild needs only the `isProxied` check plus a visible notice, and a REST fallback for the live values (v2 already polls `otgw/otmonitor` while the socket is down, per the gap audit, `v2.js:4473`).

---

## 8. `window.otgwDebug` browser-console helper

**Gap-audit row 41.** Scope: removed with `index.js`. The gap audit cites `index.js:950-1220`; the object actually spans `index.js:827-1152` at this commit.

### What it did

A developer tool in the browser DevTools console. On load the console printed "OTGW Debug Helper Loaded. Type otgwDebug.help()". Commands:

| Command | What it did | Network |
|---|---|---|
| `help()` | Print the command menu | none |
| `status()` | Flash mode, page visibility, poll timers, WS readyState | none |
| `info()` | `console.table` of device info | GET `/api/v2/device/info` |
| `settings()` | `console.table` of settings | GET `/api/v2/settings` |
| `wsStatus()` | WS state, URL, buffer length, connect/reconnect counters, timer state | none |
| `wsReconnect()` / `wsDisconnect()` | Drop and reopen (after 1 s) or just drop the log socket | WS `/ws` |
| `otmonitor()` | Print the current OT monitor data | none |
| `logs(n = 50)` | Print the last n buffered lines | none |
| `clearLogs()` | Empty the log buffers (not localStorage) | none |
| `persistence()` | Calls `window.otgwPersistence.info()` (feature 3) | none |
| `api(endpoint)` | GET `APIGW + endpoint`, print JSON or text | any GET |
| `health()` | `api('v2/health')` | GET `/api/v2/health` |
| `sendCmd(cmd)` | Send a raw OTGW command | POST `/api/v2/otgw/command/{cmd}` |
| `verbose = true/false` | Toggle the `DEBUG_WS` console logging | none |
| `exportLogs()` | Download the buffer as an otmonitor-style text file | none |
| `exportData()` | Download the OT monitor data as JSON | none |

### How it was built

A plain object literal on `window` (`index.js:827-1139`). `verbose` is an accessor wired to the file-scoped `let DEBUG_WS = false` (`index.js:1242`) through `Object.defineProperty` (`index.js:1143-1149`), so the help text's promise actually works.

The otmonitor-compatible export, the only non-trivial logic (`index.js:1055-1121`, abridged):

```js
const lines = otLogBuffer.map(entry => {
  const d = entry.data || {};
  let line = (entry.time || "00:00:00.000000").substring(0, 15); // HH:MM:SS.uuuuuu
  line = pad(line, 17);                       // otmonitor column 17
  line += (d.raw || "").trim();               // e.g. "B4001500"
  line = pad(line, 28);                       // column 28
  line += (d.dir || d.source || "").trim();   // message type, e.g. Read-Ack
  line = pad(line, 40);                       // column 40
  let desc = (d.label || "").trim();
  if (d.value !== undefined && d.value !== null && String(d.value).trim() !== "") {
    if (desc) desc += ": ";
    desc += d.value;
    if (d.unit) desc += " " + d.unit;
  }
  return line + desc;
});
// Blob -> <a download="otgw-logs-<iso>.txt"> -> click
```

`sendCmd` uses the compat alias, not the preferred endpoint:

```js
const response = await fetch(APIGW + `v2/otgw/command/${cmd}`, { method: 'POST' });
```

### Bugs found while documenting (verified in code, not run)

- `otmonitor()` and `exportData()` read a global named `data`. No such global exists in `index.js`, `graph.js`, `sat.js`, `sat-slider.js`, `theme-toggle.js`, `echarts-theme.js` or the inline scripts of `index.html` (grep: every `data` there is a local). Both always printed "No OT monitor data available" / "No data to export".
- `logs()` prints `${line}` where each buffer entry is an object `{time, data}`, so every line reads `[object Object]`. With fewer than n entries the printed line numbers go negative.
- `sendCmd` does not URL-encode the command.

### Why it was dropped

Developer tool. DevTools plus the documented REST API cover it (gap audit).

### What a rebuild would need

No firmware. Everything it called stays: `device/info`, `settings`, `health`, and POST `otgw/commands` (the preferred form, body `{"command":"PS=1"}`, `restAPI.ino:762-771`). The alias POST `otgw/command/{cmd}` (`restAPI.ino:772`) becomes **kept, UI-dead** after removal (gap audit section 3). A v2 rebuild would expose v2's own state (`logBuf`, `model`, `ws`) in a small `window.otgwDebug` object, and should use `otgw/commands`.

---

## 9. `design.html` design-system showcase page

**Gap-audit row 42.** Scope: removed by TASK-1223.12 (named in its description).

### What it did

A static reference page at `http://<device>/design.html` showing every classic design-system component in light and dark: 1 color tokens, 2 typography, 3 buttons, 4 form inputs (incl. range sliders), 5 status and badges, 6 nav bar, 7 cards and panels, 8 OT monitor table, 9 SAT panel (with the DHW slider in both themes), 10 log viewer, 11 tabs plus stats table, 12 chart container, 13 FSexplorer, 14 banners (version mismatch, crash panel). Section headings at `design.html:131-609`. Its footer marks it as a reference page not meant for production flashing, but it was in the image.

### How it was built

- One 627-line HTML file (33,684 bytes) with a page-local `<style>` block for the showcase chrome (`.design-shell`, `.design-toc`, `.design-section`, `.design-grid`, swatches), built on the design tokens.
- Loads `ds-tokens.css` and `components.css` (`design.html:7-8`), and at the end `theme-toggle.js` and `sat-slider.js` (`design.html:624-625`) for the theme button (`design.html:107`) and live slider readouts.
- No firmware route of its own: served by the `onNotFound` catch-all (`FSexplorer.ino:343`) through `handleFile` (`FSexplorer.ino:585-596`).
- Shipped in the LittleFS image: the staging step excludes only `v2.css` (`build.py`, `STAGING_EXCLUDE`).
- `evaluate.py:95` carries an allowlist entry "design.html reference-only demo helpers" for the design-system class drift gate (ADR-091). It goes with the page.
- Listed as protected in `FSexplorer.html:228` and `v2.js:2805`.

### Why it was dropped

Not user-facing, and it depends on `theme-toggle.js` and `sat-slider.js`, which are removed with the classic set. About 33.7 KB of LittleFS on a 768K partition.

### What a rebuild would need

No firmware or endpoint. A v2 showcase would be built against `v2-bundle.css`, not `components.css`. Keeping it out of the device image (a repo-only page) avoids the flash cost **(suggestion)**. Remove the `evaluate.py:95` allowlist entry and the two protected-file list entries together with the page.

---

## 10. WiFi network scan

**Gap-audit row 46.** Scope: removed with `index.js`. Endpoint **kept, UI-dead**.

### What it did

Settings page, inside the "Network / Wi-Fi" group: a sub-heading "Available Wi-Fi Networks", a **Scan** button, and after the scan a dropdown of nearby networks, for example `HomeNet [ch6, -58dBm] [secured] (current)`. Informational only: the SSID cannot be changed here; the help text points to Reset WiFi and the AP portal.

### How it was built

**Firmware** (`restAPI.ino:2458-2552`, route `kRouteNetwork` at `:2566`, dispatch at `:2592`, TASK-585). The endpoint is a state machine over the ESP32 async scan; the browser polls it:

```cpp
static bool _wifiScanStarted = false;
static void handleNetwork(...) {
  if (wc < 5 || strcasecmp_P(words[4], PSTR("scan")) != 0) { sendApiError(404, ...); return; }
  if (method != HTTP_GET) { sendApiMethodNotAllowed(F("GET")); return; }
  if (state.flash.bPICactive) { sendApiError(503, F("Scan unavailable during PIC flash")); return; }
  int16_t scanResult = WiFi.scanComplete();
  if (scanResult == WIFI_SCAN_RUNNING) {
    webSend(200, F("application/json"), F("{\"status\":\"scanning\"}")); return;
  }
  if (scanResult == WIFI_SCAN_FAILED || !_wifiScanStarted) {
    WiFi.scanNetworks(true /*async*/);
    _wifiScanStarted = true;
    webSend(200, F("application/json"), F("{\"status\":\"scanning\"}")); return;
  }
  _wifiScanStarted = false;
  // stream {"status":"ready","count":N,"networks":[{ssid,rssi,channel,secured,connected}]}
  // SSID copied to char[33], '"' and '\\' replaced by '_'
  ...
  WiFi.scanDelete();   // free the scan result memory
}
```

Response when done:

```json
{"status":"ready","count":2,"networks":[
  {"ssid":"HomeNet","rssi":-58,"channel":6,"secured":true,"connected":true},
  {"ssid":"Guest","rssi":-77,"channel":11,"secured":false,"connected":false}]}
```

**Classic UI.** `buildWifiScanPanel` (`index.js:9447-9497`) builds the panel with `createElement` and `textContent` only (no `innerHTML` on SSIDs). It is appended once after the settings render (`index.js:7049-7054`). `startWifiScan` (`index.js:9499-9546`) polls every 1 s while `status === 'scanning'`, then fills the `<select>` and preselects the connected network:

```js
function poll() {
  fetch(APIGW + 'v2/network/scan')
    .then(function(r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function(j) {
      if (j.status === 'scanning') { _wifiScanPollTimer = setTimeout(poll, 1000); return; }
      clearTimeout(_wifiScanPollTimer); _wifiScanPollTimer = null;
      if (btn) { btn.disabled = false; btn.textContent = 'Scan'; }
      if (!j.networks || j.networks.length === 0) { statusEl.textContent = 'No networks found.'; return; }
      statusEl.textContent = j.count + ' network(s) found';
      j.networks.forEach(function(n) {
        var opt = document.createElement('option');
        opt.value = n.ssid;
        opt.textContent = n.ssid + ' [ch' + n.channel + ', ' + n.rssi + 'dBm]'
                        + (n.secured ? ' [secured]' : '') + (n.connected ? ' (current)' : '');
        if (n.connected) opt.selected = true;
        netSel.appendChild(opt);
      });
      netSel.style.display = '';
    })
    .catch(function(e) { /* reset button, show 'Scan failed: ' + e.message */ });
}
```

### Edge cases

- The header comment says the endpoint refuses scans "during OTA or PIC flash operations"; the code checks only `state.flash.bPICactive`.
- `connected` compares the sanitised scan SSID with the raw current SSID, so a connected SSID that contains `"` or `\` is never marked current. Several access points with the same SSID are all marked current.
- `_wifiScanStarted` is one global for all clients. Two browsers scanning at once share one scan; whichever polls first after completion gets the list and the other starts a new scan.
- No client-side timeout: if the scan never completes, the browser polls every second until the page closes **(inferred: whether the ESP32 async scan can stay in RUNNING indefinitely was not checked)**.

### Why it was dropped

Informational only. The SSID is read-only in both UIs and is set through the AP portal.

### What a rebuild would need

GET `/api/v2/network/scan` stays (documented in `docs/api/openapi.yaml:190`). A v2 rebuild is UI-only: a button and a list on the v2 Network settings card, with the 1 s poll above and a timeout. Fix the comment-versus-code mismatch on the OTA guard at the same time.

---

## 11. The `ui_*` display preferences

**Gap-audit row 49.** Scope: the classic readers and writers go with `index.js` and `graph.js`. The seven keys and their firmware side are a **candidate for removal** (gap audit section 6), not in TASK-1223.12's acceptance criteria. `ui_onboarded` and `sat_onboarded` live in the same struct but are **kept**: v2 reads them (`v2.js:3849`, `v2.js:4007`).

### What it did

Seven log and graph toggles in the classic UI were stored on the device, not in the browser, so every browser that opened the gateway started with the same choices.

| Key | Struct field (`UItypes.h:20-30`) | Default | Classic control | Effect on load |
|---|---|---|---|---|
| `ui_autoscroll` | `bAutoScroll` | true | Log: Auto-scroll (`chkAutoScroll`) | sets `autoScroll` |
| `ui_timestamps` | `bShowTimestamp` | true | Log: Timestamps (`chkShowTimestamp`) | sets `showTimestamps`, redraws |
| `ui_capture` | `bCaptureMode` | false | Log: Capture mode (`chkCaptureMode`) | sets `captureMode`, recomputes buffer limit |
| `ui_autodownloadlog` | `bAutoDownloadLog` | false | Log: Auto-download (`chkAutoDownloadLog`) | `toggleAutoDownloadLog(true)` (15 min timer) |
| `ui_autoexport` | `bAutoExport` | false | Graph: Auto-save CSV (`chkAutoExport`) | `OTGraph.toggleAutoExport` (15 min timer) |
| `ui_autoscreenshot` | `bAutoScreenshot` | false | Graph: Auto-save PNG (`chkAutoScreenshot`) | `OTGraph.toggleAutoScreenshot` (15 min timer) |
| `ui_graphtimewindow` | `iGraphTimeWindow` | 60 | Graph: Time Window (`graphTimeWindow`) | `OTGraph.setTimeWindow(minutes)` |

### How it was built

**Firmware.**

- Struct `UISection` in `UItypes.h:20-30`, instance `settings.ui` (ADR-051, ADR-079).
- Persisted in `settings.ini` (`writeSettings`, `settingStuff.ino:284-289`, `:293`) and parsed in `updateSetting` (`settingStuff.ino:842-847`, `:851-854`; booleans via `EVALBOOLEAN`, the window via `atoi` clamped to 1..1440).
- GET `/api/v2/settings` (`restAPI.ino:3847-3852`, `:3856`): booleans as `{"value":true,"type":"b"}`, the window as an int with advertised range 0..1440.
- POST whitelist `knownSettings` (`restAPI.ino:4224-4225`).
- Also named in `docs/api/README.md:310`, `tests/webui_label_coverage.spec.js:26` and the golden fixture `scripts/json-golden/v2_settings.json` (file-level grep only).

**Classic write path.** Each control's change handler calls `saveUISetting(key, value)` which calls `sendPostSetting` (`index.js:7328-7372`): POST `/api/v2/settings` `{"name":"ui_autoscroll","value":false}` with failure feedback. Handlers: `index.js:2846` (autoscroll), `:2865` (capture), `:2932` (auto-download), `:2963` (timestamps), `:9148-9172` (`setupPersistentUIListeners`: auto-screenshot, auto-export, time window) and `graph.js:172-197` (auto-screenshot, auto-export again).

**Classic read path.** `loadPersistentUI` (`index.js:9174-9266`), called once from `initMainPage` (`index.js:3595`). One GET of the settings, then per key:

```js
fetch(apiPath + "v2/settings").then(r => r.json()).then(json => {
  const settings = json.settings;
  const getVal = (name) => settings[name] ? settings[name].value : null;
  const capVal = getVal("ui_capture");
  if (capVal !== null) {
    const chk = document.getElementById("chkCaptureMode");
    if (chk) {
      chk.checked = (capVal === true || capVal === "true");
      captureMode = chk.checked;
      if (typeof updateDynamicLimits === 'function') updateDynamicLimits();
    }
  }
  const timeVal = getVal("ui_graphtimewindow");
  if (timeVal !== null && timeVal > 0) {
    const sel = document.getElementById("graphTimeWindow");
    if (sel) { sel.value = timeVal; OTGraph.setTimeWindow(parseInt(timeVal)); }
  }
  // same pattern for the other five keys
});
```

The classic Settings page hides all seven from its generic list (`hiddenSettings`, `index.js:6242-6249`) because they have their own controls.

### Edge cases and bugs (verified in code, not run)

- **Double POST.** `chkAutoScreenshot` and `chkAutoExport` each get two change listeners that both save: one from `OTGraph.init` (`graph.js:172-197`, called at `index.js:3608`) and one from `setupPersistentUIListeners` (`index.js:9150-9163`, called at `index.js:3594`). One click therefore sends two identical POSTs.
- **`saveUISetting` is declared twice** (`index.js:3391` and `index.js:9143`). Function hoisting makes the later one win; the first one's console log never runs.
- **Range mismatch.** GET advertises 0..1440 for `ui_graphtimewindow`; the parser clamps to 1..1440.
- **v2 shows them but ignores them.** v2 lists the seven as editable rows in a "User Interface" category (`v2.js:1471`, `:1532-1538`) but never reads them to set its own controls (gap audit). Editing them in v2 changes nothing visible.

### Why it was dropped

v2 keeps its own per-page state. Device-wide log and graph preferences are a classic concept.

### What a rebuild would need

- If the keys are kept: v2 only needs a reader at load time that applies them to its log and graph controls, plus writers on change. No firmware change.
- If the keys are retired (candidate): remove `UItypes.h:20-25`, `:29`, the GET lines `restAPI.ino:3847-3852`, `:3856`, the whitelist tokens at `:4224-4225`, the persist lines `settingStuff.ino:284-289`, `:293`, the parse lines `:842-847`, `:851-854`, the v2 rows `v2.js:1532-1538` and the enum `v2.js:2319`, and update the docs, test and golden fixture named above. A rebuild would then add them back the same way.
- Overlap with ports: `ui_capture` belongs with port row 21 (log depth and capture mode); `ui_graphtimewindow` with port row 28 (graph buffer). Decide those keys together with the ports.

---

## 12. Weather location auto-fill and the OpenWeatherMap key wizard

**Gap-audit row 61.** Scope: removed with `sat.js`. Endpoint `sat/weather/needs-setup` **kept, UI-dead**.

### What it did

Two helpers on the classic SAT page, both started from `SAT.start()` (`sat.js:755-771`):

1. **Auto-fill**: on the first SAT page visit with no weather location configured, ask the browser for its position and store it as the weather location, silently.
2. **OWM key wizard**: 3 s after start, ask the firmware whether weather needs setup. If so, prompt (with `window.prompt`) for an OpenWeatherMap API key, validate it against openweathermap.org from the browser, then store it and enable weather.

Context: by default the firmware fetches weather from Open-Meteo without a key; it switches to OpenWeatherMap only when `SATweatherapikey` is set (`weatherShouldUseOwm`, `SATweather.ino:297-301`).

### How it was built

**Auto-fill** (`maybeAutoPrefillWeatherLocation`, `sat.js:985-1033`):

```js
fetch(APIGW + 'v2/sat/weather').then(r => r.json()).then(function(w) {
  var lat = parseFloat(w.latitude), lon = parseFloat(w.longitude);
  if (!isNaN(lat) && !isNaN(lon) && (lat !== 0 || lon !== 0)) return;   // already set
  navigator.geolocation.getCurrentPosition(function(pos) {
    var settings = [
      { name: 'SATweatherlat', value: pos.coords.latitude.toFixed(4) },
      { name: 'SATweatherlon', value: pos.coords.longitude.toFixed(4) }
    ];
    // POST one at a time (REST backpressure gate)
    settings.reduce(function(chain, s) {
      return chain.then(function() {
        return fetch(APIGW + 'v2/settings', { method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: '{"name":"' + s.name + '","value":"' + s.value + '"}' });
      });
    }, Promise.resolve()).then(fetchWeather).catch(function() {});
  }, function() {}, { timeout: 5000, maximumAge: 3600000 });
});
```

The manual "Detect Location" button (`sat.js:947-983`, `index.html:580`) did the same plus `SATweatherenable=1`. It is covered in v2 (`satDetectLocation`, `v2.js:3588`), which does not set `SATweatherenable`.

**Wizard trigger** (`checkWeatherNeedsSetup`, `sat.js:1038-1054`): GET `/api/v2/sat/weather/needs-setup`, expecting `{"needs_setup":bool,"has_key":bool}`; a one-shot flag `_owmWizardShown` (`sat.js:1036`) stops it repeating per page load.

**Firmware side** (`restAPI.ino:1562-1590`): `needs_setup` is true when uptime is at least 5 min, `OTcurrentSystemState.Toutside == 0.0f` (no outside temperature from the boiler, MsgID 27) and `!state.sat.weather.bValid`. `has_key` is `sWeatherApiKey[0] != '\0'`.

**Wizard** (`showOwmWizard`, `validateOwmKey`, `persistOwmSettings`, `sat.js:1056-1128`):

```js
var key = window.prompt('Outside temperature is not available.\n\n' +
                        'Step 1/2: Enter OpenWeatherMap API key (leave blank to cancel).');
if (!key) return;
validateOwmKey(key).then(function() { return persistOwmSettings(key); })
  .catch(function(e) { showFeedback('OWM validation failed: ' + e.message, true); });
// validateOwmKey: read lat/lon from GET sat/weather (fails with 'No location set' if 0/0), then
//   GET https://api.openweathermap.org/data/3.0/onecall?lat=..&lon=..&appid=<key>
//       &units=metric&exclude=minutely,hourly,daily,alerts
//   and require j.current.temp to be a number.
// persistOwmSettings: POST, one at a time, stop on the first non-ok:
//   {"name":"SATweatherapikey","value":"<key with quotes stripped>"}
//   {"name":"SATweatherenable","value":"1"}
```

Settings keys involved: `SATweatherlat`, `SATweatherlon`, `SATweatherenable`, `SATweatherapikey` (all in `knownSettings`, `restAPI.ino:4216`, `:4223`; the parser is case-insensitive). Timers: weather refresh every 30 s (`sat.js:757`), wizard check once at +3 s.

### Bugs found while documenting

- **The wizard could never open (verified in code, not run).** The needs-setup branch tests `wc == 5 && strcasecmp_P(words[5], ...)` (`restAPI.ino:1563`). `processAPI` stores the empty token before the leading slash in `words[0]` (`restAPI.ino:2811`), so `/api/v2/sat/weather/needs-setup` arrives as `["", "api", "v2", "sat", "weather", "needs-setup"]` with `wc = 6`; `sub` is `words[4]` (`restAPI.ino:1173`). Sibling branches in the same handler read `words[5]` only when `wc > 5` (humidity, area). With `wc == 5`, `words[5]` is the zeroed empty string. The request therefore falls through to `weatherSendStatusJSON()`, which has no `needs_setup` field, and `d.needs_setup` is always undefined. Fix if ever needed: `wc > 5`.
- **Validation endpoint differs from the firmware's.** The wizard validates against One Call 3.0 (`data/3.0/onecall`); the firmware fetches Current Weather 2.5 (`data/2.5/weather`, `SATweather.ino:85-86`). One Call 3.0 needs a separate subscription on the OWM side, so a key that works for the firmware could fail the wizard **(inferred from OWM's public product terms, not tested)**.
- **Geolocation needs a secure context.** Browsers expose `navigator.geolocation` positions only on HTTPS or localhost. On plain `http://<device-ip>` the request fails and auto-fill fails silently **(inferred from browser rules, not tested)**. The same limit applies to v2's Detect location button.
- **Conflicting sources on `SATweatherenable`.** The v2 comment at `v2.js:3585-3587` says a string `"1"` for a boolean setting would silently no-op. The code path contradicts that: `postSettings` (`restAPI.ino:4239-4302`) extracts the value as text with `extractJsonField` (`jsonStuff.ino:882-943`, strings and bare tokens alike), and `updateSetting` parses it with `EVALBOOLEAN` (`settingStuff.ino:1028`, `OTGW-firmware.h:151`), which accepts `"1"`. So the classic `"1"` posts should have worked. Not run on hardware; the two sources disagree.

### Why it was dropped

Location and API key are ordinary editable settings, and v2 has Detect location. The wizard uses `window.prompt` and calls openweathermap.org from the browser (gap audit). As found above, the wizard was also unreachable in practice.

### What a rebuild would need

- Still there: GET `sat/weather`, POST `settings`, and GET `sat/weather/needs-setup` (documented, `docs/api/openapi.yaml:3119`), but the latter needs the `wc > 5` fix before any UI can use it.
- A v2 rebuild would use a proper dialog instead of `window.prompt`, skip the browser-side validation (or validate against the same 2.5 endpoint the firmware uses), and set `SATweatherenable` explicitly if that is the intent.
- Firmware pieces that go away only if the endpoint is ever retired: the needs-setup branch `restAPI.ino:1562-1585`. Nothing else is classic-only; `SATweather.ino` stays.

---

## The 19 features being ported to v2 instead

One line each, with the gap-audit row. Where a backlog task exists it is named.

| Row | Feature | Task |
|---|---|---|
| 4 | Device wall clock (device time, timezone, NTP) from `device/time` | none yet |
| 7 | Firmware/filesystem mismatch banner with link to /update | none yet |
| 10 | Full live value table (every `otgw/otmonitor` entry, stale marking) | TASK-1223.14 |
| 11 | Dallas sensor temperatures in the value table | TASK-1223.14 |
| 12 | S0 pulse counter live values in the value table | TASK-1223.14 |
| 13 | Edit Dallas sensor labels (`sensors/labels`) | TASK-1223.15 |
| 14 | OT-Direct status panel (`otdirect/status`) | TASK-1223.16 |
| 16 | PS=1 fallback: keep the value table alive by REST polling | TASK-1223.14 |
| 21 | Log depth and capture mode (larger buffer or capture toggle) | none yet |
| 27 | OT Support map from persisted bitmaps (`otgw/ot-support`) | none yet |
| 28 | Graph content: 10 series plus Dallas, time-based 24 h buffer | none yet |
| 35 | All PIC gateway settings (PR= values) | TASK-1223.17 |
| 36 | Debug page shows every `device/info` field | TASK-1223.18 |
| 44 | Fixed-IP editor with validation and DHCP prefill | none yet |
| 45 | Confirm dialog before changing Board mode | none yet |
| 48 | Webhook test call for OFF as well as ON | none yet |
| 51 | BLE name-prefix filter and "restrict ingest" (needs a firmware whitelist fix) | none yet |
| 57 | SAT calibration markers: add and delete | none yet |
| 58 | DHW setpoint slider bounds from boiler MsgID 48 | none yet |

---

## Corrections to the gap audit found while writing this report

None of these changes a drop verdict.

| Row | Gap audit says | Code says (commit `3aa1ef927`) |
|---|---|---|
| 61 | The wizard is a working feature that v2 lacks | The firmware needs-setup branch is unreachable (`wc == 5` vs `wc = 6`), so the classic wizard never opened. See feature 12. |
| 41 | `window.otgwDebug` at `index.js:950-1220` | `index.js:827-1152`. `otmonitor()`, `exportData()` and `logs()` were broken. See feature 8. |
| 32 | "Auto-disable of the live log", tagged inferred | Verified in code. It also closes the WebSocket, so statistics and graph stop too, not only the log. See feature 7. |
| 46 | Endpoint guarded against OTA and PIC flash (comment) | Only the PIC-flash guard exists in code. See feature 10. |
| 49 | Seven keys, classic applies them | Also: a double POST for two of them, a duplicate `saveUISetting`, and a 0 vs 1 range mismatch. See feature 11. |
| 1.2 / 51 | `knownSettings` at `restAPI.ino:4200`, enforced at `:4258` | `knownSettings` starts at `restAPI.ino:4175`; the check is in `postSettings` at `:4257`. Line drift only. |
