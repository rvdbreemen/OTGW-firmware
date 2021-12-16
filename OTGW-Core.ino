/* 
***************************************************************************  
**  Program  : OTGW-Core.ino
**  Version  : v0.9.0
**
**  Copyright (c) 2021 Robert van den Breemen
**  Borrowed from OpenTherm library from: 
**      https://github.com/jpraus/arduino-opentherm
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#define OTGWDebugTln(...) ({ if (bDebugOTmsg) DebugTln(__VA_ARGS__);    })
#define OTGWDebugln(...)  ({ if (bDebugOTmsg) Debugln(__VA_ARGS__);    })
#define OTGWDebugTf(...)  ({ if (bDebugOTmsg) DebugTf(__VA_ARGS__);    })
#define OTGWDebugf(...)   ({ if (bDebugOTmsg) Debugf(__VA_ARGS__);    })
#define OTGWDebugT(...)   ({ if (bDebugOTmsg) DebugT(__VA_ARGS__);    })
#define OTGWDebug(...)    ({ if (bDebugOTmsg) Debug(__VA_ARGS__);    })


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
OpenthermData OTdata, delayedOTdata, tmpOTdata;

#define OTGW_BANNER "OpenTherm Gateway"

//===================[ Reset OTGW ]===============================
void resetOTGW() {
  sPICfwversion ="No version found"; //reset versionstring
  OTGWSerial.resetPic();
  //then read the first response of the firmware to make sure it reads it
  String resp = OTGWSerial.readStringUntil('\n');
  resp.trim();
  OTGWDebugTf("Received firmware version: [%s] [%s] (%d)\r\n", CSTR(resp), OTGWSerial.firmwareVersion(), strlen(OTGWSerial.firmwareVersion()));
  bOTGWonline = (resp.length()>0); 
  if (bOTGWonline) sPICfwversion = String(OTGWSerial.firmwareVersion());
  OTGWDebugTf("Current firmware version: %s\r\n", CSTR(sPICfwversion));
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
  OTGWDebugTf("Current firmware version: %s\r\n", CSTR(_ret));
  _ret.trim();
  return _ret;
}
//===================[ checkOTWGpicforupdate ]=====================
void checkOTWGpicforupdate(){
  OTGWDebugTf("OTGW PIC firmware version = [%s]\r\n", CSTR(sPICfwversion));
  String latest = checkforupdatepic("gateway.hex");
  if (!bOTGWonline) {
    sMessage = sPICfwversion; 
  } else if (latest.isEmpty() || sPICfwversion.isEmpty()) {
    sMessage = ""; //two options: no internet connection OR no firmware version
  } else if (latest != sPICfwversion) {
    sMessage = "New PIC version " + latest + " available!";
  }
  if (!checklittlefshash()) sMessage = "Flash your littleFS with matching version!";
}

//===================[ sendOTGWbootcmd ]=====================
void sendOTGWbootcmd(){
  if (!settingOTGWcommandenable) return;
  OTGWDebugTf("OTGW boot message = [%s]\r\n", CSTR(settingOTGWcommands));

  // parse and execute commands
  char bootcmds[settingOTGWcommands.length() + 1];
  settingOTGWcommands.toCharArray(bootcmds, settingOTGWcommands.length() + 1);
  char* cmd;
  int i = 0;
  cmd = strtok(bootcmds, ";");
  while (cmd != NULL) {
    OTGWDebugTf("Boot command[%d]: %s\r\n", i++, cmd);
    addOTWGcmdtoqueue(cmd, strlen(cmd), true);
    cmd = strtok(NULL, ";");
  }
}

//===================[ OTGW Command & Response ]===================
String executeCommand(const String sCmd){
  //send command to OTGW
  OTGWDebugTf("OTGW Send Cmd [%s]\r\n", CSTR(sCmd));
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
  OTGWDebugTf("Send command: [%s]\r\n", CSTR(_cmd));
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
  OTGWDebugTf("Command send [%s]-[%s] - Response line: [%s] - Returned value: [%s]\r\n", CSTR(sCmd), CSTR(_cmd), CSTR(line), CSTR(_ret));
  return _ret;
}
//===================[ Watchdog OTGW ]===============================
String initWatchDog() {
  // Hardware WatchDog is based on: 
  // https://github.com/rvdbreemen/ESPEasySlaves/tree/master/TinyI2CWatchdog
  // Code here is based on ESPEasy code, modified to work in the project.

  // configure hardware pins according to eeprom settings.
  OTGWDebugTln(F("Setup Watchdog"));
  OTGWDebugTln(F("INIT : I2C"));
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
      OTGWDebugTln(F("INIT : Reset by WD!"));
      ReasonReset = "Reset by External WD";
      //lastReset = BOOT_CAUSE_EXT_WD;
    }
  }
  return ReasonReset;
  //===========================================
}

void WatchDogEnabled(byte stateWatchdog){
    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
    Wire.write(7);                                //Write to register 7, the action register
    Wire.write(stateWatchdog);                    //1 = armed to reset, 0 = turned off     
    Wire.endTransmission();                       //That's all there is...
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
  return ((value << 8) + valueLB);
}

void OpenthermData::u16(uint16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t value = valueHB;
  return ((value << 8) + valueLB);
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
    PROGMEM_readAnything (&OTmap[message_id], OTlookupitem);
    return OTlookupitem.label;
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
// bit: description [ clear/0, set/1]
// 0: CH enable [ CH is disabled, CH is enabled]
// 1: DHW enable [ DHW is disabled, DHW is enabled]
// 2: Cooling enable [ Cooling is disabled, Cooling is enabled]
// 3: OTC active [OTC not active, OTC is active]
// 4: CH2 enable [CH2 is disabled, CH2 is enabled]
// 5: reserved
// 6: reserved
// 7: reserved

bool isCentralHeatingEnabled() {
	return OTdataObject.MasterStatus & 0x01;
}

bool isDomesticHotWaterEnabled() {
	return OTdataObject.MasterStatus & 0x02;
}

bool isCoolingEnabled() {
	return OTdataObject.MasterStatus & 0x04;
}

bool isOutsideTemperatureCompensationActive() {
	return OTdataObject.MasterStatus & 0x08;
}

bool isCentralHeating2enabled() {
	return OTdataObject.MasterStatus & 0x10;
}

//Slave
// bit: description [ clear/0, set/1]
// 0: fault indication [ no fault, fault ]
// 1: CH mode [CH not active, CH active]
// 2: DHW mode [ DHW not active, DHW active]
// 3: Flame status [ flame off, flame on ]
// 4: Cooling status [ cooling mode not active, cooling mode active ]
// 5: CH2 mode [CH2 not active, CH2 active]
// 6: diagnostic indication [no diagnostics, diagnostic event]
// 7: reserved

bool isFaultIndicator() {
	return OTdataObject.SlaveStatus & 0x01;
}

bool isCentralHeatingActive() {
	return OTdataObject.SlaveStatus & 0x02;
}

bool isDomesticHotWaterActive() {
	return OTdataObject.SlaveStatus & 0x04;
}

bool isFlameStatus() {
	return OTdataObject.SlaveStatus & 0x08;
}

bool isCoolingActive() {
	return OTdataObject.SlaveStatus & 0x10;
}

bool isCentralHeating2Active() {
	return OTdataObject.SlaveStatus & 0x20;
}

bool isDiagnosticIndicator() {
	return OTdataObject.SlaveStatus & 0x40;
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
}  //byte_to_binary

void print_f88(float& value)
{
  //function to print data
  float _value = round(OTdata.f88()*100.0) / 100.0; // round float 2 digits, like this: x.xx 
  // OTGWDebugf("%s = %3.2f %s", OTlookupitem.label, _value , OTlookupitem.unit);
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %s %s", OTlookupitem.label, _msg , OTlookupitem.unit);
  //SendMQTT
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
    value = _value;
  }
}
  

void print_s16(int16_t& value)
{    
  int16_t _value = OTdata.s16(); 
  // OTGWDebugf("%s = %5d %s", OTlookupitem.label, _value, OTlookupitem.unit);
  //Build string for MQTT
  char _msg[15] {0};
  itoa(_value, _msg, 10);
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  //SendMQTT
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
    value = _value;
  }
}

void print_s8s8(uint16_t& value)
{  
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %3d / %3d %s", OTlookupitem.label, (int8_t)OTdata.valueHB, (int8_t)OTdata.valueLB, OTlookupitem.unit);
  //Build string for MQTT
  char _msg[15] {0};
  char _topic[50] {0};
  itoa((int8_t)OTdata.valueHB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_value_hb", sizeof(_topic));
  //OTGWDebugf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, _msg);
  }
  //Build string for MQTT
  itoa((int8_t)OTdata.valueLB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_value_lb", sizeof(_topic));
  //OTGWDebugf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, _msg);
    value = OTdata.u16();
  }
}

void print_u16(uint16_t& value)
{ 
  uint16_t _value = OTdata.u16(); 
  //Build string for MQTT
  char _msg[15] {0};
  utoa(_value, _msg, 10);
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  //SendMQTT
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
    value = _value;
  }
}

void print_status(uint16_t& value)
{
  char _flag8_master[8] {0};
  char _flag8_slave[8] {0};
  
  if (OTdata.masterslave == 0) {
    // Parse master bits
    //bit: [clear/0, set/1]
    //  0: CH enable [ CH is disabled, CH is enabled]
    //  1: DHW enable [ DHW is disabled, DHW is enabled]
    //  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
    //  3: OTC active [OTC not active, OTC is active]
    //  4: CH2 enable [CH2 is disabled, CH2 is enabled]
    //  5: Summer/winter mode [Summertime, Wintertime]
    //  6: DHW blocking [ DHW not blocking, DHW blocking ]
    //  7: reserved
    _flag8_master[0] = (((OTdata.valueHB) & 0x01) ? 'C' : '-');
    _flag8_master[1] = (((OTdata.valueHB) & 0x02) ? 'D' : '-');
    _flag8_master[2] = (((OTdata.valueHB) & 0x04) ? 'C' : '-'); 
    _flag8_master[3] = (((OTdata.valueHB) & 0x08) ? 'O' : '-');
    _flag8_master[4] = (((OTdata.valueHB) & 0x10) ? '2' : '-'); 
    _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? 'W' : 'S'); 
    _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? 'B' : '-'); 
    _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
    _flag8_master[8] = '\0';

    PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
    OTGWDebugf("%s = Master [%s]", OTlookupitem.label, _flag8_master);

    //Master Status
    sendMQTTData(F("status_master"), _flag8_master);
    sendMQTTData(F("ch_enable"),             (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("dhw_enable"),            (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("cooling_enable"),        (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  delay(5); 
    sendMQTTData(F("otc_active"),            (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("ch2_enable"),            (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("summerwintertime"),      (((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("dhw_blocking"),          (((OTdata.valueHB) & 0x40) ? "ON" : "OFF"));  delay(5);

    OTdataObject.MasterStatus = OTdata.valueHB;
    } else {
    // Parse slave bits
    //  0: fault indication [ no fault, fault ]
    //  1: CH mode [CH not active, CH active]
    //  2: DHW mode [ DHW not active, DHW active]
    //  3: Flame status [ flame off, flame on ]
    //  4: Cooling status [ cooling mode not active, cooling mode active ]
    //  5: CH2 mode [CH2 not active, CH2 active]
    //  6: diagnostic indication [no diagnostics, diagnostic event]
    //  7: Electricity production [no eletric production, eletric production]
    _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'E' : '-');
    _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'C' : '-'); 
    _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'W' : '-'); 
    _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'F' : '-'); 
    _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'C' : '-'); 
    _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '2' : '-'); 
    _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-'); 
    _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? 'P' : '-');
    _flag8_slave[8] = '\0';

    PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
    OTGWDebugf("%s = Slave  [%s]", OTlookupitem.label, _flag8_slave);

    //Slave Status
    sendMQTTData(F("status_slave"), _flag8_slave);
    sendMQTTData(F("fault"),                 (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  delayms(5);  
    sendMQTTData(F("centralheating"),        (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));  delayms(5);  
    sendMQTTData(F("domestichotwater"),      (((OTdata.valueLB) & 0x04) ? "ON" : "OFF"));  delayms(5);  
    sendMQTTData(F("flame"),                 (((OTdata.valueLB) & 0x08) ? "ON" : "OFF"));  delayms(5);
    sendMQTTData(F("cooling"),               (((OTdata.valueLB) & 0x10) ? "ON" : "OFF"));  delayms(5); 
    sendMQTTData(F("centralheating2"),       (((OTdata.valueLB) & 0x20) ? "ON" : "OFF"));  delayms(5);
    sendMQTTData(F("diagnostic_indicator"),  (((OTdata.valueLB) & 0x40) ? "ON" : "OFF"));  delayms(5);
    sendMQTTData(F("eletric_production"),    (((OTdata.valueLB) & 0x80) ? "ON" : "OFF"));  delayms(5);

    OTdataObject.SlaveStatus = OTdata.valueLB;
  }

  uint16_t _value = OTdata.u16();
  // OTGWDebugf("Status u16 [%04x] _value [%04x] hb [%02x] lb [%02x]", OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
  value = _value;
}

void print_solar_storage_status(uint16_t& value)
{ 
  char _msg[15] {0};
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);

  if (OTdata.masterslave == 0) {
    // Master Solar Storage 
    // ID101:HB012: Master Solar Storage: Solar mode
    uint8_t MasterSolarMode = (OTdata.valueHB) & 0x7;
    OTGWDebugf("%s = Solar Storage Master Mode [%d] ", OTlookupitem.label, MasterSolarMode);
    sendMQTTData(F("solar_storage_master_mode"), itoa(MasterSolarMode, _msg, 10));  delayms(5);
    OTdataObject.SolarMasterStatus = OTdata.valueHB;
  } else { 
    //Slave
    // ID101:LB0: Slave Solar Storage: Fault indication
    uint8_t SlaveSolarFaultIndicator =  (OTdata.valueLB) & 0x01;
    // ID101:LB123: Slave Solar Storage: Solar mode status
    uint8_t SlaveSolarModeStatus = (OTdata.valueLB>>1) & 0x07;
    // ID101:LB45: Slave Solar Storage: Solar status
    uint8_t SlaveSolarStatus = (OTdata.valueLB>>4)& 0x03;
    OTGWDebugf("\r\n%s = Slave Solar Fault Indicator [%d] ", OTlookupitem.label, SlaveSolarFaultIndicator);
    OTGWDebugf("\r\n%s = Slave Solar Mode Status [%d] ", OTlookupitem.label, SlaveSolarModeStatus);
    OTGWDebugf("\r\n%s = Slave Solar Status [%d] ", OTlookupitem.label, SlaveSolarStatus);
    sendMQTTData(F("solar_storage_slave_fault_incidator"),  ((SlaveSolarFaultIndicator) ? "ON" : "OFF"));  delay(5); 
    sendMQTTData(F("solar_storage_mode_status"), itoa(SlaveSolarModeStatus, _msg, 10));  delay(5);
    sendMQTTData(F("solar_storage_slave_status"), itoa(SlaveSolarStatus, _msg, 10));  delay(5);
    OTdataObject.SolarSlaveStatus = OTdata.valueLB;
  }
  uint16_t _value = OTdata.u16();
  //OTGWDebugTf("Solar Storage Master / Slave Mode u16 [%04x] _value [%04x] hb [%02x] lb [%02x]", OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
  value = _value;
}

void print_statusVH(uint16_t& value)
{ 
  char _flag8_master[8] {0};
  char _flag8_slave[8] {0};

  if (OTdata.masterslave == 0){
      
    // Parse master bits
    //bit: [clear/0, set/1]
    // ID70:HB0: Master status ventilation / heat-recovery: Ventilation enable
    // ID70:HB1: Master status ventilation / heat-recovery: Bypass postion
    // ID70:HB2: Master status ventilation / heat-recovery: Bypass mode
    // ID70:HB3: Master status ventilation / heat-recovery: Free ventilation mode
    //  4: reserved
    //  5: reserved
    //  6: reserved
    //  7: reserved
    _flag8_master[0] = (((OTdata.valueHB) & 0x01) ? 'V' : '-');
    _flag8_master[1] = (((OTdata.valueHB) & 0x02) ? 'P' : '-');
    _flag8_master[2] = (((OTdata.valueHB) & 0x04) ? 'M' : '-'); 
    _flag8_master[3] = (((OTdata.valueHB) & 0x08) ? 'F' : '-');
    _flag8_master[4] = (((OTdata.valueHB) & 0x10) ? '.' : '-'); 
    _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? '.' : '-'); 
    _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? '.' : '-'); 
    _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
    _flag8_master[8] = '\0';

    PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
    OTGWDebugf("%s = VH Master [%s]", OTlookupitem.label, _flag8_master);
    //Master Status
    sendMQTTData(F("status_vh_master"), _flag8_master);
    sendMQTTData(F("vh_ventilation_enabled"),        (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("vh_bypass_position"),            (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("vh_bypass_mode"),                (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  delay(5); 
    sendMQTTData(F("vh_free_ventlation_mode"),       (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));  delay(5);

    OTdataObject.MasterStatusVH = OTdata.valueLB;
    } else {
    // Parse slave bits
    // ID70:LB0: Slave status ventilation / heat-recovery: Fault indication
    // ID70:LB1: Slave status ventilation / heat-recovery: Ventilation mode
    // ID70:LB2: Slave status ventilation / heat-recovery: Bypass status
    // ID70:LB3: Slave status ventilation / heat-recovery: Bypass automatic status
    // ID70:LB4: Slave status ventilation / heat-recovery: Free ventilation status
    // ID70:LB6: Slave status ventilation / heat-recovery: Diagnostic indication
    _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'F' : '-');
    _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'V' : '-'); 
    _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'P' : '-'); 
    _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'A' : '-'); 
    _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'F' : '-'); 
    _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '.' : '-');
    _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-'); 
    _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? '.' : '-');
    _flag8_slave[8] = '\0';

    PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
    OTGWDebugf("%s = VH Slave  [%s]", OTlookupitem.label, _flag8_slave);

    //Slave Status
    sendMQTTData(F("status_vh_slave"), _flag8_slave);
    sendMQTTData(F("vh_fault"),                   (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  delay(5);  
    sendMQTTData(F("vh_ventlation_mode"),         (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));  delay(5);  
    sendMQTTData(F("vh_bypass_status"),           (((OTdata.valueLB) & 0x04) ? "ON" : "OFF"));  delay(5);  
    sendMQTTData(F("vh_bypass_automatic_status"), (((OTdata.valueLB) & 0x08) ? "ON" : "OFF"));  delay(5);
    sendMQTTData(F("vh_free_ventliation_status"), (((OTdata.valueLB) & 0x10) ? "ON" : "OFF"));  delay(5);  
    sendMQTTData(F("vh_diagnostic_indicator"),    (((OTdata.valueLB) & 0x40) ? "ON" : "OFF"));  delay(5);

    OTdataObject.SlaveStatusVH = OTdata.valueLB;
  }

  uint16_t _value = OTdata.u16();
  //OTGWDebugTf("Status u16 [%04x] _value [%04x] hb [%02x] lb [%02x]", OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
  value = _value;
}


void print_ASFflags(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = ASF flags[%s] OEM fault code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _msg[15] {0};
  //Application Specific Fault
  sendMQTTData(F("ASF_flags"), byte_to_binary(OTdata.valueHB));
  //OEM fault code
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData(F("ASF_oemfaultcode"), _msg);

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
  sendMQTTData(F("service_request"),       (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  delay(5);  
  sendMQTTData(F("lockout_reset"),         (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  delay(5);  
  sendMQTTData(F("low_water_pressure"),    (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  delay(5);  
  sendMQTTData(F("gas_flame_fault"),       (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("air_pressure_fault"),    (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));  delay(5);  
  sendMQTTData(F("water_over_temperature"),(((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));  delay(5);
  value = OTdata.u16();
}

void print_RBPflags(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = M[%s] OEM fault code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _msg[15] {0};
  //Remote Boiler Paramaters
  sendMQTTData(F("RBP_flags_transfer_enable"), byte_to_binary(OTdata.valueHB));  delay(5);
  sendMQTTData(F("RBP_flags_read_write"), byte_to_binary(OTdata.valueLB));  delay(5);

  //bit: [clear/0, set/1]
  //0: DHW setpoint
  //1: max CH setpoint
  //2: reserved
  //3: reserved
  //4: reserved
  //5: reserved
  //6: reserved
  //7: reserved
  sendMQTTData(F("rbp_dhw_setpoint"),       (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));   delay(5); 
  sendMQTTData(F("rbp_max_ch_setpoint"),    (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));   delay(5); 

  //bit: [clear/0, set/1]
  //0: read write  DHW setpoint
  //1: read write max CH setpoint
  //2: reserved
  //3: reserved
  //4: reserved
  //5: reserved
  //6: reserved
  //7: reserved
  sendMQTTData(F("rbp_rw_dhw_setpoint"),       (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));   delay(5); 
  sendMQTTData(F("rbp_rw_max_ch_setpoint"),    (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));   delay(5); 

  value = OTdata.u16();
}

void print_slavememberid(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = Slave Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for SendMQTT
  sendMQTTData(F("slave_configuration"), byte_to_binary(OTdata.valueHB));
  char _msg[15] {0};
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData(F("slave_memberid_code"), _msg);

  
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
  // 6:  Remote water filling function
  // 7:  Heat/cool mode control 

  sendMQTTData(F("dhw_present"),                             (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("control_type_modulation"),                 (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("cooling_config"),                          (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("dhw_config"),                              (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("master_low_off_pump_control_function"),    (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));  delay(5); 
  sendMQTTData(F("ch2_present"),                             (((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("remote_water_filling_function"),           (((OTdata.valueHB) & 0x40) ? "ON" : "OFF"));  delay(5);  
  sendMQTTData(F("heat_cool_mode_control"),                  (((OTdata.valueHB) & 0x80) ? "ON" : "OFF"));  delay(5);
  value = OTdata.u16();
}

void print_mastermemberid(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = Master Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _msg[15] {0};
  sendMQTTData(F("master_configuration"), byte_to_binary(OTdata.valueHB));
  sendMQTTData(F("master_configuration_smart_power"), (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
  
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData(F("master_memberid_code"), _msg);
  value = OTdata.u16();
}

void print_vh_configmemberid(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = VH Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _msg[15] {0};
  sendMQTTData(F("vh_configuration"), byte_to_binary(OTdata.valueHB)); delay(5);
  sendMQTTData(F("vh_configuration_system_type"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("vh_configuration_bypass"),         (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  delay(5);
  sendMQTTData(F("vh_configuration_speed_control"),  (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  delay(5);
  
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData(F("vh_memberid_code"), _msg);
  value = OTdata.u16();
}

void print_solarstorage_slavememberid(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = Solar Storage Slave Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for SendMQTT
  sendMQTTData(F("solar_storage_slave_configuration"), byte_to_binary(OTdata.valueHB));
  char _msg[15] {0};
  utoa(OTdata.valueLB, _msg, 10);
  sendMQTTData(F("solar_storage_slave_memberid_code"), _msg);

  //ID103:HB0: Slave Configuration Solar Storage: System type1
  sendMQTTData(F("solar_storage_system_type"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
  value = OTdata.u16();
}

void print_flag8u8(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = M[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 value
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_flag8", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  }
  //u8 value
  char _msg[15] {0};
  utoa(OTdata.valueLB, _msg, 10);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_code", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, _msg);
    value = OTdata.u16();
  }
}

void print_flag8(uint16_t& value)
{
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = flag8 = [%s] - decimal = [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

  //Build string for MQTT
  char _topic[50] {0};
  //flag8 value
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_flag8", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
    value = OTdata.u16();
  }
}


void print_flag8flag8(uint16_t& value)
{ 
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 valueHB
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = HB flag8[%s] -[%3d] ", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_flag8", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  }
  //flag8 valueLB
  OTGWDebugf("%s = LB flag8[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_flag8", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
    value = OTdata.u16();
  }
}

void print_vh_remoteparametersetting(uint16_t& value)
{ 
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 valueHB
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = HB flag8[%s] -[%3d] ", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  sendMQTTData(F("vh_tramfer_enble_nominal_ventlation_value"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
 //flag8 valueLB
  OTGWDebugf("%s = LB flag8[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_flag8", sizeof(_topic));
  sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
  sendMQTTData(F("vh_rw_nominal_ventlation_value"),    (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  
  value = OTdata.u16();
}



void print_command(uint16_t& value)
{ 
  //Known Commands
  // ID4 (HB=1): Remote Request Boiler Lockout-reset
  // ID4 (HB=2): Remote Request Water filling
  // ID4 (HB=10): Remote Request Service request reset
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //flag8 valueHB
  utoa((OTdata.valueHB), _msg, 10);
  //OTGWDebugf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_u8", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_remote_command", sizeof(_topic));
  switch (OTdata.valueHB) {
    case 1: sendMQTTData(_topic, "Remote Request Boiler Lockout-reset");  OTGWDebugf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Boiler Lockout-reset"); break;
    case 2: sendMQTTData(_topic, "Remote Request Water filling"); OTGWDebugf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Water filling"); break;
    case 10: sendMQTTData(_topic, "Remote Request Service request reset");  OTGWDebugf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Service request reset");break;
    default: sendMQTTData(_topic, "Unknown command"); OTGWDebugf("\r\n%s = remote command [%s]", OTlookupitem.label, "Unknown command");break;
  } 

  //flag8 valueLB
  utoa((OTdata.valueLB), _msg, 10);
  //OTGWDebugf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_u8", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  value = OTdata.u16();
}

void print_u8u8(uint16_t& value)
{ 
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //flag8 valueHB
  utoa((OTdata.valueHB), _msg, 10);
  //OTGWDebugf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_hb_u8", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, _msg);
  }
  //flag8 valueLB
  utoa((OTdata.valueLB), _msg, 10);
  //OTGWDebugf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_lb_u8", sizeof(_topic));
  if ((static_cast<OpenThermMessageType>(OTdata.type) != OT_READ_DATA) && (OTdata.skipthis==false)) {
    sendMQTTData(_topic, _msg);
    value = OTdata.u16();
  }
}

void print_date(uint16_t& value)
{ 
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  //Build string for MQTT
  char _topic[50] {0};
  char _msg[10] {0};
  //flag8 valueHB
  utoa((OTdata.valueHB), _msg, 10);
  //OTGWDebugf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_month", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  //flag8 valueLB
  utoa((OTdata.valueLB), _msg, 10);
  //OTGWDebugf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
  strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
  strlcat(_topic, "_day_of_month", sizeof(_topic));
  sendMQTTData(_topic, _msg);
  value = OTdata.u16();
}

void print_daytime(uint16_t& value)
{
  //function to print data
  const char *dayOfWeekName[]  { "Unknown", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "Unknown" };
  PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
  OTGWDebugf("%s = %s - %2d:%2d", OTlookupitem.label, dayOfWeekName[(OTdata.valueHB >> 5) & 0x7], (OTdata.valueHB & 0x1F), OTdata.valueLB); 
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
  value = OTdata.u16();
}


//===================[ Command Queue implementatoin ]=============================

#define OTGW_CMD_RETRY 5
#define OTGW_CMD_INTERVAL_MS 5000
#define OTGW_DELAY_SEND_MS 1000
#define MAX_QUEUE_MSGSIZE 127
/*
  addOTWGcmdtoqueue adds a command to the queue. 
  First it checks the queue, if the command is in the queue, it's updated.
  Otherwise it's simply added to the queue, unless there are no free queue slots.
*/
void addOTWGcmdtoqueue(const char* buf, const int len, const bool force = false){
  if ((len < 3) || (buf[2] != '=')){ 
    //no valid command of less then 2 bytes
    OTGWDebugT("CmdQueue: Error:Not a valid command=[");
    for (int i = 0; i < len; i++) {
      OTGWDebug((char)buf[i]);
    }
    OTGWDebugf("] (%d)\r\n", len); 
  }

  //check to see if the cmd is in queue
  bool foundcmd = false;
  int8_t insertptr = cmdptr; //set insertprt to next empty slot
  if (!force){
    char cmd[2]; memset( cmd, 0, sizeof(cmd));
    memcpy(cmd, buf, 2);
    for (int i=0; i<cmdptr; i++){
      if (strstr(cmdqueue[i].cmd, cmd)) {
        //found cmd exists, set the inertptr to found slot
        foundcmd = true;
        insertptr = i;
        break;
      }
    } 
  } 
  if (foundcmd) OTGWDebugTf("CmdQueue: Found cmd exists in slot [%d]\r\n", insertptr);
  else OTGWDebugTf("CmdQueue: Adding cmd end of queue, slot [%d]\r\n", insertptr);

  //insert to the queue
  OTGWDebugTf("CmdQueue: Insert queue in slot[%d]:", insertptr);
  OTGWDebug("cmd[");
  for (int i = 0; i < len; i++) {
    OTGWDebug((char)buf[i]);
  }
  OTGWDebugf("] (%d)\r\n", len); 

  //copy the command into the queue
  int cmdlen = min((int)len , (int)(sizeof(cmdqueue[insertptr].cmd)-1));
  memset(cmdqueue[insertptr].cmd, 0, cmdlen+1);
  memcpy(cmdqueue[insertptr].cmd, buf, cmdlen);
  cmdqueue[insertptr].cmdlen = cmdlen;
  cmdqueue[insertptr].retrycnt = 0;
  cmdqueue[insertptr].due = millis() + OTGW_DELAY_SEND_MS; //due right away

  //if not found
  if (!foundcmd) {
    //if not reached max of queue
    if (cmdptr < CMDQUEUE_MAX) {
      cmdptr++; //next free slot
      OTGWDebugTf("CmdQueue: Next free queue slot: [%d]\r\n", cmdptr);
    } else OTGWDebugTln(F("CmdQueue: Error: Reached max queue"));
  } else OTGWDebugTf("CmdQueue: Found command at: [%d] - [%d]\r\n", insertptr, cmdptr);
}

