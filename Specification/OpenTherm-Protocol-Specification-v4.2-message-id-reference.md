# OpenTherm v4.2 – Message-ID referentietabel (uitgebreid)

Bron: `OpenTherm-Protocol-Specification-v4.2.md` (sectie **5.4 Data-Id Overview Map**, pagina 47-49).

Deze tabel bevat de geëxtraheerde berichtregels met een extra, uitgebreide attribuutuitleg per Message-ID.

| ID | Msg (R/W) | Data object | Type | Beschrijving (v4.2) | Uitgebreide attribuutuitleg |
|---:|:---:|---|---|---|---|
| 0 | R/- | Status | flag8 / flag8 | Master and Slave Status flags. | Alleen Read ondersteund. Type: Twee bytes met statusflags (master/slave of functie-specifiek). |
| 1 | -/W | Tset | f8.8 | Control Setpoint i.e. CH water temperature Setpoint (°C) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 2 | -/W | M-Config / M-MemberIDcode | flag8 / u8 | Master Configuration Flags / Master MemberID Code | Alleen Write ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 3 | R/- | S-Config / S-MemberIDcode | flag8 / u8 | Slave Configuration Flags / Slave MemberID Code | Alleen Read ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 4 | -/W | Remote Request | u8 / u8 | Remote Request | Alleen Write ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 5 | R/- | ASF-flags / OEM-fault-code | flag8 / u8 | Application-specific fault flags and OEM fault code | Alleen Read ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 6 | R/- | RBP-flags | flag8 / flag8 | Remote boiler parameter transfer-enable & read/write flags | Alleen Read ondersteund. Type: Twee bytes met statusflags (master/slave of functie-specifiek). |
| 7 | -/W | Cooling-control | f8.8 | Cooling control signal (%) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 8 | -/W | TsetCH2 | f8.8 | Control Setpoint for 2e CH circuit (°C) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 9 | R/- | TrOverride | f8.8 | Remote override room Setpoint | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 10 | R/- | TSP | u8 / u8 | Number of Transparent-Slave-Parameters supported by slave | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 11 | R/W | TSP-index / TSP-value | u8 / u8 | Index number / Value of referred-to transparent slave parameter. | Read én Write ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 12 | R/- | FHB-size | u8 / u8 | Size of Fault-History-Buffer supported by slave | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 13 | R/- | FHB-index / FHB-value | u8 / u8 | Index number / Value of referred-to fault-history buffer entry. | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 14 | -/W | Max-rel-mod-level-setting | f8.8 | Maximum relative modulation level setting (%) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 15 | R/- | Max-Capacity / Min-Mod-Level | u8 / u8 | Maximum boiler capacity (kW) / Minimum boiler modulation level(%) | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 16 | -/W | TrSet | f8.8 | Room Setpoint (°C) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 17 | R/- | Rel.-mod-level | f8.8 | Relative Modulation Level (%) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 18 | R/- | CH-pressure | f8.8 | Water pressure in CH circuit (bar) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 19 | R/- | DHW-flow-rate | f8.8 | Water flow rate in DHW circuit. (litres/minute) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 20 | R/W | Day-Time | special / u8 | Day of Week and Time of Day | Read én Write ondersteund. Type: Speciaal samengesteld formaat met aanvullende 8-bit waarde. |
| 21 | R/W | Date | u8 / u8 | Calendar date | Read én Write ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 22 | R/W | Year | u16 | Calendar year | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 23 | -/W | TrSetCH2 | f8.8 | Room Setpoint for 2nd CH circuit (°C) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 24 | -/W | Tr | f8.8 | Room temperature (°C) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 25 | R/- | Tboiler | f8.8 | Boiler flow water temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 26 | R/- | Tdhw | f8.8 | DHW temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 27 | R/W | Toutside | f8.8 | Outside temperature (°C) | Read én Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 28 | R/- | Tret | f8.8 | Return water temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 29 | R/- | Tstorage | f8.8 | Solar storage temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 30 | R/- | Tcollector | f8.8 | Solar collector temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 31 | R/- | TflowCH2 | f8.8 | Flow water temperature CH2 circuit (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 32 | R/- | Tdhw2 | f8.8 | Domestic hot water temperature 2 (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 33 | R/- | Texhaust | s16 | Boiler exhaust temperature (°C) | Alleen Read ondersteund. Type: Signed 16-bit integer (-32768..32767). |
| 34 | R/- | Tboiler-heat-exchanger | f8.8 | Boiler heat exchanger temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 35 | R/- | Boiler fan speed Setpoint and actual | u8 / u8 | Boiler fan speed Setpoint and actual value | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 36 | R/- | Flame current | f8.8 | Electrical current through burner flame [μA] | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 37 | -/W | TrCH2 | f8.8 | Room temperature for 2nd CH circuit (°C) | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 38 | R/W | Relative Humidity | f8.8 | Actual relative humidity as a percentage | Read én Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 39 | R/- | TrOverride 2 | f8.8 | Remote Override Room Setpoint 2 | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 48 | R/- | TdhwSet-UB / TdhwSet-LB | s8 / s8 | DHW Setpoint upper & lower bounds for adjustment (°C) | Alleen Read ondersteund. Type: Twee signed 8-bit waarden, vaak gebruikt voor onder/bovengrenzen. |
| 49 | R/- | MaxTSet-UB / MaxTSet-LB | s8 / s8 | Max CH water Setpoint upper & lower bounds for adjustment (°C) | Alleen Read ondersteund. Type: Twee signed 8-bit waarden, vaak gebruikt voor onder/bovengrenzen. |
| 56 | R/W | TdhwSet | f8.8 | DHW Setpoint (°C) (Remote parameter 1) | Read én Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 57 | R/W | MaxTSet | f8.8 | Max CH water Setpoint (°C) (Remote parameters 2) | Read én Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 70 | R/- | Status ventilation / heat-recovery | flag8 / flag8 | Master and Slave Status flags ventilation / heat-recovery | Alleen Read ondersteund. Type: Twee bytes met statusflags (master/slave of functie-specifiek). |
| 71 | -/W | Vset | - / u8 | Relative ventilation position (0-100%). 0% is the minimum set ventilation and 100% is the maximum set ventilation. | Alleen Write ondersteund. Type: Alleen low-byte waarde gebruikt; high-byte gereserveerd. |
| 72 | R/- | ASF-flags / OEM-fault-code ventilation / heat-recovery | flag8 / u8 | Application-specific fault flags and OEM fault code ventilation / heat- recovery | Alleen Read ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 73 | R/- | OEM diagnostic code ventilation / heat-recovery | u16 | An OEM-specific diagnostic/service code for ventilation / heat-recovery system | Alleen Read ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 74 | R/- | S-Config / S-MemberIDcode ventilation / heat-recovery | flag8 / u8 | Slave Configuration Flags / Slave MemberID Code ventilation / heat-recovery | Alleen Read ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 75 | R/- | OpenTherm version ventilation / heat-recovery | f8.8 | The implemented version of the OpenTherm Protocol Specification in the ventilation / heat-recovery system. | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 76 | R/- | Ventilation / heat-recovery version | u8 / u8 | Ventilation / heat-recovery product version number and type | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 77 | R/- | Rel-vent-level | - / u8 | Relative ventilation (0-100%) | Alleen Read ondersteund. Type: Alleen low-byte waarde gebruikt; high-byte gereserveerd. |
| 78 | R/W | RH-exhaust | - / u8 | Relative humidity exhaust air (0-100%) | Read én Write ondersteund. Type: Alleen low-byte waarde gebruikt; high-byte gereserveerd. |
| 79 | R/W | CO2-exhaust | u16 | CO2 level exhaust air (operationeel 0-2000 ppm; zie interpretatienotities) | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535); operationeel volgens v4.2 gebruikt voor 0-2000 ppm. |
| 80 | R/- | Tsi | f8.8 | Supply inlet temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 81 | R/- | Tso | f8.8 | Supply outlet temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 82 | R/- | Tei | f8.8 | Exhaust inlet temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 83 | R/- | Teo | f8.8 | Exhaust outlet temperature (°C) | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 84 | R/- | RPM-exhaust | u16 | Exhaust fan speed in rpm | Alleen Read ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 85 | R/- | RPM-supply | u16 | Supply fan speed in rpm | Alleen Read ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 86 | R/- | RBP-flags ventilation / heat-recovery | flag8 / flag8 | Remote ventilation / heat-recovery parameter transfer-enable & read/write flags | Alleen Read ondersteund. Type: Twee bytes met statusflags (master/slave of functie-specifiek). |
| 87 | R/W | Nominal ventilation value | u8 / - | Nominal relative value for ventilation (0-100 %) | Read én Write ondersteund. Type: Alleen één 8-bit waarde gebruikt; tweede deel gereserveerd. |
| 88 | R/- | TSP ventilation / heat-recovery | u8 / u8 | Number of Transparent-Slave-Parameters supported by TSP’s ventilation / heat-recovery | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 89 | R/W | TSP-index / TSP-value ventilation / heat-recovery | u8 / u8 | Index number / Value of referred-to transparent TSP’s ventilation / heat-recovery parameter. | Read én Write ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 90 | R/- | FHB-size ventilation / heat-recovery | u8 / u8 | Size of Fault-History-Buffer supported by ventilation / heat-recovery | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 91 | R/- | FHB-index / FHB-value ventilation / heat-recovery | u8 / u8 | Index number / Value of referred-to fault-history buffer entry ventilation / heat-recovery | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 93 | R/- | Brand | u8 / u8 | Index number of the character in the text string ASCII character referenced by the above index number | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 94 | R/- | Brand Version | u8 / u8 | Index number of the character in the text string ASCII character referenced by the above index number | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 95 | R/- | Brand Serial Number | u8 / u8 | Index number of the character in the text string ASCII character referenced by the above index number | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 96 | R/W | Cooling Operation Hours | u16 | Number of hours that the slave is in Cooling Mode. Reset by zero is optional for slave | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 97 | R/W | Power Cycles | u16 | Number of Power Cycles of a slave (wake-up after Reset), Reset by zero is optional for slave | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 98 | -/W | RF sensor status information | special / special | For a specific RF sensor the RF strength and battery level is written | Alleen Write ondersteund. Type: Speciaal (niet-generiek) dataformaat; interpretatie per tabeltoelichting. |
| 99 | R/W | Remote Override Operating Mode Heating/DHW | special / special | Operating Mode HC1, HC2/ Operating Mode DHW | Read én Write ondersteund. Type: Speciaal (niet-generiek) dataformaat; interpretatie per tabeltoelichting. |
| 100 | R/- | Remote override function | flag8 / - | Function of manual and program changes in master and remote room Setpoint | Alleen Read ondersteund. Type: Flag-byte in gebruik; tweede byte gereserveerd. |
| 101 | R/- | Status Solar Storage | flag8 / flag8 | Master and Slave Status flags Solar Storage | Alleen Read ondersteund. Type: Twee bytes met statusflags (master/slave of functie-specifiek). |
| 102 | R/- | ASF-flags / OEM-fault-code Solar Storage | flag8 / u8 | Application-specific fault flags and OEM fault code Solar Storage | Alleen Read ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 103 | R/- | S-Config / S-MemberIDcode Solar Storage | flag8 / u8 | Slave Configuration Flags / Slave MemberID Code Solar Storage | Alleen Read ondersteund. Type: Combinatie van bitflags en 8-bit numerieke code. |
| 104 | R/- | Solar Storage version | u8 / u8 | Solar Storage product version number and type | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 105 | R/- | TSP Solar Storage | u8 / u8 | Number of Transparent-Slave-Parameters supported by TSP’s Solar Storage | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 106 | R/W | TSP-index / TSP-value Solar Storage | u8 / u8 | Index number / Value of referred-to transparent TSP’s Solar Storage parameter. | Read én Write ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 107 | R/- | FHB-size Solar Storage | u8 / u8 | Size of Fault-History-Buffer supported by Solar Storage | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 108 | R/- | FHB-index / FHB-value Solar Storage | u8 / u8 | Index number / Value of referred-to fault-history buffer entry Solar Storage | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 109 | R/W | Electricity producer starts | u16 | Number of start of the electricity producer. | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 110 | R/W | Electricity producer hours | u16 | Number of hours the electricity producer is in operation | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 111 | R/- | Electricity production | u16 | Current electricity production in Watt. | Alleen Read ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 112 | R/W | Cumulative Electricity production | u16 | Cumulative electricity production in kWh. | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 113 | R/W | Un-successful burner starts | u16 | Number of un-successful burner starts | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 114 | R/W | Flame signal too low number | u16 | Number of times flame signal was too low | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 115 | R/- | OEM diagnostic code | u16 | OEM-specific diagnostic/service code | Alleen Read ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 116 | R/W | Successful Burner starts | u16 | Number of successful starts burner | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 117 | R/W | CH pump starts | u16 | Number of starts CH pump | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 118 | R/W | DHW pump/valve starts | u16 | Number of starts DHW pump/valve | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 119 | R/W | DHW burner starts | u16 | Number of starts burner during DHW mode | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 120 | R/W | Burner operation hours | u16 | Number of hours that burner is in operation (i.e. flame on) | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 121 | R/W | CH pump operation hours | u16 | Number of hours that CH pump has been running | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 122 | R/W | DHW pump/valve operation hours | u16 | Number of hours that DHW pump has been running or DHW valve has been opened | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 123 | R/W | DHW burner operation hours | u16 | Number of hours that burner is in operation during DHW mode | Read én Write ondersteund. Type: Unsigned 16-bit integer (0..65535). |
| 124 | -/W | OpenTherm version Master | f8.8 | The implemented version of the OpenTherm Protocol Specification in the master. | Alleen Write ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 125 | R/- | OpenTherm version Slave | f8.8 | The implemented version of the OpenTherm Protocol Specification in the slave. | Alleen Read ondersteund. Type: Signed fixed-point waarde met 8 fractiebits (resolutie 1/256). |
| 126 | -/W | Master-version | u8 / u8 | Master product version number and type | Alleen Write ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |
| 127 | R/- | Slave-version | u8 / u8 | Slave product version number and type | Alleen Read ondersteund. Type: Twee 8-bit velden (HB/LB) binnen één 16-bit frame. |

## Extra toelichting per ID-range

- **0-39**: Kernregeling (status, setpoints, sensoren en basisregeling).
- **48-49, 56-57**: Parametergrenzen en boiler-setpoints (met hiaten in deze ID-reeks).
- **70-91**: Ventilatie / heat-recovery uitbreiding.
- **93-100**: Merk/identificatie, remote override en RF/special data.
- **101-108**: Solar storage uitbreidingen.
- **109-123**: Bedrijfsuren/tellers en service-diagnostiek.
- **124-127**: Protocolversie en productversie master/slave.

## Interpretatienotities (v4.2 beperkingen)

- **ID 79 (CO2-exhaust)**: het veld is `u16` (technisch 0..65535), maar de v4.2 omschrijving gebruikt operationeel **0-2000 ppm**.
- **ID 112 (Cumulative Electricity production)**: `u16` betekent dat tellerwaarden kunnen overrollen; behandel deze waarde daarom als protocolbeperkte teller.
