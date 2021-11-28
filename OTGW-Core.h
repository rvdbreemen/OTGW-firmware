/*
***************************************************************************  
**  Program  : Header file: OTGW-Core.h 
**  Version  : v0.9.0
**
**  Copyright (c) 2021 Robert van den Breemen
**  Borrowed from OpenTherm library from: 
**      https://github.com/jpraus/arduino-opentherm
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************       
*/  

#ifndef OTGWCore_h
#define OTGWCore_h

// OTGW Serial 2 network port  
// #include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#define OTGW_SERIAL_PORT 25238     // changed the port to original default of OTmonitor 
TelnetStreamClass OTGWstream(OTGW_SERIAL_PORT); 

//Depends on the library 
#define OTGW_COMMAND_TOPIC "command"

typedef struct {
	uint16_t 	Statusflags = 0; // flag8 / flag8  Master and Slave Status flags. 
	uint8_t 	MasterStatus = 0; 
	uint8_t 	SlaveStatus = 0;
	float 		TSet = 0.0; // f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
	uint16_t	MasterConfigMemberIDcode = 0; 	// flag8 / u8  Master Configuration Flags /  Master MemberID Code 
	uint16_t	SlaveConfigMemberIDcode = 0; // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code 
	uint16_t 	Command = 0; // u8 / u8  Remote Command 
	uint16_t 	ASFflags = 0; // / OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code 
	uint16_t 	RBPflags = 0; // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags 
	float 		CoolingControl = 0.0 ; // f8.8  Cooling control signal (%) 
	float 		TsetCH2 = 0.0 ; // f8.8  Control setpoint for 2e CH circuit (°C)
	float 		TrOverride = 0.0 ; // f8.8  Remote override room setpoint 
	uint16_t 	TSP = 0; // u8 / u8  Number of Transparent-Slave-Parameters supported by slave 
	uint16_t 	TSPindexTSPvalue = 0; // u8 / u8  Index number / Value of referred-to transparent slave parameter. 
	uint16_t 	FHBsize = 0; // u8 / u8  Size of Fault-History-Buffer supported by slave 
	uint16_t 	FHBindexFHBvalue = 0; // u8 / u8  Index number / Value of referred-to fault-history buffer entry. 
	float 		MaxRelModLevelSetting = 0.0 ; // f8.8  Maximum relative modulation level setting (%) 
	uint16_t 	MaxCapacityMinModLevel = 0; // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%) 
	float 		TrSet = 0.0 ; // f8.8  Room Setpoint (°C)
	float 		RelModLevel = 0.0 ; // f8.8  Relative Modulation Level (%) 
	float 		CHPressure = 0.0 ; // f8.8  Water pressure in CH circuit  (bar) 
	float 		DHWFlowRate = 0.0 ; // f8.8  Water flow rate in DHW circuit. (litres/minute) 
	uint16_t 	DayTime = 0; // special / u8  Day of Week and Time of Day 
	uint16_t 	Date = 0; // u8 / u8  Calendar date 
	uint16_t 	Year = 0; // u16  Calendar year 
	float 		TrSetCH2 = 0.0 ; // f8.8  Room Setpoint for 2nd CH circuit (°C)
	float 		Tr = 0.0 ; // f8.8  Room temperature (°C)
	float 		Tboiler = 0.0 ; // f8.8  Boiler flow water temperature (°C)
	float 		Tdhw = 0.0 ; // f8.8  DHW temperature (°C)
	float 		Toutside = 0.0 ; // f8.8  Outside temperature (°C)
	float 		Tret = 0.0 ; // f8.8  Return water temperature (°C)
	float 		Tsolarstorage = 0.0 ; // f8.8  Solar storage temperature (°C)
	int16_t 	Tsolarcollector = 0.0 ; // s16  Solar collector temperature (°C)
	float 		TflowCH2 = 0.0 ; // f8.8  Flow water temperature CH2 circuit (°C)
	float 		Tdhw2 = 0.0 ; // f8.8  Domestic hot water temperature 2 (°C)
	int16_t 	Texhaust = 0; // s16  Boiler exhaust temperature (°C)
	uint16_t	FanSpeed = 0; // u16  Fan Speed (rpm)
	float 		ElectricalCurrentBurnerFlame = 0.0; // f88 Electrical current through burner flame (µA)
	float 		TRoomCH2= 0.0; // f88  Room Temperature for 2nd CH circuit ("°C)
	uint16_t	RelativeHumidity = 0; // u8 / u8 Relative Humidity (%)
	uint16_t 	TdhwSetUBTdhwSetLB = 0 ; // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
	uint16_t 	MaxTSetUBMaxTSetLB = 0; // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
	uint16_t	HcratioUBHcratioLB = 0; // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
	float 		TdhwSet = 0.0 ; // f8.8  DHW setpoint (°C)    (Remote parameter 1)
	float 		MaxTSet = 0.0 ; // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
	float 		Hcratio = 0.0 ; // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)

	//RF
	uint16_t	RFstrengthbatterylevel = 0; // u8/ u8 RF strength and battery level
	uint16_t 	OperatingMode_HC1_HC2_DHW = 0; // u8 / u8 Operating Mode HC1, HC2/ DHW
	uint16_t	RoomRemoteOverrideFunction = 0; // Function of manual and program changes in master and remote room setpoint

	//Electric Producer
	uint16_t 	ElectricityProducerStarts = 0; // u16 Electricity producer starts 
	uint16_t 	ElectricityProducerHours = 0; // u16 Electricity producer hours
	uint16_t 	ElectricityProduction = 0; // u16 Electricity production
	uint16_t 	CumulativElectricityProduction = 0; // u16 Cumulativ Electricity production
	
	//Solar Storage
	uint16_t 	SolarStorageStatus = 0;
	uint8_t 	SolarMasterStatus = 0;
	uint8_t 	SolarSlaveStatus = 0;	
	uint16_t	SolarStorageASFflags = 0;
	uint16_t	SolarStorageSlaveConfigMemberIDcode = 0;
	uint16_t	SolarStorageVersionType = 0;
	uint16_t 	SolarStorageTSP = 0;
	uint16_t	SolarStorageTSPindexTSPvalue = 0;
	uint16_t	SolarStorageFHBsize = 0;
	uint16_t	SolarStorageFHBindexFHBvalue = 0;

	//Ventilation/HeatRecovery Msgids
	uint16_t	StatusVH = 0;
	uint8_t 	MasterStatusVH = 0;
	uint8_t 	SlaveStatusVH = 0;
	uint16_t	ControlSetpointVH = 0;  //should be uint8_t
	uint16_t	ASFFaultCodeVH = 0;
	uint16_t	DiagnosticCodeVH = 0;
	uint16_t	ConfigMemberIDVH = 0;
	float		OpenthermVersionVH = 0.0;
	uint16_t	VersionTypeVH = 0;
	uint16_t	RelativeVentilation = 0;
	uint16_t	RelativeHumidityExhaustAir = 0;
	uint16_t	CO2LevelExhaustAir = 0;
	float		SupplyInletTemperature = 0.0;
	float		SupplyOutletTemperature = 0.0;
	float		ExhaustInletTemperature = 0.0;
	float		ExhaustOutletTemperature = 0.0;
	uint16_t	ActualExhaustFanSpeed = 0;
	uint16_t 	ActualSupplyFanSpeed = 0;
	uint16_t	RemoteParameterSettingVH = 0;
	uint16_t	NominalVentilationValue = 0;
	uint16_t	TSPNumberVH = 0;
	uint16_t	TSPEntryVH = 0;
	uint16_t	FaultBufferSizeVH = 0;
	uint16_t	FaultBufferEntryVH = 0;

	//Statitics
	uint16_t 	BurnerUnsuccessfulStarts = 0;
	uint16_t	FlameSignalTooLow = 0;
	uint16_t 	RemoteOverrideFunction = 0; // flag8 / -  Function of manual and program changes in master and remote room setpoint. 
	uint16_t 	OEMDiagnosticCode = 0; // u16  OEM-specific diagnostic/service code 
	uint16_t 	BurnerStarts = 0; // u16  Number of starts burner 
	uint16_t 	CHPumpStarts = 0; // u16  Number of starts CH pump 
	uint16_t 	DHWPumpValveStarts = 0; // u16  Number of starts DHW pump/valve 
	uint16_t 	DHWBurnerStarts = 0; // u16  Number of starts burner during DHW mode 
	uint16_t 	BurnerOperationHours = 0; // u16  Number of hours that burner is in operation (i.e. flame on) 
	uint16_t 	CHPumpOperationHours = 0; // u16  Number of hours that CH pump has been running 
	uint16_t 	DHWPumpValveOperationHours = 0; // u16  Number of hours that DHW pump has been running or DHW valve has been opened 
	uint16_t 	DHWBurnerOperationHours = 0; // u16  Number of hours that burner is in operation during DHW mode 
	float 		OpenThermVersionMaster = 0.0 ; // f8.8  The implemented version of the OpenTherm Protocol Specification in the master. 
	float 		OpenThermVersionSlave = 0.0 ; // f8.8  The implemented version of the OpenTherm Protocol Specification in the slave. 
	uint16_t 	MasterVersion = 0; // u8 / u8  Master product version number and type 
	uint16_t 	SlaveVersion = 0; // u8 / u8  Slave product version number and type

	//Rehmea
	uint16_t	RemehadFdUcodes = 0; // u16 Remeha dF-/dU-codes
	uint16_t 	RemehaServicemessage = 0; // u16 Remeha Servicemessage
	uint16_t    RemehaDetectionConnectedSCU =0; // u16 Remeha detection connected SCU’s

	//errors
	uint16_t	error01 = 0;
	uint16_t	error02 = 0;
	uint16_t	error03 = 0;
	uint16_t	error04 = 0;

} OTdataStruct;