/*
  handleOTGWqueue should be called every second from main loop. 
  This checks the queue for message are due to be resent.
  If retry max is reached the cmd is delete from the queue
*/
void handleOTGWqueue(){
  // OTGWDebugTf("CmdQueue: Commands in queue [%d]\r\n", (int)cmdptr);
  for (int i = 0; i<cmdptr; i++) {
    OTGWDebugTf("CmdQueue: Checking due in queue slot[%d]:[%lu]=>[%lu]\r\n", (int)i, (unsigned long)millis(), (unsigned long)cmdqueue[i].due);
    if (millis() >= cmdqueue[i].due) {
      OTGWDebugTf("CmdQueue: Queue slot [%d] due\r\n", i);
      sendOTGW(cmdqueue[i].cmd, cmdqueue[i].cmdlen);
      cmdqueue[i].retrycnt++;
      cmdqueue[i].due = millis() + OTGW_CMD_INTERVAL_MS;
      if (cmdqueue[i].retrycnt >= OTGW_CMD_RETRY){
        //max retry reached, so delete command from queue
        for (int j=i; j<cmdptr; j++){
          OTGWDebugTf("CmdQueue: Moving [%d] => [%d]\r\n", j+1, j);
          strlcpy(cmdqueue[j].cmd, cmdqueue[j+1].cmd, sizeof(cmdqueue[i].cmd));
          cmdqueue[j].cmdlen = cmdqueue[j+1].cmdlen;
          cmdqueue[j].retrycnt = cmdqueue[j+1].retrycnt;
          cmdqueue[j].due = cmdqueue[j+1].due;
        }
        cmdptr--;
      }
      // //exit queue handling, after 1 command
      // return;
    }
  }
}

