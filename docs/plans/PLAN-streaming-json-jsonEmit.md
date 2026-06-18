# PLAN — Embedded-robust streaming JSON writer (`JsonEmit`) for the OTGW32 REST API, with an A/B experiment to decide ADR-141's fate

Status: build-ready. Target: `feature-2.0.0-esp32s3-async` (OTGW32, ESP32-S3, AsyncWebServer). Backlog: create a task before any code (project rule); the natural parent is the ArduinoJson-migration line (ADR-141) and the load-test line (TASK-883/879).

---

## 1. GOAL + PRINCIPLE

**Goal.** Replace the ArduinoJson v7 `JsonDocument` serialization layer on the OTGW32 REST API with a header-only, non-throwing, no-materialized-document streaming writer (`JsonEmit`) that emits live firmware state directly into the response sink in medium chunks — then run a controlled A/B experiment to decide, on hard numbers, whether to keep ArduinoJson (ADR-141), go hybrid (streaming for the heavy endpoints only), or fully revert.

**Principle — embedded robustness over throughput.**
- **Non-throwing.** Every byte path discards the sink's return value and keeps going. No `new`, no `String` growth in the hot path, no `throw`, no retry, no block. A full sink truncates the wire; the firmware never crashes and the writer's own bookkeeping stays balanced.
- **No materialized document.** No `JsonDocument`, no contiguous serialize buffer built by the writer. State is emitted field-by-field as it is read.
- **Medium-chunk direct writes.** One `out.print(...)` per field / per escaped segment (tens to ~1460 bytes), never byte-at-a-time. This is the single load-bearing constraint: byte-at-a-time is what produced the ~8600x `"Failed to allocate"` log storm that starved `async_tcp` past the 30 s IDF Task-Watchdog and rebooted the device under load (alpha.211). Medium chunks cap that at ~170 logs for an 8.6 KB body — the alpha.204-proven survivable regime.
- **Stable > fast.** O(n) single emit pass, sub-ms, no ArduinoJson pool churn. The win we are buying is CPU + simplicity + removal of a contiguous-pool failure *class*, not a memory headline.
- **Survives 8–16 concurrent workers** for a 2-minute unthrottled flood with 0 reboots, behind the existing `REST_MAX_INFLIGHT=4` gate, while remaining byte-truncate-and-log (not reboot) even with the gate lifted.

**Honest scope of the claim (do not oversell).** Single-pass-into-`AsyncResponseStream` *reintroduces* one contiguous `cbuf` per response — the exact structure ADR-145 removed. Its safety is **graceful degradation** (non-throwing `cbuf::resize` → truncate-and-log), **not** lower memory. Stated plainly so the plan is not read as a silent reversal of ADR-145.

---

## 2. ARCHITECTURE

### 2.1 Chosen sink: single-pass into `AsyncResponseStream`, no document

`JsonEmit` is bound to `Print&` and never knows its sink (the `jsonEscapeTo(Print&, ...)` signature already established this). The **wiring** is single-pass: one `AsyncResponseStream` per response, the writer emits every field once, in document order, directly into it.

```cpp
AsyncResponseStream* s = restBeginStream("application/json");   // webServerCompat.h:342
if (s) { JsonEmit je(*s); /* ...field calls... */ }
restFinalize();                                                  // webServerCompat.h:351
```

**Why single-pass and not chunked-re-run (the volatile-field reason chunked-pull is rejected for a no-doc producer).**

`beginChunkedResponse` is a *pull* contract: the server calls the filler repeatedly with a cumulative byte `index`, and byte N must be **identical** regardless of which chunk-pass emits it. ADR-145's existing path satisfies this *only because it re-serializes an immutable `JsonDocument` snapshot*. A no-doc producer that re-reads **live firmware state** on each of the ~6 passes of an 8.6 KB body breaks byte-stability in two independent, deterministic ways:

- **(a) Framing corruption.** A volatile field whose decimal width changes mid-stream (`freeheap 100000→99999`, 6→5 digits; `uptime 9999→10000`; `millis()` digit rollover) shifts every subsequent byte relative to `index`. The window desynchronizes from the server's expected offset → bytes duplicated or dropped at the seam → invalid/truncated JSON.
- **(b) Value inconsistency even with stable widths.** Chunk 1 reads `uptime=100`, chunk 6 reads `uptime=106`. Syntactically valid, semantically a Frankenstein document assembled from different instants.

The only way to make chunked-re-run correct is to snapshot all volatile fields up front and cache a measured length — which re-invents the document we set out to remove. **Chunked-re-run is therefore rejected on correctness, not cost.** Single-pass emits each volatile field exactly once: no second read, no width-shift, no cross-instant mix.

