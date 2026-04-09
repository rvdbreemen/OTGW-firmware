# ADR-072: SAT Platform Compatibility Layer — ESP8266 vs. OTGW32

**Status:** Accepted
**Date:** 2026-04-09

## Context

SAT runs on two hardware platforms (ADR-063):

| Build target | MCU | OpenTherm interface |
|---|---|---|
| ESP8266 (OTGW) | ESP8266 | PIC16F1847 co-processor via UART |
| OTGW32 | ESP32-S3 | Direct GPIO OpenTherm (`HAS_DIRECT_OT`) |

The SAT control algorithm is identical on both platforms — the heating curve, PID, cycle
tracker, and MQTT topics are the same. What differs is how OT values flow into SAT and how
SAT issues commands back to the boiler.

### The problem on ESP8266 (PIC path)

On the ESP8266, OT frame values arrive as text lines from the PIC (`T80401234`, `B80401234`)
and are parsed by `processOT()` into `OTcurrentSystemState`. SAT reads from this shared struct.
Commands are issued by calling `addOTWGcmdtoqueue()` which queues text strings (`CS=45.0`,
`MM=100`) for transmission to the PIC over UART. SAT **never** writes to the UART directly.

### The problem on OTGW32 (OT-direct path)

On the OTGW32, the OT bus is driven directly by the ESP32-S3 GPIO hardware through the
OpenTherm library. Frames are decoded by the interrupt-driven library and converted to the same
`T`/`B` text format via the frame bridge pattern (ADR-063), then passed to `processOT()`.
`OTcurrentSystemState` is populated identically. SAT reads from the same struct.

For command output, the OTGW32 path also routes through `addOTWGcmdtoqueue()` — the same
function. On OTGW32, this queue is drained by the OT-Direct driver (`OTDirect.ino`) rather
than by the PIC UART writer, but SAT code does not know or care which path drains the queue.

### Key insight: SAT is platform-agnostic at the algorithm level

Because:
1. `OTcurrentSystemState` is populated identically on both platforms (via frame bridge)
2. `addOTWGcmdtoqueue()` / `addCommandToQueue()` are unified abstractions

SAT source code needs no `#if HAS_DIRECT_OT` guards. The compatibility layer is entirely
below SAT — in the frame bridge and command queue, not in SAT itself.

### Where platform conditionals DO appear

The only platform-specific SAT-adjacent code is in:

- `MQTTstuff.ino`: MQTT subscribe/publish handlers include `#if HAS_DIRECT_OT` guards for
  OT-Direct-specific MQTT topics (mode selection, relay control). These are not SAT topics.
- `OTGW-firmware.h`: `#if HAS_DIRECT_OT` guards for `OTDirectSettingsSection` and related
  structs.

The SAT `.ino` files (`SATcontrol.ino`, `SATpid.ino`, `SATcycles.ino`, `SATweather.ino`)
contain zero platform-conditional preprocessor directives.

### BLE sensor path (OTGW32 only)

`SATble.ino` provides BLE temperature sensor support. BLE is only available on the ESP32-S3,
so the entire file is guarded at the feature-flag level via `#if HAS_BLE` (not `#if HAS_DIRECT_OT`,
since BLE availability is independent of OT interface type). On ESP8266 builds, `SATble.ino`
compiles as stubs. The BLE indoor temperature feeds into SAT's external temperature input,
which is shared with the MQTT/REST external temperature path — no platform-specific logic in
the control algorithm.

### Alternatives considered

1. **Separate SAT source files per platform** (`SATcontrol_esp8266.ino` /
   `SATcontrol_otgw32.ino`) — Rejected: the algorithm is identical. Duplicating files
   creates a maintenance burden exactly like maintaining two firmware forks (ADR-063 already
   rejected that approach). Any bug fix or feature must be applied twice.

2. **`#if HAS_DIRECT_OT` guards inside SAT `.ino` files for minor differences** — Rejected:
   there are no meaningful behavioral differences in the SAT algorithm between platforms.
   Where differences exist (BLE, some MQTT topics), they are handled via feature flags at
   the module level, not scattered through the algorithm files.