static OTdataStruct OTdataObject;   


enum OpenThermResponseStatus {
	OT_NONE,
	OT_SUCCESS,
	OT_INVALID,
	OT_TIMEOUT
};

enum OpenThermMessageType {
	/*  Master to Slave */
	OT_READ_DATA       = B000,
	OT_WRITE_DATA      = B001,
	OT_INVALID_DATA    = B010,
	OT_RESERVED        = B011,
	/* Slave to Master */
	OT_READ_ACK        = B100,
	OT_WRITE_ACK       = B101,
	OT_DATA_INVALID    = B110,
	OT_UNKNOWN_DATA_ID = B111
};

enum OpenThermMessageID {
	OT_Statusflags, // flag8 / flag8  Master and Slave Status flags. 
	OT_TSet, // f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
	OT_MasterConfigMemberIDcode, // flag8 / u8  Master Configuration Flags /  Master MemberID Code 
	OT_SlaveConfigMemberIDcode, // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code 
	OT_Command, // u8 / u8  Remote Command 
	OT_ASFflags, // / OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code 
	OT_RBPflags, // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags 
	OT_CoolingControl, // f8.8  Cooling control signal (%) 
	OT_TsetCH2, // f8.8  Control setpoint for 2e CH circuit (°C)
	OT_TrOverride, // f8.8  Remote override room setpoint 
	OT_TSP, // u8 / u8  Number of Transparent-Slave-Parameters supported by slave 
	OT_TSPindexTSPvalue, // u8 / u8  Index number / Value of referred-to transparent slave parameter. 
	OT_FHBsize, // u8 / u8  Size of Fault-History-Buffer supported by slave 
	OT_FHBindexFHBvalue, // u8 / u8  Index number / Value of referred-to fault-history buffer entry. 
	OT_MaxRelModLevelSetting, // f8.8  Maximum relative modulation level setting (%) 
	OT_MaxCapacityMinModLevel, // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%) 
	OT_TrSet, // f8.8  Room Setpoint (°C)
	OT_RelModLevel, // f8.8  Relative Modulation Level (%) 
	OT_CHPressure, // f8.8  Water pressure in CH circuit  (bar) 
	OT_DHWFlowRate, // f8.8  Water flow rate in DHW circuit. (litres/minute) 
	OT_DayTime, // special / u8  Day of Week and Time of Day 
	OT_Date, // u8 / u8  Calendar date 
	OT_Year, // u16  Calendar year 
	OT_TrSetCH2, // f8.8  Room Setpoint for 2nd CH circuit (°C)
	OT_Tr, // f8.8  Room temperature (°C)
	OT_Tboiler, // f8.8  Boiler flow water temperature (°C)
	OT_Tdhw, // f8.8  DHW temperature (°C)
	OT_Toutside, // f8.8  Outside temperature (°C)
	OT_Tret, // f8.8  Return water temperature (°C)
	OT_Tsolarstorage, // f8.8  Solar storage temperature (°C)
	OT_Tsolarcollector, // s16  Solar collector temperature (°C)
	OT_TflowCH2, // f8.8  Flow water temperature CH2 circuit (°C)
	OT_Tdhw2, // f8.8  Domestic hot water temperature 2 (°C)
	OT_Texhaust, // s16  Boiler exhaust temperature (°C)
	OT_FanSpeed = 35, // u16  Fan Speed (rpm)
	OT_ElectricalCurrentBurnerFlame, // f88 Electrical current through burner flame (µA)
	OT_TRoomCH2, // f88  Room Temperature for 2nd CH circuit ("°C)
	OT_RelativeHumidity, // u8 / u8 Relative Humidity (%)
	OT_TdhwSetUBTdhwSetLB = 48, // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
	OT_MaxTSetUBMaxTSetLB, // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
	OT_HcratioUBHcratioLB, // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
	OT_TdhwSet = 56, // f8.8  DHW setpoint (°C)    (Remote parameter 1)
	OT_MaxTSet, // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
	OT_Hcratio, // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)
	OT_StatusVH = 70, // flag8 / flag8 Status Ventilation/Heat recovery
	OT_ControlSetpointVH, // u8 Control setpoint V/H
	OT_ASFFaultCodeVH, // flag8 / u8 Aplication Specific Fault Flags/Code V/H
	OT_DiagnosticCodeVH, // u16 Diagnostic Code V/H
	OT_ConfigMemberIDVH, // flag8 / u8 Config/Member ID V/H
	OT_OpenthermVersionVH, // f8.8 OpenTherm Version V/H
	OT_VersionTypeVH,	// u8 / u8 Version & Type V/H
	OT_RelativeVentilation, // u8 Relative Ventilation (%)
	OT_RelativeHumidityExhaustAir, // u8 / u8 Relative Humidity (%)
	OT_CO2LevelExhaustAir, // u16 CO2 Level (ppm)
 	OT_SupplyInletTemperature,	// f8.8 Supply Inlet Temperature (°C)
 	OT_SupplyOutletTemperature, // f8.8 Supply Outlet Temperature(°C)
 	OT_ExhaustInletTemperature, // f8.8 Exhaust Inlet Temperature (°C)
 	OT_ExhaustOutletTemperature, // f8.8 Exhaust Outlet Temperature (°C)
	OT_ActualExhaustFanSpeed, // u16 Actual Exhaust Fan Speed (rpm)
	OT_ActualSupplyFanSpeed, // u16 Actual Supply Fan Speed (rpm) 
	OT_RemoteParameterSettingVH, // flag8 / flag8 Remote Parameter Setting V/H
	OT_NominalVentilationValue, // u8 Nominal Ventilation Value
	OT_TSPNumberVH, // u8 / u8 TSP Number V/H
	OT_TSPEntryVH,	// u8 / u8 TSP Entry V/H
	OT_FaultBufferSizeVH, // u8 / u8 Fault Buffer Size V/H
	OT_FaultBufferEntryVH,	// u8 / u8 Fault Buffer Entry V/H
	OT_RFstrengthbatterylevel=98, // u8 / u8  RF strength and battery level
	OT_OperatingMode_HC1_HC2_DHW, // u8 / u8 Operating Mode HC1, HC2/ DHW
	OT_RemoteOverrideFunction, // flag8 / -  Function of manual and program changes in master and remote room setpoint. 
	OT_SolarStorageMaster,	// flag8 / flag8  Solar Storage  Master flags.
	OT_SolarStorageASFflags, // flag8 / u8 / Solar Storage OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code 
	OT_SolarStorageSlaveConfigMemberIDcode, // flag8 / u8  Solar Storage Master Configuration Flags /  Master MemberID Code 
	OT_SolarStorageVersionType, // u8 / u8 / Solar Storage product version number and type
	OT_SolarStorageTSP,	// u8 / u8 / Solar Storage Number of Transparent-Slave-Parameters supported
	OT_SolarStorageTSPindexTSPvalue, // u8 / u8 / Solar Storage Index number / Value of referred-to transparent slave parameter
	OT_SolarStorageFHBsize, // u8 /u8 / Solar Storage Size of Fault-History-Buffer supported by slave
	OT_SolarStorageFHBindexFHBvalue, // u8 /u8 / Solar Storage Index number / Value of referred-to fault-history buffer entry
	OT_ElectricityProducerStarts, // u16 Electricity producer starts
	OT_ElectricityProducerHours, //u16 Electricity producer hours
	OT_ElectricityProduction, //u16 Electricity production
	OT_CumulativElectricityProduction, // u16 Cumulativ Electricity production
	OT_BurnerUnsuccessfulStarts, // u16 Number of Un-successful burner starts 
	OT_FlameSignalTooLow, //u16 Number of times flame signal too low
	OT_OEMDiagnosticCode, // u16  OEM-specific diagnostic/service code 
	OT_BurnerStarts, // u16  Number of starts burner 
	OT_CHPumpStarts, // u16  Number of starts CH pump 
	OT_DHWPumpValveStarts, // u16  Number of starts DHW pump/valve 
	OT_DHWBurnerStarts, // u16  Number of starts burner during DHW mode 
	OT_BurnerOperationHours, // u16  Number of hours that burner is in operation (i.e. flame on) 
	OT_CHPumpOperationHours, // u16  Number of hours that CH pump has been running 
	OT_DHWPumpValveOperationHours, // u16  Number of hours that DHW pump has been running or DHW valve has been opened 
	OT_DHWBurnerOperationHours, // u16  Number of hours that burner is in operation during DHW mode 
	OT_OpenThermVersionMaster, // f8.8  The implemented version of the OpenTherm Protocol Specification in the master. 
	OT_OpenThermVersionSlave, // f8.8  The implemented version of the OpenTherm Protocol Specification in the slave. 
	OT_MasterVersion, // u8 / u8  Master product version number and type 
	OT_SlaveVersion, // u8 / u8  Slave product version number and type
	OT_RemehadFdUcodes, // u8 / u8 Remeha dF-/dU-codes
	OT_RemehaServicemessage, // u8 / u8 Remeha Servicemessage
	OT_RemehaDetectionConnectedSCU, // u8 / u8 Remeha detection connected SCU’s
};
	enum OTtype_t { ot_f88, ot_s16, ot_s8s8, ot_u16, ot_u8u8, ot_flag8, ot_flag8flag8, ot_special, ot_flag8u8, ot_u8, ot_undef}; 
 	enum OTmsgcmd_t { OT_READ, OT_WRITE, OT_RW, OT_UNDEF };
	
	struct OTlookup_t
    {
        int id;
        OTmsgcmd_t msg;
        OTtype_t type;
        char* label;
        char* friendlyname;
        char* unit;
    };

	OTlookup_t OTlookupitem;

    const OTlookup_t OTmap[] PROGMEM = {
        {   0, OT_READ  , ot_flag8flag8, "Status", "Master and Slave status", "" },
        {   1, OT_WRITE , ot_f88,        "TSet", "Control setpoint", "°C" },
        {   2, OT_WRITE , ot_flag8u8,    "MasterConfigMemberIDcode", "Master Config / Member ID", "" },
        {   3, OT_READ  , ot_flag8u8,    "SlaveConfigMemberIDcode", "Slave Config / Member ID", "" },
        {   4, OT_RW    , ot_u8u8,       "Command", "Command-Code", "" },
		{   5, OT_READ  , ot_flag8u8,    "ASFflags", "Application-specific fault", "" },
		{   6, OT_READ  , ot_flag8u8,    "RBPflags", "Remote-parameter flags", "" },
		{   7, OT_WRITE , ot_f88,        "CoolingControl", "Cooling control signal", "%" },
		{   8, OT_WRITE , ot_f88,        "TsetCH2", "Control setpoint for 2e CH circuit", "°C" },
		{   9, OT_READ  , ot_f88,        "TrOverride", "Remote override room setpoint", "" },
		{  10, OT_READ  , ot_u8u8,       "TSP", "Number of Transparent-Slave-Parameters supported by slave", "" },
		{  11, OT_RW    , ot_u8u8,       "TSPindexTSPvalue", "Index number / Value of referred-to transparent slave parameter", "" },
		{  12, OT_READ  , ot_u8u8,       "FHBsize", "Size of Fault-History-Buffer supported by slave", "" },
		{  13, OT_READ  , ot_u8u8,       "FHBindexFHBvalue", "Index number / Value of referred-to fault-history buffer entry", "" },
		{  14, OT_WRITE , ot_f88,        "MaxRelModLevelSetting", "Maximum relative modulation level setting", "%" },
		{  15, OT_READ  , ot_u8u8,       "MaxCapacityMinModLevell", "Maximum boiler capacity (kW) / Minimum boiler modulation level(%)", "kW/%" },
		{  16, OT_WRITE , ot_f88,        "TrSet", "Room Setpoint", "°C" },
		{  17, OT_READ  , ot_f88,        "RelModLevel", "Relative Modulation Level", "%" },
		{  18, OT_READ  , ot_f88,        "CHPressure", "Water pressure in CH circuit", "bar" },
		{  19, OT_READ  , ot_f88,        "DHWFlowRate", "Water flow rate in DHW circuit", "l/m" },
		{  20, OT_RW    , ot_special,    "DayTime", "Day of Week and Time of Day", "" },
		{  21, OT_RW    , ot_u8u8,       "Date", "Calendar date ", "" },
		{  22, OT_RW    , ot_u16,        "Year", "Calendar year", "" },
		{  23, OT_WRITE , ot_f88,        "TrSetCH2", "Room Setpoint for 2nd CH circuit", "°C" },
		{  24, OT_WRITE , ot_f88,        "Tr", "Room Temperature", "°C" },
		{  25, OT_READ  , ot_f88,        "Tboiler", "Boiler flow water temperature", "°C" },
		{  26, OT_READ  , ot_f88,        "Tdhw", "DHW temperature", "°C" },
		{  27, OT_READ  , ot_f88,        "Toutside", "Outside temperature", "°C" },
		{  28, OT_READ  , ot_f88,        "Tret", "Return water temperature", "°C" },
		{  29, OT_READ  , ot_f88,        "Tsolarstorage", "Solar storage temperature", "°C" },
		{  30, OT_READ  , ot_s16,        "Tsolarcollector", "Solar collector temperature", "°C" },
		{  31, OT_READ  , ot_f88,        "TflowCH2", "Flow water temperature CH2 circuit", "°C" },
		{  32, OT_READ  , ot_f88,        "Tdhw2", "Domestic hot water temperature 2", "°C" },
		{  33, OT_READ  , ot_s16,        "Texhaust", "Boiler exhaust temperature", "°C" },
		{  34, OT_UNDEF , ot_undef, "", "", "" },
		{  35, OT_READ  , ot_u16,	 		"FanSpeed", "fan speed", "rpm" },
		{  36, OT_READ  , ot_f88, 			"ElectricalCurrentBurnerFlame", "Electrical current through burner flame", "µA" },
		{  37, OT_READ  , ot_f88, 			"TRoomCH2", "Room temperature for 2nd CH circuit", "°C" },
		{  38, OT_READ  , ot_u8u8, 			"RelativeHumidity", "Relative Humidity", "%" },
		{  39, OT_UNDEF , ot_undef, "", "", "" },
		{  40, OT_UNDEF , ot_undef, "", "", "" },
		{  41, OT_UNDEF , ot_undef, "", "", "" },
		{  42, OT_UNDEF , ot_undef, "", "", "" },
		{  43, OT_UNDEF , ot_undef, "", "", "" },
		{  44, OT_UNDEF , ot_undef, "", "", "" },
		{  45, OT_UNDEF , ot_undef, "", "", "" },
		{  46, OT_UNDEF , ot_undef, "", "", "" },
		{  47, OT_UNDEF , ot_undef, "", "", "" },
		{  48, OT_READ  , ot_s8s8,        "TdhwSetUBTdhwSetLB", "DHW setpoint upper & lower bounds for adjustment", "°C" },
		{  49, OT_READ  , ot_s8s8,        "MaxTSetUBMaxTSetLB", "Max CH water setpoint upper & lower bounds for adjustment", "°C" },
		{  50, OT_READ  , ot_s8s8,        "HcratioUBHcratioLB", "OTC heat curve ratio upper & lower bounds for adjustment", "" },
		{  51, OT_UNDEF , ot_undef, "", "", "" },
		{  52, OT_UNDEF , ot_undef, "", "", "" },
		{  53, OT_UNDEF , ot_undef, "", "", "" },
		{  54, OT_UNDEF , ot_undef, "", "", "" },
		{  55, OT_UNDEF , ot_undef, "", "", "" },
		{  56, OT_RW    , ot_f88,         "TdhwSet", "DHW setpoint", "°C" },	
		{  57, OT_RW    , ot_f88,         "MaxTSet", "MaxCH water setpoint", "°C" },
		{  58, OT_RW    , ot_f88,         "Hcratio", "OTC heat curve ratio", "°C" },
		{  59, OT_UNDEF , ot_undef, "", "", "" },
		{  60, OT_UNDEF , ot_undef, "", "", "" },
		{  61, OT_UNDEF , ot_undef, "", "", "" },
		{  62, OT_UNDEF , ot_undef, "", "", "" },
		{  63, OT_UNDEF , ot_undef, "", "", "" },
		{  64, OT_UNDEF , ot_undef, "", "", "" },
		{  65, OT_UNDEF , ot_undef, "", "", "" },
		{  66, OT_UNDEF , ot_undef, "", "", "" },
		{  67, OT_UNDEF , ot_undef, "", "", "" },
		{  68, OT_UNDEF , ot_undef, "", "", "" },
		{  69, OT_UNDEF , ot_undef, "", "", "" },
		{  70, OT_READ  , ot_flag8flag8,  		"StatusVH", "Status Ventilation/Heat recovery", "" },
		{  71, OT_WRITE , ot_u8, 				"ControlSetpointVH", "Control setpoint V/H", "" },
		{  72, OT_READ  , ot_flag8u8, 			"ASFFaultCodeVH", "Application-specific Fault Flags/Code V/H", "" },
		{  73, OT_READ  , ot_u16,		 		"DiagnosticCodeVH", "Diagnostic Code V/H", "" },
		{  74, OT_READ  , ot_flag8u8,			"ConfigMemberIDVH", "Config/Member ID V/H", "" },
		{  75, OT_READ  , ot_f88, 				"OpenthermVersionVH", "OpenTherm Version V/H", "" },
		{  76, OT_READ  , ot_u8u8, 				"VersionTypeVH", "Version & Type V/H", "" },
		{  77, OT_READ  , ot_u8, 				"RelativeVentilation", "Relative Ventilation", "%" },
		{  78, OT_RW    , ot_u8u8, 				"RelativeHumidityExhaustAir", "Relative Humidity Exaust Air", "%" },
		{  79, OT_RW    , ot_u16, 				"CO2LevelExhaustAir", "CO2 Level Exhaust Air", "ppm" },
 		{  80, OT_READ  , ot_f88, 				"SupplyInletTemperature", "Supply Inlet Temperature", "°C" },
 		{  81, OT_READ  , ot_f88, 				"SupplyOutletTemperature", "Supply Outlet Temperature", "°C" },
 		{  82, OT_READ  , ot_f88, 				"ExhaustInletTemperature", "Exhaust Inlet Temperature", "°C" },
 		{  83, OT_READ  , ot_f88, 				"ExhaustOutletTemperature", "Exhaust Outlet Temperature", "°C" },
		{  84, OT_READ  , ot_u16, 				"ActualExhaustFanSpeed", "Actual Exhaust Fan Speed", "rpm" },
		{  85, OT_READ  , ot_u16, 				"ActualSupplyFanSpeed", "Actual Supply Fan Speed", "rpm" },
		{  86, OT_READ  , ot_flag8flag8, 		"RemoteParameterSettingVH", "Remote Parameter Setting V/H", "" },
		{  87, OT_RW 	, ot_u8, 				"NominalVentilationValue", "Nominal Ventilation Value", "" },
		{  88, OT_READ  , ot_u8u8, 				"TSPNumberVH", "TSP Number V/H", "" },
		{  89, OT_RW    , ot_u8u8, 				"TSPEntryVH", "TSP Entry V/H", "" },
		{  90, OT_READ  , ot_u8u8, 				"FaultBufferSizeVH", "Fault Buffer Size V/H", "" },
		{  91, OT_READ  , ot_u8u8, 				"FaultBufferEntryVH", "Fault Buffer Entry V/H", "" },
		{  92, OT_UNDEF , ot_undef, "", "", "" },
		{  93, OT_UNDEF , ot_undef, "", "", "" },
		{  94, OT_UNDEF , ot_undef, "", "", "" },
		{  95, OT_UNDEF , ot_undef, "", "", "" },
		{  96, OT_UNDEF , ot_undef, "", "", "" },
		{  97, OT_UNDEF , ot_undef, "", "", "" },
		{  98, OT_READ  , ot_u8u8, 				"RFstrengthbatterylevel", "RF strength and battery level", "" },
		{  99, OT_READ  , ot_u8u8, 				"OperatingMode_HC1_HC2_DHW", "Operating Mode HC1, HC2/ DHW", "" },
		{ 100, OT_READ  , ot_flag8,       		"RoomRemoteOverrideFunction", "Function of manual and program changes in master and remote room setpoint.", "" },
		{ 101, OT_READ  , ot_flag8flag8, 		"SolarStorageMaster", "Solar Storage Master mode", "" },
		{ 102, OT_READ  , ot_flag8u8,    		"SolarStorageASFflags", "Solar Storage Application-specific flags and OEM fault", "" },
        { 103, OT_READ  , ot_flag8u8,    		"SolarStorageSlaveConfigMemberIDcode", "Solar Storage Slave Config / Member ID", "" },
		{ 104, OT_READ  , ot_u8u8,        		"SolarStorageVersionType", "Solar Storage product version number and type", "" },
		{ 105, OT_READ  , ot_u8u8,       		"SolarStorageTSP", "Solar Storage Number of Transparent-Slave-Parameters supported", "" },
		{ 106, OT_RW    , ot_u8u8,       		"SolarStorageTSPindexTSPvalue", "Solar Storage Index number / Value of referred-to transparent slave parameter", "" },
		{ 107, OT_READ  , ot_u8u8,       		"SolarStorageFHBsize", "Solar Storage Size of Fault-History-Buffer supported by slave", "" },
		{ 108, OT_READ  , ot_u8u8,       		"SolarStorageFHBindexFHBvalue", "Solar Storage Index number / Value of referred-to fault-history buffer entry", "" },
		{ 109, OT_READ  , ot_u16, 				"ElectricityProducerStarts", "Electricity producer starts", "" },
		{ 110, OT_READ  , ot_u16, 				"ElectricityProducerHours", "Electricity producer hours", "" },
		{ 111, OT_READ  , ot_u16, 				"ElectricityProduction", "Electricity production", "" },
		{ 112, OT_READ  , ot_u16, 				"CumulativElectricityProduction", "Cumulativ Electricity production", "" },
		{ 113, OT_RW    , ot_u16,         		"BurnerUnsuccessfulStarts", "Nr of un-successful burner starts", "" },
		{ 114, OT_RW    , ot_u16,         		"FlameSignalTooLow", "Nr of times flame signal was too low", "" },
		{ 115, OT_READ  , ot_u16,         		"OEMDiagnosticCode", "OEM-specific diagnostic/service code", "" },
		{ 116, OT_RW    , ot_u16,         		"BurnerStarts", "Nr of starts burner", "" },
		{ 117, OT_RW    , ot_u16,         		"CHPumpStarts", "Nr of starts CH pump", "" },
		{ 118, OT_RW    , ot_u16,         		"DHWPumpValveStarts", "Nr of starts DHW pump/valve", "" },
		{ 119, OT_RW    , ot_u16,         		"DHWBurnerStarts", "Nr of starts burner during DHW mode", "" },
		{ 120, OT_RW    , ot_u16,         		"BurnerOperationHours", "Nr of hours that burner is in operation (i.e. flame on)", "" },
		{ 121, OT_RW    , ot_u16,         		"CHPumpOperationHours", "Nr of hours that CH pump has been running", "" },
		{ 122, OT_RW    , ot_u16,         		"DHWPumpValveOperationHours", "Nr of hours that DHW pump has been running or DHW valve has been opened ", "" },
		{ 123, OT_RW    , ot_u16,         		"DHWBurnerOperationHours", "Nr of hours that burner is in operation during DHW mode", "" },
		{ 124, OT_READ  , ot_f88,         		"OpenThermVersionMaster", "Master Version OpenTherm Protocol Specification", "" },
		{ 125, OT_READ  , ot_f88,         		"OpenThermVersionSlave", "Slave Version OpenTherm Protocol Specification", "" },
		{ 126, OT_READ  , ot_u8u8,        		"MasterVersion", "Master product version number and type", "" },
		{ 127, OT_READ  , ot_u8u8,        		"SlaveVersion", "Slave product version number and type", "" },
		{ 128, OT_UNDEF , ot_undef, "", "", "" },
		{ 129, OT_UNDEF , ot_undef, "", "", "" },
		{ 130, OT_UNDEF , ot_undef, "", "", "" },
		{ 131, OT_RW 	, ot_u8u8, 		"RemehadFdUcodes", "Remeha dF-/dU-codes", "" },
		{ 132, OT_READ 	, ot_u8u8, 		"RemehaServicemessage", "Remeha Servicemessage", "" },
		{ 133, OT_READ 	, ot_u8u8, 		"RemehaDetectionConnectedSCU", "Remeha detection connected SCU’s", "" },
		// all data ids are not defined above are resevered for future use
	};

