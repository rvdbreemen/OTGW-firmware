void handleDebug(){
    if (TelnetStream.available()>0){
        //read the next 
        char c;
        c = TelnetStream.read();
        switch (c){
            case 'h':
                Debugln();
                Debugln(F("---===[ Debug Help Menu ]===---"));
                Debugf(PSTR("1) Toggle debuglog - OT message parsing: %s\r\n"), CBOOLEAN(bDebugOTmsg));
                Debugf(PSTR("2) Toggle debuglog - API handeling: %s\r\n"), CBOOLEAN(bDebugRestAPI));
                Debugf(PSTR("3) Toggle debuglog - MQTT module: %s\r\n"), CBOOLEAN(bDebugMQTT));
                Debugf(PSTR("4) Toggle debuglog - Sensor modules: %s\r\n"), CBOOLEAN(bDebugSensors));
                Debugln(F("q) Force read settings"));
                Debugln(F("m) Force MQTT discovery"));
                Debugln(F("r) Reconnect wifi, telnet, otgwstream and mqtt"));
                Debugln(F("p) Reset PIC manually"));
                Debugln(F("a) Send PR=A command to ID PIC firmware version and type"));
                Debugln();
                break;
            case 'p':
                DebugTln(F("Manual reset PIC"));
                detectPIC();
                break;
            case 'a':
                DebugTln(F("Send PR=A command, to ID the chip"));
                getpicfwversion();
                DebugTln(F("Debug --> PR=A report firmware version, type"));
                strlcpy(sPICfwversion, OTGWSerial.firmwareVersion(), sizeof(sPICfwversion));
                OTGWDebugTf(PSTR("Current firmware version: %s\r\n"), sPICfwversion);
                strlcpy(sPICdeviceid, OTGWSerial.processorToString().c_str(), sizeof(sPICdeviceid));
                OTGWDebugTf(PSTR("Current device id: %s\r\n"), sPICdeviceid);
                strlcpy(sPICtype, OTGWSerial.firmwareToString().c_str(), sizeof(sPICtype));
                OTGWDebugTf(PSTR("Current firmware type: %s\r\n"), sPICtype);
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
            case '4':   
                bDebugSensors = !bDebugSensors;
                DebugTf(PSTR("\r\nDebug Sensors: %s\r\n"), CBOOLEAN(bDebugSensors)); 
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
                DebugTln(F("gpio output off"));
                digitalWrite(settingGPIOOUTPUTSpin, OFF);
                break;
            case 'l':
                DebugTln(F("MyDEBUG =true"));
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
