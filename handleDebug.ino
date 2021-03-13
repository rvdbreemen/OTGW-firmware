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
            case 'u':
                DebugTln("gpio output on ");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                digitalWrite(settingGPIOOUTPUTSpin, ON);
                break;
            case 'j':
                DebugTf("read gpio output state (0== led ON): %d \r\n", digitalRead(settingGPIOOUTPUTSpin));
            break;
            case 'k':
                DebugTln("read settings");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                readSettings(true);
            break;
            case 'o':
                DebugTln("gpio output off");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                digitalWrite(settingGPIOOUTPUTSpin, OFF);
                break;
            case 'l':
                DebugTln("MyDEBUG =true");
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                settingMyDEBUG = true;
                break;
            case 'f':
                if(settingMyDEBUG)
                {
                    DebugTln("MyDEBUG = true");
                }else{
                    DebugTln("MyDEBUG = false");
                }
                // DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                break;
            default:
            break;
        }

    }
}
