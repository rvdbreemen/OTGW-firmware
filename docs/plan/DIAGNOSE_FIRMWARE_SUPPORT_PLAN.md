<!-- Generated 2026-09-03 by a 147-agent design workflow, then verified against
     hardware on 2026-09-04. Status: PLAN ONLY. Nothing here is implemented. -->

# Status header, added 2026-09-04 after hardware verification

This document was written on 2026-09-03. Four of its own open items have since been
settled on a real gateway running diagnose 2.2 (bench unit 192.168.88.68). Read this
header before the plan body, which is otherwise unedited.

**Settled: the find(ETX) probe DOES succeed against diagnose.hex.** Section 8 named
this as the one unverified claim gating Stage 0's severity, and asked for "one boot
with diagnose.hex". That boot has happened repeatedly: the unit reports
`picavailable: true` with `picfwtype: diagnose` and `picfwversion: 2.2`. Nobody is
stranded today. Stage 0 remains worth doing, but as an asymmetry fix rather than a
rescue: the self-heal is gateway-only, and one missed ETX would make that permanent.

**Settled: the Stage 0 banner-case defect is real.** Verified in source on
2026-09-04: `OTGW_BANNER` is `"OpenTherm Gateway"` (OTGW-Core.ino:474), matched with
case-sensitive `strstr` at :4562, while the diagnose banner is
`"Opentherm gateway diagnostics - Version "` (OTGWSerial.cpp:97) with a lowercase g.
`fwreportinfo()` writes sFwversion, sDeviceid and sType and never touches
`bAvailable`. A telnet capture from the bench unit shows the diagnose banner falling
through to the catch-all branch: `processOT(4608): Not processed, received from OTGW
=> (Opentherm gateway diagnostics - Version 2.2) [43]`.

**Settled: the v2.2 menu really does have seven tests.** Section 8 listed this as
resting on a single source. The bench capture prints the menu directly from the PIC:
LED test, Bit timing thermostat, Bit timing boiler, Delay symmetry, Voltage levels,
Idle times, Temperature sensor, followed by `Enter test number: ` arriving with no
line terminator. That last point also confirms the byte-transparency work shipped in
v1.7.5 (TASK-1109) delivers the prompt to a connected client.

**Stale: the working-tree caveat in section 8.** TASK-1121 is committed and shipped
in v1.7.5, and what shipped differs from what this plan assumed. The plan says "leave
TASK-1121's site gate alone"; the shipped fix also DEFERS the call out of `setup()`
into `doTaskEvery1s()`, because a gate evaluated in `setup()` could never fire: no
banner has been read at that point, so `firmwareType()` is still `FIRMWARE_UNKNOWN`.
Section 5's row on boot commands is right in its conclusion and out of date on the
mechanism.

**Unchanged and still open:** every question in section 7, and the two remaining
single-source items in section 8 (per-key menu semantics, and post-flash bootloader
banner timing).

---

# Diagnose-firmware support on the 1.x line: implementation plan

All line numbers are against the **working tree** of `D:/Users/Robert/Documents/GitHub/RvdB/wt-otgw-1.x.x`, branch `otgw-1.x.x`, HEAD `9be61a170` **plus the uncommitted TASK-1121 hunks** (`OTGW-Core.ino` +20, `OTGW-firmware.ino` +10). If those get reverted, subtract 20 lines below `OTGW-Core.ino:868`.

---

## 0. Correction to the brief, before anything is built on it

The brief's established fact #2 is wrong, and every area that checked it agreed.

`triggerPICsettingsReadout()` (`OTGW-Core.ino:637-645`) is indeed gated only on `isPICEnabled()`, but it emits nothing. It sets `picSettingsCycleActive`. The function that actually queues `PR=` is `queryNextPICsetting()`, and its first line is:

```
OTGW-Core.ino:677:  if (!isPICEnabled() || !isGatewayFirmware()) return;
```

The other periodic writers are gated too: `PR=M` at its call site (`OTGW-firmware.ino:271`, `if (isPICEnabled() && isGatewayFirmware())`) and `SC=`/`SR=` inside `sendtimecommand()` (`networkStuff.ino:563`, `if (OTGWSerial.firmwareType() != FIRMWARE_OTGW) return;`).

**The ESP does not type `PR=` into the diagnose menu every few seconds.** Do not budget work for that. The real residue is user-initiated (MQTT, REST), boot commands (TASK-1121, in flight), and one boot-window probe.

---

## 1. Fix first, on its own: the PIC-recovery path is gateway-only by accident of string case

This is broken today whether or not a diagnose screen is ever built, and it is the only item here that can leave a user with no way back.

**What is wrong.** `fwreportinfo()` (`OTGW-Core.ino:5137`) is the banner callback registered in `detectPIC()` (`OTGW-Core.ino:571`). It fires for all three firmware types, and writes `sFwversion`, `sDeviceid`, `sType`. It never writes `state.pic.bAvailable`.

The only runtime re-enable of `bAvailable` is in `processOT()`:

```
OTGW-Core.ino:474:  #define OTGW_BANNER "OpenTherm Gateway"
OTGW-Core.ino:4561:  } else if (strstr(buf, OTGW_BANNER)!=NULL){
OTGW-Core.ino:4563-4566:   if (!state.pic.bAvailable) { state.pic.bAvailable = true; ... }
```

`strstr` is case-sensitive. The diagnose banner is `"Opentherm gateway diagnostics - Version "` (`src/libraries/OTGWSerial/OTGWSerial.cpp:97`) — lowercase `g` in "gateway". It can never match. Same for the interface banner? No: `banner3` is `"OpenTherm Interface "`, which also does not contain "OpenTherm Gateway".

