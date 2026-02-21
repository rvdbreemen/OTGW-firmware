/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v1.2.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**      Modified version from (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <PubSubClient.h>           // MQTT client publish and subscribe functionality
#include <ctype.h>
#include <pgmspace.h>
#include "OTGW-Core.h"              // Core OpenTherm data structures and functions

// MQTT Streaming Mode - ALWAYS ENABLED
// Large auto-discovery messages are sent in 128-byte chunks instead of
// requiring a large buffer resize. This prevents heap fragmentation on ESP8266.
// Similar to ESPHome's chunked MQTT publishing strategy.

#define MQTTDebugTln(...) ({ if (bDebugMQTT) DebugTln(__VA_ARGS__);    })
#define MQTTDebugln(...)  ({ if (bDebugMQTT) Debugln(__VA_ARGS__);    })
#define MQTTDebugTf(...)  ({ if (bDebugMQTT) DebugTf(__VA_ARGS__);    })
#define MQTTDebugf(...)   ({ if (bDebugMQTT) Debugf(__VA_ARGS__);    })
#define MQTTDebugT(...)   ({ if (bDebugMQTT) DebugT(__VA_ARGS__);    })
#define MQTTDebug(...)    ({ if (bDebugMQTT) Debug(__VA_ARGS__);    })

void doAutoConfigure(bool bForceAll = false);

// Declare some variables within global scope

static IPAddress  MQTTbrokerIP;
static char       MQTTbrokerIPchar[20];
constexpr size_t  MQTT_ID_MAX_LEN = 96;
constexpr size_t  MQTT_NAMESPACE_MAX_LEN = 192;
constexpr size_t  MQTT_TOPIC_MAX_LEN = 200;
constexpr size_t  MQTT_MSG_MAX_LEN = 1200;
constexpr size_t  MQTT_CFG_LINE_MAX_LEN = 1200;

struct MQTTAutoConfigBuffers {
  char line[MQTT_CFG_LINE_MAX_LEN];
  char topic[MQTT_TOPIC_MAX_LEN];
  char msg[MQTT_MSG_MAX_LEN];
};

// Shared autoconfig workspace for both autoconfig code paths.
// This keeps only one persistent buffer set in RAM.
static MQTTAutoConfigBuffers mqttAutoConfigBuffers;

// Guard shared MQTT autoconfig buffers against accidental re-entry.
// Current firmware is effectively single-threaded, but this protects future
// callback/timer refactors from clobbering the shared workspace.
static bool mqttAutoConfigInProgress = false;

struct MQTTAutoConfigSessionLock {
  bool locked = false;
  MQTTAutoConfigSessionLock() {
    if (!mqttAutoConfigInProgress) {
      mqttAutoConfigInProgress = true;
      locked = true;
    }
  }
  ~MQTTAutoConfigSessionLock() {
    if (locked) mqttAutoConfigInProgress = false;
  }
};

static            PubSubClient MQTTclient(wifiClient);

int8_t            reconnectAttempts = 0;
char              lastMQTTtimestamp[15] = "";

enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_WAIT_CONNECTION_ATTEMPT, MQTT_STATE_WAIT_FOR_RECONNECT, MQTT_STATE_ERROR };
enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

static char       MQTTclientId[MQTT_ID_MAX_LEN];
static char       MQTTPubNamespace[MQTT_NAMESPACE_MAX_LEN];
static char       MQTTSubNamespace[MQTT_NAMESPACE_MAX_LEN];
static char       NodeId[MQTT_ID_MAX_LEN];

// Trim whitespace on both sides in-place
static void trimInPlace(char *buffer) {
  if (buffer == nullptr) return;
  size_t len = strlen(buffer);
  while (len > 0 && isspace(static_cast<unsigned char>(buffer[len - 1]))) {
    buffer[--len] = '\0';
  }
  size_t start = 0;
  while (buffer[start] != '\0' && isspace(static_cast<unsigned char>(buffer[start]))) {
    start++;
  }
  if (start > 0) {
    memmove(buffer, buffer + start, len - start + 1);
  }
}

// Replace all occurrences of token with replacement, guarding buffer size
// MOVED TO helperStuff.ino
// static bool replaceAll ...

static bool splitLine(char *sIn, char del, byte &cID, char *cKey, size_t keySize, char *cVal, size_t valSize) {
  if (!sIn || !cKey || !cVal) return false;
  char *comment = strstr(sIn, "//");
  if (comment) *comment = '\0';

  trimInPlace(sIn);
  if (strlen(sIn) <= 3) return false;

  char *firstDel = strchr(sIn, del);
  if (firstDel == nullptr || firstDel == sIn || *(firstDel + 1) == '\0') return false;
  char *secondDel = strchr(firstDel + 1, del);
  if (secondDel == nullptr || secondDel <= firstDel + 1 || *(secondDel + 1) == '\0') return false;

  char idBuf[8]{0};
  strlcpy(idBuf, sIn, sizeof(idBuf));
  char *idDel = strchr(idBuf, del);
  if (!idDel) return false;
  *idDel = '\0';
  trimInPlace(idBuf);
  cID = static_cast<byte>(strtoul(idBuf, nullptr, 10));

  strlcpy(cKey, firstDel + 1, keySize);
  char *keyEnd = strchr(cKey, del);
  if (!keyEnd) return false;
  *keyEnd = '\0';
  trimInPlace(cKey);

  strlcpy(cVal, secondDel + 1, valSize);
  trimInPlace(cVal);

  return strlen(cKey) > 0 && strlen(cVal) > 0;
}

static void buildNamespace(char *dest, size_t destSize, const char *base, const char *segment, const char *node) {
  if (!dest) return;
  dest[0] = '\0';
  if (!base || !segment || !node) return;
  strlcpy(dest, base, destSize);
  size_t len = strlen(dest);
  if (len > 0 && dest[len - 1] == '/') dest[len - 1] = '\0';
  strlcat(dest, "/", destSize);
  strlcat(dest, segment, destSize);
  strlcat(dest, "/", destSize);
  strlcat(dest, node, destSize);
}

