# ADR-082: ESP8266 Arduino Core 2.7.4 LTS pin for the 2.0.0 line

**Status:** Accepted
**Date:** 2026-04-24

## Context

The OTGW firmware migrated the ESP8266 target from Arduino Core 2.7.4 (via
`espressif8266@2.6.3` package) to Core 3.1.2 (`espressif8266@4.2.1`) during
the 1.4.x development cycle. Core 3.1.2 brought a modern toolchain (GCC
10.3, C++17 support, newer lwIP and libraries) and is Espressif's current
recommended release.

During field testing of 1.4.2-beta, multiple reporters observed OTA-reboot
stalls: after flashing new firmware the device reached the reset phase and
then hung with WiFi in a half-associated state (associated but services
like telnet/HTTP/MQTT failed to bind). Only a physical reset button press
recovered the device. Diagnostic sessions (TASK-397 BGTRACE instrumentation
and the OTA diagnostic report deep-research-report_arduino_core_3.1.2_reboot_issue_after_OTA.md)
traced the behaviour to an Arduino Core 3.1.0 breaking change:
[PR esp8266/Arduino#8598](https://github.com/esp8266/Arduino/pull/8598)
removed the implicit `WiFiClient::stopAll()` / `WiFiUDP::stopAll()` that
the `Update` path used to rely on. Without that implicit cleanup, lwIP
TCP sockets linger through the soft-reset and the WiFi SDK state persists
across `ESP.restart()`.

The LTS-1.5.x sibling branch was created to validate a full rollback to
Core 2.7.4 on the ESP8266 target. Field reports from that branch showed
materially calmer heap behaviour (14 KB free vs 7.9 KB, 10.2 KB max free
block vs 5.3 KB) and no OTA-reboot stalls, WebSocket drops, or MQTT drops
over a multi-hour soak. Core 2.7.4 was the baseline through the 1.3.x
production line (the stable firmware prior to the 1.4.x Core 3.1.2
migration), so the rollback returns the ESP8266 target to a
field-proven configuration.

The 2.0.0 line is dual-target (ESP8266 + ESP32). Rolling back the ESP8266
target to Core 2.7.4 while leaving the ESP32 target on its own toolchain
(pioarduino-espressif32 55.03.35, Arduino-ESP32 v3.3.5) gives us the
stability win on 1.x-deployed hardware without affecting the newer board.

### Constraints

- Core 2.7.4 ships with **GCC 4.8.2**. That compiler is stricter about
  template instantiation and ctags-generated forward declarations than
  GCC 10.3. Specifically, a template that references a name defined later
  in the translation unit fails on GCC 4.8.2 where GCC 10.3 resolved it
  via two-phase / ADL lookup. Forward declarations are required in
  `OTGW-firmware.h` for helpers used from template bodies (see
  `logBootSignature`, `requestDeferredReboot`, etc.).
- GCC 4.8.2 rejects AceTime 4.x's default-member-initialiser patterns
  (specifically array-typed default initialisers in class bodies) with
  "array used as initializer" during compile. AceTime must be pinned to
  v2.0.1, which predates those patterns.
- WebSockets ≥2.7 uses `WiFiServer::accept()` which only exists on Core
  3.x; WebSockets must be pinned to 2.3.6 for Core 2.7.4 compatibility.
- The SimpleTelnet submodule's `WiFiServer::accept` call is guarded with
  `ARDUINO_ESP8266_RELEASE_*` macros so the same submodule commit builds
  on both Cores; no further pin needed there.
- PlatformIO's `scripts/patch_pio_libs.py` patches NetApiHelpers 1.0.2
  for Core 3.x; the patch pattern is already satisfied on Core 2.7.4 so
  the script becomes a no-op — safe to keep running.

## Decision

Pin the ESP8266 PlatformIO environment for the 2.0.0 line to
`espressif8266@2.6.3` (= Arduino ESP8266 Core 2.7.4 LTS) with the
following library version constraints in the shared `[env]` block:

- `https://github.com/bxparks/AceTime.git#v2.0.1` (GitHub-tag pin; 4.x not
  registry-compatible and not GCC-4.8.2-compatible)
- `WebSockets @ 2.3.6`

Leave the ESP32 environment (`[env:esp32]`) unchanged on its own toolchain.

Add forward declarations in `OTGW-firmware.h` for helper functions used
from template bodies (`logBootSignature`, `requestDeferredReboot`,
`performDeferredReboot`, `isRebootPending`, `rebootHeapWatermarkTick`,
`getMinFreeHeap`, `maybeWarnFlashMismatch`) so GCC 4.8.2 template
instantiation succeeds.

## Alternatives Considered

### Alternative 1: Stay on Core 3.1.2 and fix the regression in-tree

**Pros:** Keeps the modern toolchain; access to newer libraries and
language features.

**Cons:** The root cause is upstream in the ESP8266 Arduino Core, not in
this firmware. Working around it (manual stopAll() calls, explicit
service teardown) mitigates but does not eliminate the underlying
lwIP-state-persistence behaviour. Field reports also show calmer heap on
2.7.4 independent of the reboot-stall, suggesting Core 3.x brings a
broader heap-fragmentation regression that would require further
in-tree patches.

**Why not chosen:** We would be fighting upstream indefinitely. The 1.3.x
production history proves 2.7.4 is stable; the 1.5.x soak on 2.7.4
confirms the stabilisation carries over to the current code.

### Alternative 2: Rollback on both targets (ESP8266 + ESP32)

**Pros:** Uniformity.

**Cons:** ESP32 does not have a "2.7.4 equivalent" — it has its own
unrelated toolchain. The concept does not apply. This alternative is
effectively a no-op for ESP32.

**Why not chosen:** The ESP32 env is unaffected by the Core 3.1.2 lwIP
regression; rolling it back is both impossible and unnecessary.

### Alternative 3: Keep Core 3.1.2 on 2.0.0 and wait for upstream fix

**Pros:** No immediate work; benefit from any future Core 3.x fix.

**Cons:** Upstream has not signalled a fix timeline for the stopAll()
change. In the meantime, 2.0.0 testers on ESP8266 hardware continue to
experience the OTA-reboot stall. Deferring leaves field users with a
known-broken OTA path.

**Why not chosen:** Blocks shipping 2.0.0-beta to testers with ESP8266
hardware.

## Consequences

### Positive

- **Stability on ESP8266:** Field soak of 1.5.x on Core 2.7.4 showed no
  OTA-reboot stalls, materially more heap headroom, and zero network-
  service drops over multi-hour runs.
- **Proven baseline:** 2.7.4 was the Core shipped with the 1.3.x
  production firmware; the risk profile is well understood.
- **ESP32 untouched:** The dual-target build keeps the ESP32 hardware on
  its modern Arduino-ESP32 v3.3.5 toolchain; only the ESP8266 pin is
  reverted. SAT, OTGW32, OTDirect features remain unaffected.
- **Library pins make the trade-off explicit:** Developers see the
  AceTime v2.0.1 and WebSockets 2.3.6 pins in `platformio.ini` and
  immediately understand the GCC 4.8.2 compatibility constraint.

### Negative

- **GCC 4.8.2 toolchain:** Loses C++14/17 features on the ESP8266 side.
  Template-instantiation forward-decl requirements mean that new helpers
  used from template bodies must be declared in `OTGW-firmware.h` before
  the template include.
- **Security updates:** Core 2.7.4's lwIP is older than 3.x's. For a
  LAN-scoped device behind a firewall (see ADR-003, ADR-032) this is
  low-impact, but it is a nonzero consideration.
- **Library version lag:** AceTime 2.0.1 predates the 4.x zoneinfo
  format improvements; WebSockets 2.3.6 lacks some newer feature work.
  Neither currently matters to OTGW functionality.
- **ESP8266 vs ESP32 toolchain divergence:** The two targets now sit on
  substantially different toolchain generations. The platform abstraction
  layer (ADR-061) already mediates the runtime differences; this ADR
  only extends the divergence to the compiler level.

### Neutral

- **Build time:** Core 2.6.3 downloads are of comparable size to 4.2.1
  (~20 MB). CI impact is negligible.
- **Flash/RAM footprint:** ESP8266 Core 2.7.4 firmware measured at
  ~77% flash and ~85% RAM on the current 2.0.0 codebase, within the
  operational envelope of the Wemos D1 mini with 4M2M partition layout.

## Related

- ADR-001 (ESP8266 Platform Selection) — superseded by ADR-061;
  originally pinned the platform with no specific Core version
  preference.
- ADR-013 (Arduino Framework over ESP-IDF) — unaffected; this ADR
  chooses a Core *version* within the Arduino framework, not the
  framework itself.
- ADR-015 (NTP AceTime Time Management) — this ADR introduces an
  explicit AceTime version pin (v2.0.1) that ADR-015 did not specify.
- ADR-061 (Unified ESP8266/ESP32 Platform Abstraction) — the abstraction
  layer that makes this single-target Core downgrade possible without
  affecting ESP32.
- TASK-397 — diagnostic instrumentation that identified the Core 3.1.2
  reboot regression.
- TASK-398 — LTS branch fork that validated the rollback.
- TASK-399 — SimpleTelnet printf-stack bump that accompanied this work.
- `platformio.ini` commit `08585837` — applies this pin.
- PR [esp8266/Arduino#8598](https://github.com/esp8266/Arduino/pull/8598) —
  upstream breaking change that motivates the rollback.
- `docs/research/deep-research-report_arduino_core_3.1.2_reboot_issue_after_OTA.md`
  — field diagnostic evidence for the Core 3.1.2 OTA-reboot regression
  (the report that prompted this rollback decision).
