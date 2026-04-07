---
# METADATA
Document Title: Deep Audit – OpenTherm v4.2 Spec vs Firmware Implementation
Review Date: 2026-04-07 22:14:33 UTC
Branch Reviewed: copilot/audit-opentherm-spec-implementation (based on dev HEAD 17b0f51)
Target Version: v1.3.6-beta
Reviewer: GitHub Copilot Advanced Agent (Expert C++ / Firmware mode)
Document Type: Protocol Compliance Deep Audit
Reference Spec: docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.pdf
Status: COMPLETE
---

# OpenTherm v4.2 Deep Audit – Firmware Implementation Assessment

## 1. Executive Summary

This document is the result of a complete cross-reference between the OpenTherm Protocol
Specification v4.2 (10 November 2020) and the OTGW-firmware codebase as of v1.3.6-beta.

**Overall verdict: the implementation is of high quality.** The previous compliance review
(2026-02-15) addressed all critical and high-priority issues. The remaining gaps are minor,
well-documented, or represent deliberate design trade-offs appropriate for a gateway device.

### Scorecard

| Area | Score | Notes |
|------|-------|-------|
| Frame format / bit extraction | PASS | Correct MSG-TYPE, DATA-ID, HB/LB extraction |
| Parity handling | PASS | Delegated to PIC; 'E' prefix correctly handled |
| f8.8 codec | PASS | Two's-complement correct; see section 3.1 |
| Data-type mapping (all IDs) | PASS | See section 4 for per-ID analysis |
| R/W direction mapping | PASS | All 10 mismatches fixed in 2026-02-15 review |
| Reserved IDs (v4.x profile) | PASS | IDs 40-47, 50-55, 58-69, 92, 128+ correctly handled |
| v4.x auto-detection | PASS | Float-safe comparison against OT version >= 4.0 |
| Delayed-message / override logic | PASS | B->A / T->R skip logic correctly implemented |
| Mandatory IDs (spec section 5.2.1) | PASS | All 7+3 mandatory IDs handled |
| Status bits (ID 0, 70, 101) | PASS | All bits correctly decoded per spec section 6 |
| v4.2 new IDs (39, 93-99) | PASS | Added in 2026-02-15 review |
| MQTT throttle / de-duplication | PASS | Well-designed sliding-window throttle |
| ID 38 comment accuracy | FIXED | Was incorrect; corrected in this PR |
| WRITE-ACK capture for pure WRITE | GAP | Design choice; see section 6.3 |
| Brand string accumulation (93-95) | GAP | Partial; see section 6.4 |
| ID 100 OTmap type label | MINOR | ot_flag8 is imprecise; implementation reads LB correctly |

---

## 2. Protocol Frame Handling

### 2.1 Frame format (spec section 3)

The OT spec defines a 34-bit frame:

    START | P | MSG-TYPE(3) | SPARE(4) | DATA-ID(8) | DATA-VALUE(16) | STOP

The firmware extracts the relevant fields correctly:

```cpp
// OTGW-Core.ino
OTdata.type        = (value >> 28) & 0x7;          // 3-bit MSG-TYPE
OTdata.masterslave = (OTdata.type >> 2) & 0x1;     // MSB of type: 0=master, 1=slave
OTdata.id          = (value >> 16) & 0xFF;          // 8-bit DATA-ID
OTdata.valueHB     = (value >> 8) & 0xFF;           // HIGH byte of DATA-VALUE
OTdata.valueLB     = value & 0xFF;                  // LOW byte of DATA-VALUE
```

**Assessment: Correct.** The 4 SPARE bits (bits 27:24) are implicitly discarded since the
ID field is extracted from bits 23:16.

### 2.2 Parity (spec: "even parity over 32 bits")

The firmware does **not** independently verify parity. It relies on the PIC microcontroller
which sits on the physical OpenTherm bus. When the PIC detects a parity error it sends the
frame prefixed with 'E'. The firmware correctly identifies and marks these:

```cpp
} else if (buf[0]=='E') {
    OTdata.rsptype = OTGW_PARITY_ERROR;
}
// later:
OTdata.skipthis |= (OTdata.rsptype == OTGW_PARITY_ERROR);
```

**Assessment: Correct for a gateway role.** The PIC owns the physical layer.

### 2.3 Message type decoding

