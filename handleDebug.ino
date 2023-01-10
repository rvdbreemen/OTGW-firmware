void handleDebug(){
    if (TelnetStream.available()>0){
        //read the next 
        char c;
        c = TelnetStream.read();
        switch (c){
            case 'h':
                Debugln();
                Debugln(F("---===[ Debug Help Menu ]===---"));
                Debugln(F("1) Toggle verbose debug logging - OT message parsing"));
                Debugln(F("2) Toggle verbose debug logging - API handeling"));
                Debugln(F("3) Toggle verbose debug logging - MQTT module"));
                Debugln(F("q) Force read settings"));
                Debugln(F("m) Force MQTT discovery"));
                Debugln(F("r) Reconnect wifi, telnet, otgwstream and mqtt"));
                Debugln();
                break;
            case 'q':
                DebugTln(F("Read settings"));
                readSettings(true);
                break;
            case 'm':
                DebugTln(F("Configure MQTT Discovery"));
                DebugTf(PSTR("Enable MQTT: %s\r\n"), CBOOLEAN(settingMQTTenable));
                doAutoConfigure();
                break;
            case 'r':   
                if (WiFi.status() != WL_CONNECTED)
                {
                    DebugTln(F("Reconnecting to wifi"));
                    startWiFi(CSTR(settingHostname), 240);
                    //check OTGW and telnet
                    startTelnet();
                    startOTGWstream(); 
                } else DebugTln(F("Wifi is connected"));
                    
                if (!statusMQTTconnection) {
                    DebugTln(F("Reconnecting MQTT"));
                    startMQTT();
                } else DebugTln(F("MQTT is connected"));
                break;
            case '1':   
                bDebugOTmsg = !bDebugOTmsg; 
                DebugTf(PSTR("\r\nDebug OTmsg: %s\r\n"), CBOOLEAN(bDebugOTmsg)); 
                break;
            case '2':   
                bDebugRestAPI = !bDebugRestAPI; 
                DebugTf(PSTR("\r\nDebug RestAPI: %s\r\n"), CBOOLEAN(bDebugRestAPI)); 
                break;
            case '3':   
                bDebugMQTT = !bDebugMQTT; 
                DebugTf(PSTR("\r\nDebug MQTT: %s\r\n"), CBOOLEAN(bDebugMQTT)); 
                break;
            case 'b':
                DebugTln(F("Blink led 1"));
                blinkLED(LED1, 5, 500);
                break;
            case 'i':
                DebugTln(F("relay init"));
                initOutputs();
                break;
            case 'u':
                DebugTln(F("gpio output on "));
                digitalWrite(settingGPIOOUTPUTSpin, ON);
                break;
            case 'j':
                DebugTf(PSTR("read gpio output state (0== led ON): %d \r\n"), digitalRead(settingGPIOOUTPUTSpin));
                break;
            case 'k':
                DebugTln(F("read settings"));
                readSettings(true);
                break;
            case 'o':
                DebugTln(PSTR("gpio output off"));
                digitalWrite(settingGPIOOUTPUTSpin, OFF);
                break;
            case 'l':
                DebugTln(PSTR("MyDEBUG =true"));
                settingMyDEBUG = true;
                break;
            case 'f':
                if(settingMyDEBUG)
                {
                    DebugTln(F("MyDEBUG = true"));
                }else{
                    DebugTln(F("MyDEBUG = false"));
                }
                break;
            default:
                break;
        }

    }
}
