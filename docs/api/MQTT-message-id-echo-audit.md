# MQTT MsgID Echo Audit (OpenTherm v4.2)

**Status**: Phase 0 spec audit deliverable for TASK-478 (B-hybrid fix for master-topic flapping). Defines the `bSlaveEchoesValue` flag that gates `/boiler` subtopic publication when source-separated MQTT topics are enabled.

**Source**: `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`. Cross-referenced against captured boiler logs (Intergas, dev branch 1.5.0-beta+cd30617).

---

## Decision rule

For each MsgID with master-write support (`-/W` or `R/W`):

- **echo = true**  : slave stores the written value and returns it in the Write-Ack data field. Publishing the Write-Ack to the `/boiler` subtopic is meaningful (the value is the slave's stored representation, possibly clamped or modified).
- **echo = false** : slave acknowledges receipt but the data field of the Write-Ack response is per-spec undefined (typically returned as 0). Publishing the Write-Ack to the `/boiler` subtopic produces a fake reading and is suppressed.

For Read-only MsgIDs (`R/-`), the `bSlaveEchoesValue` flag is **not consulted** for publication (no Write-Ack ever occurs). The struct field still exists for completeness and is set to `true` by default.

When the spec or evidence is ambiguous, the conservative default is `true` (publish). Rationale: better one fake-zero in `/boiler` than a missed meaningful echo.

---

## Audit table

| MsgID | Name | Direction | Class | bSlaveEchoesValue | Reason |
|------:|------|:---------:|:-----:|:-----------------:|--------|
| 0  | Status                              | R/- | 1 | true  | Read-only; flag value moot. Special bidirectional status exchange. |
| 1  | TSet (Control Setpoint)             | -/W | 1 | **false** | **Confirmed non-echo on heat-pump controllers**. Spec is ambiguous (Class 1 -/W; data field of Write-Ack not explicitly defined). Field tester (dev beta.25, 2026-05-07) reported persistent flap between override values and 0 on `<topic>_boiler` for MsgID 1 — heat-pump returns protocol-zero in the Write-Ack data field instead of echoing the master value. Most boilers (Intergas, Remeha) echo, but the spec permits both. Flag flipped to suppress the zero on the boiler-side worldview; canonical and `_boiler` topics now show the master's intended setpoint. |
| 2  | M-Config / M-MemberIDcode           | -/W | 2 | true  | Slave responds (per spec "S must respond"); response semantics include slave's own MemberID. Conservative. |
| 3  | S-Config / S-MemberIDcode           | R/- | 2 | true  | Read-only; flag moot. |
| 4  | Remote Request                      | -/W | 3 | true  | Special: HB = Request-Code, LB = Response-Code. Slave's response code is meaningful, not an echo, but conservative default. |
| 5  | ASF-flags / OEM-fault-code          | R/- | 1 | true  | Read-only; flag moot. |
| 6  | RBP-flags                           | R/- | 5 | true  | Read-only; flag moot. |
| 7  | Cooling-control                     | -/W | 8 | **false** | Class 8 `-/W` control write; spec ambiguous on Write-Ack data field. Flipped 2026-05-07 alongside MsgID 1 under the defensive-defaults policy: when the spec does not require echo, suppressing the slave's Write-Ack data byte avoids tester-visible flap on slaves that return protocol-zero. No direct field evidence yet (most testers don't exercise cooling). |
| 8  | TsetCH2 (Control Setpoint CH2)      | -/W | 1 | **false** | Parallel to MsgID 1 — same Class 1 `-/W` controller-side write, same f8.8 shape, same heat-pump non-echo risk by direct analogy. Flipped 2026-05-07 alongside MsgID 1. |
| 9  | TrOverride                          | R/- | 8 | true  | Read-only; flag moot. |
| 10 | TSP count                           | R/- | 6 | true  | Read-only; flag moot. |
| 11 | TSP-index / TSP-value               | R/W | 6 | true  | Transparent Slave Parameter; slave stores written value by definition. |
| 12 | FHB-size                            | R/- | 7 | true  | Read-only; flag moot. |
| 13 | FHB-index / FHB-value               | R/- | 7 | true  | Read-only; flag moot. |
| 14 | Max-rel-mod-level-setting           | -/W | 8 | **false** | **Confirmed non-echo**. Spec note "Partial (S must respond)" + Intergas log shows `T900E6400` (master 100%) → `BD00E0000` (slave 0%). Slave does not store. |
| 15 | Max-Capacity / Min-Mod-Level        | R/- | 8 | true  | Read-only; flag moot. |
| 16 | TrSet (Room Setpoint)               | -/W | 4 | **false** | **Confirmed non-echo**. Class 4 sensor data from master; slave does not regulate on room setpoint. Intergas log: `T10101400` (master 20°C) → `BD0100000` (slave 0°C, marked `<ignored>`). |
| 17 | Rel.-mod-level                      | R/- | 4 | true  | Read-only; flag moot. |
| 18 | CH-pressure                         | R/- | 4 | true  | Read-only; flag moot. |
| 19 | DHW-flow-rate                       | R/- | 4 | true  | Read-only; flag moot. |
| 20 | Day-Time                            | R/W | 4 | true  | Slave stores when master writes. |
| 21 | Date                                | R/W | 4 | true  | Slave stores when master writes. |
| 22 | Year                                | R/W | 4 | true  | Slave stores when master writes. |
| 23 | TrSetCH2 (Room Setpoint CH2)        | -/W | 4 | **false** | Parallel to MsgID 16 for second heating circuit; same non-echo semantics by spec analogy. |
| 24 | Tr (Room temperature)               | -/W | 4 | **false** | **Confirmed non-echo**. Class 4 sensor data from master; slave does not regulate on room temperature. Intergas log: `T9018140F` (master 20.06°C) → `B50180000` (slave 0°C). User-reported flapping origin. |
| 25 | Tboiler                             | R/- | 4 | true  | Read-only; flag moot. |
| 26 | Tdhw                                | R/- | 4 | true  | Read-only; flag moot. |
| 27 | Toutside                            | R/W | 4 | true  | R/W: slave can store master-supplied outside temperature override. Echo expected. |
| 28 | Tret                                | R/- | 4 | true  | Read-only; flag moot. |
| 29 | Tstorage                            | R/- | 4 | true  | Read-only; flag moot. |
| 30 | Tcollector                          | R/- | 4 | true  | Read-only; flag moot. |
| 31 | TflowCH2                            | R/- | 4 | true  | Read-only; flag moot. |
| 32 | Tdhw2                               | R/- | 4 | true  | Read-only; flag moot. |
| 33 | Texhaust                            | R/- | 4 | true  | Read-only; flag moot. |
| 34 | Tboiler-heat-exchanger              | R/- | 4 | true  | Read-only; flag moot. |
| 35 | Boiler fan speed                    | R/- | 4 | true  | Read-only; flag moot. |
| 36 | Flame current                       | R/- | 4 | true  | Read-only; flag moot. |
| 37 | TrCH2 (Room temp for CH2)           | -/W | 4 | **false** | Parallel to MsgID 24 for second heating circuit; same non-echo semantics by spec analogy. |
| 38 | Relative Humidity                   | R/W | 4 | true  | Slave stores when master writes. |
| 39 | TrOverride 2                        | R/- | 8 | true  | Read-only; flag moot. |
| 48 | TdhwSet-UB / TdhwSet-LB             | R/- | 5 | true  | Read-only; flag moot. |
| 49 | MaxTSet-UB / MaxTSet-LB             | R/- | 5 | true  | Read-only; flag moot. |
| 56 | TdhwSet                             | R/W | 5 | true  | Pre-defined remote boiler parameter; slave stores. Intergas log shows OTGW gateway intercepting at 55°C. |
| 57 | MaxTSet                             | R/W | 5 | true  | Pre-defined remote boiler parameter; slave stores. Intergas log: master writes 36, OTGW clips to 31, slave stores 65 (clamped to its own limit). Echo behavior is exactly why source-separation matters for this MsgID. |
| 70 | Status ventilation/heat-recovery    | R/- | 1 | true  | Read-only; flag moot. |
| 71 | Vset                                | -/W | 1 | **false** | Class 1 `-/W` V/H control write; spec ambiguous on Write-Ack data field. Flipped 2026-05-07 alongside MsgID 1 under the defensive-defaults policy. No direct V/H field evidence yet. |
| 72 | ASF-flags ventilation               | R/- | 1 | true  | Read-only; flag moot. |
| 73 | OEM diagnostic ventilation          | R/- | 1 | true  | Read-only; flag moot. |
| 74 | S-Config ventilation                | R/- | 2 | true  | Read-only; flag moot. |
| 75 | OpenTherm version ventilation       | R/- | 2 | true  | Read-only; flag moot. |
| 76 | Ventilation/heat-recovery version   | R/- | 2 | true  | Read-only; flag moot. |
| 77 | Rel-vent-level                      | R/- | 4 | true  | Read-only; flag moot. |
| 78 | RH-exhaust                          | R/W | 4 | true  | Slave stores when master writes. |
| 79 | CO2-exhaust                         | R/W | 4 | true  | Slave stores when master writes. |
| 80 | Tsi                                 | R/- | 4 | true  | Read-only; flag moot. |
| 81 | Tso                                 | R/- | 4 | true  | Read-only; flag moot. |
| 82 | Tei                                 | R/- | 4 | true  | Read-only; flag moot. |
| 83 | Teo                                 | R/- | 4 | true  | Read-only; flag moot. |
| 84 | RPM-exhaust                         | R/- | 4 | true  | Read-only; flag moot. |
| 85 | RPM-supply                          | R/- | 4 | true  | Read-only; flag moot. |
| 86 | RBP-flags ventilation               | R/- | 5 | true  | Read-only; flag moot. |
| 87 | Nominal ventilation                 | R/W | 5 | true  | Slave stores when master writes. |
| 88 | TSP count ventilation               | R/- | 6 | true  | Read-only; flag moot. |
| 89 | TSP ventilation                     | R/W | 6 | true  | Transparent Slave Parameter; slave stores. |
| 90 | FHB-size ventilation                | R/- | 7 | true  | Read-only; flag moot. |
| 91 | FHB ventilation                     | R/- | 7 | true  | Read-only; flag moot. |
| 93 | Brand                               | R/- | 2 | true  | Read-only; flag moot. |
| 94 | Brand Version                       | R/- | 2 | true  | Read-only; flag moot. |
| 95 | Brand Serial Number                 | R/- | 2 | true  | Read-only; flag moot. |
| 96 | Cooling Operation Hours             | R/W | 4 | true  | Counter; slave stores when master writes (typically zero-reset). |
| 97 | Power Cycles                        | R/W | 4 | true  | Counter; slave stores when master writes. |
| 98 | RF sensor status information        | -/W | 4 | **false** | Master tells slave RF sensor type and signal info; slave does not store, only acts on it. Spec class 4 with no `R` direction makes this purely informational. |
| 99 | Remote Override Operating Mode      | R/W | 8 | true  | Slave stores operating mode when master writes. |
| 100 | Remote override function            | R/- | 8 | true  | Read-only; flag moot. |
| 101 | Status Solar Storage                | R/- | 1 | true  | Read-only; flag moot. |
| 102 | ASF-flags Solar Storage             | R/- | 1 | true  | Read-only; flag moot. |
| 103 | S-Config Solar Storage              | R/- | 2 | true  | Read-only; flag moot. |
| 104 | Solar Storage version               | R/- | 2 | true  | Read-only; flag moot. |
| 105 | TSP count Solar Storage             | R/- | 6 | true  | Read-only; flag moot. |
| 106 | TSP Solar Storage                   | R/W | 6 | true  | Transparent Slave Parameter; slave stores. |
| 107 | FHB-size Solar Storage              | R/- | 7 | true  | Read-only; flag moot. |
| 108 | FHB Solar Storage                   | R/- | 7 | true  | Read-only; flag moot. |
| 109 | Electricity producer starts         | R/W | 4 | true  | Counter; slave stores when master writes (typically zero-reset). |
| 110 | Electricity producer hours          | R/W | 4 | true  | Counter; slave stores when master writes. |
| 111 | Electricity production              | R/- | 4 | true  | Read-only; flag moot. |
| 112 | Cumulative Electricity production   | R/W | 4 | true  | Counter; slave stores when master writes. |
| 113 | Unsuccessful burner starts          | R/W | 4 | true  | Counter; slave stores when master writes. |
| 114 | Flame signal too low count          | R/W | 4 | true  | Counter; slave stores when master writes. |
| 115 | OEM diagnostic code                 | R/- | 1 | true  | Read-only; flag moot. |
| 116 | Successful Burner starts            | R/W | 4 | true  | Counter; slave stores when master writes. |
| 117 | CH pump starts                      | R/W | 4 | true  | Counter; slave stores when master writes. |
| 118 | DHW pump/valve starts               | R/W | 4 | true  | Counter; slave stores when master writes. |
| 119 | DHW burner starts                   | R/W | 4 | true  | Counter; slave stores when master writes. |
| 120 | Burner operation hours              | R/W | 4 | true  | Counter; slave stores when master writes. |
| 121 | CH pump operation hours             | R/W | 4 | true  | Counter; slave stores when master writes. |
| 122 | DHW pump/valve operation hours      | R/W | 4 | true  | Counter; slave stores when master writes. |
| 123 | DHW burner operation hours          | R/W | 4 | true  | Counter; slave stores when master writes. |
| 124 | OpenTherm version Master            | -/W | 2 | true  | Default conservative. Slave acknowledges but data field semantics not explicit. |
| 125 | OpenTherm version Slave             | R/- | 2 | true  | Read-only; flag moot. |
| 126 | Master-version                      | -/W | 2 | true  | Default conservative. |
| 127 | Slave-version                       | R/- | 2 | true  | Read-only; flag moot. |

