# OpenTherm specificaties (Markdown)
Deze markdown is een omzetting van de tekstbestanden in deze map.

## Bronnen
- `OT spec 2.3b.txt`
- `OT-specification2.3b.txt`
- `OT-specification2.3b-todo.txt`
- `New OT data-ids.txt`
- `OT protocol version information.txt`
- `Integration homeassistant.txt`
- `OpenTherm-Protocol-Specification-v4.2.pdf`
- `Opentherm Protocol v2.2.pdf`

## Data-ID map (uit `OT spec 2.3b.txt`)

| ID | Naam | Richting | Type 1 | Min 1 | Max 1 | Default 1 | Type 2 | Min 2 | Max 2 | Default 2 | OTGW |
|---|---|---|---|---:|---:|---|---|---:|---:|---|---|
| 0 | STATUS | READ | FLAG | 00000000 | FLAG | 000000000 | Yes |  |  |  |  |
| 1 | CONTROL SETPOINT | WRITE | F8.8 | 0 | 100 | 10,00 | Yes |  |  |  |  |
| 2 | MASTER CONFIG/MEMBERID | WRITE | FLAG | 00000000 | U8 | 0 | 255 | 0 | Yes |  |  |
| 3 | SLAVE CONFIG/MEMBERID | READ | FLAG | 00000000 | U8 | 0 | 255 | 0 | Yes |  |  |
| 4 | COMMAND | WRITE | U8 | 0 | 255 | 2 | U8 | 0 | 255 | 0 | Yes |
| 5 | FAULT FLAGS/CODE | READ | FLAG | 00000000 | U8 | 0 | 255 | 0 | Yes |  |  |
| 6 | REMOTE PARAMETER SETTINGS | READ | FLAG | 00000000 | FLAG | 00000000 | Yes |  |  |  |  |
| 7 | COOLING CONTROL | WRITE | F8.8 | 0 | 100 | 0,00 | Yes |  |  |  |  |
| 8 | TsetCH2 | WRITE | F8.8 | 0 | 100 | 10,00 | Yes |  |  |  |  |
| 9 | REMOTE ROOM SETPOINT | READ | F8.8 | -40 | 127 | 0,00 | Yes |  |  |  |  |
| 10 | TSP NUMBER | READ | U8 | 0 | 255 | 0 | U8 | 0 | 0 | 0 | Yes |
| 11 | TSP ENTRY | READ | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | Yes |
| 11 | TSP ENTRY | WRITE | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | No |
| 12 | FAULT BUFFER SIZE | READ | U8 | 0 | 255 | 0 | U8 | 0 | 0 | 0 | Yes |
| 13 | FAULT BUFFER ENTRY | READ | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | Yes |
| 14 | CAPACITY SETTING | WRITE | F8.8 | 0 | 100 | 0,00 | Yes |  |  |  |  |
| 15 | MAX CAPACITY / MIN-MOD-LEVEL | READ | U8 | 0 | 255 | 0 | U8 | 0 | 100 | 0 | Yes |
| 16 | ROOM SETPOINT | WRITE | F8.8 | -40 | 127 | 0,00 | Yes |  |  |  |  |
| 17 | RELATIVE MODULATION LEVEL | READ | F8.8 | 0 | 100 | 0,00 | Yes |  |  |  |  |
| 18 | CH WATER PRESSURE | READ | F8.8 | 0 | 5 | 0,00 | Yes |  |  |  |  |
| 19 | DHW FLOW RATE | READ | F8.8 | 0 | 16 | 0,00 | Yes |  |  |  |  |
| 20 | DAY - TIME | READ | U8 | 0 | 255 | 0 | U8 | 0 | 59 | 0 | Yes |
| 20 | DAY - TIME | WRITE | U8 | 0 | 255 | 0 | U8 | 0 | 59 | 0 | No |
| 21 | DATE | READ | U8 | 1 | 12 | 1 | U8 | 1 | 31 | 1 | Yes |
| 21 | DATE | WRITE | U8 | 1 | 12 | 1 | U8 | 1 | 31 | 1 | No |
| 22 | YEAR | READ | U16 | 1900 | 2099 | 2002 | Yes |  |  |  |  |
| 22 | YEAR | WRITE | U16 | 1900 | 2099 | 2002 | No |  |  |  |  |
| 23 | SECOND ROOM SETPOINT | WRITE | F8.8 | -40 | 127 | 0,00 | Yes |  |  |  |  |
| 24 | ROOM TEMPERATURE | WRITE | F8.8 | -40 | 127 | 20,00 | Yes |  |  |  |  |
| 25 | BOILER WATER TEMP. | READ | F8.8 | -40 | 127 | 20,00 | Yes |  |  |  |  |
| 26 | DHW TEMPERATURE | READ | F8.8 | -40 | 127 | 20,00 | Yes |  |  |  |  |
| 27 | OUTSIDE TEMPERATURE | READ | F8.8 | -40 | 127 | 10,00 | Yes |  |  |  |  |
| 28 | RETURN WATER TEMPERATURE | READ | F8.8 | -40 | 127 | 19,00 | Yes |  |  |  |  |
| 29 | SOLAR STORAGE TEMPERATURE | READ | F8.8 | -40 | 127 | 0,00 | Yes |  |  |  |  |
| 30 | SOLAR COLLECTOR TEMPERATURE | READ | F8.8 | -40 | 127 | 0,00 | Yes |  |  |  |  |
| 31 | SECOND BOILER WATER TEMP. | READ | F8.8 | -40 | 127 | 20,00 | Yes |  |  |  |  |
| 32 | SECOND DHW TEMPERATURE | READ | F8.8 | -40 | 127 | 20,00 | Yes |  |  |  |  |
| 32 | EXHAUST TEMPERATURE | READ | S16 | -40 | 127 | 20 | Yes |  |  |  |  |
| 48 | DHW SETPOINT BOUNDS | READ | S8 | 0 | 127 | 0 | S8 | 0 | 127 | 0 | Yes |
| 49 | MAX CH SETPOINT BOUNDS | READ | S8 | 0 | 127 | 10 | S8 | 0 | 127 | 90 | Yes |
| 50 | OTC HC-RATIO BOUNDS | READ | S8 | 0 | 40 | 0 | S8 | 0 | 40 | 0 | Yes |
| 56 | DHW SETPOINT | READ | F8.8 | 0 | 127 | 10,00 | Yes |  |  |  |  |
| 56 | DHW SETPOINT | WRITE | F8.8 | 0 | 127 | 10,00 | No |  |  |  |  |
| 57 | MAX CH WATER SETPOINT | READ | F8.8 | 0 | 127 | 90,00 | Yes |  |  |  |  |
| 57 | MAX CH WATER SETPOINT | WRITE | F8.8 | 0 | 127 | 90,00 | No |  |  |  |  |
| 58 | OTC HEATCURVE RATIO | READ | F8.8 | 0 | 40 | 0,00 | Yes |  |  |  |  |
| 58 | OTC HEATCURVE RATIO | WRITE | F8.8 | 0 | 40 | 0,00 | No |  |  |  |  |
| 70 | STATUS V/H | READ | FLAG | 00000000 | FLAG | 000000000 | Yes |  |  |  |  |
| 71 | CONTROL SETPOINT V/H | WRITE | U8 | 0 | 100 | 0 | Yes |  |  |  |  |
| 72 | FAULT FLAGS/CODE V/H | READ | FLAG | 00000000 | U8 | 0 | 255 | 0 | Yes |  |  |
| 73 | DIAGNOSTIC CODE V/H | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 74 | CONFIG/MEMBERID V/H | READ | FLAG | 00000000 | U8 | 0 | 255 | 0 | Yes |  |  |
| 75 | OPENTHERM VERSION V/H | READ | F8.8 | 0 | 127 | 2,32 | Yes |  |  |  |  |
| 76 | VERSION & TYPE V/H | READ | U8 | 0 | 255 | 1 | U8 | 0 | 255 | 0 | Yes |
| 77 | RELATIVE VENTILATION | READ | U8 | 0 | 255 | 0 | Yes |  |  |  |  |
| 78 | RELATIVE HUMIDITY | READ | U8 | 0 | 255 | 0 | Yes |  |  |  |  |
| 78 | RELATIVE HUMIDITY | WRITE | U8 | 0 | 255 | 0 | No |  |  |  |  |
| 79 | CO2 LEVEL | READ | U16 | 0 | 10000 | 0 | Yes |  |  |  |  |
| 79 | CO2 LEVEL | WRITE | U16 | 0 | 10000 | 0 | No |  |  |  |  |
| 80 | SUPPLY INLET TEMPERATURE | READ | F8.8 | 0 | 127 | 0,00 | Yes |  |  |  |  |
| 81 | SUPPLY OUTLET TEMPERATURE | READ | F8.8 | 0 | 127 | 0,00 | Yes |  |  |  |  |
| 82 | EXHAUST INLET TEMPERATURE | READ | F8.8 | 0 | 127 | 0,00 | Yes |  |  |  |  |
| 83 | EXHAUST OUTLET TEMPERATURE | READ | F8.8 | 0 | 127 | 0,00 | Yes |  |  |  |  |
| 84 | ACTUAL EXHAUST FAN SPEED | READ | U16 | 0 | 10000 | 0 | Yes |  |  |  |  |
| 85 | ACTUAL INLET FAN SPEED | READ | U16 | 0 | 10000 | 0 | Yes |  |  |  |  |
| 86 | REMOTE PARAMETER SETTINGS V/H | READ | FLAG | 00000000 | FLAG | 00000000 | Yes |  |  |  |  |
| 87 | NOMINAL VENTILATION VALUE | READ | U8 | 0 | 255 | 0 | Yes |  |  |  |  |
| 87 | NOMINAL VENTILATION VALUE | WRITE | U8 | 0 | 255 | 0 | No |  |  |  |  |
| 88 | TSP NUMBER V/H | READ | U8 | 0 | 255 | 0 | U8 | 0 | 0 | 0 | Yes |
| 89 | TSP ENTRY V/H | READ | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | Yes |
| 89 | TSP ENTRY V/H | WRITE | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | No |
| 90 | FAULT BUFFER SIZE V/H | READ | U8 | 0 | 255 | 0 | U8 | 0 | 0 | 0 | Yes |
| 91 | FAULT BUFFER ENTRY V/H | READ | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | Yes |
| 115 | OEM DIAGNOSTIC CODE | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 116 | BURNER STARTS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 116 | BURNER STARTS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 117 | CH PUMP STARTS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 117 | CH PUMP STARTS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 118 | DHW PUMP/VALVE STARTS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 118 | DHW PUMP/VALVE STARTS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 119 | DHW BURNER STARTS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 119 | DHW BURNER STARTS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 120 | BURNER OPERATION HOURS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 120 | BURNER OPERATION HOURS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 121 | CH PUMP OPERATION HOURS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 121 | CH PUMP OPERATION HOURS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 122 | DHW PUMP/VALVE OPERATION HOURS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 122 | DHW PUMP/VALVE OPERATION HOURS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 123 | DHW BURNER HOURS | READ | U16 | 0 | 65000 | 0 | Yes |  |  |  |  |
| 123 | DHW BURNER HOURS | WRITE | U16 | 0 | 65000 | 0 | No |  |  |  |  |
| 124 | OPENTHERM VERSION MASTER | WRITE | F8.8 | 0 | 127 | 0,00 | Yes |  |  |  |  |
| 125 | OPENTHERM VERSION SLAVE | READ | F8.8 | 0 | 127 | 0,00 | Yes |  |  |  |  |
| 126 | MASTER VERSION & TYPE | WRITE | U8 | 0 | 255 | 0 | U8 | 0 | 255 | 0 | Yes |
| 127 | SLAVE VERSION & TYPE | READ | U8 | 0 | 255 | 1 | U8 | 0 | 255 | 0 | Yes |

