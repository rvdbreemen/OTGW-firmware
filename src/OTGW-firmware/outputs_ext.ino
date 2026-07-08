/*********
**  Program  : output_ext.ino
**  Version  : v2.0.0-alpha.336
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Contributed by Sjorsjuhmaniac
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.   
*********/


// adds support to activiate a digital output
// 
void setOutputState(bool set_HIGH);

static bool outputsInitialized = false;

static bool validateGPIOOutputsConfig() {
  if (settings.outputs.iPin < 0 || settings.outputs.iPin > 16) {
    DebugTf(PSTR("GPIO Outputs: invalid pin %d\r\n"), settings.outputs.iPin);
    return false;
  }
  if (settings.outputs.iTriggerBit < 0 || settings.outputs.iTriggerBit > 15) {
    DebugTf(PSTR("GPIO Outputs: invalid trigger bit %d\r\n"), settings.outputs.iTriggerBit);
    return false;
  }
  return true;
}

void initOutputs() {
  DebugTf(PSTR("inside initOutputsO%d...\r\n"), 1);

  if (!settings.outputs.bEnabled) return;
  if (!validateGPIOOutputsConfig()) return;

  DebugTf(PSTR("init GPIO Output on GPIO%d...\r\n"), settings.outputs.iPin);

  pinMode(settings.outputs.iPin, OUTPUT);
  outputsInitialized = true;
  setOutputState(OFF);

  // set the LED with the ledState of the variable:
  // digitalWrite(ledPin, ledState);
}

// still need to hook into processOTGW
void setOutputState(uint8_t status = ON){
  (status == ON) ? setOutputState(true) : setOutputState(false);
}

void setOutputState(bool set_HIGH = true){
  if(!settings.outputs.bEnabled) return;
  if (!validateGPIOOutputsConfig()) return;
  if (!outputsInitialized) {
    pinMode(settings.outputs.iPin, OUTPUT);
    outputsInitialized = true;
  }
  digitalWrite(settings.outputs.iPin,set_HIGH?HIGH:LOW);
  DebugTf(PSTR("Output GPIO%d set to %d"), settings.outputs.iPin, digitalRead(settings.outputs.iPin));
}

void evalOutputs(){
  if(!settings.outputs.bEnabled) return;
  if (!validateGPIOOutputsConfig()) return;
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
  bool bitState = (OTcurrentSystemState.Statusflags & (1U << settings.outputs.iTriggerBit)) != 0;

  if (settings.bMyDEBUG) {
    settings.bMyDEBUG = false;
    DebugTf(PSTR("current gpio output state: %d \r\n"), digitalRead(settings.outputs.iPin));
    DebugTf(PSTR("bitState: bit: %d , state %d \r\n"), settings.outputs.iTriggerBit, bitState);
    DebugFlush();
  }

  setOutputState(bitState);
}

/***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
* 
****************************************************************************
*/
