/* 
***************************************************************************  
**  Program  : OTGWStuff
**  Version  : v0.1.0
**
**  Copyright (c) 2020 Robert van den Breemen
**  Borrowed from OpenTherm library from: 
**      https://github.com/jpraus/arduino-opentherm
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#define EXT_WD_I2C_ADDRESS 0x26
#define PIN_I2C_SDA 4
#define PIN_I2C_SCL 5

/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- Endf of macro's --- */

OpenthermData data;


//===================[ Watchdog OTGW ]===============================
String initWatchDog() {
  // configure hardware pins according to eeprom settings.
    DebugTln(F("INIT : I2C"));
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);  //configure the I2C bus
  //=============================================
  // I2C Watchdog boot status check
  String ReasonReset = "";
  
  delay(500);
  Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   // OTGW WD address
  Wire.write(0x83);             // command to set pointer
  Wire.write(17);               // pointer value to status byte
  Wire.endTransmission();
  
  Wire.requestFrom((uint8_t)EXT_WD_I2C_ADDRESS, (uint8_t)1);
  if (Wire.available())
  {
    byte status = Wire.read();
    if (status & 0x1)
    {
      DebugTln(F("INIT : Reset by WD!"));
      ReasonReset = "Reset by External WD";
      //lastReset = BOOT_CAUSE_EXT_WD;
    }
  }
  return ReasonReset;
  //===========================================
}

//===[ Feed the WatchDog before it bites! (1x per second) ]===
void feedWatchDog() {
  //make sure to do this at least once a second
  //==== feed the WD over I2C ==== 
  // Address: 0x26
  // I2C Watchdog feed
  Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
  Wire.write(0xA5);                             //Feed the dog, before it bites.
  Wire.endTransmission();                       //That's all there is...
  //==== feed the WD over I2C ==== 
}

//===================[ Watchdog OTGW ]===============================

//===================[ OTGW PS=1 Command ]===============================
void getOTGW_PS_1(){
  DebugTln("PS=1");
  Serial.write("PS=1\r\n");
  delay(100);
  while(Serial.available() > 0) 
  { 
    String strBuffer = Serial.readStringUntil('\n');
    strBuffer.trim(); //remove LF and CR (and whitespaces)
    DebugTln(strBuffer);
  }
  DebugTln("PS=0");
  Serial.write("PS=0\r\n");
}

//===================[ OTGW PS=1 Command ]===============================

//=======================================================================
float OpenthermData::f88() {
  float value = (int8_t) valueHB;
  return value + (float)valueLB / 256.0;
}

void OpenthermData::f88(float value) {
  if (value >= 0) {
    valueHB = (byte) value;
    float fraction = (value - valueHB);
    valueLB = fraction * 256.0;
  }
  else {
    valueHB = (byte)(value - 1);
    float fraction = (value - valueHB - 1);
    valueLB = fraction * 256.0;
  }
}

uint16_t OpenthermData::u16() {
  uint16_t value = valueHB;
  return (value << 8) | valueLB;
}

void OpenthermData::u16(uint16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t value = valueHB;
  return (value << 8) | valueLB;
}