**Consequence.** `bAvailable` is set exactly once for a diagnose or interface PIC, by the single `find(ETX)` probe at `OTGW-Core.ino:574`. If that probe misses, nothing re-enables it, ever. The UI would then show a correctly identified diagnose PIC (`sType` is set independently) while every PIC route refuses with 503 — including `/pic` → `upgradepic()` (`FSexplorer.ino:264`, gated on `isPICEnabled()` at `OTGW-Core.ino:5361`), which is the only way to flash `gateway.hex` back.

I am stating this as an **asymmetry, not as "users are stranded today"**: `ETX` comes from the bootloader, so the probe probably succeeds with `diagnose.hex` as well. That is unverified (see §8). The claim that holds regardless is: the self-heal is gateway-only, and one missed ETX makes that permanent.

**Fix.** Two lines in `fwreportinfo()`, alongside the existing `strlcpy` calls:

```cpp
if (!state.pic.bAvailable) {
  state.pic.bAvailable = true;
  DebugTln(F("PIC detected via firmware banner callback — PIC functions re-enabled"));
}
```

This generalises the gateway-only self-heal at `:4563-4566` to every banner type, at the one place that already sees all three.

**Files:** `src/OTGW-firmware/OTGW-Core.ino` (one hunk, ~5 lines).
**Cost:** ~40 bytes flash, 0 bytes RAM.
**Own task, own commit.** Do not fold it into the diagnose feature.

### Acceptance criteria — Stage 0

| # | Criterion | How verified |
|---|---|---|
| 0.1 | `fwreportinfo()` sets `state.pic.bAvailable = true` when it is false | **read** |
| 0.2 | No other behaviour change in `fwreportinfo()`; `sendMQTTversioninfo()` still called exactly once | **read** |
| 0.3 | `python build.py --firmware` exits 0 and the build log contains a per-target success line | **build** |
| 0.4 | On a gateway PIC, `GET /api/v2/device/info` still reports `picavailable: true` after boot and after a PIC reflash | **hardware** |
| 0.5 | On a diagnose PIC, `picavailable` is `true` and `picfwtype` is `diagnose` | **hardware** (needs `diagnose.hex` flashed) |

---

## 2. Stage 1 — stop the ESP typing into the menu, and make port 25238 actually usable

**Goal of this stage:** a user who enables port 25238 and connects with `nc` or PuTTY (raw mode) can drive the whole diagnose menu without the ESP interfering, and can reset the PIC to restart the menu. No browser work at all.

Port 25238 is already byte-transparent in **both** directions:
- outbound: `OTGW-Core.ino:4707-4710` stages every serial byte, flushed at `:4759-4760` (TASK-1109, `OTGW_PASSTHRU_CHUNK` at `:4653`);
- inbound: `OTGW-Core.ino:4771-4774` — `outByte = OTGWstream.read();` then `OTGWSerial.write(outByte);` **before** any terminator test.

So a bare digit and a lone Enter already reach the PIC today. Five things are still wrong.

### 1a. Add one positive predicate

`src/OTGW-firmware/OTGW-firmware.h`, beside line 409:

```cpp
inline bool isDiagnoseFirmware() { return OTGWSerial.firmwareType() == FIRMWARE_DIAG; }
```

