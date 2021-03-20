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
  setOutputState(true);

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

void outputsHook(const char * master, const char * slave)
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

  // _flag8_master[0] = (((OTdata.valueHB) & 0x01) ? 'C' : '-');
  // _flag8_master[1] = (((OTdata.valueHB) & 0x02) ? 'D' : '-');
  // _flag8_master[2] = (((OTdata.valueHB) & 0x04) ? 'C' : '-');
  // _flag8_master[3] = (((OTdata.valueHB) & 0x08) ? 'O' : '-');
  // _flag8_master[4] = (((OTdata.valueHB) & 0x10) ? '2' : '-');
  // _flag8_master[5] = (((OTdata.valueHB) & 0x20) ? '.' : '-');
  // _flag8_master[6] = (((OTdata.valueHB) & 0x40) ? '.' : '-');
  // _flag8_master[7] = (((OTdata.valueHB) & 0x80) ? '.' : '-');
  // _flag8_master[8] = '\0';

  // slave
  //  0: fault indication [ no fault, fault ]
  //  1: CH mode [CH not active, CH active]
  //  2: DHW mode [ DHW not active, DHW active]
  //  3: Flame status [ flame off, flame on ]
  //  4: Cooling status [ cooling mode not active, cooling mode active ]
  //  5: CH2 mode [CH2 not active, CH2 active]
  //  6: diagnostic indication [no diagnostics, diagnostic event]
  //  7: reserved

  // _flag8_slave[0] = (((OTdata.valueLB) & 0x01) ? 'E' : '-');
  // _flag8_slave[1] = (((OTdata.valueLB) & 0x02) ? 'C' : '-');
  // _flag8_slave[2] = (((OTdata.valueLB) & 0x04) ? 'W' : '-');
  // _flag8_slave[3] = (((OTdata.valueLB) & 0x08) ? 'F' : '-');
  // _flag8_slave[4] = (((OTdata.valueLB) & 0x10) ? 'C' : '-');
  // _flag8_slave[5] = (((OTdata.valueLB) & 0x20) ? '2' : '-');
  // _flag8_slave[6] = (((OTdata.valueLB) & 0x40) ? 'D' : '-');
  // _flag8_slave[7] = (((OTdata.valueLB) & 0x80) ? '.' : '-');
  // _flag8_slave[8] = '\0';

  Debugf("Master bits = M[%s] \r\n", master);
  Debugf("Slave  bits = S[%s] \r\n", slave);
  DebugTf("current gpio output state: %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
  DebugFlush();

  //only 8 bits so set it to a value we normally shouldn't reach to track for error
  int dataIDbit = 9;

  // if trigger < 8 then the trigger is one of the master bits
  // if trigger > 9 then the trigger is one of the slave bits
  // if trigger == 9 than something went wrong
  // master bits
  if (settingGPIOOUTPUTStriggerBit >= 0 && settingGPIOOUTPUTStriggerBit < 7)
  {
    Debugf("inside master [%s] \r\n", master);
    if (stricmp((const char*)master[settingGPIOOUTPUTStriggerBit], ".")!=0) 
    {
      Debugf("valid bit master [%s] \r\n", master);
      if (stricmp((const char*)master[settingGPIOOUTPUTStriggerBit], "-")!=0) 
      {
        Debugf("bit master on [%s] \r\n", master);
        setOutputState(true);
      } 
      else
      {
        Debugf("bit master off [%s] \r\n", master);
        setOutputState(false);
        /* code */
    }
  }
  }
  // slave bits
  else if (settingGPIOOUTPUTStriggerBit >= 10 && settingGPIOOUTPUTStriggerBit < 17)
  {
    if (stricmp((const char*)master[settingGPIOOUTPUTStriggerBit - 10], "-")!=0)
    {
      setOutputState(true);
    }
    else
    {
    setOutputState(false);
    }
  }
  else
  {
    // should never happen, error
    // Debugln("Illegal value for settingGPIOOUTPUTStriggerBit: %d", settingGPIOOUTPUTStriggerBit);
    DebugTf("Illegal value for settingGPIOOUTPUTStriggerBit: ...\r\n", settingGPIOOUTPUTStriggerBit);
    return;
  }
  DebugTf("end void: current gpio output state: %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
}