**What makes single-pass safe despite the contiguous buffer** (three verified primitives):
1. **Non-throwing `cbuf::resize`** (`framework-arduinoespressif32/.../cbuf.cpp:91-96,101-106`): on `xRingbufferCreate` failure it logs once and `return _size;` (old size) — never `throw`, never `new`. No `std::bad_alloc` originates here.
2. **Truncate-and-continue write** (`ESPAsyncWebServer/.../WebResponses.cpp:926-943`): `AsyncResponseStream::write(data,len)` re-checks `room()` after `resizeAdd`, logs `"Failed to allocate"` **once per write call** (not per byte), then writes only what fits. Medium chunks ⇒ ~170 logs/8.6 KB on a starved heap, which the device absorbs; byte-at-a-time ⇒ ~8600 logs ⇒ watchdog reboot (`write(uint8_t)` is `return write(&data,1)`, `WebResponses.cpp:945-947`).
3. **The `REST_MAX_INFLIGHT=4` gate** (`restAPI.ino:34,2103-2110`): checked and 503'd *before any JSON is built*, balanced by `onDisconnect` under `Connection: close`. Peak heavy-response heap is therefore **flat at ~34 KB (≤4 × ~8.6 KB) against ~95 KB heap regardless of worker count** (8/12/16 change only the count of cheap 503s).

**Residual uncatchable locus (unchanged by sink choice, documented for honesty).** `AsyncWebServerResponse::addHeader` (`WebResponses.cpp:141,165`) inserts into `std::list<AsyncWebHeader>` — a list-node + `String` alloc in the deferred async `_respond`/`_assembleHead` path, *after* the handler returns, so it is not reachable by any handler-level `try/catch` (ADR-145 already documents this). It is identical for streaming, ArduinoJson, and the 503 path. We do not eliminate it; the bounded ~34 KB peak keeps it dormant in the success regime.

### 2.2 The `JsonEmit` writer (ready to implement)

Header-only, single new file `src/OTGW-firmware/jsonEmit.h`. No allocation anywhere; every buffer is a fixed stack array.

**Structure / comma tracking.** `uint8_t _depth` + `uint16_t _firstMask` (one bit per nesting level; bit *d* set ⇒ next element at level *d* is the first ⇒ no leading comma). `MAX_DEPTH = 8` (real payloads nest 3–4 deep; 8 fits the 16-bit mask with margin). A `_suppressSep` one-shot latch lets `key()` consume the separator so the following value/container does not re-emit one — comma logic lives in exactly one place (`_sep()`), and `value()` works both standalone (in an array) and key-attached (in an object).

**Non-throwing contract, restated as code invariant.** Every `out.print(...)`/`out.write(...)` return value is discarded. `_depth`/`_firstMask` update regardless of how many bytes the sink accepted, so the state machine stays consistent even when the wire truncates. Two sticky flags make truncation observable without altering control flow:
- `_bDepthError` — set if `beginObject/beginArray` is called at `depth >= MAX_DEPTH`; the over-deep container is **dropped** (mask never indexed past its width) and its matching `end*` is also dropped, so brackets stay balanced. `ok()` returns `false`.
- The writer never reads back from the sink — a full sink is invisible to it by design (that *is* the non-throwing guarantee).

**Method set.**
- Structure: `beginObject() / beginObject(k) / endObject()`, `beginArray() / beginArray(k) / endArray()`.
- Keys (escaped, runtime `const char*`): `key(const char* k)`.
- Scalars: `value(bool)`, `value(int32_t)`, `value(uint32_t)`, `value(float, uint8_t decimals=3)`, `value(const char*)` (escaped; `nullptr → null`).
- Raw numeric token passthrough: `raw(const char* json)`, `rawP(PGM_P)` — for `sendDeviceSettings`'s `addNum` only.
- Conveniences: `field(k, …)` for each scalar type, `fieldRaw(k, json)`.
- Narrow-int forwarding overloads (`int`, `unsigned`, `int8_t/16_t`, `uint16_t`) guarded by the same `PLATFORM_INT_DISTINCT_FROM_INT32` macro the old code used, to avoid ambiguous-overload errors on Xtensa where `int32_t == long`.
- `bool ok() const` — `false` if any container was dropped.

**Float format (pinned).** Default **3 decimals** (old `sendJsonMapEntry(float)`, `jsonStuff.ino:388`); **1 decimal for Dallas temps** (old `…DallasTemp`, `jsonStuff.ino:514`). Rationale: those exact `%.Nf` formats were already proven semantically equal to ArduinoJson by the green migration run, so re-using them inherits the proof. **NaN/Inf → `null`** (matches ArduinoJson `restAPI.ino:820-822`; a `dtostrf`/`%.1f` writer that emits `nan`/`inf` produces JSON that `json.loads` throws on).

