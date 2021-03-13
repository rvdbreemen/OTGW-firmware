/*********
 * aaa
 * aaa
*********/


// GPIO where the DS18B20 is connected to
// Data wire is plugged TO GPIO 10
// #define ONE_WIRE_BUS 10


// settingGPIOOUTPUTSpin

void initOutputs() {
  if (!settingGPIOOUTPUTSenabled) return;

  DebugTf("init GPIO Output on GPIO%d...\r\n", settingGPIOOUTPUTSpin);

  pinMode(settingGPIOOUTPUTSpin, OUTPUT);

  // set the LED with the ledState of the variable:
  // digitalWrite(ledPin, ledState);
}

// still need to hook into processOTGW
void setOutput(bool set_HIGH = true)
{
  if(!settingGPIOOUTPUTSenabled) return;
  digitalWrite(settingGPIOOUTPUTSpin,set_HIGH)
}