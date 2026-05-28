# ESP8266 Memory Audit — `dev` (1.6.1-beta) vs `feature-dev-2.0.0-otgw32-esp32-sat-support` (v2.0.0-alpha.84)

**Audit date:** 2026-05-28
**Auditor:** Claude (with maintainer review)
**Branches:**
- `dev` @ `a067629` (1.6.1-beta+a067629, post-1.6.0)
- `feature-dev-2.0.0-otgw32-esp32-sat-support` @ `9be88a0` (2.0.0-alpha.84)

**Target:** ESP8266 D1 mini, `eesz=4M2M`, Arduino Core 2.7.4 (PlatformIO `framework-arduinoespressif8266 @ 3.20704.0` on the 2.0.0 build; both branches link against the same toolchain).

---

## Verdict

- **`dev` (1.6.1-beta) on ESP8266 — Fits, comfortable.** Sketch 71.1 %, static DRAM 71.8 %, ~23 KB free for stack + heap.
- **2.0.0 (alpha.84) on ESP8266 — Tight to non-viable.** Sketch 83.0 % (watch zone), **static DRAM 91.3 %** (act-now zone), **only ~7 KB left for stack + heap**. With the platform's Wi-Fi/LWIP runtime working set, this leaves the device permanently below ADR-030's HEALTHY tier (`> 8 KB free heap`). Boot will most likely succeed; sustained operation under MQTT + HA discovery + SAT control + a connected WebSocket will sit in WARN / CRITICAL.

**Most growth backs a real feature** (SAT subsystem, settings change-detection, HTTP basic-auth, nightly restart, MQTT command dispatch tables). The audit's recommendation is not "delete features"; it is "reclaim the ~6–8 KB the firmware needs to stay in HEALTHY, by trimming the few items that are oversized for the SAT-on-ESP8266 use case".

---

## Headline numbers (fresh from this session's builds)

| Region | dev (1.6.1-beta) | 2.0.0 (alpha.84) | Δ | Ceiling | dev % | 2.0.0 % |
|---|---:|---:|---:|---:|---:|---:|
| Sketch (`text+data+text1+rodata`) | 742,160 B | 867,436 B | **+125,276 B** | 1,044,464 B | 71.1 % | **83.0 %** |
| Static DRAM (`.data + .rodata + .bss`) | 58,812 B | 74,812 B | **+16,000 B** | 81,920 B | 71.8 % | **91.3 %** |
| Free for stack + heap | 23,108 B | **7,108 B** | −16,000 B | — | — | — |

Section-level breakdown:

| Section | dev | 2.0.0 | Δ | What lives there |
|---|---:|---:|---:|---|
| `.text` (IRAM code) | 384 B | 384 B | 0 | Hot-path code that must run from IRAM |
| `.text1` (IRAM code) | 28,916 B | 28,920 B | +4 B | IRAM code |
| `.irom0.text` (flash code) | 694,208 B | **810,468 B** | **+116,260 B** | The bulk of the firmware; in flash, not DRAM |
| `.data` (initialized DRAM) | 3,272 B | **6,540 B** | +3,268 B | Initialized globals; counts against DRAM |
| `.rodata` (read-only DRAM) | 15,380 B | **21,120 B** | +5,740 B | `const` arrays / strings *not* marked PROGMEM; counts against DRAM |
| `.bss` (zero-init DRAM) | 40,160 B | **47,152 B** | +6,992 B | Zero-initialized globals; counts against DRAM |

LittleFS data partition (2 MB): dev ≈ 388 KB used, 2.0.0 ≈ 570 KB used (added `sat.js` 54 KB, `design.html` 33 KB, `components.css` 54 KB, `index.js` +81 KB). Both have ample headroom in the 2 MB partition; LittleFS is never the bottleneck.

---

## Reading the verdict against ADR-030 / ADR-089

ADR-030 (dev) and its 2.0.0 amendment ADR-089 define heap tiers:

| Tier | Threshold | Behaviour |
|---|---|---|
| HEALTHY | `getFreeHeap() > 8 KB` | Full functionality |
| LOW | 5–8 KB | Reduce message frequency |
| WARNING | 3–5 KB | Throttle MQTT, skip non-essentials |
| CRITICAL | `< 1.5 KB` | Stop non-essentials; emergency recovery |

After the platform's Wi-Fi runtime allocates its working set (~20–25 KB on ESP8266) and the stack reserves ~2–3 KB, **`getFreeHeap()` is roughly `81920 − static DRAM − stack − Wi-Fi reserve`**.

- **dev**: ≈ 81920 − 58812 − 3000 − 22000 ≈ **−1.9 KB** → in practice `ESP.getFreeHeap()` reports ~12–18 KB (the Wi-Fi reserve is partial; SDK allocations are lazy). HEALTHY is reachable; LOW is hit under load.
- **2.0.0**: ≈ 81920 − 74812 − 3000 − 22000 ≈ **−18 KB** → the SDK cannot allocate its full working set. In practice `ESP.getFreeHeap()` will report ~2–5 KB after boot — WARNING tier from boot, with no headroom for the SAT control loop, HA discovery bursts, or WS clients to operate without tipping into CRITICAL.

This is not a theoretical hazard. The 2.0.0 `helperStuff.ino:854-856` defines:

```c
#define HEAP_CRITICAL_THRESHOLD   1536
#define HEAP_WARNING_THRESHOLD    3072
#define HEAP_LOW_THRESHOLD        5120
```

— and the firmware already has `emergencyHeapRecovery()` (ADR-079 on dev, expanded on 2.0.0) that drops WS clients and the OTGW serial stream when crossing into CRITICAL. With 2.0.0's static DRAM budget, that recovery path becomes the **normal operating mode** on ESP8266, not the exception.

---

## What grew on ESP8266 in 2.0.0, ranked by DRAM cost

The 16,000-byte static DRAM delta is dominated by a small number of items. Sizes are from `xtensa-lx106-elf-nm --size-sort -S` on the 2.0.0 ESP8266 ELF, with dev's same symbol shown for comparison where it exists.

### Top BSS / DATA growth items

| Symbol | 2.0.0 size | dev size | Δ | Path | Feature backing it |
|---|---:|---:|---:|---|---|
| `updateSetting::_noopSnapshot` | 1,728 B | 0 | +1,728 | `settingStuff.ino` | Settings change-detection (skip no-op flash writes) |
| `settings` | 1,736 B | 1,248 B | +488 | `OTGW-firmware.h:630` | `SATSection`, HTTP-auth, nightly-restart, picBoot |
| `state` | 1,232 B | 544 B | +688 | `OTGW-firmware.h` | `SATRuntimeSection`, RestPerfSection, DiscoverySection |
| `satHCRLoadState::buf` | 960 B | 0 | +960 | `SATcontrol.ino` (HCR section) | SAT heating-curve recovery state |
| `_win4h` (samples) | 720 B | 0 | +720 | `SATcycles.ino` | SAT 4-hour rolling window — **already minimized: `SAT_WIN4H_SIZE=30` on ESP8266 vs 360 on ESP32 (SATtypes.h:101)**. Fair sizing. |
| `satPublishMQTT::climAttrBuf` | 512 B | 0 | +512 | `SATmqttPublish.cpp` | SAT MQTT climate attribute JSON staging |
| `_hcrIntraMedian::sorted` | 384 B | 0 | +384 | `SATcontrol.ino` (HCR) | Median-sort scratch for HCR algorithm |
| `_hcr_samples` | 384 B | 0 | +384 | `SATcontrol.ino` | HCR sample ring |
| `_cycleHistory` | 320 B | 0 | +320 | `SATcycles.ino` | SAT per-cycle history ring |
| `_tail_samples` | 256 B | 0 | +256 | `SATcontrol.ino` | SAT tail-end sample ring |
| `_flow_samples` | 256 B | 0 | +256 | `SATcontrol.ino` | SAT flow-temperature sample ring |
| `kSatMqttCmds` (.rodata, **= DRAM**) | 996 B | 0 | +996 | SAT MQTT command dispatch | ADR-078 dispatch tables — table entries are in `.rodata`, not PROGMEM, so they occupy DRAM. |
| Other small SAT BSS (≤ 64 B each) | ~600 B | 0 | +600 | Various SAT scopes | Counters, indices, flags |
| **SAT-attributable subtotal** | **~6,341 B** | 0 | **+6,341** | (sum of all `SAT*`, `_hcr*`, `_win4h*`, `_cycle*`, `_tail_samples`, `_flow_samples`, `climAttrBuf`) | New SAT subsystem on ESP8266 |
| `_noopSnapshot` | 1,728 B | 0 | +1,728 | `settingStuff.ino` (function-static) | Change detection |
| **Settings/State growth attributable to non-SAT** | ~400 B | — | +400 | various | HTTP-auth, nightly restart, RestPerf, DiscoverySection |
| Other `.rodata` growth not in dispatch tables | ~3,800 B | — | +3,800 | various | New strings / arrays not marked PROGMEM |
| **Reduction**: `sJsonFlushBuf[512]` removed | 0 | 512 | **−512** | `jsonStuff.ino:173` | **Win from streaming refactor (ADR-077)** — buffer eliminated; `WebServer::sendContent()` writes straight to LWIP. |