/*
  checkOTGWcmdqueue (buf, len)
  This takes response from otgw and checks to see if the command was accepted.
  Checks the response, and finds the command (if it's there).
  Then checks if incoming response matches what was to be set.
  Only then it's deleted from the queue.
*/
void checkOTGWcmdqueue(const char *buf, int len){
  if ((len<3) || (buf[2]!=':')) {
    OTGWDebugT("CmdQueue: Error: Not a command response [");
    for (int i = 0; i < len; i++) {
      OTGWDebug((char)buf[i]);
    }
    OTGWDebugf("] (%d)\r\n", len); 
    return; //not a valid command response
  }

  OTGWDebugT("CmdQueue: Checking if command is in in queue [");
  for (int i = 0; i < len; i++) {
    OTGWDebug((char)buf[i]);
  }
  OTGWDebugf("] (%d)\r\n", len); 

  char cmd[3]; memset( cmd, 0, sizeof(cmd));
  char value[11]; memset( value, 0, sizeof(value));
  memcpy(cmd, buf, 2);
  memcpy(value, buf+3, (len-3<sizeof(value)-1)?(len-3):(sizeof(value)-1));
  for (int i=0; i<cmdptr; i++){
      OTGWDebugTf("CmdQueue: Checking [%2s]==>[%d]:[%s] from queue\r\n", cmd, i, cmdqueue[i].cmd); 
    if (strstr(cmdqueue[i].cmd, cmd)){
      //command found, check value
      OTGWDebugTf("CmdQueue: Found cmd [%2s]==>[%d]:[%s]\r\n", cmd, i, cmdqueue[i].cmd); 
      // if(strstr(cmdqueue[i].cmd, value)){
        //value found, thus remove command from queue
        OTGWDebugTf("CmdQueue: Found value [%s]==>[%d]:[%s]\r\n", value, i, cmdqueue[i].cmd); 
        OTGWDebugTf("CmdQueue: Remove from queue [%d]:[%s] from queue\r\n", i, cmdqueue[i].cmd);
        for (int j=i; j<cmdptr; j++){
          OTGWDebugTf("CmdQueue: Moving [%d] => [%d]\r\n", j+1, j);
          strlcpy(cmdqueue[j].cmd, cmdqueue[j+1].cmd, sizeof(cmdqueue[i].cmd));
          cmdqueue[j].cmdlen = cmdqueue[j+1].cmdlen;
          cmdqueue[j].retrycnt = cmdqueue[j+1].retrycnt;
          cmdqueue[j].due = cmdqueue[j+1].due;
        }
        cmdptr--;
      // } else OTGWDebugTf("Error: Did not find value [%s]==>[%d]:[%s]\r\n", value, i, cmdqueue[i].cmd); 
    }
  }
}


