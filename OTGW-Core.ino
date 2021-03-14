/* 
***************************************************************************  
**  Program  : OTGW-Core.ino
**  Version  : v0.8.1
**
**  Copyright (c) 2021 Robert van den Breemen
**  Borrowed from OpenTherm library from: 
**      https://github.com/jpraus/arduino-opentherm
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

//define Nodoshop OTGW hardware
#define OTGW_BUTTON 0   //D3
#define OTGW_RESET  14  //D5
#define OTGW_LED1   2   //D4
#define OTGW_LED2   16  //D0

//external watchdog 
#define EXT_WD_I2C_ADDRESS 0x26
#define PIN_I2C_SDA 4   //D2
#define PIN_I2C_SCL 5   //D1

//used by update firmware functions
const char *hexheaders[] = {
  "Last-Modified",
  "X-Version"
};

//Macro to Feed the Watchdog
#define FEEDWATCHDOGNOW   Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   Wire.write(0xA5);   Wire.endTransmission();

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

#define PRINTF_BINARY_PATTERN_INT16     PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i)  PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32     PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i)  PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64     PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i)  PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- Endf of macro's --- */

//some variable's
OpenthermData OTdata;

#define OTGW_BANNER "OpenTherm Gateway"

//===================[ Reset OTGW ]===============================
void resetOTGW() {
  OTGWSerial.resetPic();
  //then read the first response of the firmware to make sure it reads it
  String resp = OTGWSerial.readStringUntil('\n');
  resp.trim();
  DebugTf("Received firmware version: [%s] [%s] (%d)\r\n", CSTR(resp), OTGWSerial.firmwareVersion(), strlen(OTGWSerial.firmwareVersion()));
  bOTGWonline = (strlen(OTGWSerial.firmwareVersion())>0);
  if (bOTGWonline){
      if (resp.length()>0) {
        sPICfwversion = String(OTGWSerial.firmwareVersion());
      } else sPICfwversion ="No version found";
  } else sPICfwversion = "No OTGW connected!";
  DebugTf("Current firmware version: %s\r\n", CSTR(sPICfwversion));
}
//===================[ getpicfwversion ]===========================
String getpicfwversion(){
  String _ret="";

  String line = executeCommand("PR=A");
  int p = line.indexOf(OTGW_BANNER);
  if (p >= 0) {
    p += sizeof(OTGW_BANNER);
    _ret = line.substring(p);
  } else _ret ="No version found";
  DebugTf("Current firmware version: %s\r\n", CSTR(_ret));
  _ret.trim();
  return _ret;
}
//===================[ checkOTWGpicforupdate ]=====================
void checkOTWGpicforupdate(){
  DebugTf("OTGW PIC firmware version = [%s]\r\n", CSTR(sPICfwversion));
  String latest = checkforupdatepic("gateway.hex");
  if (!bOTGWonline) {
    sMessage = sPICfwversion; 
  } else if (latest != sPICfwversion) {
    sMessage = "New PIC version " + latest + " available!";
  }
}
//===================[ OTGW Command & Response ]===================
String executeCommand(const String sCmd){
  //send command to OTGW
  DebugTf("OTGW Send Cmd [%s]\r\n", CSTR(sCmd));
  OTGWSerial.setTimeout(1000);
  DECLARE_TIMER_MS(tmrWaitForIt, 1000);
  while((OTGWSerial.availableForWrite() < sCmd.length()+2) && !DUE(tmrWaitForIt)){
    feedWatchDog();
  }
  OTGWSerial.write(CSTR(sCmd));
  OTGWSerial.write("\r\n");
  OTGWSerial.flush();
  //wait for response
  RESTART_TIMER(tmrWaitForIt);
  while(!OTGWSerial.available() && !DUE(tmrWaitForIt)) {
    feedWatchDog();
  }
  String _cmd = sCmd.substring(0,2);
  DebugTf("Send command: [%s]\r\n", CSTR(_cmd));
  //fetch a line
  String line = OTGWSerial.readStringUntil('\n');
  line.trim();
  String _ret ="";
  if (line.startsWith(_cmd)){
    // Responses: When a serial command is accepted by the gateway, it responds with the two letters of the command code, a colon, and the interpreted data value.
    // Command:   "TT=19.125"
    // Response:  "TT: 19.13"
    //            [XX:response string]   
    _ret = line.substring(3);
  } else if (line.startsWith("NG")){
    _ret = "NG - No Good. The command code is unknown.";
  } else if (line.startsWith("SE")){
    _ret = "SE - Syntax Error. The command contained an unexpected character or was incomplete.";
  } else if (line.startsWith("BV")){
    _ret = "BV - Bad Value. The command contained a data value that is not allowed.";
  } else if (line.startsWith("OR")){
    _ret = "OR - Out of Range. A number was specified outside of the allowed range.";
  } else if (line.startsWith("NS")){
    _ret = "NS - No Space. The alternative Data-ID could not be added because the table is full.";
  } else if (line.startsWith("NF")){
    _ret = "NF - Not Found. The specified alternative Data-ID could not be removed because it does not exist in the table.";
  } else if (line.startsWith("OE")){
    _ret = "OE - Overrun Error. The processor was busy and failed to process all received characters.";
  } else if (line.length()==0) {
    //just an empty line... most likely it's a timeout situation
    _ret = "TO - Timeout. No response.";
  } else {
    _ret = line; //some commands return a string, just return that.
  } 
  DebugTf("Command send [%s]-[%s] - Response line: [%s] - Returned value: [%s]\r\n", CSTR(sCmd), CSTR(_cmd), CSTR(line), CSTR(_ret));
  return _ret;
}