```cpp
class JsonEmit {
public:
  static constexpr uint8_t MAX_DEPTH = 8;
  explicit JsonEmit(Print& out) : _out(out), _depth(0), _firstMask(0), _bDepthError(false) {}

  void beginObject()              { _openContainer('{'); }
  void beginObject(const char* k) { key(k); _openContainer('{'); }
  void endObject()                { _closeContainer('}'); }
  void beginArray()               { _openContainer('['); }
  void beginArray(const char* k)  { key(k); _openContainer('['); }
  void endArray()                 { _closeContainer(']'); }

  void key(const char* k) {
    _sep();
    _out.print('"'); jsonEscapeTo(_out, k); _out.print(F("\":"));
    _suppressSep = true;                       // value that follows is attached
  }

  void value(bool b)     { _sep(); _out.print(b ? F("true") : F("false")); }
  void value(int32_t v)  { char b[12]; snprintf_P(b,sizeof b,PSTR("%ld"),(long)v);          _sep(); _out.print(b); }
  void value(uint32_t v) { char b[12]; snprintf_P(b,sizeof b,PSTR("%lu"),(unsigned long)v); _sep(); _out.print(b); }
  void value(float f, uint8_t decimals = 3) {
    _sep();
    if (isnan(f) || isinf(f)) { _out.print(F("null")); return; }
    char fmt[8]; snprintf_P(fmt,sizeof fmt,PSTR("%%.%uf"),(unsigned)decimals);
    char b[24];  snprintf(b,sizeof b,fmt,(double)f); _out.print(b);
  }
  void value(const char* s) {
    _sep();
    if (!s) { _out.print(F("null")); return; }
    _out.print('"'); jsonEscapeTo(_out, s); _out.print('"');
  }
  void raw(const char* json) { _sep(); _out.print(json); }
  void rawP(PGM_P json)      { _sep(); _out.print(FPSTR(json)); }

  void field(const char* k, bool b)                   { key(k); value(b); }
  void field(const char* k, int32_t v)                { key(k); value(v); }
  void field(const char* k, uint32_t v)               { key(k); value(v); }
  void field(const char* k, float f, uint8_t dec = 3) { key(k); value(f, dec); }
  void field(const char* k, const char* s)            { key(k); value(s); }
  void fieldRaw(const char* k, const char* json)      { key(k); raw(json); }

  bool ok() const { return !_bDepthError; }

private:
  void _sep() {
    if (_suppressSep) { _suppressSep = false; return; }
    if (_depth > 0) {
      if (_firstMask & (1u << _depth)) _firstMask &= ~(1u << _depth);
      else                             _out.print(',');
    }
  }
  void _openContainer(char brace) {
    if (_depth >= MAX_DEPTH) { _bDepthError = true; return; }  // drop, keep balanced
    _sep(); _out.print(brace); ++_depth; _firstMask |= (1u << _depth);
  }
  void _closeContainer(char brace) {
    if (_depth == 0) return;                                   // unbalanced close: ignore
    --_depth; _out.print(brace);
  }

  Print&   _out;
  uint8_t  _depth;
  uint16_t _firstMask;
  bool     _suppressSep = false;
  bool     _bDepthError;
};
```

**`jsonEscapeTo` — full non-throwing source.** Byte-for-byte the old escape set (`git show 0a3e5eee:src/OTGW-firmware/jsonStuff.ino:26-103`: 7 named escapes + `\u%04X`, 24-byte accumulate-then-flush = medium chunks) with **one deliberate correction**: the control-char test is on `(unsigned char)` so bytes 0x80–0xFF (UTF-8 lead/continuation) pass through raw instead of being mis-escaped as `\u00XX`. The old signed `*p < 0x20` test diverged from ArduinoJson on any non-ASCII string when `char` is signed on Xtensa.

```cpp
inline void jsonEscapeTo(Print& out, const char* src) {
  if (!src) return;
  char chunk[24]; size_t chunkIdx = 0;
  for (const char* p = src; *p; ++p) {
    const char* esc = nullptr; char hex[7];
    switch (*p) {
      case '"':  esc = "\\\""; break;
      case '\\': esc = "\\\\"; break;
      case '\b': esc = "\\b";  break;
      case '\f': esc = "\\f";  break;
      case '\n': esc = "\\n";  break;
      case '\r': esc = "\\r";  break;
      case '\t': esc = "\\t";  break;
      default:
        if ((unsigned char)*p < 0x20) {                       // CORRECTION: unsigned
          snprintf_P(hex, sizeof(hex), PSTR("\\u%04X"), (unsigned char)*p);
          esc = hex;
        }
        break;
    }
    if (esc) {
      const size_t escLen = strlen(esc);
      if (chunkIdx + escLen >= sizeof(chunk)) { chunk[chunkIdx] = '\0'; out.print(chunk); chunkIdx = 0; }
      memcpy(chunk + chunkIdx, esc, escLen); chunkIdx += escLen;
    } else {
      if (chunkIdx + 1 >= sizeof(chunk))       { chunk[chunkIdx] = '\0'; out.print(chunk); chunkIdx = 0; }
      chunk[chunkIdx++] = *p;                                 // raw byte incl. 0x80-0xFF
    }
  }
  if (chunkIdx > 0) { chunk[chunkIdx] = '\0'; out.print(chunk); }
}
```

