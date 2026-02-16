/* 
***************************************************************************  
**  Program  : OTGW-Core.ino
**  Version  : v1.1.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
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
#define OTGWDebugFlush()  ({ if (bDebugOTmsg) DebugFlush();    })

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

/* --- LOG marcro's ---*/

#define OT_LOG_BUFFER_SIZE 512
char ot_log_buffer[OT_LOG_BUFFER_SIZE];

#define ClrLog()            ({ ot_log_buffer[0] = '\0'; })
#define AddLogf(...)        ({ size_t _len = strlen(ot_log_buffer); if (_len < (OT_LOG_BUFFER_SIZE - 1)) { snprintf(ot_log_buffer + _len, OT_LOG_BUFFER_SIZE - _len, __VA_ARGS__); } })
#define AddLog(logstring)   ({ size_t _len = strlen(ot_log_buffer); if (_len < (OT_LOG_BUFFER_SIZE - 1)) { strlcat(ot_log_buffer, logstring, OT_LOG_BUFFER_SIZE); } })
#define AddLogln()          ({ size_t _len = strlen(ot_log_buffer); if (_len < (OT_LOG_BUFFER_SIZE - 1)) { strlcat(ot_log_buffer, "\r\n", OT_LOG_BUFFER_SIZE); } })

/* --- End of LOG marcro's ---*/

static const char* skipOTLogTimestamp(const char* logLine)
{
  if (!logLine) return logLine;
  if (strlen(logLine) < 16) return logLine;

  const bool hasTimestamp =
      (logLine[0] >= '0' && logLine[0] <= '9') &&
      (logLine[1] >= '0' && logLine[1] <= '9') &&
      (logLine[2] == ':') &&
      (logLine[3] >= '0' && logLine[3] <= '9') &&
      (logLine[4] >= '0' && logLine[4] <= '9') &&
      (logLine[5] == ':') &&
      (logLine[6] >= '0' && logLine[6] <= '9') &&
      (logLine[7] >= '0' && logLine[7] <= '9') &&
      (logLine[8] == '.') &&
      (logLine[9] >= '0' && logLine[9] <= '9') &&
      (logLine[10] >= '0' && logLine[10] <= '9') &&
      (logLine[11] >= '0' && logLine[11] <= '9') &&
      (logLine[12] >= '0' && logLine[12] <= '9') &&
      (logLine[13] >= '0' && logLine[13] <= '9') &&
      (logLine[14] >= '0' && logLine[14] <= '9') &&
      (logLine[15] == ' ');

  return hasTimestamp ? (logLine + 16) : logLine;
}


//some variable's
OpenthermData_t OTdata, delayedOTdata, tmpOTdata;

#define OTGW_BANNER "OpenTherm Gateway"

//===================[ Send useful information to MQWTT ]=====================

/*
Publish usefull firmware version information to MQTT broker.
*/
void sendMQTTversioninfo(){
  char rebootCountBuf[12];
  snprintf_P(rebootCountBuf, sizeof(rebootCountBuf), PSTR("%lu"), static_cast<unsigned long>(rebootCount));
  sendMQTTData("otgw-firmware/version", _SEMVER_FULL);
  sendMQTTData("otgw-firmware/reboot_count", rebootCountBuf);
  sendMQTTData("otgw-firmware/reboot_reason", lastReset);
  sendMQTTData("otgw-pic/version", sPICfwversion);
  sendMQTTData("otgw-pic/deviceid", sPICdeviceid);
  sendMQTTData("otgw-pic/firmwaretype", sPICdeviceid);
  sendMQTTData("otgw-pic/picavailable", CCONOFF(bPICavailable));
}

/*
Publish state information of PIC firmware version information to MQTT broker.
*/
void sendMQTTstateinformation(){
  sendMQTTData(F("otgw-pic/boiler_connected"), CCONOFF(bOTGWboilerstate)); 
  sendMQTTData(F("otgw-pic/thermostat_connected"), CCONOFF(bOTGWthermostatstate));
  sendMQTTData(F("otgw-pic/gateway_mode"), CCONOFF(bOTGWgatewaystate));
  sendMQTTData(F("otgw-pic/otgw_connected"), CCONOFF(bOTGWonline));
  sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(bOTGWonline));
}

//===================[ Reset OTGW ]===============================
void resetOTGW() {
  OTGWSerial.resetPic();
}

/*
  To detect the pic, reset the pic, then find ETX in the response after reset (within 1 second).
  The ETX response is send by the bootload, when received it also means you have a pic connected.
*/
void detectPIC(){
  OTGWSerial.registerFirmwareCallback(fwreportinfo); //register the callback to report version, type en device ID
  OTGWSerial.resetPic(); // make sure it the firmware is detected
  bPICavailable = OTGWSerial.find(ETX);
  if (bPICavailable) {
      DebugTln(F("ETX found after reset: Pic detected!"));
  } else {
      DebugTln(F("No ETX found after reset: no Pic detected!"));
  }
}

//===================[ getpicfwversion ]===========================
/*
Get the information of the pic firmware: version  number, device type and firmware type. 
This is done by sending a PR=A command, requesting a banner from the PIC. This will trigger detection of version.
*/
String getpicfwversion(){
  String _ret="";

  String line = executeCommand("PR=A");
  int p = line.indexOf(OTGW_BANNER);
  if (p >= 0) {
    p += sizeof(OTGW_BANNER)-1;
    _ret = line.substring(p);
  } else {
    _ret ="No version found";
  }
  OTGWDebugTf(PSTR("getpicfwversion: Current firmware version: %s\r\n"), CSTR(_ret));
  _ret.trim();
  return _ret;
}
//===================[ queryOTGWgatewaymode ]======================
/*
Query the actual gateway mode setting from the PIC firmware using PR=M command.
According to OTGW documentation:
- PR=M returns "PR: G" for Gateway mode (GW=1)
- PR=M returns "PR: M" for Monitor/Standalone mode (GW=0)
This provides a reliable way to detect the actual configured mode,
rather than inferring it from message traffic.
*/
bool queryOTGWgatewaymode(){
  static uint32_t lastGatewayModeQueryMs = 0;
  static bool cachedGatewayMode = false;
  static bool hasCachedGatewayMode = false;
  constexpr uint32_t GATEWAY_MODE_QUERY_MIN_INTERVAL_MS = 60000; // hard throttle: max one PR=M per minute

  if (!bPICavailable) {
    OTGWDebugTln(F("queryOTGWgatewaymode: PIC not available"));
    return false;
  }

  const uint32_t now = millis();
  if (hasCachedGatewayMode && ((uint32_t)(now - lastGatewayModeQueryMs) < GATEWAY_MODE_QUERY_MIN_INTERVAL_MS)) {
    OTGWDebugTf(PSTR("queryOTGWgatewaymode: throttled, using cached value [%s]\r\n"), CCONOFF(cachedGatewayMode));
    return cachedGatewayMode;
  }
  
  String response = executeCommand("PR=M");
  response.trim();
  
  OTGWDebugTf(PSTR("queryOTGWgatewaymode: PR=M response=[%s]\r\n"), CSTR(response));
  
  // Response should be "G" for Gateway mode or "M" for Monitor mode
  // executeCommand() strips the "PR: " prefix, so we just get the value
  bool isGatewayMode = false;
  
  if (response.length() > 0) {
    char mode = response.charAt(0);
    if (mode == 'G' || mode == 'g') {
      isGatewayMode = true;
      OTGWDebugTln(F("queryOTGWgatewaymode: Gateway mode (G) detected"));
    } else if (mode == 'M' || mode == 'm') {
      isGatewayMode = false;
      OTGWDebugTln(F("queryOTGWgatewaymode: Monitor mode (M) detected"));
    } else {
      OTGWDebugTf(PSTR("queryOTGWgatewaymode: Unexpected response [%s], defaulting to false\r\n"), CSTR(response));
      isGatewayMode = false;
    }
  } else {
    OTGWDebugTln(F("queryOTGWgatewaymode: Empty response, defaulting to false"));
  }

  cachedGatewayMode = isGatewayMode;
  hasCachedGatewayMode = true;
  lastGatewayModeQueryMs = now;
  
  return isGatewayMode;
}

//===================[ checkOTWGpicforupdate ]=====================
void checkOTWGpicforupdate(){
  if (sPICfwversion[0] == '\0') {
    sMessage[0] = '\0'; //no firmware version found for some reason
  } else {
    OTGWDebugTf(PSTR("OTGW PIC firmware version = [%s]\r\n"), sPICfwversion);
    String latest = checkforupdatepic("gateway.hex");
    if (!bOTGWonline) {
      strlcpy(sMessage, sPICfwversion, sizeof(sMessage)); 
    } else if (latest.isEmpty()) {
      sMessage[0] = '\0'; //two options: no internet connection OR no firmware version
    } else if (latest != String(sPICfwversion)) {
      snprintf_P(sMessage, sizeof(sMessage), PSTR("New PIC version %s available!"), latest.c_str());
    }
  }
  //check if the esp8266 and the littlefs versions match
  if (!checklittlefshash()) {
    DebugTln(F("WARNING: Firmware and filesystem version mismatch detected!"));
    snprintf_P(sMessage, sizeof(sMessage), PSTR("Flash your littleFS with matching version!"));
  }
}

//===================[ sendOTGWbootcmd ]=====================
void sendOTGWbootcmd(){
  if (!settingOTGWcommandenable) return;
  OTGWDebugTf(PSTR("OTGW boot message = [%s]\r\n"), CSTR(settingOTGWcommands));

  // parse and execute commands
  char bootcmds[129];
  strlcpy(bootcmds, settingOTGWcommands, sizeof(bootcmds));
  
  char* cmd;
  int i = 0;
  cmd = strtok(bootcmds, ";");
  while (cmd != NULL) {
    OTGWDebugTf(PSTR("Boot command[%d]: %s\r\n"), i++, cmd);
    addOTWGcmdtoqueue(cmd, strlen(cmd), true);
    cmd = strtok(NULL, ";");
  }
}

