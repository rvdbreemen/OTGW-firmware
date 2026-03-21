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
                Debugf(PSTR("PIC: %s | Type: %s | Version: %s\r\n"), state.pic.sDeviceid, state.pic.sType, state.pic.sFwversion);
                Debugln();
                Debugln(F("--- Status ---"));
                Debugf(PSTR("WiFi: %s | MQTT: %s | OTGW: %s\r\n"), 
                    (WiFi.status() == WL_CONNECTED) ? "Connected" : "Disconnected",
                    CBOOLEAN(state.mqtt.bConnected),
                    CBOOLEAN(state.otgw.bOnline));
                Debugf(PSTR("Thermostat: %s | Boiler: %s | Gateway Mode: %s\r\n"),
                    CCONOFF(state.otgw.bThermostatState),
                    CCONOFF(state.otgw.bBoilerState),
                    state.otgw.bGatewayModeKnown ? CCONOFF(state.otgw.bGatewayMode) : "detecting");
                Debugf(PSTR("OTGW Simulation: %s\r\n"), CBOOLEAN(state.debug.bOTGWSimulation));
                Debugf(PSTR("CH Temp: %.1f°C | Room Temp: %.1f°C | Setpoint: %.1f°C\r\n"),
                    OTcurrentSystemState.Tboiler,
                    OTcurrentSystemState.Tr,
                    OTcurrentSystemState.TrSet);
                Debugln();
                Debugf(PSTR("1) Toggle debuglog - OT message parsing: %s\r\n"), CBOOLEAN(state.debug.bOTmsg));
                Debugf(PSTR("2) Toggle debuglog - API handeling: %s\r\n"), CBOOLEAN(state.debug.bRestAPI));
                Debugf(PSTR("3) Toggle debuglog - MQTT module: %s\r\n"), CBOOLEAN(state.debug.bMQTT));
                Debugf(PSTR("4) Toggle debuglog - Sensor modules: %s\r\n"), CBOOLEAN(state.debug.bSensors));
                Debugf(PSTR("d) Toggle debug helper - Dallas sensor simulation: %s\r\n"), CBOOLEAN(state.debug.bSensorSim));
                Debugln(F("--- Commands ---"));
                Debugln(F("q/k) Force read settings"));
                Debugln(F("F) Force MQTT discovery for ALL message IDs"));
                Debugln(F("r) Reconnect wifi, telnet, otgwstream and mqtt"));
                Debugln(F("p) Reset PIC manually"));
                Debugln(F("a) Send PR=A command to ID PIC firmware version and type"));
                Debugln(F("s/S) Toggle OTGW serial simulation replay"));
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
                strlcpy(state.pic.sFwversion, OTGWSerial.firmwareVersion(), sizeof(state.pic.sFwversion));
                OTGWDebugTf(PSTR("Current firmware version: %s\r\n"), state.pic.sFwversion);
                strlcpy(state.pic.sDeviceid, OTGWSerial.processorToString().c_str(), sizeof(state.pic.sDeviceid));
                OTGWDebugTf(PSTR("Current device id: %s\r\n"), state.pic.sDeviceid);
                strlcpy(state.pic.sType, OTGWSerial.firmwareToString().c_str(), sizeof(state.pic.sType));
                OTGWDebugTf(PSTR("Current firmware type: %s\r\n"), state.pic.sType);
                break;
            case 'q':
                DebugTln(F("Read settings"));
                readSettings(true);
                break;
            case 'F':
                DebugTln(F("Force MQTT Discovery for ALL message IDs"));
                DebugTf(PSTR("Enable MQTT: %s\r\n"), CBOOLEAN(settings.mqtt.bEnable));
                doAutoConfigure();
                break;
            case 'r':   
                if (WiFi.status() != WL_CONNECTED)
                {
                    DebugTln(F("Reconnecting to wifi"));
                    startWiFi(CSTR(settings.sHostname), 240);
                    //check OTGW and telnet
                    startTelnet();
                    startOTGWstream(); 
                } else DebugTln(F("Wifi is connected"));
                    
                if (!state.mqtt.bConnected) {
                    DebugTln(F("Reconnecting MQTT"));
                    startMQTT();
                } else DebugTln(F("MQTT is connected"));
                break;
            case '1':   
                state.debug.bOTmsg = !state.debug.bOTmsg; 
                DebugTf(PSTR("\r\nDebug OTmsg: %s\r\n"), CBOOLEAN(state.debug.bOTmsg)); 
                break;
            case '2':   
                state.debug.bRestAPI = !state.debug.bRestAPI; 
                DebugTf(PSTR("\r\nDebug RestAPI: %s\r\n"), CBOOLEAN(state.debug.bRestAPI)); 
                break;
            case '3':   
                state.debug.bMQTT = !state.debug.bMQTT; 
                DebugTf(PSTR("\r\nDebug MQTT: %s\r\n"), CBOOLEAN(state.debug.bMQTT)); 
                break;
            case '4':   
                state.debug.bSensors = !state.debug.bSensors;
                DebugTf(PSTR("\r\nDebug Sensors: %s\r\n"), CBOOLEAN(state.debug.bSensors)); 
                break;
            case 'd':
                state.debug.bSensorSim = !state.debug.bSensorSim;
                DebugTf(PSTR("\r\nDebug Dallas sensor simulation: %s\r\n"), CBOOLEAN(state.debug.bSensorSim));
                initSensors();
                if (state.debug.bSensorSim)
                {
                  pollSensors();  // Force immediate sensor data so panel/graph update right away
                }
                break;
            case 's':
            case 'S':
                state.debug.bOTGWSimulation = !state.debug.bOTGWSimulation;
                state.debug.iOTGWSimulationNextDueMs = 0;
                DebugTf(PSTR("\r\nDebug OTGW serial simulation: %s\r\n"), CBOOLEAN(state.debug.bOTGWSimulation));
                if (state.debug.bOTGWSimulation) {
                    sendEventToWebSocket_P('*', PSTR("OTGW simulation enabled [replay active]"));
                } else {
                    sendEventToWebSocket_P('*', PSTR("OTGW simulation disabled [live serial resumed]"));
                }
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
                digitalWrite(settings.outputs.iPin, ON);
                break;
            case 'j':
                DebugTf(PSTR("read gpio output state (0== led ON): %d \r\n"), digitalRead(settings.outputs.iPin));
                break;
            case 'k':
                DebugTln(F("read settings"));
                readSettings(true);
                break;
            case 'o':
                DebugTln(F("gpio output off"));
                digitalWrite(settings.outputs.iPin, OFF);
                break;
            case 'l':
                DebugTln(F("MyDEBUG =true"));
                settings.bMyDEBUG = true;
                break;
            case 'f':
                if(settings.bMyDEBUG)
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