void OpenthermData::s16(int16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

//parsing helpers
const char *statusToString(OpenThermResponseStatus status)
{
	switch (status) {
		case OT_NONE:    return "NONE";
		case OT_SUCCESS:  return "SUCCESS";
		case OT_INVALID:  return "INVALID";
		case OT_TIMEOUT:  return "TIMEOUT";
		default:          return "UNKNOWN";
	}
}

const char *messageTypeToString(OpenThermMessageType message_type)
{
	switch (message_type) {
		case OT_READ_DATA:        return "READ_DATA";
		case OT_WRITE_DATA:       return "WRITE_DATA";
		case OT_INVALID_DATA:     return "INVALID_DATA";
		case OT_RESERVED:         return "RESERVED";
		case OT_READ_ACK:         return "READ_ACK";
		case OT_WRITE_ACK:        return "WRITE_ACK";
		case OT_DATA_INVALID:     return "DATA_INVALID";
		case OT_UNKNOWN_DATA_ID:  return "UNKNOWN_DATA_ID";
		default:                  return "UNKNOWN";
	}
}

const char *messageIDToString(OpenThermMessageID message_id){
    switch (message_id) {
      case Status:                    return "Status"; // flag8 / flag8  Master and Slave Status flags. 
	    case TSet:                      return "TSet";  // f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
      case MConfigMMemberIDcode:      return "MConfigMMemberIDcode"; // flag8 / u8  Master Configuration Flags /  Master MemberID Code 
      case SConfigSMemberIDcode:      return "SConfigSMemberIDcode"; // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code 
	    case Command:                   return "Command"; // u8 / u8  Remote Command 
	    case ASFflags:                  return "ASFflags"; // / OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code 
	    case RBPflags:                  return "RBPflags"; // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags 
	    case CoolingControl:            return "CoolingControl"; // f8.8  Cooling control signal (%) 
	    case TsetCH2:                   return "TsetCH2"; // f8.8  Control setpoint for 2e CH circuit (°C)
	    case TrOverride:                return "TrOverride"; // f8.8  Remote override room setpoint 
	    case TSP:                       return "TSP";  // u8 / u8  Number of Transparent-Slave-Parameters supported by slave 
	    case TSPindexTSPvalue:          return "TSPindexTSPvalue"; // u8 / u8  Index number / Value of referred-to transparent slave parameter. 
	    case FHBsize:                   return "FHBsize"; // u8 / u8  Size of Fault-History-Buffer supported by slave 
	    case FHBindexFHBvalue:          return "FHBindexFHBvalue"; // u8 / u8  Index number / Value of referred-to fault-history buffer entry. 
	    case MaxRelModLevelSetting:     return "MaxRelModLevelSetting"; // f8.8  Maximum relative modulation level setting (%) 
	    case MaxCapacityMinModLevel:    return "MaxCapacityMinModLevel"; // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%) 
	    case TrSet:                     return "TrSet"; // f8.8  Room Setpoint (°C)
	    case RelModLevel:               return "RelModLevel"; // f8.8  Relative Modulation Level (%) 
	    case CHPressure:                return "CHPressure"; // f8.8  Water pressure in CH circuit  (bar) 
	    case DHWFlowRate:               return "DHWFlowRate"; // f8.8  Water flow rate in DHW circuit. (litres/minute) 
	    case DayTime:                   return "DayTime"; // special / u8  Day of Week and Time of Day 
	    case Date:                      return "Date"; // u8 / u8  Calendar date 
	    case Year:                      return "Year"; // u16  Calendar year 
	    case TrSetCH2:                  return "TrSetCH2"; // f8.8  Room Setpoint for 2nd CH circuit (°C)
	    case Tr:                        return "Troom"; // f8.8  Room temperature (°C)
	    case Tboiler:                   return "Tboiler"; // f8.8  Boiler flow water temperature (°C)
	    case Tdhw:                      return "Tdhw"; // f8.8  DHW temperature (°C)
	    case Toutside:                  return "Toutside"; // f8.8  Outside temperature (°C)
	    case Tret:                      return "Tret"; // f8.8  Return water temperature (°C)
	    case Tstorage:                  return "Tstorage"; // f8.8  Solar storage temperature (°C)
	    case Tcollector:                return "Tcollector"; // f8.8  Solar collector temperature (°C)
	    case TflowCH2:                  return "TflowCH2"; // f8.8  Flow water temperature CH2 circuit (°C)
	    case Tdhw2:                     return "Tdhw2"; // f8.8  Domestic hot water temperature 2 (°C)
	    case Texhaust:                  return "Texhaust"; // s16  Boiler exhaust temperature (°C)
	    case TdhwSetUBTdhwSetLB:        return "TdhwSetUBTdhwSetLB"; // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
	    case MaxTSetUBMaxTSetLB:        return "MaxTSetUBMaxTSetLB"; // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
	    case HcratioUBHcratioLB:        return "HcratioUBHcratioLB"; // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
	    case TdhwSet:                   return "TdhwSet"; // f8.8  DHW setpoint (°C)    (Remote parameter 1)
	    case MaxTSet:                   return "MaxTSet"; // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
	    case Hcratio:                   return "Hcratio"; // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)
	    case RemoteOverrideFunction:    return "RemoteOverrideFunction"; // flag8 / -  Function of manual and program changes in master and remote room setpoint. 
	    case OEMDiagnosticCode:         return "OEMDiagnosticCode"; // u16  OEM-specific diagnostic/service code 
	    case BurnerStarts:              return "BurnerStarts"; // u16  Number of starts burner 
	    case CHPumpStarts:              return "CHPumpStarts"; // u16  Number of starts CH pump 
	    case DHWPumpValveStarts:        return "DHWPumpValveStarts"; // u16  Number of starts DHW pump/valve 
	    case DHWBurnerStarts:           return "DHWBurnerStarts";  // u16  Number of starts burner during DHW mode 
	    case BurnerOperationHours:      return "BurnerOperationHours"; // u16  Number of hours that burner is in operation (i.e. flame on) 
	    case CHPumpOperationHours:      return "CHPumpOperationHours"; // u16  Number of hours that CH pump has been running 
	    case DHWPumpValveOperationHours: return "DHWPumpValveOperationHours"; // u16  Number of hours that DHW pump has been running or DHW valve has been opened 
	    case DHWBurnerOperationHours:   return "DHWBurnerOperationHours"; // u16  Number of hours that burner is in operation during DHW mode 
	    case OpenThermVersionMaster:    return "OpenThermVersionMaster"; // f8.8  The implemented version of the OpenTherm Protocol Specification in the master. 
	    case OpenThermVersionSlave:     return "OpenThermVersionSlave"; // f8.8  The implemented version of the OpenTherm Protocol Specification in the slave. 
	    case MasterVersion:             return "MasterVersion"; // u8 / u8  Master product version number and type 
	    case SlaveVersion:              return "SlaveVersion"; // u8 / u8  Slave product version number and type
      default:                        return "Unknown";
    }
}

OpenThermMessageType getMessageType(unsigned long message)
{
    OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
    return msg_type;
}

OpenThermMessageID getDataID(unsigned long frame)
{
    return (OpenThermMessageID)((frame >> 16) & 0xFF);
}

//parsing responses

bool isFault(unsigned long response) {
	return response & 0x1;
}

bool isCentralHeatingActive(unsigned long response) {
	return response & 0x2;
}

bool isHotWaterActive(unsigned long response) {
	return response & 0x4;
}

bool isFlameOn(unsigned long response) {
	return response & 0x8;
}

bool isCoolingActive(unsigned long response) {
	return response & 0x10;
}

bool isDiagnostic(unsigned long response) {
	return response & 0x40;
}

// uint16_t getUInt(const unsigned long response) const {
// 	const uint16_t u88 = response & 0xffff;
// 	return u88;
// }

// float getFloat(const unsigned long response) const {
// 	const uint16_t u88 = getUInt(response);
// 	const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
// 	return f;
// }

// float getTemperature(unsigned long response) {
// 	float temperature = isValidResponse(response) ? getFloat(response) : 0;
// 	return temperature;
// }


void print_f88(float _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = round(data.f88()*100.0) / 100.0; // round float 2 digits, like this: x.xx     
  Debugf("%-37s = %3.2f %s", _label, _OTdata , _unit);
  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["value"] = _OTdata;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_s16(int16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.s16();     
  Debugf("%-37s = %5d %s", _label, _OTdata, _unit);
  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["value"] = _OTdata;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_s8s8(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
  Debugf("%-37s = %3d / %3d %s", _label, (int8_t)data.valueHB, (int8_t)data.valueLB, _unit);
  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["valueHB"] = (int8_t)data.valueHB;
  msgid_0["valueLB"] = (int8_t)data.valueLB;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}


void print_u16(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
  Debugf("%-37s = %5d %s", _label, _OTdata, _unit);
  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["valueHB"] = _OTdata;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());  
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_status(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
  
  String _flag8_master="";
  String _flag8_slave="";
  //bit: [clear/0, set/1]
  //  0: CH enable [ CH is disabled, CH is enabled]
  //  1: DHW enable [ DHW is disabled, DHW is enabled]
  //  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
  //  3: OTC active [OTC not active, OTC is active]
  //  4: CH2 enable [CH2 is disabled, CH2 is enabled]
  //  5: reserved
  //  6: reserved
  //  7: reserved
  _flag8_master += (((data.valueHB) & 0x01) ? 'C' : '-');
  _flag8_master += (((data.valueHB) & 0x02) ? 'D' : '-');
  _flag8_master += (((data.valueHB) & 0x04) ? 'C' : '-'); 
  _flag8_master += (((data.valueHB) & 0x08) ? 'O' : '-');
  _flag8_master += (((data.valueHB) & 0x10) ? '2' : '-'); 
  _flag8_master += (((data.valueHB) & 0x20) ? '.' : '-'); 
  _flag8_master += (((data.valueHB) & 0x40) ? '.' : '-'); 
  _flag8_master += (((data.valueHB) & 0x80) ? '.' : '-');

  //slave
  //  0: fault indication [ no fault, fault ]
  //  1: CH mode [CH not active, CH active]
  //  2: DHW mode [ DHW not active, DHW active]
  //  3: Flame status [ flame off, flame on ]
  //  4: Cooling status [ cooling mode not active, cooling mode active ]
  //  5: CH2 mode [CH2 not active, CH2 active]
  //  6: diagnostic indication [no diagnostics, diagnostic event]
  //  7: reserved
  _flag8_slave += (((data.valueLB) & 0x01) ? 'E' : '-');
  _flag8_slave += (((data.valueLB) & 0x02) ? 'C' : '-'); 
  _flag8_slave += (((data.valueLB) & 0x04) ? 'W' : '-'); 
  _flag8_slave += (((data.valueLB) & 0x08) ? 'F' : '-'); 
  _flag8_slave += (((data.valueLB) & 0x10) ? 'C' : '-'); 
  _flag8_slave += (((data.valueLB) & 0x20) ? '2' : '-'); 
  _flag8_slave += (((data.valueLB) & 0x40) ? 'D' : '-'); 
  _flag8_slave += (((data.valueLB) & 0x80) ? '.' : '-');

  Debugf("%-37s = M[%s] S[%s]", _label, _flag8_master.c_str(), _flag8_slave.c_str());

  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(11);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["master-flag8"] = _flag8_master.c_str();
  msgid_0["slave-flag8"] = _flag8_slave.c_str();
  msgid_0["unit"] = _unit;
  msgid_0["fault"] = (((data.valueLB) & 0x01) ? "On": "Off");
  msgid_0["centralheating"] = (((data.valueLB) & 0x02) ? "On": "Off");
  msgid_0["domestichotwater"] = (((data.valueLB) & 0x04) ? "On": "Off"); 
  msgid_0["flame"] = (((data.valueLB) & 0x08) ? "On" : "Off"); 
  msgid_0["cooling"] = (((data.valueLB) & 0x10) ? "On" : "Off");
  msgid_0["centralheating2"] =(((data.valueLB) & 0x20) ? "On" : "Off");
  msgid_0["diagnostic_indicator"] = (((data.valueLB) & 0x40) ? "On" : "Off");
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_ASFflags(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
 
  //bit: [clear/0, set/1]
  //0: Service request [service not req’d, service required]
  //1: Lockout-reset [ remote reset disabled, rr enabled]
  //2: Low water press [ no WP fault, water pressure fault]
  //3: Gas/flame fault [ no G/F fault, gas/flame fault ]
  //4: Air press fault [ no AP fault, air pressure fault ]
  //5: Water over-temp[ no OvT fault, over-temperat. Fault]
  //6: reserved
  //7: reserved
  String _flag8="";
  _flag8 +=(((data.valueHB) & 0x80) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x40) ? '1' : '0');
  _flag8 +=(((data.valueHB) & 0x20) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x10) ? '1' : '0');
  _flag8 +=(((data.valueHB) & 0x08) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x04) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x02) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x01) ? '1' : '0');

  Debugf("%-37s = M[%s] OEM fault code [%3d]", _label, _flag8.c_str(), data.valueLB);

  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["oem_fault"] = data.valueLB;
  msgid_0["flag8"] = _flag8.c_str();
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_flag8u8(uint16_t _OTdata, const char *_label, const char*_unit)
{
  _OTdata = data.u16();  

  //function to print data
  String _flag8="";
  _flag8 +=(((data.valueHB) & 0x80) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x40) ? '1' : '0');
  _flag8 +=(((data.valueHB) & 0x20) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x10) ? '1' : '0');
  _flag8 +=(((data.valueHB) & 0x08) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x04) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x02) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x01) ? '1' : '0');
   
  Debugf("%-37s = %s / %3d", _label, _flag8.c_str(), data.valueLB);

  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["flag8"] = _flag8.c_str();
  msgid_0["value"] = data.valueLB;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_flag8(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
  
  String _flag8="";
  _flag8 +=(((data.valueHB) & 0x80) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x40) ? '1' : '0');
  _flag8 +=(((data.valueHB) & 0x20) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x10) ? '1' : '0');
  _flag8 +=(((data.valueHB) & 0x08) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x04) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x02) ? '1' : '0'); 
  _flag8 +=(((data.valueHB) & 0x01) ? '1' : '0');

  Debugf("%-37s = %s / %3d", _label, _flag8.c_str(), data.valueHB);

  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["flag8"] = _flag8.c_str();
  msgid_0["value"] = data.valueHB;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_flag8flag8(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
  
  String _flag8_HB="";
  _flag8_HB+=(((data.valueHB) & 0x80) ? '1' : '0'); 
  _flag8_HB+=(((data.valueHB) & 0x40) ? '1' : '0');
  _flag8_HB+=(((data.valueHB) & 0x20) ? '1' : '0'); 
  _flag8_HB+=(((data.valueHB) & 0x10) ? '1' : '0');
  _flag8_HB+=(((data.valueHB) & 0x08) ? '1' : '0'); 
  _flag8_HB+=(((data.valueHB) & 0x04) ? '1' : '0'); 
  _flag8_HB+=(((data.valueHB) & 0x02) ? '1' : '0'); 
  _flag8_HB+=(((data.valueHB) & 0x01) ? '1' : '0');

  String _flag8_LB="";
  _flag8_LB+=(((data.valueLB) & 0x80) ? '1' : '0'); 
  _flag8_LB+=(((data.valueLB) & 0x40) ? '1' : '0');
  _flag8_LB+=(((data.valueLB) & 0x20) ? '1' : '0'); 
  _flag8_LB+=(((data.valueLB) & 0x10) ? '1' : '0');
  _flag8_LB+=(((data.valueLB) & 0x08) ? '1' : '0'); 
  _flag8_LB+=(((data.valueLB) & 0x04) ? '1' : '0'); 
  _flag8_LB+=(((data.valueLB) & 0x02) ? '1' : '0'); 
  _flag8_LB+=(((data.valueLB) & 0x01) ? '1' : '0');

  Debugf("%-37s = %s / %s - %3d / %3d", _label, _flag8_HB.c_str(), _flag8_LB.c_str(), data.valueHB, data.valueLB);
  //BuildJOSN for MQTT  
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["flag8HB"] = _flag8_HB.c_str();
  msgid_0["flag8LB"] = _flag8_LB.c_str();
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_u8u8(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  _OTdata = data.u16();     
  Debugf("%-37s = %3d / %3d %s", _label, (uint8_t)data.valueHB, (uint8_t)data.valueLB, _unit);
  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["valueHB"] = (int8_t)data.valueHB;
  msgid_0["valueLB"] = (int8_t)data.valueLB;
  msgid_0["unit"] = _unit;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}

void print_daytime(uint16_t _OTdata, const char *_label, const char*_unit)
{
  //function to print data
  const char *dayOfWeekName[]  { "Unknown", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag", "Zondag", "Unknown" };
  _OTdata = data.u16();     
  Debugf("%-37s = %s - %2d:%2d", _label, dayOfWeekName[(data.valueHB >> 5) & 0x7], (data.valueHB & 0x1F), data.valueLB); 
  //BuildJOSN for MQTT
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  JsonArray msgid = doc.createNestedArray(messageIDToString(static_cast<OpenThermMessageID>(data.id)));
  JsonObject msgid_0 = msgid.createNestedObject();
  msgid_0["label"] = _label;
  msgid_0["dayofweek"] = dayOfWeekName[(data.valueHB >> 5) & 0x7];
  msgid_0["hour"] = (data.valueHB & 0x1F);
  msgid_0["minutes"] = data.valueLB;
  String sJson;
  serializeJson(doc, sJson);
  DebugTf("\r\n%s\r\n", sJson.c_str());
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(data.id)), sJson.c_str());
}