| Binary | Spec constant   | Firmware constant       | Match |
|--------|-----------------|------------------------|-------|
| 000    | READ-DATA       | OT_READ_DATA (0)        | PASS  |
| 001    | WRITE-DATA      | OT_WRITE_DATA (1)       | PASS  |
| 010    | INVALID-DATA    | OT_INVALID_DATA (2)     | PASS  |
| 011    | (reserved)      | OT_RESERVED (3)         | PASS  |
| 100    | READ-ACK        | OT_READ_ACK (4)         | PASS  |
| 101    | WRITE-ACK       | OT_WRITE_ACK (5)        | PASS  |
| 110    | DATA-INVALID    | OT_DATA_INVALID (6)     | PASS  |
| 111    | UNKNOWN-DATA-ID | OT_UNKNOWN_DATA_ID (7)  | PASS  |

---

## 3. Data Type Codec Verification

### 3.1 f8.8 (signed fixed-point, spec section 1)

Spec definition: "1 sign bit, 7 integer bits, 8 fractional bits. LSB = 1/256. Two's complement."

Firmware implementation:

```cpp
float OpenthermData_t::f88() {
    float value = (int8_t) valueHB;   // sign-extends HB correctly
    return value + (float)valueLB / 256.0f;
}

void OpenthermData_t::f88(float value) {
    int16_t fixed = (int16_t)(value * 256.0f);
    valueHB = (uint8_t)((fixed >> 8) & 0xFF);
    valueLB = (uint8_t)(fixed & 0xFF);
}
```

Verification against spec examples:

| Spec example     | Expected | Firmware result                              |
|------------------|----------|----------------------------------------------|
| +21.50 C (0x1580)| +21.50   | (int8_t)0x15 + 0x80/256 = 21 + 0.5 = 21.5  |
| -5.25 C (0xFAC0) | -5.25    | (int8_t)0xFA + 0xC0/256 = -6 + 0.75 = -5.25 |
| +100.00 C (0x6400)| +100.0  | (int8_t)0x64 + 0 = 100.0                     |
| -128.00 (0x8000) | -128.0   | (int8_t)0x80 + 0 = -128.0                    |

**Assessment: Correct.** The cast `(int8_t) valueHB` properly sign-extends the high byte.

One precision note: `(int16_t)(value * 256.0f)` could theoretically suffer from FP rounding
near exact boundary values. For HVAC temperatures the maximum error is +/-1/256 = 0.004 C,
which is negligible.

### 3.2 s16 (signed 16-bit)

Used for Tsolarcollector (ID 30) and Texhaust (ID 33). Implementation:

```cpp
int16_t OpenthermData_t::s16() {
    int16_t value = valueHB;
    return ((value << 8) + valueLB);
}
```

`(int16_t)valueHB << 8` sign-extends HB and combines with unsigned LB. Correct.

### 3.3 s8/s8 (two signed bytes)

Used for DHW and MaxTSet bounds (IDs 48, 49):

```cpp
AddLogf("%s = %3d / %3d", label, (int8_t)OTdata.valueHB, (int8_t)OTdata.valueLB);
```

Both bytes independently cast to int8_t. Correct.

---

## 4. Message ID Coverage

### 4.1 Reserved IDs – correctly marked OT_UNDEF

| Range | Spec status             | Firmware handling                              |
|-------|-------------------------|------------------------------------------------|
| 40-47 | Reserved (future use)   | OT_UNDEF — correct                            |
| 50-55 | Reserved in v4.x        | ot_s8s8 / OT_READ — filtered by isLegacyPreV42CompatibilityId() |
| 58-63 | Reserved in v4.x        | ot_f88 / OT_RW — filtered by isLegacyPreV42CompatibilityId() |
| 64-69 | Reserved                | OT_UNDEF — correct                            |
| 92    | Reserved                | OT_UNDEF — correct                            |
| 128+  | Reserved / vendor       | OT_UNDEF (128-130); Remeha vendor (131-133)   |

Note: IDs 56 (TdhwSet) and 57 (MaxTSet) are **not** in the reserved set:

```cpp
static bool isLegacyPreV42CompatibilityId(uint8_t msgid) {
    return (msgid >= 50U && msgid <= 55U) || (msgid >= 58U && msgid <= 69U);
    // 56 and 57 are absent — correctly preserved in v4.x
}
```

This matches the spec which explicitly keeps IDs 56-57 as valid in v4.2.

### 4.2 v4.x auto-detection

```cpp
static bool useV4xReservedIdRules() {
    return (OTcurrentSystemState.OpenThermVersionSlave >= 4.0f) ||
           (OTcurrentSystemState.OpenThermVersionMaster >= 4.0f);
}
```

The float threshold `>= 4.0f` is safe: f8.8 encodes 4.0 as 0x0400 which decodes exactly to
4.0f in single-precision float (exact representable value in IEEE 754).

