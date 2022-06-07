/*********
**  Program  : output_ext.ino
**  Version  : v0.10.0
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**  Contributed by Sjorsjuhmaniac
**
**  TERMS OF USE: MIT License. See bottom of file.   
*********/


// adds support to activiate a digital output
// 
void setOutputState(bool set_HIGH);


void initOutputs() {
  DebugTf("inside initOutputsO%d...\r\n", 1);

  if (!settingGPIOOUTPUTSenabled) return;

  DebugTf("init GPIO Output on GPIO%d...\r\n", settingGPIOOUTPUTSpin);

  pinMode(settingGPIOOUTPUTSpin, OUTPUT);
  setOutputState(OFF);

  // set the LED with the ledState of the variable:
  // digitalWrite(ledPin, ledState);
}

// still need to hook into processOTGW
void setOutputState(uint8_t status = ON){
  (status == ON) ? setOutputState(true) : setOutputState(false);
}

void setOutputState(bool set_HIGH = true){
  if(!settingGPIOOUTPUTSenabled) return;
  digitalWrite(settingGPIOOUTPUTSpin,set_HIGH?HIGH:LOW);
  DebugTf("Output GPIO%d set to %d", settingGPIOOUTPUTSpin, digitalRead(settingGPIOOUTPUTSpin));
}

void evalOutputs(){
  if(!settingGPIOOUTPUTSenabled) return;
  // master HB
  // bit: [clear/0, set/1]
  //  0: CH enable [ CH is disabled, CH is enabled]
  //  1: DHW enable [ DHW is disabled, DHW is enabled]
  //  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
  //  3: OTC active [OTC not active, OTC is active]
  //  4: CH2 enable [CH2 is disabled, CH2 is enabled]
  //  5: reserved
  //  6: reserved
  //  7: reserved

  // slave LB
  //  0: fault indication [ no fault, fault ]
  //  1: CH mode [CH not active, CH active]
  //  2: DHW mode [ DHW not active, DHW active]
  //  3: Flame status [ flame off, flame on ]
  //  4: Cooling status [ cooling mode not active, cooling mode active ]
  //  5: CH2 mode [CH2 not active, CH2 active]
  //  6: diagnostic indication [no diagnostics, diagnostic event]
  //  7: reserved
  if (!settingMyDEBUG) return;
  settingMyDEBUG = false;
  DebugTf("current gpio output state: %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
  DebugFlush();

  bool bitState = (OTcurrentSystemState.Statusflags & (2^settingGPIOOUTPUTStriggerBit));
  DebugTf("bitState: bit: %d , state %d \r\n", settingGPIOOUTPUTStriggerBit, bitState);

  setOutputState(bitState);

  DebugTf("end void: current gpio output state: %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
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
