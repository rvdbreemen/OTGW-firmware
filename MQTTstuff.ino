/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v0.9.2
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**      Modified version from (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <PubSubClient.h>           // MQTT client publish and subscribe functionality

#define MQTTDebugTln(...) ({ if (bDebugMQTT) DebugTln(__VA_ARGS__);    })
#define MQTTDebugln(...)  ({ if (bDebugMQTT) Debugln(__VA_ARGS__);    })
#define MQTTDebugTf(...)  ({ if (bDebugMQTT) DebugTf(__VA_ARGS__);    })
#define MQTTDebugf(...)   ({ if (bDebugMQTT) Debugf(__VA_ARGS__);    })
#define MQTTDebugT(...)   ({ if (bDebugMQTT) DebugT(__VA_ARGS__);    })
#define MQTTDebug(...)    ({ if (bDebugMQTT) Debug(__VA_ARGS__);    })

// Declare some variables within global scope

static IPAddress  MQTTbrokerIP;
static char       MQTTbrokerIPchar[20];

static            PubSubClient MQTTclient(wifiClient);

int8_t            reconnectAttempts = 0;
char              lastMQTTtimestamp[15] = "";

enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_WAIT_CONNECTION_ATTEMPT, MQTT_STATE_WAIT_FOR_RECONNECT, MQTT_STATE_ERROR };
enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

String            MQTTclientId;
String            MQTTPubNamespace = "";
String            MQTTSubNamespace = "";
String            NodeId = "";

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
  stateMQTT = MQTT_STATE_INIT;
  //setup for mqtt discovery
  clearMQTTConfigDone();
  NodeId = settingMQTTuniqueid;
  MQTTPubNamespace = settingMQTTtopTopic + "/value/" + NodeId;
  MQTTSubNamespace = settingMQTTtopTopic + "/set/" + NodeId;
  handleMQTT(); //initialize the MQTT statemachine
  // handleMQTT(); //then try to connect to MQTT
  // handleMQTT(); //now you should be connected to MQTT ready to send
}

bool bHAcycle = false;

// handles MQTT subscribe incoming stuff
void handleMQTTcallback(char* topic, byte* payload, unsigned int length) {

  if (bDebugMQTT) {
    DebugT("Message arrived on topic ["); Debug(topic); Debug("] = [");
    for (unsigned int i = 0; i < length; i++) {
      Debug((char)payload[i]);
    }
    Debug("] ("); Debug(length); Debug(")"); Debugln(); DebugFlush();
  }  

  //detect home assistant going down...
  char msgPayload[50];
  int msglen = min((int)(length)+1, (int)sizeof(msgPayload));
  strlcpy(msgPayload, (char *)payload, msglen);
  if (strcasecmp(topic, "homeassistant/status") == 0) {
    //incoming message on status, detect going down
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
      DebugTf("Home Assistant Status=[%s] and HA cycle status [%s]\r\n", msgPayload, CBOOLEAN(bHAcycle)); 
    }
  }


  // parse the incoming topic and execute commands
  char* token;
  char otgwcmd[20]={0};

  //first check toptopic part, it can include the seperator, e.g. "myHome/OTGW" or "OTGW""
  if (strncmp(topic, settingMQTTtopTopic.c_str(), settingMQTTtopTopic.length()) != 0) {
    MQTTDebugln(F("MQTT: wrong top topic"));
    return;
  } else {
    //remove the top topic part
    MQTTDebugTf("Parsing topic: %s/", settingMQTTtopTopic.c_str());
    topic += settingMQTTtopTopic.length();
    if (*topic == '/') topic++;
  }
  // naming convention /set/<node id>/<command>
  token = strtok(topic, "/"); 
  MQTTDebugf("%s/", token);
  if (strcasecmp(token, "set") == 0) {
    token = strtok(NULL, "/");
    MQTTDebugf("%s/", token); 
    if (strcasecmp(token, CSTR(NodeId)) == 0) {
      token = strtok(NULL, "/");
      MQTTDebugf("%s", token);
      if (token != NULL){
        //loop thru command list
        int i;
        for (i=0; i<nrcmds; i++){
          if (strcasecmp(token, setcmds[i].setcmd) == 0){
            //found a match
            if (strcasecmp(setcmds[i].ottype, "raw") == 0){
              //raw command
              snprintf(otgwcmd, sizeof(otgwcmd), "%s", msgPayload);
              MQTTDebugf(" found command, sending payload [%s]\r\n", otgwcmd);
              addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
            } else {
              //all other commands are <otgwcmd>=<payload message> 
              snprintf(otgwcmd, sizeof(otgwcmd), "%s=%s", setcmds[i].otgwcmd, msgPayload);
              MQTTDebugf(" found command, sending payload [%s]\r\n", otgwcmd);
              addOTWGcmdtoqueue((char *)otgwcmd, strlen(otgwcmd), true);
            }
            break; //exit loop
          } 
        }
        if (i >= nrcmds){
          //no match found
          MQTTDebugln();
          MQTTDebugTf("No match found for command: [%s]\r\n", token);
        }
      }
    }
  }
}

