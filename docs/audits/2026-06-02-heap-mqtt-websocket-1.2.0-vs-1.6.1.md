# Diagnosis: heap / MQTT / WebSocket behaviour — dev v1.2.0 vs v1.6.1

**Date:** 2026-06-02
**Scope:** Aggressive comparison of `dev` at tag `v1.2.0` (2026-03-02) and `v1.6.1` (2026-05-31) — 3 months, ~3590 commits apart.
**Trigger:** Field user reports "big issues": MQTT works badly, unexplained slowness, suspected heap crashes.
**Method:** `git diff`/`git show` between tags, ADR + backlog evidence, three parallel subsystem deep-dives, and direct verification of contested claims.

> **One-line conclusion.** This is **one coherent heap-pressure conflict**, not three separate bugs. The **WebSocket live-log is the heap producer**, **MQTT is the victim**, and the **REST API + slowness are symptoms** of the same scarce heap. v1.6.1 is, on most axes, *better* engineered for heap than v1.2.0 — but three deliberate trade-offs combine to expose an underlying short-write desync that v1.2.0 hid behind a large RAM buffer.

---

## 0. Platform constraint

ESP8266 (NodeMCU / Wemos D1 mini): ~40 KB RAM total, ~20–25 KB usable for the application after the Arduino core + WiFi stack. The firmware runs HTTP, WebSocket, MQTT and Telnet **concurrently** while decoding OpenTherm at ~1–10 Hz. Every KB the codebase grows in static buffers/feature-state is a KB removed from heap headroom. **Fragmentation (largest contiguous free block), not total free heap, is the real killer** on this platform.

---

## 1. Code growth (v1.2.0 → v1.6.1)

| File | v1.2.0 | v1.6.1 | Δ lines |
|---|---|---|---|
| `OTGW-Core.ino` | 2996 | 5341 | +2345 |
| `MQTTstuff.ino` | 1226 | 1771 | +545 (1851 changed) |
| `restAPI.ino` | 1048 | 1639 | +591 |
| `webSocketStuff.ino` | 202 | 294 | +92 |
| `MQTTstuff.h` | (new) | 384 | +384 |

Static `char[]` buffers in the `state`/settings structs grew from ~25 to ~49 (≈ +1.5–2 KB permanent RAM, ~5% of usable heap). REST handler/`send*` functions grew from ~24 to ~53.

---

## 2. The heap thread (directly verified)

Thresholds in `helperStuff.ino`:

| Threshold | v1.2.0 | v1.6.1 | Effect |
|---|---|---|---|
| `HEAP_LOW_THRESHOLD` | 8192 | **5120** | throttling starts 3 KB later |
| `HEAP_WARNING_THRESHOLD` | 5120 | **3072** | aggressive throttle 2 KB later |
| `HEAP_CRITICAL_THRESHOLD` | 3072 | **1536** | emergency/block only at 1.5 KB |

v1.6.1 adds a Schmitt-trigger deadband (`HEAP_LOW_RESTORE_THRESHOLD=6144`) to stop tier flapping and a **fragmentation-aware promote** (`getMaxFreeBlockSize()`: if the largest free block is small, demote one tier early). Per the code comment, the thresholds were tuned against a single tester crash log (`Crashevans`, v1.4.0-beta) — i.e. fitted to one load profile, not to user variation (number of sensors / MQTT subscriptions / browser tabs).

**Net:** v1.6.1 deliberately runs the heap much closer to the OOM edge. The static-buffer growth consumed the headroom; lowering the thresholds was the way to avoid constant throttling — at the cost of safety margin during a buffer spike.

---

## 3. MQTT — "works badly"

### 3.1 The acute crash/desync — TASK-769 (root cause code-confirmed)

- **v1.2.0:** `MQTTclient.setBufferSize(1350)` + buffered publish — the whole packet is assembled in one RAM buffer and written atomically (succeeds or fails as a whole).
- **v1.6.1:** buffer reduced to **384 B** (`MQTT_CLIENT_BUFFER_SIZE`) + `wifiClient.setSync(true)` (eliminates the ~1 KB WiFiClient shadow buffer). Writes now flush **directly to lwIP**. Under low heap the TCP send buffer is empty → `MQTTclient.write()` **short-writes** → the chunk is truncated, but `beginPublish(topic, len)` already committed a larger remaining-length → **MQTT stream desync** → the broker parses the next header as payload → **`malformed packet`** → disconnect → reconnect with the same client-id while the old session lingers → **`session taken over`** → flapping. Largest-payload path (most exposed): the discovery composer in `mqtt_configuratie.cpp` (7 stream sites).

