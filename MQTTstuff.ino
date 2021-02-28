/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v0.7.8
**
**  Copyright (c) 2021 Robert van den Breemen
**      Modified version from (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

// Declare some variables within global scope

  static IPAddress  MQTTbrokerIP;
  static char       MQTTbrokerIPchar[20];
  
  #include <PubSubClient.h>           // MQTT client publish and subscribe functionality

  static            PubSubClient MQTTclient(wifiClient);

  int8_t            reconnectAttempts = 0;
  char              lastMQTTtimestamp[15] = "";

  enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_TRY_TO_CONNECT, MQTT_STATE_IS_CONNECTED, MQTT_STATE_WAIT_CONNECTION_ATTEMPT, MQTT_STATE_WAIT_FOR_RECONNECT, MQTT_STATE_ERROR };
  enum states_of_MQTT stateMQTT = MQTT_STATE_INIT;

  String            MQTTclientId;

//===========================================================================================
void startMQTT() 
{
  if (!settingMQTTenable) return;
  stateMQTT = MQTT_STATE_INIT;
  handleMQTT(); //initialize the MQTT statemachine
  // handleMQTT(); //then try to connect to MQTT
  // handleMQTT(); //now you should be connected to MQTT ready to send
}

void handleMQTTcallback(char* topic, byte* payload, unsigned int length) {

  DebugT("Message arrived on topic ["); Debug(topic); Debug("] = [");
  for (int i = 0; i < length; i++) {
    Debug((char)payload[i]);
  }
  Debug("] ("); Debug(length); Debug(")"); Debugln();
  
  char subscribeTopic[100];
  snprintf(subscribeTopic, sizeof(subscribeTopic), "%s/", settingMQTTtopTopic.c_str());
  strlcat(subscribeTopic, OTGW_COMMAND_TOPIC, sizeof(subscribeTopic));
  //what is the incoming message?  
  if (stricmp(topic, subscribeTopic) == 0) 
  {
    //incoming command to be forwarded to OTGW
    sendOTGW((char *)payload, length);
  }
}