//===================[ Send buffer to OTGW ]=============================
/* 
  sendOTGW(const char* buf, int len) sends a string to the serial OTGW device.
  The buffer is send out to OTGW on the serial device instantly, as long as there is space in the buffer.
*/
void sendOTGW(const char* buf, int len)
{
  // while (OTGWSerial.availableForWrite() < (len+2)) {
  //   //cannot write, buffer full, wait for some space in serial out buffer
  // feedWatchDog();     //this yields for other processes
  // }

  //Send the buffer to OTGW when the Serial interface is available
  if (OTGWSerial.availableForWrite()>=len+2) {
    //check the write buffer
    //OTGWDebugf("Serial Write Buffer space = [%d] - needed [%d]\r\n",OTGWSerial.availableForWrite(), (len+2));
    OTGWDebugT("Sending to Serial [");
    for (int i = 0; i < len; i++) {
      OTGWDebug((char)buf[i]);
    }
    OTGWDebug("] ("); OTGWDebug(len); OTGWDebug(")"); OTGWDebugln();
        
    //write buffer to serial
    OTGWSerial.write(buf, len);         
    OTGWSerial.write('\r');
    OTGWSerial.write('\n');            
    OTGWSerial.flush(); 
  } else OTGWDebugln("Error: Write buffer not big enough!");
}

