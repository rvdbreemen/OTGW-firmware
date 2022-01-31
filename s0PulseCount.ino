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

// S0 Counter Settings and variables with global scope, to be defined in xx.h 
bool      settingS0COUNTERenabled = false;      
int8_t    settingS0COUNTERpin = 12;               // GPIO 12 = D6, preferred, can be any pin with Interupt support
int16_t   settingS0COUNTERdebouncetime = 80;      // Depending on S0 switch a debouncetime should be tailored
int16_t   settingS0COUNTERpulsekw = 1000;         // Most S0 counters have 1000 pulses per kW, but this can be different
int16_t   settingS0COUNTERinterval = 60;          // Sugggested measurement reporting interval
int16_t   OTGWpulseCount;                         // Number of S0 pulses in measurement interval
int32_t   OTGWpulseCountTot = 0;                  // Number of S0 pulses since start of measurement
float     OTGWs0intervalkw = 0 ;                          // Calculated kW actual consumption based on pulses and settings
time_t    OTGWS0lasttime = 0;                     // Last time S0 counters have been read


*/
#include <Arduino.h>
//-----------------------------------------------------------------------------------------------------------
volatile unsigned int pulseCount = 0;                  // Number of pulses, used to measure energy.
uint32_t timeS0Count = 0 ;
uint32_t lastS0Count = 0 ;
float   s0avgtime ;


//-----------------------------------------------------------------------------------------------------------
void IRAM_ATTR IRQcounter() {
 static unsigned long last_interrupt_time = 0;
 volatile unsigned long interrupt_time = millis();
 // If interrupts come faster than 150ms, assume it's a bounce and ignore
 if (interrupt_time - last_interrupt_time > settingS0COUNTERdebouncetime)
 {
   pulseCount++;
 }
 last_interrupt_time = interrupt_time;
}

 
void initS0Count()
{ 
  if (!settingS0COUNTERenabled) return;
  //------------------------------------------------------------------------------------------------------------------------------

  pinMode(settingS0COUNTERpin, INPUT_PULLUP);           // Set interrupt pulse counting pin as input (Dig 3 / INT1)
  OTGWpulseCount=0;                                 // Make sure pulse count starts at zero
  attachInterrupt(digitalPinToInterrupt(settingS0COUNTERpin), IRQcounter, FALLING) ;
  lastS0Count = millis();  // Initialise first timecount
  timeS0Count = millis();  // Initialise first timecount
} //end SETUP

void sendS0Counters() 
{
  if (!settingS0COUNTERenabled) return;

  if (pulseCount != 0 ) {
    noInterrupts();
    OTGWpulseCount = pulseCount; 
    OTGWpulseCountTot = OTGWpulseCountTot + pulseCount; 
    pulseCount=0; 
    interrupts();

    timeS0Count = millis();
    s0avgtime = ( timeS0Count - lastS0Count ) / OTGWpulseCount ; 
    OTGWs0intervalkw = 3600000 / s0avgtime / settingS0COUNTERpulsekw ;
    OTGWDebugTf("*** S0PulseCount( %d ) S0PulseCountTot( %d )\r\n", OTGWpulseCount, OTGWpulseCountTot) ;
    OTGWDebugTf("*** timeS0Count( %d ) lastS0count( %d )\r\n", timeS0Count, lastS0Count) ;
    OTGWDebugTf("*** S0Pulsetimeavg: %f\r\n", s0avgtime ) ;
    OTGWDebugTf("*** S0Pulsekw: %f\r\n", OTGWs0intervalkw ) ;

    lastS0Count = timeS0Count ;
    OTGWS0lasttime = now() ;
    s0sendMQ() ; 
  }
} 

void s0sendMQ() 
{
//Build string for MQTT
char _msg[15]{0};
char _topic[50]{0};
snprintf(_topic, sizeof _topic, "s0pulsecount");
snprintf(_msg, sizeof _msg, "%d", OTGWpulseCount);
sendMQTTData(_topic, _msg);

snprintf(_topic, sizeof _topic, "s0pulsecounttot");
snprintf(_msg, sizeof _msg, "%d", OTGWpulseCountTot);
sendMQTTData(_topic, _msg);

snprintf(_topic, sizeof _topic, "s0pulsetime");
snprintf(_msg, sizeof _msg, "%f", s0avgtime);
sendMQTTData(_topic, _msg);

snprintf(_topic, sizeof _topic, "s0intervalkw");
snprintf(_msg, sizeof _msg, "%f", OTGWs0intervalkw);
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
