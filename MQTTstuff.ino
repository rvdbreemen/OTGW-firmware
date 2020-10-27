/* 
***************************************************************************  
**  Program  : MQTTstuff
**  Version  : v0.1.0
**
**  Copyright (c) 2020 Robert van den Breemen
**      Modified version from (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*  RB  changed MQTT stuff to FSM 
*  RB  added safeTimers 
*  RB  sendMQTT generalized
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
  stateMQTT = MQTT_STATE_INIT;
  handleMQTT();
}
//===========================================================================================
void handleMQTT() 
{
  DECLARE_TIMER_MIN(timerMQTTwaitforconnect, 10, CATCH_UP_MISSED_TICKS);  // 10 minutes
  DECLARE_TIMER_SEC(timerMQTTwaitforretry, 3, CATCH_UP_MISSED_TICKS);     // 3 seconden backoff

  switch(stateMQTT) 
  {
    case MQTT_STATE_INIT:  
      DebugTln(F("MQTT State: MQTT Initializing")); 
      WiFi.hostByName(settingMQTTbroker.c_str(), MQTTbrokerIP);  // lookup the MQTTbroker convert to IP
      sprintf(MQTTbrokerIPchar, "%d.%d.%d.%d", MQTTbrokerIP[0], MQTTbrokerIP[1], MQTTbrokerIP[2], MQTTbrokerIP[3]);
      if (isValidIP(MQTTbrokerIP))  
      {
        DebugTf("[%s] => setServer(%s, %d)\r\n", settingMQTTbroker.c_str(), MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclient.disconnect();
        MQTTclient.setServer(MQTTbrokerIPchar, settingMQTTbrokerPort);
        MQTTclientId  = String(_HOSTNAME) + WiFi.macAddress();
        //skip try to connect
        stateMQTT = MQTT_STATE_TRY_TO_CONNECT;
      }
      else
      { // invalid IP, then goto error state
        DebugTf("ERROR: [%s] => is not a valid URL\r\n", settingMQTTbroker.c_str());
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
        MQTTclient.connect(MQTTclientId.c_str());
      } 
      else 
      {
        Debugf("Username [%s] ", settingMQTTuser.c_str());
        MQTTclient.connect(MQTTclientId.c_str(), settingMQTTuser.c_str(), settingMQTTpasswd.c_str());
      }

      //If connection was made succesful, move on to next state...
      if  (MQTTclient.connected())
      {
        reconnectAttempts = 0;  
        Debugln(F(" .. connected\r"));
        stateMQTT = MQTT_STATE_IS_CONNECTED;
        //DebugTln(F("Next State: MQTT_STATE_IS_CONNECTED"));
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
} // handleMQTT()

//===========================================================================================
String trimVal(char *in) 
{
  String Out = in;
  Out.trim();
  return Out;
} // trimVal()

//===========================================================================================
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

  if (!MQTTclient.connected() || !isValidIP(MQTTbrokerIP)) return;
  DebugTf("Sending data to MQTT server [%s]:[%d]\r\n", settingMQTTbroker.c_str(), settingMQTTbrokerPort);
  
  //build topic
  char topic[100];
  snprintf(topic, sizeof(topic), "%s/", settingMQTTtopTopic.c_str());
  strlcat(topic, item, sizeof(topic));
  DebugTf("TopicId [%s] Message [%s]\r\n", topic, json);
  MQTTclient.publish(topic, json, true); //retain message at broker

} // sendMQTTData()
 
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