### 4.3 Complete per-ID audit (all 128 spec-defined IDs)

#### Class 1 — Control and Status (IDs 0-15)

All 16 IDs correctly mapped. Key checks:
- ID 0 (Status): flag8/flag8, OT_READ — correct with special-case in is_value_valid() for master READ-DATA frame
- ID 2 (MasterConfig): flag8/u8, OT_WRITE — master sends, never reads
- ID 3 (SlaveConfig): flag8/u8, OT_READ — slave responds, mandatory
- ID 6 (RBP flags): flag8/flag8, OT_READ — transfer-enable and read/write flags both decoded

#### Class 3 — Remote Commands (IDs 16-24)

- ID 20 (DayTime): Hour mask bug (0x1F) was fixed in 2026-02-15 review. Verified correct now.
- IDs 16,23,24 correctly OT_WRITE (master-to-slave only)
- IDs 17,18,19 correctly OT_READ (slave-to-master only)

#### Class 2 — Temperatures (IDs 25-34)

All correctly f8.8 or s16. ID 27 (Toutside) correctly OT_RW after 2026-02-15 fix.

#### Class 6 — Boiler Sensors (IDs 35-39)

- ID 35 (FanSpeed): ot_u8u8 — both HB (setpoint Hz) and LB (actual Hz) published. Correct.
- ID 36 (ElectCurrentBurnerFlame): f8.8, read-only. Correct.
- ID 37 (TRoomCH2): f8.8, OT_WRITE. Fixed in 2026-02-15 review.
- ID 38 (RelativeHumidity): f8.8, OT_RW. Comment corrected in this PR.
- ID 39 (TrOverride2): f8.8, OT_READ. Added in 2026-02-15 review.

#### Undefined range (IDs 40-47): All OT_UNDEF. Correct.

#### Class 4 — Remote Parameters (IDs 48-63)

- IDs 48-49: s8/s8 bounds, OT_READ. Correct.
- IDs 50-55: Pre-v4.2 legacy, filtered in v4.x mode. Correct.
- IDs 56-57: TdhwSet/MaxTSet, OT_RW, f8.8. Preserved in v4.x. Correct.
- IDs 58-63: Pre-v4.2 legacy, filtered in v4.x mode. Correct.

#### Class 5 — Ventilation/Heat Recovery (IDs 70-91)

All 22 defined IDs correctly mapped. Key spots:
- ID 71 (ControlSetpointVH): u8, LB only per spec note — print_u8_lb() used. Correct.
- ID 77 (RelativeVentilation): u8, LB only per spec note — print_u8_lb() used. Correct.
- ID 78 (RelHumidityExhaustAir): u8, LB only per spec note — print_u8_lb() used. Correct.
- ID 87 (NominalVentilationValue): u8, HB per spec — print_u8_hb() used. Correct.
- ID 86 (RemoteParameterSettingVH): flag8/flag8. Transfer-enable and read/write flags.

#### IDs 93-100 (v4.2 new IDs)

- IDs 93-95 (Brand/BrandVersion/BrandSerialNumber): u8u8 (index+char), OT_READ. Added 2026-02-15.
- IDs 96-97 (CoolingOperationHours/PowerCycles): u16, OT_RW. Added 2026-02-15.
- ID 98 (RFstrengthbatterylevel): ot_special, OT_WRITE. Full decode of sensor type, index, signal, battery.
- ID 99 (OperatingMode_HC1_HC2_DHW): ot_special, OT_RW. Full decode of DHW/HC1/HC2 modes.
- ID 100 (RemoteOverrideFunction): ot_flag8, OT_READ. See section 6.2 for minor type note.

#### IDs 101-127 (Solar, counters, versions)

All 27 IDs correctly mapped. All counter IDs (116-123) as u16. Solar storage (101-108) fully
implemented with own status decode. Mandatory IDs 125 and 127 correctly OT_READ.

---

## 5. Status Flag Bit Verification

### 5.1 ID 0 Master Status HB — spec section 6.1

| Bit | Spec name          | Firmware topic     | Match |
|-----|--------------------|--------------------|-------|
| 0   | CH enable          | ch_enable          | PASS  |
| 1   | DHW enable         | dhw_enable         | PASS  |
| 2   | Cooling enable     | cooling_enable     | PASS  |
| 3   | OTC active         | otc_active         | PASS  |
| 4   | CH2 enable         | ch2_enable         | PASS  |
| 5   | Summer/winter mode | summerwintertime   | PASS  |
| 6   | DHW blocking       | dhw_blocking       | PASS  |
| 7   | (reserved)         | ignored            | PASS  |