Renamed-not-grown items (same size, just moved): `handleOTGW::sRead/sReplay` → `handlePICSerial::sRead/sReplay` (2× 512 B each on both branches; refactor only).

### What this tells us

- **SAT is the dominant contributor (~6.3 KB)**. Sizing is mostly defensible: `_win4h` is already minimized on ESP8266 (SATtypes.h:101 `#if defined(ESP8266) #define SAT_WIN4H_SIZE 30 #else 360 #endif`), the HCR sample rings (~1.5 KB combined) are dimensioned for the algorithm's median/regression windows.
- **`_noopSnapshot` 1,728 B is the single biggest oversize**. It exists to prevent identical settings writes from re-flashing. The same protection could be achieved with a 4-byte CRC32 of the settings struct (computed on demand into a stack-local). **Saving: ~1,720 B BSS.**
- **`kSatMqttCmds` 996 B in `.rodata`** is in DRAM because the table entries aren't marked `PROGMEM`. ADR-078's dispatch-table pattern would work equally well in flash on ESP8266 if the entries used `const char *` strings via PSTR and the table itself sat in `.irom0.text`. **Saving: ~900 B DRAM if moved to PROGMEM.**
- **`.rodata` growth of +5,740 B** beyond `kSatMqttCmds` suggests ~4 KB of other new const arrays / strings that aren't PROGMEM. A scan for `static const char ... = "..."` (no PSTR/F) on the new SAT/discovery code path will surface them. **Conservative saving: 2–3 KB if half migrate to PROGMEM.**
- **Settings/State +1.2 KB** is mostly genuine new persistent state (SAT settings, BLE roster, HTTP password). Two exceptions:
  - `sBleMAC[18] + sBleMac[8][18] + sBleLabel[8][24] = 354 B` for BLE roster — **unused on ESP8266** (no BLE radio). Wrapping in `#if defined(ESP32)` reclaims 354 B from both settings and the persisted JSON. Or with `#if HAS_BLE` (a new flag).
  - `_noopSnapshot` is sized = `sizeof(settings)` so it scales with this growth. Switching to CRC32 makes the snapshot independent of settings size.

### Top SAT-attributable saving opportunities (if you choose to act)