## OT-berichten en velden (uit `OT-specification2.3b.txt`)

- Bron: https://www.opentherm.eu/request-details/?post_ids=1833
- Information from a Monitor OpenTherm for Mbus
- ch: info
- ch: service/diagnostics
- cooling: info
- cooling: service/diagnostics
- dhw: info
- dhw: service/diagnostics
- general: info
- general: service/diagnostics
- solar: info
- solar: service/diagnostics
- ventilation: info
- ventilation: service/diagnostics

### OT messages supported
- ID0:HB0: Master status: CH enable
- ID0:HB1: Master status: DHW enable
- ID0:HB2: Master status: Cooling enable
- ID0:HB3: Master status: OTC active
- ID0:HB4: Master status: CH2 enable
- ID0:HB5: Master status: Summer/winter mode
- ID0:HB6: Master status: DHW blocking
- ID0:LB0: Slave Status: Fault indication
- ID0:LB1: Slave Status: CH mode
- ID0:LB2: Slave Status: DHW mode
- ID0:LB3: Slave Status: Flame status
- ID0:LB4: Slave Status: Cooling status
- ID0:LB5: Slave Status: CH2 mode
- ID0:LB6: Slave Status: Diagnostic/service indication
- ID0:LB7: Slave Status: Electricity production
- ID1: Control Setpoint i.e. CH water temperature Setpoint (°C)
- ID10: Number of Transparent-Slave-Parameters supported by slave
- ID100: Function of manual and program changes in master and remote room Setpoint
- ID101:HB012: Master Solar Storage: Solar mode
- ID101:LB0: Slave Solar Storage: Fault indication
- ID101:LB123: Slave Solar Storage: Solar mode status
- ID101:LB45: Slave Solar Storage: Solar status
- ID102: Application-specific fault flags and OEM fault code Solar Storage
- ID103:HB0: Slave Configuration Solar Storage: System type
- ID103:LB: Slave MemberID Code Solar Storage
- ID104: Solar Storage product version number and type
- ID105: Number of Transparent-Slave-Parameters supported by TSP’s Solar Storage
- ID106: Index number / Value of referred-to transparent TSP’s Solar Storage parameter
- ID107: Size of Fault-History-Buffer supported by Solar Storage
- ID108: Index number / Value of referred-to fault-history buffer entry Solar Storage
- ID109: Electricity producer starts
- ID11: Index number / Value of referred-to transparent slave parameter
- ID110: Electricity producer hours
- ID111: Electricity production
- ID112: Cumulative Electricity production
- ID113: Number of un-successful burner starts
- ID114: Number of times flame signal was too low
- ID115: OEM-specific diagnostic/service code
- ID116: Number of successful starts burner
- ID117: Number of starts CH pump
- ID118: Number of starts DHW pump/valve
- ID119: Number of starts burner during DHW mode
- ID12: Size of Fault-History-Buffer supported by slave
- ID120: Number of hours that burner is in operation (i.e. flame on)
- ID121: Number of hours that CH pump has been running
- ID122: Number of hours that DHW pump has been running or DHW valve has been opened
- ID123: Number of hours that burner is in operation during DHW mode
- ID124: The implemented version of the OpenTherm Protocol Specification in the master
- ID125: The implemented version of the OpenTherm Protocol Specification in the slave
- ID126: Master product version number and type
- ID127: Slave product version number and type
- ID13: Index number / Value of referred-to fault-history buffer entry
- ID14: Maximum relative modulation level setting (%)
- ID15: Maximum boiler capacity (kW) / Minimum boiler modulation level(%)
- ID16: Room Setpoint (°C)
- ID17: Relative Modulation Level (%)
- ID18: Water pressure in CH circuit (bar)
- ID19: Water flow rate in DHW circuit. (litres/minute)
- ID2:HB0: Master configuration: Smart power
- ID2:LB: Master MemberID Code
- ID20: Day of Week and Time of Day
- ID21: Calendar date
- ID22: Calendar year
- ID23: Room Setpoint for 2nd CH circuit (°C)
- ID24: Room temperature (°C)
- ID25: Boiler flow water temperature (°C)
- ID26: DHW temperature (°C)
- ID27: Outside temperature (°C)
- ID28: Return water temperature (°C)
- ID29: Solar storage temperature (°C)
- ID3:HB0: Slave configuration: DHW present
- ID3:HB1: Slave configuration: Control type
- ID3:HB2: Slave configuration: Cooling configuration
- ID3:HB3: Slave configuration: DHW configuration
- ID3:HB4: Slave configuration: Master low-off&pump control
- ID3:HB5: Slave configuration: CH2 present
- ID3:HB6: Slave configuration: Remote water filling function
- ID3:HB7: Heat/cool mode control
- ID3:LB: Slave MemberID Code
- ID30: Solar collector temperature (°C)
- ID31: Flow water temperature CH2 circuit (°C)
- ID32: Domestic hot water temperature 2 (°C)
- ID33: Boiler exhaust temperature (°C)
- ID34: Boiler heat exchanger temperature (°C)
- ID35: Boiler fan speed Setpoint and actual value
- ID36: Electrical current through burner flame [µA]
- ID37: Room temperature for 2nd CH circuit (°C)
- ID38: Relative Humidity
- ID4 (HB=1): Remote Request Boiler Lockout-reset
- ID4 (HB=10): Remote Request Service request reset
- ID4 (HB=2): Remote Request Water filling
- ID48: DHW Setpoint upper & lower bounds for adjustment (°C)
- ID49: Max CH water Setpoint upper & lower bounds for adjustment (°C)
- ID5:HB0: Service request
- ID5:HB1: Lockout-reset
- ID5:HB2: Low water pressure
- ID5:HB3: Gas/flame fault
- ID5:HB4: Air pressure fault
- ID5:HB5: Water over-temperature
- ID5:LB: OEM fault code
- ID56: DHW Setpoint (°C) (Remote parameter 1)
- ID57: Max CH water Setpoint (°C) (Remote parameters 2)
- ID6:HB0: Remote boiler parameter transfer-enable: DHW setpoint
- ID6:HB1: Remote boiler parameter transfer-enable: max. CH setpoint
- ID6:LB0: Remote boiler parameter read/write: DHW setpoint
- ID6:LB1: Remote boiler parameter read/write: max. CH setpoint
- ID7: Cooling control signal (%)
- ID70:HB0: Master status ventilation / heat-recovery: Ventilation enable
- ID70:HB1: Master status ventilation / heat-recovery: Bypass position
- ID70:HB2: Master status ventilation / heat-recovery: Bypass mode
- ID70:HB3: Master status ventilation / heat-recovery: Free ventilation mode
- ID70:LB0: Slave status ventilation / heat-recovery: Fault indication
- ID70:LB1: Slave status ventilation / heat-recovery: Ventilation mode
- ID70:LB2: Slave status ventilation / heat-recovery: Bypass status
- ID70:LB3: Slave status ventilation / heat-recovery: Bypass automatic status
- ID70:LB4: Slave status ventilation / heat-recovery: Free ventilation status
- ID70:LB6: Slave status ventilation / heat-recovery: Diagnostic indication
- ID71: Relative ventilation position (0-100%). 0% is the minimum set ventilation and 100% is the maximum set ventilation
- ID72: Application-specific fault flags and OEM fault code ventilation / heat-recovery
- ID73: An OEM-specific diagnostic/service code for ventilation / heat-recovery system
- ID74:HB0: Slave Configuration ventilation / heat-recovery: System type
- ID74:HB1: Slave Configuration ventilation / heat-recovery: Bypass
- ID74:HB2: Slave Configuration ventilation / heat-recovery: Speed control
- ID74:LB: Slave MemberID Code ventilation / heat-recovery
- ID75: The implemented version of the OpenTherm Protocol Specification in the ventilation / heat-recovery system
- ID76: Ventilation / heat-recovery product version number and type
- ID77: Relative ventilation (0-100%)
- ID78: Relative humidity exhaust air (0-100%)
- ID79: CO2 level exhaust air (0-2000 ppm)
- ID8: Control Setpoint for 2e CH circuit (°C)
- ID80: Supply inlet temperature (°C)
- ID81: Supply outlet temperature (°C)
- ID82: Exhaust inlet temperature (°C)
- ID83: Exhaust outlet temperature (°C)
- ID84: Exhaust fan speed in rpm
- ID85: Supply fan speed in rpm
- ID86:HB0: Remote ventilation / heat-recovery parameter transfer-enable: Nominal ventilation value
- ID86:LB0: Remote ventilation / heat-recovery parameter read/write : Nominal ventilation value
- ID87: Nominal relative value for ventilation (0-100 %)
- ID88: Number of Transparent-Slave-Parameters supported by TSP’s ventilation / heat-recovery
- ID89: Index number / Value of referred-to transparent TSP’s ventilation / heat-recovery parameter
- ID9: Remote override room Setpoint
- ID90: Size of Fault-History-Buffer supported by ventilation / heat-recovery
- ID91: Index number / Value of referred-to fault-history buffer entry ventilation / heat-recovery
- ID98: For a specific RF sensor the RF strength and battery level is written
- ID99: Operating Mode HC1, HC2/ Operating Mode DHW
- ID0:HB5: Master status: Summer/winter mode
- ID0:HB6: Master status: DHW blocking
- ID0:LB7: Slave Status: Electricity production
- ID2:HB0: Master configuration: Smart power
- ID3:HB6: Slave configuration: Remote water filling function
- ID3:HB7: Heat/cool mode control
- ID4 (HB=1): Remote Request Boiler Lockout-reset
- ID4 (HB=2): Remote Request Water filling
- ID4 (HB=10): Remote Request Service request reset
- ID70:HB0: Master status ventilation / heat-recovery: Ventilation enable
- ID70:HB1: Master status ventilation / heat-recovery: Bypass position
- ID70:HB2: Master status ventilation / heat-recovery: Bypass mode
- ID70:HB3: Master status ventilation / heat-recovery: Free ventilation mode
- ID70:LB0: Slave status ventilation / heat-recovery: Fault indication
- ID70:LB1: Slave status ventilation / heat-recovery: Ventilation mode
- ID70:LB2: Slave status ventilation / heat-recovery: Bypass status
- ID70:LB3: Slave status ventilation / heat-recovery: Bypass automatic status
- ID70:LB4: Slave status ventilation / heat-recovery: Free ventilation status
- ID70:LB6: Slave status ventilation / heat-recovery: Diagnostic indication
- ID74:HB0: Slave Configuration ventilation / heat-recovery: System type
- ID74:HB1: Slave Configuration ventilation / heat-recovery: Bypass
- ID74:HB2: Slave Configuration ventilation / heat-recovery: Speed control
- ID86:HB0: Remote ventilation / heat-recovery parameter transfer-enable: Nominal ventilation value
- ID86:LB0: Remote ventilation / heat-recovery parameter read/write : Nominal ventilation value
- ID101:HB012: Master Solar Storage: Solar mode
- ID101:LB0: Slave Solar Storage: Fault indication
- ID101:LB123: Slave Solar Storage: Solar mode status
- ID101:LB45: Slave Solar Storage: Solar status
- ID103:HB0: Slave Configuration Solar Storage: System type