//===========================================================================================
void handleMQTT() 
{  
  if (!settingMQTTenable) return;
  DECLARE_TIMER_MIN(timerMQTTwaitforconnect, 10, CATCH_UP_MISSED_TICKS);  // 10 minutes
  DECLARE_TIMER_SEC(timerMQTTwaitforretry, 3, CATCH_UP_MISSED_TICKS);     // 3 seconden backoff

  switch(stateMQTT) 
  {
    case MQTT_STATE_INIT:  
      DebugTln(F("MQTT State: MQTT Initializing")); 
      WiFi.hostByName(CSTR(settingMQTTbroker), MQTTbrokerIP);  // lookup the MQTTbroker convert to IP
      sprintf(MQTTbrokerIPchar, "%d.%d.%d.%d", MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
      if (isValidIP(MQTTbrokerIP))  
      {
        DebugTf("[%s] => setServer(%s, %d)\r\n", CSTR(settingMQTTbroker), MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.disconnect();
        MQTTclient.setServer(MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.setCallback(handleMQTTcallback);
        MQTTclientId  = String(_HOSTNAME) + WiFi.macAddress();
        //skip try to connect
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
      }
      else
      { // invalid IP, then goto error state
        DebugTf("ERROR: [%s] => is not a valid URL\r\n", CSTR(settingMQTTbroker));
        stateMQTT = MQTT_STATE_ERROR;
        //DebugTln(F("Next State: MQTT_STATE_ERROR"));
      }    
      RESTART_TIMER(timerMQTTwaitforconnect); 
    break;

    case MQTT_STATE_TRY_TO_CONNECT:
      DebugTln(F("MQTT State: MQTT try to connect"));
      //DebugTf("MQTT server is [%s], IP[%s]\r\n", settingMQTTbroker.c_str(), MQTTbrokerIPchar);
      
      DebugT(F("Attempting MQTT connection .. "));
      reconnectAttempts++;

      //If no username, then anonymous connection to broker, otherwise assume username/password.
      if (settingMQTTuser.length() == 0) 
      {
        Debug(F("without a Username/Password "));
        MQTTclient.connect(CSTR(MQTTclientId));
      } 
      else 
      {
        Debugf("Username [%s] ", CSTR(settingMQTTuser));
        MQTTclient.connect(CSTR(MQTTclientId), CSTR(settingMQTTuser), CSTR(settingMQTTpasswd));
      }

      //If connection was made succesful, move on to next state...
      if (MQTTclient.connected())
      {
        reconnectAttempts = 0;  
        Debugln(F(" .. connected\r"));
        stateMQTT = MQTT_STATE_IS_CONNECTED;
        //DebugTln(F("Next State: MQTT_STATE_IS_CONNECTED"));
        //First do AutoConfiguration for Homeassistant
        doAutoConfigure();
        //Subscribe to topics
        char topic[100];
        strcpy(topic, CSTR(settingMQTTtopTopic));
        strlcat(topic, "/", sizeof(topic));
        strlcat(topic, OTGW_COMMAND_TOPIC, sizeof(topic));
        DebugTf("Subscribe to MQTT: TopicId [%s]\r\n", topic);
        MQTTclient.subscribe(topic); 
      }
      else
      { // no connection, try again, do a non-blocking wait for 3 seconds.
        Debugln(F(" .. \r"));
        DebugTf("failed, retrycount=[%d], rc=[%d] ..  try again in 3 seconds\r\n", reconnectAttempts, MQTTclient.state());
        RESTART_TIMER(timerMQTTwaitforretry);
        stateMQTT = MQTT_STATE_WAIT_CONNECTION_ATTEMPT;  // if the re-connect did not work, then return to wait for reconnect
        //DebugTln(F("Next State: MQTT_STATE_WAIT_CONNECTION_ATTEMPT"));
      }
      
      //After 5 attempts... go wait for a while.
      if (reconnectAttempts >= 5)
      {
        DebugTln(F("5 attempts have failed. Retry wait for next reconnect in 10 minutes\r"));
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;  // if the re-connect did not work, then return to wait for reconnect
        //DebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }   
    break;
    
    case MQTT_STATE_IS_CONNECTED:
      if (Verbose) DebugTln(F("MQTT State: MQTT is Connected"));
      if (MQTTclient.connected()) 
      { //if the MQTT client is connected, then please do a .loop call...
        MQTTclient.loop();
      }
      else
      { //else go and wait 10 minutes, before trying again.
        RESTART_TIMER(timerMQTTwaitforconnect);
        stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
        //DebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
      }  
    break;

    case MQTT_STATE_WAIT_CONNECTION_ATTEMPT:
      //do non-blocking wait for 3 seconds
      //DebugTln(F("MQTT State: MQTT_WAIT_CONNECTION_ATTEMPT"));
      if (DUE(timerMQTTwaitforretry))
      {
        //Try again... after waitforretry non-blocking delay
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        //DebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;
    
    case MQTT_STATE_WAIT_FOR_RECONNECT:
      //do non-blocking wait for 10 minutes, then try to connect again. 
      if (Verbose) DebugTln(F("MQTT State: MQTT wait for reconnect"));
      if (DUE(timerMQTTwaitforconnect))
      {
        //remember when you tried last time to reconnect
        RESTART_TIMER(timerMQTTwaitforretry);
        reconnectAttempts = 0; 
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
        //DebugTln(F("Next State: MQTT_STATE_TRY_TO_CONNECT"));
      }
    break;

    case MQTT_STATE_ERROR:
      DebugTln(F("MQTT State: MQTT ERROR, wait for 10 minutes, before trying again"));
      //next retry in 10 minutes.
      RESTART_TIMER(timerMQTTwaitforconnect);
      stateMQTT = MQTT_STATE_WAIT_FOR_RECONNECT;
      //DebugTln(F("Next State: MQTT_STATE_WAIT_FOR_RECONNECT"));
    break;

    default:
      DebugTln(F("MQTT State: default, this should NEVER happen!"));
      //do nothing, this state should not happen
      stateMQTT = MQTT_STATE_INIT;
      //DebugTln(F("Next State: MQTT_STATE_INIT"));
    break;
  }
  statusMQTTconnection = MQTTclient.connected();
} // handleMQTT()


// bool MQTT_connected()
// {
//   if (!settingMQTTenable) return false;
//   return MQTTclient.connected();
// }

// bool getMQTTconnectstatus(){
//   if (!settingMQTTenable) return false;
//   return MQTTclient.connected();
// }


//===========================================================================================
String trimVal(char *in) 
{
  String Out = in;
  Out.trim();
  return Out;
} // trimVal()

//===========================================================================================
void sendMQTTData(const String item, const String json)
{
  if (!settingMQTTenable) return;
  sendMQTTData(CSTR(item), CSTR(json));
} 

void sendMQTTData(const char* item, const char *json) 
{
/*  
* The maximum message size, including header, is 128 bytes by default. 
* This is configurable via MQTT_MAX_PACKET_SIZE in PubSubClient.h.
* Als de json string te lang wordt zal de string niet naar de MQTT server
* worden gestuurd. Vandaar de korte namen als ED en PDl1.
* Mocht je langere, meer zinvolle namen willen gebruiken dan moet je de
* MQTT_MAX_PACKET_SIZE dus aanpassen!!!
*/
//===========================================================================================
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected() || !isValidIP(MQTTbrokerIP)) return;
  // DebugTf("Sending data to MQTT server [%s]:[%d]\r\n", settingMQTTbroker.c_str(), settingMQTTbrokerPort);
  char topic[100];
  snprintf(topic, sizeof(topic), "%s/", CSTR(settingMQTTtopTopic));
  strlcat(topic, item, sizeof(topic));
  //DebugTf("Sending MQTT: TopicId [%s] Message [%s]\r\n", topic, json);
  if (!MQTTclient.publish(topic, json, true)) DebugTln("MQTT publish failed.");
  feedWatchDog();//feed the dog
} // sendMQTTData()

//===========================================================================================
void sendMQTT(const char* topic, const char *json, const int8_t len) 
{
  if (!settingMQTTenable) return;
  if (!MQTTclient.connected() || !isValidIP(MQTTbrokerIP)) return;
  // DebugTf("Sending data to MQTT server [%s]:[%d] ", settingMQTTbroker.c_str(), settingMQTTbrokerPort);  
  //DebugTf("Sending MQTT: TopicId [%s] Message [%s]\r\n", topic, json);
  if (MQTTclient.getBufferSize() < len) MQTTclient.setBufferSize(len); //resize buffer when needed
  if (!MQTTclient.publish(topic, json, true)) DebugTln("MQTT publish failed.");
  feedWatchDog();
} // sendMQTTData()

//===========================================================================================
bool splitString(String sIn, char del, String& cKey, String& cVal)
{
  sIn.trim();                                 //trim spaces
  cKey=""; cVal="";
  if (sIn.indexOf("//")==0) return false;     //comment, skip split
  if (sIn.length()<=3) return false;          //not enough buffer, skip split
  int pos = sIn.indexOf(del);                 //determine split point
  if ((pos==0) || (pos==(sIn.length()-1))) return false; // no key or no value
  cKey = sIn.substring(0,pos); cKey.trim();   //before, and trim spaces
  cVal = sIn.substring(pos+1); cVal.trim();   //after,and trim spaces
  return true;
}
//===========================================================================================
void doAutoConfigure()
{
  if (!settingMQTTenable) return;
  const char* cfgFilename = "/mqttha.cfg";
  String sTopic="";
  String sMsg="";
  File fh; //filehandle
  //Let's open the MQTT autoconfig file
  LittleFS.begin();
  if (LittleFS.exists(cfgFilename))
  {
    fh = LittleFS.open(cfgFilename, "r");
    if (fh) {
      //Lets go read the config and send it out to MQTT line by line
      while(fh.available()) 
      {  //read file line by line, split and send to MQTT (topic, msg)
          feedWatchDog(); //start with feeding the dog
          
          String sLine = fh.readStringUntil('\n');
          // DebugTf("sline[%s]\r\n", CSTR(sLine));
          if (splitString(sLine, ',', sTopic, sMsg))
          {
            // discovery topic prefix
            DebugTf("sTopic[%s]==>", CSTR(sTopic)); DebugFlush();
            sTopic.replace("%homeassistant%", CSTR(settingMQTThaprefix));  
            Debugf("[%s]\r\n", CSTR(sTopic));DebugFlush();

            /// node
            DebugTf("sTopic[%s]==>", CSTR(sTopic)); DebugFlush();
            sTopic.replace("%node_id%", CSTR(getUniqueId()));
            Debugf("[%s]\r\n", CSTR(sTopic)); DebugFlush();

            /// ----------------------
            /// node
            DebugTf("sMsg[%s]==>", CSTR(sMsg)); DebugFlush();
            sMsg.replace("%node_id%", CSTR(getUniqueId()));
            Debugf("[%s]\r\n", CSTR(sMsg)); DebugFlush();

            /// version
            DebugTf("sMsg[%s]==>", CSTR(sMsg)); DebugFlush();
            sMsg.replace("%version%", CSTR(String(_VERSION)));
            Debugf("[%s]\r\n", CSTR(sMsg)); DebugFlush();

            // payload topic prefix
            DebugTf("sMsg[%s]==>", CSTR(sMsg)); DebugFlush();
            sMsg.replace("%OTGW%", CSTR(settingMQTTtopTopic));
            Debugf("[%s]\r\n", CSTR(sMsg)); DebugFlush();

            sendMQTT(CSTR(sTopic), CSTR(sMsg), (sTopic.length() + sMsg.length()+2));
            delay(10);
          } else DebugTf("Either comment or invalid config line: [%s]\r\n", CSTR(sLine));
      } // while available()
      fh.close();  
    } 
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