#define OT_MSGID_MAX 133

time_t msglastupdated[255] = {0}; //all msg, even if they are unknown

struct OT_cmd_t { // see all possible commands for PIC here: https://otgw.tclcode.com/firmware.html
	char cmd[15];
	int cmdlen;
	int retrycnt; 
	time_t due;
};

#define CMDQUEUE_MAX 20
struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];
static int cmdptr = 0;

enum OpenThermStatus {
	OT_NOT_INITIALIZED,
	OT_READY,
	OT_DELAY,
	OT_REQUEST_SENDING,
	OT_RESPONSE_WAITING,
	OT_RESPONSE_START_BIT,
	OT_RESPONSE_RECEIVING,
	OT_RESPONSE_READY,
	OT_RESPONSE_INVALID	
};

/**
 * Structure to hold Opentherm data packet content.
 * Use f88(), u16() or s16() functions to get appropriate value of data packet accoridng to id of message.
 */
/**
 * Structure to hold Opentherm data packet content.
 * Use f88(), u16() or s16() functions to get appropriate value of data packet according to id of message.
 */
struct OpenthermData {
  byte masterslave; //0=master, 1=slave
  byte type;
  byte id;
  byte valueHB;
  byte valueLB;

  /**
   * @return float representation of data packet value
   */
  float f88();

