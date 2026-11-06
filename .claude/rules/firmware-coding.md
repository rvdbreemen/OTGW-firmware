---
paths:
  - "src/**"
  - "test/**"
  - "safeboot/**"
---

# Firmware coding rules

Loaded when Claude reads or edits a file under `src/`, `test/` or `safeboot/` (TASK-1631). The short form is the AGENT-CORE block in CLAUDE.md.

## Critical coding rules

### PROGMEM — ALL string literals must stay in flash

```cpp
DebugTln(F("Message"));                        // F() macro
DebugTf(PSTR("Value: %d\r\n"), value);         // PSTR() for printf
snprintf_P(buf, size, PSTR("fmt: %s"), str);   // snprintf_P always
```

String comparisons: `strcmp_P()`, `strcasecmp_P()` with `PSTR()`.
Binary data: `memcmp_P()` only — **never** `strncmp_P`/`strstr_P` on binary (causes Exception (2) crash).

### No ArduinoJson — ever

JSON built manually with `snprintf_P` / `sendJsonMapEntry`. Parsed with `parseJsonKVLine()`.

### No String class in hot paths (ADR-180)

Use `char[]` with `strlcpy`, `snprintf_P`. `String` only in setup/one-off contexts.

### File serving — stream, never load into RAM

Files >2 KB: `httpServer.streamFile()`. `v2.html` is ~50 KB — never `f.readString()`.

### HTTP/WS only — never add HTTPS/WSS

Trusted LAN only. REST API works behind HTTPS reverse proxy, but WebSocket assumes plain WS.

### ESP Platform Abstraction — no raw `#ifdef ESP8266`/`ESP32` outside the abstraction layer

The 2.0.0 branch carries an explicit platform abstraction so that
application code is written against shims and capability flags, never
against raw platform symbols. **No `#if(def) ESP8266`, `#if(def) ESP32`,
`#if(def) ARDUINO_ARCH_ESP*`, or `#if(def) BOARD_NODOSHOP_ESP*` may appear
outside the allowlisted abstraction files.**

Allowlisted files (the only place these conditionals belong; the list in
`evaluate.py::ESP_ABSTRACTION_ALLOWED_FILES` is authoritative). The abstraction is the project's own Platform library under
`src/libraries/Platform/src/`:

- `src/libraries/Platform/src/platform.h` — dispatcher
- `src/libraries/Platform/src/platform_esp32.h` — ESP32 includes, shims, type aliases
- `src/libraries/Platform/src/boards.h` — pin maps, `HAS_*` capability flags and per-board tuning constants
- `src/OTGW-firmware/OTGW-ModUpdateServer{.h,-esp32.h}` — parallel mini-abstraction for the firmware update server

**The other libraries under `src/libraries/**` are vendored upstreams and OUT OF SCOPE entirely.**
They are independent upstreams that manage their own platform support and are
NOT firmware application code. Do NOT pull them into the abstraction: never add
`platformXxx()` shims into them and never strip their internal
`#if defined(ESP8266)/(ESP32)`. Examples: `src/libraries/SimpleTelnet`,
`src/libraries/OTGWSerial` (PIC serial driver). They are listed in
`evaluate.py::ESP_ABSTRACTION_EXCLUDED_LIB_DIRS` and excluded from the
abstraction scan. (Maintainer directive, 2026-05-30.)

Application code MUST instead:

1. **Call `platformXxx()` shims** from `platform_esp32.h` for any divergent API
   (heap, hostname, MAC, reset, NTP, LED, JSON tx, WiFi, BLE, …). If a
   shim does not exist yet for a divergence you need, *add the shim first*
   in `src/libraries/Platform/src/platform_esp32.h` — with an inline no-op
   stub where a board lacks the feature — and then call it unguarded from
   application code.