//===================[ OTGW Command & Response ]===================
String executeCommand(const String sCmd){
  //send command to OTGW
  OTGWDebugTf(PSTR("OTGW Send Cmd [%s]\r\n"), CSTR(sCmd));
  if (sCmd.length() < 2) {
    OTGWDebugTln(F("Send command too short"));
    return "SE - Command too short.";
  }
  OTGWSerial.setTimeout(1000);
  DECLARE_TIMER_MS(tmrWaitForIt, 1000);
  while((OTGWSerial.availableForWrite() < (int)(sCmd.length()+2)) && !DUE(tmrWaitForIt)){
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
  OTGWDebugTf(PSTR("Send command: [%s]\r\n"), CSTR(_cmd));
  //fetch a line
  String line = OTGWSerial.readStringUntil('\n');
  
  // Safety check: Prevent memory exhaustion from malformed serial data
  // OTGW responses should be <100 bytes typically, CMSG_SIZE (512) is generous limit
  if (line.length() > CMSG_SIZE) {
    OTGWDebugTf(PSTR("WARNING: OTGW response too long (%d bytes), truncating to %d\r\n"), line.length(), CMSG_SIZE);
    line = line.substring(0, CMSG_SIZE);
  }
  
  line.trim();
  String _ret ="";
  if (line.length() >= 3 && line.startsWith(_cmd) && line.charAt(2) == ':'){
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
  OTGWDebugTf(PSTR("Command send [%s]-[%s] - Response line: [%s] - Returned value: [%s]\r\n"), CSTR(sCmd), CSTR(_cmd), CSTR(line), CSTR(_ret));
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
  
  delay(100);
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
      ReasonReset = "Reset by External WD\r\n";
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

//===[ Feed the WatchDog before it bites! ]===
void feedWatchDog() {
  //Feed the watchdog at most every 100ms to prevent hardware watchdog resets
  //during blocking operations while limiting I2C bus traffic
  //==== feed the WD over I2C ==== 
  // Address: 0x26
  // I2C Watchdog feed - rate limited to 100ms
  DECLARE_TIMER_MS(timerWD, 100, SKIP_MISSED_TICKS);
  if DUE(timerWD)
  {
    Wire.beginTransmission(EXT_WD_I2C_ADDRESS);   //Nodoshop design uses the hardware WD on I2C, address 0x26
    Wire.write(0xA5);                             //Feed the dog, before it bites.
    Wire.endTransmission();                       //That's all there is...

  }

  //==== blink LED1 once a second to show we are alive ====
  DECLARE_TIMER_MS(timerWDBlink, 1000, SKIP_MISSED_TICKS);
  if DUE(timerWDBlink)
  {
    blinkLEDnow(LED1);
  }
  //yield(); 
}

//===================[ END Watchdog OTGW ]===============================

//=======================================================================
float OpenthermData_t::f88() {
  float value = (int8_t) valueHB;
  return value + (float)valueLB / 256.0f;
}

void OpenthermData_t::f88(float value) {
  if (value >= 0) {
    valueHB = (byte) value;
    float fraction = (value - valueHB);
    valueLB = fraction * 256.0f;
  }
  else {
    valueHB = (byte)(value - 1);
    float fraction = (value - valueHB - 1);
    valueLB = fraction * 256.0f;
  }
}

uint16_t OpenthermData_t::u16() {
  uint16_t value = valueHB;
  return ((value << 8) + valueLB);
}

void OpenthermData_t::u16(uint16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData_t::s16() {
  int16_t value = valueHB;
  return ((value << 8) + valueLB);
}

void OpenthermData_t::s16(int16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

//parsing helpers
const char *statusToString(OpenThermResponseStatus status)
{
	switch (status) {
		case OT_NONE:    return "None";
		case OT_SUCCESS: return "Success";
		case OT_INVALID: return "Invalid";
		case OT_TIMEOUT: return "Timeout";
		default:         return "Unknown";
	}
}

const char *messageTypeToString(OpenThermMessageType message_type)
{
	switch (message_type) {
		case OT_READ_DATA:        return "Read-Data";
		case OT_WRITE_DATA:       return "Write-Data";
		case OT_INVALID_DATA:     return "Invalid-Data";
		case OT_RESERVED:         return "Reserved";
		case OT_READ_ACK:         return "Read-Ack";
		case OT_WRITE_ACK:        return "Write-Ack";
		case OT_DATA_INVALID:     return "Data-Invalid";
		case OT_UNKNOWN_DATA_ID:  return "Unknown-Data-Id";
		default:                  return "Unknown";
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
	return OTcurrentSystemState.MasterStatus & 0x01;
}

bool isDomesticHotWaterEnabled() {
	return OTcurrentSystemState.MasterStatus & 0x02;
}

bool isCoolingEnabled() {
	return OTcurrentSystemState.MasterStatus & 0x04;
}

bool isOutsideTemperatureCompensationActive() {
	return OTcurrentSystemState.MasterStatus & 0x08;
}

bool isCentralHeating2enabled() {
	return OTcurrentSystemState.MasterStatus & 0x10;
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
	return OTcurrentSystemState.SlaveStatus & 0x01;
}

bool isCentralHeatingActive() {
	return OTcurrentSystemState.SlaveStatus & 0x02;
}

bool isDomesticHotWaterActive() {
	return OTcurrentSystemState.SlaveStatus & 0x04;
}

bool isFlameStatus() {
	return OTcurrentSystemState.SlaveStatus & 0x08;
}

bool isCoolingActive() {
	return OTcurrentSystemState.SlaveStatus & 0x10;
}

bool isCentralHeating2Active() {
	return OTcurrentSystemState.SlaveStatus & 0x20;
}

bool isDiagnosticIndicator() {
	return OTcurrentSystemState.SlaveStatus & 0x40;
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
	return OTcurrentSystemState.ASFflags & 0x0100;
}

bool isLockoutReset() {
	return OTcurrentSystemState.ASFflags & 0x0200;
}

bool isLowWaterPressure() {
	return OTcurrentSystemState.ASFflags & 0x0400;
}

bool isGasFlameFault() {
	return OTcurrentSystemState.ASFflags & 0x0800;
}

bool isAirTemperature() {
	return OTcurrentSystemState.ASFflags & 0x1000;
}

bool isWaterOverTemperature() {
	return OTcurrentSystemState.ASFflags & 0x2000;
}

const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1) {
        strlcat(b, ((x & z) == z) ? "1" : "0", sizeof(b));
    }

    return b;
}  //byte_to_binary


/*
  This determines if the value in the OpenTherm message is valid and can be used in the data object, MQTT or REST API.
  Rules are:
  - if the message is overriden (R and A messages override B and T messages), then the value is not valid for use.
  - if the OT message is a READ message, and the received OT msg is being read and acknowledged, then the value is valid.
  - if the OT message is a WRITE message, and the received OT msg is being written (OT_WRITE_DATA), then the value is valid.
  - if the OT message is a READ/WRITE message, and receive OT msg is being read and ackownledge, or, is being written, then the value is valid.
  - if the OT message is a status message (from Heating, HAVC or Solar), then the message is always valid.
*/
bool is_value_valid(OpenthermData_t OT, OTlookup_t OTlookup) {
  if (OT.skipthis) return false;
  bool _valid = false;
  _valid = _valid || (OTlookup.msgcmd==OT_READ && OT.type==OT_READ_ACK);
  _valid = _valid || (OTlookup.msgcmd==OT_WRITE && OT.type==OT_WRITE_DATA);
  _valid = _valid || (OTlookup.msgcmd==OT_RW && (OT.type==OT_READ_ACK || OT.type==OT_WRITE_DATA));
  _valid = _valid || (OT.id==OT_Statusflags) || (OT.id==OT_StatusVH) || (OT.id==OT_SolarStorageMaster);;
  return _valid;
}

void print_f88(float& value)
{
  //function to print data
  float _value = roundf(OTdata.f88()*100.0f) / 100.0f; // round float 2 digits, like this: x.xx 
  // AddLog("%s = %3.2f %s", OTlookupitem.label, _value , OTlookupitem.unit);
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  
  AddLogf("%s = %s %s", OTlookupitem.label, _msg , OTlookupitem.unit);

  //SendMQTT
  if (is_value_valid(OTdata, OTlookupitem)){
    const char* baseTopic = messageIDToString(static_cast<OpenThermMessageID>(OTdata.id));
    
    // Always publish to original topic (backward compatibility)
    sendMQTTData(baseTopic, _msg);
    
    // ADR-040: If feature enabled, also publish to source-specific topic
    publishToSourceTopic(baseTopic, _msg, OTdata.rsptype);
    
    value = _value;
  }
}
  

void print_s16(int16_t& value)
{    
  int16_t _value = OTdata.s16(); 
  // AddLogf("%s = %5d %s", OTlookupitem.label, _value, OTlookupitem.unit);
  //Build string for MQTT
  char _msg[15] {0};
  itoa(_value, _msg, 10);
  AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);

  //SendMQTT
  if (is_value_valid(OTdata, OTlookupitem)){
    const char* baseTopic = messageIDToString(static_cast<OpenThermMessageID>(OTdata.id));
    
    // Always publish to original topic (backward compatibility)
    sendMQTTData(baseTopic, _msg);
    
    // ADR-040: If feature enabled, also publish to source-specific topic
    publishToSourceTopic(baseTopic, _msg, OTdata.rsptype);
    
    value = _value;
  }
}

void print_s8s8(uint16_t& value)
{  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (int8_t)OTdata.valueHB, (int8_t)OTdata.valueLB, OTlookupitem.unit);

  //Build string for MQTT
  char _msg[15] {0};
  char _topic[50] {0};
  const char* baseTopicStr = messageIDToString(static_cast<OpenThermMessageID>(OTdata.id));
  
  // Publish HB (high byte)
  itoa((int8_t)OTdata.valueHB, _msg, 10);
  strlcpy(_topic, baseTopicStr, sizeof(_topic));
  strlcat(_topic, "_value_hb", sizeof(_topic));
  
  if (is_value_valid(OTdata, OTlookupitem)){
    // Always publish to original topic (backward compatibility)
    sendMQTTData(_topic, _msg);
    
    // ADR-040: If feature enabled, also publish to source-specific topic
    publishToSourceTopic(_topic, _msg, OTdata.rsptype);
  }
  
  // Publish LB (low byte)
  itoa((int8_t)OTdata.valueLB, _msg, 10);
  strlcpy(_topic, baseTopicStr, sizeof(_topic));
  strlcat(_topic, "_value_lb", sizeof(_topic));
  
  if (is_value_valid(OTdata, OTlookupitem)){
    // Always publish to original topic (backward compatibility)
    sendMQTTData(_topic, _msg);
    
    // ADR-040: If feature enabled, also publish to source-specific topic
    publishToSourceTopic(_topic, _msg, OTdata.rsptype);
    
    value = OTdata.u16();
  }
}

void print_u16(uint16_t& value)
{ 
  uint16_t _value = OTdata.u16(); 
  //Build string for MQTT
  char _msg[15] {0};
  utoa(_value, _msg, 10);
  
  AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);
  
  //SendMQTT
  if (is_value_valid(OTdata, OTlookupitem)){
    const char* baseTopic = messageIDToString(static_cast<OpenThermMessageID>(OTdata.id));
    
    // Always publish to original topic (backward compatibility)
    sendMQTTData(baseTopic, _msg);
    
    // ADR-040: If feature enabled, also publish to source-specific topic
    publishToSourceTopic(baseTopic, _msg, OTdata.rsptype);
    
    value = _value;
  }
}

void print_status(uint16_t& value)
{
  char _flag8_master[9] {0};
  char _flag8_slave[9] {0};
  
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
    _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? 'S' : 'W'); 
    _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? 'B' : '-'); 
    _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
    _flag8_master[8] = '\0';

    AddLog(" ");
    AddLog(OTlookupitem.label);
    AddLogf(" = Master [%s]", _flag8_master);

    //Master Status
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData("status_master", _flag8_master);
      publishMQTTOnOff("ch_enable",        ((OTdata.valueHB) & 0x01));
      publishMQTTOnOff("dhw_enable",       ((OTdata.valueHB) & 0x02));
      publishMQTTOnOff("cooling_enable",   ((OTdata.valueHB) & 0x04));
      publishMQTTOnOff("otc_active",       ((OTdata.valueHB) & 0x08));
      publishMQTTOnOff("ch2_enable",       ((OTdata.valueHB) & 0x10));
      publishMQTTOnOff("summerwintertime", ((OTdata.valueHB) & 0x20));
      publishMQTTOnOff("dhw_blocking",     ((OTdata.valueHB) & 0x40));

      OTcurrentSystemState.MasterStatus = OTdata.valueHB;
    }
  } else {
    // Parse slave bits
    //  0: fault indication [ no fault, fault ]
    //  1: CH mode [CH not active, CH active]
    //  2: DHW mode [ DHW not active, DHW active]
    //  3: Flame status [ flame off, flame on ]
    //  4: Cooling status [ cooling mode not active, cooling mode active ]
    //  5: CH2 mode [CH2 not active, CH2 active]
    //  6: diagnostic indication [no diagnostics, diagnostic event]
    //  7: Electricity production [no electric production, electric production]
    _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'E' : '-');
    _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'C' : '-'); 
    _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'W' : '-'); 
    _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'F' : '-'); 
    _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'C' : '-'); 
    _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '2' : '-'); 
    _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-'); 
    _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? 'P' : '-');
    _flag8_slave[8] = '\0';

    AddLog(" ");
    AddLog(OTlookupitem.label);
    AddLogf(" = Slave  [%s]", _flag8_slave);
    
    //Slave Status
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData("status_slave", _flag8_slave);
      publishMQTTOnOff("fault",                ((OTdata.valueLB) & 0x01));
      publishMQTTOnOff("centralheating",       ((OTdata.valueLB) & 0x02));
      publishMQTTOnOff("domestichotwater",     ((OTdata.valueLB) & 0x04));
      publishMQTTOnOff("flame",                ((OTdata.valueLB) & 0x08));
      publishMQTTOnOff("cooling",              ((OTdata.valueLB) & 0x10));
      publishMQTTOnOff("centralheating2",      ((OTdata.valueLB) & 0x20));
      publishMQTTOnOff("diagnostic_indicator", ((OTdata.valueLB) & 0x40));
      publishMQTTOnOff("electric_production",   ((OTdata.valueLB) & 0x80));

      OTcurrentSystemState.SlaveStatus = OTdata.valueLB;
    }
  }

  if (is_value_valid(OTdata, OTlookupitem)){
    // AddLogf("Status u16 [%04x] _value [%04x] hb [%02x] lb [%02x]", OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
    value = (OTcurrentSystemState.MasterStatus<<8) | OTcurrentSystemState.SlaveStatus;
  }
}

void print_solar_storage_status(uint16_t& value)
{ 
  char _msg[15] {0};

  if (OTdata.masterslave == 0) {
    // Master Solar Storage 
    // ID101:HB012: Master Solar Storage: Solar mode
    uint8_t MasterSolarMode = (OTdata.valueHB) & 0x7;
    AddLogf("%s = Solar Storage Master Mode [%d] ", OTlookupitem.label, MasterSolarMode);
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData(F("solar_storage_master_mode"), itoa(MasterSolarMode, _msg, 10));  //delayms(5);
      OTcurrentSystemState.SolarMasterStatus = OTdata.valueHB;
    }
  } else { 
    //Slave
    // ID101:LB0: Slave Solar Storage: Fault indication
    uint8_t SlaveSolarFaultIndicator =  (OTdata.valueLB) & 0x01;
    // ID101:LB123: Slave Solar Storage: Solar mode status
    uint8_t SlaveSolarModeStatus = (OTdata.valueLB>>1) & 0x07;
    // ID101:LB45: Slave Solar Storage: Solar status
    uint8_t SlaveSolarStatus = (OTdata.valueLB>>4)& 0x03;
    AddLogf("\r\n%s = Slave Solar Fault Indicator [%d] ", OTlookupitem.label, SlaveSolarFaultIndicator);
    AddLogf("\r\n%s = Slave Solar Mode Status [%d] ", OTlookupitem.label, SlaveSolarModeStatus);
    AddLogf("\r\n%s = Slave Solar Status [%d] ", OTlookupitem.label, SlaveSolarStatus);
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData(F("solar_storage_slave_fault_indicator"),  ((SlaveSolarFaultIndicator) ? "ON" : "OFF"));   
      sendMQTTData(F("solar_storage_mode_status"), itoa(SlaveSolarModeStatus, _msg, 10));  
      sendMQTTData(F("solar_storage_slave_status"), itoa(SlaveSolarStatus, _msg, 10));  
      OTcurrentSystemState.SolarSlaveStatus = OTdata.valueLB;
    }
  }
  if (is_value_valid(OTdata, OTlookupitem)){
    //OTGWDebugTf(PSTR("Solar Storage Master / Slave Mode u16 [%04x] _value [%04x] hb [%02x] lb [%02x]"), OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
    value = (OTcurrentSystemState.SolarMasterStatus<<8) | OTcurrentSystemState.SolarSlaveStatus;
  }
}

void print_statusVH(uint16_t& value)
{ 
  char _flag8_master[9] {0};
  char _flag8_slave[9] {0};

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

    
    AddLogf("%s = VH Master [%s]", OTlookupitem.label, _flag8_master);
    //Master Status
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData(F("status_vh_master"), _flag8_master);
      publishMQTTOnOff(F("vh_ventilation_enabled"),   ((OTdata.valueHB) & 0x01));
      publishMQTTOnOff(F("vh_bypass_position"),       ((OTdata.valueHB) & 0x02));
      publishMQTTOnOff(F("vh_bypass_mode"),           ((OTdata.valueHB) & 0x04));
      publishMQTTOnOff(F("vh_free_ventlation_mode"),  ((OTdata.valueHB) & 0x08));

      OTcurrentSystemState.MasterStatusVH = OTdata.valueHB;
    }
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

    
    AddLogf("%s = VH Slave  [%s]", OTlookupitem.label, _flag8_slave);

    //Slave Status
    if (is_value_valid(OTdata, OTlookupitem)){
      sendMQTTData(F("status_vh_slave"), _flag8_slave);
      publishMQTTOnOff(F("vh_fault"),                   ((OTdata.valueLB) & 0x01));
      publishMQTTOnOff(F("vh_ventlation_mode"),         ((OTdata.valueLB) & 0x02));
      publishMQTTOnOff(F("vh_bypass_status"),           ((OTdata.valueLB) & 0x04));
      publishMQTTOnOff(F("vh_bypass_automatic_status"), ((OTdata.valueLB) & 0x08));
      publishMQTTOnOff(F("vh_free_ventliation_status"), ((OTdata.valueLB) & 0x10));
      publishMQTTOnOff(F("vh_diagnostic_indicator"),    ((OTdata.valueLB) & 0x40));

      OTcurrentSystemState.SlaveStatusVH = OTdata.valueLB;
    }
  }

  if (is_value_valid(OTdata, OTlookupitem)){
    //OTGWDebugTf(PSTR("Status u16 [%04x] _value [%04x] hb [%02x] lb [%02x]"), OTdata.u16(), _value, OTdata.valueHB, OTdata.valueLB);
    value = (OTcurrentSystemState.MasterStatusVH<<8) | OTcurrentSystemState.SlaveStatusVH;
  }
}