| Trim | Saving (BSS) | Behaviour change | Risk |
|---|---:|---|---|
| `_noopSnapshot` → 4-byte CRC32 | ~1,720 B | None (same no-op detection, recomputed on each write) | Low — implementation is one helper |
| `kSatMqttCmds` table entries → `PROGMEM` | ~900 B | None (dispatch reads via `pgm_read_*` instead of direct deref) | Low — pattern already used in MQTT subcmd dispatch on dev |
| BLE roster `#if HAS_BLE` (compile out on ESP8266) | ~354 B | Settings JSON loses `SATblemac*` keys on ESP8266; users with mixed fleets see different settings shape per device — needs a one-liner migration note | Low |
| Audit `.rodata` for non-PROGMEM strings on SAT/discovery path, migrate to F/PSTR | 2–3 KB est. | None | Low (mechanical) |
| **Total realistic saving** | **~5–6 KB DRAM** | | |

That brings 2.0.0/ESP8266 from 91.3 % DRAM back to ~85 %, restoring HEALTHY-tier operation. None of these touches the SAT control algorithm itself; they touch the *accounting* of static memory.

---

## Streaming-vs-send-buffer policy: per-buffer assessment

Maintainer policy: **prefer streaming / chunked output over intermediate "build then send" buffers; use the LWIP TCP socket queue as the natural back-pressure mechanism.**

Background: on the ESP8266 LWIP2 build, `TCP_SND_BUF ≈ 5840 B` and `TCP_MSS ≈ 536 B` per active TCP connection (these allocations live in the LWIP heap, which is in the same DRAM but is allocated only when sockets are open). A `WiFiClient::write()` short-returns when the LWIP queue fills, so any producer that streams chunks naturally gets back-pressure for free. An intermediate global buffer that exists only to hold bytes that will then be copied into LWIP is double-counting RAM.