//===========================================================================================
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
      sprintf(MQTTbrokerIPchar, "%d.%d.%d.%d", MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
      if (isValidIP(MQTTbrokerIP))  
      {
        MQTTDebugTf("[%s] => setServer(%s, %d)\r\n", CSTR(settingMQTTbroker), MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.disconnect();
        MQTTclient.setServer(MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.setCallback(handleMQTTcallback);
        MQTTclient.setSocketTimeout(4); 
        MQTTclientId  = String(_HOSTNAME) + WiFi.macAddress();
        //skip try to connect
        reconnectAttempts =0;
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
      }
      else
      { // invalid IP, then goto error state
        MQTTDebugTf("ERROR: [%s] => is not a valid URL\r\n", CSTR(settingMQTTbroker));
        stateMQTT = MQTT_STATE_ERROR;
        //DebugTln(F("Next State: MQTT_STATE_ERROR"));
      }    
      RESTART_TIMER(timerMQTTwaitforconnect); 
    break;

    case MQTT_STATE_TRY_TO_CONNECT:
      MQTTDebugTln(F("MQTT State: MQTT try to connect"));
      MQTTDebugTf("MQTT server is [%s], IP[%s]\r\n", settingMQTTbroker.c_str(), MQTTbrokerIPchar);
      
      MQTTDebugT(F("Attempting MQTT connection .. "));
      reconnectAttempts++;

      //If no username, then anonymous connection to broker, otherwise assume username/password.
      if (settingMQTTuser.length() == 0) 
      {
        MQTTDebug(F("without a Username/Password "));
        if(!MQTTclient.connect(CSTR(MQTTclientId), CSTR(MQTTPubNamespace), 0, true, "offline")) PrintMQTTError();
      } 
      else 
      {
        MQTTDebugf("Username [%s] ", CSTR(settingMQTTuser));
        if(!MQTTclient.connect(CSTR(MQTTclientId), CSTR(settingMQTTuser), CSTR(settingMQTTpasswd), CSTR(MQTTPubNamespace), 0, true, "offline")) PrintMQTTError();
      }

      //If connection was made succesful, move on to next state...
      if (MQTTclient.connected())
      {
        reconnectAttempts = 0;  
        Debugln(F(" .. connected\r"));
        stateMQTT = MQTT_STATE_IS_CONNECTED;
        MQTTDebugTln(F("Next State: MQTT_STATE_IS_CONNECTED"));
        // birth message, sendMQTT retains  by default
        sendMQTT(CSTR(MQTTPubNamespace), "online");

        // First do AutoConfiguration for Homeassistant
        // doAutoConfigure();

        //Subscribe to topics
        char topic[100];
        strcpy(topic, CSTR(MQTTSubNamespace));
        strlcat(topic, "/#", sizeof(topic));
        MQTTDebugTf("Subscribe to MQTT: TopicId [%s]\r\n", topic);
        if (MQTTclient.subscribe(topic)){
          MQTTDebugTf("MQTT: Subscribed successfully to TopicId [%s]\r\n", topic);
        }
        else
        {
          MQTTDebugTf("MQTT: Subscribe TopicId [%s] FAILED! \r\n", topic);
          PrintMQTTError();
        }
        MQTTclient.subscribe("homeassistant/status"); //start monitoring the status of homeassistant, if it goes down, then force a restart after it comes back online.
        sendMQTTversioninfo();
      }
      else
      { // no connection, try again, do a non-blocking wait for 3 seconds.
        MQTTDebugln(F(" .. \r"));
        MQTTDebugTf("failed, retrycount=[%d], rc=[%d] ..  try again in 3 seconds\r\n", reconnectAttempts, MQTTclient.state());
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

//===========================================================================================
String trimVal(char *in) 
{
  String Out = in;
  Out.trim();
  return Out;
} // trimVal()

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
void sendMQTTData(const String topic, const String json, const bool retain = false)
{
  if (!settingMQTTenable) return;
  sendMQTTData(CSTR(topic), CSTR(json), retain);
}

/* 
  topic:  <string> , sensor topic, will be automatically prefixed with <mqtt topic>/value/<node_id>
  json:   <string> , payload to send
  retain: <bool> , retain mqtt message  
*/
void sendMQTTData(const char* topic, const char *json, const bool retain = false) 
{
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  char full_topic[100];
  snprintf(full_topic, sizeof(full_topic), "%s/", CSTR(MQTTPubNamespace));
  strlcat(full_topic, topic, sizeof(full_topic));
  MQTTDebugTf("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n", settingMQTTbroker.c_str(), settingMQTTbrokerPort, full_topic, json);
  if (!MQTTclient.publish(full_topic, json, retain)) PrintMQTTError();
  feedWatchDog();//feed the dog
} // sendMQTTData()

/* 
* topic:  <string> , topic will be used as is (no prefixing), retained = true
* json:   <string> , payload to send
*/
//===========================================================================================
void sendMQTT(String topic, String json){
  if (!settingMQTTenable) return;
  sendMQTT(CSTR(topic), CSTR(json), json.length());
} 

void sendMQTT(const char* topic, const char *json, const size_t len) 
{
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); PrintMQTTError(); return;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return;} 
  MQTTDebugTf("Sending MQTT: server %s:%d => TopicId [%s] --> Message [%s]\r\n", settingMQTTbroker.c_str(), settingMQTTbrokerPort, topic, json);
  if (MQTTclient.getBufferSize() < len) MQTTclient.setBufferSize(len); //resize buffer when needed

  if (MQTTclient.beginPublish(topic, len, true)){
    for (size_t i = 0; i<len; i++) {
      if(!MQTTclient.write(json[i])) PrintMQTTError();
    }  
    MQTTclient.endPublish();
  } else PrintMQTTError();

  feedWatchDog();
} // sendMQTTData()


//===========================================================================================
void resetMQTTBufferSize()
{
  if (!settingMQTTenable) return;
  MQTTclient.setBufferSize(256);
}
//===========================================================================================
bool splitLine(String sIn, char del, byte &cID, String &cKey, String &cVal) {
  sIn.trim(); //trim spaces
  cID = 39; // ID 39 is unused in the OT spec
  cKey = "";
  cVal = "";
  
  int posC = sIn.indexOf("//");
  if (posC > -1) sIn.remove(posC); // strip comments

  if (sIn.length() <= 3) return false; //not enough buffer, skip split

  unsigned int pos = sIn.indexOf(del); //determine first split point
  if ((pos <= 0) || (pos == (sIn.length() - 1))) return false; // no key or no value

  unsigned int pos2 = sIn.indexOf(del, pos+1); //determine second split point, starting from the first found
  if ((pos2 <= 0) || (pos2 <= pos) || (pos2 == (sIn.length() - 1))) return false; // no key or no value

  String sID = sIn.substring(0, pos);
  sID.trim();
  cID = (byte)sID.toInt();
 
  cKey = sIn.substring(pos+1, pos2);
  cKey.trim(); //before, and trim spaces
 
  cVal = sIn.substring(pos2 + 1);
  cVal.trim(); //after,and trim spaces

  // Debugf("Split line into: [%d] [%s] [%s]\r\n", cID, CSTR(cKey), CSTR(cVal)); DebugFlush();

  return true;
}
//===========================================================================================
bool getMQTTConfigDone(const uint8_t MSGid)
{
  uint8_t group = MSGid & 0b11100000;
  group = group>>5;
  uint8_t index = MSGid & 0b00011111;
  uint32_t result = bitRead(MQTTautoConfigMap[group], index);
  MQTTDebugTf("Reading bit %d from group %d for MSGid %d: result = %d\r\n", index, group, MSGid, result);
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
  MQTTDebugTf("Setting bit %d from group %d for MSGid %d\r\n", index, group, MSGid);
  MQTTDebugTf("Value before setting bit %d\r\n", MQTTautoConfigMap[group]);
  if(bitSet(MQTTautoConfigMap[group], index) > 0) {
    MQTTDebugTf("Value after setting bit  %d\r\n", MQTTautoConfigMap[group]);
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
void doAutoConfigure(bool bForcaAll = false){
  //force all sensors to be sent to auto configuration
  for (int i=0; i<255; i++){
    if ((getMQTTConfigDone((byte)i)==true) || bForcaAll) {
      MQTTDebugTf("Sending auto configuration for sensor %d\r\n", i);
      doAutoConfigureMsgid((byte)i);
    }
  }
//  bool success = doAutoConfigure("config"); // the string "config" should match every line non-comment in mqttha.cfg
}
//===========================================================================================
bool doAutoConfigureMsgid(byte OTid)
{
  bool _result = false;
  
  if (!settingMQTTenable) return _result;
  if (!MQTTclient.connected()) {DebugTln(F("Error: MQTT broker not connected.")); return _result;} 
  if (!isValidIP(MQTTbrokerIP)) {DebugTln(F("Error: MQTT broker IP not valid.")); return _result;} 

  byte lineID = 39; // 39 is unused in OT protocol so is a safe value
  String sMsg = "";
  String sTopic = "";

  //Let's open the MQTT autoconfig file
  File fh; //filehandle
  const char *cfgFilename = "/mqttha.cfg";
  LittleFS.begin();

  if (!LittleFS.exists(cfgFilename)) {DebugTln(F("Error: confuration file not found.")); return _result;} 

  fh = LittleFS.open(cfgFilename, "r");

  if (!fh) {DebugTln(F("Error: could not open confuration file.")); return _result;} 

  //Lets go read the config and send it out to MQTT line by line
  while (fh.available())
  {                 //read file line by line, split and send to MQTT (topic, msg)
    feedWatchDog(); //start with feeding the dog
    
    String sLine = fh.readStringUntil('\n');
    // DebugTf("sline[%s]\r\n", CSTR(sLine));
    if (!splitLine(sLine, ';', lineID, sTopic, sMsg)) {  //splitLine() also filters comments
      //MQTTDebugTf("Either comment or invalid config line: [%s]\r\n", CSTR(sLine));
      continue;
    }

    // DebugTf("looking in config file line for %d: [%d][%s] \r\n", OTid, lineID, CSTR(sTopic));
    
    // check if this is the specific line we are looking for
    if (lineID != OTid) continue;

    MQTTDebugTf("Found line in config file for %d: [%d][%s] \r\n", OTid, lineID, CSTR(sTopic));

    // discovery topic prefix
    MQTTDebugTf("sTopic[%s]==>", CSTR(sTopic)); 
    sTopic.replace("%homeassistant%", CSTR(settingMQTThaprefix));  

    /// node
    sTopic.replace("%node_id%", CSTR(NodeId));
    MQTTDebugf("[%s]\r\n", CSTR(sTopic)); 
    /// ----------------------

    MQTTDebugTf("sMsg[%s]==>", CSTR(sMsg)); 

    /// node
    sMsg.replace("%node_id%", CSTR(NodeId));

    /// hostname
    sMsg.replace("%hostname%", CSTR(settingHostname));

    /// version
    sMsg.replace("%version%", _VERSION);

    // pub topics prefix
    sMsg.replace("%mqtt_pub_topic%", CSTR(MQTTPubNamespace));

    // sub topics
    sMsg.replace("%mqtt_sub_topic%", CSTR(MQTTSubNamespace));

    MQTTDebugf("[%s]\r\n", CSTR(sMsg)); 
    DebugFlush();

    //sendMQTT(CSTR(sTopic), CSTR(sMsg), (sTopic.length() + sMsg.length()+2));
    sendMQTT(sTopic, sMsg);
    resetMQTTBufferSize();
    delay(10);
    _result = true;

    // TODO: enable this break if we are sure the old config dump method is no longer needed
    // break;

  } // while available()
  
  fh.close();

  // HA discovery msg's are rather large, reset the buffer size to release some memory
  resetMQTTBufferSize();

  return _result;
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
