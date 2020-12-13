/*
***************************************************************************  
**  Program  : Header file: OTGWStuff.h 
**  Version  : v0.4.0
**
**  Copyright (c) 2020 Robert van den Breemen
**  Borrowed from OpenTherm library from: 
**      https://github.com/jpraus/arduino-opentherm
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************       
*/  

#define OTGW_COMMAND_TOPIC "command"

typedef struct {
	uint16_t 	Status = 0; 				// flag8 / flag8  Master and Slave Status flags. 
	float 		Tset = 0.0; 					// f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
	uint16_t	MConfigMMemberIDcode = 0; 	// flag8 / u8  Master Configuration Flags /  Master MemberID Code 
	uint16_t	SConfigSMemberIDcode = 0; // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code 
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
	float 		Tstorage = 0.0 ; // f8.8  Solar storage temperature (°C)
	float 		Tcollector = 0.0 ; // f8.8  Solar collector temperature (°C)
	float 		TflowCH2 = 0.0 ; // f8.8  Flow water temperature CH2 circuit (°C)
	float 		Tdhw2 = 0.0 ; // f8.8  Domestic hot water temperature 2 (°C)
	int16_t 	Texhaust = 0; // s16  Boiler exhaust temperature (°C)
	uint16_t 	TdhwSetUBTdhwSetLB = 0 ; // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
	uint16_t 	MaxTSetUBMaxTSetLB = 0; // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
	uint16_t	HcratioUBHcratioLB = 0; // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
	float 		TdhwSet = 0.0 ; // f8.8  DHW setpoint (°C)    (Remote parameter 1)
	float 		MaxTSet = 0.0 ; // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
	float 		Hcratio = 0.0 ; // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)
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
	Status, // flag8 / flag8  Master and Slave Status flags. 
	TSet, // f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
	MConfigMMemberIDcode, // flag8 / u8  Master Configuration Flags /  Master MemberID Code 
	SConfigSMemberIDcode, // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code 
	Command, // u8 / u8  Remote Command 
	ASFflags, // / OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code 
	RBPflags, // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags 
	CoolingControl, // f8.8  Cooling control signal (%) 
	TsetCH2, // f8.8  Control setpoint for 2e CH circuit (°C)
	TrOverride, // f8.8  Remote override room setpoint 
	TSP, // u8 / u8  Number of Transparent-Slave-Parameters supported by slave 
	TSPindexTSPvalue, // u8 / u8  Index number / Value of referred-to transparent slave parameter. 
	FHBsize, // u8 / u8  Size of Fault-History-Buffer supported by slave 
	FHBindexFHBvalue, // u8 / u8  Index number / Value of referred-to fault-history buffer entry. 
	MaxRelModLevelSetting, // f8.8  Maximum relative modulation level setting (%) 
	MaxCapacityMinModLevel, // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%) 
	TrSet, // f8.8  Room Setpoint (°C)
	RelModLevel, // f8.8  Relative Modulation Level (%) 
	CHPressure, // f8.8  Water pressure in CH circuit  (bar) 
	DHWFlowRate, // f8.8  Water flow rate in DHW circuit. (litres/minute) 
	DayTime, // special / u8  Day of Week and Time of Day 
	Date, // u8 / u8  Calendar date 
	Year, // u16  Calendar year 
	TrSetCH2, // f8.8  Room Setpoint for 2nd CH circuit (°C)
	Tr, // f8.8  Room temperature (°C)
	Tboiler, // f8.8  Boiler flow water temperature (°C)
	Tdhw, // f8.8  DHW temperature (°C)
	Toutside, // f8.8  Outside temperature (°C)
	Tret, // f8.8  Return water temperature (°C)
	Tstorage, // f8.8  Solar storage temperature (°C)
	Tcollector, // f8.8  Solar collector temperature (°C)
	TflowCH2, // f8.8  Flow water temperature CH2 circuit (°C)
	Tdhw2, // f8.8  Domestic hot water temperature 2 (°C)
	Texhaust, // s16  Boiler exhaust temperature (°C)
	TdhwSetUBTdhwSetLB = 48, // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
	MaxTSetUBMaxTSetLB, // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
	HcratioUBHcratioLB, // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
	TdhwSet = 56, // f8.8  DHW setpoint (°C)    (Remote parameter 1)
	MaxTSet, // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
	Hcratio, // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)
	RemoteOverrideFunction = 100, // flag8 / -  Function of manual and program changes in master and remote room setpoint. 
	OEMDiagnosticCode = 115, // u16  OEM-specific diagnostic/service code 
	BurnerStarts, // u16  Number of starts burner 
	CHPumpStarts, // u16  Number of starts CH pump 
	DHWPumpValveStarts, // u16  Number of starts DHW pump/valve 
	DHWBurnerStarts, // u16  Number of starts burner during DHW mode 
	BurnerOperationHours, // u16  Number of hours that burner is in operation (i.e. flame on) 
	CHPumpOperationHours, // u16  Number of hours that CH pump has been running 
	DHWPumpValveOperationHours, // u16  Number of hours that DHW pump has been running or DHW valve has been opened 
	DHWBurnerOperationHours, // u16  Number of hours that burner is in operation during DHW mode 
	OpenThermVersionMaster, // f8.8  The implemented version of the OpenTherm Protocol Specification in the master. 
	OpenThermVersionSlave, // f8.8  The implemented version of the OpenTherm Protocol Specification in the slave. 
	MasterVersion, // u8 / u8  Master product version number and type 
	SlaveVersion, // u8 / u8  Slave product version number and type
};
	enum OTtype_t { ot_f88, ot_s16, ot_s8s8, ot_u16, ot_u8u8, ot_flag8, ot_flag8flag8, ot_special, ot_flag8u8, ot_undef}; 
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

    OTlookup_t OTmap[] = {
        {   0, OT_READ  , ot_flag8flag8, "Status", "Master and Slave status", "" },
        {   1, OT_WRITE , ot_f88,        "TSet", "Control setpoint", "°C" },
        {   2, OT_WRITE , ot_flag8u8,    "MConfigMMemberIDcode", "Master Config / Member ID", "" },
        {   3, OT_READ  , ot_flag8u8,    "SConfigSMemberIDcode", "Slave Config / Member ID", "" },
        {   4, OT_RW    , ot_u8u8,       "Command", "Command-Code", "" },
		{   5, OT_READ  , ot_flag8u8,    "ASFflags", "Application-specific fault", "" },
		{   6, OT_READ  , ot_flag8u8,    "RBPflags", "Remote-parameter flags ", "" },
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
		{  29, OT_READ  , ot_f88,        "Tstorage", "Solar storage temperature", "°C" },
		{  30, OT_READ  , ot_f88,        "Tcollector", "Solar collector temperature", "°C" },
		{  31, OT_READ  , ot_f88,        "TflowCH2", "Flow water temperature CH2 circuit", "°C" },
		{  32, OT_READ  , ot_s16,        "Tdhw2", "Domestic hot water temperature 2", "°C" },
		{  33, OT_READ  , ot_f88,        "Texhaust", "Boiler exhaust temperature", "°C" },
		{  34, OT_UNDEF , ot_undef, "", "", "" },
		{  35, OT_UNDEF , ot_undef, "", "", "" },
		{  36, OT_UNDEF , ot_undef, "", "", "" },
		{  37, OT_UNDEF , ot_undef, "", "", "" },
		{  38, OT_UNDEF , ot_undef, "", "", "" },
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
		{  70, OT_UNDEF , ot_undef, "", "", "" },
		{  71, OT_UNDEF , ot_undef, "", "", "" },
		{  72, OT_UNDEF , ot_undef, "", "", "" },
		{  73, OT_UNDEF , ot_undef, "", "", "" },
		{  74, OT_UNDEF , ot_undef, "", "", "" },
		{  75, OT_UNDEF , ot_undef, "", "", "" },
		{  76, OT_UNDEF , ot_undef, "", "", "" },
		{  77, OT_UNDEF , ot_undef, "", "", "" },
		{  78, OT_UNDEF , ot_undef, "", "", "" },
		{  79, OT_UNDEF , ot_undef, "", "", "" },
 		{  80, OT_UNDEF , ot_undef, "", "", "" },
		{  81, OT_UNDEF , ot_undef, "", "", "" },
		{  82, OT_UNDEF , ot_undef, "", "", "" },
		{  83, OT_UNDEF , ot_undef, "", "", "" },
		{  84, OT_UNDEF , ot_undef, "", "", "" },
		{  85, OT_UNDEF , ot_undef, "", "", "" },
		{  86, OT_UNDEF , ot_undef, "", "", "" },
		{  87, OT_UNDEF , ot_undef, "", "", "" },
		{  88, OT_UNDEF , ot_undef, "", "", "" },
		{  89, OT_UNDEF , ot_undef, "", "", "" },		
		{  90, OT_UNDEF , ot_undef, "", "", "" },
		{  91, OT_UNDEF , ot_undef, "", "", "" },
		{  92, OT_UNDEF , ot_undef, "", "", "" },
		{  93, OT_UNDEF , ot_undef, "", "", "" },
		{  94, OT_UNDEF , ot_undef, "", "", "" },
		{  95, OT_UNDEF , ot_undef, "", "", "" },
		{  96, OT_UNDEF , ot_undef, "", "", "" },
		{  97, OT_UNDEF , ot_undef, "", "", "" },
		{  98, OT_UNDEF , ot_undef, "", "", "" },
		{  99, OT_UNDEF , ot_undef, "", "", "" },
		{ 100, OT_READ  , ot_flag8,       "RRemoteOverrideFunction", "Function of manual and program changes in master and remote room setpoint.", "" },
		{ 101, OT_UNDEF , ot_undef, "", "", "" },
		{ 102, OT_UNDEF , ot_undef, "", "", "" },
		{ 103, OT_UNDEF , ot_undef, "", "", "" },
		{ 104, OT_UNDEF , ot_undef, "", "", "" },
		{ 105, OT_UNDEF , ot_undef, "", "", "" },
		{ 106, OT_UNDEF , ot_undef, "", "", "" },
		{ 107, OT_UNDEF , ot_undef, "", "", "" },
		{ 108, OT_UNDEF , ot_undef, "", "", "" },
		{ 109, OT_UNDEF , ot_undef, "", "", "" },
		{ 110, OT_UNDEF , ot_undef, "", "", "" },
		{ 111, OT_UNDEF , ot_undef, "", "", "" },
		{ 112, OT_UNDEF , ot_undef, "", "", "" },
		{ 113, OT_UNDEF , ot_undef, "", "", "" },
		{ 114, OT_UNDEF , ot_undef, "", "", "" },
		{ 115, OT_READ  , ot_u16,         "OEMDiagnosticCode", "OEM-specific diagnostic/service code", "" },
		{ 116, OT_RW    , ot_u16,         "BurnerStarts", "Nr of starts burner", "" },
		{ 117, OT_RW    , ot_u16,         "CHPumpStarts", "Nr of starts CH pump", "" },
		{ 118, OT_RW    , ot_u16,         "DHWPumpValveStarts", "Nr of starts DHW pump/valve", "" },
		{ 119, OT_RW    , ot_u16,         "DHWBurnerStarts", "Nr of starts burner during DHW mode", "" },
		{ 120, OT_RW    , ot_u16,         "BurnerOperationHours", "Nr of hours that burner is in operation (i.e. flame on)", "" },
		{ 121, OT_RW    , ot_u16,         "CHPumpOperationHours", "Nr of hours that CH pump has been running", "" },
		{ 122, OT_RW    , ot_u16,         "DHWPumpValveOperationHours", "Nr of hours that DHW pump has been running or DHW valve has been opened ", "" },
		{ 123, OT_RW    , ot_u16,         "DHWBurnerOperationHours", "Nr of hours that burner is in operation during DHW mode", "" },
		{ 124, OT_READ  , ot_f88,         "OpenThermVersionMaster", "Master Version OpenTherm Protocol Specification", "" },
		{ 125, OT_READ  , ot_f88,         "OpenThermVersionSlave", "Slave Version OpenTherm Protocol Specification", "" },
		{ 126, OT_READ  , ot_u8u8,        "MasterVersion", "Master product version number and type", "" },
		{ 127, OT_READ  , ot_u8u8,        "SlaveVersion", "Slave product version number and type", "" },
		// all data ids are not defined above are resevered for future use
	};
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

static volatile unsigned long _data;



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