## Ondersteuning / TODO-markeringen (uit `OT-specification2.3b-todo.txt`)

Legenda: `X` = ondersteund, `*` = aandachtspunt/TODO.

- Bron: https://www.opentherm.eu/request-details/?post_ids=1833
- Information from a Monitor OpenTherm for Mbus
- ch: info
- ch: service/diagnostics
- cooling: info
- cooling: service/diagnostics
- dhw: info
- dhw: service/diagnostics
- general: info
- general: service/diagnostics
- solar: info
- solar: service/diagnostics
- ventilation: info
- ventilation: service/diagnostics
- OT messages supported
- ✅ ID0:HB0: Master status: CH enable
- ✅ ID0:HB1: Master status: DHW enable
- ✅ ID0:HB2: Master status: Cooling enable
- ✅ ID0:HB3: Master status: OTC active
- ✅ ID0:HB4: Master status: CH2 enable
- ⚠️ ID0:HB5: Master status: Summer/winter mode
- ⚠️ ID0:HB6: Master status: DHW blocking
- ✅ ID0:LB0: Slave Status: Fault indication
- ✅ ID0:LB1: Slave Status: CH mode
- ✅ ID0:LB2: Slave Status: DHW mode
- ✅ ID0:LB3: Slave Status: Flame status
- ✅ ID0:LB4: Slave Status: Cooling status
- ✅ ID0:LB5: Slave Status: CH2 mode
- ⚠️ ID0:LB6: Slave Status: Diagnostic/service indication
- ⚠️ ID0:LB7: Slave Status: Electricity production
- ✅ ID1: Control Setpoint i.e. CH water temperature Setpoint (°C)
- ✅ ID10: Number of Transparent-Slave-Parameters supported by slave
- ✅ ID100: Function of manual and program changes in master and remote room Setpoint
- ⚠️ ID101:HB012: Master Solar Storage: Solar mode
- ⚠️ ID101:LB0: Slave Solar Storage: Fault indication
- ⚠️ ID101:LB123: Slave Solar Storage: Solar mode status
- ⚠️ ID101:LB45: Slave Solar Storage: Solar status
- ⚠️ ID102: Application-specific fault flags and OEM fault code Solar Storage
- ⚠️ ID103:HB0: Slave Configuration Solar Storage: System type
- ⚠️ ID103:LB: Slave MemberID Code Solar Storage
- ⚠️ ID104: Solar Storage product version number and type
- ⚠️ ID105: Number of Transparent-Slave-Parameters supported by TSP’s Solar Storage
- ⚠️ ID106: Index number / Value of referred-to transparent TSP’s Solar Storage parameter
- ⚠️ ID107: Size of Fault-History-Buffer supported by Solar Storage
- ⚠️ ID108: Index number / Value of referred-to fault-history buffer entry Solar Storage
- ⚠️ ID109: Electricity producer starts
- ✅ ID11: Index number / Value of referred-to transparent slave parameter
- ✅ ID110: Electricity producer hours
- ✅ ID111: Electricity production
- ✅ ID112: Cumulative Electricity production
- ⚠️ ID113: Number of un-successful burner starts
- ⚠️ ID114: Number of times flame signal was too low
- ✅ ID115: OEM-specific diagnostic/service code
- ✅ ID116: Number of successful starts burner
- ✅ ID117: Number of starts CH pump
- ✅ ID118: Number of starts DHW pump/valve
- ✅ ID119: Number of starts burner during DHW mode
- ✅ ID12: Size of Fault-History-Buffer supported by slave
- ✅ ID120: Number of hours that burner is in operation (i.e. flame on)
- ✅ ID121: Number of hours that CH pump has been running
- ✅ ID122: Number of hours that DHW pump has been running or DHW valve has been opened
- ✅ ID123: Number of hours that burner is in operation during DHW mode
- ✅ ID124: The implemented version of the OpenTherm Protocol Specification in the master
- ✅ ID125: The implemented version of the OpenTherm Protocol Specification in the slave
- ✅ ID126: Master product version number and type
- ✅ ID127: Slave product version number and type
- ✅ ID13: Index number / Value of referred-to fault-history buffer entry
- ✅ ID14: Maximum relative modulation level setting (%)
- ✅ ID15: Maximum boiler capacity (kW) / Minimum boiler modulation level(%)
- ✅ ID16: Room Setpoint (°C)
- ✅ ID17: Relative Modulation Level (%)
- ✅ ID18: Water pressure in CH circuit (bar)
- ✅ ID19: Water flow rate in DHW circuit. (litres/minute)
- ⚠️ ID2:HB0: Master configuration: Smart power
- ✅ ID2:LB: Master MemberID Code
- ✅ ID20: Day of Week and Time of Day
- ✅ ID21: Calendar date
- ✅ ID22: Calendar year
- ✅ ID23: Room Setpoint for 2nd CH circuit (°C)
- ✅ ID24: Room temperature (°C)
- ✅ ID25: Boiler flow water temperature (°C)
- ✅ ID26: DHW temperature (°C)
- ✅ ID27: Outside temperature (°C)
- ✅ ID28: Return water temperature (°C)
- ✅ ID29: Solar storage temperature (°C)
- ✅ ID3:HB0: Slave configuration: DHW present
- ✅ ID3:HB1: Slave configuration: Control type
- ✅ ID3:HB2: Slave configuration: Cooling configuration
- ✅ ID3:HB3: Slave configuration: DHW configuration
- ✅ ID3:HB4: Slave configuration: Master low-off&pump control
- ✅ ID3:HB5: Slave configuration: CH2 present
- ⚠️ ID3:HB6: Slave configuration: Remote water filling function
- ⚠️ ID3:HB7: Heat/cool mode control
- ✅ ID3:LB: Slave MemberID Code
- ✅ ID30: Solar collector temperature (°C)
- ✅ ID31: Flow water temperature CH2 circuit (°C)
- ✅ ID32: Domestic hot water temperature 2 (°C)
- ✅ ID33: Boiler exhaust temperature (°C)
- ✅ ID34: Boiler heat exchanger temperature (°C)
- ✅ ID35: Boiler fan speed Setpoint and actual value
- ✅ ID36: Electrical current through burner flame [µA]
- ✅ ID37: Room temperature for 2nd CH circuit (°C)
- ✅ ID38: Relative Humidity
- ⚠️ ID4 (HB=1): Remote Request Boiler Lockout-reset
- ⚠️ ID4 (HB=10): Remote Request Service request reset
- ⚠️ ID4 (HB=2): Remote Request Water filling
- ✅ ID48: DHW Setpoint upper & lower bounds for adjustment (°C)
- ✅ ID49: Max CH water Setpoint upper & lower bounds for adjustment (°C)
- ✅ ID5:HB0: Service request
- ✅ ID5:HB1: Lockout-reset
- ✅ ID5:HB2: Low water pressure
- ✅ ID5:HB3: Gas/flame fault
- ✅ ID5:HB4: Air pressure fault
- ✅ ID5:HB5: Water over-temperature
- ✅ ID5:LB: OEM fault code
- ✅ ID56: DHW Setpoint (°C) (Remote parameter 1)
- ✅ ID57: Max CH water Setpoint (°C) (Remote parameters 2)
- ⚠️ ID6:HB0: Remote boiler parameter transfer-enable: DHW setpoint
- ⚠️ ID6:HB1: Remote boiler parameter transfer-enable: max. CH setpoint
- ⚠️ ID6:LB0: Remote boiler parameter read/write: DHW setpoint
- ⚠️ ID6:LB1: Remote boiler parameter read/write: max. CH setpoint
- ✅ ID7: Cooling control signal (%)
- ⚠️ ID70:HB0: Master status ventilation / heat-recovery: Ventilation enable
- ⚠️ ID70:HB1: Master status ventilation / heat-recovery: Bypass position
- ⚠️ ID70:HB2: Master status ventilation / heat-recovery: Bypass mode
- ⚠️ ID70:HB3: Master status ventilation / heat-recovery: Free ventilation mode
- ⚠️ ID70:LB0: Slave status ventilation / heat-recovery: Fault indication
- ⚠️ ID70:LB1: Slave status ventilation / heat-recovery: Ventilation mode
- ⚠️ ID70:LB2: Slave status ventilation / heat-recovery: Bypass status
- ⚠️ ID70:LB3: Slave status ventilation / heat-recovery: Bypass automatic status
- ⚠️ ID70:LB4: Slave status ventilation / heat-recovery: Free ventilation status
- ⚠️ ID70:LB6: Slave status ventilation / heat-recovery: Diagnostic indication
- ✅ ID71: Relative ventilation position (0-100%). 0% is the minimum set ventilation and 100% is the maximum set ventilation
- ✅ ID72: Application-specific fault flags and OEM fault code ventilation / heat-recovery
- ✅ ID73: An OEM-specific diagnostic/service code for ventilation / heat-recovery system
- ⚠️ ID74:HB0: Slave Configuration ventilation / heat-recovery: System type
- ⚠️ ID74:HB1: Slave Configuration ventilation / heat-recovery: Bypass
- ⚠️ ID74:HB2: Slave Configuration ventilation / heat-recovery: Speed control
- ⚠️ ID74:LB: Slave MemberID Code ventilation / heat-recovery
- ✅ ID75: The implemented version of the OpenTherm Protocol Specification in the ventilation / heat-recovery system
- ✅ ID76: Ventilation / heat-recovery product version number and type
- ✅ ID77: Relative ventilation (0-100%)
- ✅ ID78: Relative humidity exhaust air (0-100%)
- ✅ ID79: CO2 level exhaust air (0-2000 ppm)
- ✅ ID8: Control Setpoint for 2e CH circuit (°C)
- ✅ ID80: Supply inlet temperature (°C)
- ✅ ID81: Supply outlet temperature (°C)
- ✅ ID82: Exhaust inlet temperature (°C)
- ✅ ID83: Exhaust outlet temperature (°C)
- ✅ ID84: Exhaust fan speed in rpm
- ⚠️ ID85: Supply fan speed in rpm
- ⚠️ ID86:HB0: Remote ventilation / heat-recovery parameter transfer-enable: Nominal ventilation value
- ⚠️ ID86:LB0: Remote ventilation / heat-recovery parameter read/write : Nominal ventilation value
- ✅ ID87: Nominal relative value for ventilation (0-100 %)
- ✅ ID88: Number of Transparent-Slave-Parameters supported by TSP’s ventilation / heat-recovery
- ✅ ID89: Index number / Value of referred-to transparent TSP’s ventilation / heat-recovery parameter
- ✅ ID9: Remote override room Setpoint
- ✅ ID90: Size of Fault-History-Buffer supported by ventilation / heat-recovery
- ✅ ID91: Index number / Value of referred-to fault-history buffer entry ventilation / heat-recovery
- ✅ ID98: For a specific RF sensor the RF strength and battery level is written
- ✅ ID99: Operating Mode HC1, HC2/ Operating Mode DHW

