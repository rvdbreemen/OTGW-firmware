/*********
 * aaa
 * aaa
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
void setOutputState(uint8_t status = ON)
{
  (status == ON) ? setOutputState(true) : setOutputState(false);
}
void setOutputState(bool set_HIGH = true)
{
  if(!settingGPIOOUTPUTSenabled) return;
  digitalWrite(settingGPIOOUTPUTSpin,ON);
  DebugTf("Output GPIO%d set to %d", settingGPIOOUTPUTSpin, digitalRead(settingGPIOOUTPUTSpin));
}

void evalOutputs()
{
  // master
  // bit: [clear/0, set/1]
  //  0: CH enable [ CH is disabled, CH is enabled]
  //  1: DHW enable [ DHW is disabled, DHW is enabled]
  //  2: Cooling enable [ Cooling is disabled, Cooling is enabled]]
  //  3: OTC active [OTC not active, OTC is active]
  //  4: CH2 enable [CH2 is disabled, CH2 is enabled]
  //  5: reserved
  //  6: reserved
  //  7: reserved

  // slave
  //  0: fault indication [ no fault, fault ]
  //  1: CH mode [CH not active, CH active]
  //  2: DHW mode [ DHW not active, DHW active]
  //  3: Flame status [ flame off, flame on ]
  //  4: Cooling status [ cooling mode not active, cooling mode active ]
  //  5: CH2 mode [CH2 not active, CH2 active]
  //  6: diagnostic indication [no diagnostics, diagnostic event]
  //  7: reserved
  if (!settingMyDEBUG)
  {
    return;
  }
  settingMyDEBUG = false;
  DebugTf("current gpio output state: %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
  DebugFlush();

  bool bitState = false;

  bitState = (OTdataObject.Statusflags & (2^settingGPIOOUTPUTStriggerBit));
  DebugTf("bitState: bit: %d , state %d \r\n", settingGPIOOUTPUTStriggerBit, bitState);

  setOutputState(bitState);

  DebugTf("end void: current gpio output state: %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
}