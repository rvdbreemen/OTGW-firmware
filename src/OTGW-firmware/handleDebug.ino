// Dispatch a single keypress from the telnet debug session.
// Called from onTelnetInput() in networkStuff.ino via the SimpleTelnet
// onInputReceived callback (line mode off — one char per call).

static void dumpDebugInfo() {
  Debugln(F("--- DUMP BEGIN ---"));
  // [build]
  Debugln(F("[build]"));
  Debugf(PSTR("version: %s\r\n"), _VERSION);
  Debugf(PSTR("build: %d\r\n"), _VERSION_BUILD);
  Debugf(PSTR("githash: %s\r\n"), _VERSION_GITHASH);
  Debugf(PSTR("date: %s\r\n"), _VERSION_DATE);
  // [runtime]
  Debugln(F("[runtime]"));
  Debugf(PSTR("heap.free: %u\r\n"), (unsigned)ESP.getFreeHeap());
  Debugf(PSTR("heap.frag: %u%%\r\n"), (unsigned)ESP.getHeapFragmentation());
  Debugf(PSTR("heap.minFree: %u\r\n"), (unsigned)getMinFreeHeap());
  Debugf(PSTR("heap.maxBlock: %u\r\n"), (unsigned)ESP.getMaxFreeBlockSize());
  Debugf(PSTR("uptime.seconds: %lu\r\n"), (unsigned long)state.uptime.iSeconds);
  Debugf(PSTR("uptime.reboots: %lu\r\n"), (unsigned long)state.uptime.iRebootCount);
  Debugf(PSTR("wifi.rssi: %d dBm\r\n"), WiFi.RSSI());
  Debugf(PSTR("wifi.ip: %s\r\n"), WiFi.localIP().toString().c_str());
  Debugf(PSTR("wifi.ssid: %s\r\n"), WiFi.SSID().c_str());
  Debugf(PSTR("wifi.connected: %s\r\n"), (WiFi.status() == WL_CONNECTED) ? "true" : "false");
  // [settings]
  Debugln(F("[settings]"));
  Debugf(PSTR("hostname: %s\r\n"), settings.sHostname);
  Debugf(PSTR("http_passwd: %s\r\n"), settings.sHTTPpasswd[0] ? "***" : "(not set)");
  Debugf(PSTR("led_blink: %s\r\n"), settings.bLEDblink ? "true" : "false");
  Debugf(PSTR("nightly_restart: %s\r\n"), settings.bNightlyRestart ? "true" : "false");
  Debugf(PSTR("restart_hour: %u\r\n"), (unsigned)settings.iRestartHour);
  // [settings.mqtt]
  Debugln(F("[settings.mqtt]"));
  Debugf(PSTR("enabled: %s\r\n"), settings.mqtt.bEnable ? "true" : "false");
  Debugf(PSTR("broker: %s\r\n"), settings.mqtt.sBroker);
  Debugf(PSTR("port: %d\r\n"), (int)settings.mqtt.iBrokerPort);
  Debugf(PSTR("user: %s\r\n"), settings.mqtt.sUser);
  Debugf(PSTR("passwd: %s\r\n"), settings.mqtt.sPasswd[0] ? "***" : "(not set)");
  Debugf(PSTR("ha_prefix: %s\r\n"), settings.mqtt.sHaprefix);
  Debugf(PSTR("ha_reboot_detect: %s\r\n"), settings.mqtt.bHaRebootDetect ? "true" : "false");
  Debugf(PSTR("top_topic: %s\r\n"), settings.mqtt.sTopTopic);
  Debugf(PSTR("unique_id: %s\r\n"), settings.mqtt.sUniqueid);
  Debugf(PSTR("ot_message: %s\r\n"), settings.mqtt.bOTmessage ? "true" : "false");
  Debugf(PSTR("interval: %u\r\n"), (unsigned)settings.mqtt.iInterval);
  Debugf(PSTR("separate_sources: %s\r\n"), settings.mqtt.bSeparateSources ? "true" : "false");
  Debugf(PSTR("disc_auto_verify: %s\r\n"), settings.mqtt.bDiscoveryAutoVerify ? "true" : "false");
  Debugf(PSTR("publish_ha_core_aliases: %s\r\n"), settings.mqtt.bPublishHaCoreAliases ? "true" : "false");
  // [settings.ntp]
  Debugln(F("[settings.ntp]"));
  Debugf(PSTR("enabled: %s\r\n"), settings.ntp.bEnable ? "true" : "false");
  Debugf(PSTR("timezone: %s\r\n"), settings.ntp.sTimezone);
  Debugf(PSTR("hostname: %s\r\n"), settings.ntp.sHostname);
  Debugf(PSTR("sendtime: %s\r\n"), settings.ntp.bSendtime ? "true" : "false");
  // [settings.sensors]
  Debugln(F("[settings.sensors]"));
  Debugf(PSTR("enabled: %s\r\n"), settings.sensors.bEnabled ? "true" : "false");
  Debugf(PSTR("pin: %d\r\n"), (int)settings.sensors.iPin);
  Debugf(PSTR("interval: %d\r\n"), (int)settings.sensors.iInterval);
  // [settings.s0]
  Debugln(F("[settings.s0]"));
  Debugf(PSTR("enabled: %s\r\n"), settings.s0.bEnabled ? "true" : "false");
  Debugf(PSTR("pin: %u\r\n"), (unsigned)settings.s0.iPin);
  Debugf(PSTR("debounce_ms: %u\r\n"), (unsigned)settings.s0.iDebounceTime);
  Debugf(PSTR("pulse_kw: %u\r\n"), (unsigned)settings.s0.iPulsekw);
  Debugf(PSTR("interval: %u\r\n"), (unsigned)settings.s0.iInterval);
  // [settings.outputs]
  Debugln(F("[settings.outputs]"));
  Debugf(PSTR("enabled: %s\r\n"), settings.outputs.bEnabled ? "true" : "false");
  Debugf(PSTR("pin: %d\r\n"), (int)settings.outputs.iPin);
  Debugf(PSTR("trigger_bit: %d\r\n"), (int)settings.outputs.iTriggerBit);
  // [settings.otgw]
  Debugln(F("[settings.otgw]"));
  Debugf(PSTR("boot_cmd_enable: %s\r\n"), settings.otgw.bEnable ? "true" : "false");
  Debugf(PSTR("boot_commands: %s\r\n"), settings.otgw.sCommands);
  // [state.otgw]
  Debugln(F("[state.otgw]"));
  Debugf(PSTR("online: %s\r\n"), state.otgw.bOnline ? "true" : "false");
  Debugf(PSTR("ps_mode: %s\r\n"), state.otgw.bPSmode ? "true" : "false");
  Debugf(PSTR("gateway_mode: %s\r\n"), state.otgw.bGatewayMode ? "true" : "false");
  Debugf(PSTR("gateway_mode_known: %s\r\n"), state.otgw.bGatewayModeKnown ? "true" : "false");
  Debugf(PSTR("boiler_state: %s\r\n"), state.otgw.bBoilerState ? "true" : "false");
  Debugf(PSTR("thermostat_state: %s\r\n"), state.otgw.bThermostatState ? "true" : "false");
  // [state.mqtt]
  Debugln(F("[state.mqtt]"));
  Debugf(PSTR("connected: %s\r\n"), state.mqtt.bConnected ? "true" : "false");
  // [state.pic]
  Debugln(F("[state.pic]"));
  Debugf(PSTR("available: %s\r\n"), state.pic.bAvailable ? "true" : "false");
  Debugf(PSTR("device_id: %s\r\n"), state.pic.sDeviceid);
  Debugf(PSTR("type: %s\r\n"), state.pic.sType);
  Debugf(PSTR("fw_version: %s\r\n"), state.pic.sFwversion);
  // [state.debug]
  Debugln(F("[state.debug]"));
  Debugf(PSTR("ot_msg: %s\r\n"), state.debug.bOTmsg ? "true" : "false");
  Debugf(PSTR("rest_api: %s\r\n"), state.debug.bRestAPI ? "true" : "false");
  Debugf(PSTR("mqtt: %s\r\n"), state.debug.bMQTT ? "true" : "false");
  Debugf(PSTR("mqtt_gate: %s\r\n"), state.debug.bMQTTGate ? "true" : "false");
  Debugf(PSTR("sensors: %s\r\n"), state.debug.bSensors ? "true" : "false");
  Debugf(PSTR("ntp: %s\r\n"), state.debug.bNTP ? "true" : "false");
  Debugf(PSTR("sensor_sim: %s\r\n"), state.debug.bSensorSim ? "true" : "false");
  Debugf(PSTR("otgw_sim: %s\r\n"), state.debug.bOTGWSimulation ? "true" : "false");
  // [state.uptime]
  Debugln(F("[state.uptime]"));
  Debugf(PSTR("seconds: %lu\r\n"), (unsigned long)state.uptime.iSeconds);
  Debugf(PSTR("reboots: %lu\r\n"), (unsigned long)state.uptime.iRebootCount);
  // [state.heapdiag]
  Debugln(F("[state.heapdiag]"));
  Debugf(PSTR("ws_drops: %lu\r\n"), (unsigned long)state.heapdiag.iWsDropsTotal);
  Debugf(PSTR("mqtt_drops: %lu\r\n"), (unsigned long)state.heapdiag.iMqttDropsTotal);
  Debugf(PSTR("entered_low: %lu\r\n"), (unsigned long)state.heapdiag.iEnteredLowCount);
  Debugf(PSTR("entered_warning: %lu\r\n"), (unsigned long)state.heapdiag.iEnteredWarningCount);
  Debugf(PSTR("entered_critical: %lu\r\n"), (unsigned long)state.heapdiag.iEnteredCriticalCount);
  Debugf(PSTR("drip_slow_mode: %lu\r\n"), (unsigned long)state.heapdiag.iDripSlowModeCount);
  Debugln(F("--- DUMP END ---"));
}

