// Forward declaration — defined in SATweather.ino.
void weatherFetchOpenMeteoNow();

static void formatLocalIp(char* buf, size_t bufSize)
{
    IPAddress ip = WiFi.localIP();
    snprintf_P(buf, bufSize, PSTR("%u.%u.%u.%u"), ip[0], ip[1], ip[2], ip[3]);
}

static void dumpDebugInfo() {
    char ipBuf[16];
    char ssidBuf[33];
    formatLocalIp(ipBuf, sizeof(ipBuf));
    strlcpy(ssidBuf, WiFi.SSID().c_str(), sizeof(ssidBuf));

    Debugln(F("--- DUMP BEGIN ---"));

    Debugln(F("[build]"));
    Debugf(PSTR("version: %s\r\n"), _VERSION);
    Debugf(PSTR("build: %d\r\n"), _VERSION_BUILD);
    Debugf(PSTR("githash: %s\r\n"), _VERSION_GITHASH);
    Debugf(PSTR("date: %s\r\n"), _VERSION_DATE);

    Debugln(F("[runtime]"));
    Debugf(PSTR("heap.free: %u\r\n"), (unsigned)platformFreeHeap());
    Debugf(PSTR("heap.frag: %u%%\r\n"), (unsigned)platformHeapFragmentation());
    Debugf(PSTR("heap.min_free: %u\r\n"), (unsigned)platformMinFreeHeap());
    Debugf(PSTR("heap.max_alloc: %u\r\n"), (unsigned)platformMaxFreeBlock());
    Debugf(PSTR("uptime.seconds: %lu\r\n"), (unsigned long)state.uptime.iSeconds);
    Debugf(PSTR("uptime.reboots: %lu\r\n"), (unsigned long)state.uptime.iRebootCount);
    Debugf(PSTR("wifi.connected: %s\r\n"), (WiFi.status() == WL_CONNECTED) ? "true" : "false");
    Debugf(PSTR("wifi.rssi: %d\r\n"), WiFi.RSSI());
    Debugf(PSTR("wifi.ip: %s\r\n"), ipBuf);
    Debugf(PSTR("wifi.ssid: %s\r\n"), ssidBuf);
    Debugf(PSTR("hardware.mode: %S\r\n"), (PGM_P)hardwareModeName());
    Debugf(PSTR("network.mode: %S\r\n"), (PGM_P)networkModeName());

    Debugln(F("[settings]"));
    Debugf(PSTR("hostname: %s\r\n"), settings.sHostname);
    Debugf(PSTR("http_passwd: %s\r\n"), settings.sHTTPpasswd[0] ? "***" : "(not set)");
    Debugf(PSTR("led_blink: %s\r\n"), settings.bLEDblink ? "true" : "false");
    Debugf(PSTR("dark_theme: %s\r\n"), settings.bDarkTheme ? "true" : "false");
    Debugf(PSTR("mydebug: %s\r\n"), settings.bMyDEBUG ? "true" : "false");
    Debugf(PSTR("nightly_restart: %s\r\n"), settings.bNightlyRestart ? "true" : "false");
    Debugf(PSTR("restart_hour: %u\r\n"), (unsigned)settings.iRestartHour);

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
    Debugf(PSTR("use_legacy_ot_topics: %s\r\n"), settings.mqtt.bUseLegacyOtTopics ? "true" : "false");
    Debugf(PSTR("legacy_port25238: %s\r\n"), settings.mqtt.bLegacyPort25238Enabled ? "true" : "false");

    Debugln(F("[settings.ntp]"));
    Debugf(PSTR("enabled: %s\r\n"), settings.ntp.bEnable ? "true" : "false");
    Debugf(PSTR("timezone: %s\r\n"), settings.ntp.sTimezone);
    Debugf(PSTR("hostname: %s\r\n"), settings.ntp.sHostname);
    Debugf(PSTR("sendtime: %s\r\n"), settings.ntp.bSendtime ? "true" : "false");

    Debugln(F("[settings.sensors]"));
    Debugf(PSTR("enabled: %s\r\n"), settings.sensors.bEnabled ? "true" : "false");
    Debugf(PSTR("pin: %d\r\n"), (int)settings.sensors.iPin);
    Debugf(PSTR("interval: %d\r\n"), (int)settings.sensors.iInterval);

    Debugln(F("[settings.s0]"));
    Debugf(PSTR("enabled: %s\r\n"), settings.s0.bEnabled ? "true" : "false");
    Debugf(PSTR("pin: %u\r\n"), (unsigned)settings.s0.iPin);
    Debugf(PSTR("debounce_ms: %u\r\n"), (unsigned)settings.s0.iDebounceTime);
    Debugf(PSTR("pulse_kw: %u\r\n"), (unsigned)settings.s0.iPulsekw);
    Debugf(PSTR("interval: %u\r\n"), (unsigned)settings.s0.iInterval);

    Debugln(F("[settings.outputs]"));
    Debugf(PSTR("enabled: %s\r\n"), settings.outputs.bEnabled ? "true" : "false");
    Debugf(PSTR("pin: %d\r\n"), (int)settings.outputs.iPin);
    Debugf(PSTR("trigger_bit: %d\r\n"), (int)settings.outputs.iTriggerBit);

    Debugln(F("[settings.sat]"));
    Debugf(PSTR("enabled: %s\r\n"), settings.sat.bEnabled ? "true" : "false");
    Debugf(PSTR("heating_system: %u\r\n"), (unsigned)settings.sat.iHeatingSystem);
    Debugf(PSTR("heating_source: %u\r\n"), (unsigned)settings.sat.iHeatingSource);
    Debugf(PSTR("hp_cycle_seconds: %u\r\n"), (unsigned)settings.sat.iHpCycleSeconds);
    Debugf(PSTR("target_temp: %.1f\r\n"), settings.sat.fTargetTemp);
    Debugf(PSTR("curve_coeff: %.2f\r\n"), settings.sat.fHeatingCurveCoeff);
    Debugf(PSTR("deadband: %.2f\r\n"), settings.sat.fDeadband);
    Debugf(PSTR("control_interval: %u\r\n"), (unsigned)settings.sat.iControlInterval);
    Debugf(PSTR("use_external_temp: %s\r\n"), settings.sat.bUseExternalTemp ? "true" : "false");
    Debugf(PSTR("pwm_auto_switch: %s\r\n"), settings.sat.bPwmAutoSwitch ? "true" : "false");
    Debugf(PSTR("max_rel_mod: %u\r\n"), (unsigned)settings.sat.iMaxRelModulation);
    Debugf(PSTR("overshoot_margin: %.1f\r\n"), settings.sat.fOvershootMargin);
    Debugf(PSTR("dhw_setpoint: %.1f\r\n"), settings.sat.fDhwSetpoint);
    Debugf(PSTR("dhw_enabled: %s\r\n"), settings.sat.bDhwEnabled ? "true" : "false");
    Debugf(PSTR("dhw_enable: %s\r\n"), settings.sat.bDhwEnable ? "true" : "false");
    Debugf(PSTR("window_detection: %s\r\n"), settings.sat.bWindowDetection ? "true" : "false");
    Debugf(PSTR("weather_enable: %s\r\n"), settings.sat.bWeatherEnable ? "true" : "false");
    Debugf(PSTR("weather_key: %s\r\n"), settings.sat.sWeatherApiKey[0] ? "***" : "(not set)");
    Debugf(PSTR("boiler_capacity: %.1f\r\n"), settings.sat.fBoilerCapacity);
    Debugf(PSTR("simulation: %s\r\n"), settings.sat.bSimulation ? "true" : "false");
    Debugf(PSTR("solar_gain_enable: %s\r\n"), settings.sat.bSolarGainEnable ? "true" : "false");
    Debugf(PSTR("summer_simmer: %s\r\n"), settings.sat.bSummerSimmer ? "true" : "false");
    Debugf(PSTR("comfort_adjust: %s\r\n"), settings.sat.bComfortAdjust ? "true" : "false");
    Debugf(PSTR("multi_area: %s\r\n"), settings.sat.bMultiArea ? "true" : "false");
    Debugf(PSTR("auto_tune: %s\r\n"), settings.sat.bAutoTune ? "true" : "false");
    Debugf(PSTR("max_setpoint: %.1f\r\n"), settings.sat.fMaxSetpoint);
    Debugf(PSTR("sensor_max_age_s: %lu\r\n"), (unsigned long)settings.sat.iSensorMaxAgeS);
    Debugf(PSTR("error_monitoring: %s\r\n"), settings.sat.bErrorMonitoring ? "true" : "false");
    Debugf(PSTR("auto_gains: %s\r\n"), settings.sat.bAutoGains ? "true" : "false");
    Debugf(PSTR("thermal_comfort: %s\r\n"), settings.sat.bThermalComfort ? "true" : "false");
    Debugf(PSTR("humidity_timeout_s: %u\r\n"), (unsigned)settings.sat.iHumidityTimeoutS);
    Debugf(PSTR("heating_mode: %u\r\n"), (unsigned)settings.sat.iHeatingMode);
    Debugf(PSTR("cycles_per_hour: %u\r\n"), (unsigned)settings.sat.iCyclesPerHour);
    Debugf(PSTR("valve_offset: %.2f\r\n"), settings.sat.fValveOffset);
    Debugf(PSTR("solar_freeze_integral: %s\r\n"), settings.sat.bSolarFreezeIntegral ? "true" : "false");
    Debugf(PSTR("flush_threshold_h: %u\r\n"), (unsigned)settings.sat.iSatFlushThresholdH);
    Debugf(PSTR("zone_count: %u\r\n"), (unsigned)settings.sat.iZoneCount);
    Debugf(PSTR("zone_timeout_s: %u\r\n"), (unsigned)settings.sat.iZoneTimeoutS);

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
    Debugln(F("[settings.otd]"));
    Debugf(PSTR("mode: %u\r\n"), (unsigned)settings.otd.iMode);
    Debugf(PSTR("auto_detect: %s\r\n"), settings.otd.bAutoDetect ? "true" : "false");
    Debugf(PSTR("setback_temp: %.1f\r\n"), settings.otd.fSetbackTemp);
    Debugf(PSTR("setback_timeout: %u\r\n"), (unsigned)settings.otd.iSetbackTimeout);
    Debugf(PSTR("enable_slave: %s\r\n"), settings.otd.bEnableSlave ? "true" : "false");
    Debugf(PSTR("summer_mode: %s\r\n"), settings.otd.bSummerMode ? "true" : "false");
    Debugf(PSTR("fail_safe: %s\r\n"), settings.otd.bFailSafe ? "true" : "false");
    Debugf(PSTR("msg_interval: %u\r\n"), (unsigned)settings.otd.iMsgInterval);
    Debugf(PSTR("has_bypass_relay: %s\r\n"), settings.otd.bHasBypassRelay ? "true" : "false");
    Debugf(PSTR("ch_mode: %u\r\n"), (unsigned)settings.otd.iCHMode);
    Debugf(PSTR("flow_temp: %.1f\r\n"), settings.otd.fFlowTemp);
    Debugf(PSTR("flow_max: %.1f\r\n"), settings.otd.fFlowMax);
    Debugf(PSTR("room_setpoint: %.1f\r\n"), settings.otd.fRoomSetpoint);
    Debugf(PSTR("gradient: %.2f\r\n"), settings.otd.fGradient);
    Debugf(PSTR("exponent: %.2f\r\n"), settings.otd.fExponent);
    Debugf(PSTR("offset: %.2f\r\n"), settings.otd.fOffset);
    Debugf(PSTR("room_comp: %s\r\n"), settings.otd.bRoomCompEnabled ? "true" : "false");
    Debugf(PSTR("kp: %.2f\r\n"), settings.otd.fKp);
    Debugf(PSTR("ki: %.2f\r\n"), settings.otd.fKi);
    Debugf(PSTR("kboost: %.2f\r\n"), settings.otd.fKboost);
#endif

    Debugln(F("[state.mqtt]"));
    Debugf(PSTR("connected: %s\r\n"), state.mqtt.bConnected ? "true" : "false");

    Debugln(F("[state.pic]"));
    Debugf(PSTR("available: %s\r\n"), state.pic.bAvailable ? "true" : "false");
    Debugf(PSTR("device_id: %s\r\n"), state.pic.sDeviceid);
    Debugf(PSTR("type: %s\r\n"), state.pic.sType);
    Debugf(PSTR("fw_version: %s\r\n"), state.pic.sFwversion);

    Debugln(F("[state.otbus]"));
    Debugf(PSTR("online: %s\r\n"), state.otBus.bOnline ? "true" : "false");
    Debugf(PSTR("gateway_mode: %s\r\n"), state.otBus.bGatewayMode ? "true" : "false");
    Debugf(PSTR("gateway_known: %s\r\n"), state.otBus.bGatewayModeKnown ? "true" : "false");
    Debugf(PSTR("boiler_state: %s\r\n"), state.otBus.bBoilerState ? "true" : "false");
    Debugf(PSTR("thermostat_state: %s\r\n"), state.otBus.bThermostatState ? "true" : "false");
    Debugf(PSTR("ps_mode: %s\r\n"), state.otBus.bPSmode ? "true" : "false");

    Debugln(F("[state.debug]"));
    Debugf(PSTR("ot_msg: %s\r\n"), state.debug.bOTmsg ? "true" : "false");
    Debugf(PSTR("rest_api: %s\r\n"), state.debug.bRestAPI ? "true" : "false");
    Debugf(PSTR("mqtt: %s\r\n"), state.debug.bMQTT ? "true" : "false");
    Debugf(PSTR("mqtt_gate: %s\r\n"), state.debug.bMQTTGate ? "true" : "false");
    Debugf(PSTR("sensors: %s\r\n"), state.debug.bSensors ? "true" : "false");
    Debugf(PSTR("ntp: %s\r\n"), state.debug.bNTP ? "true" : "false");
    Debugf(PSTR("sensor_sim: %s\r\n"), state.debug.bSensorSim ? "true" : "false");
    Debugf(PSTR("otgw_sim: %s\r\n"), state.debug.bOTGWSimulation ? "true" : "false");
    Debugf(PSTR("sat: %s\r\n"), state.debug.bSAT ? "true" : "false");
    Debugf(PSTR("otdirect: %s\r\n"), state.debug.bOTDirect ? "true" : "false");
#if HAS_SAT_BLE
    Debugf(PSTR("sat_ble: %s\r\n"), state.debug.bSATBLE ? "true" : "false");
#endif

    Debugln(F("[state.heapdiag]"));
    Debugf(PSTR("ws_drops: %lu\r\n"), (unsigned long)state.heapdiag.iWsDropsTotal);
    Debugf(PSTR("mqtt_drops: %lu\r\n"), (unsigned long)state.heapdiag.iMqttDropsTotal);
    Debugf(PSTR("entered_low: %lu\r\n"), (unsigned long)state.heapdiag.iEnteredLowCount);
    Debugf(PSTR("entered_warning: %lu\r\n"), (unsigned long)state.heapdiag.iEnteredWarningCount);
    Debugf(PSTR("entered_critical: %lu\r\n"), (unsigned long)state.heapdiag.iEnteredCriticalCount);
    Debugf(PSTR("drip_slow_mode: %lu\r\n"), (unsigned long)state.heapdiag.iDripSlowModeCount);
    Debugf(PSTR("max_loop_gap_ms: %lu\r\n"), (unsigned long)state.heapdiag.iMaxLoopGapMs);
    Debugf(PSTR("min_max_block: %lu\r\n"), (unsigned long)state.heapdiag.iMinMaxBlock);
    Debugf(PSTR("min_free_heap: %lu\r\n"), (unsigned long)getMinFreeHeap());
    Debugf(PSTR("maxblock_hist <2k/<4k/<8k/<16k/>=16k: %lu/%lu/%lu/%lu/%lu\r\n"),
      (unsigned long)state.heapdiag.aMaxBlockBucket[0],
      (unsigned long)state.heapdiag.aMaxBlockBucket[1],
      (unsigned long)state.heapdiag.aMaxBlockBucket[2],
      (unsigned long)state.heapdiag.aMaxBlockBucket[3],
      (unsigned long)state.heapdiag.aMaxBlockBucket[4]);
    // TASK-1017 load-test instrumentation (resettable via telnet 'z'):
    Debugf(PSTR("rest_inflight_hwm: %u\r\n"), (unsigned)state.heapdiag.iRestInflightHwm);
    Debugf(PSTR("webfile_inflight_hwm: %u\r\n"), (unsigned)state.heapdiag.iWebfileInflightHwm);
    Debugf(PSTR("tcp_active_pcbs: %u\r\n"), (unsigned)state.heapdiag.iTcpActivePcbs);
    Debugf(PSTR("rest_503: %lu\r\n"), (unsigned long)state.heapdiag.iRest503Count);
    Debugf(PSTR("webfile_503: %lu\r\n"), (unsigned long)state.heapdiag.iWebfile503Count);

    Debugln(F("[state.discovery]"));
    Debugf(PSTR("published_topics: %lu\r\n"), (unsigned long)state.discovery.iPublishedTopicCount);
    Debugf(PSTR("verify_runs: %lu\r\n"), (unsigned long)state.discovery.iVerifyRunCount);
    Debugf(PSTR("republish_triggered: %lu\r\n"), (unsigned long)state.discovery.iRepublishTriggeredCount);
    Debugf(PSTR("last_missing: %u\r\n"), (unsigned)state.discovery.iLastMissingCount);
    Debugf(PSTR("last_orphan: %u\r\n"), (unsigned)state.discovery.iLastOrphanCount);
    Debugf(PSTR("last_verify_epoch: %lu\r\n"), (unsigned long)state.discovery.iLastVerifyEpoch);

    Debugln(F("[state.sat]"));
    { char boilerStatus[20]; satGetBoilerStatusName(boilerStatus, sizeof(boilerStatus));
      Debugf(PSTR("boiler_status: %s\r\n"), boilerStatus); }
    { char manufacturer[12]; satGetManufacturerName(manufacturer, sizeof(manufacturer));
      Debugf(PSTR("manufacturer: %s\r\n"), manufacturer); }
    Debugf(PSTR("active: %s\r\n"), state.sat.bActive ? "true" : "false");
    Debugf(PSTR("control_mode: %d\r\n"), (int)state.sat.eControlMode);
    Debugf(PSTR("room_temp: %.1f\r\n"), satGetRoomTemp());
    Debugf(PSTR("outside_temp: %.1f\r\n"), satGetOutsideTemp());
    Debugf(PSTR("heating_curve: %.1f\r\n"), state.sat.fHeatingCurveValue);
    Debugf(PSTR("pid_output: %.1f\r\n"), state.sat.fPidOutput);
    Debugf(PSTR("final_setpoint: %.1f\r\n"), state.sat.fFinalSetpoint);
    Debugf(PSTR("error: %.2f\r\n"), state.sat.fError);
    Debugf(PSTR("pid_p: %.2f\r\n"), state.sat.fPidP);
    Debugf(PSTR("pid_i: %.2f\r\n"), state.sat.fPidI);
    Debugf(PSTR("pid_d: %.2f\r\n"), state.sat.fPidD);
    Debugf(PSTR("cycle_count: %u\r\n"), (unsigned)state.sat.iCycleCount);
    Debugf(PSTR("last_cycle_class: %d\r\n"), (int)state.sat.eLastCycleClass);
    Debugf(PSTR("duty_ratio: %.3f\r\n"), state.sat.fDutyRatio);
    Debugf(PSTR("pwm_duty: %.2f\r\n"), state.sat.fPwmDutyCycle);
    Debugf(PSTR("active_preset: %d\r\n"), (int)state.sat.eActivePreset);
    Debugf(PSTR("mod_suppressed: %s\r\n"), state.sat.bModSuppressed ? "true" : "false");
    Debugf(PSTR("dhw_active: %s\r\n"), state.sat.bDhwActive ? "true" : "false");
    Debugf(PSTR("fallback_active: %s\r\n"), state.sat.bFallbackActive ? "true" : "false");
    Debugf(PSTR("fallback_reason: %d\r\n"), (int)state.sat.eFallbackReason);
    Debugf(PSTR("current_modulation: %d\r\n"), (int)state.sat.iCurrentModulation);
    Debugf(PSTR("detected_source: %d\r\n"), (int)state.sat.iDetectedHeatingSource);
    Debugf(PSTR("weather_valid: %s\r\n"), state.sat.weather.bValid ? "true" : "false");
    Debugf(PSTR("weather_temp: %.1f\r\n"), state.sat.weather.fTemperature);
    Debugf(PSTR("weather_humidity: %.0f\r\n"), state.sat.weather.fHumidity);
    Debugf(PSTR("weather_wind: %.1f\r\n"), state.sat.weather.fWindSpeed);
    Debugf(PSTR("weather_cloud: %.0f\r\n"), state.sat.weather.fCloudCover);
#if HAS_WEATHER_FORECAST
    Debugf(PSTR("weather_pressure: %.1f\r\n"), state.sat.weather.fPressureMsl);
    Debugf(PSTR("weather_is_day: %s\r\n"), state.sat.weather.bIsDay ? "true" : "false");
#endif
    Debugf(PSTR("current_power_kw: %.2f\r\n"), state.sat.fCurrentPower);
    Debugf(PSTR("energy_total_kwh: %.2f\r\n"), state.sat.fEnergyTotal);
    Debugf(PSTR("energy_est_kwh: %.2f\r\n"), state.sat.fEnergyEstimatedKWh);
    Debugf(PSTR("thermal_valid: %s\r\n"), state.sat.bThermalModelValid ? "true" : "false");
    Debugf(PSTR("thermal_drop_rate: %.3f\r\n"), state.sat.fThermalDropRate);
    Debugf(PSTR("solar_gain_active: %s\r\n"), state.sat.bSolarGainActive ? "true" : "false");
    Debugf(PSTR("summer_active: %s\r\n"), state.sat.bSummerActive ? "true" : "false");
    Debugf(PSTR("humidity: %.1f\r\n"), state.sat.fHumidity);
    Debugf(PSTR("humidity_valid: %s\r\n"), state.sat.bHumidityValid ? "true" : "false");
    Debugf(PSTR("comfort_offset: %.2f\r\n"), state.sat.fComfortOffset);
    Debugf(PSTR("area0_temp: %.1f\r\n"), state.sat.fAreaTemp[0]);
    Debugf(PSTR("area1_temp: %.1f\r\n"), state.sat.fAreaTemp[1]);
    Debugf(PSTR("area2_temp: %.1f\r\n"), state.sat.fAreaTemp[2]);
    Debugf(PSTR("area3_temp: %.1f\r\n"), state.sat.fAreaTemp[3]);
    Debugf(PSTR("auto_tune_active: %s\r\n"), state.sat.bAutoTuneActive ? "true" : "false");
    Debugf(PSTR("auto_tune_cycles: %u\r\n"), (unsigned)state.sat.iAutoTuneCycles);
    Debugf(PSTR("auto_tune_score: %.3f\r\n"), state.sat.fAutoTuneScore);
#if HAS_SAT_BLE
    Debugf(PSTR("ble_temp: %.2f\r\n"), state.sat.fBleTemp);
    Debugf(PSTR("ble_humidity: %.1f\r\n"), state.sat.fBleHumidity);
    Debugf(PSTR("ble_valid: %s\r\n"), state.sat.bBleTempValid ? "true" : "false");
    Debugf(PSTR("ble_count: %u\r\n"), (unsigned)state.sat.iBleSensorCount);
    Debugf(PSTR("ble_battery: %u\r\n"), (unsigned)state.sat.iBleBattery);
    Debugf(PSTR("ble_rssi: %d\r\n"), (int)state.sat.iBleRssi);
#endif

#if defined(HAS_DIRECT_OT) && HAS_DIRECT_OT
    Debugln(F("[state.otd]"));
    Debugf(PSTR("schedule_total: %u\r\n"), (unsigned)state.otd.iScheduleTotal);
    Debugf(PSTR("schedule_active: %u\r\n"), (unsigned)state.otd.iScheduleActive);
    Debugf(PSTR("schedule_disabled: %u\r\n"), (unsigned)state.otd.iScheduleDisabled);
    Debugf(PSTR("override_count: %u\r\n"), (unsigned)state.otd.iOverrideCount);
    Debugf(PSTR("bypass_active: %s\r\n"), state.otd.bBypassActive ? "true" : "false");
    Debugf(PSTR("stepup_enabled: %s\r\n"), state.otd.bStepUpEnabled ? "true" : "false");
    Debugf(PSTR("monitor_mode: %s\r\n"), state.otd.bMonitorMode ? "true" : "false");
    Debugf(PSTR("mode: %d\r\n"), (int)state.otd.eMode);
    Debugf(PSTR("master_mode: %s\r\n"), state.otd.bMasterMode ? "true" : "false");
    Debugf(PSTR("thermostat_connected: %s\r\n"), state.otd.bThermostatConnected ? "true" : "false");
    Debugf(PSTR("setback_active: %s\r\n"), state.otd.bSetbackActive ? "true" : "false");
    Debugf(PSTR("override_mode: %d\r\n"), (int)state.otd.eOverrideMode);
    Debugf(PSTR("override_f88: %u\r\n"), (unsigned)state.otd.iOverrideF88);
#endif

    Debugln(F("--- DUMP END ---"));
}