### 5.2 ID 0 Slave Status LB — spec section 6.2

| Bit | Spec name                   | Firmware topic        | Match |
|-----|-----------------------------|-----------------------|-------|
| 0   | Fault indication            | fault                 | PASS  |
| 1   | CH mode                     | centralheating        | PASS  |
| 2   | DHW mode                    | domestichotwater      | PASS  |
| 3   | Flame status                | flame                 | PASS  |
| 4   | Cooling status              | cooling               | PASS  |
| 5   | CH2 mode                    | centralheating2       | PASS  |
| 6   | Diagnostic indication       | diagnostic_indicator  | PASS  |
| 7   | Electricity producer status | electric_production   | PASS  |

### 5.3 ID 70 Ventilation Status

Master HB bits: ventilation enable (0), bypass pos (1), bypass mode (2), free-vent mode (3).
Slave LB bits: fault (0), vent mode (1), bypass status (2), bypass auto (3), free-vent (4), diag (6).
Both correctly decoded in buildStatusVHMasterText() and buildStatusVHSlaveText(). PASS.

---

## 6. Identified Issues

### 6.1 FIXED in this PR: ID 38 comment error (OTGW-Core.h)

**Before:**
```cpp
{  38, OT_RW, ot_f88, "RelativeHumidity", "Relative Humidity", "%" },
// OTv4.2 spec: u8/u8 (HB=humidity%, LB=reserved); retained as f8.8 for backward compatibility
```

**Problem:** The OT v4.2 spec clearly states ID 38 type is "f8.8 combined" (in-repo spec
reference, line 216). The comment incorrectly attributed the f8.8 implementation to "backward
compatibility" when it is in fact the correct spec-conformant implementation. This misleading
comment could cause a future developer to "fix" the correct implementation.

**After:**
```cpp
{  38, OT_RW, ot_f88, "RelativeHumidity", "Relative Humidity", "%" },
// OTv4.2 spec section 5.3: f8.8 combined; some early Remeha docs described this as u8/u8
// but the authoritative v4.2 spec uses f8.8
```

### 6.2 Minor: ID 100 OTmap type slightly imprecise

OTmap entry: `{ 100, OT_READ, ot_flag8, "RoomRemoteOverrideFunction", ... }`

The spec says LB contains the override function flags; HB is reserved (0x00). The type
`ot_flag8` implies HB is the flag byte, but `print_remoteoverridefunction()` correctly reads
`OTdata.valueLB`. No functional bug — the type is only used by is_value_valid() to check
OT_READ_ACK, which is correct.

Recommendation: Low-priority cleanup, could rename to reflect LB usage.

### 6.3 Design trade-off: WRITE-ACK not captured for pure OT_WRITE IDs

For IDs like TSet (1), TrSet (16), Tr (24), the master sends WRITE-DATA and the slave
responds with WRITE-ACK (possibly with a clamped value). The firmware only captures
the master's WRITE-DATA for pure OT_WRITE entries.

This is intentional: for a gateway, the master's commanded value is the authoritative
setpoint. The slave's WRITE-ACK clamped value is relevant mainly for OT_RW entries
(TdhwSet=56, MaxTSet=57) where the firmware already captures it via the OT_WRITE_ACK check.

### 6.4 Feature gap: Brand string accumulation (IDs 93-95) not reconstructed

The spec describes IDs 93-95 as indexed character reads to build a string (master sends
READ-DATA with index N, slave responds with max-index and char[N]). The firmware correctly
decodes each READ-ACK response as u8u8 and publishes to MQTT, but does not accumulate
characters into a complete brand name string.

Impact: A subscriber can reconstruct the brand name from indexed responses. Future enhancement
could accumulate into a complete string topic.

### 6.5 Minor naming inconsistency: ID 100 label

| Location         | Name                       |
|------------------|----------------------------|
| OTmap label      | "RoomRemoteOverrideFunction" |
| Enum             | OT_RemoteOverrideFunction   |
| Spec section 6.16| "Remote override function"  |

The "Room" prefix in the OTmap label is from an older spec draft. No MQTT impact since
topics derive from the enum constant name. Low-priority cleanup.

---

## 7. is_value_valid() Analysis

