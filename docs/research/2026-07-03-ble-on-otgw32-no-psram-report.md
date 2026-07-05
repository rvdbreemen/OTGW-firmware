# Effect of BLE on the firmware on OTGW32 (no PSRAM)

Date: 2026-07-03. Author: on-device investigation, TASK-994 (companion to the
BLE-memory hypotheses + implementation proposal in this folder).

## Bottom line

On a board **without PSRAM** (which is exactly the OTGW32 case), enabling BLE
consumes about **64 KB of internal DRAM that cannot be recovered any other way**,
and that loss pushes the async web server below its working minimum: with BLE on,
the v2 Web UI **collapses under normal concurrent load** (a single browser opening
its assets in parallel), where with BLE off the same load is served cleanly.
PSRAM — the fix that solves this on the S3-Mini carriers — **does not exist on
OTGW32**, and the largest BLE cost (the radio controller) must stay in internal
DRAM regardless. So on OTGW32, BLE-on and a reliable web UI are in direct tension.

## Method and scope (honest proxy note)

Direct measurements were taken on the **bench ESP32-S3-Mini** (COM8 / 192.168.88.64)
running the **esp32-combo** build with **PSRAM left disabled** — i.e. the identical
"no-PSRAM ESP32-S3 running WiFi + async web + MQTT + NimBLE" condition that OTGW32
is in. OTGW32 itself is the separate **esp32** build (OT-Thing PCB, OTDirect instead
of PIC, 4 MB flash, no PSRAM). The BLE memory cost is determined by the NimBLE
stack (controller + host), which is byte-for-byte identical on any ESP32-S3
irrespective of which OT engine or peripherals are compiled in, and the web-asset
demand is the same image. The **absolute** free-heap baseline may differ by a few
KB between the two builds (OTDirect vs PIC+combo code size), but the **delta** (BLE
costs ~64 KB internal) and the **consequence** (web collapse) transfer directly.
Numbers below should be confirmed on a physical OTGW32 before being quoted as exact.

Measurements come from a temporary `device/info` probe reporting
`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` and
`heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`, plus a concurrent-load
ramp tool (1→16 simultaneous HTTP clients over the real v2 asset+API mix).

## Measured effect of BLE on internal memory (no PSRAM)

| State | Internal free | Largest free block (maxblock) |
|---|---|---|
| BLE **off** | ~82.8 KB | ~40.9 KB |
| BLE **on** (passive scan) | ~18.8 KB | ~9.7 KB |
| **Cost of BLE** | **−64.0 KB** | **−31.2 KB** |

Every byte of that cost is **internal** DRAM (there is no PSRAM to absorb it).
Disabling BLE again via the setting does **not** give the memory back in the same
boot — the firmware has no BLE de-init path today, so a board that turns BLE off
still runs at the low ceiling until it reboots.

### Where the 64 KB goes

- **BLE controller (~48 KB), precompiled `libbt`** — allocated on the internal heap
  at `esp_bt_controller_init/enable`. It carries DMA descriptors and ISR-serviced
  buffers, so it **must** live in internal RAM; it cannot be moved to PSRAM even on
  boards that have PSRAM. This is the dominant, irreducible share.
- **NimBLE host (~10–16 KB)** — mbuf pools (msys), HCI event pools (the advertisement
  hot path), ACL pool, the 4 KB host-task stack, plus the C++ scan/roster objects.
  A few KB of this is trimmable by build flags (host pool counts, disabling unused
  roles), but only a few KB, and not the controller share.

## Consequence for the Web UI (the failure the user sees)

The async web server (ESPAsyncWebServer on the AsyncTCP task) accepts incoming
connections unconditionally. A browser opening the v2 UI fires ~9 asset + API
requests in parallel; each accepted connection needs heap (per-connection buffers,
the ~2.9 KB send buffer, TCP pcbs). The system-wide backpressure gate throttles
handler *execution*, not connection *acceptance*, so under a burst the
accepted-but-queued connections pile up and drive the largest free block down until
an allocation inside the AsyncTCP task fails.

Concurrent-load ramp, no PSRAM:

| BLE | conc 2 | conc 4 | conc 8 | after load |
|---|---|---|---|---|
| **off** (maxblock ~41 KB) | clean | clean | clean | recovers instantly |
| **on** (maxblock ~9.7 KB) | mostly clean | **collapses** | **all requests time out** | **stays dead until reboot** |

With BLE on and no PSRAM, once ~4 connections arrive at once the AsyncTCP task
wedges: port 80 stops accepting, and in the worst case the whole network interface
(port 80 *and* telnet, even ICMP) goes silent and only a power/USB reset recovers
it. The board itself (FreeRTOS, loop task, OTDirect, MQTT) keeps running — it is
specifically the web server that starves. Raising the AsyncTCP task stack to 16384
(already done) removes the *permanent* stack-overflow variant of the wedge, but at
~9.7 KB largest-block the web stack is still too starved to serve a real browser's
parallel burst reliably.

BLE reception itself keeps working while this happens (sensors are still decoded);
the damage is confined to the web UI's ability to serve concurrent requests.

## Why the S3-Mini fix does not help OTGW32

On the S3-Mini carriers the same collapse is fully fixed by activating their 2 MB
PSRAM (`BOARD_HAS_PSRAM`): with PSRAM the WiFi/LWIP buffers and NimBLE host pools
land external, BLE-on costs almost no internal DRAM, and the web ramp serves cleanly
to conc=16 with instant recovery. **OTGW32 has no PSRAM chip**, so this lever is
unavailable, and even if it had PSRAM the ~48 KB controller share would still be
forced internal. OTGW32 cannot buy its way out of the collapse with memory it
does not have.

## Options for OTGW32 specifically

1. **BLE default OFF on OTGW32 (recommended).** BLE stays a user-enabotable option,
   but off by default so the web UI is reliable out of the box. Pairs naturally with:
2. **Add a real BLE de-init path** so that turning BLE off actually returns the
   ~64 KB (today it leaks until reboot). Higher risk — de-init/re-init mutates the
   precompiled controller lifecycle, the same class that has crash-looped the S3 to
   reflash-only recovery, so it needs a hardening pass (≥50 off→on cycles on ≥2
   boards) before it can back a live toggle rather than a reboot-on-change.
3. **Host-side trims (small).** Reduce static WiFi buffers (these stay internal and
   help even without PSRAM) and NimBLE host pools / unused roles. Realistically a few
   KB each — useful headroom, but nowhere near enough to make BLE-on comfortable
   against the ~48 KB internal controller floor.

The honest conclusion: on OTGW32, running continuous BLE scanning *and* a heavy
async web UI at the same time is not comfortable on the available internal RAM.
The realistic posture is BLE **default-off** on OTGW32, with an honest de-init so
that "off" genuinely frees the memory, and host-side trims as marginal headroom.

## Confirm on real hardware

These numbers are from the S3-combo-no-PSRAM proxy. To quote them as OTGW32-exact,
flash a probe esp32 build to the physical OTGW32 (COM4 / its WiFi IP) and read the
same `device/info` internal-heap fields with BLE off then on. Expect the BLE delta
(~64 KB) to match within a couple KB; the absolute baseline may differ slightly.
