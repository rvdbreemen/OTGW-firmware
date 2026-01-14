/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v1.0.0-rc4
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

// Enable MQTT streaming mode for auto-discovery to reduce heap fragmentation
// When enabled, large auto-discovery messages are sent in chunks instead of
// requiring a large buffer resize. This prevents the 256→1200→256 buffer
// resize cycle that causes heap fragmentation.
// #define USE_MQTT_STREAMING_AUTODISCOVERY

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
struct MQTT_set_cmd_t
{
    const char* setcmd;
    const char* otgwcmd;
    const char* ottype;
};


const MQTT_set_cmd_t setcmds[] {
  {   "command", "", "raw" },
  {   "setpoint", "TT", "temp" },
  {   "constant", "TC", "temp" },
  {   "outside", "OT", "temp" },
  {   "hotwater", "HW", "on" },
  {   "gatewaymode", "GW", "on" },
  {   "setback", "SB", "temp" },
  {   "maxchsetpt", "SH", "temp" },
  {   "maxdhwsetpt", "SW", "temp" },
  {   "maxmodulation", "MM", "level" },        
  {   "ctrlsetpt", "CS", "temp" },        
  {   "ctrlsetpt2", "C2", "temp" },        
  {   "chenable", "CH", "on" },        
  {   "chenable2", "H2", "on" },        
  {   "ventsetpt", "VS", "level" },
  {   "temperaturesensor", "TS", "function" },
  {   "addalternative", "AA", "function" },
  {   "delalternative", "DA", "function" },
  {   "unknownid", "UI", "function" },
  {   "knownid", "KI", "function" },
  {   "priomsg", "PM", "function" },
  {   "setresponse", "SR", "function" },
  {   "clearrespons", "CR", "function" },
  {   "resetcounter", "RS", "function" },
  {   "ignoretransitations", "IT", "function" },
  {   "overridehb", "OH", "function" },
  {   "forcethermostat", "FT", "function" },
  {   "voltageref", "VR", "function" },
  {   "debugptr", "DP", "function" },
} ;

const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);