  /**
   * @param float number to set as value of this data packet
   */
  void f88(float value);

  /**
   * @return unsigned 16b integer representation of data packet value
   */
  uint16_t u16();

  /**
   * @param unsigned 16b integer number to set as value of this data packet
   */
  void u16(uint16_t value);

  /**
   * @return signed 16b integer representation of data packet value
   */
  int16_t s16();

  /**
   * @param signed 16b integer number to set as value of this data packet
   */
  void s16(int16_t value);
};


//** Moet ik nog iets mee? Of niet...**//

// array set learn {
  //     LA PR=L LB PR=L LC PR=L LD PR=L LE PR=L LF PR=L GA PR=G GB PR=G
  //     VR PR=V GW PR=M IT PR=T SB PR=S HW PR=W
  // }

	// BoilerWaterTemperature	boilertemp	d
	// BoilerWaterTemperature2	boilertemp2	d
	// CHEnable		chenable	b
	// CH2Enable		ch2enable	b
	// CHSetpoint		chsetpoint	d
	// CentralHeating		chmode		b
	// CentralHeating2		ch2mode		b
	// ControlSetpoint		controlsp	d
	// ControlSetpoint2	controlsp2	d
	// DHWEnable		dhwenable	b
	// DHWSetpoint		dhwsetpoint	d
	// DHWTemperature		dhwtemp		d
	// DHWTemperature2		dhwtemp2	d
	// Fault			fault		b
	// Flame			flame		b
	// HotWater		dhwmode		b
	// Modulation		modulation	d
	// OutsideTemperature	outside		d
	// ReturnWaterTemperature	returntemp	d
	// RoomTemperature		roomtemp	d
	// RoomTemperature2	roomtemp2	d
	// Setpoint		setpoint	d
	// Setpoint2		setpoint2	d

    // airpresfault	"Air pressure fault"
    // boilertemp		"Boiler water temperature"
    // boilertemp2		"Boiler water temperature 2"
    // exhausttemp		"Boiler exhaust temperature"
    // ch2enable		"Central heating 2 enable"
    // ch2mode		"Central heating 2 mode"
    // chenable		"Central heating enable"
    // chmode		"Central heating mode"
    // chwsetpoint		"Central heating setpoint"
    // chbh		"CH burner hours"
    // chbs		"CH burner starts"
    // chph		"CH pump hours"
    // chps		"CH pump starts"
    // pressure		"CH water pressure"
    // controlsp		"Control setpoint"
    // controlsp2		"Control setpoint 2"
    // coolingenable	"Cooling enable"
    // coolingstatus	"Cooling status"
    // diag		"Diagnostic indication"
    // dhwbh		"DHW burner hours"
    // dhwbs		"DHW burner starts"
    // dhwflowrate		"DHW flow rate"
    // dhwph		"DHW pump hours"
    // dhwps		"DHW pump starts"
    // dhwenable		"Domestic hot water enable"
    // dhwmode		"Domestic hot water mode"
    // dhwsetpoint		"Domestic hot water setpoint"
    // dhwtemp		"Domestic hot water temp"
    // dhwtemp2		"Domestic hot water temp 2"
    // fault		"Fault indication"
    // flame		"Flame"
    // flamefault		"Gas/flame fault"
    // lockoutreset	"Lockout-reset"
    // lowpressure		"Low water pressure"
    // maxmod		"Max rel modulation level"
    // modulation		"Modulation"
    // faultcode		"OEM fault/error code"
    // otcstate		"Outside temp compensation"
    // outside		"Outside temperature"
    // override    	"Rem override room setpoint"
    // returntemp		"Return water temperature"
    // setpoint		"Room setpoint"
    // setpoint2		"Room setpoint 2"
    // roomtemp		"Room temperature"
    // roomtemp2		"Room temperature 2"
    // service		"Service request"
    // timestamp		"Time stamp"
    // overtemp		"Water over-temperature"





#endif


/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
***************************************************************************/
