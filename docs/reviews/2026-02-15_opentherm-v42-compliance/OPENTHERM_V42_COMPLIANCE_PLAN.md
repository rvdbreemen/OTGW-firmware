---
# METADATA
Document Title: OpenTherm v4.2 Protocol Compliance Analysis & Improvement Plan
Review Date: 2026-02-15 14:33:00 UTC
Implementation Date: 2026-02-15 15:47:00 UTC
Branch Reviewed: main (current codebase)
Target Version: v1.1.0-beta
Reviewer: GitHub Copilot Advanced Agent
Document Type: Compliance Analysis & Task Breakdown
Reference Spec: Specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
Status: IMPLEMENTED
---

# OpenTherm v4.2 Protocol Compliance Analysis & Improvement Plan

## 1. Executive Summary

Dit document bevat een volledige vergelijking van de OTGW-firmware implementatie met de OpenTherm Protocol Specificatie v4.2 (10 november 2020, 52 pagina's). De analyse richt zich op correcte afhandeling van alle message IDs, datatypes, richtingsindicatoren (R/W) en bitdefinities.

**Conclusie**: De firmware ondersteunt het overgrote deel van de v4.2 specificatie correct. Er zijn echter **6 ontbrekende message IDs**, **10 richting (R/W) afwijkingen**, **2 datatype inconsistenties**, **1 kritieke bug** (uurmasker ID 20), en diverse kleinere verbeterpunten gevonden.

### Samenvatting Bevindingen

| Categorie | Aantal | Ernst |
|-----------|--------|-------|
| Ontbrekende Message IDs | 6 | Medium |
| Richting (R/W) afwijkingen | 10 | Medium-Hoog |
| Datatype inconsistenties | 2 | Medium |
| Bugs (uurmasker ID 20) | 1 | **Kritiek** |
| Eenheden/label fouten | 4 | Laag |
| Code quality issues | 3 | Laag |

---

## 2. Gedetailleerde Analyse

### 2.1 Ontbrekende Message IDs

De volgende IDs zijn gedefinieerd in de v4.2 specificatie maar staan als `OT_UNDEF` in de firmware (`OTGW-Core.h` OTmap array):

#### ID 39 — TrOverride2 (Remote Override Room Setpoint 2)

| Eigenschap | Spec v4.2 | Firmware |
|------------|-----------|----------|
| **Data Object** | TrOverride 2 | ❌ Niet geïmplementeerd |
| **Type** | f8.8 | OT_UNDEF |
| **Richting** | R/- | — |
| **Bereik** | 0–30 °C (0=geen override) | — |
| **Klasse** | 8 (Special Applications) | — |
| **Verplicht** | Nee | — |

**Impact**: Systemen met twee verwarmingskringen (CH2) die remote override room setpoint 2 gebruiken, worden niet correct weergegeven. De waarde wordt niet opgeslagen, niet gepubliceerd naar MQTT en niet weergegeven in de UI.

**Locaties in firmware**:
- `OTGW-Core.h:369` — `{ 39, OT_UNDEF, ot_undef, "", "", "" }`
- Geen veld in `OTdataStruct` voor TrOverride2
- Geen case in `processOT` switch
- Geen case in `getOTGWValue` switch

---

#### ID 93 — Brand (Merknaam boiler)

| Eigenschap | Spec v4.2 | Firmware |
|------------|-----------|----------|
| **Data Object** | Brand | ❌ Niet geïmplementeerd |
| **Type** | u8 / u8 (index / ASCII char) | OT_UNDEF |
| **Richting** | R/- | — |
| **Bereik** | HB: 0–49 (index), LB: 0–255 (ASCII) | — |
| **Klasse** | 2 (Configuration) | — |
| **Verplicht** | **Ja** (Slave moet READ_ACK of DATA_INVALID sturen) | — |

**Impact**: Boiler merknaam kan niet worden uitgelezen. Dit is een **verplicht** ID sinds v4.1. Strings tot 50 karakters (index 0–49). Leespatroon: `READ-DATA(id=93, index, 0x00)` → `READ-ACK(id=93, max-index, character)`.

---

#### ID 94 — Brand Version (Merkversie)

| Eigenschap | Spec v4.2 | Firmware |
|------------|-----------|----------|
| **Data Object** | Brand Version | ❌ Niet geïmplementeerd |
| **Type** | u8 / u8 (index / ASCII char) | OT_UNDEF |
| **Richting** | R/- | — |
| **Verplicht** | **Ja** (Slave) | — |

**Impact**: Boiler firmwareversie (merk-specifiek) kan niet worden uitgelezen.

---

#### ID 95 — Brand Serial Number (Serienummer)

| Eigenschap | Spec v4.2 | Firmware |
|------------|-----------|----------|
| **Data Object** | Brand Serial Number | ❌ Niet geïmplementeerd |
| **Type** | u8 / u8 (index / ASCII char) | OT_UNDEF |
| **Richting** | R/- | — |
| **Verplicht** | **Ja** (Slave) | — |

**Impact**: Boiler serienummer kan niet worden uitgelezen.

---

#### ID 96 — Cooling Operation Hours

| Eigenschap | Spec v4.2 | Firmware |
|------------|-----------|----------|
| **Data Object** | Cooling Operation Hours | ❌ Niet geïmplementeerd |
| **Type** | u16 | OT_UNDEF |
| **Richting** | R/W | — |
| **Bereik** | 0–65535 uur | — |
| **Verplicht** | Nee | — |

**Impact**: Koeluren worden niet bijgehouden/gepubliceerd. Relevant voor systemen met koelfunctie.

---

#### ID 97 — Power Cycles

| Eigenschap | Spec v4.2 | Firmware |
|------------|-----------|----------|
| **Data Object** | Power Cycles | ❌ Niet geïmplementeerd |
| **Type** | u16 | OT_UNDEF |
| **Richting** | R/W | — |
| **Bereik** | 0–65535 cycles | — |
| **Verplicht** | Nee | — |

**Impact**: Aan/uit-cycli van de boiler worden niet bijgehouden. Nuttige diagnostische informatie.

---

### 2.2 Richting (R/W) Afwijkingen

De `OTmsgcmd_t` waarde (OT_READ, OT_WRITE, OT_RW) in de OTmap bepaalt of inkomende berichten door de `is_value_valid()` functie worden geaccepteerd. Als een ID als OT_READ staat maar de master verstuurt een WRITE-DATA bericht, wordt de waarde niet opgeslagen, niet naar MQTT gepubliceerd en niet in de REST API getoond.

**Kern van het probleem**: `is_value_valid()` (OTGW-Core.ino:618-626) controleert:
```cpp
_valid = (OTlookup.msgcmd==OT_READ && OT.type==OT_READ_ACK);
_valid = _valid || (OTlookup.msgcmd==OT_WRITE && OTdata.type==OT_WRITE_DATA);
_valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OTdata.type==OT_WRITE_DATA));
```

Als het OTmap-type niet overeenkomt met het binnenkomende berichttype, wordt de waarde verworpen.

| ID | Naam | Firmware | Spec v4.2 | Gewenst | Impact |
|---:|------|----------|-----------|---------|--------|
| 27 | Toutside | OT_READ | R/W | OT_RW | **Hoog** — Als de thermostaat de buitentemperatuur naar de boiler schrijft (WRITE-DATA), wordt deze waarde genegeerd |
| 37 | TRoomCH2 | OT_READ | -/W | OT_WRITE | **Hoog** — Kamertemperatuur CH2 wordt geschreven door master, maar firmware verwacht READ-ACK |
| 38 | RelativeHumidity | OT_READ | R/W | OT_RW | Medium — Kan zowel gelezen als geschreven worden |
| 98 | RFstrengthbatterylevel | OT_READ | -/W | OT_WRITE | Medium — RF sensorstatus wordt geschreven door master |
| 99 | OperatingMode | OT_READ | R/W | OT_RW | Medium — Operating mode kan ook geschreven worden |
| 109 | ElectricityProducerStarts | OT_READ | R/W | OT_RW | Laag — Teller kan gereset worden via write |
| 110 | ElectricityProducerHours | OT_READ | R/W | OT_RW | Laag |
| 112 | CumulativElectrProd | OT_READ | R/W | OT_RW | Laag |
| 124 | OpenThermVersionMaster | OT_READ | -/W | OT_WRITE | **Hoog** — Master schrijft zijn protocolversie, maar firmware verwacht READ-ACK |
| 126 | MasterVersion | OT_READ | -/W | OT_WRITE | **Hoog** — Masterproductversie wordt geschreven, niet gelezen |

**Meest impactvolle afwijkingen**:
- **ID 27 (Toutside)**: Veel thermostaten schrijven een buitentemperatuur van een externe sensor naar de boiler. Als OTmap alleen READ accepteert, wordt de WRITE-DATA waarde genegeerd → de buitentemperatuur in de firmware/MQTT/API is onjuist of ontbreekt.
- **ID 37 (TRoomCH2)**: Kamertemperatuur voor CH2 kring wordt door master geschreven, nooit gelezen.
- **ID 124, 126 (Master versies)**: Master schrijft zijn versie/type informatie. De firmware zal nooit een geldige READ-ACK zien voor deze IDs.

---

### 2.3 Datatype Inconsistenties

#### ID 35 — FanSpeed

| Eigenschap | OTmap | processOT switch | Spec v4.2 |
|------------|-------|-----------------|-----------|
| Type | `ot_u8u8` | `print_u16()` | u8 / u8 |
| Struct | `uint16_t FanSpeed` | — | HB=setpoint Hz, LB=actual Hz |
| Eenheid | "rpm" | — | Hz (= RPM/60) |

**Probleem**: De OTmap definieert het type als `ot_u8u8`, maar de processOT switch gebruikt `print_u16()`. Dit is inconsistent. De spec definieert twee afzonderlijke bytes: HB = fan speed setpoint in Hz, LB = actual fan speed in Hz. Door `print_u16` te gebruiken worden de twee bytes als één 16-bit getal behandeld, wat de verkeerde waarde oplevert.

**Locatie**: OTGW-Core.h:365 (OTmap), OTGW-Core.ino:1808 (switch case)

**Oplossing**: Vervang `print_u16` door `print_u8u8` in de switch, óf maak een specifieke `print_fanspeed` functie die HB en LB als aparte waarden naar MQTT publiceert (setpoint en actual).

---

#### ID 38 — RelativeHumidity

| Eigenschap | Firmware | Spec v4.2 | Remeha info |
|------------|----------|-----------|-------------|
| Type | `ot_u8u8` | f8.8 | u8/u8 |
| Richting | OT_READ | R/W | — |

**Probleem**: De v4.2 specificatie definieert dit als f8.8 (signed fixed-point), maar de Remeha-specifieke documentatie (`New OT data-ids.txt`) noemt dit als u8/u8. De firmware volgt de Remeha-interpretatie.

**Advies**: Behoud `ot_u8u8` voor backwards-compatibiliteit met bestaande Remeha-installaties, maar documenteer de afwijking van de v4.2 spec. Overweeg het f8.8 type als optionele configuratie.

---

### 2.4 Bugs

#### BUG-001: Uurmasker in DayTime MQTT (ID 20) — **KRITIEK**

**Locatie**: `OTGW-Core.ino:1297`

**Probleem**: De bitmask voor het uur-veld in de MQTT-publicatie is `0x0F` (4 bits), maar moet `0x1F` (5 bits) zijn.

```cpp
// FOUT (huidige code, regel 1297):
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10));

// CORRECT (zou moeten zijn):
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10));
```

**Spec referentie**: ID 20, HB bits 4:0 = hours (0–23). Dit vereist 5 bits (0x1F).

**Impact**:
- Uren 0–15 worden correct gerapporteerd
- Uren 16–23 worden corrupt gerapporteerd via MQTT:
  - 16:00 → wordt 0:00
  - 17:00 → wordt 1:00
  - 18:00 → wordt 2:00
  - 23:00 → wordt 7:00
- De debug log op regel 1285 gebruikt wél correct `0x1F`
- **Alleen de MQTT-publicatie is getroffen**

**Ernst**: Kritiek — Verkeerde tijdinformatie in Home Assistant en andere MQTT-consumenten voor alle uren na 15:00.

**Fix**: Eén karakter wijzigen: `0x0F` → `0x1F` op regel 1297.

---

### 2.5 Eenheden en Label Fouten

#### LABEL-001: Eenheid FanSpeed (ID 35)
- **Huidig**: "rpm"
- **Spec v4.2**: Hz (= RPM/60)
- **Locatie**: OTGW-Core.h:365

#### LABEL-002: Eenheid DHWFlowRate (ID 19)
- **Huidig**: "l/m"
- **Spec v4.2**: "l/min" (liters per minuut)
- **Locatie**: OTGW-Core.h:349

#### LABEL-003: Typo MQTT topic "eletric_production" (ID 0, slave bit 7)
- **Huidig**: `sendMQTTData("eletric_production", ...)`
- **Correct**: `sendMQTTData("electric_production", ...)`
- **Locatie**: OTGW-Core.ino:780
- **Let op**: Wijziging breekt bestaande MQTT-abonnementen!

#### LABEL-004: Typo MQTT topic "solar_storage_slave_fault_incidator"
- **Huidig**: `sendMQTTData(F("solar_storage_slave_fault_incidator"), ...)`
- **Correct**: `sendMQTTData(F("solar_storage_slave_fault_indicator"), ...)`
- **Locatie**: OTGW-Core.ino:817
- **Let op**: Wijziging breekt bestaande MQTT-abonnementen!

---

### 2.6 Code Quality Issues

#### CQ-001: Inconsistente parameter gebruik in `is_value_valid()`
**Locatie**: OTGW-Core.ino:618-626

De functie accepteert parameter `OT` maar gebruikt soms de globale `OTdata`:
```cpp
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  if (OT.skipthis) return false;
  bool _valid = false;
  _valid = _valid || (OTlookup.msgcmd==OT_READ && OT.type==OT_READ_ACK);
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OTdata.type==OT_WRITE_DATA);  // ← globale OTdata!
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OTdata.type==OT_WRITE_DATA));  // ← gemixed
  _valid = _valid || (OTdata.id==OT_Statusflags) || ...;  // ← globale OTdata!
  return _valid;
}
```

**Impact**: Momenteel geen functioneel probleem omdat `OT` en `OTdata` altijd dezelfde waarde bevatten bij aanroep. Maar het is misleidend en foutgevoelig bij toekomstige refactoring.

#### CQ-002: Dubbel struct veld `RoomRemoteOverrideFunction`
**Locatie**: OTGW-Core.h:87 en OTGW-Core.h:136

De struct `OTdataStruct` heeft twee velden voor hetzelfde concept:
- Regel 87: `uint16_t RoomRemoteOverrideFunction = 0;`
- Regel 136: `uint16_t RemoteOverrideFunction = 0;`

De processOT switch gebruikt `RemoteOverrideFunction` (regel 136). Het veld `RoomRemoteOverrideFunction` (regel 87) lijkt ongebruikt.

#### CQ-003: Array bounds check ontbreekt voor OTmap
**Locatie**: OTGW-Core.ino:1660

```cpp
PROGMEM_readAnything(&OTmap[OTdata.id], OTlookupitem);
```

Als `OTdata.id > 133` (OT_MSGID_MAX), leest dit buiten de array bounds. Hoewel OpenTherm IDs tot 127 standaard gaan en 128-255 OEM-specifiek zijn, kan een corrupt bericht of Remeha-specifiek ID een out-of-bounds read veroorzaken.

**Opmerking**: Dit is een reeds bekende bug uit eerdere reviews.

---

## 3. Volledig Message ID Overzicht

### Legenda
- ✅ = Correct geïmplementeerd
- ⚠️ = Geïmplementeerd maar met problemen
- ❌ = Ontbreekt
- ➖ = Niet van toepassing (undefined in spec)

### Klasse 1: Control & Status (IDs 0, 1, 5, 8, 70-73, 101, 102, 115)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 0 | Status | ✅ | Alle master/slave bits correct geïmplementeerd incl. v4.2 bits 5-7 |
| 1 | TSet | ✅ | |
| 5 | ASFflags | ✅ | Alle fault flags correct gedecodeerd |
| 8 | TsetCH2 | ✅ | |
| 70 | StatusVH | ✅ | Ventilatie status correct |
| 71 | ControlSetpointVH | ✅ | |
| 72 | ASFFaultCodeVH | ✅ | |
| 73 | DiagnosticCodeVH | ✅ | |
| 101 | SolarStorageMaster | ✅ | Solar storage status correct incl. mode bits |
| 102 | SolarStorageASFflags | ✅ | |
| 115 | OEMDiagnosticCode | ✅ | |

### Klasse 2: Configuration (IDs 2, 3, 74-76, 93-95, 103, 104, 124-127)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 2 | MasterConfigMemberIDcode | ✅ | Smart Power bit correct |
| 3 | SlaveConfigMemberIDcode | ✅ | Alle 8 config bits correct incl. v4.2 bits 6-7 |
| 74 | ConfigMemberIDVH | ✅ | |
| 75 | OpenthermVersionVH | ✅ | |
| 76 | VersionTypeVH | ✅ | |
| 93 | Brand | ❌ | **Ontbreekt** — Verplicht voor slave |
| 94 | BrandVersion | ❌ | **Ontbreekt** — Verplicht voor slave |
| 95 | BrandSerialNumber | ❌ | **Ontbreekt** — Verplicht voor slave |
| 103 | SolarStorageSlaveConfig | ✅ | |
| 104 | SolarStorageVersionType | ✅ | |
| 124 | OpenThermVersionMaster | ⚠️ | Richting fout: OT_READ → OT_WRITE |
| 125 | OpenThermVersionSlave | ✅ | |
| 126 | MasterVersion | ⚠️ | Richting fout: OT_READ → OT_WRITE |
| 127 | SlaveVersion | ✅ | |

### Klasse 3: Remote Request (ID 4)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 4 | Command | ✅ | |

### Klasse 4: Sensor Data (IDs 16-39, 77-85, 96-98, 109-114, 116-123)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 16 | TrSet | ✅ | |
| 17 | RelModLevel | ✅ | |
| 18 | CHPressure | ✅ | |
| 19 | DHWFlowRate | ⚠️ | Eenheid "l/m" → "l/min" |
| 20 | DayTime | ⚠️ | **BUG**: Uurmasker MQTT 0x0F → 0x1F |
| 21 | Date | ✅ | |
| 22 | Year | ✅ | |
| 23 | TrSetCH2 | ✅ | |
| 24 | Tr | ✅ | |
| 25 | Tboiler | ✅ | |
| 26 | Tdhw | ✅ | |
| 27 | Toutside | ⚠️ | Richting fout: OT_READ → OT_RW |
| 28 | Tret | ✅ | |
| 29 | Tsolarstorage | ✅ | |
| 30 | Tsolarcollector | ✅ | s16 correct |
| 31 | TflowCH2 | ✅ | |
| 32 | Tdhw2 | ✅ | |
| 33 | Texhaust | ✅ | s16 correct |
| 34 | Theatexchanger | ✅ | |
| 35 | FanSpeed | ⚠️ | print_u16 vs ot_u8u8 inconsistentie; eenheid rpm vs Hz |
| 36 | ElectricalCurrentBurnerFlame | ✅ | |
| 37 | TRoomCH2 | ⚠️ | Richting fout: OT_READ → OT_WRITE |
| 38 | RelativeHumidity | ⚠️ | Richting OT_READ → OT_RW; type u8u8 vs spec f8.8 |
| 39 | TrOverride2 | ❌ | **Ontbreekt** |
| 77 | RelativeVentilation | ✅ | |
| 78 | RelativeHumidityExhaustAir | ✅ | |
| 79 | CO2LevelExhaustAir | ✅ | |
| 80 | SupplyInletTemperature | ✅ | |
| 81 | SupplyOutletTemperature | ✅ | |
| 82 | ExhaustInletTemperature | ✅ | |
| 83 | ExhaustOutletTemperature | ✅ | |
| 84 | ActualExhaustFanSpeed | ✅ | |
| 85 | ActualSupplyFanSpeed | ✅ | |
| 96 | CoolingOperationHours | ❌ | **Ontbreekt** |
| 97 | PowerCycles | ❌ | **Ontbreekt** |
| 98 | RFstrengthbatterylevel | ⚠️ | Richting fout: OT_READ → OT_WRITE |
| 109 | ElectricityProducerStarts | ⚠️ | Richting OT_READ → OT_RW |
| 110 | ElectricityProducerHours | ⚠️ | Richting OT_READ → OT_RW |
| 111 | ElectricityProduction | ✅ | |
| 112 | CumulativElectrProd | ⚠️ | Richting OT_READ → OT_RW |
| 113 | BurnerUnsuccessfulStarts | ✅ | |
| 114 | FlameSignalTooLow | ✅ | |
| 116 | BurnerStarts | ✅ | |
| 117 | CHPumpStarts | ✅ | |
| 118 | DHWPumpValveStarts | ✅ | |
| 119 | DHWBurnerStarts | ✅ | |
| 120 | BurnerOperationHours | ✅ | |
| 121 | CHPumpOperationHours | ✅ | |
| 122 | DHWPumpValveOperationHours | ✅ | |
| 123 | DHWBurnerOperationHours | ✅ | |

### Klasse 5: Remote Boiler Parameters (IDs 6, 48-57, 86, 87)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 6 | RBPflags | ✅ | |
| 48 | TdhwSetUBTdhwSetLB | ✅ | |
| 49 | MaxTSetUBMaxTSetLB | ✅ | |
| 50-55 | Remoteparameter boundaries | ✅ | v4.2 extended parameters |
| 56 | TdhwSet | ✅ | |
| 57 | MaxTSet | ✅ | |
| 58-63 | Remote parameters 3-8 | ✅ | v4.2 extended parameters |
| 86 | RemoteParameterSettingVH | ✅ | |
| 87 | NominalVentilationValue | ✅ | |

### Klasse 6: Transparent Slave Parameters (IDs 10, 11, 88, 89, 105, 106)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 10 | TSP | ✅ | |
| 11 | TSPindexTSPvalue | ✅ | |
| 88 | TSPNumberVH | ✅ | |
| 89 | TSPEntryVH | ✅ | |
| 105 | SolarStorageTSP | ✅ | |
| 106 | SolarStorageTSPindexTSPvalue | ✅ | |

### Klasse 7: Fault History (IDs 12, 13, 90, 91, 107, 108)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 12 | FHBsize | ✅ | |
| 13 | FHBindexFHBvalue | ✅ | |
| 90 | FaultBufferSizeVH | ✅ | |
| 91 | FaultBufferEntryVH | ✅ | |
| 107 | SolarStorageFHBsize | ✅ | |
| 108 | SolarStorageFHBindexFHBvalue | ✅ | |

### Klasse 8: Special Applications (IDs 7, 9, 14, 15, 99, 100)

| ID | Naam | Status | Opmerkingen |
|---:|------|--------|-------------|
| 7 | CoolingControl | ✅ | |
| 9 | TrOverride | ✅ | |
| 14 | MaxRelModLevelSetting | ✅ | |
| 15 | MaxCapacityMinModLevel | ✅ | |
| 99 | OperatingMode | ⚠️ | Richting OT_READ → OT_RW |
| 100 | RemoteOverrideFunction | ✅ | |

---

## 4. Gedetailleerde Taken Breakdown

### Fase 1: Kritieke Bug Fix (Prioriteit: URGENT) ✅ GEÏMPLEMENTEERD

#### Taak 1.1: Fix uurmasker DayTime MQTT (ID 20) ✅
- **Bestand**: `src/OTGW-firmware/OTGW-Core.ino`
- **Regel**: 1297
- **Wijziging**: `0x0F` → `0x1F`
- **Geschatte tijd**: 5 minuten
- **Risico**: Geen — pure bugfix, backwards compatible
- **Test**: Verifieer MQTT DayTime_hour output voor uren 16-23
- **Status**: ✅ Geïmplementeerd in commit 893c73e

```cpp
// Huidige code (FOUT):
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10));

// Gecorrigeerde code:
sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10));
```

---

### Fase 2: Richting (R/W) Correcties (Prioriteit: HOOG) ✅ GEÏMPLEMENTEERD

#### Taak 2.1: Fix richtingen in OTmap array ✅
- **Bestand**: `src/OTGW-firmware/OTGW-Core.h`
- **Status**: ✅ Geïmplementeerd in commit 893c73e
- **Wijzigingen** (10 regels in OTmap):

| Regel | ID | Oud | Nieuw |
|-------|---:|-----|-------|
| 357 | 27 | `OT_READ` | `OT_RW` |
| 367 | 37 | `OT_READ` | `OT_WRITE` |
| 368 | 38 | `OT_READ` | `OT_RW` |
| 428 | 98 | `OT_READ` | `OT_WRITE` |
| 429 | 99 | `OT_READ` | `OT_RW` |
| 439 | 109 | `OT_READ` | `OT_RW` |
| 440 | 110 | `OT_READ` | `OT_RW` |
| 442 | 112 | `OT_READ` | `OT_RW` |
| 454 | 124 | `OT_READ` | `OT_WRITE` |
| 456 | 126 | `OT_READ` | `OT_WRITE` |

- **Geschatte tijd**: 15 minuten
- **Risico**: Laag — Meer berichten worden geaccepteerd als geldig; bestaande functionaliteit wordt niet gebroken
- **Impact**: Correcte verwerking van WRITE-DATA en RW berichten voor deze IDs
- **Test**: Verifieer dat buitentemperatuur (ID 27) en master versie-informatie (ID 124/126) correct worden opgeslagen en via MQTT gepubliceerd

---

### Fase 3: Ontbrekende Message IDs Toevoegen (Prioriteit: MEDIUM) ✅ GEÏMPLEMENTEERD

#### Taak 3.1: Voeg ID 39 (TrOverride2) toe ✅

**Status**: ✅ Geïmplementeerd in commit 14e865d

**Stap 1** — Struct veld toevoegen in `OTGW-Core.h`:
```cpp
// Na regel 37 (TrOverride):
float TrOverride2 = 0.0f; // f8.8  Remote override room setpoint 2 (°C)
```

**Stap 2** — Enum waarde toevoegen in `OTGW-Core.h` (al aanwezig als gap, ID 39 zit in sequentie na ID 38):
```cpp
// Geen wijziging nodig — ID 39 wordt al afgehandeld via OTmap[39]
```

**Stap 3** — OTmap entry updaten:
```cpp
// Vervang:
{  39, OT_UNDEF , ot_undef, "", "", "" },
// Door:
{  39, OT_READ  , ot_f88,  "TrOverride2", "Remote override room setpoint 2", "°C" },
```

**Stap 4** — processOT switch case toevoegen:
```cpp
case 39: print_f88(OTcurrentSystemState.TrOverride2); break;
```

**Stap 5** — getOTGWValue case toevoegen:
```cpp
case 39: return String(OTcurrentSystemState.TrOverride2); break;
```

---

#### Taak 3.2: Voeg IDs 93-95 (Brand info) toe ✅

**Status**: ✅ Geïmplementeerd in commit 14e865d

**Stap 1** — Struct velden toevoegen in `OTGW-Core.h`:
```cpp
uint16_t BrandIndex = 0;          // u8 / u8 Brand name index / character
uint16_t BrandVersionIndex = 0;   // u8 / u8 Brand version index / character
uint16_t BrandSerialIndex = 0;    // u8 / u8 Brand serial number index / character
```

**Stap 2** — OTmap entries updaten:
```cpp
// Vervang 93-95 UNDEF entries door:
{  93, OT_READ  , ot_u8u8, "Brand", "Boiler brand name (index/char)", "" },
{  94, OT_READ  , ot_u8u8, "BrandVersion", "Boiler brand version (index/char)", "" },
{  95, OT_READ  , ot_u8u8, "BrandSerialNumber", "Boiler brand serial number (index/char)", "" },
```

**Stap 3** — processOT switch cases toevoegen:
```cpp
case 93: print_u8u8(OTcurrentSystemState.BrandIndex); break;
case 94: print_u8u8(OTcurrentSystemState.BrandVersionIndex); break;
case 95: print_u8u8(OTcurrentSystemState.BrandSerialIndex); break;
```

**Stap 4** — getOTGWValue cases toevoegen:
```cpp
case 93: return String(OTcurrentSystemState.BrandIndex); break;
case 94: return String(OTcurrentSystemState.BrandVersionIndex); break;
case 95: return String(OTcurrentSystemState.BrandSerialIndex); break;
```

**Opmerking**: De brand strings (IDs 93-95) gebruiken een index/karakter patroon. Elke READ-DATA met een index retourneert een enkel ASCII karakter. Om de volledige string te lezen zijn meerdere queries nodig. De huidige u8u8 implementatie toont index en karakter als aparte waarden, wat een goede basisfunctionaliteit biedt. Een toekomstige verbetering zou een string-accumulatie functie kunnen zijn.

---

#### Taak 3.3: Voeg IDs 96-97 (Counters) toe ✅

**Status**: ✅ Geïmplementeerd in commit 14e865d

**Stap 1** — Struct velden toevoegen:
```cpp
uint16_t CoolingOperationHours = 0; // u16 Cooling operation hours
uint16_t PowerCycles = 0;           // u16 Power cycles
```

**Stap 2** — OTmap entries updaten:
```cpp
// Vervang 96-97 UNDEF entries door:
{  96, OT_RW    , ot_u16,  "CoolingOperationHours", "Cooling operation hours", "hrs" },
{  97, OT_RW    , ot_u16,  "PowerCycles", "Power cycles", "" },
```

**Stap 3** — processOT en getOTGWValue cases toevoegen.

---

### Fase 4: Datatype Correcties (Prioriteit: MEDIUM) ✅ GEÏMPLEMENTEERD

#### Taak 4.1: Fix FanSpeed (ID 35) print functie ✅

**Status**: ✅ Geïmplementeerd in commit 53973e9 (Optie A: print_u16 → print_u8u8)

**Optie A** (minimaal): Vervang `print_u16` door `print_u8u8` in de switch:
```cpp
// Was:
case OT_FanSpeed: print_u16(OTcurrentSystemState.FanSpeed); break;
// Wordt:
case OT_FanSpeed: print_u8u8(OTcurrentSystemState.FanSpeed); break;
```

**Optie B** (optimaal): Maak een specifieke `print_fanspeed` functie die HB als "setpoint" en LB als "actual" publiceert:
```cpp
void print_fanspeed(uint16_t& value) {
  AddLogf("%s = Setpoint[%d Hz] Actual[%d Hz]", OTlookupitem.label, OTdata.valueHB, OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)) {
    char _msg[10] {0};
    sendMQTTData(F("fanspeed_setpoint"), itoa(OTdata.valueHB, _msg, 10));
    sendMQTTData(F("fanspeed_actual"), itoa(OTdata.valueLB, _msg, 10));
    value = OTdata.u16();
  }
}
```

**Advies**: Optie B biedt de meeste waarde voor gebruikers.

---

### Fase 5: Eenheid/Label Correcties (Prioriteit: LAAG) ⚠️ DEELS GEÏMPLEMENTEERD

#### Taak 5.1: Fix eenheid FanSpeed ✅
- **Bestand**: `OTGW-Core.h:365`
- **Wijziging**: `"rpm"` → `"Hz"`
- **Status**: ✅ Geïmplementeerd in commit 53973e9

#### Taak 5.2: Fix eenheid DHWFlowRate ✅
- **Bestand**: `OTGW-Core.h:349`
- **Wijziging**: `"l/m"` → `"l/min"`
- **Status**: ✅ Geïmplementeerd in commit 53973e9

#### Taak 5.3: Fix typo "eletric_production" (BREAKING CHANGE) ⏭️ OVERGESLAGEN
- **Reden**: Breaking change voor bestaande Home Assistant automations
- **Bestand**: `OTGW-Core.ino:780`
- **Huidige waarde**: `"eletric_production"`
- **Correcte waarde**: `"electric_production"`
- **⚠️ BREAKING**: Bestaande Home Assistant automations die dit MQTT topic gebruiken zullen breken!
- **Advies**: Documenteer als bekende typo, overweeg backwards-compatible aanpak (publiceer op beide topics tijdelijk)

#### Taak 5.4: Fix typo "solar_storage_slave_fault_incidator" (BREAKING CHANGE) ⏭️ OVERGESLAGEN
- **Reden**: Breaking change voor bestaande Home Assistant automations
- **Bestand**: `OTGW-Core.ino:817`
- **Huidige waarde**: `"solar_storage_slave_fault_incidator"`
- **Correcte waarde**: `"solar_storage_slave_fault_indicator"`
- **⚠️ BREAKING**: Zelfde overwegingen als 5.3

---

### Fase 6: Code Quality Verbeteringen (Prioriteit: LAAG) ⚠️ DEELS GEÏMPLEMENTEERD

#### Taak 6.1: Fix `is_value_valid()` parameter consistentie ✅
- Vervang alle `OTdata.type` en `OTdata.id` door `OT.type` en `OT.id` in de functie.
- **Status**: ✅ Geïmplementeerd in commit 53973e9

#### Taak 6.2: Verwijder ongebruikt struct veld `RoomRemoteOverrideFunction` ⏭️ OVERGESLAGEN
- **Reden**: Veld wordt mogelijk door externe code/tools gerefereerd; vereist bredere impactanalyse
- Verifieer eerst of het veld nergens anders gebruikt wordt.
- Verwijder het veld als het inderdaad ongebruikt is.

#### Taak 6.3: Array bounds check toevoegen voor OTmap ⏭️ OVERGESLAGEN
- **Reden**: Reeds geïdentificeerd in eerdere codebase review; apart issue voor tracking
- Voeg een check toe voordat `OTmap[OTdata.id]` wordt benaderd:
```cpp
if (OTdata.id <= OT_MSGID_MAX) {
  PROGMEM_readAnything(&OTmap[OTdata.id], OTlookupitem);
} else {
  // Handle unknown ID gracefully
}
```

---

## 5. Implementatie Planning

### Prioriteitsmatrix

| Prioriteit | Fase | Beschrijving | Impact | Risico | Status |
|:----------:|:----:|-------------|--------|--------|:------:|
| 🔴 URGENT | 1 | Bug fix uurmasker ID 20 | Hoog | Geen | ✅ |
| 🟠 HOOG | 2 | R/W richting correcties (10 IDs) | Hoog | Laag | ✅ |
| 🟡 MEDIUM | 3 | Ontbrekende IDs toevoegen (6 IDs) | Medium | Laag | ✅ |
| 🟡 MEDIUM | 4 | Datatype correctie FanSpeed | Medium | Laag | ✅ |
| 🟢 LAAG | 5 | Eenheid/label fixes | Laag | ⚠️ Breaking | ⚠️ Deels |
| 🟢 LAAG | 6 | Code quality | Laag | Laag | ⚠️ Deels |

### Aanbevolen Volgorde

1. **Sprint 1 (Urgent)**: Fase 1 + 2 — Bug fix + R/W correcties
2. **Sprint 2 (Belangrijk)**: Fase 3 + 4 — Ontbrekende IDs + datatype fix
3. **Sprint 3 (Nice-to-have)**: Fase 5 + 6 — Labels + code quality

### Backwards Compatibiliteit

- **Fase 1-4**: Volledig backwards compatible. Geen bestaande functionaliteit wordt gebroken.
- **Fase 5 (labels)**: ⚠️ MQTT topic namen wijzigen = **BREAKING CHANGE** voor bestaande automations.
  - **Mitigatie**: Publiceer tijdelijk op zowel oud als nieuw topic, met deprecation notice in release notes.
- **Fase 6**: Volledig backwards compatible.

---

## 6. Toekomstige Verbeteringen (Buiten Scope)

### 6.1 Brand String Accumulatie (IDs 93-95)
Implementeer een mechanisme om meerdere READ-DATA requests te doen voor IDs 93-95 om volledige brand strings (tot 50 karakters) op te bouwen en als enkele MQTT string te publiceren.

### 6.2 MQTT Auto Discovery voor Nieuwe IDs
Zorg dat de `doAutoConfigureMsgid()` functie correcte Home Assistant MQTT discovery configuratie stuurt voor alle nieuwe message IDs (39, 93-97).

### 6.3 Enhanced Ventilation Status Decoding
De VH status bits (ID 70) worden momenteel als flag8 flags gepubliceerd. Overweeg meer gebruiksvriendelijke MQTT topics voor ventilatie-specifieke statussen.

### 6.4 Counter Overflow Handling
De v4.2 spec waarschuwt dat u16 counters (IDs 96, 97, 109-114, 116-123) overflown bij 65535. Overweeg overflow-detectie logica.

---

## 7. Referenties

- **Spec v4.2**: `Specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`
- **Spec v4.2 (PDF)**: `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
- **Remeha data-IDs**: `Specification/New OT data-ids.txt`
- **OTmap definitie**: `src/OTGW-firmware/OTGW-Core.h:329-468`
- **processOT functie**: `src/OTGW-firmware/OTGW-Core.ino:1720-1834`
- **getOTGWValue functie**: `src/OTGW-firmware/OTGW-Core.ino:2042-2156`
- **is_value_valid functie**: `src/OTGW-firmware/OTGW-Core.ino:618-626`
- **print_daytime functie**: `src/OTGW-firmware/OTGW-Core.ino:1269-1304`
- **OTGW PIC firmware docs**: https://otgw.tclcode.com/firmware.html