//===================[ OTGW PS=1 Command ]===============================
void getOTGW_PS_1(){
  DebugTln("PS=1");
  OTGWSerial.write("PS=1\r\n");
  OTGWSerial.flush();

  while(!OTGWSerial.available()) {
    feedWatchDog();
  }
  String line = OTGWSerial.readStringUntil('\n');
  line.trim(); //remove LF and CR (and whitespaces)
  DebugTln(line);
  DebugTln("PS=0");
  OTGWSerial.write("PS=0\r\n");
  OTGWSerial.flush();
}
//===================[ OTGW PS=1 Command ]===============================
//===================[ Watchdog OTGW ]===============================
String initWatchDog() {
  // Hardware WatchDog is based on: 
  // https://github.com/rvdbreemen/ESPEasySlaves/tree/master/TinyI2CWatchdog
  // Code here is based on ESPEasy code, modified to work in the project.

  // configure hardware pins according to eeprom settings.
  DebugTln("Setup Watchdog");
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
  DECLARE_TIMER_MS(timerWD, 1000, CATCH_UP_MISSED_TICKS);
  if DUE(timerWD)
  {
    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
    Wire.write(0xA5);                             //Feed the dog, before it bites.
    Wire.endTransmission();                       //That's all there is...
    if (settingLEDblink) blinkLEDnow(LED1);
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
  if (message_id <= OT_MSGID_MAX) {
    return OTmap[message_id].label;
  } else return "Undefined";}

OpenThermMessageType getMessageType(unsigned long message)
{
    OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
    return msg_type;
}

OpenThermMessageID getDataID(unsigned long frame)
{
    return (OpenThermMessageID)((frame >> 16) & 0xFF);
}

//parsing responses - helper functions
//  0: CH enable [ CH is disabled, CH is enabled]
//  1: DHW enable [ DHW is disabled, DHW is enabled]
//  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
//  3: OTC active [OTC not active, OTC is active]
//  4: CH2 enable [CH2 is disabled, CH2 is enabled]
//  5: reserved
//  6: reserved
//  7: reserved

bool isCentralHeatingEnabled() {
	return OTdataObject.Status & 0x0100;
}

bool isDomesticHotWaterEnabled() {
	return OTdataObject.Status & 0x0200;
}

bool isCoolingEnabled() {
	return OTdataObject.Status & 0x0400;
}

bool isOutsideTemperatureCompensationActive() {
	return OTdataObject.Status & 0x0800;
}

bool isCentralHeating2enabled() {
	return OTdataObject.Status & 0x1000;
}

//Slave
//  0: fault indication [ no fault, fault ]
//  1: CH mode [CH not active, CH active]
//  2: DHW mode [ DHW not active, DHW active]
//  3: Flame status [ flame off, flame on ]
//  4: Cooling status [ cooling mode not active, cooling mode active ]
//  5: CH2 mode [CH2 not active, CH2 active]
//  6: diagnostic indication [no diagnostics, diagnostic event]
//  7: reserved

bool isFaultIndicator() {
	return OTdataObject.Status & 0x001;
}

bool isCentralHeatingActive() {
	return OTdataObject.Status & 0x002;
}

bool isDomesticHotWaterActive() {
	return OTdataObject.Status & 0x004;
}

bool isFlameStatus() {
	return OTdataObject.Status & 0x008;
}

bool isCoolingActive() {
	return OTdataObject.Status & 0x0010;
}

bool isCentralHeating2Active() {
	return OTdataObject.Status & 0x0020;
}


bool isDiagnosticIndicator() {
	return OTdataObject.Status & 0x0040;
}

  //bit: [clear/0, set/1]
  //0: Service request [service not req’d, service required]
  //1: Lockout-reset [ remote reset disabled, rr enabled]
  //2: Low water press [ no WP fault, water pressure fault]
  //3: Gas/flame fault [ no G/F fault, gas/flame fault ]
  //4: Air press fault [ no AP fault, air pressure fault ]
  //5: Water over-temp[ no OvT fault, over-temperat. Fault]
  //6: reserved
  //7: reserved

bool isServiceRequest() {
	return OTdataObject.ASFflags & 0x0100;
}

bool isLockoutReset() {
	return OTdataObject.ASFflags & 0x0200;
}

bool isLowWaterPressure() {
	return OTdataObject.ASFflags & 0x0400;
}

bool isGasFlameFault() {
	return OTdataObject.ASFflags & 0x0800;
}

bool isAirTemperature() {
	return OTdataObject.ASFflags & 0x1000;
}

bool isWaterOverTemperature() {
	return OTdataObject.ASFflags & 0x2000;
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


void print_f88(float *value)
{
  //function to print data
  float _value = round(OTdata.f88()*100.0) / 100.0; // round float 2 digits, like this: x.xx 
  // Debugf("%-37s = %3.2f %s\r\n", OTmap[OTdata.id].label, _value , OTmap[OTdata.id].unit);
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  Debugf("%-37s = %s %s\r\n", OTmap[OTdata.id].label, _msg , OTmap[OTdata.id].unit);
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
  (*value) = _value;
}

void print_s16(int16_t *value)
{    
  int16_t _value = OTdata.s16(); 
  // Debugf("%-37s = %5d %s\r\n", OTmap[OTdata.id].label, _value, OTmap[OTdata.id].unit);
  //Build string for MQTT
  char _msg[15] {0};
  itoa(_value, _msg, 10);
  Debugf("%-37s = %s %s\r\n", OTmap[OTdata.id].label, _msg, OTmap[OTdata.id].unit);
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
  (*value) = _value;
}

void print_s8s8(uint16_t *value)
{
  uint16_t _value = OTdata.u16();
  Debugf("%-37s = %3d / %3d %s\r\n", OTmap[OTdata.id].label, (int8_t)OTdata.valueHB, (int8_t)OTdata.valueLB, OTmap[OTdata.id].unit);
  //Build string for MQTT
  char _msg[15] {0};
  char _topic[50] {0};
  itoa((int8_t)OTdata.valueHB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_value_hb", sizeof(_topic));
  Debugf("%-37s = %s %s\r\n", OTmap[OTdata.id].label, _msg, OTmap[OTdata.id].unit);
  sendMQTTData(_topic, _msg);
  //Build string for MQTT
  itoa((int8_t)OTdata.valueLB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_value_lb", sizeof(_topic));
  Debugf("%-37s = %s %s\r\n", OTmap[OTdata.id].label, _msg, OTmap[OTdata.id].unit);
  sendMQTTData(_topic, _msg);
  (*value)=_value;
}


void print_u16(uint16_t *value)
{ 
  uint16_t _value = OTdata.u16(); 
  //Build string for MQTT
  char _msg[15] {0};
  utoa(_value, _msg, 10);
  Debugf("%-37s = %s %s\r\n", OTmap[OTdata.id].label, _msg, OTmap[OTdata.id].unit);
  //SendMQTT
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
  (*value) = _value;
}

void print_status(uint16_t *value)
{
  uint16_t _value=OTdata.u16();
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

  Debugf("%-37s = M[%s] \r\n", OTmap[OTdata.id].label, _flag8_master);
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

  DebugTf("%-37s = S[%s] \r\n", OTmap[OTdata.id].label, _flag8_slave);

  //Slave Status
  sendMQTTData("status_slave", _flag8_slave);
  sendMQTTData("fault",                 (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  
  sendMQTTData("centralheating",        (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));  
  sendMQTTData("domestichotwater",      (((OTdata.valueLB) & 0x04) ? "ON" : "OFF"));  
  sendMQTTData("flame",                 (((OTdata.valueLB) & 0x08) ? "ON" : "OFF"));
  sendMQTTData("cooling",               (((OTdata.valueLB) & 0x10) ? "ON" : "OFF"));  
  sendMQTTData("centralheating2",       (((OTdata.valueLB) & 0x20) ? "ON" : "OFF"));
  sendMQTTData("diagnostic_indicator",  (((OTdata.valueLB) & 0x40) ? "ON" : "OFF"));
  (*value)=_value;
}

void print_ASFflags(uint16_t *value)
{
  uint16_t _value=OTdata.u16();
  Debugf("%-37s = M[%s] OEM fault code [%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _msg[15] {0};
  //Application Specific Fault
  sendMQTTData("ASF_flags", byte_to_binary(OTdata.valueHB));
  //OEM fault code
  utoa(OTdata.valueLB, _msg, 10);
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
  (*value)=_value;
}


void print_slavememberid(uint16_t *value)
{
  uint16_t _value=OTdata.u16();
  Debugf("%-37s = Slave Config[%s] MemberID code [%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
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
  sendMQTTData("master_low_off_pump_control_function",    (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));  
  sendMQTTData("ch2_present",                             (((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));
  (*value)=_value;
}

void print_mastermemberid(uint16_t *value)
{
  uint16_t _value=OTdata.u16();
  Debugf("%-37s = Master Config[%s] MemberID code [%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _msg[15] {0};
  sendMQTTData("master_configuration", byte_to_binary(OTdata.valueHB));
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData("master_memberid_code", _msg);
  (*value)=_value;
}

void print_flag8u8(uint16_t *value)
{
  uint16_t _value = OTdata.u16();
  Debugf("%-37s = M[%s] - [%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
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
  (*value)=_value;
}

void print_flag8(uint16_t *value)
{
  uint16_t _value=OTdata.u16();
  Debugf("%-37s = flag8 = [%s] - decimal = [%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

  //Build string for MQTT
  char _topic[50] {0};
  //flag8 value
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
  (*value)=_value;
}

void print_flag8flag8(uint16_t *value)
{ 
  uint16_t _value=OTdata.u16();
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 valueHB
  Debugf("%-37s = HB flag8[%s] -[%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  //flag8 valueLB
  Debugf("%-37s = LB flag8[%s] - [%3d]\r\n", OTmap[OTdata.id].label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
  (*value)=_value;
}

void print_u8u8(uint16_t *value)
{ 
  uint16_t _value=OTdata.u16();
  Debugf("%-37s = %3d / %3d %s\r\n", OTmap[OTdata.id].label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTmap[OTdata.id].unit);
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //flag8 valueHB
  utoa((OTdata.valueHB), _msg, 10);
  Debugf("%-37s = HB u8[%s] [%3d]\r\n", OTmap[OTdata.id].label, _msg, OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_u8", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  //flag8 valueLB
  utoa((OTdata.valueLB), _msg, 10);
  Debugf("%-37s = LB u8[%s] [%3d]\r\n", OTmap[OTdata.id].label, _msg, OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_u8", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  (*value)=_value;
}

void print_date(uint16_t *value)
{ 
  uint16_t _value=OTdata.u16();
  Debugf("%-37s = %3d / %3d %s\r\n", OTmap[OTdata.id].label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTmap[OTdata.id].unit);
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //flag8 valueHB
  utoa((OTdata.valueHB), _msg, 10);
  Debugf("%-37s = HB u8[%s] [%3d]\r\n", OTmap[OTdata.id].label, _msg, OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_month", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  //flag8 valueLB
  utoa((OTdata.valueLB), _msg, 10);
  Debugf("%-37s = LB u8[%s] [%3d]\r\n", OTmap[OTdata.id].label, _msg, OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_day_of_month", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  (*value)=_value;
}

void print_daytime(uint16_t *value)
{
  uint16_t _value = OTdata.u16();    
  //function to print data
  const char *dayOfWeekName[]  { "Unknown", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag", "Zondag", "Unknown" };
  Debugf("%-37s = %s - %2d:%2d\r\n", OTmap[OTdata.id].label, dayOfWeekName[(OTdata.valueHB >> 5) & 0x7], (OTdata.valueHB & 0x1F), OTdata.valueLB); 
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //dayofweek
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_dayofweek", sizeof(_topic));
  sendMQTTData(_topic, dayOfWeekName[(OTdata.valueHB >> 5) & 0x7]);
  //hour
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hour", sizeof(_topic));
  sendMQTTData(_topic, itoa((OTdata.valueHB & 0x0F), _msg, 10)); 
  //min
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_minutes", sizeof(_topic));
  sendMQTTData(_topic, itoa((OTdata.valueLB), _msg, 10)); 
  (*value) = _value;
}


//===================[ Send buffer to OTGW ]=============================

int sendOTGW(const char* buf, int len)
{
  //Send the buffer to OTGW when the Serial interface is available
  if (OTGWSerial.availableForWrite()>=len+2) {
    //check the write buffer
    //Debugf("Serial Write Buffer space = [%d] - needed [%d]\r\n",OTGWSerial.availableForWrite(), (len+2));
    DebugT("Sending to Serial [");
    for (int i = 0; i < len; i++) {
      Debug((char)buf[i]);
    }
    Debug("] ("); Debug(len); Debug(")"); Debugln();
    
    while (OTGWSerial.availableForWrite()==(len+2)) {
      //cannot write, buffer full, wait for some space in serial out buffer
      feedWatchDog();     //this yields for other processes
    }

    if (OTGWSerial.availableForWrite()>= (len+2)) {
      //write buffer to serial
      OTGWSerial.write(buf, len);
      // OTGWSerial.write("PS=0\r\n");
      OTGWSerial.write('\r');
      OTGWSerial.write('\n');
      OTGWSerial.flush(); 
    } else Debugln("Error: Write buffer not big enough!");
  } else Debugln("Error: Serial device not found!");
}

/*
  This function checks if the string received is a valid "raw OT message".
  Raw OTmessages are 9 chars long and start with TBARE when talking to OTGW PIC.
*/
bool isvalidotmsg(const char *buf, int len){
  char *chk = "TBARE";
  bool _ret =  (len==9);
  _ret &= (strchr(chk, buf[0])!=NULL);
  return _ret;
}

/*
  Process OTGW messages coming from the PIC.
  It knows about:
  - raw OTmsg format
  - error format
  - ...
*/
void processOTGW(const char * buf, int len){ 
  if (isvalidotmsg(buf, len)) { 
    //OT protocol messages are 9 chars long
    if (settingMQTTOTmessage) sendMQTTData("otmessage", buf);
    // source of otmsg
    if (buf[0]=='B')
    {
      DebugT("Boiler           ");
    } else if (buf[0]=='T')
    {
      DebugT("Thermostat       ");
    } else if (buf[0]=='R')
    {
      DebugT("Request Boiler   ");
    } else if (buf[0]=='A')
    {
      DebugT("Answer Themostat ");
    } else if (buf[0]=='E')
    {
      DebugT("Parity error     ");
    } 

    const char *bufval = buf + 1;
    uint32_t value = strtoul(bufval, NULL, 16);
    Debugf("msg=[%s] value=[%08x]", bufval, value);

    //split 32bit value into the relevant OT protocol parts
    OTdata.type = (value >> 28) & 0x7;         // byte 1 = take 3 bits that define msg msgType
    OTdata.id = (value >> 16) & 0xFF;          // byte 2 = message id 8 bits 
    OTdata.valueHB = (value >> 8) & 0xFF;      // byte 3 = high byte
    OTdata.valueLB = value & 0xFF;             // byte 4 = low byte

    //print message frame
    Debugf("\ttype[%3d] id[%3d] hb[%3d] lb[%3d]\t", OTdata.type, OTdata.id, OTdata.valueHB, OTdata.valueLB);

    //print message Type and ID
    Debugf("[%-16s]\t", messageTypeToString(static_cast<OpenThermMessageType>(OTdata.type)));
    Debugf("[%-30s]\t", messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)));
    DebugFlush();

    //keep track of update
    msglastupdated[OTdata.id] = now();

    //next step interpret the OT protocol
    if (static_cast<OpenThermMessageType>(OTdata.type) == OT_READ_ACK || static_cast<OpenThermMessageType>(OTdata.type) == OT_WRITE_DATA) {

      //#define OTprint(data, value, text, format) ({ data= value; Debugf("[%37s]", text); Debugf("= [format]", data)})
      //interpret values f8.8
      switch (static_cast<OpenThermMessageID>(OTdata.id)) {   
        case TSet:                          print_f88(&OTdataObject.TSet); break;         
        case CoolingControl:                print_f88(&OTdataObject.CoolingControl ); break;
        case TsetCH2:                       print_f88(&OTdataObject.TsetCH2);  break;
        case TrOverride:                    print_f88(&OTdataObject.TrOverride);  break;        
        case MaxRelModLevelSetting:         print_f88(&OTdataObject.MaxRelModLevelSetting);  break;
        case TrSet:                         print_f88(&OTdataObject.TrSet);  break;
        case TrSetCH2:                      print_f88(&OTdataObject.TrSetCH2);  break;
        case RelModLevel:                   print_f88(&OTdataObject.RelModLevel);  break;
        case CHPressure:                    print_f88(&OTdataObject.CHPressure); break;
        case DHWFlowRate:                   print_f88(&OTdataObject.DHWFlowRate);  break;
        case Tr:                            print_f88(&OTdataObject.Tr); break;  
        case Tboiler:                       print_f88(&OTdataObject.Tboiler);  break;
        case Tdhw:                          print_f88(&OTdataObject.Tdhw);  break;
        case Toutside:                      print_f88(&OTdataObject.Toutside);  break;
        case Tret:                          print_f88(&OTdataObject.Tret);  break;
        case Tstorage:                      print_f88(&OTdataObject.Tstorage);  break;
        case Tcollector:                    print_f88(&OTdataObject.Tcollector ); break;
        case TflowCH2:                      print_f88(&OTdataObject.TflowCH2); break;          
        case Tdhw2:                         print_f88(&OTdataObject.Tdhw2); break;
        case Texhaust:                      print_s16(&OTdataObject.Texhaust);  break; 
        case TdhwSet:                       print_f88(&OTdataObject.TdhwSet); break;
        case MaxTSet:                       print_f88(&OTdataObject.MaxTSet); break;
        case Hcratio:                       print_f88(&OTdataObject.Hcratio); break;
        case OpenThermVersionMaster:        print_f88(&OTdataObject.OpenThermVersionMaster); break;
        case OpenThermVersionSlave:         print_f88(&OTdataObject.OpenThermVersionSlave); break;
        case Status:                        print_status(&OTdataObject.Status); break;
        case ASFflags:                      print_ASFflags(&OTdataObject.ASFflags); break;
        case MConfigMMemberIDcode:          print_mastermemberid(&OTdataObject.MConfigMMemberIDcode); break; 
        case SConfigSMemberIDcode:          print_slavememberid(&OTdataObject.SConfigSMemberIDcode); break;   
        case Command:                       print_u8u8(&OTdataObject.Command);  break; 
        case RBPflags:                      print_flag8flag8(&OTdataObject.RBPflags); break; 
        case TSP:                           print_u8u8(&OTdataObject.TSP); break; 
        case TSPindexTSPvalue:              print_u8u8(&OTdataObject.TSPindexTSPvalue);  break; 
        case FHBsize:                       print_u8u8(&OTdataObject.FHBsize);  break;  
        case FHBindexFHBvalue:              print_u8u8(&OTdataObject.FHBindexFHBvalue);  break; 
        case MaxCapacityMinModLevel:        print_u8u8(&OTdataObject.MaxCapacityMinModLevel);  break; 
        case DayTime:                       print_daytime(&OTdataObject.DayTime);  break; 
        case Date:                          print_date(&OTdataObject.Date);  break; 
        case Year:                          print_u16(&OTdataObject.Year);  break; 
        case TdhwSetUBTdhwSetLB:            print_s8s8(&OTdataObject.TdhwSetUBTdhwSetLB); break;  
        case MaxTSetUBMaxTSetLB:            print_s8s8(&OTdataObject.MaxTSetUBMaxTSetLB); break;  
        case HcratioUBHcratioLB:            print_s8s8(&OTdataObject.HcratioUBHcratioLB ); break;  
        case RemoteOverrideFunction:        print_flag8(&OTdataObject.RemoteOverrideFunction); break;
        case OEMDiagnosticCode:             print_u16(&OTdataObject.OEMDiagnosticCode);  break;
        case BurnerStarts:                  print_u16(&OTdataObject.BurnerStarts);  break; 
        case CHPumpStarts:                  print_u16(&OTdataObject.CHPumpStarts );  break; 
        case DHWPumpValveStarts:            print_u16(&OTdataObject.DHWPumpValveStarts);  break; 
        case DHWBurnerStarts:               print_u16(&OTdataObject.DHWBurnerStarts );  break;
        case BurnerOperationHours:          print_u16(&OTdataObject.BurnerOperationHours);  break;
        case CHPumpOperationHours:          print_u16(&OTdataObject.CHPumpOperationHours);  break; 
        case DHWPumpValveOperationHours:    print_u16(&OTdataObject.DHWPumpValveOperationHours);  break;  
        case DHWBurnerOperationHours:       print_u16(&OTdataObject.DHWBurnerOperationHours);  break; 
        case MasterVersion:                 print_u8u8(&OTdataObject.MasterVersion); break; 
        case SlaveVersion:                  print_u8u8(&OTdataObject.SlaveVersion); break;
        case StatusVH:                      print_flag8flag8(&OTdataObject.StatusVH); break;
		    case ControlSetpointVH:             print_u8u8(&OTdataObject.ControlSetpointVH); break;
        case FaultFlagsCodeVH:              print_flag8u8(&OTdataObject.FaultFlagsCodeVH ); break;
		    case DiagnosticCodeVH:              print_u16(&OTdataObject.DiagnosticCodeVH); break;
        case ConfigMemberIDVH:              print_flag8u8(&OTdataObject.ConfigMemberIDVH); break;
		    case OpenthermVersionVH:            print_f88(&OTdataObject.OpenthermVersionVH); break;
		    case VersionTypeVH:                 print_u8u8(&OTdataObject.VersionTypeVH); break;
		    case RelativeVentilation:           print_u8u8(&OTdataObject.RelativeVentilation); break;
	      case RelativeHumidityVH:            print_u8u8(&OTdataObject.RelativeHumidityVH ); break;
		    case CO2LevelVH:                    print_u16(&OTdataObject.CO2LevelVH ); break;
 		    case SupplyInletTemperature:        print_f88(&OTdataObject.SupplyInletTemperature); break;
 		    case SupplyOutletTemperature:       print_f88(&OTdataObject.SupplyOutletTemperature); break;
 		    case ExhaustInletTemperature:       print_f88(&OTdataObject.ExhaustInletTemperature); break;
 		    case ExhaustOutletTemperature:      print_f88(&OTdataObject.ExhaustOutletTemperature); break;
        case ActualExhaustFanSpeed:         print_u16(&OTdataObject.ActualExhaustFanSpeed); break;
		    case ActualInletFanSpeed:           print_u16(&OTdataObject.ActualInletFanSpeed); break;
		    case RemoteParameterSettingVH:      print_flag8flag8(&OTdataObject.RemoteParameterSettingVH); break;
		    case NominalVentilationValue:       print_u8u8(&OTdataObject.NominalVentilationValue); break;
        case TSPNumberVH:                   print_u8u8(&OTdataObject.TSPNumberVH); break;
		    case TSPEntryVH:                    print_u8u8(&OTdataObject.TSPEntryVH ); break;
		    case FaultBufferSizeVH:             print_u8u8(&OTdataObject.FaultBufferSizeVH); break;
		    case FaultBufferEntryVH:            print_u8u8(&OTdataObject.FaultBufferEntryVH); break;
        case FanSpeed:                      print_u16(&OTdataObject.FanSpeed); break;
        case ElectricalCurrentBurnerFlame:  print_f88(&OTdataObject.ElectricalCurrentBurnerFlame ); break;
	      case TRoomCH2:                      print_f88(&OTdataObject.TRoomCH2); break;
        case RelativeHumidity:              print_u8u8(&OTdataObject.RelativeHumidity); break;
        case RFstrengthbatterylevel:        print_u8u8(&OTdataObject.RFstrengthbatterylevel ); break;
	      case OperatingMode_HC1_HC2_DHW:     print_u8u8(&OTdataObject.OperatingMode_HC1_HC2_DHW ); break; 
        case ElectricityProducerStarts:     print_u16(&OTdataObject.ElectricityProducerStarts); break;
	      case ElectricityProducerHours:      print_u16(&OTdataObject.ElectricityProducerHours ); break;
	      case ElectricityProduction:         print_u16(&OTdataObject.ElectricityProduction); break;
        case CumulativElectricityProduction:print_u16(&OTdataObject.CumulativElectricityProduction); break;
        case RemehadFdUcodes:               print_u8u8(&OTdataObject.RemehadFdUcodes); break;
	      case RemehaServicemessage:          print_u8u8(&OTdataObject.RemehaServicemessage); break;
        case RemehaDetectionConnectedSCU:   print_u8u8(&OTdataObject.RemehaDetectionConnectedSCU); break;
      }
    } else Debugln(); //next line 
  } else if (strstr(buf, "Error 01")!= NULL) {
    OTdataObject.error01++;
    DebugTf("Error 01 = %d\r\n",OTdataObject.error01);
    sendMQTTData("Error 01", String(OTdataObject.error01));
  } else if (strstr(buf, "Error 02")!= NULL) {
    OTdataObject.error02++;
    DebugTf("Error 02 = %d\r\n",OTdataObject.error02);
    sendMQTTData("Error 02", String(OTdataObject.error02));
  } else if (strstr(buf, "Error 03")!= NULL) {
    OTdataObject.error03++;
    DebugTf("Error 03 = %d\r\n",OTdataObject.error03);
    sendMQTTData("Error 03", String(OTdataObject.error03));
  } else if (strstr(buf, "Error 04")!= NULL){
    OTdataObject.error04++;
    DebugTf("Error 04 = %d\r\n",OTdataObject.error04);
    sendMQTTData("Error 04", String(OTdataObject.error04));
  } else DebugTf("Unexpected received from OTGW => [%s] [%d]\r\n", buf, len);
 
}


//====================[ HandleOTGW ]====================
/*  
** This is the core of the OTGW firmware. 
** This code basically reads from serial, connected to the OTGW hardware, and
** processes each OT message coming. It can also be used to send data into the
** OpenTherm Gateway. 
**
** The main purpose is to read each OT Msg (9 bytes), or anything that comes 
** from the OTGW hardware firmware. Default it assumes raw OT messages, however
** it can handle the other messages to, like PS=1/PS=0 etc.
** 
** Also, this code bit implements the serial 2 network (port 25238). The serial port 
** is forwarded to port 25238, and visavera. So you can use it with OTmonitor (the 
** original OpenTherm program that comes with the hardware). The serial port and 
** ser2net port 25238 are both "line read" into the read buffer (coming from OTGW 
** thru serial) and write buffer  (coming from 25238 going to serial).
**
** The write buffer (incoming from port 25238) is also line printed to the Debug (port 23).
** The read line buffer is per line parsed by the proces OT parser code (processOTGW (buf, len)).
*/
void handleOTGW()
{
  //handle serial communication and line processing
  #define MAX_BUFFER_READ 256
  #define MAX_BUFFER_WRITE 128
  static char sRead[MAX_BUFFER_READ];
  static char sWrite[MAX_BUFFER_WRITE];
  static size_t bytes_read = 0;
  static size_t bytes_write = 0;
  static uint8_t inByte;
  static uint8_t outByte;

  //handle incoming data from network (port 25238) sent to serial port OTGW (WRITE BUFFER)
  while (OTGWstream.available()){
    //OTGWSerial.write(OTGWstream.read()); //just forward it directly to Serial
    outByte = OTGWstream.read();  // read from port 25238
    while (OTGWSerial.availableForWrite()==0) {
      //cannot write, buffer full, wait for some space in serial out buffer
      feedWatchDog();     //this yields for other processes
    }
    OTGWSerial.write(outByte);        // write to serial port
    OTGWSerial.flush();               // wait for write to serial
    if (outByte == '\n')
    { //on newline, do something...
      sWrite[bytes_write] = 0;
      DebugTf("Net2Ser: Sending to OTGW: [%s] (%d)\r\n", sWrite, bytes_write);
      //check for reset command
      if (stricmp(sWrite, "GW=R")==0){
        //detect [GW=R], then reset the gateway the gpio way
        DebugTln("Detected: GW=R. Reset gateway command executed.");
        resetOTGW();
      }
      bytes_write = 0; //start next line
    } else if  (outByte == '\r')
    {
      // skip LF
    } 
    else 
    {
      if (bytes_write < (MAX_BUFFER_WRITE-1))
        sWrite[bytes_write++] = outByte;
    }
  }
  
  //Handle incoming data from OTGW through serial port (READ BUFFER)
  while(OTGWSerial.available()) 
  {
    inByte = OTGWSerial.read();   // read from serial port
    OTGWstream.write(inByte); // write to port 25238
    if (inByte== '\n')
    { //line terminator, continue to process incoming message
      sRead[bytes_read] = 0;
      blinkLEDnow(LED2);
      processOTGW(sRead, bytes_read);
      bytes_read = 0;
      break; // to continue processing incoming message
    } 
    else if (inByte == '\r')
    { // just ignore LF... 
    } 
    else
    {
      if (bytes_read < (MAX_BUFFER_READ-1))
        sRead[bytes_read++] = inByte;
    }
  }
  
}// END of handleOTGW

//====================[ functions for REST API ]====================
String getOTGWValue(int msgid)
{
  switch (static_cast<OpenThermMessageID>(msgid)) { 
    case TSet:                              return String(OTdataObject.TSet); break;         
    case CoolingControl:                    return String(OTdataObject.CoolingControl); break;
    case TsetCH2:                           return String(OTdataObject.TsetCH2);  break;
    case TrOverride:                        return String(OTdataObject.TrOverride);  break;        
    case MaxRelModLevelSetting:             return String(OTdataObject.MaxRelModLevelSetting);  break;
    case TrSet:                             return String(OTdataObject.TrSet);  break;
    case TrSetCH2:                          return String(OTdataObject.TrSetCH2);  break;
    case RelModLevel:                       return String(OTdataObject.RelModLevel);  break;
    case CHPressure:                        return String(OTdataObject.CHPressure); break;
    case DHWFlowRate:                       return String(OTdataObject.DHWFlowRate);  break;
    case Tr:                                return String(OTdataObject.Tr);  break;  
    case Tboiler:                           return String(OTdataObject.Tboiler);  break;
    case Tdhw:                              return String(OTdataObject.Tdhw);  break;
    case Toutside:                          return String(OTdataObject.Toutside);  break;
    case Tret:                              return String(OTdataObject.Tret);  break;
    case Tstorage:                          return String(OTdataObject.Tstorage);  break;
    case Tcollector:                        return String(OTdataObject.Tcollector); break;
    case TflowCH2:                          return String(OTdataObject.TflowCH2); break;          
    case Tdhw2:                             return String(OTdataObject.Tdhw2); break;
    case Texhaust:                          return String(OTdataObject.Texhaust); break; 
    case TdhwSet:                           return String(OTdataObject.TdhwSet); break;
    case MaxTSet:                           return String(OTdataObject.MaxTSet); break;
    case Hcratio:                           return String(OTdataObject.Hcratio); break;
    case OpenThermVersionMaster:            return String(OTdataObject.OpenThermVersionMaster); break;
    case OpenThermVersionSlave:             return String(OTdataObject.OpenThermVersionSlave); break;
    case Status:                            return String(OTdataObject.Status); break;
    case ASFflags:                          return String(OTdataObject.ASFflags); break;
    case MConfigMMemberIDcode:              return String(OTdataObject.MConfigMMemberIDcode); break; 
    case SConfigSMemberIDcode:              return String(OTdataObject.SConfigSMemberIDcode); break;   
    case Command:                           return String(OTdataObject.Command);  break; 
    case RBPflags:                          return String(OTdataObject.RBPflags); break; 
    case TSP:                               return String(OTdataObject.TSP); break; 
    case TSPindexTSPvalue:                  return String(OTdataObject.TSPindexTSPvalue);  break; 
    case FHBsize:                           return String(OTdataObject.FHBsize);  break;  
    case FHBindexFHBvalue:                  return String(OTdataObject.FHBindexFHBvalue);  break; 
    case MaxCapacityMinModLevel:            return String(OTdataObject.MaxCapacityMinModLevel);  break; 
    case DayTime:                           return String(OTdataObject.DayTime);  break; 
    case Date:                              return String(OTdataObject.Date);  break; 
    case Year:                              return String(OTdataObject.Year);  break; 
    case TdhwSetUBTdhwSetLB:                return String(OTdataObject.TdhwSetUBTdhwSetLB); break;  
    case MaxTSetUBMaxTSetLB:                return String(OTdataObject.MaxTSetUBMaxTSetLB); break;  
    case HcratioUBHcratioLB:                return String(OTdataObject.HcratioUBHcratioLB); break;  
    case RemoteOverrideFunction:            return String(OTdataObject.RemoteOverrideFunction); break;
    case OEMDiagnosticCode:                 return String(OTdataObject.OEMDiagnosticCode);  break;
    case BurnerStarts:                      return String(OTdataObject.BurnerStarts);  break; 
    case CHPumpStarts:                      return String(OTdataObject.CHPumpStarts);  break; 
    case DHWPumpValveStarts:                return String(OTdataObject.DHWPumpValveStarts);  break; 
    case DHWBurnerStarts:                   return String(OTdataObject.DHWBurnerStarts);  break;
    case BurnerOperationHours:              return String(OTdataObject.BurnerOperationHours);  break;
    case CHPumpOperationHours:              return String(OTdataObject.CHPumpOperationHours);  break; 
    case DHWPumpValveOperationHours:        return String(OTdataObject.DHWPumpValveOperationHours);  break;  
    case DHWBurnerOperationHours:           return String(OTdataObject.DHWBurnerOperationHours);  break; 
    case MasterVersion:                     return String(OTdataObject.MasterVersion); break; 
    case SlaveVersion:                      return String(OTdataObject.SlaveVersion); break;
    case StatusVH:                          return String(OTdataObject.StatusVH); break;
    case ControlSetpointVH:                 return String(OTdataObject.ControlSetpointVH); break;
    case FaultFlagsCodeVH:                  return String(OTdataObject.FaultFlagsCodeVH); break;
    case DiagnosticCodeVH:                  return String(OTdataObject.DiagnosticCodeVH); break;
    case ConfigMemberIDVH:                  return String(OTdataObject.ConfigMemberIDVH); break;
    case OpenthermVersionVH:                return String(OTdataObject.OpenthermVersionVH); break;
    case VersionTypeVH:                     return String(OTdataObject.VersionTypeVH); break;
    case RelativeVentilation:               return String(OTdataObject.RelativeVentilation); break;
    case RelativeHumidityVH:                return String(OTdataObject.RelativeHumidityVH); break;
    case CO2LevelVH:                        return String(OTdataObject.CO2LevelVH); break;
    case SupplyInletTemperature:            return String(OTdataObject.SupplyInletTemperature); break;
    case SupplyOutletTemperature:           return String(OTdataObject.SupplyOutletTemperature); break;
    case ExhaustInletTemperature:           return String(OTdataObject.ExhaustInletTemperature); break;
    case ExhaustOutletTemperature:          return String(OTdataObject.ExhaustOutletTemperature); break;
    case ActualExhaustFanSpeed:             return String(OTdataObject.ActualExhaustFanSpeed); break;
    case ActualInletFanSpeed:               return String(OTdataObject.ActualInletFanSpeed); break;
    case RemoteParameterSettingVH:          return String(OTdataObject.RemoteParameterSettingVH); break;
    case NominalVentilationValue:           return String(OTdataObject.NominalVentilationValue); break;
    case TSPNumberVH:                       return String(OTdataObject.TSPNumberVH); break;
    case TSPEntryVH:                        return String(OTdataObject.TSPEntryVH); break;
    case FaultBufferSizeVH:                 return String(OTdataObject.FaultBufferSizeVH); break;
    case FaultBufferEntryVH:                return String(OTdataObject.FaultBufferEntryVH); break;
    case FanSpeed:                          return String(OTdataObject.FanSpeed); break;
    case ElectricalCurrentBurnerFlame:      return String(OTdataObject.ElectricalCurrentBurnerFlame); break;
    case TRoomCH2:                          return String(OTdataObject.TRoomCH2); break;
    case RelativeHumidity:                  return String(OTdataObject.RelativeHumidity); break;
    case RFstrengthbatterylevel:            return String(OTdataObject.RFstrengthbatterylevel); break;
    case OperatingMode_HC1_HC2_DHW:         return String(OTdataObject.OperatingMode_HC1_HC2_DHW); break;
    case ElectricityProducerStarts:         return String(OTdataObject.ElectricityProducerStarts); break;
    case ElectricityProducerHours:          return String(OTdataObject.ElectricityProducerHours); break;
    case ElectricityProduction:             return String(OTdataObject.ElectricityProduction); break;
    case CumulativElectricityProduction:    return String(OTdataObject.CumulativElectricityProduction); break;
    case RemehadFdUcodes:                   return String(OTdataObject.RemehadFdUcodes); break;
    case RemehaServicemessage:              return String(OTdataObject.RemehaServicemessage); break;
    case RemehaDetectionConnectedSCU:       return String(OTdataObject.RemehaDetectionConnectedSCU); break;
    default: return "not implemented yet!";
  } 
}

void startOTGWstream()
{
  OTGWstream.begin();
}

void upgradepicnow(const char *filename) {
  if (OTGWSerial.busy()) return; // if already in programming mode, never call it twice
  DebugTln("Start PIC upgrade now.");
  fwupgradestart(filename);  
  while (OTGWSerial.busy()){
    feedWatchDog();
    //blink the led during flash...
    DECLARE_TIMER_MS(timerUpgrade, 500);
    if (DUE(timerUpgrade)) {
        blinkLEDnow(LED2);
    }
  }
  // When you are done, then reset the PIC one more time, to capture the actual fwversion of the OTGW
  resetOTGW();
}

void fwupgradedone(OTGWError result, short errors = 0, short retries = 0) {
  DebugTf("Upgrade finished: Errorcode = %d - %d retries, %d errors\n", result, retries, errors);
  switch (result) {
    case OTGW_ERROR_NONE:          errorupgrade = "PIC upgrade was succesful."; break;
    case OTGW_ERROR_MEMORY:        errorupgrade = "Not enough memory available."; break;
    case OTGW_ERROR_INPROG:        errorupgrade = "Firmware upgrade in progress."; break;
    case OTGW_ERROR_HEX_ACCESS:    errorupgrade = "Could not open hex file."; break;
    case OTGW_ERROR_HEX_FORMAT:    errorupgrade = "Invalid format of hex file."; break;
    case OTGW_ERROR_HEX_DATASIZE:  errorupgrade = "Wrong data size in hex file."; break;
    case OTGW_ERROR_HEX_CHECKSUM:  errorupgrade = "Bad checksum in hex file."; break;
    case OTGW_ERROR_MAGIC:         errorupgrade = "Hex file does not contain expected data."; break;
    case OTGW_ERROR_RESET:         errorupgrade = "PIC reset failed."; break;
    case OTGW_ERROR_RETRIES:       errorupgrade = "Too many retries."; break;
    case OTGW_ERROR_MISMATCHES:   errorupgrade = "Too many mismatches."; break;
    default:                       errorupgrade = "Unknown state."; break;
  }
}


// Schelte's firmware integration
void fwupgradestart(const char *hexfile) {
  OTGWError result;
  
  digitalWrite(LED1, LOW);
  result = OTGWSerial.startUpgrade(hexfile);
  if (result!= OTGW_ERROR_NONE) {
    fwupgradedone(result);
  } else {
    OTGWSerial.registerFinishedCallback(fwupgradedone);
  }
}

String checkforupdatepic(String filename){
  WiFiClient client;
  HTTPClient http;
  String latest = "";
  int code;

  http.begin(client, "http://otgw.tclcode.com/download/" + filename);
  http.collectHeaders(hexheaders, 2);
  code = http.sendRequest("HEAD");
  if (code == HTTP_CODE_OK) {
    for (int i = 0; i< http.headers(); i++) {
      DebugTf("%s: %s\r\n", hexheaders[i], http.header(i).c_str());
    }
    latest = http.header(1);
    DebugTf("Update %s -> %s\r\n", filename.c_str(), latest.c_str());
    http.end();
  }
  return latest; 
}

void refreshpic(String filename, String version) {
  WiFiClient client;
  HTTPClient http;
  String latest;
  int code;

  if (latest=checkforupdatepic(filename) != "") {
    if (latest != version) {
      DebugTf("Update %s: %s -> %s\r\n", filename.c_str(), version.c_str(), latest.c_str());
      http.begin(client, "http://otgw.tclcode.com/download/" + filename);
      code = http.GET();
      if (code == HTTP_CODE_OK) {
        File f = LittleFS.open("/" + filename, "w");
        if (f) {
          http.writeToStream(&f);
          f.close();
          String verfile = "/" + filename;
          verfile.replace(".hex", ".ver");
          f = LittleFS.open(verfile, "w");
          if (f) {
            f.print(latest + "\n");
            f.close();
            DebugTf("Update successful\n");
          }
        }
      }
    }
  }
  http.end();
}

void upgradepic() {
  String action = httpServer.arg("action");
  String filename = httpServer.arg("name");
  String version = httpServer.arg("version");
  DebugTf("Action: %s %s %s\r\n", action.c_str(), filename.c_str(), version.c_str());
  if (action == "upgrade") {
    upgradepicnow(String("/" + filename).c_str());
  } else if (action == "refresh") {
    refreshpic(filename, version);
  } else if (action == "delete") {
    String path = "/" + filename;
    LittleFS.remove(path);
    path.replace(".hex", ".ver");
    LittleFS.remove(path);
  }
  httpServer.sendHeader("Location", "index.html#tabPICflash", true);
  httpServer.send(303, "text/html", "<a href='index.html#tabPICflash'>Return</a>");
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