**Per-project safety note for the escape scan:** it is a `char*` string-walk terminated on `'\0'`. Never point it at a non-NUL-terminated binary buffer (project rule: binary uses `memcmp_P` only; `strstr_P`/`strncmp_P` on binary → Exception 2). All call sites pass C strings, which is correct.

---

## 3. SCOPE — endpoint inventory

### 3.1 Convert first: the HEAVY set (>2 KB — the pressure sources)

Order by pressure and by how much type-surface each one forces:

| # | Endpoint | Handler:line | ~Size | Forces these surface features |
|---|---|---|---|---|
| 1 | GET `/api/v2/settings` | `sendDeviceSettings` restAPI.ino:2839 | **~7.3 KB** | **raw numeric passthrough** (`addNum` 2872, ~30 float fields emit `"value":21.5` unquoted while `type:"f"` stays a string) + runtime per-setting keys + `#if HAS_*` blocks + `password=NN` placeholder strings |
| 2 | GET `/api/v2/debug` | `handleDebugDump` restAPI.ino:1694 | **~6.4 KB** | ~190 flat dotted keys (`settings.sat.*`), all scalar types, platform `#if` tail |
| 3 | GET `/api/v2/sat/status` | `satSendStatusJSON` SATcontrol.ino:2006 | **~2.7 KB / 132 fields** | **dynamic `area_%u_*` runtime keys** (2206) + **nested array-of-object** `last_blocked_cmds` (2163) + NaN→null floats + appended `ble_*` via `satBLESendStatusJSON(o)` (SATble.ino:691) |
| 4 | GET `/api/v2/device/info` | `sendDeviceInfoV2` restAPI.ino:2376 | **~2.3 KB** | ~95 flat keys, String/IP values (copied), 503-guard on low heap |
| 5 | GET `/api/v2/otgw/boiler-support` | `handleOtgw` restAPI.ino:648/660 | up to ~6 KB ceiling | **two unbounded 0..255 arrays-of-object** (`unsupported_read` + `unsupported_write`) |
| 6 | GET `/api/v2/otgw/ot-support` | `handleOtgw` restAPI.ino:682 | up to ~6–8 KB ceiling | **one unbounded 0..255 array-of-object** (`msgids`); compact (set bits only) so typical ≪ ceiling |
| 7 | GET `/api/v2/otgw/otmonitor` (+`/telegraf`) | `sendOTmonitorV2` restAPI.ino:2257 | ~2.2 KB+ | **Dallas-address dynamic keys** (`String(addr)` 2347), `{value,unit,epoch}` nested objects, OTStateLock |
| 8 | GET `/api/v2/sat/weather` | `weatherSendStatusJSON` SATweather.ino:560 | ~1–2.5 KB | **arrays-of-scalar** (4×24 `forecast_*`, ESP32 `HAS_WEATHER_FORECAST` only) + NaN→null |

The three unbounded `0..255` arrays (items 5×2 + 6) are the structural stress test for array-of-object emission; `otmonitor`/`sensors`/`settings` are the stress test for dynamic runtime keys; `settings` alone forces the `raw` passthrough.

### 3.2 Leave on ArduinoJson (LIGHT set, <2 KB)

`sensors`, `sat/diagnostics`, `sat/ble` roster, `otdirect/overrides`, `otgw/overrides`, `network/scan`, `otdirect/settings`, `otdirect/status`, `otgw/messages/{id}`, `otgw/label/{label}`, `health`, `pic/settings`, `device/crashlog`, `pic/flashstatus`, `pic/update`, `filesystem/check`, `flash`, `device/time`, `sat/weather/needs-setup`, and the two FSexplorer **root-as-array** lists (`apifirmwarefilelist`, `apilistfiles`). **None introduces a type or structure the heavy set doesn't already require** — so they can stay on ArduinoJson indefinitely (this is exactly the hybrid outcome, if the experiment lands there).

### 3.3 The complete surface `JsonEmit` MUST cover

**Scalars:** `bool` (real unquoted JSON bool — the old `CBOOLEAN` string-bool was the *broken* case), signed `int32`, unsigned `uint32`/`unsigned long`, native `float` with **NaN/Inf→null**, `const char*`/`String`/`char[]` (escaped, copied), and a **raw pre-serialized numeric token** (settings `addNum` only — this is the scoping lever: convert settings ⇒ need `raw`; defer settings ⇒ drop `raw`).

**Structures:** root-as-object (most), **root-as-array** (FSexplorer — but those stay on ArduinoJson, so root-array is *not* required for the first heavy wave), nested object (2 levels: settings, otmonitor), **array-of-objects** (the three 0..255 arrays, `last_blocked_cmds`), **array-of-scalars** (`forecast_*`), and **dynamic runtime keys** (`area_%u_*`, Dallas `String(addr)`, per-setting names) — keys are runtime `const char*`, never required to be `F()`/PROGMEM.