/*
  This function checks if the string received is a valid "raw OT message".
  Raw OTmessages are 9 chars long and start with TBARE when talking to OTGW PIC.
  Message is not an OTmessage if length is not 9 long OR 3th char is ':' (= OTGW command response)
*/
bool isvalidotmsg(const char *buf, int len){
  char *chk = "TBARE";
  bool _ret =  (len==9);    //check 9 chars long
  _ret &= (buf[2]!=':');    //not a otgw command response 
  _ret &= (strchr(chk, buf[0])!=NULL); //1 char matches any of 'B', 'T', 'A', 'R' or 'E'
  return _ret;
}

/*
  Process OTGW messages coming from the PIC.
  It knows about:
  - raw OTmsg format
  - error format
  - ...
*/
void processOTGW(const char *buf, int len){
  static timer_t epochBoilerlastseen = 0;
  static timer_t epochThermostatlastseen = 0;
  static bool bOTGWboilerpreviousstate = false;
  static bool bOTGWthermostatpreviousstate = false;
  static bool bOTGWpreviousstate = false;

  if (isvalidotmsg(buf, len)) { 
    //OT protocol messages are 9 chars long
    if (settingMQTTOTmessage) sendMQTTData(F("otmessage"), buf);

    // counter of number of OT messages processed
    static int32_t cntOTmessagesprocessed = 0;
    cntOTmessagesprocessed++;
    // char _msg[15] {0};
    // sendMQTTData(F("otmsg_count"), itoa(cntOTmessagesprocessed, _msg, 10)); 

    // source of otmsg
    if (buf[0]=='B'){
      epochBoilerlastseen = now(); 
      OTdata.rsptype = OTGW_BOILER;
    } else if (buf[0]=='T'){
      epochThermostatlastseen = now();
      OTdata.rsptype = OTGW_THERMOSTAT;
    } else if (buf[0]=='R')    {
      epochBoilerlastseen = now();
      OTdata.rsptype = OTGW_REQUEST_BOILER;
    } else if (buf[0]=='A')    {
      epochThermostatlastseen = now();
      OTdata.rsptype = OTGW_ANSWER_THERMOSTAT;
    } else if (buf[0]=='E')    {
      OTdata.rsptype = OTGW_PARITY_ERROR;
    } 

    //If the Boiler or Thermostat messages have not been seen for 30 seconds, then set the state to false. 
    bOTGWboilerstate = (now() < (epochBoilerlastseen+30));  
    if (bOTGWboilerstate != bOTGWboilerpreviousstate) {
      sendMQTTData(F("otgw-pic/boiler_connected"), CBOOLEAN(bOTGWboilerstate)); 
      bOTGWboilerpreviousstate = bOTGWboilerstate;
    }
    bOTGWthermostatstate = (now() < (epochThermostatlastseen+30));
    if (bOTGWthermostatstate != bOTGWthermostatpreviousstate) {      
      sendMQTTData(F("otgw-pic/thermostat_connected"), CBOOLEAN(bOTGWthermostatstate));
      bOTGWthermostatpreviousstate = bOTGWthermostatstate;
    }
    //If either Boiler or Thermostat is offline, then the OTGW is considered offline as a whole.
    bOTGWonline = bOTGWboilerstate && bOTGWthermostatstate;
    if (bOTGWonline != bOTGWpreviousstate) {
      sendMQTTData(F("otgw-pic/pic_connected"), CBOOLEAN(bOTGWonline));
      sendMQTT(CSTR(MQTTPubNamespace), CBOOLEAN(bOTGWonline));
      // nodeMCU online/offline zelf naar 'otgw-firmware/' pushen
      bOTGWpreviousstate = bOTGWonline; //remember state, so we can detect statechanges
    }

    const char *bufval = buf + 1; //skip the first char
    uint32_t value = 0;
    //split 32bit value into the relevant OT protocol parts
    memset(OTdata.buf, 0, sizeof(OTdata.buf));        // clear buffer
    memcpy(OTdata.buf, buf, len);                     // copy the raw message to the buffer
    OTdata.len = len;                                 // set the length of the message  
    sscanf(bufval, "%8x", &value);                    // extract the value
    OTdata.value = value;                             // store the value
    OTdata.type = (value >> 28) & 0x7;                // byte 1 = take 3 bits that define msg msgType
    OTdata.masterslave = (OTdata.type >> 2) & 0x1;    // MSB from type --> 0 = master and 1 = slave
    OTdata.id = (value >> 16) & 0xFF;                 // byte 2 = message id 8 bits 
    OTdata.valueHB = (value >> 8) & 0xFF;             // byte 3 = high byte
    OTdata.valueLB = value & 0xFF;                    // byte 4 = low byte
    OTdata.time = millis();                           // time of reception    
    OTdata.skipthis = false;                          // default: do not skip this message (will be sent to MQTT)
    
 
    if (cntOTmessagesprocessed == 1) {       //first message needs to be put in the buffer
      //just store current message and delay processing
      delayedOTdata = OTdata;       //store current msg
      OTGWDebugln("delaying first message!");
    } else {                              //any other message will be processed
      //when the gateway overrides the boiler or thermostat, then do not use the results for decoding anywhere (skip this)
      bool skipthis = (delayedOTdata.id == OTdata.id) && (OTdata.time - delayedOTdata.time < 500) && 
          (((OTdata.rsptype == OTGW_ANSWER_THERMOSTAT) && (delayedOTdata.rsptype == OTGW_BOILER)) ||
            ((OTdata.rsptype == OTGW_REQUEST_BOILER) && (delayedOTdata.rsptype == OTGW_THERMOSTAT))) ;
      //if B --> A, then gateway tells the thermostat what it needs to hear, thus the boiler value is the real thing, then skip current A message, and use B value.
      //if T --> R, then gateway overrides the thermostat, and tells the boiler what to do, thus the gateway value is the real thing, then use current R message, and skip T value.
      if (OTdata.type != OT_WRITE_ACK) {
        //ignore the request from the thermostat
        delayedOTdata.skipthis = skipthis;
      } else {
        //ignore the response from the boiler
        OTdata.skipthis = skipthis;
      }

      tmpOTdata = delayedOTdata;    //fetch delayed msg
      delayedOTdata = OTdata;       //store current msg
      OTdata = tmpOTdata;           //then process delayed msg

      // Decode and print OpenTherm Gateway Message
      switch (OTdata.rsptype){
        case OTGW_BOILER:
          OTGWDebugT("Boiler            ");
          break;
        case OTGW_THERMOSTAT:
          OTGWDebugT("Thermostat        ");
          break;
        case OTGW_REQUEST_BOILER:
          OTGWDebugT("Request Boiler    ");
          break;
        case OTGW_ANSWER_THERMOSTAT:
          OTGWDebugT("Answer Thermostat ");
          break;
        case OTGW_PARITY_ERROR:
          OTGWDebugT("Parity Error      ");
          break;
        default:
          OTGWDebugT("Unknown           ");
          break;
      }

      //print OTmessage to debug
      OTGWDebugf("%s (%d)", OTdata.buf, OTdata.len);
      //OTGWDebugf("[%08x]", OTdata.value);      //print message frame
      //OTGWDebugf("\ttype[%3d] id[%3d] hb[%3d] lb[%3d]\t", OTdata.type, OTdata.id, OTdata.valueHB, OTdata.valueLB);
      //print message Type and ID
      OTGWDebugf("[MsgID=%3d]", OTdata.id);
      OTGWDebugf("[%-16s]", messageTypeToString(static_cast<OpenThermMessageType>(OTdata.type)));
      OTGWDebugf("[%-30s]", messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)));
      // OTGWDebugf("[M=%d]",OTdata.master);
      OTGWDebug("\t");
      

      //keep track of update
      msglastupdated[OTdata.id] = now();

      //Read information from this OT message ready for use...
      PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);

      //next step interpret the OT protocol
      //On OT_WRITE_ACK or READ_ACK, or, status msgid's, then parse. 
          
        //#define OTprint(data, value, text, format) ({ data= value; OTGWDebugf("[%37s]", text); OTGWDebugf("= [format]", data)})
        //interpret values f8.8

        switch (static_cast<OpenThermMessageID>(OTdata.id)) {   
          case OT_Statusflags:                            print_status(OTdataObject.Statusflags); break;
          case OT_TSet:                                   print_f88(OTdataObject.TSet); break;         
          case OT_CoolingControl:                         print_f88(OTdataObject.CoolingControl); break;
          case OT_TsetCH2:                                print_f88(OTdataObject.TsetCH2); break;
          case OT_TrOverride:                             print_f88(OTdataObject.TrOverride); break;        
          case OT_MaxRelModLevelSetting:                  print_f88(OTdataObject.MaxRelModLevelSetting); break;
          case OT_TrSet:                                  print_f88(OTdataObject.TrSet); break;
          case OT_TrSetCH2:                               print_f88(OTdataObject.TrSetCH2); break;
          case OT_RelModLevel:                            print_f88(OTdataObject.RelModLevel); break;
          case OT_CHPressure:                             print_f88(OTdataObject.CHPressure); break;
          case OT_DHWFlowRate:                            print_f88(OTdataObject.DHWFlowRate); break;
          case OT_Tr:                                     print_f88(OTdataObject.Tr); break;  
          case OT_Tboiler:                                print_f88(OTdataObject.Tboiler);break;
          case OT_Tdhw:                                   print_f88(OTdataObject.Tdhw); break;
          case OT_Toutside:                               print_f88(OTdataObject.Toutside); break;
          case OT_Tret:                                   print_f88(OTdataObject.Tret); break;
          case OT_Tsolarstorage:                          print_f88(OTdataObject.Tsolarstorage); break;
          case OT_Tsolarcollector:                        print_s16(OTdataObject.Tsolarcollector); break;
          case OT_TflowCH2:                               print_f88(OTdataObject.TflowCH2); break;          
          case OT_Tdhw2:                                  print_f88(OTdataObject.Tdhw2 ); break;
          case OT_Texhaust:                               print_s16(OTdataObject.Texhaust); break; 
          case OT_Theatexchanger:                         print_f88(OTdataObject.Theatexchanger); break;
          case OT_TdhwSet:                                print_f88(OTdataObject.TdhwSet); break;
          case OT_MaxTSet:                                print_f88(OTdataObject.MaxTSet); break;
          case OT_Hcratio:                                print_f88(OTdataObject.Hcratio); break;
          case OT_Remoteparameter4:                       print_f88(OTdataObject.Remoteparameter4); break;
          case OT_Remoteparameter5:                       print_f88(OTdataObject.Remoteparameter5); break;
          case OT_Remoteparameter6:                       print_f88(OTdataObject.Remoteparameter6); break;
          case OT_Remoteparameter7:                       print_f88(OTdataObject.Remoteparameter7); break;
          case OT_Remoteparameter8:                       print_f88(OTdataObject.Remoteparameter8); break;
          case OT_OpenThermVersionMaster:                 print_f88(OTdataObject.OpenThermVersionMaster); break;
          case OT_OpenThermVersionSlave:                  print_f88(OTdataObject.OpenThermVersionSlave); break;
          case OT_ASFflags:                               print_ASFflags(OTdataObject.ASFflags); break;
          case OT_MasterConfigMemberIDcode:               print_mastermemberid(OTdataObject.MasterConfigMemberIDcode); break; 
          case OT_SlaveConfigMemberIDcode:                print_slavememberid(OTdataObject.SlaveConfigMemberIDcode); break;   
          case OT_Command:                                print_command(OTdataObject.Command );  break; 
          case OT_RBPflags:                               print_RBPflags(OTdataObject.RBPflags); break; 
          case OT_TSP:                                    print_u8u8(OTdataObject.TSP); break; 
          case OT_TSPindexTSPvalue:                       print_u8u8(OTdataObject.TSPindexTSPvalue); break; 
          case OT_FHBsize:                                print_u8u8(OTdataObject.FHBsize); break;  
          case OT_FHBindexFHBvalue:                       print_u8u8(OTdataObject.FHBindexFHBvalue); break; 
          case OT_MaxCapacityMinModLevel:                 print_u8u8(OTdataObject.MaxCapacityMinModLevel); break; 
          case OT_DayTime:                                print_daytime(OTdataObject.DayTime); break; 
          case OT_Date:                                   print_date(OTdataObject.Date); break; 
          case OT_Year:                                   print_u16(OTdataObject.Year); break; 
          case OT_TdhwSetUBTdhwSetLB:                     print_s8s8(OTdataObject.TdhwSetUBTdhwSetLB ); break;  
          case OT_MaxTSetUBMaxTSetLB:                     print_s8s8(OTdataObject.MaxTSetUBMaxTSetLB); break;  
          case OT_HcratioUBHcratioLB:                     print_s8s8(OTdataObject.HcratioUBHcratioLB); break; 
          case OT_Remoteparameter4boundaries:             print_s8s8(OTdataObject.Remoteparameter4boundaries); break;
          case OT_Remoteparameter5boundaries:             print_s8s8(OTdataObject.Remoteparameter5boundaries); break;
          case OT_Remoteparameter6boundaries:             print_s8s8(OTdataObject.Remoteparameter6boundaries); break;
          case OT_Remoteparameter7boundaries:             print_s8s8(OTdataObject.Remoteparameter7boundaries); break;
          case OT_Remoteparameter8boundaries:             print_s8s8(OTdataObject.Remoteparameter8boundaries); break;
          case OT_RemoteOverrideFunction:                 print_flag8(OTdataObject.RemoteOverrideFunction); break;
          case OT_OEMDiagnosticCode:                      print_u16(OTdataObject.OEMDiagnosticCode); break;
          case OT_BurnerStarts:                           print_u16(OTdataObject.BurnerStarts); break; 
          case OT_CHPumpStarts:                           print_u16(OTdataObject.CHPumpStarts); break; 
          case OT_DHWPumpValveStarts:                     print_u16(OTdataObject.DHWPumpValveStarts); break; 
          case OT_DHWBurnerStarts:                        print_u16(OTdataObject.DHWBurnerStarts); break;
          case OT_BurnerOperationHours:                   print_u16(OTdataObject.BurnerOperationHours); break;
          case OT_CHPumpOperationHours:                   print_u16(OTdataObject.CHPumpOperationHours); break; 
          case OT_DHWPumpValveOperationHours:             print_u16(OTdataObject.DHWPumpValveOperationHours); break;  
          case OT_DHWBurnerOperationHours:                print_u16(OTdataObject.DHWBurnerOperationHours); break; 
          case OT_MasterVersion:                          print_u8u8(OTdataObject.MasterVersion ); break; 
          case OT_SlaveVersion:                           print_u8u8(OTdataObject.SlaveVersion); break;
          case OT_StatusVH:                               print_statusVH(OTdataObject.StatusVH); break;
          case OT_ControlSetpointVH:                      print_u8u8(OTdataObject.ControlSetpointVH); break;
          case OT_ASFFaultCodeVH:                         print_flag8u8(OTdataObject.ASFFaultCodeVH); break;
          case OT_DiagnosticCodeVH:                       print_u16(OTdataObject.DiagnosticCodeVH); break;
          case OT_ConfigMemberIDVH:                       print_vh_configmemberid(OTdataObject.ConfigMemberIDVH); break;
          case OT_OpenthermVersionVH:                     print_f88(OTdataObject.OpenthermVersionVH); break;
          case OT_VersionTypeVH:                          print_u8u8(OTdataObject.VersionTypeVH ); break;
          case OT_RelativeVentilation:                    print_u8u8(OTdataObject.RelativeVentilation); break;
          case OT_RelativeHumidityExhaustAir:             print_u8u8(OTdataObject.RelativeHumidityExhaustAir); break;
          case OT_CO2LevelExhaustAir:                     print_u16(OTdataObject.CO2LevelExhaustAir); break;
          case OT_SupplyInletTemperature:                 print_f88(OTdataObject.SupplyInletTemperature); break;
          case OT_SupplyOutletTemperature:                print_f88(OTdataObject.SupplyOutletTemperature); break;
          case OT_ExhaustInletTemperature:                print_f88(OTdataObject.ExhaustInletTemperature); break;
          case OT_ExhaustOutletTemperature:               print_f88(OTdataObject.ExhaustOutletTemperature); break;
          case OT_ActualExhaustFanSpeed:                  print_u16(OTdataObject.ActualExhaustFanSpeed); break;
          case OT_ActualSupplyFanSpeed:                   print_u16(OTdataObject.ActualSupplyFanSpeed); break;
          case OT_RemoteParameterSettingVH:               print_vh_remoteparametersetting(OTdataObject.RemoteParameterSettingVH); break;
          case OT_NominalVentilationValue:                print_u8u8(OTdataObject.NominalVentilationValue); break;
          case OT_TSPNumberVH:                            print_u8u8(OTdataObject.TSPNumberVH); break;
          case OT_TSPEntryVH:                             print_u8u8(OTdataObject.TSPEntryVH); break;
          case OT_FaultBufferSizeVH:                      print_u8u8(OTdataObject.FaultBufferSizeVH); break;
          case OT_FaultBufferEntryVH:                     print_u8u8(OTdataObject.FaultBufferEntryVH); break;
          case OT_FanSpeed:                               print_u16(OTdataObject.FanSpeed); break;
          case OT_ElectricalCurrentBurnerFlame:           print_f88(OTdataObject.ElectricalCurrentBurnerFlame); break;
          case OT_TRoomCH2:                               print_f88(OTdataObject.TRoomCH2); break;
          case OT_RelativeHumidity:                       print_u8u8(OTdataObject.RelativeHumidity); break;
          case OT_RFstrengthbatterylevel:                 print_u8u8(OTdataObject.RFstrengthbatterylevel); break;
          case OT_OperatingMode_HC1_HC2_DHW:              print_u8u8(OTdataObject.OperatingMode_HC1_HC2_DHW ); break; 
          case OT_ElectricityProducerStarts:              print_u16(OTdataObject.ElectricityProducerStarts); break;
          case OT_ElectricityProducerHours:               print_u16(OTdataObject.ElectricityProducerHours); break;
          case OT_ElectricityProduction:                  print_u16(OTdataObject.ElectricityProduction); break;
          case OT_CumulativElectricityProduction:         print_u16(OTdataObject.CumulativElectricityProduction); break;
          case OT_RemehadFdUcodes:                        print_u8u8(OTdataObject.RemehadFdUcodes); break;
          case OT_RemehaServicemessage:                   print_u8u8(OTdataObject.RemehaServicemessage); break;
          case OT_RemehaDetectionConnectedSCU:            print_u8u8(OTdataObject.RemehaDetectionConnectedSCU); break;
          case OT_SolarStorageMaster:                     print_solar_storage_status(OTdataObject.SolarStorageStatus ); break;
          case OT_SolarStorageASFflags:                   print_flag8u8(OTdataObject.SolarStorageASFflags); break;
          case OT_SolarStorageSlaveConfigMemberIDcode:    print_solarstorage_slavememberid(OTdataObject.SolarStorageSlaveConfigMemberIDcode); break;
          case OT_SolarStorageVersionType:                print_u8u8(OTdataObject.SolarStorageVersionType); break;
          case OT_SolarStorageTSP:                        print_u8u8(OTdataObject.SolarStorageTSP ); break;
          case OT_SolarStorageTSPindexTSPvalue:           print_u8u8(OTdataObject.SolarStorageTSPindexTSPvalue ); break;
          case OT_SolarStorageFHBsize:                    print_u8u8(OTdataObject.SolarStorageFHBsize ); break;
          case OT_SolarStorageFHBindexFHBvalue:           print_u8u8(OTdataObject.SolarStorageFHBindexFHBvalue ); break;
          case OT_BurnerUnsuccessfulStarts:               print_u16(OTdataObject.BurnerUnsuccessfulStarts); break;
          case OT_FlameSignalTooLow:                      print_u16(OTdataObject.FlameSignalTooLow); break;
          default: 
              OTGWDebugTf("Unknown message [%02d] value [%04X] f8.8 [%3.2f] u16 [%d] s16 [%d]", OTdata.id, OTdata.value,  OTdata.f88(), OTdata.u16(), OTdata.s16());
              break;
        }
        if (OTdata.skipthis) OTGWDebug(" <ignored> ");
        OTGWDebugln(); // end of line
    } 
  } else if (buf[2]==':') { //seems to be a response to a command, so check to verify if it was
    checkOTGWcmdqueue(buf, len);
  } else if (strstr(buf, "\r\nError 01")!= NULL) {
    OTdataObject.error01++;
    OTGWDebugTf("\r\nError 01 = %d\r\n",OTdataObject.error01);
    sendMQTTData(F("Error 01"), String(OTdataObject.error01));
  } else if (strstr(buf, "Error 02")!= NULL) {
    OTdataObject.error02++;
    OTGWDebugTf("\r\nError 02 = %d\r\n",OTdataObject.error02);
    sendMQTTData(F("Error 02"), String(OTdataObject.error02));
  } else if (strstr(buf, "Error 03")!= NULL) {
    OTdataObject.error03++;
    OTGWDebugTf("\r\nError 03 = %d\r\n",OTdataObject.error03);
    sendMQTTData(F("Error 03"), String(OTdataObject.error03));
  } else if (strstr(buf, "Error 04")!= NULL){
    OTdataObject.error04++;
    OTGWDebugTf("\r\nError 04 = %d\r\n",OTdataObject.error04);
    sendMQTTData(F("Error 04"), String(OTdataObject.error04));
  } else OTGWDebugTf("\r\nNot processed, received from OTGW => [%s] [%d]\r\n", buf, len);
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
    if (outByte == '\r')
    { //on CR, do something...
      sWrite[bytes_write] = 0;
      OTGWDebugTf("Net2Ser: Sending to OTGW: [%s] (%d)\r\n", sWrite, bytes_write);
      //check for reset command
      if (stricmp(sWrite, "GW=R")==0){
        //detected [GW=R], then reset the gateway the gpio way
        OTGWDebugTln(F("Detected: GW=R. Reset gateway command executed."));
        resetOTGW();
      } else if (stricmp(sWrite, "PS=1")==0) {
        //detected [PS=1], then PrintSummary mode = true --> From this point on you need to ask for summary.
        bPrintSummarymode = true;
        //reset all msglastupdated in webui
        for(int i = 0; i <= OT_MSGID_MAX; i++){
          msglastupdated[i] = 0; //clear epoch values
        }
        sMessage = "PS=1 mode; No UI updates.";
      } else if (stricmp(sWrite, "PS=0")==0) {
        //detected [PS=0], then PrintSummary mode = OFF --> Raw mode is turned on again.
        bPrintSummarymode = false;
        sMessage = "";
      }
      bytes_write = 0; //start next line
    } else if  (outByte == '\n')
    {
      // on LF, skip 
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
    if (inByte== '\r')
    { //on CR, continue to process incoming message
      sRead[bytes_read] = 0;
      blinkLEDnow(LED2);
      processOTGW(sRead, bytes_read);
      bytes_read = 0;
      break; // to continue processing incoming message
    } 
    else if (inByte == '\n')
    { // on LF, just ignore... 
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
    case OT_TSet:                              return String(OTdataObject.TSet); break;         
    case OT_CoolingControl:                    return String(OTdataObject.CoolingControl); break;
    case OT_TsetCH2:                           return String(OTdataObject.TsetCH2);  break;
    case OT_TrOverride:                        return String(OTdataObject.TrOverride);  break;        
    case OT_MaxRelModLevelSetting:             return String(OTdataObject.MaxRelModLevelSetting);  break;
    case OT_TrSet:                             return String(OTdataObject.TrSet);  break;
    case OT_TrSetCH2:                          return String(OTdataObject.TrSetCH2);  break;
    case OT_RelModLevel:                       return String(OTdataObject.RelModLevel);  break;
    case OT_CHPressure:                        return String(OTdataObject.CHPressure); break;
    case OT_DHWFlowRate:                       return String(OTdataObject.DHWFlowRate);  break;
    case OT_Tr:                                return String(OTdataObject.Tr);  break;  
    case OT_Tboiler:                           return String(OTdataObject.Tboiler);  break;
    case OT_Tdhw:                              return String(OTdataObject.Tdhw);  break;
    case OT_Toutside:                          return String(OTdataObject.Toutside);  break;
    case OT_Tret:                              return String(OTdataObject.Tret);  break;
    case OT_Tsolarstorage:                     return String(OTdataObject.Tsolarstorage);  break;
    case OT_Tsolarcollector:                   return String(OTdataObject.Tsolarcollector); break;
    case OT_TflowCH2:                          return String(OTdataObject.TflowCH2); break;          
    case OT_Tdhw2:                             return String(OTdataObject.Tdhw2); break;
    case OT_Texhaust:                          return String(OTdataObject.Texhaust); break; 
    case OT_Theatexchanger:                    return String(OTdataObject.Theatexchanger); break;
    case OT_TdhwSet:                           return String(OTdataObject.TdhwSet); break;
    case OT_MaxTSet:                           return String(OTdataObject.MaxTSet); break;
    case OT_Hcratio:                           return String(OTdataObject.Hcratio); break;
    case OT_Remoteparameter4:                  return String(OTdataObject.Remoteparameter4); break;
    case OT_Remoteparameter5:                  return String(OTdataObject.Remoteparameter5); break;
    case OT_Remoteparameter6:                  return String(OTdataObject.Remoteparameter6); break;
    case OT_Remoteparameter7:                  return String(OTdataObject.Remoteparameter7); break;
    case OT_Remoteparameter8:                  return String(OTdataObject.Remoteparameter8); break;
    case OT_OpenThermVersionMaster:            return String(OTdataObject.OpenThermVersionMaster); break;
    case OT_OpenThermVersionSlave:             return String(OTdataObject.OpenThermVersionSlave); break;
    case OT_Statusflags:                       return String(OTdataObject.Statusflags); break;
    case OT_ASFflags:                          return String(OTdataObject.ASFflags); break;
    case OT_MasterConfigMemberIDcode:          return String(OTdataObject.MasterConfigMemberIDcode); break; 
    case OT_SlaveConfigMemberIDcode:           return String(OTdataObject.SlaveConfigMemberIDcode); break;   
    case OT_Command:                           return String(OTdataObject.Command);  break; 
    case OT_RBPflags:                          return String(OTdataObject.RBPflags); break; 
    case OT_TSP:                               return String(OTdataObject.TSP); break; 
    case OT_TSPindexTSPvalue:                  return String(OTdataObject.TSPindexTSPvalue);  break; 
    case OT_FHBsize:                           return String(OTdataObject.FHBsize);  break;  
    case OT_FHBindexFHBvalue:                  return String(OTdataObject.FHBindexFHBvalue);  break; 
    case OT_MaxCapacityMinModLevel:            return String(OTdataObject.MaxCapacityMinModLevel);  break; 
    case OT_DayTime:                           return String(OTdataObject.DayTime);  break; 
    case OT_Date:                              return String(OTdataObject.Date);  break; 
    case OT_Year:                              return String(OTdataObject.Year);  break; 
    case OT_TdhwSetUBTdhwSetLB:                return String(OTdataObject.TdhwSetUBTdhwSetLB); break;  
    case OT_MaxTSetUBMaxTSetLB:                return String(OTdataObject.MaxTSetUBMaxTSetLB); break;  
    case OT_HcratioUBHcratioLB:                return String(OTdataObject.HcratioUBHcratioLB); break; 
    case OT_Remoteparameter4boundaries:        return String(OTdataObject.Remoteparameter4boundaries); break; 
    case OT_Remoteparameter5boundaries:        return String(OTdataObject.Remoteparameter5boundaries); break;
    case OT_Remoteparameter6boundaries:        return String(OTdataObject.Remoteparameter6boundaries); break;
    case OT_Remoteparameter7boundaries:        return String(OTdataObject.Remoteparameter7boundaries); break;
    case OT_Remoteparameter8boundaries:        return String(OTdataObject.Remoteparameter8boundaries); break;     
    case OT_RemoteOverrideFunction:            return String(OTdataObject.RemoteOverrideFunction); break;
    case OT_OEMDiagnosticCode:                 return String(OTdataObject.OEMDiagnosticCode);  break;
    case OT_BurnerStarts:                      return String(OTdataObject.BurnerStarts);  break; 
    case OT_CHPumpStarts:                      return String(OTdataObject.CHPumpStarts);  break; 
    case OT_DHWPumpValveStarts:                return String(OTdataObject.DHWPumpValveStarts);  break; 
    case OT_DHWBurnerStarts:                   return String(OTdataObject.DHWBurnerStarts);  break;
    case OT_BurnerOperationHours:              return String(OTdataObject.BurnerOperationHours);  break;
    case OT_CHPumpOperationHours:              return String(OTdataObject.CHPumpOperationHours);  break; 
    case OT_DHWPumpValveOperationHours:        return String(OTdataObject.DHWPumpValveOperationHours);  break;  
    case OT_DHWBurnerOperationHours:           return String(OTdataObject.DHWBurnerOperationHours);  break; 
    case OT_MasterVersion:                     return String(OTdataObject.MasterVersion); break; 
    case OT_SlaveVersion:                      return String(OTdataObject.SlaveVersion); break;
    case OT_StatusVH:                          return String(OTdataObject.StatusVH); break;
    case OT_ControlSetpointVH:                 return String(OTdataObject.ControlSetpointVH); break;
    case OT_ASFFaultCodeVH:                    return String(OTdataObject.ASFFaultCodeVH); break;
    case OT_DiagnosticCodeVH:                  return String(OTdataObject.DiagnosticCodeVH); break;
    case OT_ConfigMemberIDVH:                  return String(OTdataObject.ConfigMemberIDVH); break;
    case OT_OpenthermVersionVH:                return String(OTdataObject.OpenthermVersionVH); break;
    case OT_VersionTypeVH:                     return String(OTdataObject.VersionTypeVH); break;
    case OT_RelativeVentilation:               return String(OTdataObject.RelativeVentilation); break;
    case OT_RelativeHumidityExhaustAir:        return String(OTdataObject.RelativeHumidityExhaustAir); break;
    case OT_CO2LevelExhaustAir:                return String(OTdataObject.CO2LevelExhaustAir); break;
    case OT_SupplyInletTemperature:            return String(OTdataObject.SupplyInletTemperature); break;
    case OT_SupplyOutletTemperature:           return String(OTdataObject.SupplyOutletTemperature); break;
    case OT_ExhaustInletTemperature:           return String(OTdataObject.ExhaustInletTemperature); break;
    case OT_ExhaustOutletTemperature:          return String(OTdataObject.ExhaustOutletTemperature); break;
    case OT_ActualExhaustFanSpeed:             return String(OTdataObject.ActualExhaustFanSpeed); break;
    case OT_ActualSupplyFanSpeed:              return String(OTdataObject.ActualSupplyFanSpeed); break;
    case OT_RemoteParameterSettingVH:          return String(OTdataObject.RemoteParameterSettingVH); break;
    case OT_NominalVentilationValue:           return String(OTdataObject.NominalVentilationValue); break;
    case OT_TSPNumberVH:                       return String(OTdataObject.TSPNumberVH); break;
    case OT_TSPEntryVH:                        return String(OTdataObject.TSPEntryVH); break;
    case OT_FaultBufferSizeVH:                 return String(OTdataObject.FaultBufferSizeVH); break;
    case OT_FaultBufferEntryVH:                return String(OTdataObject.FaultBufferEntryVH); break;
    case OT_FanSpeed:                          return String(OTdataObject.FanSpeed); break;
    case OT_ElectricalCurrentBurnerFlame:      return String(OTdataObject.ElectricalCurrentBurnerFlame); break;
    case OT_TRoomCH2:                          return String(OTdataObject.TRoomCH2); break;
    case OT_RelativeHumidity:                  return String(OTdataObject.RelativeHumidity); break;
    case OT_RFstrengthbatterylevel:            return String(OTdataObject.RFstrengthbatterylevel); break;
    case OT_OperatingMode_HC1_HC2_DHW:         return String(OTdataObject.OperatingMode_HC1_HC2_DHW); break;
    case OT_ElectricityProducerStarts:         return String(OTdataObject.ElectricityProducerStarts); break;
    case OT_ElectricityProducerHours:          return String(OTdataObject.ElectricityProducerHours); break;
    case OT_ElectricityProduction:             return String(OTdataObject.ElectricityProduction); break;
    case OT_CumulativElectricityProduction:    return String(OTdataObject.CumulativElectricityProduction); break;
    case OT_RemehadFdUcodes:                   return String(OTdataObject.RemehadFdUcodes); break;
    case OT_RemehaServicemessage:              return String(OTdataObject.RemehaServicemessage); break;
    case OT_RemehaDetectionConnectedSCU:       return String(OTdataObject.RemehaDetectionConnectedSCU); break;
    case OT_SolarStorageMaster:                return String(OTdataObject.SolarStorageStatus); break;
    case OT_SolarStorageASFflags:              return String(OTdataObject.SolarStorageASFflags); break;
    case OT_SolarStorageSlaveConfigMemberIDcode:  return String(OTdataObject.SolarStorageSlaveConfigMemberIDcode); break;
    case OT_SolarStorageVersionType:           return String(OTdataObject.SolarStorageVersionType); break;
    case OT_SolarStorageTSP:                   return String(OTdataObject.SolarStorageTSP); break;
    case OT_SolarStorageTSPindexTSPvalue:      return String(OTdataObject.SolarStorageTSPindexTSPvalue); break;
    case OT_SolarStorageFHBsize:               return String(OTdataObject.SolarStorageFHBsize); break;
    case OT_SolarStorageFHBindexFHBvalue:      return String(OTdataObject.SolarStorageFHBindexFHBvalue); break;

    default: return "Error: not implemented yet!\r\n";
  } 
}

