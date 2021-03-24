void handleDebug(){
    if (TelnetStream.available()>0){
        //read the next 
        char c;
        c = TelnetStream.read();
        switch (c){
            case 'm':
                DebugTln("Configure MQTT Discovery");
                DebugTf("Enable MQTT: %s", CBOOLEAN(settingMQTTenable));
                doAutoConfigure();
                break;
            case 'r':   
                if (WiFi.status() != WL_CONNECTED)
                {
                    DebugTln("Reconnecting to wifi");
                    startWiFi(_HOSTNAME, 240);
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
            default:
            break;
        }

    }
}