Typed accessor, not `strcmp_P` on `state.pic.sType`, matching the two existing sites that already use the enum (`networkStuff.ino:563` and TASK-1121's `OTGW-Core.ino:887`). **Positive polarity is load-bearing**: `firmwareType()` starts at `FIRMWARE_UNKNOWN` (`OTGWSerial.cpp:53`), so this fails **open** — an unidentified PIC keeps working. A negative `!= FIRMWARE_OTGW` test would fail closed and break real gateways during the window before the banner is parsed, and would also silently apply diagnose behaviour to interface firmware.

Reject the alternative "mode enum": `OTGWFirmware` (`OTGWSerial.h:40-44`) already enumerates the three types, and `state.pic.sType` already carries them as strings for MQTT and REST. A third copy is state to keep in sync for no benefit.

**Cost:** 0 RAM, ~30 bytes flash.

### 1b. Gate the command queue door

`addOTWGcmdtoqueue()` (`OTGW-Core.ino:2950`) is the single door: all ten producers route through it (`MQTTstuff.ino:727, 758`; `networkStuff.ino:576, 581, 587`; `OTGW-Core.ino:592, 616, 716, 906`; `restAPI.ino:246`). Add, after the existing `isPICEnabled()` check at `:2951`:

```cpp
if (isDiagnoseFirmware()) {
  OTGWDebugTln(F("CmdQueue: PIC runs diagnose firmware - command ignored"));
  return;
}
```

This is what actually stops a stray Home Assistant automation from typing into the menu. Severity matters here: the retry engine sends every queued entry **five times over twenty seconds** (`OTGW-Core.ino:2921-2922`, applied at `:3073-3082`), and a diagnose PIC never emits the `LL:` response that would clear it early — `checkOTGWcmdqueue()` is only reached under `buf[2]==':'`. One HA setpoint publish becomes five `TT=` bursts, each terminated by CR+LF at `OTGW-Core.ino:3174-3176`, and CR is the key that ends the LED test.

**This does not lock out the way back.** The PIC upgrade path never touches the queue: `upgradepic()` → `upgradepicnow()` → `OTGWSerial.startUpgrade()` (`OTGW-Core.ino:5186`) writes bootloader packets directly. Verified by grepping every `addOTWGcmdtoqueue` call site.

### 1c. Make the REST layer tell the truth

`handleCommandSubmit()` (`restAPI.ino:223`) calls `addOTWGcmdtoqueue()` at `:246` and then unconditionally sends `202 {"status":"queued"}` at `:247-248`. With 1b in place, the queue silently drops it and the API lies. Add an explicit refusal before the format check:

```cpp
if (isDiagnoseFirmware()) {
  sendApiError(409, F("PIC runs diagnose firmware - gateway commands refused"));
  return;
}
```

Two gates, deliberately: the queue door is the floor that catches every producer, the 409 is the truthful response for the one producer that has a caller waiting.

### 1d. Clear the latched settings-cycle flag

`triggerPICsettingsReadout()` sets `picSettingsCycleActive = true` (`:643`) with only an `isPICEnabled()` gate. `queryNextPICsetting()` returns at `:677` **before** the clear at `:688`, so on a non-gateway PIC the flag latches true forever. Effects: `doTaskEvery3s()` (`OTGW-firmware.ino:248-251`) calls into a permanent no-op every three seconds, `GET /api/v2/pic/settings` never refreshes, and the coalescing guard at `:639-641` never re-arms, so the first real readout after returning to gateway firmware is suppressed.

No serial traffic, so this is a correctness wart rather than a diagnose hazard. Fix it in the same patch: clear the flag and reset `picSettingsQueryIdx` where the gate trips, so the state is explicit rather than accidentally correct. Two lines, 0 RAM.

### 1e. Stop untermined keystrokes from eating the ser2net reset

This is the one that decides whether Stage 1 actually delivers.

The inbound line assembler resets only on CR:

```
OTGW-Core.ino:4817:      bytes_write = 0; //start next line
OTGW-Core.ino:4824-4826:  else { if (bytes_write < (MAX_BUFFER_WRITE-1)) sWrite[bytes_write++] = outByte; }
```

The LED test requires sending `4` and `6` with no Enter. After doing that, a user who sends `GW=R\r` from the same session has `sWrite == "46GW=R"`, and the exact match at `:4804` (`strcmp_P(sWrite, PSTR("GW=R"))`) fails. No hardware reset. Same for the `PS=1` / `PS=0` sniffers at `:4809` and `:4812`.

This matters because **there is no REST route for a hardware PIC reset**. `resetOTGW()` has exactly three callers: `MQTTstuff.ino:746`, the ser2net CR sniffer at `OTGW-Core.ino:4808`, and `setup()` at `OTGW-firmware.ino:199`. Resetting the PIC to reprint the menu is how a user recovers from a wrong keypress, and after any untermined keystroke the ser2net path loses it.

**Fix, diagnose-only so gateway behaviour is bit-identical:** add a tail comparison next to the existing exact match, guarded by `isDiagnoseFirmware()`:

```cpp
} else if (isDiagnoseFirmware() && bytes_write >= 4 &&
           strcmp_P(sWrite + bytes_write - 4, PSTR("GW=R")) == 0) {
  OTGWDebugTln(F("Detected: GW=R after menu keystrokes. Reset gateway command executed."));
  sendEventToWebSocket_P('!', PSTR("GW=R [reset]"));
  resetOTGW();
}
```

Note the ordering is harmless: the byte write at `:4773` already delivered `G`,`W`,`=`,`R`,CR into the menu before the reset fires ~1 ms later, and the reset restarts the menu anyway.

Do **not** solve this by skipping accumulation in diagnose mode — that removes the very assembly that `GW=R` needs.

### 1f. Leave TASK-1121 alone

The `sendOTGWbootcmd()` gate at `OTGW-Core.ino:887` becomes redundant once 1b lands, because boot commands go through `addOTWGcmdtoqueue()` at `:906`. Keep it. It is harmless, it emits a specific debug line, its comment documents the `setup()` ordering trap correctly, and reverting it starts a merge fight with the agent currently editing the same file.

### Files, cost, and what the user gains

| Item | File | Lines |
|---|---|---|
| 1a predicate | `src/OTGW-firmware/OTGW-firmware.h` | 1 |
| 1b queue gate | `src/OTGW-firmware/OTGW-Core.ino` ~`:2951` | 4 |
| 1c REST 409 | `src/OTGW-firmware/restAPI.ino` ~`:227` | 4 |
| 1d latch fix | `src/OTGW-firmware/OTGW-Core.ino` ~`:677` | 3 |
| 1e tail match | `src/OTGW-firmware/OTGW-Core.ino` ~`:4803` | 6 |

**Cost:** 0 bytes static RAM. Roughly 250 bytes of flash out of ~280 kB free. Nothing here touches the ~18 kB heap budget.

**After Stage 1 the user can:** flip `settings.mqtt.bLegacyPort25238Enabled`, connect one raw TCP client to port 25238, see the full diagnose menu including the terminator-less `Enter test number: ` prompt, press `1`-`7`, press `4`/`6` inside the LED test with no Enter, press Enter to end a test, and send `GW=R` to restart the menu — with no `PR=`, boot-command, MQTT or REST traffic interleaved. That is the whole feature, minus the browser.

### Acceptance criteria — Stage 1

| # | Criterion | How verified |
|---|---|---|
| 1.1 | `isDiagnoseFirmware()` exists in `OTGW-firmware.h` and tests `== FIRMWARE_DIAG` (positive polarity) | **read** |
| 1.2 | `addOTWGcmdtoqueue()` returns early when `isDiagnoseFirmware()`, after the `isPICEnabled()` check | **read** |
| 1.3 | No `addOTWGcmdtoqueue` call site is left ungated: `grep -n "addOTWGcmdtoqueue(" src/OTGW-firmware/*.ino` still returns exactly the ten known producers, all upstream of the new gate | **read** |
| 1.4 | `POST /api/v2/otgw/commands` returns 409 with a diagnose PIC, never 202 | **hardware** |
| 1.5 | `queryNextPICsetting()` clears `picSettingsCycleActive` on the non-gateway path | **read** |
| 1.6 | The PIC flash path contains no call to `addOTWGcmdtoqueue` (the escape hatch survives the gate) | **read** |
| 1.7 | `python build.py --firmware` exits 0 with a per-target success line; `python evaluate.py --quick` shows no new failures | **build** |
| 1.8 | On a gateway PIC: boot commands still execute, `PR=` polling still runs, MQTT set-topics still work | **hardware** (regression gate) |
| 1.9 | On a diagnose PIC, port 25238 open, MQTT connected: menu prints and stays intact for 5 minutes with no interleaved ESP text | **hardware** |
| 1.10 | On a diagnose PIC: send `4`, `6`, `4` with no CR, then `GW=R\r`; the PIC resets and reprints the menu | **hardware** |
| 1.11 | On a gateway PIC: `GW=R\r` still resets, and `X` followed by `GW=R\r` does **not** (the tail match is diagnose-only) | **hardware** |

---

## 3. Stage 2 — raw PIC output in the browser

**Goal:** the diagnose menu and the terminator-less prompt appear in the web UI. Read-only. The user still types into port 25238.

### What blocks it today

Two independent things, and the second is the bigger one.

1. `handleOTGW()` hands lines to the parser only on a terminator (`OTGW-Core.ino:4712`, dispatch at `:4723`). The prompt is declared in the PIC source with a `0x1A` sentinel that is never transmitted, so it never forms a line and never reaches `processOT()` → `sendLogToWebSocket()`.
2. Even the CRLF-terminated menu lines are swallowed. They fall through every branch of `processOT()` to the terminal `else` at `OTGW-Core.ino:4606-4608`, which calls `reportOTGWEvent(buf, '<', true)` — `suppressDuringStartup = true`. `reportOTGWEvent()` returns early while `isOTGWStartupQuietPeriodActive()` (`:185`), and the 15 s window (`:122`) is armed by exactly the two events that produce the menu: `resetOTGW()` (`:562`) and `detectPIC()` (`:572`). So after every ESP boot or PIC reset, the browser sees **nothing** for 15 seconds — which is the whole menu.

A raw byte tap sidesteps both, because it never enters `processOT()`.

### Design: sentinel-prefixed raw frames on the existing WebSocket

Add a static coalescing accumulator in `OTGW-Core.ino`, fed from the same drain loop that already stages the ser2net bytes:

```cpp
static char     rawTapBuf[128];
static uint8_t  rawTapLen = 0;
static uint32_t rawTapLastByteMs = 0;
```

Append each byte alongside `passthru[passthruLen++]` at `OTGW-Core.ino:4707`. Flush from a new `serviceRawTerminal()` called from `doBackgroundTasks()` **after** `handleOTGW()`, on buffer-full OR a ~20 ms idle gap OR ~100 ms max age. Flush emits one frame: sentinel byte `0x01`, then the sanitised payload, through a new `sendRawToWebSocket(const char*, size_t)` in `webSocketStuff.ino` that reuses `canSendWebSocket()` unchanged.

Three mechanics that decide whether this ships working:

- **Broadcast from outside the drain loop.** `broadcastTXT` runs `WEBSOCKETS_YIELD()` once per client slot (`libraries/WebSockets/src/WebSocketsServer.cpp:199`), and on ESP8266 that macro is `delay(0)` (`libraries/WebSockets/src/WebSockets.h:72-73`). The drain loop's own comment (`OTGW-Core.ino:4689-4690`) forbids yielding inside it, because the static `sRead`/`bytes_read` would be clobbered on re-entry. Flushing from a separate function called after `handleOTGW()` returns keeps that invariant intact.
- **Pass an explicit length and sanitise.** `broadcastTXT(payload, 0)` does `length = strlen(payload)` (`WebSocketsServer.cpp:188-190`), so a NUL truncates the frame silently. Use the length-taking overload (`WebSocketsServer.h:65`) and map bytes outside `0x20..0x7E` plus CR/LF/TAB to `.`, because RFC 6455 text frames must be valid UTF-8 and a stray high byte closes the socket. The menu is ASCII, so sanitising is lossless in practice and is the KISS answer.
- **The time window is mandatory, not an optimisation.** At 9600 baud against a ~1 kHz `doBackgroundTasks()` loop, a per-flush hook emits roughly one frame per byte — about 960 frames/s, each a `malloc` per client inside `sendFrame`. That is the fragmentation signature TASK-901 chased.

Under heap throttle (`canSendWebSocket()` returns false), keep accumulating to the 128-byte cap and then drop **one** chunk, emitting a single visible elision marker rather than dropping silently. A gap the user can see beats a corrupted menu the user cannot distinguish from a corrupted PIC.

Clear `rawTapBuf` when the last WebSocket client disconnects (`webSocketStuff.ino:145-149`), so a reconnecting browser does not get a stale mid-line chunk replayed at it — ADR-079's `doWebSocketDisconnectAll()` (`helperStuff.ino:1220-1223`) makes that a real path, not a theoretical one.

Browser side: in the existing `onmessage` (`data/index.js:1656`), test `charCodeAt(0) === 1` first and append the rest to a `<pre>` with `textContent`; everything else falls through to `parseLogLine` unchanged.

### Why WebSocket and not an HTTP-polled tail

Two areas disagreed here, and both leaned on ADR-079 ("HTTP is the transport emergency heap recovery does not attack"). **That argument does not survive the code.** `canServeHttp()` (`helperStuff.ino:1118-1133`) withholds the entire `httpServer.handleClient()` pump whenever heap is not HEALTHY and `maxBlock < HTTP_SERVE_MIN_MAXBLOCK` (2048, `OTGW-firmware.h:144`), and `reapPendingHttpConnections()` then destroys pending connections. Since `maxBlock <= freeHeap` always, HTTP serving is guaranteed already shut at the moment `emergencyHeapRecovery()` fires. HTTP degrades **earlier** than the WebSocket, not later. Do not write "HTTP is the transport ADR-079 leaves alone" into the ADR.

The two discriminators that do hold, and both favour WebSocket:

- **Echo latency.** A 250-500 ms poll is acceptable for reading a static menu and poor for a keystroke whose effect the user is watching on an LED. WebSocket delivers in tens of milliseconds.
- **Static RAM.** The WebSocket tap reuses the socket the frontend already holds (`data/index.js:1582`, one socket, cap 3 at `webSocketStuff.ino:59`) and adds only the coalescing buffer. An HTTP tail needs a ring that survives across requests **plus** a cursor, and gains nothing.

**The honest cost of choosing WebSocket:** `data/index.js:1560` builds `ws://` on port 81, which is mixed-content-blocked behind an HTTPS reverse proxy. The existing live OT log has exactly the same limitation, so diagnose is no worse than what already ships — but it means the documented fallback for proxied users is port 25238, i.e. Stage 1.

### Files and cost

| File | Change |
|---|---|
| `src/OTGW-firmware/OTGW-Core.ino` | accumulator + append in drain loop + `serviceRawTerminal()` (~45 lines) |
| `src/OTGW-firmware/OTGW-firmware.ino` | one call in `doBackgroundTasks()` after `handleOTGW()` (1 line) |
| `src/OTGW-firmware/webSocketStuff.ino` | `sendRawToWebSocket()` + clear-on-last-disconnect (~25 lines) |
| `src/OTGW-firmware/data/index.js` | sentinel branch + `<pre>` renderer (~30 lines) |
| `src/OTGW-firmware/data/index.html`, `index.css` | diagnose output pane (~15 lines each, both stylesheets) |

**Cost:** ~133 bytes static RAM (0.7 % of the ~18 kB free heap), ~400 bytes flash, 0 bytes per connected client. LittleFS impact is noise against ~1.26 MB free.

Reuse the existing `.ot-log-container` > `.ot-log-content` **pair** as nested in `data/index.html:125-126` for styling. The container owns both the dark ground (`index.css:955`) and the scroll bound the content inherits via `max-height: inherit` (`index.css:979`). Detaching `.ot-log-content` from it loses the scroll in both themes and legibility in the light theme. Do **not** reuse the `commands-only` class: `index.css:770` puts `.ot-log-container` in its hide list. Copy the pattern under a new name. Both `index.css` and `index_dark.css` are independent full stylesheets, so every rule is authored twice.

**After Stage 2 the user can:** open the web UI on a diagnose PIC and read the menu and the prompt, including within 15 seconds of a PIC reset. They still need port 25238 to press anything.

### Acceptance criteria — Stage 2

| # | Criterion | How verified |
|---|---|---|
| 2.1 | `broadcastTXT` for the raw tap is called with an explicit length, never the default `0` | **read** |
| 2.2 | The raw flush is called from `doBackgroundTasks()` after `handleOTGW()` returns, never from inside the drain loop | **read** |
| 2.3 | `OTGW_PASSTHRU_CHUNK` is still present in `src/OTGW-firmware/OTGW-Core.ino` (ADR-095 pre-commit `require_pattern`) | **read** / pre-commit hook |
| 2.4 | Sanitiser maps every byte outside `0x20..0x7E` ∪ {CR, LF, TAB} to a replacement, verified by a unit test in `test/host/` following the `test_extractJsonField.cpp` pattern | **host test** (`test\run_tests.bat`) |
| 2.5 | Buffer is cleared on last WebSocket client disconnect | **read** |
| 2.6 | On a diagnose PIC, the browser renders the full menu **and** the trailing `Enter test number: ` with its space, within 2 s of a PIC reset | **hardware** |
| 2.7 | On a gateway PIC with normal OT traffic and a browser open for 10 minutes, `state.heapdiag` shows no increase in WS drops versus a Stage 1 build, and largest-free-block does not trend down | **hardware** |
| 2.8 | `python build.py` (firmware + filesystem) exits 0; `python evaluate.py --quick` shows no new failures | **build** |

---

## 4. Stage 3 — keystrokes from the browser, and the screen

**Goal:** the user drives the entire diagnose menu from the web UI, with no external client.

### The command queue cannot carry a keystroke, and relaxing the validators does not help

Three layers reject a bare digit, and the third is the one that kills the obvious fix:

1. `data/index.js:2497` — `if (!/^[A-Z]{2}=.+$/.test(normalizedCmd))` aborts before any `fetch`.
2. `restAPI.ino:236-241` — 400 `"Invalid command format (expected LL=value)"`. This is the message Schelte hit.
3. `OTGW-Core.ino:2962` — `if ((len < 3) || (buf[2] != '=')){ ... return; }` in `addOTWGcmdtoqueue()`, silently.

Even if all three were relaxed, `sendOTGW()` appends the terminator unconditionally:

```
OTGW-Core.ino:3174-3176:  OTGWSerial.write(buf, len);
                          OTGWSerial.write('\r');
                          OTGWSerial.write('\n');
```

A queued `4` arrives as `4\r\n` — the keystroke **plus** the exact Enter that ends the LED test. There is no flag or overload to suppress it, and `sendOTGW()` has one caller, the queue drain at `OTGW-Core.ino:3063`. On top of that the retry engine would resend it five times.

So the keystroke path must bypass the queue. That is not a shortcut, it is the conclusion. Direct-to-UART writes outside the queue are an established pattern here, not a new one: `OTGW-Core.ino:4773` (ser2net) and `OTGW-firmware.ino:295` (`PR=A` probe) both do it today.

### Design

**Endpoint.** One `else if` branch inside the existing `handleOtgw()` chain (`restAPI.ino:458-481` is the neighbourhood), so no new `kV2Routes[]` row and no new PROGMEM route string:

- `POST /api/v2/otgw/keys/{token}` where `{token}` is either 1-8 characters from `[0-9]`, or the literal `enter`.

Digits-only makes `enter` unambiguous as a reserved token and makes the byte range tight enough to state in one sentence: `0x30..0x39` plus `0x0D`, nothing else, ever. **Reject a multi-segment form** (`/keys/4/6`): `strtok_r` plus the `wc < API_MAX_WORDS` bound (`restAPI.ino:189`, `API_MAX_WORDS` = 6) would silently truncate to `4` and answer 202, which is exactly the 4/6 alternation the feature exists for. Return 400 on extra segments.

**Writer.** A new `sendOTGWraw(const char* buf, size_t len)` in `OTGW-Core.ino`, replicating the gates `sendOTGW()` already applies plus one it does not:

| Gate | Source |
|---|---|
| `isPICEnabled()` | copy of `OTGW-Core.ino:3148` |
| `state.debug.bOTGWSimulation` | copy of `OTGW-Core.ino:3152` |
| `OTGWSerial.availableForWrite() >= len` | adapted from `:3164` — `>= len`, not `len+2`, because nothing is appended |
| `!isFlashing()` (`OTGW-firmware.h:536`) | **new**; `sendOTGW()` has no flash gate because its caller chain sits inside `if (!isFlashing())`, and an HTTP handler does not |

Then `OTGWSerial.write(buf, len)` and `flush()`. No terminator, ever.

**Firmware-type gate.** Refuse with 409 unless `isDiagnoseFirmware()`. Use the banner-derived type, never a settings toggle: a toggle goes stale when someone flashes gateway firmware back and leaves the raw endpoint open; the banner is re-read on every PIC reset and cannot.

**Rate limit.** `checkApiRateLimit()` returns true immediately for anything that is not a GET (`restAPI.ino:914`), so a new POST endpoint inherits **no** rate limiting. It needs its own: a `static uint32_t lastKeyMs` with a ~100 ms minimum interval, 429 otherwise. Generous against human keypress rates, and 8 bytes at 9600 baud is ~8.3 ms of wire time, so 100 ms cannot outrun the UART. 4 bytes RAM.

**Queue quiet.** Set `lastSer2netCmdMs = millis()` on each raw write, reusing the existing 2 s suppression at `OTGW-Core.ino:3057`. One line, and it is belt-and-braces independent of whether every other gate is correct.

**Auth posture, worth stating in the ADR so review does not stall on a false regression.** As a POST under `/api/v2` the endpoint inherits `checkHttpAuth()` automatically (`restAPI.ino:1003-1006`). On a default passwordless device that returns true immediately, which is the ADR-032 posture — and `POST /api/v2/otgw/commands` is already equally open while accepting a far larger command surface. The raw endpoint accepts ten digit values plus CR. It is strictly narrower.

**Frontend.** A **keypad**, not a list of test-name buttons, and not a terminal emulator. Buttons for `0`-`9` and `Enter`, each emitting exactly one byte, plus the Stage 2 output pane above them rendering the PIC's own menu text. `0` is on the keypad because at least one test prompts for a numeric value, not because Test 0 is interesting — that is incidental and explicitly not designed for. The PIC echoes its own input and prints its own prompts, so the browser needs no local echo and no state model. An optional `keydown` accelerator may be layered on top; it must never be the only path.

Gate the panel on `device.picfwtype === 'diagnose'` from `GET /api/v2/device/info` (`restAPI.ino:1253`), applied through a new `applyPICFirmwareType(type)` mirroring `applyPICAvailability()` (`data/index.js:1797`) plus a cached module global. **Fail closed to the normal UI when the field is absent** — that is the normal case for a PIC-less device (the field is emitted only inside an `isPICEnabled()` guard at `restAPI.ino:1250`). Wire it into the four sites that already apply PIC UI state: `data/index.js:3020, 4004, 4255, 4940`. Adding it to `refreshGatewayMode()` (`data/index.js:456`, already fetching `device/info` on a ~60 s throttle) costs one line and no extra HTTP request, and gives reverse-direction detection when the user flashes `gateway.hex` back.

**Do not key the panel off the optimistic filename-derived type** written at `data/index.js:5669-5672` after a flash. That is what was *intended*, not what is running; a failed flash would drop the user into a diagnose keypad talking to gateway firmware.

**Do not mark the panel `pic-only`.** `applyPICAvailability(true)` unconditionally removes `hidden` from every `.pic-only` element (`data/index.js:1801`) and is re-invoked on navigation. A diagnose panel carrying that class would be silently re-shown on a gateway PIC. Give it its own class.

### Files and cost

| File | Change |
|---|---|
| `src/OTGW-firmware/restAPI.ino` | one `else if` + handler + rate limit (~50 lines) |
| `src/OTGW-firmware/OTGW-Core.ino` | `sendOTGWraw()` (~25 lines) |
| `src/OTGW-firmware/OTGW-firmware.h` | one declaration |
| `src/OTGW-firmware/data/index.js` | `applyPICFirmwareType()` + keypad wiring (~90 lines) |
| `src/OTGW-firmware/data/index.html` | keypad markup (~20 lines) |
| `src/OTGW-firmware/data/index.css` + `index_dark.css` | ~25 lines each |

**Cost:** ~4 bytes RAM, ~700 bytes flash. Frontend assets are noise against ~1.26 MB free LittleFS.

**After Stage 3 the user can:** run every diagnose test from the browser, with no terminal program and without enabling port 25238.

### Acceptance criteria — Stage 3

| # | Criterion | How verified |
|---|---|---|
| 3.1 | `sendOTGWraw()` writes no CR and no LF; `grep` shows no `write('\r')` or `write('\n')` in it | **read** |
| 3.2 | `sendOTGWraw()` carries all four gates: `isPICEnabled`, `bOTGWSimulation`, `availableForWrite() >= len`, `isFlashing()` | **read** |
| 3.3 | Token parser accepts `[0-9]{1,8}` and the literal `enter`; rejects anything else and rejects extra path segments with 400. Unit-tested in `test/host/` | **host test** |
| 3.4 | The endpoint returns 409 unless `isDiagnoseFirmware()` | **read** + **hardware** on a gateway PIC |
| 3.5 | The endpoint has its own rate limit; `checkApiRateLimit` still present in `restAPI.ino` (ADR-086 pre-commit `require_pattern`) | **read** / pre-commit hook |
| 3.6 | Frontend fails closed to the normal UI when `picfwtype` is absent or not `diagnose` | **read** |
| 3.7 | The diagnose panel does not carry the `pic-only` class | **read** |
| 3.8 | On a diagnose PIC: select test 1 from the keypad, then press `4` and `6` alternately with no Enter; the LED count changes each time; Enter ends the test and returns to the menu | **hardware** |
| 3.9 | On a gateway PIC: the keypad is not rendered, and a hand-crafted POST to the keys endpoint returns 409 | **hardware** |
| 3.10 | Panel appears within ~60 s of flashing `diagnose.hex`, and disappears within ~60 s of flashing `gateway.hex` back, with no page reload | **hardware** |
| 3.11 | `python build.py` exits 0; `python evaluate.py --quick` shows no new failures | **build** |

---

## 5. Decisions where the analysis disagreed

| Question | Chosen | Why the other loses |
|---|---|---|
| Detection signal | `OTGWSerial.firmwareType()` (typed) | `state.pic.sType` is a `char[32]` string projection of the same enum, carrying a fourth value (`"no pic found"`, `OTGW-firmware.h:265`) that has no enum counterpart. Testing the projection instead of the source is what the project's "typed internal control flow" rule forbids. Cost is a wash: 0 bytes RAM either way, since `sType` stays for MQTT and REST regardless. |
| Gate polarity | positive `== FIRMWARE_DIAG` | `!= FIRMWARE_OTGW` is also true for `FIRMWARE_INTF` and `FIRMWARE_UNKNOWN`, so it fails closed during the boot window before any banner is parsed, and silently applies diagnose behaviour to interface firmware. |
| Where the gate lives | queue door (`addOTWGcmdtoqueue`), plus an explicit REST 409 | Per-call-site gates are what already exists and they are one new call site away from silently regressing — `queryOTGWgatewaymode()` is protected only by its caller (`OTGW-firmware.ino:271`). The queue door catches all ten producers at once. The REST 409 is needed on top because the queue drop is silent and the caller has been told 202. |
| Browser transport | WebSocket raw frames | The ADR-079 argument for HTTP polling is wrong: `canServeHttp()` (`helperStuff.ino:1118-1133`) shuts HTTP earlier than heap recovery drops WS clients. The surviving discriminators are echo latency (tens of ms vs 250-500 ms) and static RAM (0 new bytes per client). |
| Keystroke transport | new REST POST | WebSocket input is ~20 lines cheaper and `WStype_TEXT` is already arriving and being discarded (`webSocketStuff.ino:180-184`), but port 81 can never be password-gated, whereas a POST under `/api/v2` inherits `checkHttpAuth()` for the minority who set a password. ADR-079 also drops all WS clients during heap recovery, killing the input channel exactly when the device is stressed. |
| Boot commands | leave TASK-1121's site gate, add the queue door above it | A `isGatewayFirmware()` gate at `OTGW-firmware.ino:205` would fail **closed** and break real gateways: nothing has read the banner at that point in `setup()`. TASK-1121 got the polarity right; the queue door makes it redundant but reverting it costs a merge conflict for nothing. |
| Screen placement | dedicated `.page-section` | Not a cost argument — a sixth section is three small edits, since `displayMainPage` already renders its nav from `<template id="pageNavTemplate">` (`data/index.html:44-60, 64-65`). It is a UX argument: under diagnose there is no gateway telemetry, so the main page is empty anyway. Note `webhookPage()` (`data/index.js:3140-3151`) hand-clears four ids instead of calling `setActivePageSection()`; a sixth section needs one line added there or two sections render at once. |

---

## 6. What I would not build

**A mode enum or a `bDiagnoseMode` setting.** Duplicate state that can go stale against the PIC. The banner cannot.

**A second `WebSocketsServer` on another port.** ~1.1 kB of permanent static RAM (`WebSocketsServer.h:110` statically allocates `WSclient_t _clients[5]` whether or not clients connect), for a socket the existing server can carry.

**Server-Sent Events.** Detached-client SSE is implementable on `ESP8266WebServer`, but every connect costs a 2 s `HTTP_MAX_CLOSE_WAIT` outage of the entire HTTP server, and a permanently-held socket is exactly the shape TASK-1039's rationale (`helperStuff.ino:1136-1153`) was written to prevent. It also escapes the only heap valve there is: `emergencyHeapRecovery()` releases WS and OTGWstream clients, nothing else.

**A terminal emulator.** The PIC echoes its own input and prints its own prompts. A keypad plus a `<pre>` is enough. No ANSI, no cursor addressing, no global key capture.

**Relaxing the `LL=value` validators.** Three separate reasons it is the wrong fix: it would weaken validation for all ten queue producers in gateway mode; the queue re-applies the same check independently at `OTGW-Core.ino:2962` so relaxing REST alone produces a lying 202; and `sendOTGW()` appends CR+LF unconditionally at `:3174-3176`, so even a fully relaxed path would send the Enter that ends the LED test.

**A REST hardware-reset route.** Real gap — `resetOTGW()` is reachable from MQTT and ser2net but not from the web UI — and worth its own task, but not needed for this feature once §1e restores the ser2net path. Deliberately out of scope so it does not grow this one.

**Anything for an empty or bricked PIC.** The maintainer scoped it out and Schelte is building it separately. No cost added here to accommodate it.

### Where I disagree with Schelte

Two places. Both are cases where his diagnosis is right and the implied fix would not work.

**1. "In this mode all command processing is unnecessary; only the serial passthrough to the legacy port needs to work."**

Taken literally, that invites a passthrough that reads `HardwareSerial` directly. It must not. `matchBanner()` runs **inside** `OTGWSerial::read()` (`src/libraries/OTGWSerial/OTGWSerial.cpp:855`, body at `:983-1006`), and that is the only thing in the firmware that re-detects the PIC firmware type. A direct-`HardwareSerial` passthrough would leave the ESP permanently believing the PIC is diagnose after the user reflashes `gateway.hex` — the gate would be one-way with no way out. The current loop already does the right thing (`OTGW-Core.ino:4706` reads through `OTGWSerial.read()`, and the same read fills the ser2net chunk buffer); the plan keeps it. Suppression is write-side only.

**2. "Our firmware currently blocks that with a message that a command must look like TT=7."**

Correct symptom, and `restAPI.ino:239` is the exact message. But the implied fix — relax that check — cannot work, for the reason above: `sendOTGW()` appends CR+LF below every validator, so a relaxed path delivers `7\r\n`. And the shape check he hit is not the review hardening it looks like: `git blame` shows the `isalpha` prefix tests came from review PR #511 (2026-03-21), while `cmdLen < 3 || cmdStr[2] != '='` predates it and mirrors a 2021-era guard in `addOTWGcmdtoqueue()`. Delete the review's contribution entirely and a bare digit is still rejected on length. The keystroke path has to be a separate direct-to-UART writer, which is what Stage 3 builds.

---

## 7. Open questions — one line each unblocks the work

For **Schelte**:

1. Is the diagnose firmware's Enter a bare CR (`0x0D`), or CR+LF? Decides what `POST /keys/enter` emits.
2. Does the menu need any key beyond `0`-`9` and Enter — Escape, Ctrl-C, `q` to abort a running test, Backspace? Each extra key is one entry in the allowlist.
3. Does the PIC echo typed keys back over serial? If yes the browser needs no local echo; if no, the panel must decide whether to fake one.
4. Do any tests emit bytes outside `0x20..0x7E`, or bare CR for in-place updates such as a live voltage reading? Decides sanitiser and CR handling.
5. Does the diagnose firmware reprint its banner every time it returns to the menu? If yes, `fwreportinfo()` calls `sendMQTTversioninfo()` (`OTGW-Core.ino:5158`) each time and wants throttling.

For **the maintainer**:

6. Should the gate also cover `FIRMWARE_INTF` (interface firmware)? It does not speak `PR=` either. The positive `== FIRMWARE_DIAG` test deliberately leaves it alone.
7. Is a 15 s blind window after every PIC reset acceptable for the OT Monitor tab in diagnose mode, or should `reportOTGWEvent()`'s startup suppression be bypassed when `isDiagnoseFirmware()`? Stage 2's raw tap sidesteps it for the diagnose pane; this is only about the existing log tab.
8. Should the ADR be authored now as Proposed, or after Stage 2 fixes the transport? Drafting the decision sentence now and holding Status: Proposed is my inclination.
9. Should port 25238's default flip to enabled when a diagnose PIC is detected, or stay opt-in with a documented note? I recommend staying opt-in — it is `SimpleTelnet<1>`, so auto-enabling would silently cost someone their OTmonitor session.

---

## 8. Unverified, and claims resting on a single source

**Unverified, and it gates Stage 0's severity.** Whether `detectPIC()`'s `find(ETX)` probe (`OTGW-Core.ino:574`) actually succeeds against `diagnose.hex`. ETX comes from the bootloader, which runs before the application image, so it should be firmware-independent — but I have not seen it on hardware. If it ever fails, every PIC route including `/pic` refuses permanently, which is what makes Stage 0 worth doing on its own. **One boot with `diagnose.hex` settles it.**

**Single source: Schelte's account.** The per-key semantics of the diagnose menu — that `4` and `6` change the LED count without Enter, that Enter ends a test, that the prompt ends in an untransmitted `0x1A` — come from him alone. No diagnose PIC source is in this repository; the only in-repo artefact is the banner literal at `OTGWSerial.cpp:97`. I have relied on the structural point (a burst of ASCII is consumed keystroke by keystroke, so any multi-character command is disruptive) rather than on exact per-digit outcomes, and every acceptance criterion that depends on them is marked **hardware**.

**Single source: the seven-test v2.2 menu.** The menu text in the brief is v2.2. `src/OTGW-firmware/data/pic16f1847/diagnose.hex` decodes to an earlier version with six tests. Menu items 1-6 are textually identical, so item 7 and any per-test prompt behaviour in 2.2 are inferred, not verified.

**Not verified on hardware.** Whether the post-flash bootloader `CMD_RESET` (`OTGWSerial.cpp:769-774`) reliably produces a banner the ESP catches, and therefore whether `sType` self-heals after every PIC reflash without an explicit re-probe. The code path exists and users already see the new PIC version after an ordinary gateway-to-gateway flash, which is indirect evidence it works; I did not trace the timing against `_upgrade`'s deletion at `OTGWSerial.cpp:1029`.

**Working-tree caveat.** `src/OTGW-firmware/OTGW-Core.ino` and `OTGW-firmware.ino` are modified and uncommitted (TASK-1121, agent `fw-transparency`). `data/pic16f1847/gateway.hex`, `version.h` and `data/version.hash` are also dirty. Any agent implementing this must stage explicit paths, never `git add -A`.

**Two live pre-commit gates touch these files and must keep passing.** ADR-095 requires the literal `OTGW_PASSTHRU_CHUNK` in `src/OTGW-firmware/OTGW-Core.ino` (verified present at `:4653`); ADR-086 requires `checkApiRateLimit` in `src/OTGW-firmware/restAPI.ino` (verified present at `:913`). Neither is at risk from this plan, but neither symbol may be removed.

**ADR guidance.** One ADR, Proposed only, never self-accepted. Its decision is one sentence: *when the PIC runs non-gateway firmware, the ESP relinquishes the serial link and acts as a transparent terminal.* Anchor it on ADR-095, whose Consequences already state that "byte transparency of the pipe is not the same as exclusive ownership of the link", and which explicitly scopes link ownership out. Its Enforcement block must be **empty or `llm_judge` only** — a `require_pattern` saying the ESP never writes in diagnose mode would fail on day one against the deliberately ungated `PR=A` probe at `OTGW-firmware.ino:295`, which carries a written rationale for staying ungated.