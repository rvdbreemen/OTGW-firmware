void handleDebug(){
    if (TelnetStream.available()>0){
        //read the next 
        char c;
        c = TelnetStream.read();
        switch (c){
            case 'h':
                Debugln();
                Debugln(F("---===[ Debug Help Menu ]===---"));
                Debugf(PSTR("ESP Firmware: %s\r\n"), _VERSION);
                Debugf(PSTR("FS Hash match: %s\r\n"), CBOOLEAN(checklittlefshash()));
                Debugf(PSTR("PIC: %s | Type: %s | Version: %s\r\n"), sPICdeviceid, sPICtype, sPICfwversion);
                Debugln();
                Debugln(F("--- Status ---"));
                Debugf(PSTR("WiFi: %s | MQTT: %s | OTGW: %s\r\n"), 
                    (WiFi.status() == WL_CONNECTED) ? "Connected" : "Disconnected",
                    CBOOLEAN(statusMQTTconnection),
                    CBOOLEAN(bOTGWonline));
                Debugf(PSTR("Thermostat: %s | Boiler: %s | Gateway: %s\r\n"), 
                    CBOOLEAN(bOTGWthermostatstate),
                    CBOOLEAN(bOTGWboilerstate),
                    CBOOLEAN(bOTGWgatewaystate));
                Debugf(PSTR("CH Temp: %.1f°C | Room Temp: %.1f°C | Setpoint: %.1f°C\r\n"),
                    OTcurrentSystemState.Tboiler,
                    OTcurrentSystemState.Tr,
                    OTcurrentSystemState.TrSet);
                Debugln();
                Debugf(PSTR("1) Toggle debuglog - OT message parsing: %s\r\n"), CBOOLEAN(bDebugOTmsg));
                Debugf(PSTR("2) Toggle debuglog - API handeling: %s\r\n"), CBOOLEAN(bDebugRestAPI));
                Debugf(PSTR("3) Toggle debuglog - MQTT module: %s\r\n"), CBOOLEAN(bDebugMQTT));
                Debugf(PSTR("4) Toggle debuglog - Sensor modules: %s\r\n"), CBOOLEAN(bDebugSensors));
                Debugf(PSTR("d) Toggle debug helper - Dallas sensor simulation: %s\r\n"), CBOOLEAN(bDebugSensorSimulation));
                Debugln(F("--- Commands ---"));
                Debugln(F("q/k) Force read settings"));
                Debugln(F("m) Force MQTT discovery (seen messages only)"));
                Debugln(F("F) Force MQTT discovery for ALL message IDs"));
                Debugln(F("r) Reconnect wifi, telnet, otgwstream and mqtt"));
                Debugln(F("p) Reset PIC manually"));
                Debugln(F("a) Send PR=A command to ID PIC firmware version and type"));
                Debugln(F("--- GPIO/Debug ---"));
                Debugln(F("b) Blink LED 1 (5 times)"));
                Debugln(F("i) Initialize relay outputs"));
                Debugln(F("u) GPIO output ON"));
                Debugln(F("o) GPIO output OFF"));
                Debugln(F("j) Read GPIO output state"));
                Debugln(F("l) Set MyDEBUG = true"));
                Debugln(F("f) Show MyDEBUG status"));
                Debugln(F("d) Toggle Dallas sensor simulation"));
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
            case 'F':
                DebugTln(F("Force MQTT Discovery for ALL message IDs"));
                DebugTf(PSTR("Enable MQTT: %s\r\n"), CBOOLEAN(settingMQTTenable));
                doAutoConfigure(true);
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
            case 'd':
                bDebugSensorSimulation = !bDebugSensorSimulation;
                DebugTf(PSTR("\r\nDebug Dallas sensor simulation: %s\r\n"), CBOOLEAN(bDebugSensorSimulation));
                initSensors();
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