**Minimal sufficient surface:** `bool / int32 / uint32 / float(NaN→null) / string(escaped) / raw-number-token / object / array`, nestable, root-object, runtime-string keys. The first heavy wave (items 1–4 + 5/6 arrays + 7) exercises *all* of it.

---

## 4. BUILD STEPS — ordered, incremental, each independently testable

Backlog first (`backlog task edit <id> -s "In Progress" -a @claude`). Bump prerelease per the policy on every firmware-touching commit (`bin/bump-prerelease.sh`). Build full image every time (`python build.py`, not `--firmware` alone — the LittleFS data carries the web UI). Evaluator green (`python evaluate.py --quick`) before each push.

**Step 0 — Header + golden harness, no behavior change.**
- Add `src/OTGW-firmware/jsonEmit.h` (§2.2) and a host-side unit harness that runs `jsonEscapeTo` and `JsonEmit` against fixed inputs (compile the header for host where feasible, else a tiny on-device `/api/v2/_jsontest` scaffold behind a build flag). Verify: escaping of `"` `\` `\n` `\r` `\t` `\u00XX`, UTF-8 0x80–0xFF passthrough, empty `{}`/`[]`, single-element array (no separator), all-fields-suppressed object, depth-overflow drops-but-balances, NaN/Inf→null, float 3dp/1dp. **Gate: items 1–7 of §6 all pass before any endpoint is converted.**
- `python build.py` clean; `python evaluate.py --quick` green. Commit (header only — no endpoint touches the new code path yet).

**Step 1 — Convert one heavy endpoint end-to-end: `device/info` (item 4).** Smallest of the heavy set, no `raw`, no dynamic keys, no unbounded array — the cleanest first conversion to shake out the wiring.
- Rewrite `sendDeviceInfoV2` to `restBeginStream` + `JsonEmit` + `restFinalize`, keeping the 503 low-heap guard.
- `python build.py` → flash OTGW32.
- **`json_golden compare`** for `/api/v2/device/info` (A build vs the ArduinoJson B build): expect **0 FAIL, 0 missing keys**; byte-diffs from whitespace/key-order are informational.
- One quick load smoke (8 workers, 1 min) to confirm no regression. Commit (own prerelease tag).

**Step 2 — `sat/status` (item 3).** Forces dynamic `area_%u_*` keys + nested array-of-object + NaN→null + the `satBLESendStatusJSON(o)` append. This proves the hardest *structure* surface.
- Convert; `python build.py`; flash; `json_golden compare` for `/api/v2/sat/status`; byte-check floats + bools (§6 items 1–2) on this endpoint specifically. Commit.

**Step 3 — `settings` (item 1).** Forces `raw` numeric passthrough (`fieldRaw`/`raw`) + per-setting runtime keys + `#if HAS_*` gated blocks. This proves the hardest *type* surface.
- Convert `addNum` to `je.fieldRaw(name, token)`; keep `password=NN` placeholders as escaped strings. `python build.py`; flash; `json_golden compare`; byte-check the ~30 raw float fields emit unquoted numbers with `type:"f"` still a string. Commit.

**Step 4 — `debug` (item 2).** Bulk flat-key conversion; mechanical after Steps 1–3. Convert; build; flash; `json_golden compare`. Commit.

**Step 5 — The unbounded arrays: `otmonitor` (7), `boiler-support` (5), `ot-support` (6).** Array-of-object 0..255 stress + Dallas dynamic keys + OTStateLock held across the emit. Convert one per commit; each: build, flash, `json_golden compare`, strict-parse the full 0..255 expansion. Commit each.

**Step 6 — `sat/weather` (8, ESP32 forecast arrays).** Array-of-scalar + NaN→null. Convert; build; flash; `json_golden compare`. Commit.

**Step 7 — Full-surface golden sweep.** `json_golden compare` across **all 31 endpoints** (converted + still-ArduinoJson): assert **0 FAIL, 0 missing/extra keys**, **0 `json.loads` parse errors**. This is the A-build's correctness gate and the precondition for the A/B matrix (§5).

After each step the tree is buildable, flashable, and golden-clean — you can stop at any step and ship a partial (hybrid) conversion.

---

## 5. A/B EXPERIMENT — does the serialization *approach* change robustness under load?

### 5.1 The one trap that invalidates everything if you get it wrong

**A is a JSON-layer revert on alpha.214 HEAD, version-bumped (suggest `alpha.215-streamtest`) — NOT a `git checkout` of alpha.204.** alpha.204 differs from 214 by ~10 alphas of unrelated change (partition rebalance for ArduinoJson, methodCompat two-enum fix, the gate+backstop, TWDT loop-stall work). Checking out 204 measures "204 vs 214", not "streaming vs ArduinoJson". **alpha.204 is a positive control only** (a known-good streaming data point), never the experimental arm. The A build is literally the artifact a partial/full ADR-141 revert would ship — same partition table, same methodCompat, same heap floors, same everything-else as B. The experiment cannot start until A exists and passes the §4 Step-7 golden sweep.

### 5.2 Four builds (two axes: approach × gate)