## Nieuwe OT Data-ID's (uit `New OT data-ids.txt`)

- Bron: https://www.domoticaforum.eu/viewtopic.php?f=70&t=10893
- Bron: http://www.opentherm.eu/product/view/18/feeling-d201-ot
- ID 98: For a specific RF sensor the RF strength and battery level is written
- ID 99: Operating Mode HC1, HC2/ Operating Mode DHW
- Bron: https://www.opentherm.eu/request-details/?post_ids=1833
- ID 109: Electricity producer starts
- ID 110: Electricity producer hours
- ID 111: Electricity production
- ID 112: Cumulative Electricity production
- Bron: https://www.opentherm.eu/request-details/?post_ids=1833
- New OT (Remeha) Data-ID's
- ID 36:   {f8.8}   "Electrical current through burner flame" (µA)
- ID 37:   {f8.8}   "Room temperature for 2nd CH circuit"
- ID 38:   {u8 u8}   "Relative Humidity"
- OT Remeha qSense <-> Remeha Tzerra communication
- ID 131:   {u8 u8}   "Remeha dF-/dU-codes"
- ID 132:   {u8 u8}   "Remeha Servicemessage"
- ID 133:   {u8 u8}   "Remeha detection connected SCU’s"
- "Remeha dF-/dU-codes": Should match the dF-/dU-codes written on boiler nameplate. Read-Data Request (0 0) returns the data. Also accepts Write-Data Requests (dF dU), this returns the boiler to its factory defaults.
- "Remeha Servicemessage" Read-Data Request (0 0), boiler returns (0 2) in case of no boiler service. Write-Data Request (1 255) clears the boiler service message.
- boiler returns (1 1) = next service type is "A"
- boiler returns (1 2) = next service type is "B"
- boiler returns (1 3) = next service type is "C"
- "Remeha detection connected SCU’s": Write-Data Request (255 1) enables detection of connected SCU prints, correct response is (Write-Ack 255 1).
- Other Remeha info:
- Data-ID 5: corresponds with the Remeha E:xx fault codes.
- Data-ID 11: corresponds with the Remeha Pxx parameter codes.
- Data-ID 35: reported value is fan speed in rpm/60 .
- Data-ID 115: corresponds with the Remeha Status and Sub-status numbers using {u8 u8} data-type.