// Dispatch a single keypress from the telnet debug session.
// Called from onTelnetInput() in networkStuff.ino via the SimpleTelnet
// onInputReceived callback (line mode off — one char per call).
void handleDebugChar(char c){
        switch (c){
            case 'h':
                Debugln();
                Debugln(F("---===[ Debug Commands ]===---"));
                Debugln(F("Toggle keys (current state shown in welcome banner):"));
                Debugln(F("  1) OT message parsing       2) REST API handling"));
                Debugln(F("  3) MQTT module              4) Sensor modules"));
                Debugln(F("  5) SAT control loop         6) OTDirect frame handling"));
#if HAS_SAT_BLE
                Debugln(F("  7) SAT BLE sensor scan"));
#endif
                Debugln(F("  g) MQTT interval gating     n) NTP time sync"));
                Debugln(F("  d) Dallas-sensor simulation"));
                Debugln();
                Debugln(F("--- Actions ---"));
                Debugln(F("  D) Dump full debug info (settings + state)"));
                Debugln(F("  q) Force read settings"));
                Debugln(F("  F) Force MQTT discovery for ALL message IDs"));
                Debugln(F("  V) Trigger MQTT discovery verification"));
                Debugln(F("  r) Reconnect WiFi/telnet/MQTT"));
                Debugln(F("  p) Reset PIC manually"));
                Debugln(F("  a) Send PR=A to identify PIC firmware version & type"));
                Debugln(F("  s/S) Toggle OTGW serial-simulation replay"));
                Debugln(F("  w) Trigger Open-Meteo weather fetch and dump state"));
                Debugln(F("  z) Reset heap soak diagnostics (watermark, histogram, counters)"));
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
                // ADR-127: on a combo running OTDirect the PIC reset line is
                // the W5500 SPI clock — never pulse it in that mode.
                if (isOTDirectEnabled()) {
                  DebugTln(F("PIC reset skipped: board is in OT-Direct mode"));
                } else {
                  DebugTln(F("Manual reset PIC"));
                  detectPIC();
                }
                break;
            case 'a':
#if HAS_PIC
                if (isPICEnabled()) {          // skip when the PIC is unavailable
                  DebugTln(F("Send PR=A command, to ID the chip"));
                  getpicfwversion();
                  DebugTln(F("Debug --> PR=A report firmware version, type"));
                  strlcpy(state.pic.sFwversion, OTGWSerial.firmwareVersion(), sizeof(state.pic.sFwversion));
                  OTDebugTf(PSTR("Current firmware version: %s\r\n"), state.pic.sFwversion);
                  strlcpy(state.pic.sDeviceid, OTGWSerial.processorToString().c_str(), sizeof(state.pic.sDeviceid));
                  OTDebugTf(PSTR("Current device id: %s\r\n"), state.pic.sDeviceid);
                  strlcpy(state.pic.sType, OTGWSerial.firmwareToString().c_str(), sizeof(state.pic.sType));
                  OTDebugTf(PSTR("Current firmware type: %s\r\n"), state.pic.sType);
                } else {
                  DebugTln(F("PIC not available or not active in this mode"));
                }
#else
                DebugTln(F("PIC not available on this hardware"));
#endif
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
                    //check OTGW and telnet
                    startTelnet();
                    startPICStream();
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
            case '5':
                state.debug.bSAT = !state.debug.bSAT;
                DebugTf(PSTR("\r\nDebug SAT: %s\r\n"), CBOOLEAN(state.debug.bSAT));
                break;
            case '6':
                state.debug.bOTDirect = !state.debug.bOTDirect;
                DebugTf(PSTR("\r\nDebug OTDirect: %s\r\n"), CBOOLEAN(state.debug.bOTDirect));
                break;
#if HAS_SAT_BLE
            case '7':
                state.debug.bSATBLE = !state.debug.bSATBLE;
                DebugTf(PSTR("\r\nDebug SAT BLE: %s\r\n"), CBOOLEAN(state.debug.bSATBLE));
                break;
#endif
            case 'g':
                state.debug.bMQTTGate = !state.debug.bMQTTGate;
                DebugTf(PSTR("\r\nDebug MQTT Gating: %s\r\n"), CBOOLEAN(state.debug.bMQTTGate));
                break;
            case 'n':
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
            case 'w':
                weatherFetchOpenMeteoNow();
                break;
            case 'z':
                resetHeapWatermark();
                DebugTln(F("Heap soak diagnostics reset (watermark + histogram + pressure counters; min_free_heap is native, not reset). Use from a healthy heap for accurate tier counts."));
                break;
            default:
                break;
        }
}

// Called from doBackgroundTasks().
// SimpleTelnet is cooperative: loop() accepts new TCP clients, checks
// disconnects, and dispatches onInputReceived callbacks.
void handleDebug(){
    debugTelnet.loop();
}

// Called from doBackgroundTasks().
// Port 25238 uses the same cooperative SimpleTelnet service model as
// the debug telnet port: begin() binds the listener, loop() accepts new
// clients and maintains existing ones.
void handleOTGWstream(){
    if (!settings.mqtt.bLegacyPort25238Enabled) return;
    OTGWstream.loop();
}