//set command list
// Move strings to PROGMEM to save RAM
const char s_raw[] PROGMEM = "raw";
const char s_temp[] PROGMEM = "temp";
const char s_on[] PROGMEM = "on";
const char s_level[] PROGMEM = "level";
const char s_function[] PROGMEM = "function";
const char s_empty[] PROGMEM = "";

const char s_cmd_command[] PROGMEM = "command";
const char s_cmd_setpoint[] PROGMEM = "setpoint";
const char s_cmd_constant[] PROGMEM = "constant";
const char s_cmd_outside[] PROGMEM = "outside";
const char s_cmd_hotwater[] PROGMEM = "hotwater";
const char s_cmd_gatewaymode[] PROGMEM = "gatewaymode";
const char s_cmd_setback[] PROGMEM = "setback";
const char s_cmd_maxchsetpt[] PROGMEM = "maxchsetpt";
const char s_cmd_maxdhwsetpt[] PROGMEM = "maxdhwsetpt";
const char s_cmd_maxmodulation[] PROGMEM = "maxmodulation";
const char s_cmd_ctrlsetpt[] PROGMEM = "ctrlsetpt";
const char s_cmd_ctrlsetpt2[] PROGMEM = "ctrlsetpt2";
const char s_cmd_chenable[] PROGMEM = "chenable";
const char s_cmd_chenable2[] PROGMEM = "chenable2";
const char s_cmd_ventsetpt[] PROGMEM = "ventsetpt";
const char s_cmd_temperaturesensor[] PROGMEM = "temperaturesensor";
const char s_cmd_addalternative[] PROGMEM = "addalternative";
const char s_cmd_delalternative[] PROGMEM = "delalternative";
const char s_cmd_unknownid[] PROGMEM = "unknownid";
const char s_cmd_knownid[] PROGMEM = "knownid";
const char s_cmd_priomsg[] PROGMEM = "priomsg";
const char s_cmd_setresponse[] PROGMEM = "setresponse";
const char s_cmd_clearrespons[] PROGMEM = "clearrespons";
const char s_cmd_resetcounter[] PROGMEM = "resetcounter";
const char s_cmd_ignoretransitations[] PROGMEM = "ignoretransitations";
const char s_cmd_overridehb[] PROGMEM = "overridehb";
const char s_cmd_forcethermostat[] PROGMEM = "forcethermostat";
const char s_cmd_voltageref[] PROGMEM = "voltageref";
const char s_cmd_debugptr[] PROGMEM = "debugptr";

const char s_otgw_TT[] PROGMEM = "TT";
const char s_otgw_TC[] PROGMEM = "TC";
const char s_otgw_OT[] PROGMEM = "OT";
const char s_otgw_HW[] PROGMEM = "HW";
const char s_otgw_GW[] PROGMEM = "GW";
const char s_otgw_SB[] PROGMEM = "SB";
const char s_otgw_SH[] PROGMEM = "SH";
const char s_otgw_SW[] PROGMEM = "SW";
const char s_otgw_MM[] PROGMEM = "MM";
const char s_otgw_CS[] PROGMEM = "CS";
const char s_otgw_C2[] PROGMEM = "C2";
const char s_otgw_CH[] PROGMEM = "CH";
const char s_otgw_H2[] PROGMEM = "H2";
const char s_otgw_VS[] PROGMEM = "VS";
const char s_otgw_TS[] PROGMEM = "TS";
const char s_otgw_AA[] PROGMEM = "AA";
const char s_otgw_DA[] PROGMEM = "DA";
const char s_otgw_UI[] PROGMEM = "UI";
const char s_otgw_KI[] PROGMEM = "KI";
const char s_otgw_PM[] PROGMEM = "PM";
const char s_otgw_SR[] PROGMEM = "SR";
const char s_otgw_CR[] PROGMEM = "CR";
const char s_otgw_RS[] PROGMEM = "RS";
const char s_otgw_IT[] PROGMEM = "IT";
const char s_otgw_OH[] PROGMEM = "OH";
const char s_otgw_FT[] PROGMEM = "FT";
const char s_otgw_VR[] PROGMEM = "VR";
const char s_otgw_DP[] PROGMEM = "DP";

struct MQTT_set_cmd_t
{
    PGM_P setcmd;
    PGM_P otgwcmd;
    PGM_P ottype;
};


const MQTT_set_cmd_t setcmds[] PROGMEM = {
  {   s_cmd_command, s_empty, s_raw },
  {   s_cmd_setpoint, s_otgw_TT, s_temp },
  {   s_cmd_constant, s_otgw_TC, s_temp },
  {   s_cmd_outside, s_otgw_OT, s_temp },
  {   s_cmd_hotwater, s_otgw_HW, s_on },  // HW=0 (off), HW=1 (on), HW=P (DHW push), HW=<other> (auto)
  {   s_cmd_gatewaymode, s_otgw_GW, s_on },
  {   s_cmd_setback, s_otgw_SB, s_temp },
  {   s_cmd_maxchsetpt, s_otgw_SH, s_temp },
  {   s_cmd_maxdhwsetpt, s_otgw_SW, s_temp },
  {   s_cmd_maxmodulation, s_otgw_MM, s_level },        
  {   s_cmd_ctrlsetpt, s_otgw_CS, s_temp },        
  {   s_cmd_ctrlsetpt2, s_otgw_C2, s_temp },        
  {   s_cmd_chenable, s_otgw_CH, s_on },        
  {   s_cmd_chenable2, s_otgw_H2, s_on },        
  {   s_cmd_ventsetpt, s_otgw_VS, s_level },
  {   s_cmd_temperaturesensor, s_otgw_TS, s_function },
  {   s_cmd_addalternative, s_otgw_AA, s_function },
  {   s_cmd_delalternative, s_otgw_DA, s_function },
  {   s_cmd_unknownid, s_otgw_UI, s_function },
  {   s_cmd_knownid, s_otgw_KI, s_function },
  {   s_cmd_priomsg, s_otgw_PM, s_function },
  {   s_cmd_setresponse, s_otgw_SR, s_function },
  {   s_cmd_clearrespons, s_otgw_CR, s_function },
  {   s_cmd_resetcounter, s_otgw_RS, s_function },
  {   s_cmd_ignoretransitations, s_otgw_IT, s_function },
  {   s_cmd_overridehb, s_otgw_OH, s_function },
  {   s_cmd_forcethermostat, s_otgw_FT, s_function },
  {   s_cmd_voltageref, s_otgw_VR, s_function },
  {   s_cmd_debugptr, s_otgw_DP, s_function },
} ;

