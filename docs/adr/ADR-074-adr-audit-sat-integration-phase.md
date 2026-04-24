# ADR-074: ADR Audit — SAT Integration Phase

**Status:** Accepted
**Date:** 2026-04-09

## Context

The SAT integration (ADR-085 plus ADR-063, ADR-064, ADR-087, ADR-066 through ADR-073; originally written as ADR-062 through ADR-073, renumbered on 2026-04-24 per TASK-412) introduced significant new subsystems:
weather compensation, PID control, BLE sensors, OPV calibration, cycle tracking, and a
large new MQTT topic surface. Before closing the SAT audit phase, all existing ADRs were
reviewed for conflicts, outdated decisions, or superseded guidance.

This ADR records the findings of that review and acts as the authoritative record of
which ADRs were found outdated and what action was taken.

## Findings

### ADR-018: ArduinoJson for Data Interchange — already superseded

**Status: Already handled.** ADR-018 was superseded by ADR-042 (Streaming JSON, No
ArduinoJson) on 2026-02-28. The status line in the file reads:

> `**Status:** Superseded by ADR-042`

The file is correctly marked. SAT code correctly follows ADR-042: it uses
`snprintf_P` / `sendJsonMapEntry` / `dtostrf` for all JSON generation and does not
import or use ArduinoJson. No further action required.

### ADR-004: Static Buffer Allocation — already superseded

**Status: Already handled.** ADR-004 was superseded by ADR-053 (Large Feature Buffer
Static Allocation) on 2026-03-21. The status line reads:

> `**Status:** Superseded by ADR-053`

SAT code follows ADR-053 rules: static file-scope variables for algorithm state,
PROGMEM for constant tables, no `new`/`malloc`. One documented String exception in
`SATweather.ino` is consistent with the ADR-004 / ADR-049 one-off allowance.
ADR-070 (SAT Memory Allocation, this phase) documents the SAT-specific application
of these rules. No further action required on ADR-004 itself.

### ADR-001: ESP8266 Platform Selection — already superseded

**Status: Already handled.** ADR-001 was superseded by ADR-061 (Unified Platform
Abstraction). SAT runs correctly on both ESP8266 and ESP32-S3 (OTGW32) with no
platform-conditional code in SAT source files (documented in ADR-072).
No further action required.

### Duplicate ADR-061 filename — file naming conflict

**Status: Resolved.** Renamed `ADR-061-wifi-reconnect-timeout-tuning.md` to
`ADR-075-wifi-reconnect-timeout-tuning.md`. ADR-047 updated to reference ADR-075.

Two separate ADR files shared the number 061. The WiFi reconnect tuning ADR was
assigned the wrong number — it supersedes parameter values from ADR-047, not ADR-001.

| File | Content |
|------|---------|
| `ADR-061-unified-esp8266-esp32-platform-abstraction.md` | Unified ESP8266/ESP32 platform abstraction layer |
| `ADR-075-wifi-reconnect-timeout-tuning.md` | WiFi reconnect timeout parameter tuning (renumbered from 061) |

### ADR-006: MQTT Integration Pattern — still current

ADR-006 defines the `value/<nodeId>/` and `set/<nodeId>/` topic structure. SAT topics
follow this structure exactly (via the `sat/` sub-prefix). No conflict. ADR-073
(SAT MQTT Topic Structure) extends this pattern rather than superseding it.

### ADR-016: OpenTherm Command Queue — still current

SAT issues all boiler commands via `addOTWGcmdtoqueue()` / `addCommandToQueue()`, exactly
as mandated by ADR-016. The command queue contract is unchanged by SAT integration.

### ADR-038: OpenTherm Data Flow Pipeline — still current, now extended

ADR-038 documents the `processOT()` → MQTT publisher pipeline. SAT reads from
`OTcurrentSystemState` which is populated by this pipeline. The pipeline is unchanged;
SAT is a consumer, not a modifier. ADR-072 (SAT Platform Compatibility) documents how
SAT fits into this pipeline.

### ADR-041: JIT HA Discovery — still current

SAT uses `mqttha.cfg` for HA auto-discovery, consistent with ADR-041. The `mqttha.cfg`
file has been extended with SAT entity definitions. No conflict.

### ADR-051: Dual Encapsulating Structs — still current, extended

ADR-051 defines the `settings.sat.*` and `state.sat.*` pattern. SAT implementation
follows this exactly. ADR-070 (SAT Memory Allocation) documents SAT's specific additions
to these structs.

### ADR-049: String Prohibition in Protocol Paths — still current

SAT follows this rule. One documented String exception in `SATweather.ino` (HTTP
payload) is consistent with the one-off allowance. ADR-070 documents this exception.

### SAT-phase ADRs (ADR-063..069 + ADR-085 + ADR-087) — all accepted, no conflicts

All ADRs written for the SAT integration phase (ADR-085 SAT overview [renumbered from 062],
063 OTGW32 hardware, 064 OT-direct mode, ADR-087 frame bridge [renumbered from 065],
066 thermostat auto-detection, 067 OLED library, 068 OT-direct schedule tuning,
069 PID v3) are consistent with each other and with ADR-070 through ADR-073 written in
this audit phase.

## Decision

No existing ADRs require status changes beyond those already applied. The audit confirms:

1. ADR-004, ADR-018, ADR-001 are correctly marked as superseded.
2. ADR-006, ADR-016, ADR-038, ADR-041, ADR-049, ADR-051 are current and consistent
   with SAT integration.
3. ADR-063, ADR-064, ADR-066 through ADR-073, plus ADR-085 (renumbered from 062) and ADR-087 (renumbered from 065), are the authoritative records for all SAT-related decisions.
4. The ADR-061 filename collision was resolved: the WiFi reconnect timeout ADR is now
   ADR-075. ADR-047 updated to reference ADR-075.

## Consequences

### Benefits

- SAT integration is fully documented across ADRs 063-073 plus ADR-085 and ADR-087 (originally authored as ADR-062 and ADR-065; renumbered via TASK-412) with no gaps
- No existing ADRs are in conflict with SAT code
- The one outdated-status issue (ADR-018, ADR-004, ADR-001) was already resolved before
  this audit
- ADR-071 (heating curve), ADR-072 (platform layer), ADR-073 (MQTT topics) fill three
  documentation gaps that existed since SAT code was merged

### Risks

- The ADR-061 filename collision has been resolved: the WiFi reconnect timeout ADR is now
  ADR-075. ADR-047 was updated to reference ADR-075 instead of ADR-061.

## Related

- ADR-004: Static Buffer Allocation (superseded by ADR-053)
- ADR-018: ArduinoJson (superseded by ADR-042)
- ADR-001: ESP8266 Platform Selection (superseded by ADR-061 platform abstraction)
- ADR-053: Large Feature Buffer Static Allocation
- ADR-042: Streaming JSON — No ArduinoJson
- ADR-061: Unified ESP8266/ESP32 Platform Abstraction
- ADR-063, ADR-064, ADR-066 through ADR-073, ADR-085 (renumbered from 062), ADR-087 (renumbered from 065): SAT integration ADRs
- ADR-075: WiFi Reconnect Timeout Tuning (renumbered from ADR-061)
- ADR-070: SAT Memory Allocation (this audit phase)
- ADR-071: SAT Heating Curve (this audit phase)
- ADR-072: SAT Platform Compatibility (this audit phase)
- ADR-073: SAT MQTT Topic Structure (this audit phase)