void print_ASFflags(uint16_t& value)
{
  AddLogf("%s = ASF flags[%s] OEM faultcode [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _msg[15] {0};
    //Application Specific Fault
    sendMQTTData(F("ASF_flags"), byte_to_binary(OTdata.valueHB));
    //OEM fault code
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("OEMFaultCode"), _msg);

    //bit: [clear/0, set/1]
    //0: Service request [service not req’d, service required]
    //1: Lockout-reset [ remote reset disabled, rr enabled]
    //2: Low water press [ no WP fault, water pressure fault]
    //3: Gas/flame fault [ no G/F fault, gas/flame fault ]
    //4: Air press fault [ no AP fault, air pressure fault ]
    //5: Water over-temp[ no OvT fault, over-temperat. Fault]
    //6: reserved
    //7: reserved
    publishMQTTOnOff(F("service_request"),       ((OTdata.valueHB) & 0x01));
    publishMQTTOnOff(F("lockout_reset"),         ((OTdata.valueHB) & 0x02));
    publishMQTTOnOff(F("low_water_pressure"),    ((OTdata.valueHB) & 0x04));
    publishMQTTOnOff(F("gas_flame_fault"),       ((OTdata.valueHB) & 0x08));
    publishMQTTOnOff(F("air_pressure_fault"),    ((OTdata.valueHB) & 0x10));
    publishMQTTOnOff(F("water_over_temperature"), ((OTdata.valueHB) & 0x20));
    value = OTdata.u16();
  }
}

void print_RBPflags(uint16_t& value)
{
  AddLogf("%s = M[%s] OEM fault code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    //Remote Boiler Paramaters
    sendMQTTData(F("RBP_flags_transfer_enable"), byte_to_binary(OTdata.valueHB));  
    sendMQTTData(F("RBP_flags_read_write"), byte_to_binary(OTdata.valueLB));  

    //bit: [clear/0, set/1]
    //0: DHW setpoint
    //1: max CH setpoint
    //2: reserved
    //3: reserved
    //4: reserved
    //5: reserved
    //6: reserved
    //7: reserved
    sendMQTTData(F("rbp_dhw_setpoint"),       (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));    
    sendMQTTData(F("rbp_max_ch_setpoint"),    (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));    

    //bit: [clear/0, set/1]
    //0: read write  DHW setpoint
    //1: read write max CH setpoint
    //2: reserved
    //3: reserved
    //4: reserved
    //5: reserved
    //6: reserved
    //7: reserved
    sendMQTTData(F("rbp_rw_dhw_setpoint"),       (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));    
    sendMQTTData(F("rbp_rw_max_ch_setpoint"),    (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));    

    value = OTdata.u16();
  }
}