| Build | JSON approach | Gate (`REST_MAX_INFLIGHT`) | try/catch backstop | Heap floors |
|---|---|---|---|---|
| **A-raw** | Streaming (no doc) | **OFF** (`-DREST_MAX_INFLIGHT=255`) | OFF | **ON** |
| **A-ship** | Streaming | **ON** (`=4`) | OFF (no-op for A) | ON |
| **B-raw** | ArduinoJson chunked | **OFF** (`=255`) | **OFF** | ON |
| **B-ship** | ArduinoJson chunked | **ON** (`=4`) | **ON** | ON |

Toggles (compile-time, ADR-126 — no runtime detection): gate via `-DREST_MAX_INFLIGHT=255|4` at `restAPI.ino:34`; **backstop OFF in B-raw is mandatory** — it is an ArduinoJson-specific `bad_alloc` mitigation; leaving it on measures "approach + mitigation", not the naked approach, and B-raw-with-backstop-off is exactly what reproduces the known "ArduinoJson rebooted at 8w" data point. A has no `JsonDocument` so the backstop is a no-op for A (`*` immaterial). **Heap floors stay ON in all four** (`platformFreeHeap()<4096` `restAPI.ino:2131`, per-handler maxblock-503 `restAPI.ino:2373-2379`) — "gate OFF" lifts *only* the concurrency cap, nothing else.

### 5.3 Matrix: 4 builds × {8,12,16} workers × reps

