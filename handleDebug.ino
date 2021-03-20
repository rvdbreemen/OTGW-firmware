void handleDebug(){
    if (TelnetStream.available()>0){
        //read the next 
        char c;
        c = TelnetStream.read();
        switch (c){
            case 'q':
                DebugTln("Read settings");
                readSettings(true);
                break;
            case 'm':
                DebugTln("Configure MQTT Discovery");
                DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                doAutoConfigure();
                break;
            case 'r':   
                if (WiFi.status() != WL_CONNECTED)
                {
                    DebugTln("Reconnecting to wifi");
                    startWiFi(CSTR(settingHostname), 240);
                    //check OTGW and telnet
                    startTelnet();
                    startOTGWstream(); 
                } else DebugTln("Wifi is connected");
                    
                if (!statusMQTTconnection) {
                    DebugTln("Reconnecting MQTT");
                    startMQTT();
                } else DebugTln("MQTT is connected");
                break;
            case '1':   bDebugOTmsg = !bDebugOTmsg; DebugTf("\r\nDebug OTmsg: %s\r\n", CBOOLEAN(bDebugOTmsg)); break;
            case '2':   bDebugRestAPI = !bDebugRestAPI; DebugTf("\r\nDebug RestAPI: %s\r\n", CBOOLEAN(bDebugRestAPI)); break;
            case '3':   bDebugMQTT = !bDebugMQTT; DebugTf("\r\nDebug MQTT: %s\r\n", CBOOLEAN(bDebugMQTT)); break;
            case 'b':
                DebugTln("Blink led 1");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                blinkLED(LED1, 5, 500);
            break;
            case 'i':
                DebugTln("relay init");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                initOutputs();
            break;
            case 'q':
                DebugTln("led 1 on ");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                digitalWrite(16,ON);
            break;
            case 's':
                DebugTln("read settings");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                readSettings(true);
            break;
            case 'w':
                DebugTln("led 1 off ");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                digitalWrite(16,OFF);
            break;
            default:
            break;
        }

    }
}