//=====================================[ Handle OTGW ]=====================================================
void handleOTGW(){
  //let's try this, read all data from the serial device, and dump it to the telnet stream.
  while(Serial.available() > 0) 
  { 

    String strBuffer = Serial.readStringUntil('\n');
    strBuffer.trim(); //remove LF and CR (and whitespaces)

    // DebugTf("[%s] [%d]\r\n", strBuffer.c_str(), strBuffer.length());

    if (strBuffer.length()>=9) {
      //parse value
      uint32_t value = strtoul(strBuffer.substring(1).c_str(), NULL, 16);
      // DebugTf("Value=[%08x]\r\n", (uint32_t)value);
      //processing message
      if (strBuffer.charAt(0)=='B')
      {
        DebugT("Boiler           ");
      } else 
      if (strBuffer.charAt(0)=='T')
      {
        DebugT("Thermostat       ");
      } else
      if (strBuffer.charAt(0)=='R')
      {
        DebugT("Request Boiler   ");
      } else      
      if (strBuffer.charAt(0)=='A')
      {
        DebugT("Answer Themostat ");
      } else      
      if (strBuffer.charAt(0)=='E')
      {
        DebugT("Parity error     ");
      } else
      {
        DebugTf("Unexpected=[%c] ", strBuffer.charAt(0));
      }
      //Debugf("msg=[%s] value=[%08x]\r\n", strBuffer.c_str(), value);

      //split 32bit value into the relevant OT protocol parts
      data.type = (value >> 28) & 0x7;         // byte 1 = take 3 bits that define msg msgType
      data.id = (value >> 16) & 0xFF;          // byte 2 = message id 8 bits 
      data.valueHB = (value >> 8) & 0xFF;      // byte 3 = high byte
      data.valueLB = value & 0xFF;             // byte 4 = low byte

      //print message frame
      Debugf("type=[%3d] id=[%3d] hb=[%3d] lb=[%3d]", data.type, data.id, data.valueHB, data.valueLB);

      //print message Type and ID
      Debugf("\t[%16s]", messageTypeToString(static_cast<OpenThermMessageType>(data.type)));
      Debugf("\t[%30s]\t", messageIDToString(static_cast<OpenThermMessageID>(data.id)));
      Debugln(); DebugFlush();

      //next step interpret the OT protocol
      if (static_cast<OpenThermMessageType>(data.type) == OT_READ_ACK || static_cast<OpenThermMessageType>(data.type) == OT_WRITE_DATA) {

        //#define OTprint(data, value, text, format) ({ data= value; Debugf("[%37s]", text); Debugf("= [format]", data)})
        //interpret values f8.8
        switch (static_cast<OpenThermMessageID>(data.id)) { 
          case TSet:                      print_f88(OTdata.Tset,                                  "Control setpoint", "°C"); break;         
          case CoolingControl:            print_f88(OTdata.CoolingControl,                        "Cooling control signal","%");   break;
          case TsetCH2:                   print_f88(OTdata.TsetCH2,                               "Control setpoint for 2e circuit","°C");  break;
          case TrOverride:                print_f88(OTdata.TrOverride,                            "Remote override room setpoint","°C");  break;        
          case MaxRelModLevelSetting:     print_f88(OTdata.MaxRelModLevelSetting,                 "Max Rel Modulation level setting","%");  break;
          case TrSet:                     print_f88(OTdata.TrSet,                                 "Room setpoint","°C");  break;
          case RelModLevel:               print_f88(OTdata.RelModLevel,                           "Relative Modulation Level","%");  break;
          case CHPressure:                print_f88(OTdata.CHPressure,                            "Water pressure in CH circuit","bar"); break;
          case DHWFlowRate:               print_f88(OTdata.DHWFlowRate,                           "Water flow rate in DHW circuit","l/min");  break;
          case TrSetCH2:                  print_f88(OTdata.TrSetCH2,                              "Room Setpoint for 2nd CH circuit","°C");  break;  
          case Tr:                        print_f88(OTdata.Tr,                                    "Room temperature","°C");  break;  
          case Tboiler:                   print_f88(OTdata.Tboiler,                               "Boiler flow water temperature","°C");  break;
          case Tdhw:                      print_f88(OTdata.Tdhw,                                  "DHW temperature","°C");  break;
          case Toutside:                  print_f88(OTdata.Toutside,                              "Outside temperature","°C");  break;
          case Tret:                      print_f88(OTdata.Tret,                                  "Return water temperature","°C");  break;
          case Tstorage:                  print_f88(OTdata.Tstorage,                              "Solar storage temperature","°C");  break;
          case Tcollector:                print_f88(OTdata.Tcollector,                            "Solar collector temperature","°C");  break;
          case TflowCH2:                  print_f88(OTdata.TflowCH2,                              "Flow water temperature CH2 cir.","°C");  break;          
          case Tdhw2:                     print_f88(OTdata.Tdhw2,                                 "Domestic hot water temperature 2","°C");  break;
          case Texhaust:                  print_s16(OTdata.Texhaust,                              "Boiler exhaust temperature","°C");  break; // s16  Boiler exhaust temperature (°C)
          case TdhwSet:                   print_f88(OTdata.TdhwSet,                               "DHW setpoint","°C");  break;
          case MaxTSet:                   print_f88(OTdata.MaxTSet,                               "Max CH water setpoint","°C");  break;
          case Hcratio:                   print_f88(OTdata.Hcratio,                               "OTC heat curve ratio","°C");  break;
          case OpenThermVersionMaster:    print_f88(OTdata.OpenThermVersionMaster,                "Master OT protocol version","");  break;
          case OpenThermVersionSlave:     print_f88(OTdata.OpenThermVersionSlave,                 "Slave OT protocol version","");  break;
          case Status:                    print_status(OTdata.Status,                             "Status",""); break;
          case ASFflags:                  print_ASFflags(OTdata.ASFflags,                         "Application Specific Fault",""); break;
          // case MConfigMMemberIDcode:      print_flag8u8(OTdata.MConfigMMemberIDcode,              "Master Config / Member ID", ""); break; // flag8 / u8  Master Configuration Flags /  Master MemberID Code 
          // case SConfigSMemberIDcode:      print_flag8u8(OTdata.SConfigSMemberIDcode,              "Slave  Config / Member ID", ""); break; // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code  
          case Command:                   print_u8u8(OTdata.Command,                              "Command","");  break; // u8 / u8  Remote Command 
          case RBPflags:                  print_flag8flag8(OTdata.RBPflags,                       "RBPflags", ""); break; // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags 
          case TSP:                       print_u8u8(OTdata.TSP,                                  "Nr of Transp. Slave Parameters", ""); break; // u8 / u8  Number of Transparent-Slave-Parameters supported by slave 
          case TSPindexTSPvalue:          print_u8u8(OTdata.TSPindexTSPvalue,                     "TSPindexTSPvalue", "");  break; // u8 / u8  Index number / Value of referred-to transparent slave parameter. 
          case FHBsize:                   print_u8u8(OTdata.FHBsize,                              "FHBsize", "");  break;  // u8 / u8  Size of Fault-History-Buffer supported by slave 
          case FHBindexFHBvalue:          print_u8u8(OTdata.FHBindexFHBvalue,                     "FHBindexFHBvalue", "");  break;  // u8 / u8  Index number / Value of referred-to fault-history buffer entry. 
          case MaxCapacityMinModLevel:    print_u8u8(OTdata.MaxCapacityMinModLevel,               "MaxCapacityMinModLevel", "");  break;  // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%) 
          case DayTime:                   print_daytime(OTdata.DayTime,                           "Day of Week - Daytime (hour/min)", "");  break; // special / u8  Day of Week and Time of Day 
          case Date:                      print_u8u8(OTdata.Date,                                 "Date (Month/Day)", "");  break; // u8 / u8  Calendar date 
          case Year:                      print_u16(OTdata.Year,                                  "Year","");  break; // u16  Calendar year 
          case TdhwSetUBTdhwSetLB:        print_s8s8(OTdata.TdhwSetUBTdhwSetLB,                   "TdhwSetUBTdhwSetLB", "°C"); break;  // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
          case MaxTSetUBMaxTSetLB:        print_s8s8(OTdata.MaxTSetUBMaxTSetLB,                   "MaxTSetUBMaxTSetLB", "°C"); break;  // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
          case HcratioUBHcratioLB:        print_s8s8(OTdata.HcratioUBHcratioLB,                   "HcratioUBHcratioLB", ""); break;  // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
          case RemoteOverrideFunction:    print_flag8(OTdata.RemoteOverrideFunction,              "RemoteOverrideFunction", ""); break; // u8 / flag8 -  Function of manual and program changes in master and remote room setpoint. 
          case OEMDiagnosticCode:         print_u16(OTdata.OEMDiagnosticCode,                     "OEM diagnostic/service code","");  break; // u16  OEM-specific diagnostic/service code 
          case BurnerStarts:              print_u16(OTdata.BurnerStarts,                          "Nr of starts burner","");  break; // u16  Number of starts burner 
          case CHPumpStarts:              print_u16(OTdata.CHPumpStarts,                          "Nr of starts CH pump","");  break; // u16  Number of starts CH pump 
          case DHWPumpValveStarts:        print_u16(OTdata.DHWPumpValveStarts,                    "Nr of starts DHW pump/valve","");  break; // u16  Number of starts DHW pump/valve
          case DHWBurnerStarts:           print_u16(OTdata.DHWBurnerStarts,                       "Nr of starts burner during DHW","");  break; // u16  Number of starts burner during DHW mode 
          case BurnerOperationHours:      print_u16(OTdata.BurnerOperationHours,                  "Nr of hours burner operation","");  break; // u16  Number of hours that burner is in operation (i.e. flame on) 
          case CHPumpOperationHours:      print_u16(OTdata.CHPumpOperationHours,                  "Nr of hours CH pump running","");  break; // u16  Number of hours that CH pump has been running 
          case DHWPumpValveOperationHours: print_u16(OTdata.DHWPumpValveOperationHours,           "Nr of hours DHW valve open","");  break; // u16  Number of hours that DHW pump has been running or DHW valve has been opened 
          case DHWBurnerOperationHours:   print_u16(OTdata.DHWBurnerOperationHours,               "Nr of hours burner operation DHW","");  break; // u16  Number of hours that burner is in operation during DHW mode 
          case MasterVersion:             print_u8u8(OTdata.MasterVersion,                        "MasterVersion (version/type)","" ); break;// u8 / u8  Master product version number and type 
          case SlaveVersion:              print_u8u8(OTdata.SlaveVersion,                         "SlaveVersion  (version/type)", ""); break;// u8 / u8  Slave product version number and type
        }
      } 
      Debugln(); 
    }
  }   // while Serial.available()
}

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