void print_slavememberid(uint16_t& value)
{
  AddLogf("%s = Slave Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
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

    sendMQTTData(F("dhw_present"),                             (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
    sendMQTTData(F("control_type_modulation"),                 (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  
    sendMQTTData(F("cooling_config"),                          (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  
    sendMQTTData(F("dhw_config"),                              (((OTdata.valueHB) & 0x08) ? "ON" : "OFF"));  
    sendMQTTData(F("master_low_off_pump_control_function"),    (((OTdata.valueHB) & 0x10) ? "ON" : "OFF"));   
    sendMQTTData(F("ch2_present"),                             (((OTdata.valueHB) & 0x20) ? "ON" : "OFF"));  
    sendMQTTData(F("remote_water_filling_function"),           (((OTdata.valueHB) & 0x40) ? "ON" : "OFF"));    
    sendMQTTData(F("heat_cool_mode_control"),                  (((OTdata.valueHB) & 0x80) ? "ON" : "OFF"));  
    value = OTdata.u16();
  }
}

void print_mastermemberid(uint16_t& value)
{
  AddLogf("%s = Master Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _msg[15] {0};
    sendMQTTData(F("master_configuration"), byte_to_binary(OTdata.valueHB));
    sendMQTTData(F("master_configuration_smart_power"), (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
    
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("master_memberid_code"), _msg);
    value = OTdata.u16();
  }
}

void print_vh_configmemberid(uint16_t& value)
{
  AddLogf("%s = VH Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _msg[15] {0};
    sendMQTTData(F("vh_configuration"), byte_to_binary(OTdata.valueHB)); 
    sendMQTTData(F("vh_configuration_system_type"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
    sendMQTTData(F("vh_configuration_bypass"),         (((OTdata.valueHB) & 0x02) ? "ON" : "OFF"));  
    sendMQTTData(F("vh_configuration_speed_control"),  (((OTdata.valueHB) & 0x04) ? "ON" : "OFF"));  
    
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("vh_memberid_code"), _msg);
    value = OTdata.u16();
  }
}

void print_solarstorage_slavememberid(uint16_t& value)
{
  AddLogf("%s = Solar Storage Slave Config[%s] MemberID code [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
     //Build string for SendMQTT
    sendMQTTData(F("solar_storage_slave_configuration"), byte_to_binary(OTdata.valueHB));
    char _msg[15] {0};
    utoa(OTdata.valueLB, _msg, 10);
    sendMQTTData(F("solar_storage_slave_memberid_code"), _msg);

    //ID103:HB0: Slave Configuration Solar Storage: System type1
    sendMQTTData(F("solar_storage_system_type"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
    value = OTdata.u16();
  }
}

void print_remoteoverridefunction(uint16_t& value)
{
// MsdID 100 Remote override room setpoint 
// LB: Remote override function 
// bit: description  [ clear/0, set/1] 
// 0:  Manual change priority [disable overruling remote 
//     setpoint by manual setpoint change, enable overruling 
//     remote setpoint by manual setpoint change ] 
// 1:  Program change priority [disable overruling remote 
//     setpoint by program setpoint change, enable overruling 
//     remote setpoint by program setpoint change ] 
// 2:  reserved  
// 3:  reserved 
// 4:  reserved 
// 5:  reserved 
// 6:  reserved 
// 7:  reserved 
// HB: reserved 
  
  AddLogf("%s = flag8 = [%s] - decimal = [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _topic[50] {0};
    //flag8 value
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_flag8", sizeof(_topic));
    sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
    //report remote override flags to MQTT
    sendMQTTData(F("remote_override_manual_change_priority"),             (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  
    sendMQTTData(F("remote_override_program_change_priority"),            (((OTdata.valueLB) & 0x02) ? "ON" : "OFF"));  
    value = OTdata.u16();
  }
}

void print_flag8u8(uint16_t& value)
{
  AddLogf("%s = M[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueLB);

  if (is_value_valid(OTdata, OTlookupitem)){
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
    value = OTdata.u16(); 
  }
}

void print_flag8(uint16_t& value)
{
  
  AddLogf("%s = flag8 = [%s] - decimal = [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);

   if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _topic[50] {0};
    //flag8 value
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_flag8", sizeof(_topic));
    sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
    value = OTdata.u16();
  }
}


void print_flag8flag8(uint16_t& value)
{ 
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 valueHB
  
  AddLogf("%s = HB flag8[%s] -[%3d] ", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);

  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_hb_flag8", sizeof(_topic));
    sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
  }
  //flag8 valueLB
  AddLogf("%s = LB flag8[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_lb_flag8", sizeof(_topic));
    sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
    value = OTdata.u16();
  }
}

void print_vh_remoteparametersetting(uint16_t& value)
{ 
  //Build string for MQTT
  char _topic[50] {0};
  //flag8 valueHB
  
  AddLogf("%s = HB flag8[%s] -[%3d] ", OTlookupitem.label, byte_to_binary(OTdata.valueHB), OTdata.valueHB);
  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_hb_flag8", sizeof(_topic));
    sendMQTTData(_topic, byte_to_binary(OTdata.valueHB));
    sendMQTTData(F("vh_tramfer_enble_nominal_ventlation_value"),    (((OTdata.valueHB) & 0x01) ? "ON" : "OFF"));  
  }
  //flag8 valueLB
  AddLogf("%s = LB flag8[%s] - [%3d]", OTlookupitem.label, byte_to_binary(OTdata.valueLB), OTdata.valueLB);
  if (is_value_valid(OTdata, OTlookupitem)){
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_lb_flag8", sizeof(_topic));
    sendMQTTData(_topic, byte_to_binary(OTdata.valueLB));
    sendMQTTData(F("vh_rw_nominal_ventlation_value"),    (((OTdata.valueLB) & 0x01) ? "ON" : "OFF"));  
    value = OTdata.u16();
  }
}

void print_command(uint16_t& value)
{ 
  //Known Commands
  // ID4 (HB=1): Remote Request Boiler Lockout-reset
  // ID4 (HB=2): Remote Request Water filling
  // ID4 (HB=10): Remote Request Service request reset
  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _topic[50] {0};
    char _msg[10] {0};
    //flag8 valueHB
    utoa((OTdata.valueHB), _msg, 10);
    //AddLogf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_hb_u8", sizeof(_topic));
    sendMQTTData(_topic, _msg);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_remote_command", sizeof(_topic));
    switch (OTdata.valueHB) {
      case 1: sendMQTTData(_topic, "Remote Request Boiler Lockout-reset");  AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Boiler Lockout-reset"); break;
      case 2: sendMQTTData(_topic, "Remote Request Water filling"); AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Water filling"); break;
      case 10: sendMQTTData(_topic, "Remote Request Service request reset");  AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Remote Request Service request reset");break;
      default: sendMQTTData(_topic, "Unknown command"); AddLogf("\r\n%s = remote command [%s]", OTlookupitem.label, "Unknown command");break;
    } 

    //flag8 valueLB
    utoa((OTdata.valueLB), _msg, 10);
    //AddLogf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_lb_u8", sizeof(_topic));
    sendMQTTData(_topic, _msg);
    value = OTdata.u16();
  }
}

void print_u8u8(uint16_t& value)
{ 
  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);

  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _topic[50] {0};
    char _msg[10] {0};
    //flag8 valueHB
    utoa((OTdata.valueHB), _msg, 10);
    //AddLogf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_hb_u8", sizeof(_topic));
    sendMQTTData(_topic, _msg);
    //flag8 valueLB
    utoa((OTdata.valueLB), _msg, 10);
    //AddLogf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_lb_u8", sizeof(_topic));
    sendMQTTData(_topic, _msg);
    value = OTdata.u16();
  }
}

void print_date(uint16_t& value)
{ 
  
  AddLogf("%s = %3d / %3d %s", OTlookupitem.label, (uint8_t)OTdata.valueHB, (uint8_t)OTdata.valueLB, OTlookupitem.unit);
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _topic[50] {0};
    char _msg[10] {0};
    //flag8 valueHB
    utoa((OTdata.valueHB), _msg, 10);
    //AddLogf("%s = HB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueHB);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_month", sizeof(_topic));
    sendMQTTData(_topic, _msg);
    //flag8 valueLB
    utoa((OTdata.valueLB), _msg, 10);
    //AddLogf("%s = LB u8[%s] [%3d]", OTlookupitem.label, _msg, OTdata.valueLB);
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_day_of_month", sizeof(_topic));
    sendMQTTData(_topic, _msg);
    value = OTdata.u16();
  }
}

void print_daytime(uint16_t& value)
{
  //function to print data
  static const char str_unknown[] PROGMEM = "Unknown";
  static const char str_monday[] PROGMEM = "Monday";
  static const char str_tuesday[] PROGMEM = "Tuesday";
  static const char str_wednesday[] PROGMEM = "Wednesday";
  static const char str_thursday[] PROGMEM = "Thursday";
  static const char str_friday[] PROGMEM = "Friday";
  static const char str_saturday[] PROGMEM = "Saturday";
  static const char str_sunday[] PROGMEM = "Sunday";
  static const char* const dayOfWeekName[] PROGMEM = { str_unknown, str_monday, str_tuesday, str_wednesday, str_thursday, str_friday, str_saturday, str_sunday, str_unknown };
  
  uint8_t dayIdx = (OTdata.valueHB >> 5) & 0x7;
  char dayName[15];
  strcpy_P(dayName, (PGM_P)pgm_read_ptr(&dayOfWeekName[dayIdx]));
  AddLogf("%s = %s - %.2d:%.2d", OTlookupitem.label, dayName, (OTdata.valueHB & 0x1F), OTdata.valueLB); 
  if (is_value_valid(OTdata, OTlookupitem)){
    //Build string for MQTT
    char _topic[50] {0};
    char _msg[10] {0};
    //dayofweek
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_dayofweek", sizeof(_topic));
    sendMQTTData(_topic, dayName); 
    //hour
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_hour", sizeof(_topic));
    sendMQTTData(_topic, itoa((OTdata.valueHB & 0x1F), _msg, 10)); 
    //min
    strlcpy(_topic, messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), sizeof(_topic));
    strlcat(_topic, "_minutes", sizeof(_topic));
    sendMQTTData(_topic, itoa((OTdata.valueLB), _msg, 10)); 
    value = OTdata.u16();
  }
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

//void addOTWGcmdtoqueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = OTGW_DELAY_SEND_MS);
void addOTWGcmdtoqueue(const char* buf, const int len, const bool forceQueue, const int16_t delay){
  constexpr int kMaxCmdLen = (int)(sizeof(cmdqueue[0].cmd) - 1);

  if (buf == nullptr) {
    OTGWDebugTln(F("CmdQueue: Error: null command"));
    return;
  }
  if ((len < 3) || (buf[2] != '=')){ 
    // Not a valid command
    OTGWDebugT(F("CmdQueue: Error: Not a valid command=["));
    for (int i = 0; i < len; i++) {
      OTGWDebug((char)buf[i]);
    }
    OTGWDebugf(PSTR("] (%d)\r\n"), len);
    return;
  }
  if (len > kMaxCmdLen) {
    OTGWDebugTf(PSTR("CmdQueue: Error: command too long (%d > %d)\r\n"), len, kMaxCmdLen);
    OTGWDebugFlush();
    return;
  }

  //check to see if the cmd is in queue
  bool foundcmd = false;
  int8_t insertptr = cmdptr; //set insertptr to next empty slot
  if (!forceQueue){
    char cmd[3];
    memset(cmd, 0, sizeof(cmd));
    memcpy(cmd, buf, 2);
    for (int i=0; i<cmdptr; i++){
      if (strncmp(cmdqueue[i].cmd, cmd, 2) == 0) {
        //found cmd exists, set the inertptr to found slot
        foundcmd = true;
        insertptr = i;
        break;
      }
    } 
  } 
  if (foundcmd) OTGWDebugTf(PSTR("CmdQueue: Found cmd exists in slot [%d]\r\n"), insertptr);
  else OTGWDebugTf(PSTR("CmdQueue: Adding cmd end of queue, slot [%d]\r\n"), insertptr);

  if (!foundcmd && cmdptr >= CMDQUEUE_MAX) {
    OTGWDebugTln(F("CmdQueue: Error: Reached max queue"));
    OTGWDebugFlush();
    return;
  }
  if (insertptr < 0 || insertptr >= CMDQUEUE_MAX) {
    OTGWDebugTf(PSTR("CmdQueue: Error: Invalid insert slot [%d]\r\n"), insertptr);
    OTGWDebugFlush();
    return;
  }

  //insert to the queue
  OTGWDebugTf(PSTR("CmdQueue: Insert queue in slot[%d]:"), insertptr);
  OTGWDebug(F("cmd["));
  for (int i = 0; i < len; i++) {
    OTGWDebug((char)buf[i]);
  }
  OTGWDebugf(PSTR("] (%d)\r\n"), len); 

  //copy the command into the queue
  int cmdlen = min((int)len , (int)(sizeof(cmdqueue[insertptr].cmd)-1));
  memset(cmdqueue[insertptr].cmd, 0, cmdlen+1);
  memcpy(cmdqueue[insertptr].cmd, buf, cmdlen);
  cmdqueue[insertptr].cmdlen = cmdlen;
  cmdqueue[insertptr].retrycnt = 0;
  const uint32_t safeDelay = (delay <= 0) ? 0u : (uint32_t)delay;
  cmdqueue[insertptr].due = millis() + safeDelay;

  //if not found
  if (!foundcmd) {
    //if not reached max of queue
    if (cmdptr < CMDQUEUE_MAX) {
      cmdptr++; //next free slot
      OTGWDebugTf(PSTR("CmdQueue: Next free queue slot: [%d]\r\n"), cmdptr);
    } else {
      // Should be prevented above; keep as defensive fallback.
      OTGWDebugTln(F("CmdQueue: Error: Reached max queue"));
    }
  } else OTGWDebugTf(PSTR("CmdQueue: Found command at: [%d] - [%d]\r\n"), insertptr, cmdptr);
  OTGWDebugFlush();
}

/*
  handleOTGWqueue should be called every second from main loop. 
  This checks the queue for message are due to be resent.
  If retry max is reached the cmd is delete from the queue
*/
void handleOTGWqueue(){
  // OTGWDebugTf(PSTR("CmdQueue: Commands in queue [%d]\r\n"), (int)cmdptr);
  const uint32_t now = millis();
  for (int i = 0; i < cmdptr; i++) {
    // OTGWDebugTf(PSTR("CmdQueue: Checking due in queue slot[%d]:[%lu]=>[%lu]\r\n"), (int)i, (unsigned long)millis(), (unsigned long)cmdqueue[i].due);
    if ((int32_t)(now - cmdqueue[i].due) >= 0) {
      OTGWDebugTf(PSTR("CmdQueue: Queue slot [%d] due\r\n"), i);
      sendOTGW(cmdqueue[i].cmd, cmdqueue[i].cmdlen);
      cmdqueue[i].retrycnt++;
      cmdqueue[i].due = now + OTGW_CMD_INTERVAL_MS;
      if (cmdqueue[i].retrycnt >= OTGW_CMD_RETRY){
        //max retry reached, so delete command from queue
        OTGWDebugTf(PSTR("CmdQueue: Delete [%d] from queue\r\n"), i);
        for (int j = i; j < (cmdptr - 1); j++){
          // OTGWDebugTf(PSTR("CmdQueue: Moving [%d] => [%d]\r\n"), j+1, j);
          strlcpy(cmdqueue[j].cmd, cmdqueue[j+1].cmd, sizeof(cmdqueue[j].cmd));
          cmdqueue[j].cmdlen = cmdqueue[j+1].cmdlen;
          cmdqueue[j].retrycnt = cmdqueue[j+1].retrycnt;
          cmdqueue[j].due = cmdqueue[j+1].due;
        }
        cmdptr--;
        cmdqueue[cmdptr].cmd[0] = '\0';
        cmdqueue[cmdptr].cmdlen = 0;
        cmdqueue[cmdptr].retrycnt = 0;
        cmdqueue[cmdptr].due = 0;
        i--; // re-check current index after shift
      }
      // //exit queue handling, after 1 command
      // return;
    }
  }
  OTGWDebugFlush();
}

/*
  checkOTGWcmdqueue (buf, len)
  This takes response from otgw and checks to see if the command was accepted.
  Checks the response, and finds the command (if it's there).
  Then checks if incoming response matches what was to be set.
  Only then it's deleted from the queue.
*/
void checkOTGWcmdqueue(const char *buf, unsigned int len){
  if ((len<3) || (buf[2]!=':')) {
    OTGWDebugT(F("CmdQueue: Error: Not a command response ["));
    for (unsigned int i = 0; i < len; i++) {
      OTGWDebug((char)buf[i]);
    }
    OTGWDebugf(PSTR("] (%d)\r\n"), len); 
    return; //not a valid command response
  }

  OTGWDebugT(F("CmdQueue: Checking if command is in in queue ["));
  for (unsigned int i = 0; i < len; i++) {
    OTGWDebug((char)buf[i]);
  }
  OTGWDebugf(PSTR("] (%d)\r\n"), len); 

  char cmd[3]; memset( cmd, 0, sizeof(cmd));
  char value[11]; memset( value, 0, sizeof(value));
  memcpy(cmd, buf, 2);
  memcpy(value, buf+3, ((len-3)<(sizeof(value)-1))?(len-3):(sizeof(value)-1));
  for (int i=0; i<cmdptr; i++){
      OTGWDebugTf(PSTR("CmdQueue: Checking [%2s]==>[%d]:[%s] from queue\r\n"), cmd, i, cmdqueue[i].cmd); 
    if (strncmp(cmdqueue[i].cmd, cmd, 2) == 0){
      //command found, check value
      OTGWDebugTf(PSTR("CmdQueue: Found cmd [%2s]==>[%d]:[%s]\r\n"), cmd, i, cmdqueue[i].cmd); 
      // if(strstr(cmdqueue[i].cmd, value)){
        //value found, thus remove command from queue
        OTGWDebugTf(PSTR("CmdQueue: Found value [%s]==>[%d]:[%s]\r\n"), value, i, cmdqueue[i].cmd); 
        OTGWDebugTf(PSTR("CmdQueue: Remove from queue [%d]:[%s] from queue\r\n"), i, cmdqueue[i].cmd);
        for (int j = i; j < (cmdptr - 1); j++){
          OTGWDebugTf(PSTR("CmdQueue: Moving [%d] => [%d]\r\n"), j+1, j);
          strlcpy(cmdqueue[j].cmd, cmdqueue[j+1].cmd, sizeof(cmdqueue[j].cmd));
          cmdqueue[j].cmdlen = cmdqueue[j+1].cmdlen;
          cmdqueue[j].retrycnt = cmdqueue[j+1].retrycnt;
          cmdqueue[j].due = cmdqueue[j+1].due;
        }
        cmdptr--;
        cmdqueue[cmdptr].cmd[0] = '\0';
        cmdqueue[cmdptr].cmdlen = 0;
        cmdqueue[cmdptr].retrycnt = 0;
        cmdqueue[cmdptr].due = 0;
        break;
      // } else OTGWDebugTf(PSTR("Error: Did not find value [%s]==>[%d]:[%s]\r\n"), value, i, cmdqueue[i].cmd); 
    }
  }
  OTGWDebugFlush();
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
    OTGWDebugT(F("Sending to Serial ["));
    for (int i = 0; i < len; i++) {
      OTGWDebug((char)buf[i]);
    }
    OTGWDebug(F("] (")); OTGWDebug(len); OTGWDebug(F(")")); OTGWDebugln();
        
    //write buffer to serial
    OTGWSerial.write(buf, len);         
    OTGWSerial.write('\r');
    OTGWSerial.write('\n');            
    OTGWSerial.flush(); 
  } else OTGWDebugln(F("Error: Write buffer not big enough!"));
}

/*
  This function checks if the string received is a valid "raw OT message".
  Raw OTmessages are 9 chars long and start with TBARE when talking to OTGW PIC.
  Message is not an OTmessage if length is not 9 long OR 3th char is ':' (= OTGW command response)
*/
bool isvalidotmsg(const char *buf, int len){
  bool _ret =  (len==9);    //check 9 chars long
  _ret &= (buf[2]!=':');    //not a otgw command response 
  char c = buf[0];
  _ret &= (c=='T' || c=='B' || c=='A' || c=='R' || c=='E'); //1 char matches any of 'B', 'T', 'A', 'R' or 'E'
  return _ret;
}

/*
  Process OTGW messages coming from the PIC.
  It knows about:
  - raw OTmsg format
  - error format
  - ...
*/
void processOT(const char *buf, int len){
  static time_t epochBoilerlastseen = 0;
  static time_t epochThermostatlastseen = 0;
  static time_t epochGatewaylastseen = 0;
  static bool bOTGWboilerpreviousstate = false;
  static bool bOTGWthermostatpreviousstate = false;
  static bool bOTGWgatewaypreviousstate = false;
  static bool bOTGWpreviousstate = false;
  time_t now = time(nullptr);

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
      epochBoilerlastseen = now; 
      OTdata.rsptype = OTGW_BOILER;
    } else if (buf[0]=='T'){
      epochThermostatlastseen = now;
      OTdata.rsptype = OTGW_THERMOSTAT;
    } else if (buf[0]=='R')    {
      epochGatewaylastseen = now;
      OTdata.rsptype = OTGW_REQUEST_BOILER;
    } else if (buf[0]=='A')    {
      epochGatewaylastseen = now;
      OTdata.rsptype = OTGW_ANSWER_THERMOSTAT;
    } else if (buf[0]=='E')    {
      OTdata.rsptype = OTGW_PARITY_ERROR;
    } 

    //If the Boiler messages have not been seen for 30 seconds, then set the state to false. 
    bOTGWboilerstate = (now < (epochBoilerlastseen+30));  
    if ((bOTGWboilerstate != bOTGWboilerpreviousstate) || (cntOTmessagesprocessed==1)) {
      sendMQTTData(F("otgw-pic/boiler_connected"), CCONOFF(bOTGWboilerstate)); 
      bOTGWboilerpreviousstate = bOTGWboilerstate;
    }

    //If the Thermostat messages have not been seen for 30 seconds, then set the state to false. 
    bOTGWthermostatstate = (now < (epochThermostatlastseen+30));
    if ((bOTGWthermostatstate != bOTGWthermostatpreviousstate) || (cntOTmessagesprocessed==1)){      
      sendMQTTData(F("otgw-pic/thermostat_connected"), CCONOFF(bOTGWthermostatstate));
      bOTGWthermostatpreviousstate = bOTGWthermostatstate;
    }
    
    // Gateway mode is now detected via PR=M command in doTaskEvery30s()
    // We still track gateway message activity (R/A messages) for online status detection
    // but don't use it to determine gateway mode anymore
    bool bOTGWgatewayactive = (now < (epochGatewaylastseen+30));

    //If both (Boiler and Thermostat and Gateway) are offline, then the OTGW is considered offline as a whole.
    bOTGWonline = (bOTGWboilerstate && bOTGWthermostatstate) || (bOTGWboilerstate && bOTGWgatewayactive);
    if ((bOTGWonline != bOTGWpreviousstate) || (cntOTmessagesprocessed==1)){
      sendMQTTData(F("otgw-pic/otgw_connected"), CCONOFF(bOTGWonline));
      sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(bOTGWonline));
      // nodeMCU online/offline zelf naar 'otgw-firmware/' pushen
      bOTGWpreviousstate = bOTGWonline; //remember state, so we can detect statechanges
    }

    //clear ot log buffer
    ClrLog();
    // Start log with timestamp
    AddLog(getOTLogTimestamp());
    AddLog(" ");
    
    //process the OTGW message
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
    OTdata.skipthis = false;                          // default: do not skip this message (will be sent to MQTT and not stored in state data object)
    
    if (cntOTmessagesprocessed == 1) {       //first message needs to be put in the buffer
      //just store current message and delay processing
      delayedOTdata = OTdata;       //store current msg
      OTGWDebugln(F("delaying first message!"));
    } else {                              //any other message will be processed
      //when the gateway overrides the boiler or thermostat, then do not use the results for decoding anywhere (skip this)
      //if B --> A, then gateway tells the thermostat what it needs to hear, then use current A message, and skip B value.
      //if T --> R, then gateway overrides the thermostat, and tells the boiler what to do, then use current R message, and skip T value.
      bool skipthis = (delayedOTdata.id == OTdata.id) && (OTdata.time - delayedOTdata.time < 500) &&  
           (((OTdata.rsptype == OTGW_ANSWER_THERMOSTAT) && (delayedOTdata.rsptype == OTGW_BOILER)) ||
            ((OTdata.rsptype == OTGW_REQUEST_BOILER) && (delayedOTdata.rsptype == OTGW_THERMOSTAT)));

      //delay message processing by 1 message, to make sure detection of value decoding is done correctly with R and A message.
      tmpOTdata = delayedOTdata;          //fetch delayed msg
      delayedOTdata = OTdata;             //store current msg
      OTdata = tmpOTdata;                 //then process delayed msg
      OTdata.skipthis = skipthis;         //skip if needed

      //when parity error in OTGW then skip data to MQTT nor store it local in data object
      OTdata.skipthis |= (OTdata.rsptype == OTGW_PARITY_ERROR);

      //keep track of last update time of each message id
      msglastupdated[OTdata.id] = now;
      
      //Read information from this OT message ready for use...
      if (OTdata.id <= OT_MSGID_MAX) {
        PROGMEM_readAnything (&OTmap[OTdata.id], OTlookupitem);
      } else {
        //unknown message id, set safe defaults to prevent OTmap OOB read
        OTlookupitem.id = OTdata.id;
        OTlookupitem.msgcmd = OT_UNDEF;
        OTlookupitem.type = ot_undef;
        OTlookupitem.label = "Unknown";
        OTlookupitem.friendlyname = "Unknown";
        OTlookupitem.unit = "";
      }

      // check wheter MQTT topic needs to be configuered
      if (is_value_valid(OTdata, OTlookupitem) && settingMQTTenable ) {
        if(getMQTTConfigDone(OTdata.id)==false) {
          MQTTDebugTf(PSTR("Need to set MQTT config for message %s (%d)\r\n"), OTlookupitem.label, OTdata.id);
          bool success = doAutoConfigureMsgid(OTdata.id, NodeId);
          if(success) {
            MQTTDebugTf(PSTR("Successfully sent MQTT config for message %s (%d)\r\n"), OTlookupitem.label, OTdata.id);
            setMQTTConfigDone(OTdata.id);
          } else {
            MQTTDebugTf(PSTR("Not able to complete MQTT configuration for message %s (%d)\r\n"), OTlookupitem.label, OTdata.id);
          }
        } else {
          // MQTTDebugTf(PSTR("No need to set MQTT config for message %s (%d)\r\n"), OTlookupitem.label, OTdata.id);
        }
      }


      // Decode and print OpenTherm Gateway Message
      switch (OTdata.rsptype){
        case OTGW_BOILER:
          AddLog("Boiler            ");
          break;
        case OTGW_THERMOSTAT:
          AddLog("Thermostat        ");
          break;
        case OTGW_REQUEST_BOILER:
          AddLog("Request Boiler    ");
          break;
        case OTGW_ANSWER_THERMOSTAT:
          AddLog("Answer Thermostat ");
          break;
        case OTGW_PARITY_ERROR:
          AddLog("Parity Error      ");
          break;
        default:
          AddLog("Unknown           ");
          break;
      }

      //print message Type and ID
      AddLogf(" %s %3d", OTdata.buf, OTdata.id);
      AddLogf(" %-16s", messageTypeToString(static_cast<OpenThermMessageType>(OTdata.type)));
      //OTGWDebugf("[%-30s]", messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)));
      //OTGWDebugf("[M=%d]",OTdata.master);

      //Add indicators for parity error, skip message or valid value
      if (OTdata.rsptype == OTGW_PARITY_ERROR) AddLog("P"); 
      else if (OTdata.skipthis) AddLog("-"); 
      else if (is_value_valid(OTdata, OTlookupitem)) AddLog(">");
      else AddLog(" ");  //placeholder for alignment
      
      AddLog(" ");  // Space before payload for readability
      
      //next step interpret the OT protocol
          
      //#define OTprint(data, value, text, format) ({ data= value; OTGWDebugf("[%37s]", text); OTGWDebugf("= [format]", data)})
        //interpret values f8.8

      switch (static_cast<OpenThermMessageID>(OTdata.id)) {   
        case OT_Statusflags:                            print_status(OTcurrentSystemState.Statusflags); break;
        case OT_TSet:                                   print_f88(OTcurrentSystemState.TSet); break;         
        case OT_CoolingControl:                         print_f88(OTcurrentSystemState.CoolingControl); break;
        case OT_TsetCH2:                                print_f88(OTcurrentSystemState.TsetCH2); break;
        case OT_TrOverride:                             print_f88(OTcurrentSystemState.TrOverride); break;        
        case OT_MaxRelModLevelSetting:                  print_f88(OTcurrentSystemState.MaxRelModLevelSetting); break;
        case OT_TrSet:                                  print_f88(OTcurrentSystemState.TrSet); break;
        case OT_TrSetCH2:                               print_f88(OTcurrentSystemState.TrSetCH2); break;
        case OT_RelModLevel:                            print_f88(OTcurrentSystemState.RelModLevel); break;
        case OT_CHPressure:                             print_f88(OTcurrentSystemState.CHPressure); break;
        case OT_DHWFlowRate:                            print_f88(OTcurrentSystemState.DHWFlowRate); break;
        case OT_Tr:                                     print_f88(OTcurrentSystemState.Tr); break;  
        case OT_Tboiler:                                print_f88(OTcurrentSystemState.Tboiler);break;
        case OT_Tdhw:                                   print_f88(OTcurrentSystemState.Tdhw); break;
        case OT_Toutside:                               print_f88(OTcurrentSystemState.Toutside); break;
        case OT_Tret:                                   print_f88(OTcurrentSystemState.Tret); break;
        case OT_Tsolarstorage:                          print_f88(OTcurrentSystemState.Tsolarstorage); break;
        case OT_Tsolarcollector:                        print_s16(OTcurrentSystemState.Tsolarcollector); break;
        case OT_TflowCH2:                               print_f88(OTcurrentSystemState.TflowCH2); break;          
        case OT_Tdhw2:                                  print_f88(OTcurrentSystemState.Tdhw2 ); break;
        case OT_Texhaust:                               print_s16(OTcurrentSystemState.Texhaust); break; 
        case OT_Theatexchanger:                         print_f88(OTcurrentSystemState.Theatexchanger); break;
        case OT_TdhwSet:                                print_f88(OTcurrentSystemState.TdhwSet); break;
        case OT_MaxTSet:                                print_f88(OTcurrentSystemState.MaxTSet); break;
        case OT_Hcratio:                                print_f88(OTcurrentSystemState.Hcratio); break;
        case OT_Remoteparameter4:                       print_f88(OTcurrentSystemState.Remoteparameter4); break;
        case OT_Remoteparameter5:                       print_f88(OTcurrentSystemState.Remoteparameter5); break;
        case OT_Remoteparameter6:                       print_f88(OTcurrentSystemState.Remoteparameter6); break;
        case OT_Remoteparameter7:                       print_f88(OTcurrentSystemState.Remoteparameter7); break;
        case OT_Remoteparameter8:                       print_f88(OTcurrentSystemState.Remoteparameter8); break;
        case OT_OpenThermVersionMaster:                 print_f88(OTcurrentSystemState.OpenThermVersionMaster); break;
        case OT_OpenThermVersionSlave:                  print_f88(OTcurrentSystemState.OpenThermVersionSlave); break;
        case OT_ASFflags:                               print_ASFflags(OTcurrentSystemState.ASFflags); break;
        case OT_MasterConfigMemberIDcode:               print_mastermemberid(OTcurrentSystemState.MasterConfigMemberIDcode); break; 
        case OT_SlaveConfigMemberIDcode:                print_slavememberid(OTcurrentSystemState.SlaveConfigMemberIDcode); break;   
        case OT_Command:                                print_command(OTcurrentSystemState.Command );  break; 
        case OT_RBPflags:                               print_RBPflags(OTcurrentSystemState.RBPflags); break; 
        case OT_TSP:                                    print_u8u8(OTcurrentSystemState.TSP); break; 
        case OT_TSPindexTSPvalue:                       print_u8u8(OTcurrentSystemState.TSPindexTSPvalue); break; 
        case OT_FHBsize:                                print_u8u8(OTcurrentSystemState.FHBsize); break;  
        case OT_FHBindexFHBvalue:                       print_u8u8(OTcurrentSystemState.FHBindexFHBvalue); break; 
        case OT_MaxCapacityMinModLevel:                 print_u8u8(OTcurrentSystemState.MaxCapacityMinModLevel); break; 
        case OT_DayTime:                                print_daytime(OTcurrentSystemState.DayTime); break; 
        case OT_Date:                                   print_date(OTcurrentSystemState.Date); break; 
        case OT_Year:                                   print_u16(OTcurrentSystemState.Year); break; 
        case OT_TdhwSetUBTdhwSetLB:                     print_s8s8(OTcurrentSystemState.TdhwSetUBTdhwSetLB ); break;  
        case OT_MaxTSetUBMaxTSetLB:                     print_s8s8(OTcurrentSystemState.MaxTSetUBMaxTSetLB); break;  
        case OT_HcratioUBHcratioLB:                     print_s8s8(OTcurrentSystemState.HcratioUBHcratioLB); break; 
        case OT_Remoteparameter4boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter4boundaries); break;
        case OT_Remoteparameter5boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter5boundaries); break;
        case OT_Remoteparameter6boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter6boundaries); break;
        case OT_Remoteparameter7boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter7boundaries); break;
        case OT_Remoteparameter8boundaries:             print_s8s8(OTcurrentSystemState.Remoteparameter8boundaries); break;
        case OT_RemoteOverrideFunction:                 print_remoteoverridefunction(OTcurrentSystemState.RemoteOverrideFunction); break;
        case OT_OEMDiagnosticCode:                      print_u16(OTcurrentSystemState.OEMDiagnosticCode); break;
        case OT_BurnerStarts:                           print_u16(OTcurrentSystemState.BurnerStarts); break; 
        case OT_CHPumpStarts:                           print_u16(OTcurrentSystemState.CHPumpStarts); break; 
        case OT_DHWPumpValveStarts:                     print_u16(OTcurrentSystemState.DHWPumpValveStarts); break; 
        case OT_DHWBurnerStarts:                        print_u16(OTcurrentSystemState.DHWBurnerStarts); break;
        case OT_BurnerOperationHours:                   print_u16(OTcurrentSystemState.BurnerOperationHours); break;
        case OT_CHPumpOperationHours:                   print_u16(OTcurrentSystemState.CHPumpOperationHours); break; 
        case OT_DHWPumpValveOperationHours:             print_u16(OTcurrentSystemState.DHWPumpValveOperationHours); break;  
        case OT_DHWBurnerOperationHours:                print_u16(OTcurrentSystemState.DHWBurnerOperationHours); break; 
        case OT_MasterVersion:                          print_u8u8(OTcurrentSystemState.MasterVersion ); break; 
        case OT_SlaveVersion:                           print_u8u8(OTcurrentSystemState.SlaveVersion); break;
        case OT_StatusVH:                               print_statusVH(OTcurrentSystemState.StatusVH); break;
        case OT_ControlSetpointVH:                      print_u8u8(OTcurrentSystemState.ControlSetpointVH); break;
        case OT_ASFFaultCodeVH:                         print_flag8u8(OTcurrentSystemState.ASFFaultCodeVH); break;
        case OT_DiagnosticCodeVH:                       print_u16(OTcurrentSystemState.DiagnosticCodeVH); break;
        case OT_ConfigMemberIDVH:                       print_vh_configmemberid(OTcurrentSystemState.ConfigMemberIDVH); break;
        case OT_OpenthermVersionVH:                     print_f88(OTcurrentSystemState.OpenthermVersionVH); break;
        case OT_VersionTypeVH:                          print_u8u8(OTcurrentSystemState.VersionTypeVH ); break;
        case OT_RelativeVentilation:                    print_u8u8(OTcurrentSystemState.RelativeVentilation); break;
        case OT_RelativeHumidityExhaustAir:             print_u8u8(OTcurrentSystemState.RelativeHumidityExhaustAir); break;
        case OT_CO2LevelExhaustAir:                     print_u16(OTcurrentSystemState.CO2LevelExhaustAir); break;
        case OT_SupplyInletTemperature:                 print_f88(OTcurrentSystemState.SupplyInletTemperature); break;
        case OT_SupplyOutletTemperature:                print_f88(OTcurrentSystemState.SupplyOutletTemperature); break;
        case OT_ExhaustInletTemperature:                print_f88(OTcurrentSystemState.ExhaustInletTemperature); break;
        case OT_ExhaustOutletTemperature:               print_f88(OTcurrentSystemState.ExhaustOutletTemperature); break;
        case OT_ActualExhaustFanSpeed:                  print_u16(OTcurrentSystemState.ActualExhaustFanSpeed); break;
        case OT_ActualSupplyFanSpeed:                   print_u16(OTcurrentSystemState.ActualSupplyFanSpeed); break;
        case OT_RemoteParameterSettingVH:               print_vh_remoteparametersetting(OTcurrentSystemState.RemoteParameterSettingVH); break;
        case OT_NominalVentilationValue:                print_u8u8(OTcurrentSystemState.NominalVentilationValue); break;
        case OT_TSPNumberVH:                            print_u8u8(OTcurrentSystemState.TSPNumberVH); break;
        case OT_TSPEntryVH:                             print_u8u8(OTcurrentSystemState.TSPEntryVH); break;
        case OT_FaultBufferSizeVH:                      print_u8u8(OTcurrentSystemState.FaultBufferSizeVH); break;
        case OT_FaultBufferEntryVH:                     print_u8u8(OTcurrentSystemState.FaultBufferEntryVH); break;
        case OT_FanSpeed:                               print_u8u8(OTcurrentSystemState.FanSpeed); break;
        case OT_ElectricalCurrentBurnerFlame:           print_f88(OTcurrentSystemState.ElectricalCurrentBurnerFlame); break;
        case OT_TRoomCH2:                               print_f88(OTcurrentSystemState.TRoomCH2); break;
        case OT_RelativeHumidity:                       print_u8u8(OTcurrentSystemState.RelativeHumidity); break;
        case OT_TrOverride2:                            print_f88(OTcurrentSystemState.TrOverride2); break;
        case OT_Brand:                                  print_u8u8(OTcurrentSystemState.Brand); break;
        case OT_BrandVersion:                           print_u8u8(OTcurrentSystemState.BrandVersion); break;
        case OT_BrandSerialNumber:                      print_u8u8(OTcurrentSystemState.BrandSerialNumber); break;
        case OT_CoolingOperationHours:                  print_u16(OTcurrentSystemState.CoolingOperationHours); break;
        case OT_PowerCycles:                            print_u16(OTcurrentSystemState.PowerCycles); break;
        case OT_RFstrengthbatterylevel:                 print_u8u8(OTcurrentSystemState.RFstrengthbatterylevel); break;
        case OT_OperatingMode_HC1_HC2_DHW:              print_u8u8(OTcurrentSystemState.OperatingMode_HC1_HC2_DHW ); break; 
        case OT_ElectricityProducerStarts:              print_u16(OTcurrentSystemState.ElectricityProducerStarts); break;
        case OT_ElectricityProducerHours:               print_u16(OTcurrentSystemState.ElectricityProducerHours); break;
        case OT_ElectricityProduction:                  print_u16(OTcurrentSystemState.ElectricityProduction); break;
        case OT_CumulativElectricityProduction:         print_u16(OTcurrentSystemState.CumulativElectricityProduction); break;
        case OT_RemehadFdUcodes:                        print_u8u8(OTcurrentSystemState.RemehadFdUcodes); break;
        case OT_RemehaServicemessage:                   print_u8u8(OTcurrentSystemState.RemehaServicemessage); break;
        case OT_RemehaDetectionConnectedSCU:            print_u8u8(OTcurrentSystemState.RemehaDetectionConnectedSCU); break;
        case OT_SolarStorageMaster:                     print_solar_storage_status(OTcurrentSystemState.SolarStorageStatus ); break;
        case OT_SolarStorageASFflags:                   print_flag8u8(OTcurrentSystemState.SolarStorageASFflags); break;
        case OT_SolarStorageSlaveConfigMemberIDcode:    print_solarstorage_slavememberid(OTcurrentSystemState.SolarStorageSlaveConfigMemberIDcode); break;
        case OT_SolarStorageVersionType:                print_u8u8(OTcurrentSystemState.SolarStorageVersionType); break;
        case OT_SolarStorageTSP:                        print_u8u8(OTcurrentSystemState.SolarStorageTSP ); break;
        case OT_SolarStorageTSPindexTSPvalue:           print_u8u8(OTcurrentSystemState.SolarStorageTSPindexTSPvalue ); break;
        case OT_SolarStorageFHBsize:                    print_u8u8(OTcurrentSystemState.SolarStorageFHBsize ); break;
        case OT_SolarStorageFHBindexFHBvalue:           print_u8u8(OTcurrentSystemState.SolarStorageFHBindexFHBvalue ); break;
        case OT_BurnerUnsuccessfulStarts:               print_u16(OTcurrentSystemState.BurnerUnsuccessfulStarts); break;
        case OT_FlameSignalTooLow:                      print_u16(OTcurrentSystemState.FlameSignalTooLow); break;
        default: 
            AddLogf("Unknown message [%02d] value [%04X] f8.8 [%3.2f] u16 [%d] s16 [%d]", OTdata.id, OTdata.value,  OTdata.f88(), OTdata.u16(), OTdata.s16());
            break;
      }
      if (OTdata.skipthis) AddLog(" <ignored> ");
      AddLogln();
      OTGWDebugT(skipOTLogTimestamp(ot_log_buffer));
   
      // Send log buffer directly to WebSocket (no JSON, no queue)
      sendLogToWebSocket(ot_log_buffer);
      
      OTGWDebugFlush();
      ClrLog();
    } 
  } else if (buf[2]==':') { //seems to be a response to a command, so check to verify if it was
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);
    sendMQTTData(F("event_report"), buf);
  } else if (strcmp_P(buf, PSTR("NG")) == 0) {
    Debugln(F("NG - No Good. The command code is unknown."));
    sendMQTTData(F("event_report"), F("NG - No Good. The command code is unknown."));
  } else if (strcmp_P(buf, PSTR("SE")) == 0) {
    Debugln(F("SE - Syntax Error. The command contained an unexpected character or was incomplete."));
    sendMQTTData(F("event_report"), F("SE - Syntax Error. The command contained an unexpected character or was incomplete."));
  } else if (strcmp_P(buf, PSTR("BV")) == 0) {
    Debugln(F("BV - Bad Value. The command contained a data value that is not allowed."));
    sendMQTTData(F("event_report"), F("BV - Bad Value. The command contained a data value that is not allowed."));
  } else if (strcmp_P(buf, PSTR("OR")) == 0) {
    Debugln(F("OR - Out of Range. A number was specified outside of the allowed range."));
    sendMQTTData(F("event_report"), F("OR - Out of Range. A number was specified outside of the allowed range."));
  } else if (strcmp_P(buf, PSTR("NS")) == 0) {
    Debugln(F("NS - No Space. The alternative Data-ID could not be added because the table is full."));
    sendMQTTData(F("event_report"), F("NS - No Space. The alternative Data-ID could not be added because the table is full."));
  } else if (strcmp_P(buf, PSTR("NF")) == 0) {
    Debugln(F("NF - Not Found. The specified alternative Data-ID could not be removed because it does not exist in the table."));
    sendMQTTData(F("event_report"), F("NF - Not Found. The specified alternative Data-ID could not be removed because it does not exist in the table."));
  } else if (strcmp_P(buf, PSTR("OE")) == 0) {
    Debugln(F("OE - Overrun Error. The processor was busy and failed to process all received characters."));
    sendMQTTData(F("event_report"), F("OE - Overrun Error. The processor was busy and failed to process all received characters."));
  } else if (strcmp_P(buf, PSTR("Thermostat disconnected")) == 0) {
    Debugln(F("Thermostat disconnected"));
    sendMQTTData(F("event_report"), F("Thermostat disconnected"));
  } else if (strcmp_P(buf, PSTR("Thermostat connected")) == 0) {
    Debugln(F("Thermostat connected"));
    sendMQTTData(F("event_report"), F("Thermostat connected"));
  } else if (strcmp_P(buf, PSTR("Low power")) == 0) {
    Debugln(F("Low power"));
    sendMQTTData(F("event_report"), F("Low power"));
  } else if (strcmp_P(buf, PSTR("Medium power")) == 0) {
    Debugln(F("Medium power"));
    sendMQTTData(F("event_report"), F("Medium power"));
  } else if (strcmp_P(buf, PSTR("High power")) == 0) {
    Debugln(F("High power"));
    sendMQTTData(F("event_report"), F("High power"));
  } else if (strstr_P(buf, PSTR("\r\nError 01"))!= NULL) {
    char errorBuf[12];
    OTcurrentSystemState.error01++;
    OTGWDebugTf(PSTR("\r\nError 01 = %d\r\n"),OTcurrentSystemState.error01);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error01);
    sendMQTTData(F("Error 01"), errorBuf);
  } else if (strstr_P(buf, PSTR("Error 02"))!= NULL) {
    char errorBuf[12];
    OTcurrentSystemState.error02++;
    OTGWDebugTf(PSTR("\r\nError 02 = %d\r\n"),OTcurrentSystemState.error02);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error02);
    sendMQTTData(F("Error 02"), errorBuf);
  } else if (strstr_P(buf, PSTR("Error 03"))!= NULL) {
    char errorBuf[12];
    OTcurrentSystemState.error03++;
    OTGWDebugTf(PSTR("\r\nError 03 = %d\r\n"),OTcurrentSystemState.error03);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error03);
    sendMQTTData(F("Error 03"), errorBuf);
  } else if (strstr_P(buf, PSTR("Error 04"))!= NULL){
    char errorBuf[12];
    OTcurrentSystemState.error04++;
    OTGWDebugTf(PSTR("\r\nError 04 = %d\r\n"),OTcurrentSystemState.error04);
    snprintf_P(errorBuf, sizeof(errorBuf), PSTR("%u"), OTcurrentSystemState.error04);
    sendMQTTData(F("Error 04"), errorBuf);
  } else if (strstr(buf, OTGW_BANNER)!=NULL){
    //found a banner, so get the version of PIC
    strlcpy(sPICfwversion, OTGWSerial.firmwareVersion(), sizeof(sPICfwversion));
    OTGWDebugTf(PSTR("Current firmware version: %s\r\n"), sPICfwversion);
    strlcpy(sPICdeviceid, OTGWSerial.processorToString().c_str(), sizeof(sPICdeviceid));
    OTGWDebugTf(PSTR("Current device id: %s\r\n"), sPICdeviceid);    
    strlcpy(sPICtype, OTGWSerial.firmwareToString().c_str(), sizeof(sPICtype));
    OTGWDebugTf(PSTR("Current firmware type: %s\r\n"), sPICtype);
  } else {
    OTGWDebugTf(PSTR("Not processed, received from OTGW => (%s) [%d]\r\n"), buf, len);
  }
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
** The read line buffer is per line parsed by the proces OT parser code (processOT (buf, len)).
*/
void handleOTGW()
{
  //handle serial communication and line processing
  #define MAX_BUFFER_READ 256       //need to be 256 because of long PS=1 responses 
  #define MAX_BUFFER_WRITE 128
  static char sRead[MAX_BUFFER_READ];
  static char sWrite[MAX_BUFFER_WRITE];
  static size_t bytes_read = 0;
  static size_t bytes_write = 0;
  static uint8_t outByte;

  //Handle incoming data from OTGW through serial port (READ BUFFER)
  if (OTGWSerial.hasOverrun()) {
    DebugT(F("Serial Overrun\r\n"));
  }
  if (OTGWSerial.hasRxError()){
    DebugT(F("Serial Rx Error\r\n"));
  }
  
  while (OTGWSerial.available()) {
    outByte = OTGWSerial.read();
    OTGWstream.write(outByte);
    if (outByte == '\r' || outByte == '\n') {
      if (bytes_read == 0) continue;
      blinkLEDnow(LED2);
      sRead[bytes_read] = '\0';
      processOT(sRead, bytes_read);
      bytes_read = 0;
    } else if (bytes_read < (MAX_BUFFER_READ-1)) {
      sRead[bytes_read++] = outByte;
    } else {
      // Buffer overflow detected - discard current buffer and log error
      OTcurrentSystemState.errorBufferOverflow++;
      DebugTf(PSTR("Serial Buffer Overflow! Discarding %d bytes. Total overflows: %d\r\n"), 
              bytes_read, OTcurrentSystemState.errorBufferOverflow);
      // Rate limit MQTT notifications - only send every 10 overflows to avoid overwhelming broker
      static uint8_t overflowsSinceLastReport = 0;
      overflowsSinceLastReport++;
      if (overflowsSinceLastReport >= 10) {
        char overflowCountBuf[7] = {0};
        utoa(OTcurrentSystemState.errorBufferOverflow, overflowCountBuf, 10);
        sendMQTTData(F("Error_BufferOverflow"), overflowCountBuf);
        overflowsSinceLastReport = 0;
      }
      // Reset buffer to prevent processing corrupted data
      bytes_read = 0;
      // Skip remaining bytes until we hit a line terminator to resync (max 256 bytes to prevent blocking)
      uint16_t skipCount = 0;
      while (OTGWSerial.available() && skipCount < MAX_BUFFER_READ) {
        outByte = OTGWSerial.read();
        OTGWstream.write(outByte);
        skipCount++;
        if (outByte == '\r' || outByte == '\n') {
          break;
        }
      }
    }
  }

  //handle incoming data from network (port 25238) sent to serial port OTGW (WRITE BUFFER)
  while (OTGWstream.available()){
    outByte = OTGWstream.read();  // read from port 25238
    OTGWSerial.write(outByte);    // write to serial port
    if (outByte == '\r')
    { //on CR, do something...
      sWrite[bytes_write] = 0;
      OTGWDebugTf(PSTR("Net2Ser: Sending to OTGW: [%s] (%d)\r\n"), sWrite, bytes_write);
      //check for reset command
      if (strcmp_P(sWrite, PSTR("GW=R"))==0){
        //detected [GW=R], then reset the gateway the gpio way
        OTGWDebugTln(F("Detected: GW=R. Reset gateway command executed."));
        resetOTGW();
      } else if (strcasecmp_P(sWrite, PSTR("PS=1"))==0) {
        //detected [PS=1], then PrintSummary mode = true --> From this point on you need to ask for summary.
        bPSmode = true;
        //reset all msglastupdated in webui
        for(int i = 0; i <= OT_MSGID_MAX; i++){
          msglastupdated[i] = 0; //clear epoch values
        }
        strlcpy(sMessage, "PS=1 mode; No UI updates.", sizeof(sMessage));
      } else if (strcasecmp_P(sWrite, PSTR("PS=0"))==0) {
        //detected [PS=0], then PrintSummary mode = OFF --> Raw mode is turned on again.
        bPSmode = false;
        sMessage[0] = '\0';
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
}// END of handleOTGW

//====================[ functions for REST API ]====================
const char* getOTGWValue(int msgid)
{
  static char buffer[32];
  
  switch (static_cast<OpenThermMessageID>(msgid)) { 
    case OT_TSet:                              dtostrf(OTcurrentSystemState.TSet, 0, 2, buffer); return buffer;
    case OT_CoolingControl:                    dtostrf(OTcurrentSystemState.CoolingControl, 0, 2, buffer); return buffer;
    case OT_TsetCH2:                           dtostrf(OTcurrentSystemState.TsetCH2, 0, 2, buffer); return buffer;
    case OT_TrOverride:                        dtostrf(OTcurrentSystemState.TrOverride, 0, 2, buffer); return buffer;
    case OT_TrOverride2:                       dtostrf(OTcurrentSystemState.TrOverride2, 0, 2, buffer); return buffer;
    case OT_MaxRelModLevelSetting:             dtostrf(OTcurrentSystemState.MaxRelModLevelSetting, 0, 2, buffer); return buffer;
    case OT_TrSet:                             dtostrf(OTcurrentSystemState.TrSet, 0, 2, buffer); return buffer;
    case OT_TrSetCH2:                          dtostrf(OTcurrentSystemState.TrSetCH2, 0, 2, buffer); return buffer;
    case OT_RelModLevel:                       dtostrf(OTcurrentSystemState.RelModLevel, 0, 2, buffer); return buffer;
    case OT_CHPressure:                        dtostrf(OTcurrentSystemState.CHPressure, 0, 2, buffer); return buffer;
    case OT_DHWFlowRate:                       dtostrf(OTcurrentSystemState.DHWFlowRate, 0, 2, buffer); return buffer;
    case OT_Tr:                                dtostrf(OTcurrentSystemState.Tr, 0, 2, buffer); return buffer;
    case OT_Tboiler:                           dtostrf(OTcurrentSystemState.Tboiler, 0, 2, buffer); return buffer;
    case OT_Tdhw:                              dtostrf(OTcurrentSystemState.Tdhw, 0, 2, buffer); return buffer;
    case OT_Toutside:                          dtostrf(OTcurrentSystemState.Toutside, 0, 2, buffer); return buffer;
    case OT_Tret:                              dtostrf(OTcurrentSystemState.Tret, 0, 2, buffer); return buffer;
    case OT_Tsolarstorage:                     dtostrf(OTcurrentSystemState.Tsolarstorage, 0, 2, buffer); return buffer;
    case OT_Tsolarcollector:                   dtostrf(OTcurrentSystemState.Tsolarcollector, 0, 2, buffer); return buffer;
    case OT_TflowCH2:                          dtostrf(OTcurrentSystemState.TflowCH2, 0, 2, buffer); return buffer;
    case OT_Tdhw2:                             dtostrf(OTcurrentSystemState.Tdhw2, 0, 2, buffer); return buffer;
    case OT_Texhaust:                          dtostrf(OTcurrentSystemState.Texhaust, 0, 2, buffer); return buffer;
    case OT_Theatexchanger:                    dtostrf(OTcurrentSystemState.Theatexchanger, 0, 2, buffer); return buffer;
    case OT_TdhwSet:                           dtostrf(OTcurrentSystemState.TdhwSet, 0, 2, buffer); return buffer;
    case OT_MaxTSet:                           dtostrf(OTcurrentSystemState.MaxTSet, 0, 2, buffer); return buffer;
    case OT_Hcratio:                           dtostrf(OTcurrentSystemState.Hcratio, 0, 2, buffer); return buffer;
    case OT_Remoteparameter4:                  dtostrf(OTcurrentSystemState.Remoteparameter4, 0, 2, buffer); return buffer;
    case OT_Remoteparameter5:                  dtostrf(OTcurrentSystemState.Remoteparameter5, 0, 2, buffer); return buffer;
    case OT_Remoteparameter6:                  dtostrf(OTcurrentSystemState.Remoteparameter6, 0, 2, buffer); return buffer;
    case OT_Remoteparameter7:                  dtostrf(OTcurrentSystemState.Remoteparameter7, 0, 2, buffer); return buffer;
    case OT_Remoteparameter8:                  dtostrf(OTcurrentSystemState.Remoteparameter8, 0, 2, buffer); return buffer;
    case OT_OpenThermVersionMaster:            dtostrf(OTcurrentSystemState.OpenThermVersionMaster, 0, 2, buffer); return buffer;
    case OT_OpenThermVersionSlave:             dtostrf(OTcurrentSystemState.OpenThermVersionSlave, 0, 2, buffer); return buffer;
    case OT_Statusflags:                       dtostrf(OTcurrentSystemState.Statusflags, 0, 2, buffer); return buffer;
    case OT_ASFflags:                          dtostrf(OTcurrentSystemState.ASFflags, 0, 2, buffer); return buffer;
    case OT_MasterConfigMemberIDcode:          dtostrf(OTcurrentSystemState.MasterConfigMemberIDcode, 0, 2, buffer); return buffer;
    case OT_SlaveConfigMemberIDcode:           dtostrf(OTcurrentSystemState.SlaveConfigMemberIDcode, 0, 2, buffer); return buffer;
    case OT_Command:                           dtostrf(OTcurrentSystemState.Command, 0, 2, buffer); return buffer;
    case OT_RBPflags:                          dtostrf(OTcurrentSystemState.RBPflags, 0, 2, buffer); return buffer;
    case OT_TSP:                               dtostrf(OTcurrentSystemState.TSP, 0, 2, buffer); return buffer;
    case OT_TSPindexTSPvalue:                  dtostrf(OTcurrentSystemState.TSPindexTSPvalue, 0, 2, buffer); return buffer;
    case OT_FHBsize:                           dtostrf(OTcurrentSystemState.FHBsize, 0, 2, buffer); return buffer;
    case OT_FHBindexFHBvalue:                  dtostrf(OTcurrentSystemState.FHBindexFHBvalue, 0, 2, buffer); return buffer;
    case OT_MaxCapacityMinModLevel:            dtostrf(OTcurrentSystemState.MaxCapacityMinModLevel, 0, 2, buffer); return buffer;
    case OT_DayTime:                           dtostrf(OTcurrentSystemState.DayTime, 0, 2, buffer); return buffer;
    case OT_Date:                              dtostrf(OTcurrentSystemState.Date, 0, 2, buffer); return buffer;
    case OT_Year:                              dtostrf(OTcurrentSystemState.Year, 0, 2, buffer); return buffer;
    case OT_TdhwSetUBTdhwSetLB:                dtostrf(OTcurrentSystemState.TdhwSetUBTdhwSetLB, 0, 2, buffer); return buffer;
    case OT_MaxTSetUBMaxTSetLB:                dtostrf(OTcurrentSystemState.MaxTSetUBMaxTSetLB, 0, 2, buffer); return buffer;
    case OT_HcratioUBHcratioLB:                dtostrf(OTcurrentSystemState.HcratioUBHcratioLB, 0, 2, buffer); return buffer;
    case OT_Remoteparameter4boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter4boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter5boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter5boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter6boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter6boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter7boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter7boundaries, 0, 2, buffer); return buffer;
    case OT_Remoteparameter8boundaries:        dtostrf(OTcurrentSystemState.Remoteparameter8boundaries, 0, 2, buffer); return buffer;
    case OT_RemoteOverrideFunction:            dtostrf(OTcurrentSystemState.RemoteOverrideFunction, 0, 2, buffer); return buffer;
    case OT_OEMDiagnosticCode:                 dtostrf(OTcurrentSystemState.OEMDiagnosticCode, 0, 2, buffer); return buffer;
    case OT_BurnerStarts:                      dtostrf(OTcurrentSystemState.BurnerStarts, 0, 2, buffer); return buffer;
    case OT_CHPumpStarts:                      dtostrf(OTcurrentSystemState.CHPumpStarts, 0, 2, buffer); return buffer;
    case OT_DHWPumpValveStarts:                dtostrf(OTcurrentSystemState.DHWPumpValveStarts, 0, 2, buffer); return buffer;
    case OT_DHWBurnerStarts:                   dtostrf(OTcurrentSystemState.DHWBurnerStarts, 0, 2, buffer); return buffer;
    case OT_BurnerOperationHours:              dtostrf(OTcurrentSystemState.BurnerOperationHours, 0, 2, buffer); return buffer;
    case OT_CHPumpOperationHours:              dtostrf(OTcurrentSystemState.CHPumpOperationHours, 0, 2, buffer); return buffer;
    case OT_DHWPumpValveOperationHours:        dtostrf(OTcurrentSystemState.DHWPumpValveOperationHours, 0, 2, buffer); return buffer;
    case OT_DHWBurnerOperationHours:           dtostrf(OTcurrentSystemState.DHWBurnerOperationHours, 0, 2, buffer); return buffer;
    case OT_Brand:                             dtostrf(OTcurrentSystemState.Brand, 0, 2, buffer); return buffer;
    case OT_BrandVersion:                      dtostrf(OTcurrentSystemState.BrandVersion, 0, 2, buffer); return buffer;
    case OT_BrandSerialNumber:                 dtostrf(OTcurrentSystemState.BrandSerialNumber, 0, 2, buffer); return buffer;
    case OT_CoolingOperationHours:             dtostrf(OTcurrentSystemState.CoolingOperationHours, 0, 2, buffer); return buffer;
    case OT_PowerCycles:                       dtostrf(OTcurrentSystemState.PowerCycles, 0, 2, buffer); return buffer;
    case OT_MasterVersion:                     dtostrf(OTcurrentSystemState.MasterVersion, 0, 2, buffer); return buffer;
    case OT_SlaveVersion:                      dtostrf(OTcurrentSystemState.SlaveVersion, 0, 2, buffer); return buffer;
    case OT_StatusVH:                          dtostrf(OTcurrentSystemState.StatusVH, 0, 2, buffer); return buffer;
    case OT_ControlSetpointVH:                 dtostrf(OTcurrentSystemState.ControlSetpointVH, 0, 2, buffer); return buffer;
    case OT_ASFFaultCodeVH:                    dtostrf(OTcurrentSystemState.ASFFaultCodeVH, 0, 2, buffer); return buffer;
    case OT_DiagnosticCodeVH:                  dtostrf(OTcurrentSystemState.DiagnosticCodeVH, 0, 2, buffer); return buffer;
    case OT_ConfigMemberIDVH:                  dtostrf(OTcurrentSystemState.ConfigMemberIDVH, 0, 2, buffer); return buffer;
    case OT_OpenthermVersionVH:                dtostrf(OTcurrentSystemState.OpenthermVersionVH, 0, 2, buffer); return buffer;
    case OT_VersionTypeVH:                     dtostrf(OTcurrentSystemState.VersionTypeVH, 0, 2, buffer); return buffer;
    case OT_RelativeVentilation:               dtostrf(OTcurrentSystemState.RelativeVentilation, 0, 2, buffer); return buffer;
    case OT_RelativeHumidityExhaustAir:        dtostrf(OTcurrentSystemState.RelativeHumidityExhaustAir, 0, 2, buffer); return buffer;
    case OT_CO2LevelExhaustAir:                dtostrf(OTcurrentSystemState.CO2LevelExhaustAir, 0, 2, buffer); return buffer;
    case OT_SupplyInletTemperature:            dtostrf(OTcurrentSystemState.SupplyInletTemperature, 0, 2, buffer); return buffer;
    case OT_SupplyOutletTemperature:           dtostrf(OTcurrentSystemState.SupplyOutletTemperature, 0, 2, buffer); return buffer;
    case OT_ExhaustInletTemperature:           dtostrf(OTcurrentSystemState.ExhaustInletTemperature, 0, 2, buffer); return buffer;
    case OT_ExhaustOutletTemperature:          dtostrf(OTcurrentSystemState.ExhaustOutletTemperature, 0, 2, buffer); return buffer;
    case OT_ActualExhaustFanSpeed:             dtostrf(OTcurrentSystemState.ActualExhaustFanSpeed, 0, 2, buffer); return buffer;
    case OT_ActualSupplyFanSpeed:              dtostrf(OTcurrentSystemState.ActualSupplyFanSpeed, 0, 2, buffer); return buffer;
    case OT_RemoteParameterSettingVH:          dtostrf(OTcurrentSystemState.RemoteParameterSettingVH, 0, 2, buffer); return buffer;
    case OT_NominalVentilationValue:           dtostrf(OTcurrentSystemState.NominalVentilationValue, 0, 2, buffer); return buffer;
    case OT_TSPNumberVH:                       dtostrf(OTcurrentSystemState.TSPNumberVH, 0, 2, buffer); return buffer;
    case OT_TSPEntryVH:                        dtostrf(OTcurrentSystemState.TSPEntryVH, 0, 2, buffer); return buffer;
    case OT_FaultBufferSizeVH:                 dtostrf(OTcurrentSystemState.FaultBufferSizeVH, 0, 2, buffer); return buffer;
    case OT_FaultBufferEntryVH:                dtostrf(OTcurrentSystemState.FaultBufferEntryVH, 0, 2, buffer); return buffer;
    case OT_FanSpeed:                          dtostrf(OTcurrentSystemState.FanSpeed, 0, 2, buffer); return buffer;
    case OT_ElectricalCurrentBurnerFlame:      dtostrf(OTcurrentSystemState.ElectricalCurrentBurnerFlame, 0, 2, buffer); return buffer;
    case OT_TRoomCH2:                          dtostrf(OTcurrentSystemState.TRoomCH2, 0, 2, buffer); return buffer;
    case OT_RelativeHumidity:                  dtostrf(OTcurrentSystemState.RelativeHumidity, 0, 2, buffer); return buffer;
    case OT_RFstrengthbatterylevel:            dtostrf(OTcurrentSystemState.RFstrengthbatterylevel, 0, 2, buffer); return buffer;
    case OT_OperatingMode_HC1_HC2_DHW:         dtostrf(OTcurrentSystemState.OperatingMode_HC1_HC2_DHW, 0, 2, buffer); return buffer;
    case OT_ElectricityProducerStarts:         dtostrf(OTcurrentSystemState.ElectricityProducerStarts, 0, 2, buffer); return buffer;
    case OT_ElectricityProducerHours:          dtostrf(OTcurrentSystemState.ElectricityProducerHours, 0, 2, buffer); return buffer;
    case OT_ElectricityProduction:             dtostrf(OTcurrentSystemState.ElectricityProduction, 0, 2, buffer); return buffer;
    case OT_CumulativElectricityProduction:    dtostrf(OTcurrentSystemState.CumulativElectricityProduction, 0, 2, buffer); return buffer;
    case OT_BurnerUnsuccessfulStarts:          dtostrf(OTcurrentSystemState.BurnerUnsuccessfulStarts, 0, 2, buffer); return buffer;
    case OT_FlameSignalTooLow:                 dtostrf(OTcurrentSystemState.FlameSignalTooLow, 0, 2, buffer); return buffer;
    case OT_RemehadFdUcodes:                   dtostrf(OTcurrentSystemState.RemehadFdUcodes, 0, 2, buffer); return buffer;
    case OT_RemehaServicemessage:              dtostrf(OTcurrentSystemState.RemehaServicemessage, 0, 2, buffer); return buffer;
    case OT_RemehaDetectionConnectedSCU:       dtostrf(OTcurrentSystemState.RemehaDetectionConnectedSCU, 0, 2, buffer); return buffer;
    case OT_SolarStorageMaster:                dtostrf(OTcurrentSystemState.SolarStorageStatus, 0, 2, buffer); return buffer;
    case OT_SolarStorageASFflags:              dtostrf(OTcurrentSystemState.SolarStorageASFflags, 0, 2, buffer); return buffer;
    case OT_SolarStorageSlaveConfigMemberIDcode:  dtostrf(OTcurrentSystemState.SolarStorageSlaveConfigMemberIDcode, 0, 2, buffer); return buffer;
    case OT_SolarStorageVersionType:           dtostrf(OTcurrentSystemState.SolarStorageVersionType, 0, 2, buffer); return buffer;
    case OT_SolarStorageTSP:                   dtostrf(OTcurrentSystemState.SolarStorageTSP, 0, 2, buffer); return buffer;
    case OT_SolarStorageTSPindexTSPvalue:      dtostrf(OTcurrentSystemState.SolarStorageTSPindexTSPvalue, 0, 2, buffer); return buffer;
    case OT_SolarStorageFHBsize:               dtostrf(OTcurrentSystemState.SolarStorageFHBsize, 0, 2, buffer); return buffer;
    case OT_SolarStorageFHBindexFHBvalue:      dtostrf(OTcurrentSystemState.SolarStorageFHBindexFHBvalue, 0, 2, buffer); return buffer;
    default: 
      strncpy_P(buffer, PSTR("Error: not implemented yet!\r\n"), sizeof(buffer) - 1);
      buffer[sizeof(buffer) - 1] = '\0';
      return buffer;
  } // switch
} // getOTGWValue

void startOTGWstream()
{
  OTGWstream.begin();
}

//---------[ Upgrade PIC stuff taken from Schelte Bron's NodeMCU Firmware ]---------

void upgradepicnow(const char *filename) {
  if (OTGWSerial.busy()) {
    DebugTln(F("ERROR: PIC upgrade already in progress, ignoring request"));
    return; // if already in programming mode, never call it twice
  }
  DebugTln(F(""));
  DebugTln(F(">>> Initiating PIC Upgrade <<<"));
  DebugTf(PSTR("Hex file: %s\r\n"), filename);
  fwupgradestart(filename);
  // Upgrade runs in background via OTGWSerial callbacks and upgradeTick called from available()
  DebugTln(F("PIC upgrade object created and started"));
  DebugTln(F("Upgrade runs in background via:"));
  DebugTln(F("  - handleOTGW() processes serial data"));
  DebugTln(F("  - OTGWSerial.available() calls upgradeTick()"));
  DebugTln(F("  - Progress callbacks update WebUI"));
  DebugTln(F(">>> Background upgrade active <<<"));
  DebugTln(F(""));
}

// Helper function to escape JSON strings for WebSocket messages
// Escapes quotes, backslashes, and control characters
// Note: Input is assumed to be null-terminated; embedded nulls are replaced with spaces
static void jsonEscape(const char *in, char *out, size_t outSize) {
  size_t j = 0;
  for (size_t i = 0; in[i] != '\0' && j + 1 < outSize; ++i) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      if (j + 2 >= outSize) break;
      out[j++] = '\\';
      out[j++] = c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      if (j + 1 >= outSize) break;
      out[j++] = ' '; // Replace control characters with space
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

void fwupgradedone(OTGWError result, short errors = 0, short retries = 0) {
  DebugTln(F(""));
  DebugTln(F("=== PIC Upgrade Complete ==="));
  DebugTf(PSTR("Result code: %d\r\n"), (int)result);
  DebugTf(PSTR("Errors: %d, Retries: %d\r\n"), errors, retries);
  switch (result) {
    case OTGWError::OTGW_ERROR_NONE:          snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("PIC upgrade was successful")); break;
    case OTGWError::OTGW_ERROR_MEMORY:        snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Not enough memory available")); break;
    case OTGWError::OTGW_ERROR_INPROG:        snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Firmware upgrade in progress")); break;
    case OTGWError::OTGW_ERROR_HEX_ACCESS:    snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Could not open hex file")); break;
    case OTGWError::OTGW_ERROR_HEX_FORMAT:    snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Invalid format of hex file")); break;
    case OTGWError::OTGW_ERROR_HEX_DATASIZE:  snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Wrong data size in hex file")); break;
    case OTGWError::OTGW_ERROR_HEX_CHECKSUM:  snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Bad checksum in hex file")); break;
    case OTGWError::OTGW_ERROR_MAGIC:         snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Hex file does not contain expected data")); break;
    case OTGWError::OTGW_ERROR_RESET:         snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("PIC reset failed")); break;
    case OTGWError::OTGW_ERROR_RETRIES:       snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Too many retries")); break;
    case OTGWError::OTGW_ERROR_MISMATCHES:    snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Too many mismatches")); break;
    case OTGWError::OTGW_ERROR_DEVICE:        snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Wrong PIC (16F88 <=> 16F1847)")); break;
    default:                                  snprintf_P(errorupgrade, sizeof(errorupgrade), PSTR("Unknown state")); break;
  }
  DebugTf(PSTR("Message: %s\r\n"), CSTR(errorupgrade));
  DebugTf(PSTR("File: %s\r\n"), currentPICFlashFile);
  OTGWDebugTf(PSTR("Upgrade finished: Errorcode = %d - %s - %d retries, %d errors\r\n"), result, CSTR(errorupgrade), retries, errors);
  
  // Mark flash as complete
  isPICFlashing = false;
  currentPICFlashProgress = (result == OTGWError::OTGW_ERROR_NONE) ? 100 : -1; // -1 indicates error
  if (result == OTGWError::OTGW_ERROR_NONE) {
    DebugTln(F("*** UPGRADE SUCCESSFUL ***"));
  } else {
    DebugTln(F("*** UPGRADE FAILED ***"));
  }

#ifndef DISABLE_WEBSOCKET
  // Send completion message in format frontend expects
  // Escape strings to prevent JSON injection
  char buf[320]; // Sized for escaped filename (129) + error (257) + JSON overhead (~70)
  char filenameEsc[129]; // currentPICFlashFile is 65 chars, doubled for worst-case escaping
  char errorEsc[257]; // errorupgrade is 129 chars, doubled for worst-case escaping
  jsonEscape(currentPICFlashFile, filenameEsc, sizeof(filenameEsc));
  jsonEscape(errorupgrade, errorEsc, sizeof(errorEsc));
  
  const char *state = (result == OTGWError::OTGW_ERROR_NONE) ? "end" : "error";
  int written = snprintf_P(buf, sizeof(buf), 
    PSTR("{\"state\":\"%s\",\"flash_written\":100,\"flash_total\":100,\"filename\":\"%s\",\"error\":\"%s\"}"),
    state, filenameEsc, errorEsc);
  
  if (written > 0 && written < (int)sizeof(buf)) {
    DebugTln(F("Sending WebSocket completion message..."));
    sendWebSocketJSON(buf);
  } else {
    DebugTln(F("ERROR: WebSocket message too large, not sent"));
  }
#endif

  DebugTln(F("============================"));
  DebugTln(F(""));

  // Note: Keep filename and progress for polling API until next flash starts
}

void fwupgradestep(int pct) {
  // Only log every 10% to avoid spam, plus always log 0% and 100%
  static int lastReportedPct = -1;
  bool shouldLog = (pct == 0) || (pct == 100) || (pct % 10 == 0 && pct != lastReportedPct);
  if (shouldLog) {
    DebugTf(PSTR("Upgrade progress: %d%%\r\n"), pct);
    lastReportedPct = pct;
  }
  
  // Update progress for polling API
  currentPICFlashProgress = pct;
  
#ifndef DISABLE_WEBSOCKET
  // Send progress message in format frontend expects
  // Use percentage as flash_written for progress display
  char buf[256]; // Sized for escaped filename (129) + JSON overhead (~90)
  char filenameEsc[129]; // currentPICFlashFile is 65 chars, doubled for worst-case escaping
  jsonEscape(currentPICFlashFile, filenameEsc, sizeof(filenameEsc));
  
  const char *state = (pct == 0) ? "start" : "write";
  int written = snprintf_P(buf, sizeof(buf), 
    PSTR("{\"state\":\"%s\",\"flash_written\":%d,\"flash_total\":100,\"filename\":\"%s\"}"),
    state, pct, filenameEsc);
  
  if (written > 0 && written < (int)sizeof(buf)) {
    sendWebSocketJSON(buf);
  }
#endif
}

void fwreportinfo(OTGWFirmware fw, const char *version) {
    DebugTln(F("Callback: fwreportinfo"));
    strlcpy(sPICfwversion, version, sizeof(sPICfwversion));
    //sPICfwversion = String(OTGWSerial.firmwareVersion());
    DebugTf(PSTR("Current firmware version: %s\r\n"), sPICfwversion);
    strlcpy(sPICdeviceid, OTGWSerial.processorToString().c_str(), sizeof(sPICdeviceid));
    DebugTf(PSTR("Current device id: %s\r\n"), sPICdeviceid);
    //instead of using the firmware string
    strlcpy(sPICtype, OTGWSerial.firmwareToString(fw).c_str(), sizeof(sPICtype));
    OTGWDebugTf(PSTR("Current firmware type: %s\r\n"), sPICtype);
}

void fwupgradestart(const char *hexfile) {
  DebugTln(F("--- fwupgradestart() ---"));
  DebugTf(PSTR("Hex file path: %s\r\n"), hexfile);
  OTGWError result;
  
  // Store filename for WebSocket progress messages and polling API
  // Extract just the filename from the path
  const char *filename = strrchr(hexfile, '/');
  if (filename) {
    filename++; // Skip the '/'
  } else {
    filename = hexfile; // No path, use as-is
  }
  strlcpy(currentPICFlashFile, filename, sizeof(currentPICFlashFile));
  DebugTf(PSTR("Extracted filename: %s\r\n"), currentPICFlashFile);
  
  // Mark flash as started
  isPICFlashing = true;
  currentPICFlashProgress = 0;
  errorupgrade[0] = '\0'; // Clear previous error
  DebugTln(F("Flash state set: isPICFlashing=true, progress=0"));

  // Turn on LED to indicate flashing
  digitalWrite(LED1, LOW);
  DebugTln(F("LED1 activated (LOW=ON)"));
  DebugTln(F("Calling OTGWSerial.startUpgrade()..."));
  result = OTGWSerial.startUpgrade(hexfile);
  if (result!= OTGWError::OTGW_ERROR_NONE) {
    DebugTf(PSTR("ERROR: startUpgrade() failed immediately with error code %d\r\n"), (int)result);
    fwupgradedone(result);
  } else {
    DebugTln(F("SUCCESS: startUpgrade() returned OTGW_ERROR_NONE"));
    DebugTln(F("Registering callbacks..."));
    OTGWSerial.registerFinishedCallback(fwupgradedone);
    OTGWSerial.registerProgressCallback(fwupgradestep);
    DebugTln(F("Callbacks registered: fwupgradedone, fwupgradestep"));
    DebugTln(F("Upgrade is now running in background"));
  }
  DebugTln(F("--- fwupgradestart() complete ---"));
}

String checkforupdatepic(String filename){
  WiFiClient client;
  HTTPClient http;
  String latest = "";
  int code;

  http.begin(client, "http://otgw.tclcode.com/download/" + String(sPICdeviceid) + "/" + filename);
  char useragent[40] = "esp8266-otgw-firmware/";
  strlcat(useragent, _SEMVER_CORE, sizeof(useragent));
  http.setUserAgent(useragent);
  http.collectHeaders(hexheaders, 2);
  code = http.sendRequest("HEAD");
  if (code == HTTP_CODE_OK) {
    for (int i = 0; i< http.headers(); i++) {
      DebugTf(PSTR("%s: %s\r\n"), hexheaders[i], http.header(i).c_str());
    }
    latest = http.header(1);
    DebugTf(PSTR("Update %s -> [%s]\r\n"), filename.c_str(), latest.c_str());
  } else OTGWDebugln(F("Failed to fetch version from Schelte Bron website"));
  http.end(); // Always close connection, even on failure (Finding #24)

  return latest; 
}

void refreshpic(String filename, String version) {
  if (strcmp(sPICdeviceid, "unknown") == 0) return; // no pic version found, don't upgrade

  WiFiClient client;
  HTTPClient http;
  String latest;
  int code;

  latest = checkforupdatepic(filename);

  if (latest != version) {
    OTGWDebugTf(PSTR("Update (%s)%s: %s -> %s\r\n"), sPICdeviceid, filename.c_str(), version.c_str(), latest.c_str());
    http.begin(client, "http://otgw.tclcode.com/download/" + String(sPICdeviceid) + "/" + filename);
    char useragent[40] = "esp8266-otgw-firmware/";
    strlcat(useragent, _SEMVER_CORE, sizeof(useragent));
    http.setUserAgent(useragent);
    code = http.GET();
    if (code == HTTP_CODE_OK) {
      File f = LittleFS.open("/" + String(sPICdeviceid) + "/" + filename, "w");
      if (f) {
        http.writeToStream(&f);
        f.close();
        String verfile = "/" + String(sPICdeviceid) + "/" + filename;
        verfile.replace(".hex", ".ver");
        f = LittleFS.open(verfile, "w");
        if (f) {
          f.print(latest + "\n");
          f.close();
          OTGWDebugTln(F("Update successful"));
        }
      }
    }
    http.end();
  }
}

// --- Pending Upgrade Logic ---
String pendingUpgradePath = "";

void handlePendingUpgrade() {
  if (pendingUpgradePath != F("")) {
    DebugTln(F(""));
    DebugTln(F("=== Starting Deferred PIC Upgrade ==="));
    DebugTf(PSTR("Hex file path: %s\r\n"), pendingUpgradePath.c_str());
    DebugTf(PSTR("Flash state: isESPFlashing=%d, isPICFlashing=%d\r\n"), isESPFlashing, isPICFlashing);
    DebugTf(PSTR("Free heap: %d bytes\r\n"), ESP.getFreeHeap());
    upgradepicnow(pendingUpgradePath.c_str());
    pendingUpgradePath = "";
    DebugTln(F("Deferred upgrade initiated, upgrade now runs in background"));
    DebugTln(F("Monitor progress via telnet or WebUI"));
    DebugTln(F("======================================="));
  }
}

void upgradepic() {
  const String action = httpServer.arg("action");
  const String filename = httpServer.arg("name");
  const String version = httpServer.arg("version");

  DebugTln(F("=== PIC Flash HTTP Request Received ==="));
  DebugTf(PSTR("Action: %s, File: %s, Version: %s\r\n"), action.c_str(), filename.c_str(), version.c_str());
  DebugTf(PSTR("PIC Device ID: %s\r\n"), sPICdeviceid);
  DebugTf(PSTR("Current state: isPICFlashing=%d, isESPFlashing=%d\r\n"), isPICFlashing, isESPFlashing);
  
  if (action.isEmpty() || filename.isEmpty()) {
    DebugTln(F("ERROR: Missing action or filename parameter"));
    httpServer.send_P(400, PSTR("text/plain"), PSTR("Missing action or name"));
    return;
  }

  if (strcmp(sPICdeviceid, "unknown") == 0) {
    DebugTln(F("ERROR: PIC device id is unknown, cannot upgrade"));
    httpServer.send_P(400, PSTR("text/plain"), PSTR("PIC device not detected"));
    return; // no pic version found, don't upgrade
  }
  
  if (action == F("upgrade")) {
    DebugTf(PSTR("Upgrade requested for /%s/%s\r\n"), sPICdeviceid, filename.c_str());
    httpServer.send_P(200, PSTR("application/json"), PSTR("{\"status\":\"started\"}"));
    httpServer.client().flush();  // Ensure response buffer is sent to client
    DebugTln(F("HTTP response sent and flushed"));
    
    // Defer the actual upgrade start to the main loop to ensure HTTP response is sent
    pendingUpgradePath = "/" + String(sPICdeviceid) + "/" + filename;
    DebugTf(PSTR("Pending upgrade queued: [%s]\r\n"), pendingUpgradePath.c_str());
    DebugTln(F("=== HTTP handler complete, upgrade will start in main loop ==="));
    return;
  } else if (action == F("refresh")) {
    DebugTf(PSTR("Refresh %s/%s\r\n"), sPICdeviceid, filename.c_str());
    refreshpic(filename, version);
  } else if (action == F("delete")) {
    DebugTf(PSTR("Delete %s/%s\r\n"), sPICdeviceid, filename.c_str());
    char path[64];
    snprintf_P(path, sizeof(path), PSTR("/%s/%s"), sPICdeviceid, filename.c_str());
    LittleFS.remove(path);
    char *ext = strstr(path, ".hex");
    if (ext) {
      strlcpy(ext, ".ver", sizeof(path) - (ext - path));
      LittleFS.remove(path);
    }
  }
  httpServer.sendHeader(F("Location"), F("index.html#tabPICflash"), true);
  httpServer.send_P(303, PSTR("text/html; charset=UTF-8"), PSTR("<a href='index.html#tabPICflash'>Return</a>"));
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