---

## Summary

- **MsgIDs marked `bSlaveEchoesValue=false`**: 10 entries (1, 7, 8, 14, 16, 23, 24, 37, 71, 98). Three groups:
  - **Class 4 `-/W` informational sensor writes** (14, 16, 23, 24, 37): per-spec undefined Write-Ack data field; slave does not store these values. Original audit (TASK-478).
  - **Master-to-slave informational write** (98 RFstrengthbatterylevel): slave does not store, only acts on it.
  - **Class 1 / Class 8 `-/W` control-direction writes** (1 TSet, 7 Cooling-control, 8 TsetCH2, 14 MaxRelModLevelSetting, 71 Vset): spec is ambiguous about Write-Ack data — both echo and protocol-zero are spec-compliant. Defensive-defaults policy (added 2026-05-07): suppress when spec does not REQUIRE echo, so user-visible flap on non-echo slaves is impossible by construction. MsgID 1 confirmed by heat-pump tester report on dev beta.25; MsgIDs 7, 8, 71 flipped under the same policy without direct field evidence (asymmetric cost: missing an echo is informational, seeing a flap is visibly broken).
- **MsgIDs marked `bSlaveEchoesValue=true`**: all others (~66 entries). For Read-only MsgIDs the flag is moot. For write-supported MsgIDs (counters, TSPs, etc.) the slave is required by spec to store and echo, so `true` is correct.

## Future extensions

If a user reports flapping on a MsgID currently set to `true`, capture an OpenTherm log showing the write/ack pair, verify the slave returns 0 (or other non-meaningful value) in the Write-Ack, and flip the flag to `false` with the log evidence cited in this audit doc.

All Class 1 / Class 8 `-/W` control-direction candidates have been flipped to `false` as of 2026-05-07 (TSet, Cooling-control, TsetCH2, Vset). Future flips would target Class 2 / Class 4 R/W counter or configuration writes if a tester reports a similar pattern; the current spec analysis suggests those slaves are obligated to echo, so they should remain `true` until evidence shows otherwise.
- 2 (M-Config), 4 (Remote Request), 124 (OT version Master), 126 (Master-version): protocol-handshake writes with ambiguous Write-Ack data semantics.

## References

- Source spec: `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`
- Implementation task: TASK-478
- Plan: `C:\Users\rvdbr\.claude\plans\the-design-package-still-elegant-globe.md`
- User-reported regression: flapping of `Tr` (room temperature), `TrSet` (room setpoint), `MaxRelModLevelSetting` since v1.4.1 in MQTT and HA UI.