```cpp
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
    if (OT.skipthis) return false;
    if (isMsgIdReservedInActiveProfile(OT.id)) return false;
    bool _valid = false;
    _valid = _valid || (OTlookup.msgcmd==OT_READ  && OT.type==OT_READ_ACK);
    _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OT.type==OT_WRITE_DATA);
    _valid = _valid || (OTlookup.msgcmd==OT_RW    && (OT.type==OT_READ_ACK
                                                    || OT.type==OT_WRITE_DATA
                                                    || OT.type==OT_WRITE_ACK));
    _valid = _valid || (OT.id==OT_Statusflags)
                    || (OT.id==OT_StatusVH)
                    || (OT.id==OT_SolarStorageMaster);
    return _valid;
}
```

The special-case for status IDs (last OR) is necessary and correct. For ID 0, the master
sends READ-DATA (type=0) with its own status flags in HB. Under the standard OT_READ rule
this master frame would be rejected (type is READ_DATA not READ_ACK). The special case
allows both frames to pass, which is the correct semantic: HB = master flags, LB = slave flags.

No functional issue found.

---

## 8. Delayed-Message / Override-Skip Logic

```cpp
bool skipthis = (delayedOTdata.id == OTdata.id)
             && (OTdata.time - delayedOTdata.time < 500)
             && (((OTdata.rsptype == OTGW_ANSWER_THERMOSTAT)
                   && (delayedOTdata.rsptype == OTGW_BOILER))
              || ((OTdata.rsptype == OTGW_REQUEST_BOILER)
                   && (delayedOTdata.rsptype == OTGW_THERMOSTAT)));
```

This correctly implements the OTGW gateway override logic:

- T->R case: OTGW intercepts Thermostat (T), sends modified Request-Boiler (R) for same ID
  within 500ms. Original T message is marked skipthis.
- B->A case: OTGW intercepts Boiler (B) response, sends modified Answer-Thermostat (A).
  Original B value is skipped in favour of A.

The 500ms window is appropriate (OT cycle is ~1 sec; OTGW response is typically <100ms).

The first message received after startup is always buffered (not processed) to ensure the
delay pair is always populated. This is deliberate and documented.

Assessment: Correct.

---

## 9. Mandatory ID Compliance (spec section 5.2.1)

| ID  | Name                  | Mandatory (who) | Firmware |
|-----|-----------------------|-----------------|---------|
| 0   | Status                | M + S           | PASS    |
| 1   | TSet                  | M               | PASS    |
| 3   | SlaveConfig           | S               | PASS    |
| 17  | RelModLevel           | S               | PASS    |
| 25  | Tboiler               | S               | PASS    |
| 93  | Brand                 | S (v4.x)        | PASS    |
| 94  | BrandVersion          | S (v4.x)        | PASS    |
| 95  | BrandSerialNumber     | S (v4.x)        | PASS    |
| 125 | OT version slave      | S               | PASS    |
| 127 | SlaveVersion          | S               | PASS    |

All mandatory IDs are handled.

---

## 10. Code Quality Observations

### Positive findings

1. RAII OTPublishGate: prevents MQTT throttle state from getting stuck on early return paths.
2. OTSpecCompatMode enum: clean separation of v4.x strict / pre-v4.2 / auto, with safe float comparison.
3. PROGMEM usage: string literals consistently in flash (PSTR(), F() macros) — critical for ESP8266 RAM.
4. Command queue with deduplication: prevents queue flooding from repeated commands.
5. processPSSummary(): handles PS=1 summary mode with type-safe parsers per field type.
6. Double guard: isMsgIdReservedInActiveProfile() checked in both is_value_valid() and decodeAndPublishOTValue().

### Improvement opportunities (non-breaking)

1. ID 38 comment — fixed in this PR.
2. is_value_valid for OT_WRITE: capturing WRITE-ACK would provide "boiler-accepted setpoint"
   alongside "commanded setpoint". Optional enhancement.
3. Brand string accumulation — future enhancement.
4. ID 100 OTmap label — low-priority rename, no MQTT impact.

---

## 11. Conclusion

The OTGW-firmware OpenTherm v4.2 implementation is **solid and spec-conformant**.
The 2026-02-15 compliance review addressed all critical issues including the hour-mask bug
in ID 20, R/W direction mismatches in 10 IDs, and 6 missing message IDs.

Code change made in this PR:
- Fix misleading comment on ID 38 in OTGW-Core.h: the f8.8 implementation is correct per
  v4.2 spec; the old comment incorrectly stated it was a legacy compatibility choice.

All other findings are either correctly implemented, deliberate design trade-offs for the
gateway role, or low-priority cosmetic improvements.

**Spec conformance level: HIGH — suitable for production use against OpenTherm v4.2 devices.**
