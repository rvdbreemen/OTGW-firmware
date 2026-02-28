# OpenTherm v4.2 – Complete Message-ID Reference (Comprehensive)

Source: `OpenTherm-Protocol-Specification-v4.2.pdf` (v4.2, 10 November 2020, 52 pages)

This document provides an exhaustive, machine-readable reference of every defined Message-ID in OpenTherm Protocol Specification v4.2. It consolidates the Data-ID Overview Map (§5.4, pages 47–49), the detailed Data Class descriptions (§5.3, pages 34–46), the mandatory ID table (§5.2.1, page 32), and all bit-level flag definitions from the PDF.

---

## Table of Contents

- [1. Data Type Reference](#1-data-type-reference)
- [2. Message Type Reference](#2-message-type-reference)
- [3. Frame Format](#3-frame-format)
- [4. Mandatory IDs for Central Heating Devices](#4-mandatory-ids-for-central-heating-devices)
- [5. Complete Message-ID Table](#5-complete-message-id-table)
- [6. Detailed Flag Bit Definitions](#6-detailed-flag-bit-definitions)
- [7. Remote Request Codes (ID 4)](#7-remote-request-codes-id-4)
- [8. Remote Override Operating Modes (ID 99)](#8-remote-override-operating-modes-id-99)
- [9. RF Sensor Status Information (ID 98)](#9-rf-sensor-status-information-id-98)
- [10. ID Range Summary](#10-id-range-summary)
- [11. Interpretation Notes](#11-interpretation-notes)

---

## 1. Data Type Reference

These data types are used throughout the OpenTherm protocol for the 16-bit DATA-VALUE field.

| Type | Full Name | Size | Range | Description |
|------|-----------|------|-------|-------------|
| `flag8` | 8-bit flag byte | 8 bits | n/a | Byte composed of 8 single-bit flags |
| `u8` | Unsigned 8-bit integer | 8 bits | 0–255 | Unsigned byte |
| `s8` | Signed 8-bit integer | 8 bits | −128–+127 | Two's complement signed byte |
| `f8.8` | Signed fixed-point | 16 bits | −128.00–+127.996 | 1 sign bit, 7 integer bits, 8 fractional bits. LSB represents 1/256. Two's complement. |
| `u16` | Unsigned 16-bit integer | 16 bits | 0–65535 | Unsigned 16-bit word |
| `s16` | Signed 16-bit integer | 16 bits | −32768–+32767 | Two's complement signed 16-bit word |
| `special` | Special format | varies | varies | Application-specific composite format; see per-ID notes |

### f8.8 Encoding Examples

| Temperature | HB (hex) | LB (hex) | 16-bit (hex) | 16-bit (decimal) | Calculation |
|-------------|----------|----------|--------------|-------------------|-------------|
| +21.50 °C | 0x15 | 0x80 | 0x1580 | 5504 | 5504 / 256 = 21.50 |
| −5.25 °C | 0xFA | 0xC0 | 0xFAC0 | −1344 | −1344 / 256 = −5.25 |
| +0.00 °C | 0x00 | 0x00 | 0x0000 | 0 | 0 / 256 = 0.00 |
| +100.00 °C | 0x64 | 0x00 | 0x6400 | 25600 | 25600 / 256 = 100.00 |

---

## 2. Message Type Reference

The 3-bit MSG-TYPE field in the frame determines the message direction and meaning.

### Master-to-Slave Messages

| Binary | Decimal | Message Type | Abbreviation | Description |
|--------|---------|--------------|--------------|-------------|
| `000` | 0 | READ-DATA | RD | Master requests a data value from slave |
| `001` | 1 | WRITE-DATA | WD | Master writes a data value to slave |
| `010` | 2 | INVALID-DATA | ID | Master sends data it knows is invalid |
| `011` | 3 | _(reserved)_ | — | Reserved for future use |

### Slave-to-Master Messages

| Binary | Decimal | Message Type | Abbreviation | Description |
|--------|---------|--------------|--------------|-------------|
| `100` | 4 | READ-ACK | RA | Slave acknowledges read; data is returned |
| `101` | 5 | WRITE-ACK | WA | Slave acknowledges write; data may be echoed or modified |
| `110` | 6 | DATA-INVALID | DI | Slave recognises ID but data is not available or invalid |
| `111` | 7 | UNKNOWN-DATAID | UI | Slave does not recognise the data identifier |

### Conversation Patterns

| Request (Master) | Possible Responses (Slave) |
|-----------------|--------------------------|
| READ-DATA | READ-ACK, DATA-INVALID, UNKNOWN-DATAID |
| WRITE-DATA | WRITE-ACK, DATA-INVALID, UNKNOWN-DATAID |
| INVALID-DATA | DATA-INVALID, UNKNOWN-DATAID |

---

## 3. Frame Format

```
Start bit (1)                                                    Stop bit (1)
 |   <<<<<<<<<<<<<<< 32-BIT FRAME >>>>>>>>>>>>>>>>>>               |
 |   <--- BYTE 1 --->  <--- BYTE 2 -->  <--- BYTE 3 -->  <--- BYTE 4 --->  |

 1  P  MSG-TYPE(3)  SPARE(4)  DATA-ID(8)  DATA-VALUE(16)  1
    ^   ^^^          ^^^^      ^^^^^^^^    ^^^^^^^^^^^^^^^^
    |   |            |         |           |
    |   |            |         |           +-- 16-bit data (HB + LB)
    |   |            |         +------------- 8-bit Data-ID (0–255)
    |   |            +----------------------- Always 0000 (reserved)
    |   +------------------------------------ 3-bit message type
    +---------------------------------------- Even parity over 32 bits
```

- **Total transmission**: 34 bits (1 start + 32 data + 1 stop)
- **Bit rate**: 1000 bits/sec (Manchester/Bi-phase-L encoding)
- **Frame duration**: ~34 ms
- **Parity**: Even parity over all 32 bits of the frame
- **Bit encoding**: Manchester (Bi-phase-L): '1' = active-to-idle transition, '0' = idle-to-active transition

### Timing Parameters

| Parameter | Min | Typ | Max | Unit |
|-----------|-----|-----|-----|------|
| Bit rate | — | 1000 | — | bits/sec |
| Period between mid-bit transitions | 900 | 1000 | 1150 | µs |
| Slave answering time | 20 | <100 | 400 | ms |
| Master wait time (MWT) | 100 | — | — | ms |
| Master communication interval (MCI) | MWT | <1000 | 1150 | ms |

---

## 4. Mandatory IDs for Central Heating Devices

Per §5.2.1 (page 32), the following IDs are mandatory when a device supports central heating.

| ID | Description | Master Requirement | Slave Requirement |
|---:|-------------|-------------------|-------------------|
| 0 | Master and Slave Status flags | Must send READ_DATA; must support all master status bits | Must respond READ_ACK; must support all slave status bits |
| 1 | Control Setpoint (CH water temp) | Must send WRITE_DATA or INVALID_DATA | Must respond WRITE_ACK |
| 2 | Master Configuration | Not mandatory (only if Smart Power needed) | Must respond WRITE_ACK; must support bit 0 (Smart Power) |
| 3 | Slave Configuration flags | Must send READ_DATA (at least at startup) | Must respond READ_ACK; must support all slave config flags |
| 14 | Max relative modulation level | Not mandatory; recommended for on-off control | Must respond WRITE_ACK |
| 17 | Relative Modulation Level | Not mandatory | Must respond READ_ACK or DATA_INVALID |
| 25 | Boiler temperature | Not mandatory | Must respond READ_ACK or DATA_INVALID |
| 93 | Brand Index | Not mandatory | Must respond READ_ACK or DATA_INVALID (if out of range) |
| 94 | Brand Version | Not mandatory | Must respond READ_ACK or DATA_INVALID (if out of range) |
| 95 | Brand Serial Number | Not mandatory | Must respond READ_ACK or DATA_INVALID (if out of range) |
| 125 | OpenTherm Version Slave | Not mandatory | Must respond READ_ACK |
| 127 | Slave product version | Not mandatory | Must respond READ_ACK |

> **Note**: For non-CH devices, consult the OpenTherm Function Matrix (separate document on the OTA website) for per-function mandatory IDs.

---

## 5. Complete Message-ID Table

Legend:
- **Msg**: `R` = Read-Data supported, `W` = Write-Data supported, `-` = not supported in that direction
- **Class**: Data class number per §5.3
- **HB**: High Byte of DATA-VALUE, **LB**: Low Byte of DATA-VALUE
- **Range**: Valid operational range per specification
- **Unit**: Engineering unit where applicable

### Class 1: Control and Status Information (IDs 0, 1, 5, 8, 70, 71, 72, 73, 101, 102, 115)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 0 | R/- | Status | flag8 | flag8 | Master status flags (see [§6.1](#61-id-0--master-status-hb)) | Slave status flags (see [§6.2](#62-id-0--slave-status-lb)) | n/a | — | **Yes** (M+S) |
| 1 | -/W | Tset (Control Setpoint) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–100 | °C | **Yes** (M+S) |
| 5 | R/- | ASF-flags / OEM-fault-code | flag8 | u8 | Application-specific fault flags (see [§6.3](#63-id-5--application-specific-fault-flags-hb)) | OEM fault code (0–255) | 0–255 | — | No |
| 8 | -/W | TsetCH2 (Control Setpoint CH2) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–100 | °C | No |
| 70 | R/- | Status ventilation/heat-recovery | flag8 | flag8 | Master ventilation status (see [§6.4](#64-id-70--ventilation-master-status-hb)) | Slave ventilation status (see [§6.5](#65-id-70--ventilation-slave-status-lb)) | n/a | — | No |
| 71 | -/W | Vset | — | u8 | Reserved (0x00) | Relative ventilation position | 0–100 | % | No |
| 72 | R/- | ASF-flags / OEM-fault-code (ventilation) | flag8 | u8 | Ventilation fault flags (see [§6.6](#66-id-72--ventilation-fault-flags-hb)) | OEM fault code ventilation (0–255) | 0–255 | — | No |
| 73 | R/- | OEM diagnostic code (ventilation) | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | — | No |
| 101 | R/- | Status Solar Storage | flag8 | flag8 | Master Solar Storage status (see [§6.7](#67-id-101--solar-storage-master-status-hb)) | Slave Solar Storage status (see [§6.8](#68-id-101--solar-storage-slave-status-lb)) | n/a | — | No |
| 102 | R/- | ASF-flags / OEM-fault-code (Solar Storage) | flag8 | u8 | Solar Storage fault flags (reserved) | OEM fault code Solar Storage (0–255) | 0–255 | — | No |
| 115 | R/- | OEM diagnostic code | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | — | No |

### Class 2: Configuration Information (IDs 2, 3, 74, 75, 76, 93, 94, 95, 103, 104, 124, 125, 126, 127)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 2 | -/W | M-Config / M-MemberIDcode | flag8 | u8 | Master config flags (see [§6.9](#69-id-2--master-configuration-hb)) | Master MemberID code | 0–255 | — | Partial (S must respond) |
| 3 | R/- | S-Config / S-MemberIDcode | flag8 | u8 | Slave config flags (see [§6.10](#610-id-3--slave-configuration-hb)) | Slave MemberID code | 0–255 | — | **Yes** (M+S) |
| 74 | R/- | S-Config / S-MemberIDcode (ventilation) | flag8 | u8 | Ventilation config flags (see [§6.11](#611-id-74--ventilation-configuration-hb)) | MemberID code ventilation | 0–255 | — | No |
| 75 | R/- | OpenTherm version (ventilation) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–127 | — | No |
| 76 | R/- | Ventilation/heat-recovery version | u8 | u8 | Product type | Product version | 0–255 | — | No |
| 93 | R/- | Brand | u8 | u8 | Brand index number (0–49) | ASCII character at that index | 0–49 / 0–255 | — | **Yes** (S) |
| 94 | R/- | Brand Version | u8 | u8 | Brand version index (0–49) | ASCII character at that index | 0–49 / 0–255 | — | **Yes** (S) |
| 95 | R/- | Brand Serial Number | u8 | u8 | Brand serial index (0–49) | ASCII character at that index | 0–49 / 0–255 | — | **Yes** (S) |
| 103 | R/- | S-Config / S-MemberIDcode (Solar Storage) | flag8 | u8 | Solar Storage config (see [§6.12](#612-id-103--solar-storage-configuration-hb)) | Solar Storage MemberID | 0–255 | — | No |
| 104 | R/- | Solar Storage version | u8 | u8 | Product type | Product version | 0–255 | — | No |
| 124 | -/W | OpenTherm version Master | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–127 | — | No |
| 125 | R/- | OpenTherm version Slave | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–127 | — | **Yes** (S) |
| 126 | -/W | Master-version | u8 | u8 | Product type | Product version | 0–255 | — | No |
| 127 | R/- | Slave-version | u8 | u8 | Product type | Product version | 0–255 | — | **Yes** (S) |

### Class 3: Remote Request (ID 4)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 4 | -/W | Remote Request | u8 | u8 | Request-Code (see [§7](#7-remote-request-codes-id-4)) | Request-Response-Code | 0–255 | — | No |

### Class 4: Sensor and Informational Data (IDs 16–39, 77–85, 96–98, 109–114, 116–123)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 16 | -/W | TrSet (Room Setpoint) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 17 | R/- | Rel.-mod-level (Relative Modulation Level) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–100 | % | **Yes** (S) |
| 18 | R/- | CH-pressure (CH water pressure) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–5 | bar | No |
| 19 | R/- | DHW-flow-rate | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–16 | l/min | No |
| 20 | R/W | Day-Time (Day of Week & Time) | special | u8 | HB bits 7,6,5 = day of week (1=Mon…7=Sun, 0=no DoW); bits 4,3,2,1,0 = hours (0–23) | Minutes (0–59) | see desc | — | No |
| 21 | R/W | Date (Calendar date) | u8 | u8 | Month (1–12; 1=January) | Day of month (1–31) | 1–12 / 1–31 | — | No |
| 22 | R/W | Year (Calendar year) | — | — | _u16 combined_ | _u16 combined_ | 0–65535 (practical: 1999–2099) | — | No |
| 23 | -/W | TrSetCH2 (Room Setpoint CH2) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 24 | -/W | Tr (Room temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 25 | R/- | Tboiler (Boiler flow water temp) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | **Yes** (S) |
| 26 | R/- | Tdhw (DHW temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 27 | R/W | Toutside (Outside temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 28 | R/- | Tret (Return water temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 29 | R/- | Tstorage (Solar storage temp) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 30 | R/- | Tcollector (Solar collector temp) | — | — | _s16 combined_ | _s16 combined_ | −40–250 | °C | No |
| 31 | R/- | TflowCH2 (Flow temp CH2) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 32 | R/- | Tdhw2 (DHW temperature 2) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 33 | R/- | Texhaust (Exhaust temperature) | — | — | _s16 combined_ | _s16 combined_ | −40–500 | °C | No |
| 34 | R/- | Tboiler-heat-exchanger | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 35 | R/- | Boiler fan speed (Setpoint/actual) | u8 | u8 | Fan speed Setpoint in Hz (RPM/60) | Actual fan speed in Hz (RPM/60) | 0–255 | Hz | No |
| 36 | R/- | Flame current | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–127 | µA | No |
| 37 | -/W | TrCH2 (Room temp for CH2) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 38 | R/W | Relative Humidity | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–100 | % | No |
| 39 | R/- | TrOverride 2 (Remote Override Room Setpoint 2) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–30 | °C | No |
| 77 | R/- | Rel-vent-level (Relative ventilation) | — | u8 | Reserved (0x00) | Relative ventilation level | 0–100 | % | No |
| 78 | R/W | RH-exhaust (Relative humidity exhaust) | — | u8 | Reserved (0x00) | Relative humidity exhaust air | 0–100 | % | No |
| 79 | R/W | CO2-exhaust (CO2 level) | — | — | _u16 combined_ | _u16 combined_ | 0–2000 (operational) | ppm | No |
| 80 | R/- | Tsi (Supply inlet temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 81 | R/- | Tso (Supply outlet temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 82 | R/- | Tei (Exhaust inlet temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 83 | R/- | Teo (Exhaust outlet temperature) | — | — | _f8.8 combined_ | _f8.8 combined_ | −40–127 | °C | No |
| 84 | R/- | RPM-exhaust (Exhaust fan speed) | — | — | _u16 combined_ | _u16 combined_ | 0–6000 | rpm | No |
| 85 | R/- | RPM-supply (Supply/inlet fan speed) | — | — | _u16 combined_ | _u16 combined_ | 0–6000 | rpm | No |
| 96 | R/W | Cooling Operation Hours | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | hours | No |
| 97 | R/W | Power Cycles | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | cycles | No |
| 98 | -/W | RF sensor status information | special | special | Sensor type (see [§9](#9-rf-sensor-status-information-id-98)) | RF strength & battery (see [§9](#9-rf-sensor-status-information-id-98)) | — | — | No |
| 109 | R/W | Electricity producer starts | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | starts | No |
| 110 | R/W | Electricity producer hours | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | hours | No |
| 111 | R/- | Electricity production | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | Watt | No |
| 112 | R/W | Cumulative Electricity production | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | kWh | No |
| 113 | R/W | Unsuccessful burner starts | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | starts | No |
| 114 | R/W | Flame signal too low count | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | count | No |
| 116 | R/W | Successful Burner starts | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | starts | No |
| 117 | R/W | CH pump starts | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | starts | No |
| 118 | R/W | DHW pump/valve starts | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | starts | No |
| 119 | R/W | DHW burner starts | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | starts | No |
| 120 | R/W | Burner operation hours | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | hours | No |
| 121 | R/W | CH pump operation hours | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | hours | No |
| 122 | R/W | DHW pump/valve operation hours | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | hours | No |
| 123 | R/W | DHW burner operation hours | — | — | _u16 combined_ | _u16 combined_ | 0–65535 | hours | No |

### Class 5: Pre-Defined Remote Boiler Parameters (IDs 6, 48, 49, 56, 57, 86, 87)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 6 | R/- | RBP-flags (Remote Boiler Parameter flags) | flag8 | flag8 | Transfer-enable flags (see [§6.13](#613-id-6--remote-boiler-parameter-transfer-enable-flags-hb)) | Read/write flags (see [§6.14](#614-id-6--remote-parameter-readwrite-flags-lb)) | n/a | — | No |
| 48 | R/- | TdhwSet-UB / TdhwSet-LB | s8 | s8 | DHW Setpoint upper bound | DHW Setpoint lower bound | 0–127 | °C | No |
| 49 | R/- | MaxTSet-UB / MaxTSet-LB | s8 | s8 | Max CH Setpoint upper bound | Max CH Setpoint lower bound | 0–127 | °C | No |
| 56 | R/W | TdhwSet (DHW Setpoint, Remote parameter 1) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–127 | °C | No |
| 57 | R/W | MaxTSet (Max CH water Setpoint, Remote parameter 2) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–127 | °C | No |
| 86 | R/- | RBP-flags (ventilation/heat-recovery) | flag8 | flag8 | Ventilation transfer-enable flags (see [§6.15](#615-id-86--ventilation-remote-parameter-flags)) | Ventilation read/write flags | n/a | — | No |
| 87 | R/W | Nominal ventilation value | u8 | — | Nominal ventilation (0–100%) | Reserved (0x00) | 0–100 | % | No |

### Class 6: Transparent Slave Parameters (IDs 10, 11, 88, 89, 105, 106)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 10 | R/- | TSP count | u8 | u8 | Number of TSPs supported | Reserved (0x00) | 0–255 | — | No |
| 11 | R/W | TSP-index / TSP-value | u8 | u8 | TSP index number | TSP value | 0–255 | — | No |
| 88 | R/- | TSP count (ventilation) | u8 | u8 | Number of ventilation TSPs | Reserved (0x00) | 0–255 | — | No |
| 89 | R/W | TSP-index / TSP-value (ventilation) | u8 | u8 | Ventilation TSP index | Ventilation TSP value | 0–255 | — | No |
| 105 | R/- | TSP count (Solar Storage) | u8 | u8 | Number of Solar Storage TSPs | Reserved (0x00) | 0–255 | — | No |
| 106 | R/W | TSP-index / TSP-value (Solar Storage) | u8 | u8 | Solar Storage TSP index | Solar Storage TSP value | 0–255 | — | No |

### Class 7: Fault History Data (IDs 12, 13, 90, 91, 107, 108)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 12 | R/- | FHB-size | u8 | u8 | Size of Fault History Buffer | Reserved (0x00) | 0–255 | — | No |
| 13 | R/- | FHB-index / FHB-value | u8 | u8 | FHB entry index number | FHB entry value | 0–255 | — | No |
| 90 | R/- | FHB-size (ventilation) | u8 | u8 | Ventilation FHB size | Reserved (0x00) | 0–255 | — | No |
| 91 | R/- | FHB-index / FHB-value (ventilation) | u8 | u8 | Ventilation FHB index | Ventilation FHB value | 0–255 | — | No |
| 107 | R/- | FHB-size (Solar Storage) | u8 | u8 | Solar Storage FHB size | Reserved (0x00) | 0–255 | — | No |
| 108 | R/- | FHB-index / FHB-value (Solar Storage) | u8 | u8 | Solar Storage FHB index | Solar Storage FHB value | 0–255 | — | No |

### Class 8: Control of Special Applications (IDs 7, 9, 14, 15, 39, 99, 100)

| ID | Msg | Data Object | HB Type | LB Type | HB Description | LB Description | Range | Unit | Mandatory |
|---:|:---:|-------------|---------|---------|----------------|----------------|-------|------|-----------|
| 7 | -/W | Cooling-control | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–100 | % | No |
| 9 | R/- | TrOverride (Remote Override Room Setpoint) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–30 (0=no override) | °C | No |
| 14 | -/W | Max-rel-mod-level-setting | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–100 | % | Partial (S must respond) |
| 15 | R/- | Max-Capacity / Min-Mod-Level | u8 | u8 | Maximum boiler capacity | Minimum modulation level | 0–255 / 0–100 | kW / % | No |
| 39 | R/- | TrOverride 2 (Remote Override Room Setpoint 2) | — | — | _f8.8 combined_ | _f8.8 combined_ | 0–30 (0=no override) | °C | No |
| 99 | R/W | Remote Override Operating Mode (Heating/DHW) | special | special | DHW operating mode (see [§8](#8-remote-override-operating-modes-id-99)) | Heating operating mode (see [§8](#8-remote-override-operating-modes-id-99)) | 0–255 | — | No |
| 100 | R/- | Remote override function | flag8 | — | Reserved (0x00) | Override function flags (see [§6.16](#616-id-100--remote-override-function-lb)) | 0–255 | — | No |

---

## 6. Detailed Flag Bit Definitions

### 6.1 ID 0 – Master Status (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | CH enable | CH is disabled | CH is enabled |
| 1 | DHW enable | DHW is disabled | DHW is enabled |
| 2 | Cooling enable | Cooling is disabled | Cooling is enabled |
| 3 | OTC active | OTC not active | OTC is active |
| 4 | CH2 enable | CH2 is disabled | CH2 is enabled |
| 5 | Summer/winter mode | Winter mode active | Summer mode active |
| 6 | DHW blocking | DHW unblocked | DHW blocked |
| 7 | Reserved | — | — |

### 6.2 ID 0 – Slave Status (LB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Fault indication | No fault | Fault |
| 1 | CH mode | CH not active | CH active |
| 2 | DHW mode | DHW not active | DHW active |
| 3 | Flame status | Flame off | Flame on |
| 4 | Cooling status | Cooling mode not active | Cooling mode active |
| 5 | CH2 mode | CH2 not active | CH2 active |
| 6 | Diagnostic/service indication | No diagnostic/service | Diagnostic/service event |
| 7 | Electricity production | Off | On |

### 6.3 ID 5 – Application-Specific Fault Flags (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Service request | Service not required | Service required |
| 1 | Lockout-reset | Remote reset disabled | Remote reset enabled |
| 2 | Low water pressure | No water pressure fault | Water pressure fault |
| 3 | Gas/flame fault | No gas/flame fault | Gas/flame fault |
| 4 | Air pressure fault | No air pressure fault | Air pressure fault |
| 5 | Water over-temperature | No over-temperature fault | Over-temperature fault |
| 6 | Reserved | — | — |
| 7 | Reserved | — | — |

### 6.4 ID 70 – Ventilation Master Status (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Ventilation enable | Disabled | Enabled |
| 1 | Bypass position (manual mode only) | Close bypass | Open bypass |
| 2 | Bypass mode | Manual | Automatic |
| 3 | Free ventilation mode | Not active | Active |
| 4–7 | Reserved | — | — |

### 6.5 ID 70 – Ventilation Slave Status (LB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Fault indication | No fault | Fault |
| 1 | Ventilation mode | Not active | Active |
| 2 | Bypass status | Closed | Open |
| 3 | Bypass automatic status | Manual | Automatic |
| 4 | Free ventilation status | Not active | Active |
| 5 | Reserved | — | — |
| 6 | Diagnostic indication | No diagnostics | Diagnostic event |
| 7 | Reserved | — | — |

### 6.6 ID 72 – Ventilation Fault Flags (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Service request | Service not required | Service required |
| 1 | Exhaust fan fault | No fault | Fault |
| 2 | Inlet fan fault | No fault | Fault |
| 3 | Frost protection | Not active | Active |
| 4–7 | Reserved | — | — |

### 6.7 ID 101 – Solar Storage Master Status (HB)

Bits 2,1,0 define the Solar mode command from master:

| Bits 2:0 | Solar Mode |
|:--------:|------------|
| `000` | Off (solar completely switched off) |
| `001` | DHW eco (solar heating enabled) |
| `010` | DHW comfort (boiler keeps small part of storage tank loaded) |
| `011` | DHW single boost (boiler does single loading of storage tank) |
| `100` | DHW continuous boost (boiler keeps whole tank loaded) |
| `101`–`111` | Reserved |

### 6.8 ID 101 – Solar Storage Slave Status (LB)

| Bits | Name | Values |
|:----:|------|--------|
| 0 | Fault indication | 0 = no fault, 1 = fault |
| 3,2,1 | Solar mode (echo) | Same encoding as HB bits 2:0 |
| 5,4 | Solar status | `00` = standby, `01` = loading by sun, `10` = loading by boiler, `11` = anti-legionella mode active |
| 7,6 | Reserved | — |

### 6.9 ID 2 – Master Configuration (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Smart Power | Not implemented | Implemented |
| 1–7 | Reserved | — | — |

> **Note** (§5.3.2): Since protocol version 3.0 it is mandatory for a slave to support Smart Power. A gateway must support Smart Power on the slave interface.

### 6.10 ID 3 – Slave Configuration (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | DHW present | DHW not present | DHW is present |
| 1 | Control type | Modulating | On/off |
| 2 | Cooling config | Cooling not supported | Cooling supported |
| 3 | DHW config | Instantaneous or not-specified | Storage tank |
| 4 | Master low-off&pump control | Allowed | Not allowed |
| 5 | CH2 present | CH2 not present | CH2 present |
| 6 | Remote water filling function | Available or unknown (unknown for protocol ≤2.2) | Not available |
| 7 | Heat/cool mode control | Switching done by master | Switching done by slave |

### 6.11 ID 74 – Ventilation Configuration (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | System type | Central exhaust ventilation | Heat-recovery ventilation |
| 1 | Bypass | Not present | Present |
| 2 | Speed control | 3-speed | Variable |
| 3–7 | Reserved | — | — |

### 6.12 ID 103 – Solar Storage Configuration (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | System type | DHW preheat system | DHW parallel system |
| 1–7 | Reserved | — | — |

### 6.13 ID 6 – Remote Boiler Parameter Transfer-Enable Flags (HB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | DHW Setpoint | Transfer disabled | Transfer enabled |
| 1 | Max CH Setpoint | Transfer disabled | Transfer enabled |
| 2–7 | Reserved | — | — |

### 6.14 ID 6 – Remote Parameter Read/Write Flags (LB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | DHW Setpoint | Read-only | Read/write |
| 1 | Max CH Setpoint | Read-only | Read/write |
| 2–7 | Reserved | — | — |

### 6.15 ID 86 – Ventilation Remote Parameter Flags

**HB – Transfer-Enable Flags:**

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Nominal ventilation value | Transfer disabled | Transfer enabled |
| 1–7 | Reserved | — | — |

**LB – Read/Write Flags:**

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Nominal ventilation value | Read-only | Read/write |
| 1–7 | Reserved | — | — |

### 6.16 ID 100 – Remote Override Function (LB)

| Bit | Name | Clear (0) | Set (1) |
|:---:|------|-----------|---------|
| 0 | Manual change priority | Disable overruling remote Setpoint by manual Setpoint change | Enable overruling remote Setpoint by manual Setpoint change |
| 1 | Program change priority | Disable overruling remote Setpoint by program Setpoint change | Enable overruling remote Setpoint by program Setpoint change |
| 2–7 | Reserved | — | — |

---

## 7. Remote Request Codes (ID 4)

ID 4 supports remote commands from master to slave. The Request-Code is in the HB; the Response-Code is in the LB.

### Request Codes (HB)

| Code | Name | Description |
|:----:|------|-------------|
| 0 | Normal operation | Back to normal operation mode |
| 1 | BLOR | Boiler Lock-out Reset request |
| 2 | CHWF | CH water filling request |
| 3 | Service max power | Service mode maximum power request (e.g., CO₂ measurement during Chimney Sweep Function) |
| 4 | Service min power | Service mode minimum power request (CO₂ measurement) |
| 5 | Service spark test | Service mode spark test request (no gas) |
| 6 | Service fan max | Service mode fan maximum speed request (no flame) |
| 7 | Service fan min | Service mode fan minimum speed request (no flame) |
| 8 | Service 3-way CH | Service mode 3-way valve to CH request (no pump, no flame) |
| 9 | Service 3-way DHW | Service mode 3-way valve to DHW request (no pump, no flame) |
| 10 | Reset service flag | Request to reset service request flag |
| 11 | Service test 1 | OEM-specific service test |
| 12 | Auto air purge | Automatic hydronic air purge |
| 13–255 | Reserved | Reserved for future use |

### Response Codes (LB)

| Range | Meaning |
|:-----:|---------|
| 0–127 | Request refused |
| 128–255 | Request accepted |

### Conversation Example

```
Master:  WRITE-DATA (id=4, Cmd=BLOR, 0x00)
Slave:   WRITE-ACK  (id=4, Cmd=BLOR, Req-Response)   → Request accepted
         DATA-INVALID  (id=4, BLOR, 0x00)             → Request not recognised
         UNKNOWN-DATAID (id=4, BLOR, 0x00)            → Remote Request not supported
```

---

## 8. Remote Override Operating Modes (ID 99)

### LB – Heating Operating Mode

| Bits | Field | Values |
|:----:|-------|--------|
| 3:0 | Operating Mode HC1 | 0 = No override, 1 = Auto (time switch program), 2 = Comfort, 3 = Precomfort, 4 = Reduced, 5 = Protection (e.g. frost), 6 = Off, 7–15 = reserved |
| 7:4 | Operating Mode HC2 | 0 = No override, 1 = Auto (time switch program), 2 = Comfort, 3 = Precomfort, 4 = Reduced, 5 = Protection (e.g. frost), 6 = Off, 7–15 = reserved |

### HB – DHW Operating Mode

| Bits | Field | Values |
|:----:|-------|--------|
| 3:0 | Operating Mode DHW | 0 = No override, 1 = Auto (time switch program), 2 = Anti-Legionella, 3 = Comfort, 4 = Reduced, 5 = Protection (e.g. frost), 6 = Off, 7–15 = reserved |
| 4 | Manual DHW push | 0 = no push, 1 = push (raise DHW to Comfort level once, then return to previous mode; for storage tanks) |
| 7:5 | Reserved | Set to 0 |

> **Note**: Operating Modes are chosen according to prEN 15'500 with extensions. With 'No Override' for Heating and DHW you can change Heating or DHW independently.

---

## 9. RF Sensor Status Information (ID 98)

### HB – Sensor Type

| Bits | Field | Values |
|:----:|-------|--------|
| 3:0 | Sensor index | Index of the specific RF sensor (0–15) |
| 7:4 | Sensor type | `0000` = Room temperature controllers, `0001` = Room temperature sensors, `0010` = Outside temperature sensors, `1111` = Not defined type, Others = Reserved (future: radiator valves, humidity, CO₂, wind velocity) |

### LB – RF and Battery Indication

| Bits | Field | Values |
|:----:|-------|--------|
| 1:0 | Battery indication | `00` = No battery indication, `01` = Low battery (possible loss of functionality), `10` = Nearly low battery (advice to replace), `11` = No low battery |
| 4:2 | Signal strength | `000` = No signal strength indication, `001` = Strength 1 (Weak/lost signal), `010` = Strength 2, `011` = Strength 3, `100` = Strength 4, `101` = Strength 5 (Perfect) |
| 7:5 | Reserved | — |

---

## 10. ID Range Summary

| ID Range | Category | Description |
|:--------:|----------|-------------|
| 0–39 | Core control | Status, setpoints, sensors, and basic control |
| 40–47 | _Undefined_ | Reserved for future use |
| 48–49 | Parameter bounds | DHW Setpoint and max CH Setpoint upper/lower bounds |
| 50–55 | _Undefined_ | Reserved for future use (IDs 50, 58 removed in v2.3D – were OTC heat curve) |
| 56–57 | Remote boiler params | DHW Setpoint and max CH water Setpoint |
| 58–69 | _Undefined_ | Reserved for future use |
| 70–91 | Ventilation / heat-recovery | Complete ventilation and heat-recovery extension |
| 92 | _Undefined_ | Reserved |
| 93–95 | Brand identification | Brand name, version, serial number (mandatory for slave since v4.1) |
| 96–100 | Special data | Cooling hours, power cycles, RF sensor, remote override |
| 101–108 | Solar storage | Complete Solar Storage extension |
| 109–112 | Electricity production | Electricity producer counters and stats |
| 113–114 | Burner diagnostics | Unsuccessful starts, flame signal low count |
| 115 | OEM diagnostic | OEM-specific diagnostic/service code |
| 116–123 | Counters / hours | Burner starts, pump starts, operation hours |
| 124–127 | Protocol/product version | OpenTherm version and product version (master + slave) |
| 128–255 | _Test & Diagnostics_ | OEM/member-specific area; requires MemberID handshake |

---

## 11. Interpretation Notes

### General Rules
- All Data-item IDs are **decimal** unless noted otherwise.
- `0x00` (two zero bytes) is the default/dummy data-value when no real data is sent (e.g., in a Read-Data request).
- Unused/reserved bits should be set to **0** by the transmitter; the receiver should **ignore** them for forward compatibility.
- `R` and `W` indicate whether READ-DATA and WRITE-DATA are supported, respectively.

### Specific ID Notes

| ID | Note |
|---:|------|
| 0 | **Status exchange** is a special conversation: master sends `READ-DATA(id=0, MasterStatus, 0x00)`, slave MUST respond `READ-ACK(id=0, MasterStatus, SlaveStatus)`. Slave cannot respond with DATA-INVALID or UNKNOWN-DATAID. `WRITE-DATA(id=0,…)` should NOT be used. CHenable bit (0:HB:0) has priority over Control Setpoint (ID 1). |
| 1 | Default range 0–100 °C. The master decides the actual operational range. |
| 2 | Since protocol v3.0, Smart Power support (bit 0) is mandatory on slave interfaces. Gateways must support Smart Power on the slave interface. MemberID code 0 = customer non-specific device. |
| 9 | Value 0 = no override; 1–30 = valid remote override room Setpoint in °C. |
| 20 | HB bits 7:5 = day of week (1=Monday…7=Sunday, 0=no day info); HB bits 4:0 = hours (0–23); LB = minutes (0–59). |
| 30 | Solar collector temperature uses **s16** (not f8.8) because it can reach 250 °C. |
| 33 | Exhaust temperature uses **s16** (not f8.8) because it can reach 500 °C. |
| 35 | Fan speed is expressed in Hz (= RPM/60). HB = Setpoint, LB = actual. |
| 71 | Only LB is used (0–100%); HB should be 0x00. |
| 77 | Only LB is used (0–100%); HB should be 0x00. |
| 78 | Only LB is used (0–100%); HB should be 0x00. |
| 79 | `u16` technically allows 0–65535, but operational range is 0–2000 ppm per v4.2. |
| 87 | Only HB is used (0–100%); LB should be 0x00. Nominal value for mid-position in 3-speed ventilation. |
| 93–95 | Reading example: `READ-DATA(id=93, index, 0x00)` → `READ-ACK(id=93, max-index, character)`. Strings can be up to 50 characters (index 0–49). |
| 96–97 | Reset by writing zero is optional for the slave. |
| 99 | Operating Modes follow prEN 15'500. "No Override" (value 0) allows independent Heating/DHW control. "Manual DHW push" raises DHW to Comfort once then returns. |
| 109–112 | All electricity counters: reset by writing zero is optional for the slave. Counter values may overroll (u16 limitation). |
| 116–123 | All operational counters: reset by writing zero is optional for the slave. |
| 124 | Written by master to inform slave of its protocol version. |
| 125 | Read from slave to discover its protocol version. |

### Counter Overflow Warning
All u16 counters (IDs 96, 97, 109–114, 116–123) will overflow at 65535. Implementations should handle rollover appropriately. For operation hours, 65535 hours ≈ 7.5 years of continuous operation.

---

## Physical Layer Quick Reference

### Signal Levels

| Parameter | Symbol | Min | Typ | Max | Unit |
|-----------|--------|-----|-----|-----|------|
| Current signal High (slave→master) | I_high | 17 | — | 23 | mA |
| Current signal Low (slave→master) | I_low | 5 | — | 9 | mA |
| Current receive threshold (master) | I_rcv | 11.5 | 13 | 14.5 | mA |
| Voltage signal High (master→slave) | V_high | 15 | — | 18 | V |
| Voltage signal Low (master→slave) | V_low | — | — | 8 | V |
| Voltage receive threshold (slave) | V_rcv | 9.5 | 11 | 12.5 | V |
| Max open-circuit voltage | — | — | — | 42 | Vdc |

### Signal Timing

| Parameter | Symbol | Min | Typ | Max | Unit |
|-----------|--------|-----|-----|-----|------|
| Current signal rise time | t_r | — | 20 | 50 | µs |
| Current signal fall time | t_f | — | 20 | 50 | µs |
| Voltage signal rise time | t_r | — | 20 | 50 | µs |
| Voltage signal fall time | t_f | — | 20 | 50 | µs |

### Power Modes

| Mode | Idle Current | Idle Voltage | Available Power | Description |
|------|-------------|-------------|-----------------|-------------|
| Low Power | Low (I_low) | Low (V_low) | 40 mW (5 mA @ 8 V) | Mandatory startup mode; basic operation guaranteed |
| Medium Power | High (I_high) | Low (V_low) | 136 mW (17 mA @ 8 V) | Extended power; requires Smart Power handshake |
| High Power | High (I_high) | High (V_high) | 306 mW (17 mA @ 18 V) | Maximum power; requires Smart Power handshake |

### Transmission Medium

| Parameter | Specification |
|-----------|--------------|
| Number of wires | 2 |
| Wiring type | Untwisted pair (twisted pair recommended in noisy environments) |
| Maximum line length | 50 metres |
| Maximum cable resistance | 2 × 5 Ω |
| Connection polarity | Polarity-free (interchangeable) |
| Galvanic isolation | Required per EN60730-1 |

---

## OT/Lite Protocol Equivalence

The OT/Lite protocol is a reduced subset of OpenTherm for low-cost applications:

### OT/Lite Mandatory Data-IDs

| OT/Lite ID | OT Full ID | Data Object | Description |
|:----------:|:----------:|-------------|-------------|
| 0 | 0 | Status | Master and Slave Status flags |
| 1 | 1 | Tset | Control Setpoint (°C) |
| 9 | 9 | TrOverride | Remote room Setpoint override |
| 14 | 14 | Max-rel-mod | Maximum relative modulation level |
| 16 | 16 | TrSet | Room Setpoint (°C) |
| 24 | 24 | Tr | Room temperature (°C) |
| 25 | 25 | Tboiler | Boiler flow water temperature |
| 56 | 56 | TdhwSet | DHW Setpoint |
| 57 | 57 | MaxTSet | Max CH water Setpoint |

### OT/Lite Optional Data-IDs

| OT/Lite ID | OT Full ID | Data Object | Description |
|:----------:|:----------:|-------------|-------------|
| 3 | 3 | S-Config | Slave Configuration flags |
| 5 | 5 | ASF-flags | Application fault flags + OEM code |
| 6 | 6 | RBP-flags | Remote boiler parameter flags |
| 15 | 15 | Max-Cap/Min-Mod | Max capacity / Min modulation |
| 17 | 17 | Rel-mod-level | Relative Modulation Level |
| 18 | 18 | CH-pressure | CH water pressure |
| 19 | 19 | DHW-flow-rate | DHW flow rate |
| 26 | 26 | Tdhw | DHW temperature |
| 27 | 27 | Toutside | Outside temperature |
| 28 | 28 | Tret | Return water temperature |
| 48 | 48 | TdhwSet bounds | DHW Setpoint bounds |
| 49 | 49 | MaxTSet bounds | Max CH Setpoint bounds |
| 100 | 100 | Override function | Remote override function |

---

## Change History Summary

| Version | Date | Key Changes |
|:-------:|------|-------------|
| 1.0 | 10 Jun 1997 | Initial release |
| 2.0 | 18 Jan 2000 | Slave configuration reorganisation (ID 3); DHW blocking; OTC active; maintenance indication |
| 2.1 | 12 Dec 2002 | Smart Power protocol extension |
| 2.2 | 11 Aug 2003 | Ventilation / heat-recovery extension (IDs 70–91); Transparent Slave Parameters |
| 2.3 | 21 Jan 2005 | Extensions (details pending) |
| 2.3b | 01 Aug 2006 | Merge 2.3 to single specification |
| 2.3C | 02 Sep 2008 | Refinements |
| 2.3D | 15 Dec 2010 | Removed IDs 50, 58 (OTC heat curve); new Class 8 IDs |
| 3.0 | 12 Apr 2011 | Major protocol revision |
| 4.0 | 22 Jun 2015 | Spec format update; ID additions (34–39, 96–98, 109–114, 116–119) |
| 4.1 | 22 Sep 2016 | Solar Storage extension (IDs 101–108); brand IDs (93–95) mandatory |
| 4.2 | 10 Nov 2020 | ID 99 Remote Override Operating Mode; ID 39 TrOverride2; ID 0 summer/winter + DHW blocking; editorial cleanup |

---

_Document generated from OpenTherm Protocol Specification v4.2 (©1996-2020 The OpenTherm Association). All IDs not listed above (40–47, 50–55, 58–69, 92, 128–255) are reserved._