**Fix (TASK-769, commits `7e5a61ed` / `f8314a0b`):** bounded retry-with-yield (`MQTT_WRITE_MAX_RETRIES=10`) on short writes + on genuine failure drop the TCP link via `MQTTclient.disconnect()` instead of `endPublish()` on a truncated payload. **AC#6 (field validation by the reporter) is still open** — confirm the user runs a build *after* this fix.

### 3.2 Slowness source #1 — the 15 s synchronous connect (ADR-080)

`MQTTclient.connect()` (PubSubClient) blocks the main loop up to 15 s per attempt (`setSocketTimeout(15)`, `MQTTstuff.ino:778`), gated to retry every 42 s (`timerMQTTwaitforconnect`). During a slow/unreachable broker this consumes ~36% of the loop budget → frozen UI/telnet, delayed publishes. Identical in both versions; now documented and explicitly accepted (steady-state `MQTTclient.loop()` is non-blocking).

### 3.3 Discovery model

- **v1.2.0:** discovery published in large bursts (60 s timer / manual `F` = all-at-once spike).
- **v1.6.1 (ADR-073):** adaptive **drip** — `loopMQTTDiscovery()`, 1 ID/tick, 2 s normal / 10 s under pressure, gated by `MQTT_DISCOVERY_HEAP_MIN=3000`. Plus **status-burst gating** (`beginStatusBurst`/2 s cooldown) so the drip does not collide with the ~9-publishes-in-20ms status fan-out. → flatter heap profile, but constant light pressure every tick.

### 3.4 Behaviour changes (breaking) that *feel* like "MQTT broken"

- **On-change publishing is now the default (ADR-081):** on upgrade `MQTTinterval=0 → 60`, `MQTTonChangePublishing=true`. Consumers that relied on every repeated frame being republished see different behaviour.
- **Availability now reflects the MQTT link, not the OT bus (ADR-074):** automations triggering on entity `unavailable` must move to the `otgw_connected` sensor.

---

## 4. WebSocket — the heap producer (corrected)

**Correction to an earlier reading:** v1.2.0 `sendLogToWebSocket()` (`webSocketStuff.ino:172–176`) calls `webSocket.broadcastTXT(logMessage)` **unconditionally** — no heap gate. v1.6.1 (`webSocketStuff.ino:264–267`) adds `&& canSendWebSocket()` — the gate was **introduced** in this window. Under WARNING (<3072 B) live-log frames are now **silently dropped** (no browser notice) → "log frozen/slow", but this is protective.

**Why WebSocket is the producer (ADR-083):** `broadcastTXT` **copies** the payload into a per-client send buffer → each pushed OT frame costs up to `MAX_WEBSOCKET_CLIENTS` (=3, unchanged) allocations + WS framing, ~2–4 KB lwIP per client. Field evidence (George `geo83_44083`, Discord, 2026-05-31): with the live-log open, heap pressure rises and within ~10 min HA MQTT sensors go `unavailable`; closing the tab keeps MQTT solid for hours.

**The coupling is the core problem:** WebSocket and MQTT cross the LOW/WARNING tier boundaries at the *same* free-heap values. Relaxing the MQTT gate to keep publishing also relaxes the WebSocket throttle → the producer runs hotter → worse. **ADR-083 (Proposed)** splits this into per-consumer ladders sharing one CRITICAL floor + drop-to-latest coalescing. **Not yet landed.**

**Frontend improvements in 1.6.1:** `pagehide`/`beforeunload` → `disconnectOTLogWebSocket()` (fixes the reload-storm where v1.2.0 accumulated stale clients until the 3-client cap → "can't connect"). Trade-off: hidden tabs now lose live data. WiFi reconnect now explicitly closes the WS server (`refreshServicesAfterWifiReconnect()`) → a brief live-log interruption on every WiFi blip.

---

## 5. REST API — heap churn & fragmentation gates

- **`GET /api/v2/device/info`** caused excessive heap churn and multiple TCP yield points → buffers reduced, yields consolidated, `BootFlashCache` avoids 8 SPI queries per call (TASK-701).
- New contiguous-block prechecks: `DEVICE_INFO_MIN_HEAP_BLOCK=1536`, `VERIFICATION_MIN_HEAP_START=6000` → under fragmentation the endpoint can return **503 "low heap"** even with >4 KB total free. (The over-conservative 8192-byte gate was *reduced* in TASK-723 because earlier builds refused too often.)
- 1.6.1 now exposes diagnostics: `hd_fragmentation_pct`, `hd_enter_low/warning/critical`, and monotonic lifetime drop counters `iWsDropsTotal` / `iMqttDropsTotal` (TASK-697).