Reps = 2 baseline, **3 at the crux** (gate-OFF 8w and 16w, both raw arms — reboots are TWDT-timing stochastic; 2 reps can't separate fluke from pattern).

| Build | 8w | 12w | 16w | runs |
|---|---|---|---|---|
| A-raw | **3** | 2 | **3** | 8 |
| B-raw | **3** | 2 | **3** | 8 |
| A-ship | 2 | 2 | 2 | 6 |
| B-ship | 2 | 2 | 2 | 6 |
| **Total** | | | | **28** |

Each run **2.0 min** (`--minutes 2`, above the 1.5 min floor, matching the window the known 8w reboot appeared in), `--delay 0` (unthrottled flood — probing the failure envelope, not realistic dashboard rate). Flash a build **once**, run all its worker points back-to-back; **between every run, reboot the device and poll `/api/v2/device/info` until `freeheap` recovers to within a few % of cold-boot baseline** (script it — do not eyeball; sequential runs accumulate fragmentation and run N+1 must not inherit run N's heap).

### 5.4 Exact invocations (serial cap + load, in parallel on the one device)

`test_load.py` captures the bootcount delta but **not the reset reason / panic** — the serial cap + addr2line is the only thing that tells you *why* a reboot happened, and that "why" is the spine of the verdict.

```bash
# Terminal 1 — serial (COM4), slightly longer than load window + cooldown:
python scripts/tests/_serialcap.py --port COM4 --seconds 180 \
    --out captures/<BUILD>_<W>w_rep<R>.serial.txt

# Terminal 2 — HTTP load:
python scripts/tests/test_load.py --host 192.168.88.39 --minutes 2 --workers <W> \
    | tee captures/<BUILD>_<W>w_rep<R>.load.txt

# If bootcount delta != 0, decode the panic from the matching .elf for THAT build:
xtensa-esp-elf-addr2line -pfiaC -e <path-to-this-build>.ino.elf <PC/backtrace addrs>
```

**Two small prerequisite harness edits** (spec now, both read-only-safe):
1. `--endpoints` filter (or a heavy-only subset `/v2/settings`,`/v2/debug`,`/v2/sat/status`) — `worker()` currently round-robins all 9 endpoints, so aggregate stats can't attribute OOM to the *heavy* ones. **Required only if the hybrid branch is in play** (run a heavy-only mini-campaign A-raw vs B-raw, 8w+16w, ×3, if the aggregate lands near the hybrid boundary).
2. A scripted baseline-recovery poll between runs (§5.3).

### 5.5 Metrics per run

`bootcount` delta · **reset reason + panic decode** (serialcap + addr2line) · heap floor · **maxblock floor** · fragmentation peak + post-cooldown recovery · handled% (with the caveat below) · ADR-089 tier entries (low/warning/critical). Plus, **once per build** (correctness is a build property, not a load property): `json_golden compare` 0-FAIL + the §6 byte-checks.

### 5.6 Validate the rig BEFORE trusting any verdict (positive controls)

- **B-raw @ 8w MUST reboot** (known: ArduinoJson rebooted at 8w pre-gate). If it survives clean, the environment has drifted — **stop and reconcile**.
- **A-raw @ 16w should survive** (known: hand-rolled alpha.204 did 16w/2min clean). If A-raw reboots at 16w, the revert is **not faithful** to the streaming approach — fix the build before continuing.

### 5.7 Read the metrics correctly (two gate-state traps)

- **handled% is NOT comparable across gate states.** Gate ON: `handled = ok + 503(busy)`, so a device scores high by *refusing* load; gate OFF those 503s vanish. **Do not use `test_load.py`'s single PASS/FAIL to compare arms.**
  - gate-OFF discriminator = `bootcount` delta + reset reason + **maxblock floor** + heap floor (handled% informational only).
  - gate-ON discriminator = 0 reboots + **real-200 throughput** (the `ok` count, NOT 503-inflated handled%) + heap recovery.
- **maxblock floor is the mechanistic discriminator, weighted above the binary reboot.** A 2-min no-reboot window can be luck; a healthy contiguous-block floor (`>5000`, the harness `MIN_MAXBLOCK_FLOOR`) proves *headroom* — that B's failure mode (contiguous JsonDocument pool realloc nearing OOM) is not being approached. A has no pool; expect A's maxblock floor to stay high where B's craters.

### 5.8 DECISION CRITERIA (concrete thresholds)

The **panic decode on B-raw's reboot decides everything**: a **heap/`bad_alloc` backtrace** ⇒ the JSON approach is the culprit (revert/hybrid eligible); a **bare TWDT / core-1 reset** ⇒ it's TASK-879 (loop-task starvation, approach-independent — A-raw would reboot the same way) ⇒ keep ArduinoJson.

**→ MATERIALLY MORE ROBUST — expand A to all endpoints / consider full ADR-141 revert** — ALL of:
1. A-raw = **0 reboots across {8,12,16}w**, all reps; AND
2. B-raw reboots at **≥1 worker level** AND its panic decode is a **heap/`bad_alloc` backtrace, not a bare TWDT/core-1 reset**; AND
3. A-raw's **maxblock floor stays `>5000`** at every level where B-raw's **craters `<5000`**; AND
4. A passes **`json_golden` 0-FAIL** AND the explicit **float/bool byte-checks** (§6 items 1–2).

**→ MARGINAL — keep ArduinoJson + gate (ship B-ship)** — ANY of:
- A-raw and B-raw behave **alike** (both survive, or both reboot at similar levels with comparable maxblock floors); OR
- A-raw wins gate-OFF **but both A-ship and B-ship are 0-reboot** with acceptable real-200 throughput + heap recovery (the gate already closes the shipping gap); OR
- B-raw's reboots decode to **TWDT/core-1 starvation** (TASK-879) — gap is approach-independent.

**→ HYBRID — streaming for heavy endpoints only, ArduinoJson for light** — when:
- A-raw wins gate-OFF **specifically and only on the heavy endpoints** (per the §5.4 per-endpoint mini-campaign), AND B-raw's reboots are heap/`bad_alloc`-driven and **disappear when those heavy endpoints are excluded**.

**Tie-break:** on any boundary result, **weight gate-OFF maxblock floor + panic-decode attribution over the binary reboot count.** Reboots at a 2-min window are stochastic; contiguous-block headroom and the reset *reason* are the load-bearing signals.

---

## 6. RISKS + CORRECTNESS CHECKLIST

A hand-emitting writer can ship escaping/structure/format bugs ArduinoJson makes impossible. `json_golden.py` is **necessary but blind to the two bugs a streaming writer is most likely to ship** — `strip_volatile()` normalizes them away: it collapses `float.is_integer()→int` (line 64, so `20`≡`20.0` *passes*), and `"false"→false` (line 66-67, so a quoted/`1`/`0` bool *passes*). The byte-checks below are mandatory because of this. **A is eligible for the §5 load matrix only after all seven pass.**

1. **Float-format byte-check (golden blind spot #1).** On heavy endpoints, assert **byte-level** that decimal-bearing values (`tset`, `troom`, `dhwsetpoint`, `modulation`, all temps/setpoints) emit `NN.N`, not bare `NN`. Use a dedicated byte-diff, NOT `json_golden compare`. (HA number/sensor entities can choke on a type/format flip.)
2. **Boolean byte-check (golden blind spot #2).** Assert booleans emit literal `true`/`false` — no quotes, not `1`/`0`. Same reason.
3. **String escaping.** Inject a value containing `"` and a newline into a settable field (hostname, SSID, sensor name, crashlog text, filename) and confirm valid JSON out. Confirm the escape scan never string-walks a non-NUL-terminated buffer (per-project binary rule).
4. **Comma / structure balance.** Trailing comma before `}`/`]`, missing comma between elements — especially around **conditionally-emitted fields** (present only when a sensor exists). Strict-parse (`json.loads`) **every** endpoint, assert **0 parse errors**. Edge cases: `{}`, `[]`, single-element array, all-fields-suppressed object — covered by the Step-0 harness.
5. **Truncation-under-OOM policy — DECIDED: all-or-nothing is preferred, but the chosen sink degrades to truncate-and-log.** A truncated `200 OK` is worse than a clean failure (HA parses garbage, may corrupt state silently). The honest tension: single-pass-into-`AsyncResponseStream` *can* emit a truncated 200 if `cbuf` saturates mid-emit — the non-throwing primitive guarantees no crash but **not** an all-or-nothing body. **Mitigation that keeps us in the safe regime:** the `REST_MAX_INFLIGHT=4` gate + heap floors keep peak heavy-response heap at ~34 KB / ~95 KB, the alpha.204 regime where the cbuf never saturates, so truncation does not occur in practice. **Acceptance criterion:** during the §5 matrix, assert across all captured response bodies that no 200 response is a truncated/partial body (strict-parse every sampled response; any short body on a 200 is a fail). If truncated-200s appear under load, that is itself a decision signal against the push sink and must be recorded, not hidden.
6. **NaN/Inf.** Every computed float (uninitialized sensor, divide-by-zero) emits `null`, never `nan`/`inf`. Covered by `value(float)`; spot-check `sat/status`, `sat/weather`, `sat/diagnostics`, `sendOTValue` f88.
7. **Key ordering / completeness.** `json_golden compare` (semantic, order-independent): **0 FAIL, 0 missing/extra keys** across all 31 endpoints. Byte-diffs (whitespace/order) are expected and informational; a missing/extra *key* is a real contract break.

**Residual uncatchable `addHeader` throw** (§2.1): real, deferred-async, unreachable by any handler `try/catch`, **identical for A / B / 503** — does not discriminate, stays dormant in the bounded-heap regime. Record it as a known-accepted risk (already in ADR-145), not a blocker.

**Other risks:** narrow-int overload ambiguity on Xtensa (`int32_t==long`) — mitigated by the `PLATFORM_INT_DISTINCT_FROM_INT32`-guarded forwarding overloads. Concurrent-build `.pio` corruption — never run two builds in one worktree (bug-034 class). ESP-abstraction boundary — `jsonEmit.h` is platform-neutral application code (uses only `Print&`, `snprintf_P`, `F()`), introduces no raw `#ifdef ESP32`, so it does not move the abstraction baseline.

---

## 7. HONEST FRAMING

**This plan is evidence-gathering, not a foregone revert.** ADR-141 (ArduinoJson v7) is an Accepted, documented migration; ADR-145 (chunked-pull) is its shipped under-load mitigation. We reverse neither on aesthetics or intuition — only if the §5 numbers justify it, and then only via a superseding ADR (per the project's immutable-ADR rule), not an edit.

**What was already verified vs what the experiment must still establish.** Verified from primary source: the per-byte log-storm mechanism (`WebResponses.cpp:945-947`), non-throwing `cbuf::resize` (`cbuf.cpp:91-96`), truncate-and-continue write (`WebResponses.cpp:926-943`), the gate's pre-build 503 + balanced counter (`restAPI.ino:34,2103-2110`), the chunked-re-run volatile-field hazard, and the old escape/float contract (`0a3e5eee:jsonStuff.ino`). **Still unmeasured and the reason the experiment exists:** whether ArduinoJson's contiguous pool is *causally* responsible for the under-load reboot, or whether the reboot is TASK-879 TWDT core-1 starvation that is independent of the JSON approach (per `project_task883_load_test_twdt`: the alpha.211 8w reboot's ArduinoJson-causation is UNMEASURED). The A/B with panic decode is the discriminator that has never been run.

**What would prove each outcome (record these as the experiment's pre-registered hypotheses):**
- **Full revert justified** ⇔ A-raw 0 reboots everywhere **and** B-raw reboots with a **heap/`bad_alloc` backtrace** **and** A's maxblock floor stays high where B's craters **and** A passes all §6 byte-checks. (Approach removes the failure class.)
- **Hybrid justified** ⇔ the above, but the B-raw `bad_alloc` reboots are **attributable to the heavy endpoints only** and vanish when those are excluded. (Failure concentrated in the few large responses; convert only those — KISS, minimal change surface.)
- **Keep ArduinoJson + gate justified** ⇔ the arms behave alike, **or** the gate already makes both ship-builds 0-reboot, **or** B-raw's reboots decode to **bare TWDT/core-1** (TASK-879, approach-independent). (No reason to revert an accepted migration for an academic edge the gate neutralizes.)

**The decisive single fact** is the B-raw panic decode: heap-backtrace → the JSON approach is on trial; TWDT/core-1 → the JSON approach is exonerated and the real bug is TASK-879. Everything else in §5 is there to keep that one read-out honest (faithful A build, gate-OFF isolation, backstop-OFF on B-raw, positive controls, stochastic-reboot reps, maxblock tie-break).

**Cost of being wrong, recorded for the decision-maker.** A premature full revert re-opens the partition-rebalance question (`project_esp32_partition_rebalance_arduinojson`), discards ArduinoJson's correctness guarantees, and re-introduces hand-rolled-escaping risk (the very §6 surface). A premature "keep" leaves a contiguous-pool OOM class latent behind a 4-deep gate. Hybrid is the minimum-regret branch if the evidence is mixed — it removes the OOM trigger from the measured culprits while keeping ArduinoJson where it is proven and cheap.