const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);

//===========================================================================================
void startMQTT() 
{
  if (!settingMQTTenable) return;
  
  // Static buffer allocation to prevent heap fragmentation on ESP8266
  // Allocates 1350 bytes permanently but avoids dynamic reallocation issues
  MQTTclient.setBufferSize(1350);
  
  stateMQTT = MQTT_STATE_INIT;
  //setup for mqtt discovery
  clearMQTTConfigDone();
  strlcpy(NodeId, CSTR(settingMQTTuniqueid), sizeof(NodeId));
  buildNamespace(MQTTPubNamespace, sizeof(MQTTPubNamespace), CSTR(settingMQTTtopTopic), "value", NodeId);
  buildNamespace(MQTTSubNamespace, sizeof(MQTTSubNamespace), CSTR(settingMQTTtopTopic), "set", NodeId);
  handleMQTT(); //initialize the MQTT statemachine
  // handleMQTT(); //then try to connect to MQTT
  // handleMQTT(); //now you should be connected to MQTT ready to send
}

bool bHAcycle = false;

// handles MQTT subscribe incoming stuff
void handleMQTTcallback(char* topic, byte* payload, unsigned int length) {

  if (bDebugMQTT) {
    DebugT(F("Message arrived on topic [")); Debug(topic); Debug(F("] = ["));
    for (unsigned int i = 0; i < length; i++) {
      Debug((char)payload[i]);
    }
    Debug(F("] (")); Debug(length); Debug(F(")")); Debugln(); DebugFlush();
  }  

  //detect home assistant going down...
  char msgPayload[50];
  int msglen = min((int)(length)+1, (int)sizeof(msgPayload));
  strlcpy(msgPayload, (char *)payload, msglen);
  if (strcasecmp_P(topic, PSTR("homeassistant/status")) == 0) {
    //incoming message on status, detect going down
    if (!settingMQTTharebootdetection) {
      //So if the HA reboot detection is turned of, we will just look for HA going online.
      //This means everytime there is "online" message, we will restart MQTT configuration, including the HA Auto Discovery. 
      bHAcycle = true; 
    }
    if (strcasecmp_P(msgPayload, PSTR("offline")) == 0){
      //home assistant went down
      DebugTln(F("Home Assistant went offline!"));
      bHAcycle = true; //set flag, so it triggers when it goes back online
    } else if ((strcasecmp_P(msgPayload, PSTR("online")) == 0) && bHAcycle){
      DebugTln(F("Home Assistant went online!"));
      bHAcycle = false; //clear flag, so it does not trigger again
      //restart stuff, to make sure it works correctly again
      startMQTT();  // fixing some issues with hanging HA AutoDiscovery in some scenario's?
    } else {
      DebugTf(PSTR("Home Assistant Status=[%s] and HA cycle status [%s]\r\n"), msgPayload, CBOOLEAN(bHAcycle)); 
    }
  }


  // parse the incoming topic and execute commands
  char* token;
  char otgwcmd[51]={0};

  //first check toptopic part, it can include the seperator, e.g. "myHome/OTGW" or "OTGW""
  const size_t topTopicLen = strlen(CSTR(settingMQTTtopTopic));
  if (strncmp(topic, CSTR(settingMQTTtopTopic), topTopicLen) != 0) {
    MQTTDebugln(F("MQTT: wrong top topic"));
    return;
  } else {
    //remove the top topic part
    MQTTDebugTf(PSTR("Parsing topic: %s/"), CSTR(settingMQTTtopTopic));
    topic += topTopicLen;
    while (*topic == '/') {
      topic++;
    }
  }
  // naming convention /set/<node id>/<command>
  token = strtok(topic, "/"); 
  if (token == NULL) { MQTTDebugln(F("MQTT: missing 'set' token")); return; }
  MQTTDebugf(PSTR("%s/"), token);
  if (strcasecmp(token, "set") == 0) {
    token = strtok(NULL, "/");
    if (token == NULL) { MQTTDebugln(F("MQTT: missing node-id token")); return; }
    MQTTDebugf(PSTR("%s/"), token); 
    if (strcasecmp(token, NodeId) == 0) {
      token = strtok(NULL, "/");
      MQTTDebugf(PSTR("%s"), token);
      if (token != NULL){
        //loop thru command list
        int i;
        for (i=0; i<nrcmds; i++){
          // Read setcmd pointer from Flash
          PGM_P pSetCmd = (PGM_P)pgm_read_ptr(&setcmds[i].setcmd);
          if (strcasecmp_P(token, pSetCmd) == 0){
            //found a match
            // Read ottype and otgwcmd from Flash
            PGM_P pOtType = (PGM_P)pgm_read_ptr(&setcmds[i].ottype);
            PGM_P pOtgwCmd = (PGM_P)pgm_read_ptr(&setcmds[i].otgwcmd);
            
            if (strcasecmp_P("raw", pOtType) == 0){
              //raw command
              snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s"), msgPayload);
              MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
              addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
            } else {
              //all other commands are <otgwcmd>=<payload message> 
              // Copy command string from Flash to temp buffer for snprintf
              char cmdBuf[10];
              strncpy_P(cmdBuf, pOtgwCmd, sizeof(cmdBuf));
              cmdBuf[sizeof(cmdBuf)-1] = 0; // Ensure null termination
              
              snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s=%s"), cmdBuf, msgPayload);
              MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
              addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
            }
            break; //exit loop
          } 
        }
        if (i >= nrcmds){
          //no match found
          MQTTDebugln();
          MQTTDebugTf(PSTR("No match found for command: [%s]\r\n"), token);
        }
      }
    }
  }
}

