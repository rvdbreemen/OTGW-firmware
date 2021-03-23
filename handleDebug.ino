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
            case '1':   bDebugOTmsg = !bDebugOTmsg; DebugTf("\r\nDebug OTmsg: %s\r\n", CBOOLEAN(bDebugOTmsg)); break;
            case '2':   bDebugRestAPI = !bDebugRestAPI; DebugTf("\r\nDebug RestAPI: %s\r\n", CBOOLEAN(bDebugRestAPI)); break;
            case '3':   bDebugMQTT = !bDebugMQTT; DebugTf("\r\nDebug MQTT: %s\r\n", CBOOLEAN(bDebugMQTT)); break;
            default:
            break;
        }

    }
}
