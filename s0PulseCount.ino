/* 
***************************************************************************  
**  Program  : s0PulseCount
**  Version  : v0.0.2
**
**  Copyright (c) 2021-2022 Rob Roos / Robert van Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      

Functionality to measure number of pulses from a S0 output, eg from an energy consumption meter.
The S0 port is to be connected in a NO mode, with pulse closing contact pulling a configurable pin to Low.

MQ interface is enabled with Home Assistant AutoConfigure, for this the OTGW function is reused by using a foney dataid (245)

// S0 Counter Settings and variables with global scope, to be defined in xx.h 
bool      settingS0COUNTERenabled = false;      
uint8_t   settingS0COUNTERpin = 12;               // GPIO 12 = D6, preferred, can be any pin with Interupt support
uint16_t  settingS0COUNTERdebouncetime = 80;      // Depending on S0 switch a debouncetime should be tailored
uint16_t  settingS0COUNTERpulsekw = 1000;         // Most S0 counters have 1000 pulses per kW, but this can be different
uint16_t  settingS0COUNTERinterval = 60;          // Sugggested measurement reporting interval
uint16_t  OTGWs0pulseCount;                       // Number of S0 pulses in measurement interval
uint32_t  OTGWs0pulseCountTot = 0;                // Number of S0 pulses since start of measurement
float     OTGWs0powerkw = 0 ;                     // Calculated kW actual consumption based on time between last pulses and settings
time_t    OTGWs0lasttime = 0;                     // Last time S0 counters have been read
byte      OTGWs0dataid = 245;                     // Foney dataid to be used to do autoconfigure, align value with mqttha.cfg contents

*/
#include <Arduino.h>
//-----------------------------------------------------------------------------------------------------------
volatile uint8_t pulseCount = 0;                  // Number of pulses, used to measure energy.
volatile uint32_t last_pulse_duration = 0;     // Duration of the time between last pulses

//-----------------------------------------------------------------------------------------------------------
void IRAM_ATTR IRQcounter() {
 static uint32_t last_interrupt_time = 0;
 volatile uint32_t interrupt_time;

interrupt_time = millis() ;
 // If interrupts come faster than debouncetime, assume it's a bounce and ignore
 if (interrupt_time - last_interrupt_time > settingS0COUNTERdebouncetime)
 {
   pulseCount++;
   last_pulse_duration = interrupt_time - last_interrupt_time ;
 }
 last_interrupt_time = interrupt_time;
}

 
void initS0Count()
{ 
  if (!settingS0COUNTERenabled) return;
  //------------------------------------------------------------------------------------------------------------------------------

  pinMode(settingS0COUNTERpin, INPUT_PULLUP);         // Set interrupt pulse counting pin as input (Dig 3 / INT1)
  OTGWs0pulseCount=0;                                 // Make sure pulse count starts at zero
  attachInterrupt(digitalPinToInterrupt(settingS0COUNTERpin), IRQcounter, FALLING) ;
  if (bDebugSensors) DebugTf("*** S0PulseCounter initialized on GPIO[%d] )\r\n", settingS0COUNTERpin) ;
} //end SETUP

void sendS0Counters() 
{
  if (!settingS0COUNTERenabled) return;

  if (pulseCount != 0 ) {
    noInterrupts();
    OTGWs0pulseCount = pulseCount; 
    OTGWs0pulseCountTot = OTGWs0pulseCountTot + pulseCount; 
    pulseCount=0; 
    interrupts();

    OTGWs0powerkw =  (float) 3600000 / (float)settingS0COUNTERpulsekw  / (float)last_pulse_duration ;
    if (bDebugSensors) DebugTf("*** S0PulseCount(%d) S0PulseCountTot(%d)\r\n", OTGWs0pulseCount, OTGWs0pulseCountTot) ;
    if (bDebugSensors) DebugTf("*** S0LastPulsetime(%d) S0Pulsekw:(%4.3f) \r\n", last_pulse_duration, OTGWs0powerkw) ;
    OTGWs0lasttime = now() ;
    if (settingMQTTenable ) {
      sensorAutoConfigure(OTGWs0dataid, true , "" ) ;     // Configure S0 sensor with the  
      s0sendMQ() ; 
    }  
  }
} 

void s0sendMQ() 
{
//Build string for MQTT
char _msg[15]{0};
char _topic[50]{0};
snprintf(_topic, sizeof _topic, "s0pulsecount");
snprintf(_msg, sizeof _msg, "%d", OTGWs0pulseCount);
sendMQTTData(_topic, _msg);

snprintf(_topic, sizeof _topic, "s0pulsecounttot");
snprintf(_msg, sizeof _msg, "%d", OTGWs0pulseCountTot);
sendMQTTData(_topic, _msg);

snprintf(_topic, sizeof _topic, "s0pulsetime");
snprintf(_msg, sizeof _msg, "%d", last_pulse_duration);
sendMQTTData(_topic, _msg);

snprintf(_topic, sizeof _topic, "s0powerkw");
snprintf(_msg, sizeof _msg, "%4.3f", OTGWs0powerkw);
sendMQTTData(_topic, _msg);

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
****************************************************************************
*/
