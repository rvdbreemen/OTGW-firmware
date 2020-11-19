/* 
***************************************************************************  
**  Program  : OTGWStuff
**  Version  : v0.2.0
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

//some variable's
OpenthermData OTdata;
DECLARE_TIMER_MS(timerWD, 1000, CATCH_UP_MISSED_TICKS);

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

  if DUE(timerWD)
  {
    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
    Wire.write(0xA5);                             //Feed the dog, before it bites.
    Wire.endTransmission();                       //That's all there is...
  }
  yield();

  //==== feed the WD over I2C ==== 
}

//===================[ END Watchdog OTGW ]===============================

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
	    case Command:                   return "Remote Command"; // u8 / u8  Remote Command 
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

const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1) {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}


void print_f88(float _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = round(OTdata.f88()*100.0) / 100.0; // round float 2 digits, like this: x.xx 
  Debugf("%-37s = %3.2f %s\r\n", _label, _value , _unit);
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  Debugf("%-37s = %s %s\r\n", _label, _msg , _unit);
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
}

void print_s16(int16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.s16();     
  Debugf("%-37s = %5d %s\r\n", _label, _value, _unit);
  //Build string for MQTT
  char _msg[15] {0};
  itoa(_value, _msg, 10);
  Debugf("%-37s = %s %s\r\n", _label, _msg, _unit);
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
}

void print_s8s8(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
  Debugf("%-37s = %3d / %3d %s\r\n", _label, (int8_t)OTdata.valueHB, (int8_t)OTdata.valueLB, _unit);
  //Build string for MQTT
  char _msg[15] {0};
  char _topic[50] {0};
  itoa((int8_t)OTdata.valueHB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_value_hb", sizeof(_topic));
  Debugf("%-37s = %s %s\r\n", _label, _msg, _unit);
  sendMQTTData(_topic, _msg);
  //Build string for MQTT
  itoa((int8_t)OTdata.valueLB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_value_lb", sizeof(_topic));
  Debugf("%-37s = %s %s\r\n", _label, _msg, _unit);
  sendMQTTData(_topic, _msg);
}


void print_u16(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
  //Build string for MQTT
  char _msg[15] {0};
  utoa(_value, _msg, 10);
  Debugf("%-37s = %s %s\r\n", _label, _msg, _unit);
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
}

void print_status(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
  
  char _flag8_master[8] {0};
  char _flag8_slave[8] {0};
    //bit: [clear/0, set/1]
  //  0: CH enable [ CH is disabled, CH is enabled]
  //  1: DHW enable [ DHW is disabled, DHW is enabled]
  //  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
  //  3: OTC active [OTC not active, OTC is active]
  //  4: CH2 enable [CH2 is disabled, CH2 is enabled]
  //  5: reserved
  //  6: reserved
  //  7: reserved
  _flag8_master[0] = (((OTdata.valueHB) & 0x01) ? 'C' : '-');
  _flag8_master[1] = (((OTdata.valueHB) & 0x02) ? 'D' : '-');
  _flag8_master[2] = (((OTdata.valueHB) & 0x04) ? 'C' : '-'); 
  _flag8_master[3] = (((OTdata.valueHB) & 0x08) ? 'O' : '-');
  _flag8_master[4] = (((OTdata.valueHB) & 0x10) ? '2' : '-'); 
  _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? '.' : '-'); 
  _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? '.' : '-'); 
  _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
  _flag8_master[8] = '\0';

  Debugf("%-37s = M[%s] \r\n", _label, _flag8_master);
  //Master Status
  sendMQTTData("status_master", _flag8_master);
  sendMQTTData("ch_enable",             (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));
  sendMQTTData("dhw_enable",            (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));
  sendMQTTData("cooling_enable",        (((OTdata.valueHB) & 0x04) ? "ON" : "OFF")); 
  sendMQTTData("otc_active",            (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));
  sendMQTTData("ch2_enable",            (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));

  //Slave
  //  0: fault indication [ no fault, fault ]
  //  1: CH mode [CH not active, CH active]
  //  2: DHW mode [ DHW not active, DHW active]
  //  3: Flame status [ flame off, flame on ]
  //  4: Cooling status [ cooling mode not active, cooling mode active ]
  //  5: CH2 mode [CH2 not active, CH2 active]
  //  6: diagnostic indication [no diagnostics, diagnostic event]
  //  7: reserved
  _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'E' : '-');
  _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'C' : '-'); 
  _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'W' : '-'); 
  _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'F' : '-'); 
  _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'C' : '-'); 
  _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '2' : '-'); 
  _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-'); 
  _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? '.' : '-');
  _flag8_slave[8] = '\0';

  DebugTf("%-37s = S[%s] \r\n", _label, _flag8_slave);

  //Slave Status
  sendMQTTData("status_slave", _flag8_slave);
  sendMQTTData("fault",                 (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  
  sendMQTTData("centralheating",        (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));  
  sendMQTTData("domestichotwater",      (((OTdata.valueLB) & 0x04) ? "ON" : "OFF"));  
  sendMQTTData("flame",                 (((OTdata.valueLB) & 0x08) ? "ON" : "OFF"));
  sendMQTTData("cooling",               (((OTdata.valueLB) & 0x10) ? "ON" : "OFF"));  
  sendMQTTData("centralheating2",       (((OTdata.valueLB) & 0x20) ? "ON" : "OFF"));
  sendMQTTData("diagnostic_indicator",  (((OTdata.valueLB) & 0x40) ? "ON" : "OFF"));
}

void print_ASFflags(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
 
  Debugf("%-37s = M[%s] OEM fault code [%3d]\r\n", _label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  //Build string for MQTT
  char _msg[15] {0};
  //Application Specific Fault
  sendMQTTData("ASF_flags", byte_to_binary(OTdata.valueHB));
  //OEM fault code
  sendMQTTData("ASF_oemfaultcode", _msg);

  //bit: [clear/0, set/1]
    //bit: [clear/0, set/1]
  //0: Service request [service not req’d, service required]
  //1: Lockout-reset [ remote reset disabled, rr enabled]
  //2: Low water press [ no WP fault, water pressure fault]
  //3: Gas/flame fault [ no G/F fault, gas/flame fault ]
  //4: Air press fault [ no AP fault, air pressure fault ]
  //5: Water over-temp[ no OvT fault, over-temperat. Fault]
  //6: reserved
  //7: reserved
  sendMQTTData("service_request",       (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
  sendMQTTData("lockout_reset",         (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  
  sendMQTTData("low_water_pressure",    (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  
  sendMQTTData("gas_flame_fault",       (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));
  sendMQTTData("air_pressure_fault",    (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));  
  sendMQTTData("water_over-temperature",(((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));
}


void print_slavememberid(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
 
  Debugf("%-37s = Slave Config[%s] MemberID code [%3d]\r\n", _label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  //Build string for SendMQTT
  sendMQTTData("slave_configuration", byte_to_binary(OTdata.valueHB));
  char _msg[15] {0};
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData("slave_memberid_code", _msg);

  
  // bit: description  [ clear/0, set/1] 
  // 0:  DHW present  [ dhw not present, dhw is present ] 
  // 1:  Control type  [ modulating, on/off ] 
  // 2:  Cooling config  [ cooling not supported,  
  //     cooling supported] 
  // 3:  DHW config  [instantaneous or not-specified, 
  //     storage tank] 
  // 4:  Master low-off&pump control function [allowed, 
  //     not allowed] 
  // 5:  CH2 present  [CH2 not present, CH2 present] 
  // 6:  reserved 
  // 7:  reserved 
  sendMQTTData("dhw_present",                             (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
  sendMQTTData("control_type",                            (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  
  sendMQTTData("cooling_config",                          (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  
  sendMQTTData("dhw_config",                              (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));
  sendMQTTData("master_low_off_pomp_control_function",    (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));  
  sendMQTTData("ch2_present",                             (((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));
}

void print_mastermemberid(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
 
  Debugf("%-37s = Master Config[%s] MemberID code [%3d]\r\n", _label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  //Build string for MQTT
  char _msg[15] {0};
  sendMQTTData("master_configuration", byte_to_binary(OTdata.valueHB));
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData("master_memberid_code", _msg);


}

void print_flag8u8(uint16_t _value, const char *_label, const char*_unit)
{
  _value = OTdata.u16();  

  Debugf("%-37s = M[%s] - [%3d]\r\n", _label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  //Build string for MQTT
  char _topic[50] {0};
  //flag8 value
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  //u8 value
  char _msg[15] {0};
  utoa(OTdata.valueLB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_code", sizeof(_topic));
  sendMQTTData(_topic, _msg);
}



void print_flag8(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
  
  Debugf("%-37s = flag8 = [%s] - decimal = [%3d]\r\n", _label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

  //Build string for MQTT
  char _topic[50] {0};
  //flag8 value
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
}

void print_flag8flag8(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
  
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 valueHB
  Debugf("%-37s = HB flag8[%s] -[%3d]\r\n", _label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  //flag8 valueLB
  Debugf("%-37s = LB flag8[%s] - [%3d]\r\n", _label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
}

void print_u8u8(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  _value = OTdata.u16();     
  Debugf("%-37s = %3d / %3d %s\r\n", _label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, _unit);
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //flag8 valueHB
  utoa((OTdata.valueHB), _msg, 10);
  Debugf("%-37s = HB u8[%s] [%3d]\r\n", _label, _msg, OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_u8", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  //flag8 valueLB
  utoa((OTdata.valueLB), _msg, 10);
  Debugf("%-37s = LB u8[%s] [%3d]\r\n", _label, _msg, OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_u8", sizeof(_topic));
  sendMQTTData(_topic, _msg);
}

void print_daytime(uint16_t _value, const char *_label, const char*_unit)
{
  //function to print data
  const char *dayOfWeekName[]  { "Unknown", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag", "Zondag", "Unknown" };
  _value = OTdata.u16();     
  Debugf("%-37s = %s - %2d:%2d\r\n", _label, dayOfWeekName[(OTdata.valueHB >> 5) & 0x7], (OTdata.valueHB & 0x1F), OTdata.valueLB); 
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //dayofweek
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_dayofweek", sizeof(_topic));
  sendMQTTData(_topic, dayOfWeekName[(OTdata.valueHB >> 5) & 0x7]);
  //dayofweek
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hour", sizeof(_topic));
  sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10)); 
  //dayofweek
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_minutes", sizeof(_topic));
  sendMQTTData(_topic, itoa(OTdata.valueLB, _msg, 10)); 
}
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

//===================[ Send buffer to OTGW ]=============================

int sendOTGW(const char* buf, int len)
{
  //Just send the buffer to OTGW when the Serial interface is available
  if (Serial) {
    //check the write buffer
    if (Serial.availableForWrite()>= (len+2)) {
      //write buffer to serial
      // Debugf("Sending command OTGW to [%s]", buf);
      Serial.write(buf, len);
      // Serial.write("PS=0\r\n");
      Serial.write("\r\n");
    } else Debugln("Error: Write buffer not big enough!");
  } else Debugln("Error: Serial device not found!");
}

//===================[ Handle OTGW ]=====================================
void handleOTGW_1(){
//let's try this, read all data from the serial device, and dump it to the telnet stream.
  while(Serial.available() > 0) 
  { 
    feedWatchDog(); //let's make sure we don't get bit
    char rIn = Serial.read();       
    TelnetStream.write((char)rIn);
    //DebugTf("[%s] [%d]\r\n", strBuffer.c_str(), strBuffer.length());
  }
}

void handleOTGW_2(){
//let's try this, read all data from the serial device, and dump it to the telnet stream.
  while(Serial.available() > 0) 
  { 
    feedWatchDog(); //let's make sure we don't get bit
    String strBuffer = Serial.readStringUntil('\n');      
    TelnetStream.write((char *)strBuffer.c_str(), strBuffer.length());
    TelnetStream.write('\n');
    //DebugTf("[%s] [%d]\r\n", strBuffer.c_str(), strBuffer.length());
  }
}

void handleOTGW(){
  //let's try this, read all data from the serial device, and dump it to the telnet stream.
  while(Serial.available() > 0) 
  { 
    feedWatchDog(); //let's make sure we don't get bit
    String strBuffer = Serial.readStringUntil('\n');
    strBuffer.trim(); //remove LF and CR (and whitespaces)

    DebugTf("[%s] [%d]\r\n", strBuffer.c_str(), strBuffer.length());
    
    if (strBuffer.length()>=9) {
      DebugTf("%s ", strBuffer.c_str());
      //parse value
      uint32_t value = strtoul(strBuffer.substring(1).c_str(), NULL, 16);
      // DebugTf("Value=[%08x]\r\n", (uint32_t)value);
      //processing message
      // if (strBuffer.charAt(0)=='B')
      // {
      //   DebugT("Boiler           ");
      // } else 
      // if (strBuffer.charAt(0)=='T')
      // {
      //   DebugT("Thermostat       ");
      // } else
      // if (strBuffer.charAt(0)=='R')
      // {
      //   DebugT("Request Boiler   ");
      // } else      
      // if (strBuffer.charAt(0)=='A')
      // {
      //   DebugT("Answer Themostat ");
      // } else      
      // if (strBuffer.charAt(0)=='E')
      // {
      //   DebugT("Parity error     ");
      // } else
      // {
      //   DebugTf("Unexpected=[%c] ", strBuffer.charAt(0));
      // }
      //Debugf("msg=[%s] value=[%08x]", strBuffer.c_str(), value);

      //split 32bit value into the relevant OT protocol parts
      OTdata.type = (value >> 28) & 0x7;         // byte 1 = take 3 bits that define msg msgType
      OTdata.id = (value >> 16) & 0xFF;          // byte 2 = message id 8 bits 
      OTdata.valueHB = (value >> 8) & 0xFF;      // byte 3 = high byte
      OTdata.valueLB = value & 0xFF;             // byte 4 = low byte

      //print message frame
      //Debugf("\ttype[%3d] id[%3d] hb[%3d] lb[%3d]\t", OTdata.type, OTdata.id, OTdata.valueHB, OTdata.valueLB);

      //print message Type and ID
      Debugf("[%-16s]\t", messageTypeToString(static_cast<OpenThermMessageType>(OTdata.type)));
      Debugf("[%-30s]\t", messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)));
      DebugFlush();

      //next step interpret the OT protocol
      if (static_cast<OpenThermMessageType>(OTdata.type) == OT_READ_ACK || static_cast<OpenThermMessageType>(OTdata.type) == OT_WRITE_DATA) {

        //#define OTprint(data, value, text, format) ({ data= value; Debugf("[%37s]", text); Debugf("= [format]", data)})
        //interpret values f8.8
        switch (static_cast<OpenThermMessageID>(OTdata.id)) { 
          case TSet:                      print_f88(OTdataObject.Tset,                                "Control setpoint", "°C"); break;         
          case CoolingControl:            print_f88(OTdataObject.CoolingControl,                      "Cooling control signal","%");   break;
          case TsetCH2:                   print_f88(OTdataObject.TsetCH2,                             "Control setpoint for 2e circuit","°C");  break;
          case TrOverride:                print_f88(OTdataObject.TrOverride,                          "Remote override room setpoint","°C");  break;        
          case MaxRelModLevelSetting:     print_f88(OTdataObject.MaxRelModLevelSetting,               "Max Rel Modulation level setting","%");  break;
          case TrSet:                     print_f88(OTdataObject.TrSet,                               "Room setpoint","°C");  break;
          case RelModLevel:               print_f88(OTdataObject.RelModLevel,                         "Relative Modulation Level","%");  break;
          case CHPressure:                print_f88(OTdataObject.CHPressure,                          "Water pressure in CH circuit","bar"); break;
          case DHWFlowRate:               print_f88(OTdataObject.DHWFlowRate,                         "Water flow rate in DHW circuit","l/min");  break;
          case Tr:                        print_f88(OTdataObject.Tr,                                  "Room temperature","°C");  break;  
          case Tboiler:                   print_f88(OTdataObject.Tboiler,                             "Boiler flow water temperature","°C");  break;
          case Tdhw:                      print_f88(OTdataObject.Tdhw,                                "DHW temperature","°C");  break;
          case Toutside:                  print_f88(OTdataObject.Toutside,                            "Outside temperature","°C");  break;
          case Tret:                      print_f88(OTdataObject.Tret,                                "Return water temperature","°C");  break;
          case Tstorage:                  print_f88(OTdataObject.Tstorage,                            "Solar storage temperature","°C");  break;
          case Tcollector:                print_f88(OTdataObject.Tcollector,                          "Solar collector temperature","°C");  break;
          case TflowCH2:                  print_f88(OTdataObject.TflowCH2,                            "Flow water temperature CH2 cir.","°C");  break;          
          case Tdhw2:                     print_f88(OTdataObject.Tdhw2,                               "Domestic hot water temperature 2","°C");  break;
          case Texhaust:                  print_s16(OTdataObject.Texhaust,                            "Boiler exhaust temperature","°C");  break; // s16  Boiler exhaust temperature (°C)
          case TdhwSet:                   print_f88(OTdataObject.TdhwSet,                             "DHW setpoint","°C");  break;
          case MaxTSet:                   print_f88(OTdataObject.MaxTSet,                             "Max CH water setpoint","°C");  break;
          case Hcratio:                   print_f88(OTdataObject.Hcratio,                             "OTC heat curve ratio","");  break;
          case OpenThermVersionMaster:    print_f88(OTdataObject.OpenThermVersionMaster,              "Master OT protocol version","");  break;
          case OpenThermVersionSlave:     print_f88(OTdataObject.OpenThermVersionSlave,               "Slave OT protocol version","");  break;
          case Status:                    print_status(OTdataObject.Status,                           "Status",""); break;
          case ASFflags:                  print_ASFflags(OTdataObject.ASFflags,                       "Application Specific Fault",""); break;
          case MConfigMMemberIDcode:      print_mastermemberid(OTdataObject.MConfigMMemberIDcode,     "Master Config / Member ID", ""); break; // flag8 / u8  Master Configuration Flags /  Master MemberID Code 
          case SConfigSMemberIDcode:      print_slavememberid(OTdataObject.SConfigSMemberIDcode,      "Slave  Config / Member ID", ""); break; // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code  
          case Command:                   print_u8u8(OTdataObject.Command,                            "Command","");  break; // u8 / u8  Remote Command 
          case RBPflags:                  print_flag8flag8(OTdataObject.RBPflags,                     "RBPflags", ""); break; // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags 
          case TSP:                       print_u8u8(OTdataObject.TSP,                                "Nr of Transp. Slave Parameters", ""); break; // u8 / u8  Number of Transparent-Slave-Parameters supported by slave 
          case TSPindexTSPvalue:          print_u8u8(OTdataObject.TSPindexTSPvalue,                   "TSPindexTSPvalue", "");  break; // u8 / u8  Index number / Value of referred-to transparent slave parameter. 
          case FHBsize:                   print_u8u8(OTdataObject.FHBsize,                            "FHBsize", "");  break;  // u8 / u8  Size of Fault-History-Buffer supported by slave 
          case FHBindexFHBvalue:          print_u8u8(OTdataObject.FHBindexFHBvalue,                   "FHBindexFHBvalue", "");  break;  // u8 / u8  Index number / Value of referred-to fault-history buffer entry. 
          case MaxCapacityMinModLevel:    print_u8u8(OTdataObject.MaxCapacityMinModLevel,             "MaxCapacityMinModLevel", "");  break;  // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%) 
          case DayTime:                   print_daytime(OTdataObject.DayTime,                         "Day of Week - Daytime (hour/min)", "");  break; // special / u8  Day of Week and Time of Day 
          case Date:                      print_u8u8(OTdataObject.Date,                               "Date (Month/Day)", "");  break; // u8 / u8  Calendar date 
          case Year:                      print_u16(OTdataObject.Year,                                "Year","");  break; // u16  Calendar year 
          case TdhwSetUBTdhwSetLB:        print_s8s8(OTdataObject.TdhwSetUBTdhwSetLB,                 "TdhwSetUBTdhwSetLB", "°C"); break;  // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
          case MaxTSetUBMaxTSetLB:        print_s8s8(OTdataObject.MaxTSetUBMaxTSetLB,                 "MaxTSetUBMaxTSetLB", "°C"); break;  // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
          case HcratioUBHcratioLB:        print_s8s8(OTdataObject.HcratioUBHcratioLB,                 "HcratioUBHcratioLB", ""); break;  // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment  
          case RemoteOverrideFunction:    print_flag8(OTdataObject.RemoteOverrideFunction,            "RemoteOverrideFunction", ""); break; // u8 / flag8 -  Function of manual and program changes in master and remote room setpoint. 
          case OEMDiagnosticCode:         print_u16(OTdataObject.OEMDiagnosticCode,                   "OEM diagnostic/service code","");  break; // u16  OEM-specific diagnostic/service code 
          case BurnerStarts:              print_u16(OTdataObject.BurnerStarts,                        "Nr of starts burner","");  break; // u16  Number of starts burner 
          case CHPumpStarts:              print_u16(OTdataObject.CHPumpStarts,                        "Nr of starts CH pump","");  break; // u16  Number of starts CH pump 
          case DHWPumpValveStarts:        print_u16(OTdataObject.DHWPumpValveStarts,                  "Nr of starts DHW pump/valve","");  break; // u16  Number of starts DHW pump/valve
          case DHWBurnerStarts:           print_u16(OTdataObject.DHWBurnerStarts,                     "Nr of starts burner during DHW","");  break; // u16  Number of starts burner during DHW mode 
          case BurnerOperationHours:      print_u16(OTdataObject.BurnerOperationHours,                "Nr of hours burner operation","");  break; // u16  Number of hours that burner is in operation (i.e. flame on) 
          case CHPumpOperationHours:      print_u16(OTdataObject.CHPumpOperationHours,                "Nr of hours CH pump running","");  break; // u16  Number of hours that CH pump has been running 
          case DHWPumpValveOperationHours:print_u16(OTdataObject.DHWPumpValveOperationHours,          "Nr of hours DHW valve open","");  break; // u16  Number of hours that DHW pump has been running or DHW valve has been opened 
          case DHWBurnerOperationHours:   print_u16(OTdataObject.DHWBurnerOperationHours,             "Nr of hours burner operation DHW","");  break; // u16  Number of hours that burner is in operation during DHW mode 
          case MasterVersion:             print_u8u8(OTdataObject.MasterVersion,                      "MasterVersion (version/type)","" ); break;// u8 / u8  Master product version number and type 
          case SlaveVersion:              print_u8u8(OTdataObject.SlaveVersion,                       "SlaveVersion  (version/type)", ""); break;// u8 / u8  Slave product version number and type
        }
      } 
      Debugln(); 
    }
  }   // while Serial.available()
}

void startOTGWstream()
{
  OTGWstream.begin();
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