//===========================================================================================
// sendMQTT - streaming version for large messages (auto-discovery)
//===========================================================================================
// sendMQTT - streaming version for large messages (auto-discovery)
// Sends the message in small chunks to avoid buffer reallocation
// This prevents heap fragmentation on ESP8266
//===========================================================================================
void sendMQTT(const char* topic, const char *json);

// Forward declaration; implementation is provided later in this file
void sendMQTTStreaming(const char* topic, const char *json, const size_t len);

void handleMQTT() 
{  
  if (!settingMQTTenable) return;
  DECLARE_TIMER_SEC(timerMQTTwaitforconnect, 42, CATCH_UP_MISSED_TICKS);   // wait before trying to connect again
  DECLARE_TIMER_SEC(timerMQTTwaitforretry, 3, CATCH_UP_MISSED_TICKS);     // wait for retry

  //State debug timers
  DECLARE_TIMER_SEC(timerMQTTdebugwaitforreconnect, 13);
  DECLARE_TIMER_SEC(timerMQTTdebugerrorstate, 13);
  DECLARE_TIMER_SEC(timerMQTTdebugwaitconnectionattempt, 1);
  DECLARE_TIMER_SEC(timerMQTTdebugisconnected, 60);
  
  if (MQTTclient.connected()) MQTTclient.loop();  //always do a MQTTclient.loop() first

  switch(stateMQTT) 
  {
    case MQTT_STATE_INIT:  
      MQTTDebugTln(F("MQTT State: MQTT Initializing")); 
      WiFi.hostByName(CSTR(settingMQTTbroker), MQTTbrokerIP);  // lookup the MQTTbroker convert to IP
      snprintf_P(MQTTbrokerIPchar, sizeof(MQTTbrokerIPchar), PSTR("%d.%d.%d.%d"), MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
      if (isValidIP(MQTTbrokerIP))  
      {
        MQTTDebugTf(PSTR("[%s] => setServer(%s, %d)\r\n"), CSTR(settingMQTTbroker), MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.disconnect();
        MQTTclient.setServer(MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.setCallback(handleMQTTcallback);
        MQTTclient.setSocketTimeout(15);  // Increased from 4 to 15 seconds for better stability
        MQTTclient.setKeepAlive(60);      // Set to 60 seconds (default was 15) to reduce reconnections
        uint8_t mac[6]{0};
        WiFi.macAddress(mac);
        snprintf_P(MQTTclientId, sizeof(MQTTclientId), PSTR("%s%02X%02X%02X%02X%02X%02X"), _HOSTNAME, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        //skip try to connect
        reconnectAttempts =0;
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
      }
      else
      { // invalid IP, then goto error state
        MQTTDebugTf(PSTR("ERROR: [%s] => is not a valid URL\r\n"), CSTR(settingMQTTbroker));
        stateMQTT = MQTT_STATE_ERROR;
        //DebugTln(F("Next State: MQTT_STATE_ERROR"));
      }    
      RESTART_TIMER(timerMQTTwaitforconnect); 
    break;

    case MQTT_STATE_TRY_TO_CONNECT:
      MQTTDebugTln(F("MQTT State: MQTT try to connect"));
      MQTTDebugTf(PSTR("MQTT server is [%s], IP[%s]\r\n"), settingMQTTbroker, MQTTbrokerIPchar);
      
      MQTTDebugT(F("Attempting MQTT connection .. "));
      reconnectAttempts++;

      //If no username, then anonymous connection to broker, otherwise assume username/password.
      if (strlen(settingMQTTuser) == 0) 
      {
        MQTTDebug(F("without a Username/Password "));
        if(!MQTTclient.connect(MQTTclientId, MQTTPubNamespace, 0, true, "offline")) PrintMQTTError();
      } 
      else 
      {
        MQTTDebugf(PSTR("Username [%s] "), CSTR(settingMQTTuser));
        if(!MQTTclient.connect(MQTTclientId, CSTR(settingMQTTuser), CSTR(settingMQTTpasswd), MQTTPubNamespace, 0, true, "offline")) PrintMQTTError();
      }

      //If connection was made succesful, move on to next state...
      if (MQTTclient.connected())
      {
        reconnectAttempts = 0;  
        MQTTDebugln(F(" .. connected\r"));
        Debugln(F("MQTT connected"));	
        stateMQTT = MQTT_STATE_IS_CONNECTED;
        MQTTDebugTln(F("Next State: MQTT_STATE_IS_CONNECTED"));
        // birth message, sendMQTT retains  by default
        sendMQTT(MQTTPubNamespace, "online");

        // First do AutoConfiguration for Homeassistant
        doAutoConfigure();

        //Subscribe to topics
        char topic[100];
        strlcpy(topic, MQTTSubNamespace, sizeof(topic));
        strlcat(topic, "/#", sizeof(topic));
        MQTTDebugTf(PSTR("Subscribe to MQTT: TopicId [%s]\r\n"), topic);
        if (MQTTclient.subscribe(topic)){
          MQTTDebugTf(PSTR("MQTT: Subscribed successfully to TopicId [%s]\r\n"), topic);
        }
        else
        {
          MQTTDebugTf(PSTR("MQTT: Subscribe TopicId [%s] FAILED! \r\n"), topic);
          PrintMQTTError();
        }
        MQTTclient.subscribe("homeassistant/status"); //start monitoring the status of homeassistant, if it goes down, then force a restart after it comes back online.
        sendMQTTversioninfo();
      }
      else
      { // no connection, try again, do a non-blocking wait for 3 seconds.
        MQTTDebugln(F(" .. \r"));
        MQTTDebugTf(PSTR("failed, retrycount=[%d], rc=[%d] ..  try again in 3 seconds\r\n"), reconnectAttempts, MQTTclient.state());
        RESTART_TIMER(timerMQTTwaitforretry);
        stateMQTT = MQTT_STATE_WAIT_CONNECTION_ATTEMPT;  // if the re-connect did not work, then return to wait for reconnect
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_CONNECTION_ATTEMPT"));
      }
      
      //After 5 attempts... go wait for a while.
      if (reconnectAttempts >= 5)
      {
        MQTTDebugTln(F("5 attempts have failed. Retry wait for next reconnect in 10 minutes\r"));
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;  // if the re-connect did not work, then return to wait for reconnect
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }   
    break;
    
    case MQTT_STATE_IS_CONNECTED:
      if DUE(timerMQTTdebugisconnected) MQTTDebugTln(F("MQTT State: MQTT is Connected"));
      if (MQTTclient.connected()) 
      { //if the MQTT client is connected, then please do a .loop call...
        MQTTclient.loop();
      }
      else
      { //else go and wait 10 minutes, before trying again.
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }  
    break;

    case MQTT_STATE_WAIT_CONNECTION_ATTEMPT:
      //do non-blocking wait for 3 seconds
      if  DUE(timerMQTTdebugwaitconnectionattempt) MQTTDebugTln(F("MQTT State: MQTT_WAIT_CONNECTION_ATTEMPT"));
      if (DUE(timerMQTTwaitforretry))
      {
        //Try again... after waitforretry non-blocking delay
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;
    
    case MQTT_STATE_WAIT_FOR_RECONNECT:
      //do non-blocking wait for 10 minutes, then try to connect again. 
      if DUE(timerMQTTdebugwaitforreconnect) MQTTDebugTln(F("MQTT State: MQTT wait for reconnect"));
      if (DUE(timerMQTTwaitforconnect))
      {
        //remember when you tried last time to reconnect
        RESTART_TIMER(timerMQTTwaitforretry);
        reconnectAttempts = 0; 
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        MQTTDebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;

    case MQTT_STATE_ERROR:
      if DUE(timerMQTTdebugerrorstate) MQTTDebugTln(F("MQTT State: MQTT ERROR, wait for 10 minutes, before trying again"));
      //wait for next retry
      RESTART_TIMER(timerMQTTwaitforconnect);
      stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
      MQTTDebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
    break;

    default:
      MQTTDebugTln(F("MQTT State: default, this should NEVER happen!"));
      //do nothing, this state should not happen
      stateMQTT = MQTT_STATE_INIT;
      DebugTln(F("Next State: MQTT_STATE_INIT"));
    break;
  }
  statusMQTTconnection = MQTTclient.connected();
} // handleMQTT()

void PrintMQTTError(){
  MQTTDebugln();
  switch (MQTTclient.state())
  {
    case MQTT_CONNECTION_TIMEOUT     : MQTTDebugTln(F("Error: MQTT connection timeout"));break;
    case MQTT_CONNECTION_LOST        : MQTTDebugTln(F("Error: MQTT connections lost"));break;
    case MQTT_CONNECT_FAILED         : MQTTDebugTln(F("Error: MQTT connection failed"));break;
    case MQTT_DISCONNECTED           : MQTTDebugTln(F("Error: MQTT disconnected"));break;
    case MQTT_CONNECTED              : MQTTDebugTln(F("Error: MQTT connected"));break;
    case MQTT_CONNECT_BAD_PROTOCOL   : MQTTDebugTln(F("Error: MQTT connect bad protocol"));break;
    case MQTT_CONNECT_BAD_CLIENT_ID  : MQTTDebugTln(F("Error: MQTT connect bad client id"));break;
    case MQTT_CONNECT_UNAVAILABLE    : MQTTDebugTln(F("Error: MQTT connect unavailable"));break;
    case MQTT_CONNECT_BAD_CREDENTIALS: MQTTDebugTln(F("Error: MQTT connect bad credentials"));break;
    case MQTT_CONNECT_UNAUTHORIZED   : MQTTDebugTln(F("Error: MQTT connect unauthorized"));break;
    default: MQTTDebugTln(F("Error: MQTT unknown error"));
  }
}

/* 
  topic:  <string> , sensor topic, will be automatically prefixed with <mqtt topic>/value/<node_id>
  json:   <string> , payload to send
  retain: <bool> , retain mqtt message  
*/
void sendMQTTData(const char* topic, const char *json, const bool retain) 
{
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  
  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return;
  }
  
  char full_topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topic, sizeof(full_topic));
  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n"), settingMQTTbroker, settingMQTTbrokerPort, full_topic, json);
  if (!MQTTclient.publish(full_topic, json, retain)) PrintMQTTError();
  feedWatchDog();//feed the dog
} // sendMQTTData()

void sendMQTTData(const __FlashStringHelper *topic, const char *json, const bool retain)
{
  char topicBuf[MQTT_TOPIC_MAX_LEN];
  strncpy_P(topicBuf, reinterpret_cast<PGM_P>(topic), sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';
  sendMQTTData(topicBuf, json, retain);
}

void sendMQTTData(const __FlashStringHelper *topic, const __FlashStringHelper *json, const bool retain)
{
  static char payloadBuf[MQTT_MSG_MAX_LEN];
  strncpy_P(payloadBuf, reinterpret_cast<PGM_P>(json), sizeof(payloadBuf) - 1);
  payloadBuf[sizeof(payloadBuf) - 1] = '\0';
  sendMQTTData(topic, payloadBuf, retain);
}

/* 
* topic:  <string> , topic will be used as is (no prefixing), retained = true
* json:   <string> , payload to send
*/
//===========================================================================================
// Streaming version - sends message in 128-byte chunks to avoid buffer reallocation
// This prevents heap fragmentation on ESP8266 (similar to ESPHome's approach)
void sendMQTT(const char* topic, const char *json) {
  sendMQTTStreaming(topic, json, strlen(json));
}

void sendMQTTStreaming(const char* topic, const char *json, const size_t len) 
{
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  
  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return;
  }
  
  MQTTDebugTf(PSTR("Sending MQTT (streaming): server %s:%d => TopicId [%s] (len=%d bytes)\r\n"), 
              settingMQTTbroker, settingMQTTbrokerPort, topic, len);

  // Use beginPublish which tells PubSubClient the total length upfront
  // This allows it to use its buffer efficiently without reallocation
  if (!MQTTclient.beginPublish(topic, len, true)) {
    PrintMQTTError();
    return;
  }

  // Write message in small chunks to avoid buffer overflow
  // PubSubClient's write() method handles buffering internally
  const size_t CHUNK_SIZE = 128; // Small chunks fit comfortably in 256-byte buffer
  size_t pos = 0;
  
  while (pos < len) {
    size_t chunkLen = (len - pos) > CHUNK_SIZE ? CHUNK_SIZE : (len - pos);
    
    // Write chunk
    for (size_t i = 0; i < chunkLen; i++) {
      if (!MQTTclient.write(json[pos + i])) {
        PrintMQTTError();
        MQTTclient.endPublish(); // Clean up even on error
        return;
      }
    }
    
    pos += chunkLen;
    feedWatchDog(); // Feed watchdog during long write operations
  }
  
  if (!MQTTclient.endPublish()) {
    PrintMQTTError();
  }

  feedWatchDog();
} // sendMQTTStreaming()

//===========================================================================================
// Helper functions to reduce duplicated MQTT topic building patterns
//===========================================================================================

/**
 * Publish ON/OFF value to MQTT topic
 * Reduces duplicate pattern of boolean-to-string conversion
 */
void publishMQTTOnOff(const char* topic, bool value) {
  sendMQTTData(topic, value ? "ON" : "OFF");
}

void publishMQTTOnOff(const __FlashStringHelper* topic, bool value) {
  sendMQTTData(topic, value ? "ON" : "OFF");
}

/**
 * Publish numeric value as string to MQTT topic
 * Reduces duplicate pattern of number-to-string conversion with static buffer
 */
void publishMQTTNumeric(const char* topic, float value, uint8_t decimals = 2) {
  static char buffer[16];
  dtostrf(value, 1, decimals, buffer);
  sendMQTTData(topic, buffer);
}

void publishMQTTNumeric(const __FlashStringHelper* topic, float value, uint8_t decimals = 2) {
  static char buffer[16];
  dtostrf(value, 1, decimals, buffer);
  sendMQTTData(topic, buffer);
}

/**
 * Publish integer value as string to MQTT topic
 */
void publishMQTTInt(const char* topic, int value) {
  static char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d", value);
  sendMQTTData(topic, buffer);
}

void publishMQTTInt(const __FlashStringHelper* topic, int value) {
  static char buffer[12];
  snprintf(buffer, sizeof(buffer), "%d", value);
  sendMQTTData(topic, buffer);
}

void publishToSourceTopic(const char* topic, const char* json, const bool retain)
{
  if (!settingMQTTSeparateSources || !topic || !json) return;

  // OTdata.rsptype is populated by processOT() before print_*() calls this helper.
  char sourceTopic[MQTT_TOPIC_MAX_LEN];

  switch (OTdata.rsptype) {
    case OTGW_THERMOSTAT:
      snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_thermostat"), topic);
      break;
    case OTGW_BOILER:
      snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_boiler"), topic);
      break;
    case OTGW_REQUEST_BOILER:
    case OTGW_ANSWER_THERMOSTAT:
      snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_gateway"), topic);
      break;
    default:
      return;
  }

  sendMQTTData(sourceTopic, json, retain);
}

//===========================================================================================
// resetMQTTBufferSize() - Static Buffer Strategy
//
// INTENTIONALLY A NO-OP to maintain static 1350-byte MQTT buffer allocation.
//
// Historical context:
// - Originally shrunk buffer to 256 bytes after auto-discovery to "save RAM"
// - This caused heap fragmentation through repeated realloc() calls on ESP8266
// - Bot reviewer identified this as defeating the static buffer fix
//
// Current strategy:
// - Buffer allocated once at startup (1350 bytes)
// - NEVER resized during runtime = zero fragmentation
// - MQTT streaming handles all message sizes efficiently
// - Function kept as no-op for API compatibility with existing code
//
// Memory impact: 1350 bytes permanently allocated (acceptable trade-off for stability)
//===========================================================================================
void resetMQTTBufferSize()
{
  if (!settingMQTTenable) return;
  // Intentionally empty - maintains static buffer, prevents heap fragmentation
  // See function comment above for rationale
}
//===========================================================================================
bool getMQTTConfigDone(const uint8_t MSGid)
{
  uint8_t group = MSGid & 0b11100000;
  group = group>>5;
  uint8_t index = MSGid & 0b00011111;
  uint32_t result = bitRead(MQTTautoConfigMap[group], index);
  MQTTDebugTf(PSTR("Reading bit %d from group %d for MSGid %d: result = %d\r\n"), index, group, MSGid, result);
  if (result > 0) {
    return true;
  } else {
    return false;
  }
}
//===========================================================================================
void setMQTTConfigDone(const uint8_t MSGid)
{
  uint8_t group = MSGid & 0b11100000;
  group = group>>5;
  uint8_t index = MSGid & 0b00011111;
  MQTTDebugTf(PSTR("Setting bit %d from group %d for MSGid %d\r\n"), index, group, MSGid);
  MQTTDebugTf(PSTR("Value before setting bit %d\r\n"), MQTTautoConfigMap[group]);
  bitSet(MQTTautoConfigMap[group], index);
  MQTTDebugTf(PSTR("Value after setting bit  %d\r\n"), MQTTautoConfigMap[group]);
}
//===========================================================================================
void clearMQTTConfigDone()
{
  memset(MQTTautoConfigMap, 0, sizeof(MQTTautoConfigMap));
}
//===========================================================================================
void doAutoConfigure(bool bForceAll){
  // Use shared static buffers (zero heap allocation, single RAM reservation).
  // Lock scope is limited to the file-parse/publish loop so it is released
  // before configSensors() is called.  configSensors() -> sensorAutoConfigure()
  // -> doAutoConfigureMsgid() can then acquire the lock independently.

  if (!settingMQTTenable) return;

  {
    // Lock released at end of this block, before configSensors() is called.
    MQTTAutoConfigSessionLock sessionLock;
    if (!sessionLock.locked) {
      MQTTDebugTln(F("MQTT autoconfig already running, skipping doAutoConfigure()"));
      return;
    }

    char *sLine = mqttAutoConfigBuffers.line;
    char *sTopic = mqttAutoConfigBuffers.topic;
    char *sMsg = mqttAutoConfigBuffers.msg;

    // 2. Open File ONCE
    LittleFS.begin();
    if (!LittleFS.exists(F("/mqttha.cfg"))) {
      DebugTln(F("Error: configuration file not found.")); 
      return;
    } 

    File fh = LittleFS.open(F("/mqttha.cfg"), "r");
    if (!fh) {
      DebugTln(F("Error: could not open configuration file.")); 
      return;
    } 

    byte lineID = 0;
    
    // 3. Loop through file once
    while (fh.available()) {
      feedWatchDog(); // Keep the dog happy during IO
      
      // Read line
      size_t len = fh.readBytesUntil('\n', sLine, MQTT_CFG_LINE_MAX_LEN - 1);
      sLine[len] = '\0';
      
      // Parse Line
      if (!splitLine(sLine, ';', lineID, sTopic, MQTT_TOPIC_MAX_LEN, sMsg, MQTT_MSG_MAX_LEN)) continue;

      // 4. Decision: Do we send this line?
      // Skip Dallas sensors - they need per-sensor addresses from configSensors()
      if (lineID == OTGWdallasdataid) continue;

      if (bForceAll || !getMQTTConfigDone(lineID)) {

         MQTTDebugTf(PSTR("Processing AutoConfig for ID %d\r\n"), lineID);

         // --- Perform Replacements (Topic & Msg) ---
         if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%homeassistant%", CSTR(settingMQTThaprefix))) continue;
         if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%node_id%", NodeId)) continue;
         if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%sensor_id%", "")) continue;

         if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%node_id%", NodeId)) continue;
         if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%sensor_id%", "")) continue;
         if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%hostname%", CSTR(settingHostname))) continue;
         if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%version%", _VERSION)) continue;
         if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%mqtt_pub_topic%", MQTTPubNamespace)) continue;
         if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%mqtt_sub_topic%", MQTTSubNamespace)) continue;

         // Static buffer strategy: use MQTT streaming with fixed 1350-byte buffer
         // No dynamic resizing - streaming handles arbitrarily large messages efficiently
         size_t msgLen = strlen(sMsg);

         // Send retained message (uses beginPublish/write/endPublish streaming)
         sendMQTTStreaming(sTopic, sMsg, msgLen);
         setMQTTConfigDone(lineID);

         doBackgroundTasks(); // Yield to network stack
      }
    }

    fh.close();
    
    // Note: No buffer cleanup needed - static buffers persist across calls
    // resetMQTTBufferSize() is a no-op for static buffer strategy
    resetMQTTBufferSize();
  } // Lock released here - configSensors() can now acquire it independently

  // Trigger Dallas configuration separately as it requires specific sensor addresses
  if (settingMQTTenable && (bForceAll || !getMQTTConfigDone(OTGWdallasdataid))) {
    configSensors();
  }
}
//===========================================================================================
bool doAutoConfigureMsgid(byte OTid)
{ 
  // check if foney dataid is called to do autoconfigure for temp sensors, call configsensors instead 
  if (OTid == OTGWdallasdataid) {
    MQTTDebugTf(PSTR("Sending auto configuration for temp sensors %d\r\n"), OTid);
    configSensors() ;
    return true;
  }  
  return doAutoConfigureMsgid(OTid, nullptr);
}

bool doAutoConfigureMsgid(byte OTid, const char *cfgSensorId )
{
  bool _result = false;
  const char *sensorId = (cfgSensorId != nullptr) ? cfgSensorId : "";

  MQTTAutoConfigSessionLock sessionLock;
  if (!sessionLock.locked) {
    MQTTDebugTln(F("MQTT autoconfig already running, skipping doAutoConfigureMsgid()"));
    return _result;
  }
  if (!settingMQTTenable) {
    return _result;
  }
  if (!MQTTclient.connected()) {
    DebugTln(F("Error: MQTT broker not connected.")); 
    return _result;
  } 
  if (!isValidIP(MQTTbrokerIP)) {
    DebugTln(F("Error: MQTT broker IP not valid.")); 
    return _result;
  } 

  // Shared static buffers with doAutoConfigure() to avoid duplicate persistent RAM usage
  char *sLine = mqttAutoConfigBuffers.line;
  char *sTopic = mqttAutoConfigBuffers.topic;
  char *sMsg = mqttAutoConfigBuffers.msg;
  byte lineID = 39; // 39 is unused in OT protocol so is a safe value

  //Let's open the MQTT autoconfig file
  File fh; //filehandle
  // const char *cfgFilename = "/mqttha.cfg"; // moved to usage
  LittleFS.begin();

  if (!LittleFS.exists(F("/mqttha.cfg"))) {
    DebugTln(F("Error: configuration file not found.")); 
    return _result;
  } 

  fh = LittleFS.open(F("/mqttha.cfg"), "r");

  if (!fh) {
    DebugTln(F("Error: could not open configuration file.")); 
    return _result;
  } 

  //Lets go read the config and send it out to MQTT line by line
  while (fh.available())
  {
    // Explicitly clear reused shared buffers each iteration for robustness.
    memset(sTopic, 0, MQTT_TOPIC_MAX_LEN);
    memset(sMsg, 0, MQTT_MSG_MAX_LEN);

    //read file line by line, split and send to MQTT (topic, msg)
    feedWatchDog(); //start with feeding the dog
    
    size_t len = fh.readBytesUntil('\n', sLine, MQTT_CFG_LINE_MAX_LEN - 1);
    sLine[len] = '\0';
    if (!splitLine(sLine, ';', lineID, sTopic, MQTT_TOPIC_MAX_LEN, sMsg, MQTT_MSG_MAX_LEN)) {  //splitLine() also filters comments
      continue;
    }

    // check if this is the specific line we are looking for
    // Old config dump method (dumping all lines) is no longer used - we now fetch specific lines by ID
    if (lineID != OTid) continue;

    MQTTDebugTf(PSTR("Found line in config file for %d: [%d][%s] \r\n"), OTid, lineID, sTopic);

    // discovery topic prefix
    MQTTDebugTf(PSTR("sTopic[%s]==>"), sTopic); 
    if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%homeassistant%", CSTR(settingMQTThaprefix))) { MQTTDebugTln(F("MQTT: topic replacement overflow")); continue; }

    /// node
    if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%node_id%", NodeId)) { MQTTDebugTln(F("MQTT: node_id replacement overflow")); continue; }

    /// SensorId
    if (!replaceAll(sTopic, MQTT_TOPIC_MAX_LEN, "%sensor_id%", sensorId)) { MQTTDebugTln(F("MQTT: sensor_id replacement overflow")); continue; }

    MQTTDebugf(PSTR("[%s]\r\n"), sTopic); 
    /// ----------------------

    MQTTDebugTf(PSTR("sMsg[%s]==>"), sMsg); 

    /// node
    if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%node_id%", NodeId)) { MQTTDebugTln(F("MQTT: node_id replacement overflow")); continue; }

    /// SensorId
    if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%sensor_id%", sensorId)) { MQTTDebugTln(F("MQTT: sensor_id replacement overflow")); continue; }

    /// hostname
    if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%hostname%", CSTR(settingHostname))) { MQTTDebugTln(F("MQTT: hostname replacement overflow")); continue; }

    /// version
    if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%version%", _VERSION)) { MQTTDebugTln(F("MQTT: version replacement overflow")); continue; }

    // pub topics prefix
    if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%mqtt_pub_topic%", MQTTPubNamespace)) { MQTTDebugTln(F("MQTT: mqtt_pub_topic replacement overflow")); continue; }

    // sub topics
    if (!replaceAll(sMsg, MQTT_MSG_MAX_LEN, "%mqtt_sub_topic%", MQTTSubNamespace)) { MQTTDebugTln(F("MQTT: mqtt_sub_topic replacement overflow")); continue; }

    MQTTDebugf(PSTR("[%s]\r\n"), sMsg); 
    DebugFlush();
    
    // Static buffer strategy: use MQTT streaming with fixed 1350-byte buffer
    // No dynamic resizing - streaming handles arbitrarily large messages efficiently
    sendMQTTStreaming(sTopic, sMsg, strlen(sMsg));
    resetMQTTBufferSize();  // No-op, kept for API compatibility
    _result = true;

    // TODO: enable this break if we are sure the old config dump method is no longer needed
    // break;

  } // while available()
  
  fh.close();
  
  // Note: No buffer cleanup needed - static buffer persists across calls
  // resetMQTTBufferSize() is a no-op for static buffer strategy
  resetMQTTBufferSize();

  return _result;

}

void sensorAutoConfigure(byte dataid, bool finishflag , const char *cfgSensorId = nullptr) {
 // Special version of Autoconfigure for sensors
 // dataid is a foney id, not used by OT 
 // check wheter MQTT topic needs to be configured
 // cfgNodeId can be set to alternate NodeId to allow for multiple temperature sensors, should normally be NodeId
 // When finishflag is true, check on dataid is already done and complete the config.  On false do the config and leave completion to caller
 if(getMQTTConfigDone(dataid)==false or !finishflag) {
   MQTTDebugTf(PSTR("Need to set MQTT config for sensor id(%d)\r\n"),dataid);
   bool success = doAutoConfigureMsgid(dataid,cfgSensorId);
   if(success) {
     MQTTDebugTf(PSTR("Successfully sent MQTT config for sensor id(%d)\r\n"),dataid);
     if (finishflag) setMQTTConfigDone(dataid);
     } else {
       MQTTDebugTf(PSTR("Not able to complete MQTT configuration for sensor id(%d)\r\n"),dataid);
     }
   } else {
   // MQTTDebugTf(PSTR("No need to set MQTT config for sensor id(%d)\r\n"),dataid);
   }
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
