/*
***************************************************************************  
**  Program  : Header file: OTGWStuff.h 
**  Version  : v0.2.0
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