3. **Abstract `OTInterface` class with virtual methods for `readState()` and `sendCommand()`**
   — Rejected per KISS principle. The frame bridge pattern (ADR-063) already provides a
   zero-overhead abstraction: `OTcurrentSystemState` and `addOTWGcmdtoqueue()` are the de
   facto interface. A C++ virtual dispatch layer would add indirection, complicate the
   cooperative-stack call chain, and add flash overhead without any concrete benefit.

4. **Use `OTcurrentSystemState` directly on ESP8266, separate OT-Direct state struct on
   OTGW32** — Rejected: the frame bridge (ADR-063, ADR-065) explicitly converts OTGW32
   frames to the same text format as the PIC, so `processOT()` populates
   `OTcurrentSystemState` identically. Separate structs would duplicate the population logic.

## Decision

**SAT source files contain no platform preprocessor guards.** The compatibility layer is
implemented entirely via:

1. **Frame bridge pattern** (ADR-065): OTGW32 GPIO frames are formatted as 9-character text
   strings and passed to `processOT()`, which populates `OTcurrentSystemState`. SAT reads
   this struct directly.

2. **Unified command queue** (`addOTWGcmdtoqueue()` / `addCommandToQueue()`): SAT queues
   text commands (`CS=`, `MM=`, `CH=`). On ESP8266, the queue is drained to the PIC UART.
   On OTGW32, it is drained to the OT-Direct master driver. SAT does not know which.

3. **Feature-flag-guarded modules**: `SATble.ino` uses `#if HAS_BLE`. Platform guards for
   OT-Direct-specific topics in `MQTTstuff.ino` do not touch SAT logic.

### The compatibility contract

SAT code may assume:

- `OTcurrentSystemState.Tboiler`, `.Tset`, `.RelModLevel`, `.Statusflags`, `.MasterStatus`,
  `.CHpressure` are valid and updated regularly by `processOT()`
- `addCommandToQueue(cmd, len, false, 0)` reliably enqueues a command for the boiler
- `settings.sat.*` and `state.sat.*` are initialized before `initSAT()` is called
- `millis()` is monotonic (wraps after ~49 days; SAT uses unsigned subtraction throughout)

SAT code must NOT assume:

- Any specific frame format or UART path
- The number of milliseconds between OT frames
- That commands execute synchronously (the queue may be drained seconds later)

## Consequences

### Benefits

- SAT source files are clean: no `#if` forests, easy to read and audit
- Adding a third platform (e.g., ESP32 + PIC, if it ever exists) requires no SAT changes,
  only frame bridge and command queue adaptation
- Frame bridge pattern (ADR-063, ADR-065) is the single point of OT abstraction; tested
  independently of SAT
- BLE temperature input integrates via the same external temperature path as MQTT/REST,
  so the PID and heating curve logic is identical regardless of sensor source

### Trade-offs

- SAT developers must understand that `OTcurrentSystemState` is populated asynchronously;
  values are only as fresh as the last OT frame (typically 1 second on gas boilers)
- The command queue means SAT commands are not acknowledged synchronously. PIC communication
  failures are detected by the `_sat_picFailCount` counter in the safety system, not by
  direct command return values

### Risks

- If `processOT()` is not called frequently enough (e.g., during long-running operations),
  `OTcurrentSystemState` may be stale. The SAT safety system's stale-temperature expiry
  (5 minutes for indoor, 10 minutes for outdoor) provides partial protection, but boiler
  state staleness (flow temp, flame status) is not separately guarded. On OTGW32 this
  is less likely because the OT-Direct driver is interrupt-driven.

## Related

- ADR-063: OTGW32 Hardware Support — dual build targets, `HAS_PIC` / `HAS_DIRECT_OT` flags
- ADR-064: OT-Direct Operating Mode Architecture
- ADR-065: Frame Bridge Pattern — converts GPIO frames to PIC text format
- ADR-062: SAT Integration — overall SAT architecture
- ADR-070: SAT Memory Allocation — static state, no platform-conditional buffers
- `OTDirect.ino`: frame bridge implementation (`processOT()` call with text format)
- `OTGW-Core.ino`: `addOTWGcmdtoqueue()`, `processOT()`
- `SATble.ino`: BLE sensor path, `#if HAS_BLE` guard