2. **Gate optional features with `HAS_*` flags** from `boards.h`
   (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`,
   `HAS_SAT_BLE`, `HAS_WEATHER_FORECAST`, etc.). If your feature does not
   yet have a flag, add one to `boards.h` first.
3. **Never read raw board macros (`BOARD_NODOSHOP_ESP32`, …) outside
   `boards.h`.** Those decide which `HAS_*` flags are set; the rest of
   the firmware sees only the `HAS_*` flags.
4. **Never call `ESP.getXxx()` directly** when a `platformXxx()` shim
   already exists (`platformFreeHeap`, `platformMinFreeHeap`,
   `platformMaxFreeBlock`, `platformHeapFragmentation`, `platformRestart`,
   `platformChipId`, `platformFlashChip*`, etc.). Skipping the shim is a
   quieter form of the same leak — it bypasses the platform's substitution
   point.

When adding a new feature that diverges per platform, the **first** thing
to write is the shim or the `HAS_*` flag, not the application code that
needs it. The platform headers are the public API of the abstraction; the
.ino files are clients.

`evaluate.py::check_esp_abstraction_boundary()` enforces this rule with a
baseline-ratchet: it FAILs on any new violation above the recorded
`ESP_ABSTRACTION_BASELINE` and WARNs while any pre-existing violation
remains. The current baseline and remediation roadmap live in
`docs/audits/2026-05-28-esp-abstraction-leak-audit.md` (TASK-739) and the
tier tasks TASK-740..746. **Each tier task must lower
`ESP_ABSTRACTION_BASELINE` in `evaluate.py` as part of its Definition of
Done.**

### Architecture rules

- PIC commands: always `addCommandToQueue()`, never direct serial write
- SAT's own `CS=` (the `CS=0` releases included) goes through `satEnqueueOwnCS()`. While SAT is enabled it owns MsgID 1, and both the OTDirect handler (ADR-179) and the PIC command queue (`addCommandToQueue()`, TASK-1236) refuse any unmarked `CS=` as external, SAT's own included. On PIC boards `satPicCSKeepalive()` re-sends SAT's CS every 30 s, because the PIC drops a CS= after ~63-126 s without a new one
- Timers: `DECLARE_TIMER_SEC()` / `DECLARE_TIMER_MS()` + `DUE()` (see `safeTimers.h`)
- `doBackgroundTasks()` can re-enter (called from inside `doAutoConfigure()`'s file-reading loop). Shared scratch state must declare an acquisition contract: see ADR-090 for the pattern (RAII or inUse-flag, both with fail-safe on contention)
- Typed control flow: `enum class` or numeric IDs, never string tokens as discriminators
- Frontend JS: Chrome/Firefox/Safari (latest + 2). Check element existence, try-catch JSON.parse, response.ok, .catch() on all async
- **At most 2 concurrent API calls (hard cap, NEVER burst above it).** The device's REST backpressure gate (`restAPI.ino`, `REST_MAX_INFLIGHT`) and file-serve gate (`WEB_FILE_MAX_INFLIGHT`) are both capped at 2 — empirically confirmed as the highest offered concurrency with zero 503s under nominal load and zero incidents under combined-stress/overload (ADR-165, TASK-1015 two-phase load-test study). Both gates tighten toward 1 under heap pressure regardless. No `Promise.all` / `.map()` over `fetch` for anything that can exceed 2 in flight — a burst above the cap gets a cheap 503, which surfaces as silent partial saves (the SAT onboarding "Could not save the settings, SAT was not enabled" bug, TASK-1014). This applies to GETs too, not just POSTs. Existing sequential-chain code (`kv.reduce((c,p)=>c.then(()=>post(...)), Promise.resolve())`, one request at a time) stays as-is — it's safely within the new N<=2 ceiling, and no client-side `MAX_INFLIGHT` knob has been built to actually raise it (ADR-165 deliberately left that undone). Note: `post()` fires the fetch eagerly when called, so if you ever do allow up to 2 in flight, serialize by deferring the *call* (build `[name,value]` pairs, dispatch up to 2 at a time), not by awaiting a pre-built array of already-launched promises. The v2 bundle enforces the cap centrally (ADR-184's queue, carried into `v2.js` by TASK-1210). The first lines of `v2.js` wrap `window.fetch`, so every same-origin `/api/` request waits in one first-in-first-out queue with at most 2 in flight. Bursts there no longer reach the device, but they still queue and delay each other, so keep writing calls one at a time. A 503 that still shows on page load comes from the connection and file-serve gates (ADR-188, ADR-147), not from more than 2 API calls.
- Webui sources live in `src/OTGW-firmware/data/`; `python build.py` (not `--firmware`) minifies them in a staging copy (ADR-190) and builds each page as one gzipped file with its CSS, JavaScript and fonts inlined (`scripts/webui_pages.py`, ADR-215, Proposed): `v2.html.gz` (sources `v2.html`, `ds-tokens.css`, `v2.css`, `v2.js`, `fonts/`) and `FSexplorer.html.gz` (sources `FSexplorer.html`, `fsexplorer.css`). Only the gzipped pages ship. The firmware update pages are `src/OTGW-firmware/webui/*.html`, gzipped at build time into the generated `updateServerHtml.h`. To try a web UI change on a running unit, push it with `scripts/ota_flash.py --webui` (never hand-upload source files). FSexplorer is the recovery page and must not depend on the v2 files. The 2.0.0 line ships only the v2 UI (ADR-192); the classic UI's dropped features are recorded in `docs/history/2026-10-15-retired-classic-web-ui-features.md`
- Log container contract: `.ot-log-content` has `white-space: pre` (components.css); `\n` is the line separator. Prefer `textContent` over `innerHTML` for plain text (skips HTML parser and per-line escape)

### Naming conventions

- Variables/functions: camelCase (`settingHostname`, `startWiFi`)
- Constants: UPPER_CASE (`CMSG_SIZE`)
- Global settings: `setting` prefix (`settingMqttBroker`)

---

## Settings & State Architecture (ADR-051)

- `OTGWSettings settings` — persistent, serialized to LittleFS
- `OTGWState state` — transient, never persisted
- Two-level sub-sections, Hungarian prefixes: `b`=bool, `s`=char[], `i`=int, `f`=float
- Access: `settings.mqtt.sBroker`, `state.otgw.bOnline`

---

## REST API

- `/api/v2/` — current. Dispatch table `kV2Routes[]` in `restAPI.ino`.
- Errors: `sendApiError(httpCode, F("message"))`

---

## Important Constraints

- Never write to `Serial` after OTGW init (it's the PIC serial link)
- Never flash PIC firmware over WiFi using OTmonitor (bricks the PIC)
- **Firmware OTA on 4MB boards goes through the safeboot updater (ADR-191).** The 4MB table has a safeboot app/factory partition (0x10000, 960K), one app slot app0/ota_0 (0x100000, 2240K) and LittleFS (0x330000, 768K). The main firmware never writes ota_0 itself: `_handleUploadStart` (`OTGW-ModUpdateServer-esp32.h`) still refuses an app image (it would overwrite the running app, TASK-959). Instead POST /safeboot (admin-protected) erases otadata and restarts into safeboot (`safeboot/src/main.cpp`), which writes ota_0 and restarts into it. Bootloader app rollback is on: a new image stays pending until 30 s of healthy loop() or a planned restart (`safebootMarkAppValid()`), so an early crash returns to safeboot. Filesystem OTA works from the main firmware as before. Units on the old layout (app0 @0x10000) need one USB `flash_otgw --migrate`; `flash_otgw --update` reads the unit's partition table and refuses a layout mismatch. The offsets live in `partitions_otgw_esp32.csv` and are mirrored in build.py, flash_otgw.bat/.sh and flash_esp.py; `tests/test_build_safeboot.py` fails when they drift. Dual-slot app OTA on an 8MB module still works through `hasSpareAppOtaSlot()`.
- Never add HTTPS/WSS
- Always `addCommandToQueue()` for OTGW commands
- Always validate buffer sizes before string operations
- Always `feedWatchDog()` in long-running loops

---

## Debugging User-Reported Issues

When investigating a user-reported bug, behavioural deviation, or "this should work" complaint, follow the reference precedence in this order:

1. **Read the OpenTherm specification first.** `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` is the ground truth. The spec defines what the protocol *says* should happen. Anchor every investigation here before opening implementation code, regardless of how confident you feel about the topic.
2. **Then consult `other-projects/`.** Read `other-projects/README.md` and `other-projects/CLAUDE.md` before quoting any reference implementation. Those files document the precedence ladder inside that directory: Schelte Bron's projects (otgw, otmonitor, otgwmcu) for PIC and otmonitor protocol authority; the HA `opentherm_gw` component for user-perceived behaviour; `pyotgw` for the engine under HA; OT-Thing and SAT for alternative implementations.
3. **Never modify code under `other-projects/`.** It is upstream reference, read-only by convention. The full rule and rationale live in `other-projects/CLAUDE.md`.

**Spec wins on design decisions.** When the spec is explicit and unambiguous, our firmware matches the spec, even if a downstream client behaves differently.

**HA component behaviour matters for understanding the report.** Most user complaints in practice are "this differs from what Home Assistant shows me", not "this differs from the OpenTherm spec". Match the report against the HA component to triage; match the design against the spec to decide.

**When spec-correct and HA-perceived behaviour conflict**, capture the trade-off as an ADR (`docs/adr/`). Do not silently align with one or the other; the next maintainer needs to know we made the call deliberately.