## Protocolversie-informatie (uit `OT protocol version information.txt`)

- According to this document from 2016 the OpenTherm Association is testing OT version 5.
- Bron: https://www.opentherm.eu/wp-content/uploads/2016/03/OpenTherm-workshop-at-the-Mostra-Convegno-20161.pdf
- Version history
- 20 years of protocol development:
- –first official release : 1996-1997
- –Version 2.0 : 2000
- –Version 2.2 : 2003 (february 7)
- –Version 3.0: 2006 (Smart power)
- –Version 4.0: 2011 (may 12)
- Version 5.0: Currently being tested
- • Larger datapackages
- • Addressing
- • Slave-master communication
- According to the document there are (or will be) 7 mandatory ID's.

## Home Assistant integratie-notities (uit `Integration homeassistant.txt`)

- Bron: https://github.com/martenjacobs/py-otgw-mqtt
- value/otgw => The status of the service
- value/otgw/flame_status
- value/otgw/flame_status_ch
- value/otgw/flame_status_dhw
- value/otgw/flame_status_bit
- value/otgw/control_setpoint
- value/otgw/remote_override_setpoint
- value/otgw/max_relative_modulation_level
- value/otgw/room_setpoint
- value/otgw/relative_modulation_level
- value/otgw/ch_water_pressure
- value/otgw/room_temperature
- value/otgw/boiler_water_temperature
- value/otgw/dhw_temperature
- value/otgw/outside_temperature
- value/otgw/return_water_temperature
- value/otgw/dhw_setpoint
- value/otgw/max_ch_water_setpoint
- value/otgw/burner_starts
- value/otgw/ch_pump_starts
- value/otgw/dhw_pump_starts
- value/otgw/dhw_burner_starts
- value/otgw/burner_operation_hours
- value/otgw/ch_pump_operation_hours
- value/otgw/dhw_pump_valve_operation_hours
- value/otgw/dhw_burner_operation_hours
- Subscription topics
- By default, the service listens to messages from the following MQTT topics:
- set/otgw/room_setpoint/temporary
- set/otgw/room_setpoint/constant
- set/otgw/outside_temperature
- set/otgw/hot_water/enable
- set/otgw/hot_water/temperature
- set/otgw/central_heating/enable
