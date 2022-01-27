/*

// S0 Counter Settings and variables with global scope, to be defined in xx.h 
bool      settingS0COUNTERenabled = false;
int8_t    settingS0COUNTERpin = 12;
int16_t   settingS0COUNTERdebouncetime = 80;
int16_t   settingS0COUNTERpulsekw = 1000;
int16_t   settingS0COUNTERinterval = 60;
unsigned long OTGWpulseCount;  
unsigned long OTGWpulseCountTot = 0;  
float OTGWS0kW = 0 ;



*/
#include <Arduino.h>
//-----------------------------------------------------------------------------------------------------------
// const byte pulse_count_pin=        12;  // GPIO 12 = D6 for now, should be setting                 

// Pulse counting settings 
volatile unsigned int pulseCount = 0;                  // Number of pulses, used to measure energy.
unsigned long timeS0Count = 0 ;
unsigned long lastS0Count = 0 ;

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
void IRAM_ATTR IRQcounter() {
 volatile unsigned long last_interrupt_time = 0;
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
    float avgtimeS0 ;
    noInterrupts();
    OTGWpulseCount = pulseCount; 
    OTGWpulseCountTot = OTGWpulseCountTot + pulseCount; 
    pulseCount=0; 
    interrupts();

    timeS0Count = millis();
    avgtimeS0 = ( timeS0Count - lastS0Count ) / OTGWpulseCount ; 
    OTGWS0kW = 3600000 / avgtimeS0 / settingS0COUNTERpulsekw ;
    OTGWDebugTf("*** S0PulseCount( %d ) S0PulseCountTot( %d )\r\n", OTGWpulseCount, OTGWpulseCountTot) ;
    OTGWDebugTf("*** timeS0Count( %d ) lastS0count( %d )\r\n", timeS0Count, lastS0Count) ;
    OTGWDebugTf("*** S0Pulsetimeavg: %f\r\n", avgtimeS0 ) ;
    OTGWDebugTf("*** S0Pulsekw: %f\r\n", OTGWS0kW ) ;

    lastS0Count = timeS0Count ;
    OTGWS0lasttime = now() ;

  }
} 