void handleDebugChar(char c){
        switch (c){
            case 'h':
                Debugln();
                Debugln(F("---===[ Debug Commands ]===---"));
                Debugln(F("Toggle keys (current state shown in welcome banner):"));
                Debugln(F("  1) OT message parsing       2) REST API handling"));
                Debugln(F("  3) MQTT communication       4) MQTT interval gating"));
                Debugln(F("  5) Sensor modules           6) NTP time sync"));
                Debugln(F("  d) Dallas-sensor simulation"));
                Debugln();
                Debugln(F("--- Actions ---"));
                Debugln(F("  D) Dump full debug info (settings + state, INI format)"));
                Debugln(F("  q) Force read settings"));
                Debugln(F("  F) Force MQTT discovery for ALL message IDs"));
                Debugln(F("  r) Reconnect WiFi & refresh MQTT/WS clients"));
                Debugln(F("  p) Reset PIC manually"));
                Debugln(F("  a) Send PR=A to identify PIC firmware version & type"));
                Debugln(F("  s/S) Toggle OTGW serial-simulation replay"));
                Debugln(F("--- GPIO / Misc ---"));
                Debugln(F("  b) Blink LED 1 (5x)"));
                Debugln(F("  i) Initialize relay outputs"));
                Debugln(F("  u) GPIO output ON"));
                Debugln(F("  o) GPIO output OFF"));
                Debugln(F("  j) Read GPIO output state"));
                Debugln(F("  l) Toggle MyDEBUG"));
                Debugln(F("  f) Show MyDEBUG status"));
                Debugln();
                break;
            case 'D':
                dumpDebugInfo();
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
            case 'V':
                // ADR-062 / TASK-349: trigger on-demand retained-discovery verification
                DebugTln(F("Trigger MQTT discovery verification"));
                if (!startDiscoveryVerification()) {
                    DebugTln(F("[verify] refused (MQTT? pending drip? heap? flashing? already active?)"));
                }
                break;
            case 'r':
                if (WiFi.status() != WL_CONNECTED)
                {
                    DebugTln(F("Reconnecting to wifi"));
                    startWiFi(CSTR(settings.sHostname), 240);
                    refreshServicesAfterWifiReconnect();
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
                state.debug.bMQTTGate = !state.debug.bMQTTGate;
                DebugTf(PSTR("\r\nDebug MQTT Gating: %s\r\n"), CBOOLEAN(state.debug.bMQTTGate));
                break;
            case '5':
                state.debug.bSensors = !state.debug.bSensors;
                DebugTf(PSTR("\r\nDebug Sensors: %s\r\n"), CBOOLEAN(state.debug.bSensors));
                break;
            case '6':
                state.debug.bNTP = !state.debug.bNTP;
                DebugTf(PSTR("\r\nDebug NTP: %s\r\n"), CBOOLEAN(state.debug.bNTP));
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
            case 'o':
                DebugTln(F("gpio output off"));
                digitalWrite(settings.outputs.iPin, OFF);
                break;
            case 'l':
                settings.bMyDEBUG = !settings.bMyDEBUG;
                DebugTf(PSTR("\r\nMyDEBUG: %s\r\n"), CBOOLEAN(settings.bMyDEBUG));
                break;
            case 'f':
                DebugTf(PSTR("MyDEBUG: %s\r\n"), CBOOLEAN(settings.bMyDEBUG));
                break;
            default:
                break;
        }
}

// Called from doBackgroundTasks() — no-op now that input is handled via
// the SimpleTelnet onInputReceived callback registered in startTelnet().
void handleDebug(){}