// const char learnmsg[] { "LA", "PR=L", "LB", "PR=L", "LC", "PR=L", "LD", "PR=L", "LE", "PR=L", "LF", "PR=L", "GA", "PR=G", "GB", "PR=G", "VR", "PR=V", "GW", "PR=M", "IT", "PR=T", "SB", "PR=S", "HW", "PR=W" } ;
// const int nrlearnmsg = sizeof(learnmsg) / sizeof(learnmsg[0]);

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
  if (strcasecmp(topic, "homeassistant/status") == 0) {
    //incoming message on status, detect going down
    if (!settingMQTTharebootdetection) {
      //So if the HA reboot detection is turned of, we will just look for HA going online.
      //This means everytime there is "online" message, we will restart MQTT configuration, including the HA Auto Discovery. 
      bHAcycle = true; 
    }
    if (strcasecmp(msgPayload, "offline") == 0){
      //home assistant went down
      DebugTln(F("Home Assistant went offline!"));
      bHAcycle = true; //set flag, so it triggers when it goes back online
    } else if ((strcasecmp(msgPayload, "online") == 0) && bHAcycle){
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
  MQTTDebugf(PSTR("%s/"), token);
  if (strcasecmp(token, "set") == 0) {
    token = strtok(NULL, "/");
    MQTTDebugf(PSTR("%s/"), token); 
    if (strcasecmp(token, NodeId) == 0) {
      token = strtok(NULL, "/");
      MQTTDebugf(PSTR("%s"), token);
      if (token != NULL){
        //loop thru command list
        int i;
        for (i=0; i<nrcmds; i++){
          if (strcasecmp(token, setcmds[i].setcmd) == 0){
            //found a match
            if (strcasecmp(setcmds[i].ottype, "raw") == 0){
              //raw command
              snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s"), msgPayload);
              MQTTDebugf(PSTR(" found command, sending payload [%s]\r\n"), otgwcmd);
              addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
            } else {
              //all other commands are <otgwcmd>=<payload message> 
              snprintf_P(otgwcmd, sizeof(otgwcmd), PSTR("%s=%s"), setcmds[i].otgwcmd, msgPayload);
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
// Sends the message in small chunks to avoid buffer reallocation
// This prevents the 256→1200→256 buffer resize cycle that causes heap fragmentation
//===========================================================================================
#ifdef USE_MQTT_STREAMING_AUTODISCOVERY

void sendMQTT(const char* topic, const char *json);

// Forward declaration; implementation is provided later in this file
void sendMQTTStreaming(const char* topic, const char *json, const size_t len);
#else

//===========================================================================================
// sendMQTT - original version (uses full buffer)
//===========================================================================================
void sendMQTT(const char* topic, const char *json) {
  sendMQTT(topic, json, strlen(json));
}
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
#ifdef USE_MQTT_STREAMING_AUTODISCOVERY

// Streaming version - sends message in chunks to avoid buffer reallocation
// This prevents the 256→1200→256 buffer resize cycle that causes heap fragmentation
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

#else

// Original version - uses full buffer (may cause buffer reallocation)
void sendMQTT(const char* topic, const char *json) {
  sendMQTT(topic, json, strlen(json));
}

void sendMQTT(const char* topic, const char *json, const size_t len) 
{
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  
  // Check heap health before publishing
  if (!canPublishMQTT()) {
    // Message dropped due to low heap - canPublishMQTT() handles logging
    return;
  }
  
  MQTTDebugTf(PSTR("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n"), settingMQTTbroker, settingMQTTbrokerPort, topic, json);

  // Stream the message byte-by-byte to minimize buffer usage
  if (MQTTclient.beginPublish(topic, len, true)){
    for (size_t i = 0; i<len; i++) {
      if(!MQTTclient.write(json[i])) PrintMQTTError();
    }  
    MQTTclient.endPublish();
  } else PrintMQTTError();

  feedWatchDog();
} // sendMQTT()

#endif


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
bool setMQTTConfigDone(const uint8_t MSGid)
{
  uint8_t group = MSGid & 0b11100000;
  group = group>>5;
  uint8_t index = MSGid & 0b00011111;
  MQTTDebugTf(PSTR("Setting bit %d from group %d for MSGid %d\r\n"), index, group, MSGid);
  MQTTDebugTf(PSTR("Value before setting bit %d\r\n"), MQTTautoConfigMap[group]);
  if(bitSet(MQTTautoConfigMap[group], index) > 0) {
    MQTTDebugTf(PSTR("Value after setting bit  %d\r\n"), MQTTautoConfigMap[group]);
    return true;
  } else {
    return false;
  }
}
//===========================================================================================
void clearMQTTConfigDone()
{
  memset(MQTTautoConfigMap, 0, sizeof(MQTTautoConfigMap));
}
//===========================================================================================
void doAutoConfigure(bool bForceAll){
  // Refactored for Single-Pass Efficiency and Dynamic Buffer Resizing
  
  if (!settingMQTTenable) return;

  // 1. Allocate buffers on HEAP to save STATIC RAM during normal operation
  char* sLine = new char[MQTT_CFG_LINE_MAX_LEN];
  char* sTopic = new char[MQTT_TOPIC_MAX_LEN];
  char* sMsg = new char[MQTT_MSG_MAX_LEN];
  
  // Check allocation success
  if (!sLine || !sTopic || !sMsg) {
     DebugTln(F("Error: Out of memory for AutoConfigure"));
     if(sLine) delete[] sLine; 
     if(sTopic) delete[] sTopic; 
     if(sMsg) delete[] sMsg;
     return;
  }

  // 2. Open File ONCE
  LittleFS.begin();
  if (!LittleFS.exists(F("/mqttha.cfg"))) {
    DebugTln(F("Error: configuration file not found.")); 
    delete[] sLine; delete[] sTopic; delete[] sMsg;
    return;
  } 

  File fh = LittleFS.open(F("/mqttha.cfg"), "r");
  if (!fh) {
    DebugTln(F("Error: could not open configuration file.")); 
    delete[] sLine; delete[] sTopic; delete[] sMsg;
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
    if (bForceAll || getMQTTConfigDone(lineID) || (lineID == OTGWdallasdataid)) {
       
       bool isDallas = (lineID == OTGWdallasdataid);
       
       // Special handling: Dallas sensors are managed by configSensors() which calls the worker function.
       // We can skip them here to avoid duplication or complex logic, or handle them if desired.
       // Current implementation delegates Dallas to configSensors(), so for bulk update, we should check if we need to trigger that.
       if (isDallas) {
          // For Dallas, we can't easily bulk-process because we need the sensor address which isn't in the config file
          // So we skip it here and handle it strictly via the worker logic which configSensors calls.
          continue; 
       }

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
       sendMQTT(sTopic, sMsg, msgLen);
       
       doBackgroundTasks(); // Yield to network stack
    }
  }

  fh.close();
  
  // Cleanup - note: resetMQTTBufferSize() is now a no-op for static buffer strategy
  resetMQTTBufferSize();
  delete[] buffer;  // Delete the single allocated buffer, not the partitions
  
  // Trigger Dallas configuration separately as it requires specific sensor addresses
  if (settingMQTTenable && (bForceAll || getMQTTConfigDone(OTGWdallasdataid))) {
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

  // Allocate buffers dynamically to save stack/static RAM
  const size_t totalBufferSize = MQTT_MSG_MAX_LEN + MQTT_TOPIC_MAX_LEN + MQTT_CFG_LINE_MAX_LEN;
  char* buffer = new char[totalBufferSize];
  char* sMsg   = nullptr;
  char* sTopic = nullptr;
  char* sLine  = nullptr;

  if (!buffer) {
    DebugTln(F("Error: Out of memory in doAutoConfigureMsgid"));
    return _result;
  }

  // Partition the single buffer into separate logical regions
  sMsg   = buffer;
  sTopic = sMsg + MQTT_MSG_MAX_LEN;
  sLine  = sTopic + MQTT_TOPIC_MAX_LEN;
  byte lineID = 39; // 39 is unused in OT protocol so is a safe value

  //Let's open the MQTT autoconfig file
  File fh; //filehandle
  // const char *cfgFilename = "/mqttha.cfg"; // moved to usage
  LittleFS.begin();

  if (!LittleFS.exists(F("/mqttha.cfg"))) {
    DebugTln(F("Error: configuration file not found.")); 
    delete[] sMsg; delete[] sTopic; delete[] sLine;
    return _result;
  } 

  fh = LittleFS.open(F("/mqttha.cfg"), "r");

  if (!fh) {
    DebugTln(F("Error: could not open configuration file.")); 
    delete[] sMsg; delete[] sTopic; delete[] sLine;
    return _result;
  } 

  //Lets go read the config and send it out to MQTT line by line
  while (fh.available())
  {
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
    sendMQTT(sTopic, sMsg, strlen(sMsg));
    resetMQTTBufferSize();  // No-op, kept for API compatibility
    _result = true;

    // TODO: enable this break if we are sure the old config dump method is no longer needed
    // break;

  } // while available()
  
  fh.close();
  delete[] buffer;  // Fix: delete the single allocated buffer, not the partitions

  // HA discovery msg's are rather large, reset the buffer size to release some memory
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