---

## 6. Emergency heap recovery — corrected

Both versions have `emergencyHeapRecovery()` (v1.2.0 `helperStuff.ino:866` — verified):

- **v1.2.0:** only `resetMQTTBufferSize()` + yield/delay → marginal (tens of bytes).
- **v1.6.1 (ADR-079):** three actions under CRITICAL — (1) **drop all WebSocket clients**, (2) **stop+restart OTGWstream port 25238**, (3) **clear the MQTT discovery pending bitmap** → ~4–8 KB. Telnet is deliberately kept alive; MQTT is deliberately *not* disconnected (a 15 s reconnect costs more than it frees). Trade-off: exactly when heap is tight, the live-log and OTmonitor session drop.

---

## 7. The crash chain, end to end

```
WebUI live-log open
  → broadcastTXT copies OT frames ×3 clients          (WS = producer; gated in 1.6.1 but thresholds are low)
  → free heap drops + fragments (maxBlock < 1536)
  → MQTT publish: short-write on empty TCP sndbuf
  → sync-mode flush straight to lwIP → truncated chunk
  → endPublish() on truncation → MQTT stream desync
  → broker "malformed packet" → disconnect
  → reconnect same client-id → "session taken over"    (flapping → HA "unavailable")
  → connect() blocks loop 15 s / 42 s                  (slow/frozen UI + telnet)
  → under CRITICAL: emergencyHeapRecovery drops WS + OTGWstream
Close tab → producer gone → heap recovers → MQTT solid for hours
```

---

## 8. Ranked root causes & how to address them

See the companion section in the PR/task; summarised here:

1. **Coupled WS/MQTT heap ladder (producer starves victim)** — ADR-083 (Proposed). *Fix:* land per-consumer ladders + drop-to-latest WS coalescing, shared CRITICAL floor.
2. **MQTT short-write desync under low heap** — TASK-769. *Fix:* already implemented (retry-with-yield + `disconnect()` on truncation); needs field validation (AC#6).
3. **Heap thresholds tuned too low for the static-RAM growth** — `helperStuff.ino`. *Fix:* re-tune from real `logHeapStats` telemetry; consider making them configurable; do this only *after* #2 lands (relaxing the guard before the desync fix increases malformed-packet disconnects).
4. **15 s synchronous MQTT connect blocks the loop** — ADR-080. *Fix (long-term):* async MQTT client (own ADR); short-term: ensure broker reachability/DNS so `TRY_TO_CONNECT` is rarely entered.
5. **REST/device-info heap churn + fragmentation 503s** — TASK-701/723. *Fix:* already largely addressed; monitor `hd_fragmentation_pct` and confirm no spurious 503s remain.

---

## 9. Field diagnosis checklist

Ask the user for:
1. **Exact build** (`/api/v2/device/info` or telnet banner). Pre-TASK-769 (fw 1.6.0 / early beta) = still carries the desync.
2. **Telnet log**, grep for: `malformed`, `session taken over`, `HEAP-CRITICAL: Blocking WebSocket`, `[heap-recovery]`, and `logHeapStats` (`free` / `maxBlock`).

Immediate user mitigations:
- **Close the WebUI live-log / browser tab** when not actively watching — proven #1 trigger.
- **Update to the latest 1.6.1+** (TASK-769 truncated-publish protection).
- Verify **broker reachability + DNS** (else the 15 s sync-blocker bites).
- Migrate HA automations from entity `unavailable` to **`otgw_connected`** (ADR-074).

---

## References

- ADR-030 (heap monitoring), ADR-073 (JIT discovery), ADR-074 (availability = MQTT link), ADR-079 (emergency recovery actions), ADR-080 (15 s connect sync-blocker), ADR-081 (on-change default), ADR-083 (per-consumer heap gating — Proposed).
- TASK-697, TASK-701, TASK-723, TASK-769, TASK-779, TASK-786.
- Code: `MQTTstuff.ino` (`:778` socket timeout, `:283` `writeMqttChunk`), `helperStuff.ino` (thresholds, `canSendWebSocket`, `emergencyHeapRecovery`), `webSocketStuff.ino` (`:264` gated send, `MAX_WEBSOCKET_CLIENTS`), `mqtt_configuratie.cpp` (discovery composers), `restAPI.ino` (device/info gates).
</content>
</invoke>