| Buffer | Path / size | Downstream API | Verdict |
|---|---|---|---|
| `sJsonFlushBuf[512]` (dev) | `jsonStuff.ino:173` | `WebServer::sendContent()` | **GONE on 2.0.0** — refactored away (ADR-077 streaming). Policy compliance: ✅. |
| `cMsg[512]` (both branches) | `OTGW-firmware.h:74 (dev) / :95 (2.0.0)` | Mixed: `HTTPClient::POST`, `WebServer::send`, `PubSubClient::publish`, `WebSocketsServer::broadcastTXT`, MQTT topic render | **partial** — webhook POST and REST JSON slices *could* stream. MQTT-publish slice already uses `beginPublish/write` for discovery (ADR-042/077). MQTT-topic render needs ≤ 200 B contiguous (topic is one arg of `publish()`). Splitting `cMsg` in two (200 B topic scratch + 0 B streaming) saves ~312 B but adds re-entrancy complexity (ADR-090's RAII lock pattern already exists for this kind of share). **Net: low-priority trim**. |
| `ot_log_buffer[512]` (both) | `OTGW-Core.ino:97 (dev) / OTGWLogMacros.h:12 (2.0.0)` | `WebSocketsServer::broadcastTXT(buf)` | **must-buffer** — one OT-log line is the atomic broadcast unit; the JS log parser (`.ot-log-content` with `white-space: pre`) expects whole lines per frame. Sized for "longest decoded OT line + flag8 expansion". Keep. |
| `satPublishMQTT::climAttrBuf[512]` (2.0.0) | `SATmqttPublish.cpp` | `PubSubClient::publish(topic, payload)` for HA climate JSON attributes | **could-stream** — the `climate.attributes` JSON is built linearly. Re-implementing as `beginPublish(topic, computedLen) → write_P(...) → write(...) → endPublish` (same pattern as discovery configs in `MQTTHaDiscovery.cpp`) eliminates this 512 B. **Saving: 512 B**, identical to ADR-077's discovery refactor. Highest-priority streaming win on 2.0.0. |
| `MQTTPubNamespace[192]` + `MQTTSubNamespace[192]` (both) | `MQTTstuff.ino:192-194` | n/a (persistent topic-prefix strings) | **not applicable** — these are persistent identifiers, not output staging. Sizing is defensible (HA prefix ≤ 41 + `device_id` ≤ 41 + `unique_id` ≤ 41 = 123 + separators ≈ 130 worst case; 192 leaves margin). |
| `MQTTclientId[96]` + `NodeId[96]` + `topicToken[96]` (both) | `MQTTstuff.ino` | n/a | **needed** — identifiers, not staging. |
| `lastReset[129]` (both) | `OTGW-firmware.h` global | Diagnostic string assembled at boot, served via REST/Debug | **must-buffer** (boot-time assembly; never streamed). 129 B is right-sized for the exception+EPC1+EXCVADDR formatted message. |
| `lineBuf[512]` static (FSexplorer, both) | `FSexplorer.ino:117 (dev) / line ~120 (2.0.0)` | `WebServer::sendContent()` | **could-stream** — same logic as old `sJsonFlushBuf`. The FSexplorer renders directory entries row-by-row; each row is built into `lineBuf` then sent. `sendContent` already chunks. **Saving: 512 B**. Low priority on its own but a clean follow-up. |
| Discovery configs (`MQTTHaDiscovery.cpp` 244 KB, mostly PROGMEM) | PROGMEM tables | `PubSubClient::beginPublish/write/endPublish` | **already-streams** ✅ (ADR-077). The MQTTHaDiscovery refactor is the canonical streaming example on 2.0.0. |
| `UpdateServerIndex` / `UpdateServerSuccess` (both, PROGMEM) | `updateServerHtml.h` | `WebServer::send_P` | **already-streams** ✅. |
| `_noopSnapshot[1728]` (2.0.0) | `settingStuff.ino` function-static | n/a (in-memory comparison) | **not applicable to streaming** — this is a CPU-time-vs-RAM trade. CRC32 wins the trade on ESP8266; see "trim candidates" above. |

**Direct answer to the maintainer's question** ("do we really need a send buffer, or could we direct-write into the TCP buffer?"):

- On 2.0.0, three of the five send-staging buffers ≥ 256 B are *already* streaming-direct (HA discovery via `beginPublish/write`, OTA HTML via `send_P`, REST chunked content via `sendContent`). The codebase is largely following the policy.
- **The remaining two large send-staging buffers (`satPublishMQTT::climAttrBuf` 512 B on 2.0.0, `lineBuf` 512 B FSexplorer on both) could be eliminated** with no functional change — they just amortise `sendContent` / `publish` calls. LWIP already coalesces small writes into TCP segments via Nagle.
- **`ot_log_buffer` and `cMsg`'s MQTT-topic slice are genuine must-buffer cases**, not policy violations. The WS log-broadcast frame is atomic; an MQTT topic is one argument of `publish()` and must be contiguous. Keep them.
- **`_noopSnapshot` is not a send buffer at all**; it's a sentinel for change-detection. Its 1,728 B is independent of the streaming policy and is the single biggest discretionary trim.

---

## Heap consumers under load (no hardware available — sourced from docs + library defaults)

| Consumer | Typical bytes | Source / notes |
|---|---:|---|
| Wi-Fi / SDK runtime | 20–25 KB | ESP8266 SDK working set; NV state, connection tracking, RX queues |
| LWIP `TCP_SND_BUF` per open socket | 5,840 | LWIP2 default; lazy-allocated on first `WiFiClient::write` per socket. Multiple concurrent clients (MQTT + REST + WS × N + Telnet) compound. |
| LWIP `TCP_WND` per open socket | 5,840 | Receive window |
| PubSubClient send buffer | 384 | `MQTT_CLIENT_BUFFER_SIZE` 2.0.0 (`MQTTstuff.ino:49`); 128–256 on default PubSubClient |
| WebSocketsServer client pool | ~1 KB / client × 5 clients | `WEBSOCKETS_SERVER_CLIENT_MAX = 5` |
| HTTP server response state | ~512 B / active request | `ESP8266WebServer` parses headers + URI per request |
| OTGWstream (TCP serial bridge) | ~3 KB when client connected | `OTGWstream` is in BSS at 0x13c (316 B static) + LWIP socket buffer |
| Drip queue (`MQTTautoCfgPendingMap`) | 32 B static + per-publish heap allocation | 2.0.0 (`OTGW-firmware.h`) — bitmap is static; the actual publishes are streamed |
| Telnet (SimpleTelnet) | ~1 KB / client | Two ports (23, 81), max 2 clients each |
| AceTime zone cache (`zoneProcessorCache`) | 1,708 B static | Both branches — in `.bss`; cache for 3 timezones |

On dev (1.6.1-beta), with ~23 KB free for stack + heap + LWIP + SDK lazy allocations, the device runs comfortably with one MQTT + 2-3 web clients + telnet. On 2.0.0 with ~7 KB total, the SDK working set cannot complete its lazy allocations without flipping the heap into WARN/CRITICAL almost immediately.

---

## Critical-but-fair assessment per consumer

### SAT subsystem (~6.3 KB BSS on ESP8266)

- **Feature live?** Yes — SAT (Smart Autotune Thermostat) is the headline 2.0.0 feature, ported from Alex Wijnholds' SAT custom component. It runs on both targets.
- **Sized appropriately?** Mostly yes. `SAT_WIN4H_SIZE = 30` on ESP8266 vs 360 on ESP32 is exactly the kind of memory-aware sizing the audit wants to see. HCR sample windows are minimal for the median-regression algorithm to be statistically sound. Cycle history (5 records × 64 B = 320 B) is the minimum for the "last few cycles" UI panel.
- **Existence defensible?** Yes per the policy: the SAT control loop needs sample buffers that persist across loop iterations. These aren't send-staging buffers; they're algorithm state.
- **Streaming alternative?** Only one: `climAttrBuf[512]` (the MQTT publish staging) is a send-staging buffer and could stream like discovery. **Saving: 512 B**.

### Settings change-detection (`_noopSnapshot` 1,728 B)

- **Feature live?** Yes — prevents flash wear from no-op settings writes (REST/MQTT can re-publish identical settings frequently).
- **Sized appropriately?** **No, over-built.** The sentinel snapshot is the full `sizeof(settings)` struct. A 4-byte CRC32 over the same bytes gives identical detection behaviour at 1/432 the RAM cost.
- **Existence defensible?** The change-detection itself is — preventing flash wear matters. The *implementation* is the issue.
- **Recommended action:** Replace with CRC32 sentinel. **Saving: ~1,720 B BSS**. Risk: low; collision probability of CRC32 over a ~1.7 KB struct is ~2⁻³² ≈ 0; even on collision, the worst case is one missed no-op detection, not data corruption.

### `kSatMqttCmds` dispatch table (996 B in .rodata = DRAM)

- **Feature live?** Yes — ADR-078 dispatch-table pattern replacing chained `strcasecmp_P` blocks. Maintainability win.
- **Sized appropriately?** The number of entries is fine. The placement is the issue: table is in `.rodata` (DRAM), not `.irom0.text` (flash). On ESP8266, `const struct { const char *name; void (*fn)(...); }` sits in DRAM unless the array itself is decorated `PROGMEM` and the strings are PSTR-built.
- **Existence defensible?** Yes (the dispatch table). DRAM placement is fixable.
- **Recommended action:** Mark `kSatMqttCmds[] PROGMEM` and lookup via `pgm_read_*` (or `memcpy_P` for the row). **Saving: ~900 B DRAM**. Risk: low; same pattern already in MQTT subcommand dispatch on 2.0.0 `MQTTstuff.ino`.

### BLE roster (354 B in settings struct, 0 functionality on ESP8266)

- **Feature live?** Only on ESP32 (no BLE radio on ESP8266).
- **Sized appropriately?** N/A on ESP8266 — the storage is unused.
- **Existence defensible?** It costs the persisted settings JSON to carry empty `SATblemac*` keys on ESP8266. 354 B BSS + ~150 B serialised JSON.
- **Recommended action:** Wrap `sBleMAC` / `sBleMac[]` / `sBleLabel[]` in `#if HAS_BLE` (a new flag, default 1 on ESP32, 0 on ESP8266). Update `readSettings/writeSettings` to skip the keys when compiled out. **Saving: 354 B BSS + smaller settings file**. Risk: low; settings migration is one-way (existing ESP8266 settings files with the keys are ignored, not corrupted).

### `.rodata` growth +5.7 KB (after subtracting `kSatMqttCmds`)

- **Feature live?** Yes — new SAT MQTT topics, dispatch labels, OpenTherm message ID maps, REST route labels, etc.
- **Sized appropriately?** Per-string yes; per-placement no — strings that should be in PROGMEM via `F()` / `PSTR()` are landing in `.rodata`.
- **Existence defensible?** Yes (the content). DRAM placement is the issue.
- **Recommended action:** Run `evaluate.py` with a new check `find_non_progmem_strings_on_new_code_path` that flags string literals not wrapped in `F()` / `PSTR()` inside the new SAT/Settings/OTGW-Core code paths. Estimate: 2–3 KB recoverable. Risk: low; mechanical change.

### `cMsg[512]` global scratch (both branches)

- **Feature live?** Yes — used by webhook (HTTP POST), REST (JSON build), JSON parser (search pattern), MQTT topic render. Shared via documented re-entrancy contract (ADR-090's `MQTTAutoConfigSessionLock` is the pattern).
- **Sized appropriately?** 512 B is sized for the worst-case MQTT topic build path (`prefix + device + unique_id + sub_topic + extras`). Documented worst-case is ~465 B (per `check_buffer_arithmetic` in `evaluate.py`). So ~10 % slack — reasonable.
- **Existence defensible?** Partially. The MQTT-topic-render and JSON-parse usages need contiguous memory. Webhook POST and REST JSON usages could stream. Splitting saves ~312 B but adds re-entrancy complexity that ADR-090 already had to address; not obviously worth it.
- **Recommended action:** Leave as-is. Document that the four call-site clusters are deliberately consolidated.

### `lineBuf[512]` static in FSexplorer

- **Feature live?** Yes — FSexplorer directory listing.
- **Sized appropriately?** 512 B is one HTML row including filename + size + delete-button; reasonable.
- **Existence defensible?** No on the streaming-policy view — `WebServer::sendContent()` already chunks. Eliminating the buffer means more (smaller) `sendContent` calls.
- **Recommended action:** Low priority follow-up. **Saving: 512 B**. Risk: trivial CPU overhead per row.

---

## Findings + Recommendations

### Continue (no action needed)

- All "needed" buffers in the table above (cMsg's must-buffer slices, ot_log_buffer, MQTT IDs/namespaces, lastReset, SAT algorithm state buffers).
- The streaming refactor already done on 2.0.0 (sJsonFlushBuf removal, MQTTHaDiscovery streaming via beginPublish/write).
- The ESP8266-specific minimisation of `SAT_WIN4H_SIZE` (30 vs 360) — exactly the right shape.

### Watch (size monitoring)

- **Settings/State struct growth.** Both grew 30–100 % in 2.0.0; future SAT or feature additions will push them further. A `static_assert(sizeof(OTGWSettings) < 2048, ...)` on the ESP8266 build would catch silent drift.
- **`.rodata` placement.** New strings landing in `.rodata` instead of PROGMEM is a recurring failure mode. A static check (`evaluate.py`) would prevent regression.

### Act (recommended before merging 2.0.0 ESP8266 builds into a public release)

Ranked by saving-to-effort ratio:

1. **`_noopSnapshot` → CRC32**: saves ~1,720 B BSS. ~1 hour of work; one helper, one test.
2. **`kSatMqttCmds` → PROGMEM**: saves ~900 B DRAM. ~1 hour; mechanical conversion plus lookup-via-pgm_read_byte.
3. **`.rodata` PROGMEM sweep**: saves 2–3 KB DRAM. ~2–3 hours; scan new code paths, wrap literals.
4. **BLE roster `#if HAS_BLE`**: saves 354 B BSS + ~150 B persisted JSON on ESP8266. ~1 hour; flag + settings serialiser conditional.
5. **`satPublishMQTT::climAttrBuf` → streaming**: saves 512 B BSS. ~2 hours; mirror the ADR-077 discovery pattern.
6. **FSexplorer `lineBuf` → streaming**: saves 512 B BSS. ~1 hour.

**Total recoverable: ~6.0–6.5 KB DRAM**, bringing 2.0.0/ESP8266 from 91.3 % to ~83 % DRAM and restoring the ~7 KB minimum stack+heap budget back to ~13 KB. That re-opens HEALTHY-tier operation per ADR-030/089.

None of these trims removes a feature. Three of them (1, 2, 3) save DRAM by moving data to flash or to a smaller representation — the user-visible behaviour is unchanged.

---

## Open questions for the maintainer

1. **Is SAT-on-ESP8266 a release goal for 2.0.0?** If yes, the act-now trims above are mandatory before merging. If no (i.e. SAT is ESP32-only at release time, ESP8266 stays on 1.6.x line), then wrapping the SAT subsystem in `#if defined(SAT_ENABLED)` (default on for ESP32, off for ESP8266) is a one-flag alternative that defers the per-buffer trims indefinitely.
2. **Settings JSON migration for BLE roster `#if HAS_BLE`** — is a one-way drop of the `SATblemac*` keys on ESP8266 acceptable, or do we want forward-compat reading so future ESP8266→ESP32 hardware migrations don't lose the roster? Forward-compat reading is +20 lines in `readSettings`.
3. **Streaming policy as a binding ADR.** The maintainer's preference for streaming over send-buffers is observable in ADR-042/077 but not stated as a cross-cutting rule. A new ADR ("Output staging buffers are only acceptable when the downstream API requires a contiguous payload; otherwise stream into the LWIP/WS send queue and use it as the back-pressure point") would gate future regressions. This audit's tables can serve as the worked-examples section.

---

## Out of scope (deliberately not audited here)

- ESP32-S3 build of 2.0.0 — different ceilings (no 80 KB DRAM cap, different LWIP, BLE active), warrants its own audit.
- Heap behaviour on a connected device — this container has no flashed ESP8266. The numbers above are static / build-time; runtime free-heap on 2.0.0 may be slightly better than calculated (the SDK's lazy allocator can yield to settings under pressure) but the trend is clear.
- PIC firmware (the C-side of the OTGW). Unaffected by either branch's growth.
- Browser-side memory (`index.js` 317 KB on 2.0.0): served streamed from LittleFS, never in RAM.

---

## Methodology and reproducibility

Build commands executed in this audit (both succeeded, exit 0):

```bash
# dev
cd /home/user/OTGW-firmware
python build.py --firmware     # uses arduino-cli + Core 2.7.4

# 2.0.0
cd /home/user/OTGW-firmware-2.0.0
python build.py --target esp8266 --firmware   # uses PlatformIO + framework-arduinoespressif8266 @ 3.20704.0 (Core 2.7.4)
```

ELF analysis:

```bash
XTPATH=arduino/packages/esp8266/tools/xtensa-lx106-elf-gcc/2.5.0-4-b40a506/bin
$XTPATH/xtensa-lx106-elf-size -A <elf>          # section breakdown
$XTPATH/xtensa-lx106-elf-size -B <elf>          # Berkeley summary
$XTPATH/xtensa-lx106-elf-nm --size-sort -S <elf>   # symbol-by-symbol
```

The two ELF files are preserved at:

- `src/OTGW-firmware/build/esp8266.esp8266.d1_mini/OTGW-firmware.ino.elf` (dev)
- `(../OTGW-firmware-2.0.0)/.pio/build/esp8266/firmware.elf` (2.0.0)