void startOTGWstream()
{
  OTGWstream.begin();
}

void upgradepicnow(const char *filename) {
  if (OTGWSerial.busy()) return; // if already in programming mode, never call it twice
  OTGWDebugTln(F("Start PIC upgrade now."));
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
  
  switch (result) {
    case OTGW_ERROR_NONE:          errorupgrade = "PIC upgrade was succesful"; break;
    case OTGW_ERROR_MEMORY:        errorupgrade = "Not enough memory available"; break;
    case OTGW_ERROR_INPROG:        errorupgrade = "Firmware upgrade in progress"; break;
    case OTGW_ERROR_HEX_ACCESS:    errorupgrade = "Could not open hex file"; break;
    case OTGW_ERROR_HEX_FORMAT:    errorupgrade = "Invalid format of hex file"; break;
    case OTGW_ERROR_HEX_DATASIZE:  errorupgrade = "Wrong data size in hex file"; break;
    case OTGW_ERROR_HEX_CHECKSUM:  errorupgrade = "Bad checksum in hex file"; break;
    case OTGW_ERROR_MAGIC:         errorupgrade = "Hex file does not contain expected data"; break;
    case OTGW_ERROR_RESET:         errorupgrade = "PIC reset failed"; break;
    case OTGW_ERROR_RETRIES:       errorupgrade = "Too many retries"; break;
    case OTGW_ERROR_MISMATCHES:    errorupgrade = "Too many mismatches"; break;
    default:                       errorupgrade = "Unknown state"; break;
  }
  OTGWDebugTf("Upgrade finished: Errorcode = %d - %s - %d retries, %d errors\n", result, CSTR(errorupgrade), retries, errors);
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
      OTGWDebugTf("%s: %s\r\n", hexheaders[i], http.header(i).c_str());
    }
    latest = http.header(1);
    OTGWDebugTf("Update %s -> %s\r\n", filename.c_str(), latest.c_str());
    http.end();
  } else OTGWDebugln("Failed to fetch version from Schelte Bron website");
  return latest; 
}

void refreshpic(String filename, String version) {
  WiFiClient client;
  HTTPClient http;
  String latest;
  int code;

  if (latest=checkforupdatepic(filename) != "") {
    if (latest != version) {
      OTGWDebugTf("Update %s: %s -> %s\r\n", filename.c_str(), version.c_str(), latest.c_str());
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
            OTGWDebugTf("Update successful\n");
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
  OTGWDebugTf("Action: %s %s %s\r\n", action.c_str(), filename.c_str(), version.c_str());
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
