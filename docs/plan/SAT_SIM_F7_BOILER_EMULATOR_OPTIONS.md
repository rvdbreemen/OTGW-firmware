# SAT Simulation F7 — Boiler-Emulator Options (PARKED)

Date: 2026-06-03
Status: **Parked** (decision deferred by maintainer)
Task: TASK-802 (feat-2.0.0: SAT sim F7 — host-side boiler-emulator)
Related: TASK-795 (SAT simulation contract / availability gate), ADR-117

## Why this exists

F7 (SAT simulation plan §12) wants a way to validate the TASK-795 **availability
gate** on the bench *without a physical boiler*. The gate (ADR-117 §4.2) forcibly
disables simulation the moment a real boiler answers the OT bus, so its ACs
(edge auto-disable within ~1s, REST 409, MQTT reject) cannot be exercised unless
something convinces the firmware a boiler slave is present.

The first attempt was a host-side TCP script talking to the OTGW32 OTDirect bridge
(port 25238). That approach was investigated and **does not work** — see the
finding below. F7 is parked until we pick one of the options here.

## Verified finding: the OTDirect TCP bridge cannot trip the gate

Confirmed by reading the 2.0.0 source (2026-06-03):

- `handleOTDirectBridgeStream()` (`src/OTGW-firmware/OTDirect.ino:651-703`) reads a
  line from TCP port 25238 and relays it **straight to the PIC** via
  `sendPICSerial(sWrite, ...)`. It does **not** parse the input as an OpenTherm
  frame and does **not** populate any boiler cache.
- `otBoilerCacheValid[3]` (`OTDirect.ino:269`) — the flag behind
  `satBoilerHardwarePresent()` / the `satOnBoilerDetected()` edge — is set **only**
  from genuine OT-bus boiler responses processed in `handleMasterResponse()`
  (`OTDirect.ino:~1200`, via `bridgeFrameToParser('B', response)`).

So feeding a synthetic `B40030004` (MsgID 3 slave MemberID) into port 25238 is
relayed to the PIC and never reaches the cache. The TCP-bridge path cannot make
the firmware believe a boiler is present. This answers the plan's deferred
"open question" (OTDirect TCP bridge vs serial PIC emulator): **the TCP bridge is
not a viable injection point.**

## What already exists

`scripts/sat_boiler_emulator.py` (stdlib only, PowerShell-launchable) — committed.
It builds **correct** OT MsgID 3 (Slave Config / MemberID) and MsgID 0 (Status)
READ-ACK frames and has a `--dry-run` decode mode. It is useful as a frame-builder
/ reference even though its live-send mode cannot trip the gate. Its docstring and
`scripts/README.md` document the asymmetry above.

## Options (pick one when F7 is resumed)

### Option A — Serial-level PIC emulator
A host program that speaks the PIC serial protocol (the layer the firmware actually
talks to), emitting slave MsgID 3 + status responses so `handleMasterResponse()`
fills `otBoilerCacheValid[3]`.
- **Pros:** faithful — exercises the real code path the gate watches; no firmware
  change.
- **Cons:** not pure host-side — needs a serial loopback or a second device wired to
  the OTGW32's PIC serial; more bench setup; protocol-accurate framing required.
- **Effort:** medium-high (PIC serial protocol fidelity + hardware wiring).

### Option B — Firmware test-injection hook
A debug-only command (e.g. a Telnet/`debug` or REST `?test=` path, gated behind a
build flag or `state.debug.*`) that synthesizes a slave MsgID 3 into
`otBoilerCache` exactly as `handleMasterResponse()` would, so the existing host
script can trip the gate over TCP.
- **Pros:** pure host-side bench test (no extra hardware); reuses
  `scripts/sat_boiler_emulator.py`; fast, repeatable, CI-friendly.
- **Cons:** adds a test-only path into firmware (must be clearly gated and
  documented so it cannot fire in production); small attack/footgun surface.
- **Effort:** low-medium (one guarded injection function + a command to call it).

### Option C — Park F7 (current choice)
Keep the frame-builder/dry-run tool; validate the availability gate on real
hardware when a boiler is available on the bench. No new work.
- **Pros:** zero cost; the gate is still validated eventually on real hardware.
- **Cons:** no automated / boiler-free bench validation of the §4.2 ACs;
  regressions in the edge hook could go unnoticed until a hardware test.
- **Effort:** none.

## Recommendation (for when this is revisited)

**Option B** (firmware test-injection hook) gives the best cost/coverage trade-off:
it makes the availability-gate ACs repeatable on any bench (and in CI) with no
extra hardware, and the existing host script becomes the driver. The risk
(a test path in firmware) is contained by gating it behind a debug flag and
documenting it, the same discipline already used for `state.debug.*` switches.
Option A is the most faithful but the heaviest and needs hardware. Option C is the
status quo while parked.

## Current state

- F7 parked (Option C in effect).
- `scripts/sat_boiler_emulator.py` committed as a frame-builder/reference.
- TASK-802 set back to To Do (parked), pointing at this document.
