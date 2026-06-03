// =======================================================================
// SINGLE SOURCE OF TRUTH for Home Assistant MQTT auto-discovery configs.
// =======================================================================
//
// Hand-written streaming HA discovery data + compose functions.
// See ADR-077 (streaming MQTT HA discovery architecture) for the rationale.
// Edit this file directly to add, remove, or modify discovery entries.
//
// Historical note (2026-04-22):
//   This file was originally auto-generated from src/OTGW-firmware/data/mqttha.cfg
//   by tools/generate_mqttha_data.py. From 2026-04-20 onwards the file
//   diverged from the cfg (see commit bc9bd6a2 "on-demand discovery
//   verification and republish", which added hand-written entries the
//   generator cannot produce). The cfg has been retired as a build input
//   and archived at docs/archive/mqttha.cfg for historical reference only.
//   Do NOT run tools/generate_mqttha_data.py against this file -- it would
//   overwrite all post-2026-04-20 hand-edits. The former auto-generator
//   under tools/generate_mqttha_*.py is no longer part of the build.
//
// Last generator run: 2026-04-17T17:40:17Z (snapshot of the cfg-based
//                                            baseline before retirement)
//
// Current contents (hand-maintained):
//   Sensors        : 385 entries (122 unique OT/pseudo IDs incl. TASK-543 SAT groups)
//   Binary sensors : 58 entries (12 unique OT/pseudo IDs)
//   Climate        : 2 entries
//   Number         : 1 entries

#include "MQTTstuff.h"
#include <platform.h>          // platformFreeHeap() / platformMaxFreeBlock() (ESP8266 + ESP32)

// ========== Named PROGMEM strings: Labels ==========
const char ha_lbl_status_master[] PROGMEM = "status_master";
const char ha_lbl_status_slave[] PROGMEM = "status_slave";
const char ha_lbl_tset[] PROGMEM = "TSet";
const char ha_lbl_master_configuration[] PROGMEM = "master_configuration";
const char ha_lbl_master_memberid_code[] PROGMEM = "master_memberid_code";
const char ha_lbl_slave_configuration[] PROGMEM = "slave_configuration";
const char ha_lbl_slave_memberid_code[] PROGMEM = "slave_memberid_code";
const char ha_lbl_command_hb_u8[] PROGMEM = "Command_hb_u8";
const char ha_lbl_command_lb_u8[] PROGMEM = "Command_lb_u8";
const char ha_lbl_command_remote_command[] PROGMEM = "Command_remote_command";
const char ha_lbl_asf_flags[] PROGMEM = "ASF_flags";
const char ha_lbl_oemfaultcode[] PROGMEM = "OEMFaultCode";
const char ha_lbl_rbp_flags_read_write[] PROGMEM = "RBP_flags_read_write";
const char ha_lbl_rbp_flags_transfer_enable[] PROGMEM = "RBP_flags_transfer_enable";
const char ha_lbl_coolingcontrol[] PROGMEM = "CoolingControl";
const char ha_lbl_tsetch2[] PROGMEM = "TsetCH2";
const char ha_lbl_troverride[] PROGMEM = "TrOverride";
const char ha_lbl_tsp_hb_u8[] PROGMEM = "TSP_hb_u8";
const char ha_lbl_tsp_lb_u8[] PROGMEM = "TSP_lb_u8";
const char ha_lbl_tspindextspvalue_hb_u8[] PROGMEM = "TSPindexTSPvalue_hb_u8";
const char ha_lbl_tspindextspvalue_lb_u8[] PROGMEM = "TSPindexTSPvalue_lb_u8";
const char ha_lbl_fhbsize_hb_u8[] PROGMEM = "FHBsize_hb_u8";
const char ha_lbl_fhbsize_lb_u8[] PROGMEM = "FHBsize_lb_u8";
const char ha_lbl_fhbindexfhbvalue_hb_u8[] PROGMEM = "FHBindexFHBvalue_hb_u8";
const char ha_lbl_fhbindexfhbvalue_lb_u8[] PROGMEM = "FHBindexFHBvalue_lb_u8";
const char ha_lbl_maxrelmodlevelsetting[] PROGMEM = "MaxRelModLevelSetting";
const char ha_lbl_maxcapacityminmodlevel_hb_u8[] PROGMEM = "MaxCapacityMinModLevel_hb_u8";
const char ha_lbl_maxcapacityminmodlevel_lb_u8[] PROGMEM = "MaxCapacityMinModLevel_lb_u8";
const char ha_lbl_trset[] PROGMEM = "TrSet";
const char ha_lbl_relmodlevel[] PROGMEM = "RelModLevel";
const char ha_lbl_chpressure[] PROGMEM = "CHPressure";
const char ha_lbl_dhwflowrate[] PROGMEM = "DHWFlowRate";
const char ha_lbl_daytime_dayofweek[] PROGMEM = "DayTime_dayofweek";
const char ha_lbl_daytime_hour[] PROGMEM = "DayTime_hour";
const char ha_lbl_daytime_minutes[] PROGMEM = "DayTime_minutes";
const char ha_lbl_date_day_of_month[] PROGMEM = "Date_day_of_month";
const char ha_lbl_date_month[] PROGMEM = "Date_month";
const char ha_lbl_year[] PROGMEM = "Year";
const char ha_lbl_trsetch2[] PROGMEM = "TrSetCH2";
const char ha_lbl_tr[] PROGMEM = "Tr";
const char ha_lbl_tboiler[] PROGMEM = "Tboiler";
const char ha_lbl_tdhw[] PROGMEM = "Tdhw";
const char ha_lbl_toutside[] PROGMEM = "Toutside";
const char ha_lbl_tret[] PROGMEM = "Tret";
const char ha_lbl_tsolarstorage[] PROGMEM = "Tsolarstorage";
const char ha_lbl_tsolarcollector[] PROGMEM = "Tsolarcollector";
const char ha_lbl_tflowch2[] PROGMEM = "TflowCH2";
const char ha_lbl_tdhw2[] PROGMEM = "Tdhw2";
const char ha_lbl_texhaust[] PROGMEM = "Texhaust";
const char ha_lbl_theatexchanger[] PROGMEM = "Theatexchanger";
const char ha_lbl_fanspeed_hb_u8[] PROGMEM = "FanSpeed_hb_u8";
const char ha_lbl_fanspeed_lb_u8[] PROGMEM = "FanSpeed_lb_u8";
const char ha_lbl_electricalcurrentburnerflame[] PROGMEM = "ElectricalCurrentBurnerFlame";
const char ha_lbl_troomch2[] PROGMEM = "TRoomCH2";
const char ha_lbl_relativehumidity[] PROGMEM = "RelativeHumidity";
const char ha_lbl_troverride2[] PROGMEM = "TrOverride2";
const char ha_lbl_tdhwsetubtdhwsetlb_value_hb[] PROGMEM = "TdhwSetUBTdhwSetLB_value_hb";
const char ha_lbl_tdhwsetubtdhwsetlb_value_lb[] PROGMEM = "TdhwSetUBTdhwSetLB_value_lb";
const char ha_lbl_maxtsetubmaxtsetlb_value_hb[] PROGMEM = "MaxTSetUBMaxTSetLB_value_hb";
const char ha_lbl_maxtsetubmaxtsetlb_value_lb[] PROGMEM = "MaxTSetUBMaxTSetLB_value_lb";
const char ha_lbl_hcratioubhcratiolb_value_hb[] PROGMEM = "HcratioUBHcratioLB_value_hb";
const char ha_lbl_hcratioubhcratiolb_value_lb[] PROGMEM = "HcratioUBHcratioLB_value_lb";
const char ha_lbl_remoteparameter4boundaries_value_hb[] PROGMEM = "Remoteparameter4boundaries_value_hb";
const char ha_lbl_remoteparameter4boundaries_value_lb[] PROGMEM = "Remoteparameter4boundaries_value_lb";
const char ha_lbl_remoteparameter5boundaries_value_hb[] PROGMEM = "Remoteparameter5boundaries_value_hb";
const char ha_lbl_remoteparameter5boundaries_value_lb[] PROGMEM = "Remoteparameter5boundaries_value_lb";
const char ha_lbl_remoteparameter6boundaries_value_hb[] PROGMEM = "Remoteparameter6boundaries_value_hb";
const char ha_lbl_remoteparameter6boundaries_value_lb[] PROGMEM = "Remoteparameter6boundaries_value_lb";
const char ha_lbl_remoteparameter7boundaries_value_hb[] PROGMEM = "Remoteparameter7boundaries_value_hb";
const char ha_lbl_remoteparameter7boundaries_value_lb[] PROGMEM = "Remoteparameter7boundaries_value_lb";
const char ha_lbl_remoteparameter8boundaries_value_hb[] PROGMEM = "Remoteparameter8boundaries_value_hb";
const char ha_lbl_remoteparameter8boundaries_value_lb[] PROGMEM = "Remoteparameter8boundaries_value_lb";
const char ha_lbl_tdhwset[] PROGMEM = "TdhwSet";
const char ha_lbl_maxtset[] PROGMEM = "MaxTSet";
const char ha_lbl_hcratio[] PROGMEM = "Hcratio";
const char ha_lbl_remoteparameter4[] PROGMEM = "Remoteparameter4";
const char ha_lbl_remoteparameter5[] PROGMEM = "Remoteparameter5";
const char ha_lbl_remoteparameter6[] PROGMEM = "Remoteparameter6";
const char ha_lbl_remoteparameter7[] PROGMEM = "Remoteparameter7";
const char ha_lbl_remoteparameter8[] PROGMEM = "Remoteparameter8";
const char ha_lbl_status_vh_master[] PROGMEM = "status_vh_master";
const char ha_lbl_status_vh_slave[] PROGMEM = "status_vh_slave";
const char ha_lbl_controlsetpointvh[] PROGMEM = "ControlSetpointVH";
const char ha_lbl_controlsetpointvh_hb_u8[] PROGMEM = "ControlSetpointVH_hb_u8";
const char ha_lbl_controlsetpointvh_lb_u8[] PROGMEM = "ControlSetpointVH_lb_u8";
const char ha_lbl_asffaultcodevh_code[] PROGMEM = "ASFFaultCodeVH_code";
const char ha_lbl_asffaultcodevh_flag8[] PROGMEM = "ASFFaultCodeVH_flag8";
const char ha_lbl_diagnosticcodevh[] PROGMEM = "DiagnosticCodeVH";
const char ha_lbl_vh_configuration[] PROGMEM = "vh_configuration";
const char ha_lbl_vh_memberid_code[] PROGMEM = "vh_memberid_code";
const char ha_lbl_openthermversionvh[] PROGMEM = "OpenthermVersionVH";
const char ha_lbl_versiontypevh_hb_u8[] PROGMEM = "VersionTypeVH_hb_u8";
const char ha_lbl_versiontypevh_lb_u8[] PROGMEM = "VersionTypeVH_lb_u8";
const char ha_lbl_relativeventilation[] PROGMEM = "RelativeVentilation";
const char ha_lbl_relativeventilation_hb_u8[] PROGMEM = "RelativeVentilation_hb_u8";
const char ha_lbl_relativeventilation_lb_u8[] PROGMEM = "RelativeVentilation_lb_u8";
const char ha_lbl_relativehumidityexhaustair[] PROGMEM = "RelativeHumidityExhaustAir";
const char ha_lbl_relativehumidityexhaustair_hb_u8[] PROGMEM = "RelativeHumidityExhaustAir_hb_u8";
const char ha_lbl_relativehumidityexhaustair_lb_u8[] PROGMEM = "RelativeHumidityExhaustAir_lb_u8";
const char ha_lbl_co2levelexhaustair[] PROGMEM = "CO2LevelExhaustAir";
const char ha_lbl_supplyinlettemperature[] PROGMEM = "SupplyInletTemperature";
const char ha_lbl_supplyoutlettemperature[] PROGMEM = "SupplyOutletTemperature";
const char ha_lbl_exhaustinlettemperature[] PROGMEM = "ExhaustInletTemperature";
const char ha_lbl_exhaustoutlettemperature[] PROGMEM = "ExhaustOutletTemperature";
const char ha_lbl_actualexhaustfanspeed[] PROGMEM = "ActualExhaustFanSpeed";
const char ha_lbl_actualsupplyfanspeed[] PROGMEM = "ActualSupplyFanSpeed";
const char ha_lbl_remoteparametersettingvh_hb_flag8[] PROGMEM = "RemoteParameterSettingVH_hb_flag8";
const char ha_lbl_remoteparametersettingvh_lb_flag8[] PROGMEM = "RemoteParameterSettingVH_lb_flag8";
const char ha_lbl_vh_rw_nominal_ventilation_value[] PROGMEM = "vh_rw_nominal_ventilation_value";
const char ha_lbl_vh_transfer_enable_nominal_ventilation_value[] PROGMEM = "vh_transfer_enable_nominal_ventilation_value";
const char ha_lbl_nominalventilationvalue[] PROGMEM = "NominalVentilationValue";
const char ha_lbl_nominalventilationvalue_hb_u8[] PROGMEM = "NominalVentilationValue_hb_u8";
const char ha_lbl_nominalventilationvalue_lb_u8[] PROGMEM = "NominalVentilationValue_lb_u8";
const char ha_lbl_tspnumbervh_hb_u8[] PROGMEM = "TSPNumberVH_hb_u8";
const char ha_lbl_tspnumbervh_lb_u8[] PROGMEM = "TSPNumberVH_lb_u8";
const char ha_lbl_tspentryvh_hb_u8[] PROGMEM = "TSPEntryVH_hb_u8";
const char ha_lbl_tspentryvh_lb_u8[] PROGMEM = "TSPEntryVH_lb_u8";
const char ha_lbl_faultbuffersizevh_hb_u8[] PROGMEM = "FaultBufferSizeVH_hb_u8";
const char ha_lbl_faultbuffersizevh_lb_u8[] PROGMEM = "FaultBufferSizeVH_lb_u8";
const char ha_lbl_faultbufferentryvh_hb_u8[] PROGMEM = "FaultBufferEntryVH_hb_u8";
const char ha_lbl_faultbufferentryvh_lb_u8[] PROGMEM = "FaultBufferEntryVH_lb_u8";
const char ha_lbl_brand_hb_u8[] PROGMEM = "Brand_hb_u8";
const char ha_lbl_brand_lb_u8[] PROGMEM = "Brand_lb_u8";
const char ha_lbl_brandversion_hb_u8[] PROGMEM = "BrandVersion_hb_u8";
const char ha_lbl_brandversion_lb_u8[] PROGMEM = "BrandVersion_lb_u8";
const char ha_lbl_brandserialnumber_hb_u8[] PROGMEM = "BrandSerialNumber_hb_u8";
const char ha_lbl_brandserialnumber_lb_u8[] PROGMEM = "BrandSerialNumber_lb_u8";
const char ha_lbl_coolingoperationhours[] PROGMEM = "CoolingOperationHours";
const char ha_lbl_powercycles[] PROGMEM = "PowerCycles";
const char ha_lbl_rfsensorstatusinformation_battery_indication[] PROGMEM = "RFSensorStatusInformation_battery_indication";
const char ha_lbl_rfsensorstatusinformation_battery_indication_code[] PROGMEM = "RFSensorStatusInformation_battery_indication_code";
const char ha_lbl_rfsensorstatusinformation_sensor_index[] PROGMEM = "RFSensorStatusInformation_sensor_index";
const char ha_lbl_rfsensorstatusinformation_sensor_type[] PROGMEM = "RFSensorStatusInformation_sensor_type";
const char ha_lbl_rfsensorstatusinformation_sensor_type_code[] PROGMEM = "RFSensorStatusInformation_sensor_type_code";
const char ha_lbl_rfsensorstatusinformation_signal_strength[] PROGMEM = "RFSensorStatusInformation_signal_strength";
const char ha_lbl_rfsensorstatusinformation_signal_strength_code[] PROGMEM = "RFSensorStatusInformation_signal_strength_code";
const char ha_lbl_rfstrengthbatterylevel_hb_u8[] PROGMEM = "RFstrengthbatterylevel_hb_u8";
const char ha_lbl_rfstrengthbatterylevel_lb_u8[] PROGMEM = "RFstrengthbatterylevel_lb_u8";
const char ha_lbl_operatingmode_hc1_hc2_dhw_hb_u8[] PROGMEM = "OperatingMode_HC1_HC2_DHW_hb_u8";
const char ha_lbl_operatingmode_hc1_hc2_dhw_lb_u8[] PROGMEM = "OperatingMode_HC1_HC2_DHW_lb_u8";
const char ha_lbl_remoteoverrideoperatingmode_dhw_mode[] PROGMEM = "RemoteOverrideOperatingMode_dhw_mode";
const char ha_lbl_remoteoverrideoperatingmode_dhw_mode_code[] PROGMEM = "RemoteOverrideOperatingMode_dhw_mode_code";
const char ha_lbl_remoteoverrideoperatingmode_hc1_mode[] PROGMEM = "RemoteOverrideOperatingMode_hc1_mode";
const char ha_lbl_remoteoverrideoperatingmode_hc1_mode_code[] PROGMEM = "RemoteOverrideOperatingMode_hc1_mode_code";
const char ha_lbl_remoteoverrideoperatingmode_hc2_mode[] PROGMEM = "RemoteOverrideOperatingMode_hc2_mode";
const char ha_lbl_remoteoverrideoperatingmode_hc2_mode_code[] PROGMEM = "RemoteOverrideOperatingMode_hc2_mode_code";
const char ha_lbl_remoteoverrideoperatingmode_manual_dhw_push[] PROGMEM = "RemoteOverrideOperatingMode_manual_dhw_push";
const char ha_lbl_roomremoteoverridefunction_flag8[] PROGMEM = "RoomRemoteOverrideFunction_flag8";
const char ha_lbl_solar_storage_master_mode[] PROGMEM = "solar_storage_master_mode";
const char ha_lbl_solar_storage_mode_status[] PROGMEM = "solar_storage_mode_status";
const char ha_lbl_solar_storage_slave_status[] PROGMEM = "solar_storage_slave_status";
const char ha_lbl_solarstorageasfflags_code[] PROGMEM = "SolarStorageASFflags_code";
const char ha_lbl_solarstorageasfflags_flag8[] PROGMEM = "SolarStorageASFflags_flag8";
const char ha_lbl_solar_storage_slave_configuration[] PROGMEM = "solar_storage_slave_configuration";
const char ha_lbl_solar_storage_slave_memberid_code[] PROGMEM = "solar_storage_slave_memberid_code";
const char ha_lbl_solarstorageversiontype_hb_u8[] PROGMEM = "SolarStorageVersionType_hb_u8";
const char ha_lbl_solarstorageversiontype_lb_u8[] PROGMEM = "SolarStorageVersionType_lb_u8";
const char ha_lbl_solarstoragetsp_hb_u8[] PROGMEM = "SolarStorageTSP_hb_u8";
const char ha_lbl_solarstoragetsp_lb_u8[] PROGMEM = "SolarStorageTSP_lb_u8";
const char ha_lbl_solarstoragetspindextspvalue_hb_u8[] PROGMEM = "SolarStorageTSPindexTSPvalue_hb_u8";
const char ha_lbl_solarstoragetspindextspvalue_lb_u8[] PROGMEM = "SolarStorageTSPindexTSPvalue_lb_u8";
const char ha_lbl_solarstoragefhbsize_hb_u8[] PROGMEM = "SolarStorageFHBsize_hb_u8";
const char ha_lbl_solarstoragefhbsize_lb_u8[] PROGMEM = "SolarStorageFHBsize_lb_u8";
const char ha_lbl_solarstoragefhbindexfhbvalue_hb_u8[] PROGMEM = "SolarStorageFHBindexFHBvalue_hb_u8";
const char ha_lbl_solarstoragefhbindexfhbvalue_lb_u8[] PROGMEM = "SolarStorageFHBindexFHBvalue_lb_u8";
const char ha_lbl_electricityproducerstarts[] PROGMEM = "ElectricityProducerStarts";
const char ha_lbl_electricityproducerhours[] PROGMEM = "ElectricityProducerHours";
const char ha_lbl_electricityproduction[] PROGMEM = "ElectricityProduction";
const char ha_lbl_cumulativeelectricityproduction[] PROGMEM = "CumulativeElectricityProduction";
const char ha_lbl_burnerunsuccessfulstarts[] PROGMEM = "BurnerUnsuccessfulStarts";
const char ha_lbl_flamesignaltoolow[] PROGMEM = "FlameSignalTooLow";
const char ha_lbl_oemdiagnosticcode[] PROGMEM = "OEMDiagnosticCode";
const char ha_lbl_burnerstarts[] PROGMEM = "BurnerStarts";
const char ha_lbl_chpumpstarts[] PROGMEM = "CHPumpStarts";
const char ha_lbl_dhwpumpvalvestarts[] PROGMEM = "DHWPumpValveStarts";
const char ha_lbl_dhwburnerstarts[] PROGMEM = "DHWBurnerStarts";
const char ha_lbl_burneroperationhours[] PROGMEM = "BurnerOperationHours";
const char ha_lbl_chpumpoperationhours[] PROGMEM = "CHPumpOperationHours";
const char ha_lbl_dhwpumpvalveoperationhours[] PROGMEM = "DHWPumpValveOperationHours";
const char ha_lbl_dhwburneroperationhours[] PROGMEM = "DHWBurnerOperationHours";
const char ha_lbl_openthermversionmaster[] PROGMEM = "OpenThermVersionMaster";
const char ha_lbl_openthermversionslave[] PROGMEM = "OpenThermVersionSlave";
const char ha_lbl_masterversion_hb_u8[] PROGMEM = "MasterVersion_hb_u8";
const char ha_lbl_masterversion_lb_u8[] PROGMEM = "MasterVersion_lb_u8";
const char ha_lbl_slaveversion_hb_u8[] PROGMEM = "SlaveVersion_hb_u8";
const char ha_lbl_slaveversion_lb_u8[] PROGMEM = "SlaveVersion_lb_u8";
const char ha_lbl_remehadfducodes_hb_u8[] PROGMEM = "RemehadFdUcodes_hb_u8";
const char ha_lbl_remehadfducodes_lb_u8[] PROGMEM = "RemehadFdUcodes_lb_u8";
const char ha_lbl_remehaservicemessage_hb_u8[] PROGMEM = "RemehaServicemessage_hb_u8";
const char ha_lbl_remehaservicemessage_lb_u8[] PROGMEM = "RemehaServicemessage_lb_u8";
const char ha_lbl_remehadetectionconnectedscu_hb_u8[] PROGMEM = "RemehaDetectionConnectedSCU_hb_u8";
const char ha_lbl_remehadetectionconnectedscu_lb_u8[] PROGMEM = "RemehaDetectionConnectedSCU_lb_u8";
const char ha_lbl_s0powerkw[] PROGMEM = "s0powerkw";
const char ha_lbl_s0pulsecount[] PROGMEM = "s0pulsecount";
const char ha_lbl_s0pulsecounttot[] PROGMEM = "s0pulsecounttot";
const char ha_lbl_s0pulsetime[] PROGMEM = "s0pulsetime";
const char ha_lbl_sensor_id[] PROGMEM = "%sensor_id%";
// Heap & discovery statistics labels (TASK-346, faux dataid 247)
// Each label is the topic suffix after <topTopic>/value/<uniqueid>/
const char ha_lbl_stats_ws_drops[] PROGMEM                = "otgw-firmware/stats/ws_drops";
const char ha_lbl_stats_mqtt_drops[] PROGMEM              = "otgw-firmware/stats/mqtt_drops";
const char ha_lbl_stats_enter_low[] PROGMEM               = "otgw-firmware/stats/enter_low";
const char ha_lbl_stats_enter_warning[] PROGMEM           = "otgw-firmware/stats/enter_warning";
const char ha_lbl_stats_enter_critical[] PROGMEM          = "otgw-firmware/stats/enter_critical";
const char ha_lbl_stats_drip_burst_skip[] PROGMEM         = "otgw-firmware/stats/drip_burst_skip";
const char ha_lbl_stats_drip_cooldown_skip[] PROGMEM      = "otgw-firmware/stats/drip_cooldown_skip";
const char ha_lbl_stats_drip_slowmode[] PROGMEM           = "otgw-firmware/stats/drip_slowmode";
const char ha_lbl_stats_free_heap[] PROGMEM               = "otgw-firmware/stats/free_heap";
const char ha_lbl_stats_max_block[] PROGMEM               = "otgw-firmware/stats/max_block";
const char ha_lbl_stats_frag_pct[] PROGMEM                = "otgw-firmware/stats/frag_pct";
const char ha_lbl_stats_disc_verify_runs[] PROGMEM        = "otgw-firmware/stats/disc_verify_runs";
const char ha_lbl_stats_disc_republish_triggered[] PROGMEM = "otgw-firmware/stats/disc_republish_triggered";
const char ha_lbl_stats_disc_last_missing[] PROGMEM       = "otgw-firmware/stats/disc_last_missing";
const char ha_lbl_stats_disc_last_orphan[] PROGMEM        = "otgw-firmware/stats/disc_last_orphan";
const char ha_lbl_stats_disc_published_topics[] PROGMEM   = "otgw-firmware/stats/disc_published_topics";
const char ha_lbl_stats_disc_last_verify_epoch[] PROGMEM  = "otgw-firmware/stats/disc_last_verify_epoch";
// Firmware diagnostic labels (TASK-540 / TASK-541, faux dataid 248). Plain topic paths.
const char ha_lbl_fw_reboot_count[]  PROGMEM = "otgw-firmware/reboot_count";
const char ha_lbl_fw_reboot_reason[] PROGMEM = "otgw-firmware/reboot_reason";
const char ha_lbl_fw_version[]       PROGMEM = "otgw-firmware/version";
const char ha_lbl_fw_hostname[]      PROGMEM = "otgw-firmware/hostname";
const char ha_lbl_fw_hardware_type[] PROGMEM = "otgw-firmware/hardware_type";  // ADR-113
// PIC info labels (TASK-540 / TASK-541, faux dataid 249). MQTT_HA_FLAG_IS_PIC_ENTRY auto-prepends "otgw-pic/".
const char ha_lbl_pic_version[]       PROGMEM = "version";
const char ha_lbl_pic_deviceid[]      PROGMEM = "deviceid";
const char ha_lbl_pic_firmwaretype[]  PROGMEM = "firmwaretype";
const char ha_lbl_pic_designer[]      PROGMEM = "designer";
// ADR-113 stage 2 (TASK-754): ha_lbl_pic_available removed.
// PIC settings labels (TASK-540 / TASK-541, faux dataid 250). IS_PIC flag prepends "otgw-pic/" → "otgw-pic/settings/<x>".
const char ha_lbl_pic_set_setpoint_override[]   PROGMEM = "settings/setpoint_override";
const char ha_lbl_pic_set_setback[]              PROGMEM = "settings/setback";
const char ha_lbl_pic_set_dhw_override[]         PROGMEM = "settings/dhw_override";
const char ha_lbl_pic_set_gpio[]                 PROGMEM = "settings/gpio";
const char ha_lbl_pic_set_gpio_states[]          PROGMEM = "settings/gpio_states";
const char ha_lbl_pic_set_led[]                  PROGMEM = "settings/led";
const char ha_lbl_pic_set_tweaks[]               PROGMEM = "settings/tweaks";
const char ha_lbl_pic_set_temp_sensor[]          PROGMEM = "settings/temp_sensor";
const char ha_lbl_pic_set_smart_power[]          PROGMEM = "settings/smart_power";
const char ha_lbl_pic_set_thermostat_detect[]    PROGMEM = "settings/thermostat_detect";
const char ha_lbl_pic_set_builddate[]            PROGMEM = "settings/builddate";
const char ha_lbl_pic_set_clock_mhz[]            PROGMEM = "settings/clock_mhz";
const char ha_lbl_pic_set_reset_cause[]          PROGMEM = "settings/reset_cause";
const char ha_lbl_pic_set_standalone_interval[]  PROGMEM = "settings/standalone_interval";
const char ha_lbl_pic_set_voltage_ref[]          PROGMEM = "settings/voltage_ref";
// 2.0.0-specific diagnostic labels (TASK-541, faux dataid 251). Plain topic paths.
// OTDirect (always present on 2.0.0; flame metrics are diagnostic counters/gauges).
const char ha_lbl_otdirect_flame_duty_pct[]        PROGMEM = "otgw32/flame_duty_pct";
const char ha_lbl_otdirect_flame_cycles_per_hr[]   PROGMEM = "otgw32/flame_cycles_per_hour";
// SAT BLE diagnostics (signal/battery/availability — measurements live in TASK-543).
const char ha_lbl_sat_ble_sensor_rssi[]            PROGMEM = "sat/ble_sensor_rssi";
const char ha_lbl_sat_ble_battery[]                PROGMEM = "sat/ble_battery";
const char ha_lbl_sat_ble_sensor_count[]           PROGMEM = "sat/ble_sensor_count";
const char ha_lbl_sat_ble_temp_valid[]             PROGMEM = "sat/ble_temp_valid";
// SAT pressure status (string sibling of ch_pressure measurement which lives in TASK-543).
const char ha_lbl_sat_ch_pressure_status[]         PROGMEM = "sat/ch_pressure_status";
#define DECLARE_SAT_DISCOVERY_STRINGS(key, topic, friendly) \
    const char ha_lbl_##key[] PROGMEM = topic; \
    const char ha_name_##key[] PROGMEM = friendly;
// TASK-543: user-facing SAT/OTDirect discovery surface (plain topic paths).
DECLARE_SAT_DISCOVERY_STRINGS(sat_target,                 "sat/target",                        "SAT_Target")
DECLARE_SAT_DISCOVERY_STRINGS(sat_mode,                   "sat/mode",                          "SAT_Mode")
DECLARE_SAT_DISCOVERY_STRINGS(sat_setpoint,               "sat/setpoint",                      "SAT_Setpoint")
DECLARE_SAT_DISCOVERY_STRINGS(sat_heating_curve,          "sat/heating_curve",                 "SAT_Heating_Curve")
DECLARE_SAT_DISCOVERY_STRINGS(sat_pid_output,             "sat/pid_output",                    "SAT_PID_Output")
DECLARE_SAT_DISCOVERY_STRINGS(sat_error,                  "sat/error",                         "SAT_Error")
DECLARE_SAT_DISCOVERY_STRINGS(sat_room_temp,              "sat/room_temp",                     "SAT_Room_Temp")
DECLARE_SAT_DISCOVERY_STRINGS(sat_outside_temp,           "sat/outside_temp",                  "SAT_Outside_Temp")
DECLARE_SAT_DISCOVERY_STRINGS(sat_pid_p,                  "sat/pid_p",                         "SAT_PID_P")
DECLARE_SAT_DISCOVERY_STRINGS(sat_pid_i,                  "sat/pid_i",                         "SAT_PID_I")
DECLARE_SAT_DISCOVERY_STRINGS(sat_pid_d,                  "sat/pid_d",                         "SAT_PID_D")
DECLARE_SAT_DISCOVERY_STRINGS(sat_raw_derivative,         "sat/raw_derivative",                "SAT_Raw_Derivative")
DECLARE_SAT_DISCOVERY_STRINGS(sat_kp,                     "sat/kp",                            "SAT_Kp")
DECLARE_SAT_DISCOVERY_STRINGS(sat_ki,                     "sat/ki",                            "SAT_Ki")
DECLARE_SAT_DISCOVERY_STRINGS(sat_kd,                     "sat/kd",                            "SAT_Kd")
DECLARE_SAT_DISCOVERY_STRINGS(sat_boiler_status,          "sat/boiler_status",                 "SAT_Boiler_Status")
DECLARE_SAT_DISCOVERY_STRINGS(sat_cycle_class,            "sat/cycle_class",                   "SAT_Cycle_Class")
DECLARE_SAT_DISCOVERY_STRINGS(sat_pwm_duty,               "sat/pwm_duty",                      "SAT_PWM_Duty")
DECLARE_SAT_DISCOVERY_STRINGS(sat_duty_ratio,             "sat/duty_ratio",                    "SAT_Duty_Ratio")
DECLARE_SAT_DISCOVERY_STRINGS(sat_overshoot_fraction,     "sat/overshoot_fraction",            "SAT_Overshoot_Fraction")
DECLARE_SAT_DISCOVERY_STRINGS(sat_cycle_phase,            "sat/cycle_phase",                   "SAT_Cycle_Phase")
DECLARE_SAT_DISCOVERY_STRINGS(sat_overshoot_margin,       "sat/overshoot_margin",              "SAT_Overshoot_Margin")
DECLARE_SAT_DISCOVERY_STRINGS(sat_cycles_this_hour,       "sat/cycles_this_hour",              "SAT_Cycles_This_Hour")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_cycles,             "sat/4h_cycles",                    "SAT_4H_Cycles")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_avg_on_sec,         "sat/4h_avg_on_sec",                "SAT_4H_Avg_On_Sec")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_avg_off_sec,        "sat/4h_avg_off_sec",               "SAT_4H_Avg_Off_Sec")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_avg_flow_temp,      "sat/4h_avg_flow_temp",             "SAT_4H_Avg_Flow_Temp")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_duty_ratio,         "sat/4h_duty_ratio",                "SAT_4H_Duty_Ratio")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_overshoot_fraction, "sat/4h_overshoot_fraction",        "SAT_4H_Overshoot_Fraction")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_underheat_fraction, "sat/4h_underheat_fraction",        "SAT_4H_Underheat_Fraction")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_flow_ret_delta_p50, "sat/4h_flow_ret_delta_p50",        "SAT_4H_Flow_Ret_Delta_P50")
DECLARE_SAT_DISCOVERY_STRINGS(sat_4h_flow_ret_delta_p90, "sat/4h_flow_ret_delta_p90",        "SAT_4H_Flow_Ret_Delta_P90")
DECLARE_SAT_DISCOVERY_STRINGS(sat_ble_temp,              "sat/ble_temp",                     "SAT_BLE_Temp")
DECLARE_SAT_DISCOVERY_STRINGS(sat_ble_humidity,          "sat/ble_humidity",                 "SAT_BLE_Humidity")
DECLARE_SAT_DISCOVERY_STRINGS(sat_ch_pressure,           "sat/ch_pressure",                  "SAT_CH_Pressure")
DECLARE_SAT_DISCOVERY_STRINGS(sat_flame_status,          "sat/flame_status",                 "SAT_Flame_Status")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_temperature,   "sat/weather/temperature",          "SAT_Weather_Temperature")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_apparent_temp, "sat/weather/apparent_temp",        "SAT_Weather_Apparent_Temp")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_humidity,      "sat/weather/humidity",             "SAT_Weather_Humidity")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_wind_speed,    "sat/weather/wind_speed",           "SAT_Weather_Wind_Speed")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_wind_direction,"sat/weather/wind_direction",       "SAT_Weather_Wind_Direction")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_wind_gusts,    "sat/weather/wind_gusts",           "SAT_Weather_Wind_Gusts")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_cloud_cover,   "sat/weather/cloud_cover",          "SAT_Weather_Cloud_Cover")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_pressure_msl,  "sat/weather/pressure_msl",         "SAT_Weather_Pressure_MSL")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_precipitation, "sat/weather/precipitation",        "SAT_Weather_Precipitation")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_rain,          "sat/weather/rain",                 "SAT_Weather_Rain")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_snowfall,      "sat/weather/snowfall",             "SAT_Weather_Snowfall")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_weather_code,  "sat/weather/weather_code",         "SAT_Weather_Code")
DECLARE_SAT_DISCOVERY_STRINGS(sat_active,                "sat/active",                       "SAT_Active")
DECLARE_SAT_DISCOVERY_STRINGS(sat_safety_tripped,        "sat/safety_tripped",               "SAT_Safety_Tripped")
DECLARE_SAT_DISCOVERY_STRINGS(sat_flame_health,          "sat/flame_health",                 "SAT_Flame_Health")
DECLARE_SAT_DISCOVERY_STRINGS(sat_valves_open,           "sat/valves_open",                  "SAT_Valves_Open")
DECLARE_SAT_DISCOVERY_STRINGS(sat_weather_is_day,        "sat/weather/is_day",               "SAT_Weather_Is_Day")
static const char ha_tpl_sat_pwm_percent[]        PROGMEM = "{{ (value | float * 100) | round(0) }}";
static const char ha_tpl_sat_ratio_percent[]      PROGMEM = "{{ (value | float * 100) | round(1) }}";
static const char ha_tpl_sat_kmh_to_ms[]          PROGMEM = "{{ (value | float / 3.6) | round(1) }}";
static const char ha_json_sat_pid_attributes[]    PROGMEM = "sat/pid_attributes";
static const char ha_json_sat_cycle_attributes[]  PROGMEM = "sat/cycle_attributes";
static const char ha_bincls_problem[]             PROGMEM = "problem";
const char ha_lbl_centralheating[] PROGMEM = "centralheating";
const char ha_lbl_centralheating2[] PROGMEM = "centralheating2";
const char ha_lbl_ch2_enable[] PROGMEM = "ch2_enable";
const char ha_lbl_ch_enable[] PROGMEM = "ch_enable";
const char ha_lbl_cooling[] PROGMEM = "cooling";
const char ha_lbl_cooling_enable[] PROGMEM = "cooling_enable";
const char ha_lbl_dhw_blocking[] PROGMEM = "dhw_blocking";
const char ha_lbl_dhw_enable[] PROGMEM = "dhw_enable";
const char ha_lbl_diagnostic_indicator[] PROGMEM = "diagnostic_indicator";
const char ha_lbl_domestichotwater[] PROGMEM = "domestichotwater";
const char ha_lbl_electric_production[] PROGMEM = "electric_production";
const char ha_lbl_fault[] PROGMEM = "fault";
const char ha_lbl_flame[] PROGMEM = "flame";
const char ha_lbl_otc_active[] PROGMEM = "otc_active";
const char ha_lbl_summerwintertime[] PROGMEM = "summerwintertime";
const char ha_lbl_boiler_connected[] PROGMEM = "boiler_connected";
const char ha_lbl_thermostat_connected[] PROGMEM = "thermostat_connected";
const char ha_lbl_master_configuration_smart_power[] PROGMEM = "master_configuration_smart_power";
const char ha_lbl_ch2_present[] PROGMEM = "ch2_present";
const char ha_lbl_control_type_modulation[] PROGMEM = "control_type_modulation";
const char ha_lbl_cooling_config[] PROGMEM = "cooling_config";
const char ha_lbl_dhw_config[] PROGMEM = "dhw_config";
const char ha_lbl_dhw_present[] PROGMEM = "dhw_present";
const char ha_lbl_heat_cool_mode_control[] PROGMEM = "heat_cool_mode_control";
const char ha_lbl_master_low_off_pump_control_function[] PROGMEM = "master_low_off_pump_control_function";
const char ha_lbl_remote_water_filling_function[] PROGMEM = "remote_water_filling_function";
const char ha_lbl_air_pressure_fault[] PROGMEM = "air_pressure_fault";
const char ha_lbl_gas_flame_fault[] PROGMEM = "gas_flame_fault";
const char ha_lbl_lockout_reset[] PROGMEM = "lockout_reset";
const char ha_lbl_low_water_pressure[] PROGMEM = "low_water_pressure";
const char ha_lbl_service_request[] PROGMEM = "service_request";
const char ha_lbl_water_over_temperature[] PROGMEM = "water_over_temperature";
const char ha_lbl_rbp_dhw_setpoint[] PROGMEM = "rbp_dhw_setpoint";
const char ha_lbl_rbp_max_ch_setpoint[] PROGMEM = "rbp_max_ch_setpoint";
const char ha_lbl_rbp_rw_dhw_setpoint[] PROGMEM = "rbp_rw_dhw_setpoint";
const char ha_lbl_rbp_rw_max_ch_setpoint[] PROGMEM = "rbp_rw_max_ch_setpoint";
const char ha_lbl_vh_bypass_automatic_status[] PROGMEM = "vh_bypass_automatic_status";
const char ha_lbl_vh_bypass_mode[] PROGMEM = "vh_bypass_mode";
const char ha_lbl_vh_bypass_position[] PROGMEM = "vh_bypass_position";
const char ha_lbl_vh_bypass_status[] PROGMEM = "vh_bypass_status";
const char ha_lbl_vh_diagnostic_indicator[] PROGMEM = "vh_diagnostic_indicator";
const char ha_lbl_vh_fault[] PROGMEM = "vh_fault";
const char ha_lbl_vh_free_ventilation_mode[] PROGMEM = "vh_free_ventilation_mode";
const char ha_lbl_vh_free_ventliation_status[] PROGMEM = "vh_free_ventliation_status";
const char ha_lbl_vh_ventilation_enabled[] PROGMEM = "vh_ventilation_enabled";
const char ha_lbl_vh_ventilation_mode[] PROGMEM = "vh_ventilation_mode";
const char ha_lbl_vh_configuration_bypass[] PROGMEM = "vh_configuration_bypass";
const char ha_lbl_vh_configuration_speed_control[] PROGMEM = "vh_configuration_speed_control";
const char ha_lbl_vh_configuration_system_type[] PROGMEM = "vh_configuration_system_type";
const char ha_lbl_remote_override_manual_change_priority[] PROGMEM = "remote_override_manual_change_priority";
const char ha_lbl_remote_override_program_change_priority[] PROGMEM = "remote_override_program_change_priority";
const char ha_lbl_solar_storage_slave_fault_indicator[] PROGMEM = "solar_storage_slave_fault_indicator";
const char ha_lbl_solar_storage_system_type[] PROGMEM = "solar_storage_system_type";

// ---------------------------------------------------------------------------
// ADR-105: HA-core alias labels (37 topics, gated by settings.mqtt.bPublishHaCoreAliases)
// Category A: supports_* capability aliases (12)
const char ha_lbl_alias_supports_master_smart_power[]                       PROGMEM = "supports_master_smart_power";
const char ha_lbl_alias_supports_hot_water[]                                PROGMEM = "supports_hot_water";
const char ha_lbl_alias_supports_cooling[]                                  PROGMEM = "supports_cooling";
const char ha_lbl_alias_supports_pump_control[]                             PROGMEM = "supports_pump_control";
const char ha_lbl_alias_supports_ch_2[]                                     PROGMEM = "supports_ch_2";
const char ha_lbl_alias_supports_remote_reset[]                             PROGMEM = "supports_remote_reset";
const char ha_lbl_alias_supports_lockout_reset[]                            PROGMEM = "supports_lockout_reset";
const char ha_lbl_alias_supports_hot_water_setpoint_transfer[]              PROGMEM = "supports_hot_water_setpoint_transfer";
const char ha_lbl_alias_supports_central_heating_setpoint_transfer[]        PROGMEM = "supports_central_heating_setpoint_transfer";
const char ha_lbl_alias_supports_hot_water_setpoint_writing[]               PROGMEM = "supports_hot_water_setpoint_writing";
const char ha_lbl_alias_supports_central_heating_setpoint_writing[]         PROGMEM = "supports_central_heating_setpoint_writing";
const char ha_lbl_alias_supports_ventilation_bypass[]                       PROGMEM = "supports_ventilation_bypass";
// Category B: plain-name HA-core verbatim (12)
const char ha_lbl_alias_fault_indication[]                                  PROGMEM = "fault_indication";
const char ha_lbl_alias_central_heating[]                                   PROGMEM = "central_heating";
const char ha_lbl_alias_hot_water[]                                         PROGMEM = "hot_water";
const char ha_lbl_alias_central_heating_2[]                                 PROGMEM = "central_heating_2";
const char ha_lbl_alias_diagnostic_indication[]                             PROGMEM = "diagnostic_indication";
const char ha_lbl_alias_control_type[]                                      PROGMEM = "control_type";
const char ha_lbl_alias_hot_water_config[]                                  PROGMEM = "hot_water_config";
const char ha_lbl_alias_service_required[]                                  PROGMEM = "service_required";
const char ha_lbl_alias_gas_fault[]                                         PROGMEM = "gas_fault";
const char ha_lbl_alias_water_overtemperature[]                             PROGMEM = "water_overtemperature";
const char ha_lbl_alias_override_manual_change_prio[]                       PROGMEM = "override_manual_change_prio";
const char ha_lbl_alias_override_program_change_prio[]                      PROGMEM = "override_program_change_prio";
// Category C: plain-name firmware-coined (13)
const char ha_lbl_alias_ventilation_enabled[]                               PROGMEM = "ventilation_enabled";
const char ha_lbl_alias_ventilation_bypass_position[]                       PROGMEM = "ventilation_bypass_position";
const char ha_lbl_alias_ventilation_bypass_mode[]                           PROGMEM = "ventilation_bypass_mode";
const char ha_lbl_alias_ventilation_free_mode[]                             PROGMEM = "ventilation_free_mode";
const char ha_lbl_alias_ventilation_fault[]                                 PROGMEM = "ventilation_fault";
const char ha_lbl_alias_ventilation_active[]                                PROGMEM = "ventilation_active";
const char ha_lbl_alias_ventilation_bypass_status[]                         PROGMEM = "ventilation_bypass_status";
const char ha_lbl_alias_ventilation_bypass_automatic[]                      PROGMEM = "ventilation_bypass_automatic";
const char ha_lbl_alias_ventilation_free_status[]                           PROGMEM = "ventilation_free_status";
const char ha_lbl_alias_ventilation_diagnostic[]                            PROGMEM = "ventilation_diagnostic";
const char ha_lbl_alias_ventilation_system_type[]                           PROGMEM = "ventilation_system_type";
const char ha_lbl_alias_ventilation_speed_control_type[]                    PROGMEM = "ventilation_speed_control_type";
const char ha_lbl_alias_solar_storage_fault[]                               PROGMEM = "solar_storage_fault";

// ========== Named PROGMEM strings: Friendly names ==========
const char ha_name_status_master[] PROGMEM = "Status_Master";
const char ha_name_status_slave[] PROGMEM = "Status_Slave";
const char ha_name_control_setpoint[] PROGMEM = "Control_setpoint";
const char ha_name_status_master_configuration[] PROGMEM = "Status_Master_Configuration";
const char ha_name_status_master_memberid_code[] PROGMEM = "Status_Master_MemberID_Code";
const char ha_name_status_slave_configuration[] PROGMEM = "Status_Slave_Configuration";
const char ha_name_status_slave_memberid_code[] PROGMEM = "Status_Slave_MemberID_Code";
const char ha_name_remote_command_code[] PROGMEM = "Remote_Command_Code";
const char ha_name_remote_command_response[] PROGMEM = "Remote_Command_Response";
const char ha_name_remote_command[] PROGMEM = "Remote_Command";
const char ha_name_application_specific_fault[] PROGMEM = "Application_Specific_Fault";
const char ha_name_oemfaultcode[] PROGMEM = "OEM_Fault_Code";
const char ha_name_rbp_flags_read_write[] PROGMEM = "RBP_flags_read_write";
const char ha_name_rbp_flags_transfer_enable[] PROGMEM = "RBP_flags_transfer_enable";
const char ha_name_cooling_control_signal[] PROGMEM = "Cooling_control_signal";
const char ha_name_control_setpoint_2[] PROGMEM = "Control_setpoint_2";
const char ha_name_remote_override_room_setpoint[] PROGMEM = "Remote_override_room_setpoint";
const char ha_name_tsp_count[] PROGMEM = "TSP_Count";
const char ha_name_tsp_index[] PROGMEM = "TSP_Index";
const char ha_name_tsp_entry_index[] PROGMEM = "TSP_Entry_Index";
const char ha_name_tsp_entry_value[] PROGMEM = "TSP_Entry_Value";
const char ha_name_fault_history_buffer_size[] PROGMEM = "Fault_History_Buffer_Size";
const char ha_name_fault_history_buffer_max[] PROGMEM = "Fault_History_Buffer_Max";
const char ha_name_fault_history_index[] PROGMEM = "Fault_History_Index";
const char ha_name_fault_history_value[] PROGMEM = "Fault_History_Value";
const char ha_name_max_rel_modulation_level_setting[] PROGMEM = "Max_Rel_Modulation_level_setting";
const char ha_name_maxcapacityminmodlevel_hb_u8[] PROGMEM = "Max_Capacity_Min_Mod_Level_HB_u8";
const char ha_name_maxcapacityminmodlevel_lb_u8[] PROGMEM = "Max_Capacity_Min_Mod_Level_LB_u8";
const char ha_name_room_setpoint[] PROGMEM = "Room_setpoint";
const char ha_name_relative_modulation_level[] PROGMEM = "Relative_Modulation_Level";
const char ha_name_water_pressure_in_ch_circuit[] PROGMEM = "Water_pressure_in_CH_circuit";
const char ha_name_water_flow_rate_in_dhw_circuit[] PROGMEM = "Water_flow_rate_in_DHW_circuit";
const char ha_name_water_flow_rate_in_dhw_circuit_2[] PROGMEM = "Water_flow_rate_in_DHW_circuit";
const char ha_name_daytime_dayofweek[] PROGMEM = "DayTime_Day_Of_Week";
const char ha_name_daytime_hour[] PROGMEM = "DayTime_Hour";
const char ha_name_daytime_minutes[] PROGMEM = "DayTime_Minutes";
const char ha_name_date_day_of_month[] PROGMEM = "Date_day_of_month";
const char ha_name_date_month[] PROGMEM = "Date_month";
const char ha_name_year[] PROGMEM = "Year";
const char ha_name_room_setpoint_ch2[] PROGMEM = "Room_Setpoint_CH2";
const char ha_name_room_temperature[] PROGMEM = "Room_Temperature";
const char ha_name_boiler_flow_water_temperature[] PROGMEM = "Boiler_flow_water_temperature";
const char ha_name_dhw_temperature[] PROGMEM = "DHW_temperature";
const char ha_name_outside_temperature[] PROGMEM = "Outside_Temperature";
const char ha_name_return_water_temperature[] PROGMEM = "Return_water_temperature";
const char ha_name_solar_storage_temperature[] PROGMEM = "Solar_storage_temperature";
const char ha_name_solar_collector_temperature[] PROGMEM = "Solar_collector_temperature";
const char ha_name_flow_water_temperature_ch2_cir[] PROGMEM = "Flow_water_temperature_CH2_circuit";
const char ha_name_flow_water_temperature_ch2[] PROGMEM = "Flow_water_temperature_CH2";
const char ha_name_domestic_hot_water_temperature_2[] PROGMEM = "Domestic_hot_water_temperature_2";
const char ha_name_boiler_exhaust_temperature[] PROGMEM = "Boiler_exhaust_temperature";
const char ha_name_heat_exchanger_temperature[] PROGMEM = "Heat_Exchanger_Temperature";
const char ha_name_boiler_fan_speed_setpoint[] PROGMEM = "Boiler_fan_speed_setpoint";
const char ha_name_boiler_fan_speed_actual[] PROGMEM = "Boiler_fan_speed_actual";
const char ha_name_electricalcurrentburnerflame[] PROGMEM = "Electrical_Current_Burner_Flame";
const char ha_name_room_temperature_ch2[] PROGMEM = "Room_Temperature_CH2";
const char ha_name_relative_humidity[] PROGMEM = "Relative_Humidity";
const char ha_name_remote_override_setpoint_ch2[] PROGMEM = "Remote_Override_Setpoint_CH2";
const char ha_name_tdhwsetubtdhwsetlb_value_hb[] PROGMEM = "Tdhw_Set_UB_Tdhw_Set_LB_value_hb";
const char ha_name_tdhwsetubtdhwsetlb_value_lb[] PROGMEM = "Tdhw_Set_UB_Tdhw_Set_LB_value_lb";
const char ha_name_maxtsetubmaxtsetlb_value_hb[] PROGMEM = "Max_TSet_UB_Max_TSet_LB_value_hb";
const char ha_name_maxtsetubmaxtsetlb_value_lb[] PROGMEM = "Max_TSet_UB_Max_TSet_LB_value_lb";
const char ha_name_hcratioubhcratiolb_value_hb[] PROGMEM = "HCratio_UB_HCratio_LB_value_hb";
const char ha_name_hcratioubhcratiolb_value_lb[] PROGMEM = "HCratio_UB_HCratio_LB_value_lb";
const char ha_name_remote_parameter_4_boundary_hb[] PROGMEM = "Remote_Parameter_4_Boundary_HB";
const char ha_name_remote_parameter_4_boundary_lb[] PROGMEM = "Remote_Parameter_4_Boundary_LB";
const char ha_name_remote_parameter_5_boundary_hb[] PROGMEM = "Remote_Parameter_5_Boundary_HB";
const char ha_name_remote_parameter_5_boundary_lb[] PROGMEM = "Remote_Parameter_5_Boundary_LB";
const char ha_name_remote_parameter_6_boundary_hb[] PROGMEM = "Remote_Parameter_6_Boundary_HB";
const char ha_name_remote_parameter_6_boundary_lb[] PROGMEM = "Remote_Parameter_6_Boundary_LB";
const char ha_name_remote_parameter_7_boundary_hb[] PROGMEM = "Remote_Parameter_7_Boundary_HB";
const char ha_name_remote_parameter_7_boundary_lb[] PROGMEM = "Remote_Parameter_7_Boundary_LB";
const char ha_name_remote_parameter_8_boundary_hb[] PROGMEM = "Remote_Parameter_8_Boundary_HB";
const char ha_name_remote_parameter_8_boundary_lb[] PROGMEM = "Remote_Parameter_8_Boundary_LB";
const char ha_name_dhw_setpoint[] PROGMEM = "DHW_setpoint";
const char ha_name_max_ch_water_setpoint[] PROGMEM = "Max_CH_water_setpoint";
const char ha_name_otc_heat_curve_ratio[] PROGMEM = "OTC_heat_curve_ratio";
const char ha_name_remote_parameter_4[] PROGMEM = "Remote_Parameter_4";
const char ha_name_remote_parameter_5[] PROGMEM = "Remote_Parameter_5";
const char ha_name_remote_parameter_6[] PROGMEM = "Remote_Parameter_6";
const char ha_name_remote_parameter_7[] PROGMEM = "Remote_Parameter_7";
const char ha_name_remote_parameter_8[] PROGMEM = "Remote_Parameter_8";
const char ha_name_status_vh_master[] PROGMEM = "status_VH_master";
const char ha_name_status_vh_slave[] PROGMEM = "status_VH_slave";
const char ha_name_vh_relative_ventilation_position[] PROGMEM = "VH_relative_ventilation_position";
const char ha_name_controlsetpointvh_hb_u8[] PROGMEM = "Control_Setpoint_VH_HB_u8";
const char ha_name_controlsetpointvh_lb_u8[] PROGMEM = "Control_Setpoint_VH_LB_u8";
const char ha_name_asffaultcodevh_code[] PROGMEM = "ASF_Fault_Code_VH_code";
const char ha_name_asffaultcodevh_flag8[] PROGMEM = "ASF_Fault_Code_VH_flag8";
const char ha_name_diagnosticcodevh[] PROGMEM = "Diagnostic_Code_VH";
const char ha_name_vh_configuration[] PROGMEM = "VH_configuration";
const char ha_name_vh_memberid_code[] PROGMEM = "VH_MemberID_Code";
const char ha_name_openthermversionvh[] PROGMEM = "OpenTherm_Version_VH";
const char ha_name_versiontypevh_hb_u8[] PROGMEM = "Version_Type_VH_HB_u8";
const char ha_name_versiontypevh_lb_u8[] PROGMEM = "Version_Type_VH_LB_u8";
const char ha_name_relative_ventilation[] PROGMEM = "Relative_Ventilation";
const char ha_name_relativeventilation_hb_u8[] PROGMEM = "Relative_Ventilation_HB_u8";
const char ha_name_relativeventilation_lb_u8[] PROGMEM = "Relative_Ventilation_LB_u8";
const char ha_name_relativeventilation[] PROGMEM = "Relative_Ventilation";
const char ha_name_relative_humidity_exhaust_air[] PROGMEM = "Relative_Humidity_Exhaust_Air";
const char ha_name_relativehumidityexhaustair_hb_u8[] PROGMEM = "Relative_Humidity_Exhaust_Air_HB_u8";
const char ha_name_relativehumidityexhaustair_lb_u8[] PROGMEM = "Relative_Humidity_Exhaust_Air_LB_u8";
const char ha_name_co2_level_exhaust_air[] PROGMEM = "CO2_Level_Exhaust_Air";
const char ha_name_supply_inlet_temperature[] PROGMEM = "Supply_Inlet_Temperature";
const char ha_name_supplyinlettemperature[] PROGMEM = "Supply_Inlet_Temperature";
const char ha_name_supply_outlet_temperature[] PROGMEM = "Supply_Outlet_Temperature";
const char ha_name_supplyoutlettemperature[] PROGMEM = "Supply_Outlet_Temperature";
const char ha_name_exhaust_inlet_temperature[] PROGMEM = "Exhaust_Inlet_Temperature";
const char ha_name_exhaustinlettemperature[] PROGMEM = "Exhaust_Inlet_Temperature";
const char ha_name_exhaust_outlet_temperature[] PROGMEM = "Exhaust_Outlet_Temperature";
const char ha_name_exhaustoutlettemperature[] PROGMEM = "Exhaust_Outlet_Temperature";
const char ha_name_exhaust_fan_speed[] PROGMEM = "Exhaust_Fan_Speed";
const char ha_name_supply_fan_speed[] PROGMEM = "Supply_Fan_Speed";
const char ha_name_remoteparametersettingvh_hb_flag8[] PROGMEM = "Remote_Parameter_Setting_VH_HB_flag8";
const char ha_name_remoteparametersettingvh_lb_flag8[] PROGMEM = "Remote_Parameter_Setting_VH_LB_flag8";
const char ha_name_vh_rw_nominal_ventilation_value[] PROGMEM = "VH_rw_nominal_ventilation_value";
const char ha_name_vh_transfer_enable_nominal_ventilation_value[] PROGMEM = "VH_transfer_enable_nominal_ventilation_value";
const char ha_name_nominalventilationvalue[] PROGMEM = "Nominal_Ventilation_Value";
const char ha_name_nominalventilationvalue_hb_u8[] PROGMEM = "Nominal_Ventilation_Value_HB_u8";
const char ha_name_nominalventilationvalue_lb_u8[] PROGMEM = "Nominal_Ventilation_Value_LB_u8";
const char ha_name_tspnumbervh_hb_u8[] PROGMEM = "TSP_Number_VH_HB_u8";
const char ha_name_tspnumbervh_lb_u8[] PROGMEM = "TSP_Number_VH_LB_u8";
const char ha_name_tspentryvh_hb_u8[] PROGMEM = "TSP_Entry_VH_HB_u8";
const char ha_name_tspentryvh_lb_u8[] PROGMEM = "TSP_Entry_VH_LB_u8";
const char ha_name_faultbuffersizevh_hb_u8[] PROGMEM = "Fault_Buffer_Size_VH_HB_u8";
const char ha_name_faultbuffersizevh_lb_u8[] PROGMEM = "Fault_Buffer_Size_VH_LB_u8";
const char ha_name_faultbufferentryvh_hb_u8[] PROGMEM = "Fault_Buffer_Entry_VH_HB_u8";
const char ha_name_faultbufferentryvh_lb_u8[] PROGMEM = "Fault_Buffer_Entry_VH_LB_u8";
const char ha_name_brand_hb_u8[] PROGMEM = "Brand_HB_u8";
const char ha_name_brand_lb_u8[] PROGMEM = "Brand_LB_u8";
const char ha_name_brandversion_hb_u8[] PROGMEM = "Brand_Version_HB_u8";
const char ha_name_brandversion_lb_u8[] PROGMEM = "Brand_Version_LB_u8";
const char ha_name_brandserialnumber_hb_u8[] PROGMEM = "Brand_Serial_Number_HB_u8";
const char ha_name_brandserialnumber_lb_u8[] PROGMEM = "Brand_Serial_Number_LB_u8";
const char ha_name_coolingoperationhours[] PROGMEM = "Cooling_Operation_Hours";
const char ha_name_powercycles[] PROGMEM = "Power_Cycles";
const char ha_name_rf_sensor_battery[] PROGMEM = "RF_Sensor_Battery";
const char ha_name_rf_sensor_battery_code[] PROGMEM = "RF_Sensor_Battery_Code";
const char ha_name_rf_sensor_index[] PROGMEM = "RF_Sensor_Index";
const char ha_name_rf_sensor_type[] PROGMEM = "RF_Sensor_Type";
const char ha_name_rf_sensor_type_code[] PROGMEM = "RF_Sensor_Type_Code";
const char ha_name_rf_signal_strength[] PROGMEM = "RF_Signal_Strength";
const char ha_name_rf_signal_strength_code[] PROGMEM = "RF_Signal_Strength_Code";
const char ha_name_rf_signal_battery_hb[] PROGMEM = "RF_Signal_Battery_HB";
const char ha_name_rf_signal_battery_lb[] PROGMEM = "RF_Signal_Battery_LB";
const char ha_name_operatingmode_hc1_hc2_dhw_hb_u8[] PROGMEM = "Operating_Mode_HC1_HC2_DHW_HB_u8";
const char ha_name_operatingmode_hc1_hc2_dhw_lb_u8[] PROGMEM = "Operating_Mode_HC1_HC2_DHW_LB_u8";
const char ha_name_remoteoverrideoperatingmode_dhw_mode[] PROGMEM = "Remote_Override_Operating_Mode_DHW_mode";
const char ha_name_remoteoverrideoperatingmode_dhw_mode_code[] PROGMEM = "Remote_Override_Operating_Mode_DHW_mode_code";
const char ha_name_remoteoverrideoperatingmode_hc1_mode[] PROGMEM = "Remote_Override_Operating_Mode_HC1_mode";
const char ha_name_remoteoverrideoperatingmode_hc1_mode_code[] PROGMEM = "Remote_Override_Operating_Mode_HC1_mode_code";
const char ha_name_remoteoverrideoperatingmode_hc2_mode[] PROGMEM = "Remote_Override_Operating_Mode_HC2_mode";
const char ha_name_remoteoverrideoperatingmode_hc2_mode_code[] PROGMEM = "Remote_Override_Operating_Mode_HC2_mode_code";
const char ha_name_remoteoverrideoperatingmode_manual_dhw_push[] PROGMEM = "Remote_Override_Operating_Mode_manual_DHW_push";
const char ha_name_roomremoteoverridefunction_flag8[] PROGMEM = "Room_Remote_Override_Function_flag8";
const char ha_name_solar_storage_master_mode[] PROGMEM = "solar_storage_master_mode";
const char ha_name_solar_storage_mode_status[] PROGMEM = "solar_storage_mode_status";
const char ha_name_solar_storage_slave_status[] PROGMEM = "solar_storage_slave_status";
const char ha_name_solarstorageasfflags_code[] PROGMEM = "Solar_Storage_ASF_Flags_code";
const char ha_name_solarstorageasfflags_flag8[] PROGMEM = "Solar_Storage_ASF_Flags_flag8";
const char ha_name_solar_storage_slave_configuration[] PROGMEM = "solar_storage_slave_configuration";
const char ha_name_solar_storage_slave_memberid_code[] PROGMEM = "Solar_Storage_Slave_MemberID_Code";
const char ha_name_solarstorageversiontype_hb_u8[] PROGMEM = "Solar_Storage_Version_Type_HB_u8";
const char ha_name_solarstorageversiontype_lb_u8[] PROGMEM = "Solar_Storage_Version_Type_LB_u8";
const char ha_name_solarstoragetsp_hb_u8[] PROGMEM = "Solar_Storage_TSP_HB_u8";
const char ha_name_solarstoragetsp_lb_u8[] PROGMEM = "Solar_Storage_TSP_LB_u8";
const char ha_name_solarstoragetspindextspvalue_hb_u8[] PROGMEM = "Solar_Storage_TSP_Index_TSP_Value_hb_u8";
const char ha_name_solarstoragetspindextspvalue_lb_u8[] PROGMEM = "Solar_Storage_TSP_Index_TSP_Value_lb_u8";
const char ha_name_solarstoragefhbsize_hb_u8[] PROGMEM = "Solar_Storage_FHB_Size_hb_u8";
const char ha_name_solarstoragefhbsize_lb_u8[] PROGMEM = "Solar_Storage_FHB_Size_lb_u8";
const char ha_name_solarstoragefhbindexfhbvalue_hb_u8[] PROGMEM = "Solar_Storage_FHB_Index_FHB_Value_hb_u8";
const char ha_name_solarstoragefhbindexfhbvalue_lb_u8[] PROGMEM = "Solar_Storage_FHB_Index_FHB_Value_lb_u8";
const char ha_name_electricityproducerstarts[] PROGMEM = "Electricity_Producer_Starts";
const char ha_name_electricityproducerhours[] PROGMEM = "Electricity_Producer_Hours";
const char ha_name_electricityproduction[] PROGMEM = "Electricity_Production";
const char ha_name_cumulativeelectricityproduction[] PROGMEM = "Cumulative_Electricity_Production";
const char ha_name_burnerunsuccessfulstarts[] PROGMEM = "Burner_Unsuccessful_Starts";
const char ha_name_flamesignaltoolow[] PROGMEM = "Flame_Signal_Too_Low";
const char ha_name_oemdiagnosticcode[] PROGMEM = "OEM_Diagnostic_Code";
const char ha_name_burnerstarts[] PROGMEM = "Burner_Starts";
const char ha_name_chpumpstarts[] PROGMEM = "CH_Pump_Starts";
const char ha_name_dhwpumpvalvestarts[] PROGMEM = "DHW_Pump_Valve_Starts";
const char ha_name_dhwburnerstarts[] PROGMEM = "DHW_Burner_Starts";
const char ha_name_burneroperationhours[] PROGMEM = "Burner_Operation_Hours";
const char ha_name_chpumpoperationhours[] PROGMEM = "CH_Pump_Operation_Hours";
const char ha_name_dhwpumpvalveoperationhours[] PROGMEM = "DHW_Pump_Valve_Operation_Hours";
const char ha_name_dhwburneroperationhours_dhw[] PROGMEM = "DHW_Burner_Operation_Hours";
const char ha_name_dhwburneroperationhours[] PROGMEM = "DHW_Burner_Operation_Hours";
const char ha_name_master_ot_protocol_version[] PROGMEM = "Master_OT_protocol_version";
const char ha_name_slave_ot_protocol_version[] PROGMEM = "Slave_OT_protocol_version";
const char ha_name_masterversion_hb_u8[] PROGMEM = "Master_Version_HB_u8";
const char ha_name_masterversion_lb_u8[] PROGMEM = "Master_Version_LB_u8";
const char ha_name_slaveversion_hb_u8[] PROGMEM = "Slave_Version_HB_u8";
const char ha_name_slaveversion_lb_u8[] PROGMEM = "Slave_Version_LB_u8";
const char ha_name_remeha_fd_u_codes_hb[] PROGMEM = "Remeha_FD_U_Codes_HB";
const char ha_name_remeha_fd_u_codes_lb[] PROGMEM = "Remeha_FD_U_Codes_LB";
const char ha_name_remeha_service_message_hb[] PROGMEM = "Remeha_Service_Message_HB";
const char ha_name_remeha_service_message_lb[] PROGMEM = "Remeha_Service_Message_LB";
const char ha_name_remeha_detection_connected_scu_hb[] PROGMEM = "Remeha_Detection_Connected_SCU_HB";
const char ha_name_remeha_detection_connected_scu_lb[] PROGMEM = "Remeha_Detection_Connected_SCU_LB";
const char ha_name_s0_power_kw[] PROGMEM = "S0_Power_kw";
const char ha_name_s0_pulse_count[] PROGMEM = "S0_Pulse_Count";
const char ha_name_s0_pulse_count_total[] PROGMEM = "S0_Pulse_Count_Total";
const char ha_name_s0_pulse_time[] PROGMEM = "S0_Pulse_Time";
const char ha_name_sensor_id[] PROGMEM = "%sensor_id%";
// Heap & discovery statistics friendly names (TASK-346, faux dataid 247)
const char ha_name_stats_ws_drops[] PROGMEM                = "Stats_WS_Drops";
const char ha_name_stats_mqtt_drops[] PROGMEM              = "Stats_MQTT_Drops";
const char ha_name_stats_enter_low[] PROGMEM               = "Stats_Enter_Low";
const char ha_name_stats_enter_warning[] PROGMEM           = "Stats_Enter_Warning";
const char ha_name_stats_enter_critical[] PROGMEM          = "Stats_Enter_Critical";
const char ha_name_stats_drip_burst_skip[] PROGMEM         = "Stats_Drip_Burst_Skip";
const char ha_name_stats_drip_cooldown_skip[] PROGMEM      = "Stats_Drip_Cooldown_Skip";
const char ha_name_stats_drip_slowmode[] PROGMEM           = "Stats_Drip_Slowmode";
const char ha_name_stats_free_heap[] PROGMEM               = "Stats_Free_Heap";
const char ha_name_stats_max_block[] PROGMEM               = "Stats_Max_Block";
const char ha_name_stats_frag_pct[] PROGMEM                = "Stats_Fragmentation";
const char ha_name_stats_disc_verify_runs[] PROGMEM        = "Stats_Discovery_Verify_Runs";
const char ha_name_stats_disc_republish_triggered[] PROGMEM = "Stats_Discovery_Republish_Triggered";
const char ha_name_stats_disc_last_missing[] PROGMEM       = "Stats_Discovery_Last_Missing";
const char ha_name_stats_disc_last_orphan[] PROGMEM        = "Stats_Discovery_Last_Orphan";
const char ha_name_stats_disc_published_topics[] PROGMEM   = "Stats_Discovery_Published_Topics";
const char ha_name_stats_disc_last_verify_epoch[] PROGMEM  = "Stats_Discovery_Last_Verify_Epoch";
// Firmware diagnostic friendly names (TASK-540 / TASK-541, faux dataid 248)
const char ha_name_fw_reboot_count[]  PROGMEM = "Reboot_Count";
const char ha_name_fw_reboot_reason[] PROGMEM = "Reboot_Reason";
const char ha_name_fw_version[]       PROGMEM = "Firmware_Version";
const char ha_name_fw_hostname[]      PROGMEM = "Hostname";
const char ha_name_fw_hardware_type[] PROGMEM = "Hardware_Type";  // ADR-113
// PIC info friendly names (TASK-540 / TASK-541, faux dataid 249)
const char ha_name_pic_version[]       PROGMEM = "PIC_Version";
const char ha_name_pic_deviceid[]      PROGMEM = "PIC_DeviceID";
const char ha_name_pic_firmwaretype[]  PROGMEM = "PIC_FirmwareType";
const char ha_name_pic_designer[]      PROGMEM = "PIC_Designer";
// ADR-113 stage 2 (TASK-754): ha_name_pic_available removed.
// PIC settings friendly names (TASK-540 / TASK-541, faux dataid 250)
const char ha_name_pic_set_setpoint_override[]  PROGMEM = "PIC_Setpoint_Override";
const char ha_name_pic_set_setback[]             PROGMEM = "PIC_Setback";
const char ha_name_pic_set_dhw_override[]        PROGMEM = "PIC_DHW_Override";
const char ha_name_pic_set_gpio[]                PROGMEM = "PIC_GPIO";
const char ha_name_pic_set_gpio_states[]         PROGMEM = "PIC_GPIO_States";
const char ha_name_pic_set_led[]                 PROGMEM = "PIC_LED";
const char ha_name_pic_set_tweaks[]              PROGMEM = "PIC_Tweaks";
const char ha_name_pic_set_temp_sensor[]         PROGMEM = "PIC_Temp_Sensor";
const char ha_name_pic_set_smart_power[]         PROGMEM = "PIC_Smart_Power";
const char ha_name_pic_set_thermostat_detect[]   PROGMEM = "PIC_Thermostat_Detect";
const char ha_name_pic_set_builddate[]           PROGMEM = "PIC_Builddate";
const char ha_name_pic_set_clock_mhz[]           PROGMEM = "PIC_Clock_MHz";
const char ha_name_pic_set_reset_cause[]         PROGMEM = "PIC_Reset_Cause";
const char ha_name_pic_set_standalone_interval[] PROGMEM = "PIC_Standalone_Interval";
const char ha_name_pic_set_voltage_ref[]         PROGMEM = "PIC_Voltage_Ref";
// 2.0.0-specific diagnostic friendly names (TASK-541, faux dataid 251)
const char ha_name_otdirect_flame_duty_pct[]      PROGMEM = "OTDirect_Flame_Duty_Pct";
const char ha_name_otdirect_flame_cycles_per_hr[] PROGMEM = "OTDirect_Flame_Cycles_Per_Hour";
const char ha_name_sat_ble_sensor_rssi[]          PROGMEM = "SAT_BLE_Sensor_RSSI";
const char ha_name_sat_ble_battery[]              PROGMEM = "SAT_BLE_Battery";
const char ha_name_sat_ble_sensor_count[]         PROGMEM = "SAT_BLE_Sensor_Count";
const char ha_name_sat_ble_temp_valid[]           PROGMEM = "SAT_BLE_Temp_Valid";
const char ha_name_sat_ch_pressure_status[]       PROGMEM = "SAT_CH_Pressure_Status";
#undef DECLARE_SAT_DISCOVERY_STRINGS
const char ha_name_central_heating[] PROGMEM = "Central_Heating";
const char ha_name_central_heating_2[] PROGMEM = "Central_Heating_2";
const char ha_name_central_heating_2_enable[] PROGMEM = "central_heating_2_enable";
const char ha_name_central_heating_enable[] PROGMEM = "Central_Heating_enable";
const char ha_name_cooling[] PROGMEM = "Cooling";
const char ha_name_cooling_enable[] PROGMEM = "Cooling_enable";
const char ha_name_dhw_blocking[] PROGMEM = "DHW_blocking";
const char ha_name_domestic_hot_water_enable[] PROGMEM = "Domestic_Hot_Water_enable";
const char ha_name_diagnostic_indicator[] PROGMEM = "Diagnostic_Indicator";
const char ha_name_domestic_hot_water[] PROGMEM = "Domestic_Hot_Water";
const char ha_name_electric_production[] PROGMEM = "Electric_Production";
const char ha_name_fault[] PROGMEM = "Fault";
const char ha_name_flame[] PROGMEM = "Flame";
const char ha_name_otc_enable[] PROGMEM = "OTC_enable";
const char ha_name_summerwintertime[] PROGMEM = "Summer_Winter_Time";
const char ha_name_boiler_connected[] PROGMEM = "Boiler_Connected";
const char ha_name_thermostat_connected[] PROGMEM = "Thermostat_Connected";
const char ha_name_master_configuration_smart_power[] PROGMEM = "master_configuration_smart_power";
const char ha_name_ch2_present[] PROGMEM = "CH2_present";
const char ha_name_control_type_modulation[] PROGMEM = "control_type_modulation";
const char ha_name_cooling_configs[] PROGMEM = "Cooling_configs";
const char ha_name_dhw_config[] PROGMEM = "DHW_config";
const char ha_name_dhw_present[] PROGMEM = "DHW_present";
const char ha_name_heat_cool_mode_control[] PROGMEM = "heat_cool_mode_control";
const char ha_name_master_low_off_pump_control_function[] PROGMEM = "Master_low_off_pump_control_function";
const char ha_name_remote_water_filling_function[] PROGMEM = "remote_water_filling_function";
const char ha_name_air_press_fault[] PROGMEM = "Air_press_fault";
const char ha_name_gas_flame_fault[] PROGMEM = "Gas_flame_fault";
const char ha_name_lockout_reset[] PROGMEM = "Lockout_reset";
const char ha_name_low_water_press[] PROGMEM = "Low_water_press";
const char ha_name_service_request[] PROGMEM = "Service_request";
const char ha_name_water_over_temp[] PROGMEM = "Water_over_temp";
const char ha_name_rbp_dhw_setpoint[] PROGMEM = "RBP_DHW_setpoint";
const char ha_name_rbp_max_ch_setpoint[] PROGMEM = "RBP_max_CH_setpoint";
const char ha_name_rbp_rw_dhw_setpoint[] PROGMEM = "RBP_rw_DHW_setpoint";
const char ha_name_rbp_rw_max_ch_setpoint[] PROGMEM = "RBP_rw_max_CH_setpoint";
const char ha_name_vh_bypass_automatic_status[] PROGMEM = "VH_bypass_automatic_status";
const char ha_name_vh_bypass_mode[] PROGMEM = "VH_bypass_mode";
const char ha_name_vh_bypass_position[] PROGMEM = "VH_bypass_position";
const char ha_name_vh_bypass_status[] PROGMEM = "VH_bypass_status";
const char ha_name_vh_diagnostic_indicator[] PROGMEM = "VH_diagnostic_indicator";
const char ha_name_vh_fault[] PROGMEM = "VH_fault";
const char ha_name_vh_free_ventilation_mode[] PROGMEM = "VH_free_ventilation_mode";
const char ha_name_vh_free_ventliation_status[] PROGMEM = "VH_free_ventliation_status";
const char ha_name_vh_ventilation_enabled[] PROGMEM = "VH_ventilation_enabled";
const char ha_name_vh_ventilation_mode[] PROGMEM = "VH_ventilation_mode";
const char ha_name_vh_configuration_bypass[] PROGMEM = "VH_configuration_bypass";
const char ha_name_vh_configuration_speed_control[] PROGMEM = "VH_configuration_speed_control";
const char ha_name_vh_configuration_system_type[] PROGMEM = "VH_configuration_system_type";
const char ha_name_remote_override_manual_change_priority[] PROGMEM = "remote_override_manual_change_priority";
const char ha_name_remote_override_program_change_priority[] PROGMEM = "remote_override_program_change_priority";
const char ha_name_solar_storage_slave_fault_indicator[] PROGMEM = "solar_storage_slave_fault_indicator";
const char ha_name_solar_storage_system_type[] PROGMEM = "solar_storage_system_type";

// ---------------------------------------------------------------------------
// ADR-105: HA-core alias friendly names (37 entries)
// Category A: supports_* capability aliases (12)
const char ha_name_alias_supports_master_smart_power[]                       PROGMEM = "Supports_master_smart_power";
const char ha_name_alias_supports_hot_water[]                                PROGMEM = "Supports_hot_water";
const char ha_name_alias_supports_cooling[]                                  PROGMEM = "Supports_cooling";
const char ha_name_alias_supports_pump_control[]                             PROGMEM = "Supports_pump_control";
const char ha_name_alias_supports_ch_2[]                                     PROGMEM = "Supports_CH_2";
const char ha_name_alias_supports_remote_reset[]                             PROGMEM = "Supports_remote_reset";
const char ha_name_alias_supports_lockout_reset[]                            PROGMEM = "Supports_lockout_reset";
const char ha_name_alias_supports_hot_water_setpoint_transfer[]              PROGMEM = "Supports_hot_water_setpoint_transfer";
const char ha_name_alias_supports_central_heating_setpoint_transfer[]        PROGMEM = "Supports_central_heating_setpoint_transfer";
const char ha_name_alias_supports_hot_water_setpoint_writing[]               PROGMEM = "Supports_hot_water_setpoint_writing";
const char ha_name_alias_supports_central_heating_setpoint_writing[]         PROGMEM = "Supports_central_heating_setpoint_writing";
const char ha_name_alias_supports_ventilation_bypass[]                       PROGMEM = "Supports_ventilation_bypass";
// Category B: plain-name HA-core verbatim (12)
const char ha_name_alias_fault_indication[]                                  PROGMEM = "Fault_indication";
const char ha_name_alias_central_heating[]                                   PROGMEM = "Central_heating";
const char ha_name_alias_hot_water[]                                         PROGMEM = "Hot_water";
const char ha_name_alias_central_heating_2[]                                 PROGMEM = "Central_heating_2";
const char ha_name_alias_diagnostic_indication[]                             PROGMEM = "Diagnostic_indication";
const char ha_name_alias_control_type[]                                      PROGMEM = "Control_type";
const char ha_name_alias_hot_water_config[]                                  PROGMEM = "Hot_water_config";
const char ha_name_alias_service_required[]                                  PROGMEM = "Service_required";
const char ha_name_alias_gas_fault[]                                         PROGMEM = "Gas_fault";
const char ha_name_alias_water_overtemperature[]                             PROGMEM = "Water_overtemperature";
const char ha_name_alias_override_manual_change_prio[]                       PROGMEM = "Override_manual_change_prio";
const char ha_name_alias_override_program_change_prio[]                      PROGMEM = "Override_program_change_prio";
// Category C: plain-name firmware-coined (13)
const char ha_name_alias_ventilation_enabled[]                               PROGMEM = "Ventilation_enabled";
const char ha_name_alias_ventilation_bypass_position[]                       PROGMEM = "Ventilation_bypass_position";
const char ha_name_alias_ventilation_bypass_mode[]                           PROGMEM = "Ventilation_bypass_mode";
const char ha_name_alias_ventilation_free_mode[]                             PROGMEM = "Ventilation_free_mode";
const char ha_name_alias_ventilation_fault[]                                 PROGMEM = "Ventilation_fault";
const char ha_name_alias_ventilation_active[]                                PROGMEM = "Ventilation_active";
const char ha_name_alias_ventilation_bypass_status[]                         PROGMEM = "Ventilation_bypass_status";
const char ha_name_alias_ventilation_bypass_automatic[]                      PROGMEM = "Ventilation_bypass_automatic";
const char ha_name_alias_ventilation_free_status[]                           PROGMEM = "Ventilation_free_status";
const char ha_name_alias_ventilation_diagnostic[]                            PROGMEM = "Ventilation_diagnostic";
const char ha_name_alias_ventilation_system_type[]                           PROGMEM = "Ventilation_system_type";
const char ha_name_alias_ventilation_speed_control_type[]                    PROGMEM = "Ventilation_speed_control_type";
const char ha_name_alias_solar_storage_fault[]                               PROGMEM = "Solar_storage_fault";
// ========== Sensor array (289 entries, sorted by id) ==========
const uint16_t MQTT_HA_SENSOR_COUNT = 385;

const MqttHaSensorCfg PROGMEM mqttHaSensors[] = {
//  {id, flags, label, friendlyName, deviceClass, unit, stateClass, icon, entityCat, enabledByDefault}
    // --- OT ID 0 ---
    {  0, 0x00, ha_lbl_status_master, ha_name_status_master, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::list_status, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_status_slave, ha_name_status_slave, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::list_status, HaEntityCat::none, true},
    // --- OT ID 1 ---
    {  1, 0x00, ha_lbl_tset, ha_name_control_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    {  1, 0x07, ha_lbl_tset, ha_name_control_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 2 ---
    {  2, 0x00, ha_lbl_master_configuration, ha_name_status_master_configuration, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::cog, HaEntityCat::diagnostic, true},
    {  2, 0x00, ha_lbl_master_memberid_code, ha_name_status_master_memberid_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::card_account_details, HaEntityCat::diagnostic, true},
    // --- OT ID 3 ---
    {  3, 0x00, ha_lbl_slave_configuration, ha_name_status_slave_configuration, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::cog, HaEntityCat::diagnostic, true},
    {  3, 0x00, ha_lbl_slave_memberid_code, ha_name_status_slave_memberid_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::card_account_details, HaEntityCat::diagnostic, true},
    // --- OT ID 4 ---
    {  4, 0x00, ha_lbl_command_hb_u8, ha_name_remote_command_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::console, HaEntityCat::none, true},
    {  4, 0x00, ha_lbl_command_lb_u8, ha_name_remote_command_response, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::console, HaEntityCat::none, true},
    {  4, 0x00, ha_lbl_command_remote_command, ha_name_remote_command, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::console, HaEntityCat::none, true},
    // --- OT ID 5 ---
    {  5, 0x00, ha_lbl_asf_flags, ha_name_application_specific_fault, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, true},
    {  5, 0x00, ha_lbl_oemfaultcode, ha_name_oemfaultcode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, true},
    // --- OT ID 6 ---
    {  6, 0x00, ha_lbl_rbp_flags_read_write, ha_name_rbp_flags_read_write, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::diagnostic, true},
    {  6, 0x00, ha_lbl_rbp_flags_transfer_enable, ha_name_rbp_flags_transfer_enable, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::diagnostic, true},
    // --- OT ID 7 ---
    {  7, 0x00, ha_lbl_coolingcontrol, ha_name_cooling_control_signal, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::percent_outline, HaEntityCat::none, true},
    {  7, 0x07, ha_lbl_coolingcontrol, ha_name_cooling_control_signal, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::percent_outline, HaEntityCat::none, true},
    // --- OT ID 8 ---
    {  8, 0x00, ha_lbl_tsetch2, ha_name_control_setpoint_2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    {  8, 0x07, ha_lbl_tsetch2, ha_name_control_setpoint_2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 9 ---
    {  9, 0x00, ha_lbl_troverride, ha_name_remote_override_room_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    {  9, 0x07, ha_lbl_troverride, ha_name_remote_override_room_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 10 ---
    { 10, 0x00, ha_lbl_tsp_hb_u8, ha_name_tsp_count, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, true},
    { 10, 0x00, ha_lbl_tsp_lb_u8, ha_name_tsp_index, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, true},
    // --- OT ID 11 ---
    { 11, 0x00, ha_lbl_tspindextspvalue_hb_u8, ha_name_tsp_entry_index, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, true},
    { 11, 0x00, ha_lbl_tspindextspvalue_lb_u8, ha_name_tsp_entry_value, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, true},
    // --- OT ID 12 ---
    { 12, 0x00, ha_lbl_fhbsize_hb_u8, ha_name_fault_history_buffer_size, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, true},
    { 12, 0x00, ha_lbl_fhbsize_lb_u8, ha_name_fault_history_buffer_max, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, true},
    // --- OT ID 13 ---
    { 13, 0x00, ha_lbl_fhbindexfhbvalue_hb_u8, ha_name_fault_history_index, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, true},
    { 13, 0x00, ha_lbl_fhbindexfhbvalue_lb_u8, ha_name_fault_history_value, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, true},
    // --- OT ID 14 ---
    { 14, 0x00, ha_lbl_maxrelmodlevelsetting, ha_name_max_rel_modulation_level_setting, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::speedometer, HaEntityCat::none, true},
    { 14, 0x07, ha_lbl_maxrelmodlevelsetting, ha_name_max_rel_modulation_level_setting, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::speedometer, HaEntityCat::none, true},
    // --- OT ID 15 ---
    { 15, 0x00, ha_lbl_maxcapacityminmodlevel_hb_u8, ha_name_maxcapacityminmodlevel_hb_u8, HaDeviceClass::power, HaUnit::kW, HaStateClass::none, HaIcon::flash, HaEntityCat::diagnostic, true},
    { 15, 0x00, ha_lbl_maxcapacityminmodlevel_lb_u8, ha_name_maxcapacityminmodlevel_lb_u8, HaDeviceClass::power_factor, HaUnit::percent, HaStateClass::none, HaIcon::angle_acute, HaEntityCat::diagnostic, true},
    // --- OT ID 16 ---
    { 16, 0x00, ha_lbl_trset, ha_name_room_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 16, 0x07, ha_lbl_trset, ha_name_room_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 17 ---
    { 17, 0x00, ha_lbl_relmodlevel, ha_name_relative_modulation_level, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::percent_outline, HaEntityCat::none, true},
    { 17, 0x07, ha_lbl_relmodlevel, ha_name_relative_modulation_level, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::percent_outline, HaEntityCat::none, true},
    // --- OT ID 18 ---
    { 18, 0x00, ha_lbl_chpressure, ha_name_water_pressure_in_ch_circuit, HaDeviceClass::none, HaUnit::bar, HaStateClass::measurement, HaIcon::gauge, HaEntityCat::none, true},
    { 18, 0x07, ha_lbl_chpressure, ha_name_water_pressure_in_ch_circuit, HaDeviceClass::pressure, HaUnit::bar, HaStateClass::measurement, HaIcon::gauge, HaEntityCat::none, true},
    // --- OT ID 19 ---
    { 19, 0x00, ha_lbl_dhwflowrate, ha_name_water_flow_rate_in_dhw_circuit, HaDeviceClass::none, HaUnit::l_min, HaStateClass::measurement, HaIcon::water, HaEntityCat::none, true},
    { 19, 0x07, ha_lbl_dhwflowrate, ha_name_water_flow_rate_in_dhw_circuit_2, HaDeviceClass::none, HaUnit::l_min, HaStateClass::measurement, HaIcon::water, HaEntityCat::none, true},
    // --- OT ID 20 ---
    { 20, 0x00, ha_lbl_daytime_dayofweek, ha_name_daytime_dayofweek, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    { 20, 0x00, ha_lbl_daytime_hour, ha_name_daytime_hour, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    { 20, 0x00, ha_lbl_daytime_minutes, ha_name_daytime_minutes, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    // --- OT ID 21 ---
    { 21, 0x00, ha_lbl_date_day_of_month, ha_name_date_day_of_month, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    { 21, 0x00, ha_lbl_date_month, ha_name_date_month, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    // --- OT ID 22 ---
    { 22, 0x00, ha_lbl_year, ha_name_year, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    { 22, 0x07, ha_lbl_year, ha_name_year, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::calendar, HaEntityCat::none, true},
    // --- OT ID 23 ---
    { 23, 0x00, ha_lbl_trsetch2, ha_name_room_setpoint_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 23, 0x07, ha_lbl_trsetch2, ha_name_room_setpoint_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 24 ---
    { 24, 0x00, ha_lbl_tr, ha_name_room_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 24, 0x07, ha_lbl_tr, ha_name_room_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 25 ---
    { 25, 0x00, ha_lbl_tboiler, ha_name_boiler_flow_water_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 25, 0x07, ha_lbl_tboiler, ha_name_boiler_flow_water_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 26 ---
    { 26, 0x00, ha_lbl_tdhw, ha_name_dhw_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 26, 0x07, ha_lbl_tdhw, ha_name_dhw_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 27 ---
    { 27, 0x00, ha_lbl_toutside, ha_name_outside_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 27, 0x07, ha_lbl_toutside, ha_name_outside_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 28 ---
    { 28, 0x00, ha_lbl_tret, ha_name_return_water_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 28, 0x07, ha_lbl_tret, ha_name_return_water_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 29 ---
    { 29, 0x00, ha_lbl_tsolarstorage, ha_name_solar_storage_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 29, 0x07, ha_lbl_tsolarstorage, ha_name_solar_storage_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 30 ---
    { 30, 0x00, ha_lbl_tsolarcollector, ha_name_solar_collector_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 30, 0x07, ha_lbl_tsolarcollector, ha_name_solar_collector_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 31 ---
    { 31, 0x00, ha_lbl_tflowch2, ha_name_flow_water_temperature_ch2_cir, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 31, 0x07, ha_lbl_tflowch2, ha_name_flow_water_temperature_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 32 ---
    { 32, 0x00, ha_lbl_tdhw2, ha_name_domestic_hot_water_temperature_2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 32, 0x07, ha_lbl_tdhw2, ha_name_domestic_hot_water_temperature_2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 33 ---
    { 33, 0x00, ha_lbl_texhaust, ha_name_boiler_exhaust_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 33, 0x07, ha_lbl_texhaust, ha_name_boiler_exhaust_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 34 ---
    { 34, 0x00, ha_lbl_theatexchanger, ha_name_heat_exchanger_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 34, 0x07, ha_lbl_theatexchanger, ha_name_heat_exchanger_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 35 ---
    { 35, 0x00, ha_lbl_fanspeed_hb_u8, ha_name_boiler_fan_speed_setpoint, HaDeviceClass::none, HaUnit::Hz, HaStateClass::measurement, HaIcon::fan, HaEntityCat::none, true},
    { 35, 0x00, ha_lbl_fanspeed_lb_u8, ha_name_boiler_fan_speed_actual, HaDeviceClass::none, HaUnit::Hz, HaStateClass::measurement, HaIcon::fan, HaEntityCat::none, true},
    // --- OT ID 36 ---
    { 36, 0x00, ha_lbl_electricalcurrentburnerflame, ha_name_electricalcurrentburnerflame, HaDeviceClass::none, HaUnit::uA, HaStateClass::none, HaIcon::lightning_bolt, HaEntityCat::none, true},
    { 36, 0x07, ha_lbl_electricalcurrentburnerflame, ha_name_electricalcurrentburnerflame, HaDeviceClass::none, HaUnit::uA, HaStateClass::none, HaIcon::lightning_bolt, HaEntityCat::none, true},
    // --- OT ID 37 ---
    { 37, 0x00, ha_lbl_troomch2, ha_name_room_temperature_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 37, 0x07, ha_lbl_troomch2, ha_name_room_temperature_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 38 ---
    { 38, 0x00, ha_lbl_relativehumidity, ha_name_relative_humidity, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::percent_outline, HaEntityCat::none, true},
    { 38, 0x07, ha_lbl_relativehumidity, ha_name_relative_humidity, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::percent_outline, HaEntityCat::none, true},
    // --- OT ID 39 ---
    { 39, 0x00, ha_lbl_troverride2, ha_name_remote_override_setpoint_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 39, 0x07, ha_lbl_troverride2, ha_name_remote_override_setpoint_ch2, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 48 ---
    { 48, 0x00, ha_lbl_tdhwsetubtdhwsetlb_value_hb, ha_name_tdhwsetubtdhwsetlb_value_hb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 48, 0x00, ha_lbl_tdhwsetubtdhwsetlb_value_lb, ha_name_tdhwsetubtdhwsetlb_value_lb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 48, 0x07, ha_lbl_tdhwsetubtdhwsetlb_value_hb, ha_name_tdhwsetubtdhwsetlb_value_hb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 48, 0x07, ha_lbl_tdhwsetubtdhwsetlb_value_lb, ha_name_tdhwsetubtdhwsetlb_value_lb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 49 ---
    { 49, 0x00, ha_lbl_maxtsetubmaxtsetlb_value_hb, ha_name_maxtsetubmaxtsetlb_value_hb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 49, 0x00, ha_lbl_maxtsetubmaxtsetlb_value_lb, ha_name_maxtsetubmaxtsetlb_value_lb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 49, 0x07, ha_lbl_maxtsetubmaxtsetlb_value_hb, ha_name_maxtsetubmaxtsetlb_value_hb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 49, 0x07, ha_lbl_maxtsetubmaxtsetlb_value_lb, ha_name_maxtsetubmaxtsetlb_value_lb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 50 ---
    { 50, 0x00, ha_lbl_hcratioubhcratiolb_value_hb, ha_name_hcratioubhcratiolb_value_hb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    { 50, 0x00, ha_lbl_hcratioubhcratiolb_value_lb, ha_name_hcratioubhcratiolb_value_lb, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::none, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 51 ---
    { 51, 0x00, ha_lbl_remoteparameter4boundaries_value_hb, ha_name_remote_parameter_4_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 51, 0x00, ha_lbl_remoteparameter4boundaries_value_lb, ha_name_remote_parameter_4_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 51, 0x07, ha_lbl_remoteparameter4boundaries_value_hb, ha_name_remote_parameter_4_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 51, 0x07, ha_lbl_remoteparameter4boundaries_value_lb, ha_name_remote_parameter_4_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    // --- OT ID 52 ---
    { 52, 0x00, ha_lbl_remoteparameter5boundaries_value_hb, ha_name_remote_parameter_5_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 52, 0x00, ha_lbl_remoteparameter5boundaries_value_lb, ha_name_remote_parameter_5_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 52, 0x07, ha_lbl_remoteparameter5boundaries_value_hb, ha_name_remote_parameter_5_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 52, 0x07, ha_lbl_remoteparameter5boundaries_value_lb, ha_name_remote_parameter_5_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    // --- OT ID 53 ---
    { 53, 0x00, ha_lbl_remoteparameter6boundaries_value_hb, ha_name_remote_parameter_6_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 53, 0x00, ha_lbl_remoteparameter6boundaries_value_lb, ha_name_remote_parameter_6_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 53, 0x07, ha_lbl_remoteparameter6boundaries_value_hb, ha_name_remote_parameter_6_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 53, 0x07, ha_lbl_remoteparameter6boundaries_value_lb, ha_name_remote_parameter_6_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    // --- OT ID 54 ---
    { 54, 0x00, ha_lbl_remoteparameter7boundaries_value_hb, ha_name_remote_parameter_7_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 54, 0x00, ha_lbl_remoteparameter7boundaries_value_lb, ha_name_remote_parameter_7_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 54, 0x07, ha_lbl_remoteparameter7boundaries_value_hb, ha_name_remote_parameter_7_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 54, 0x07, ha_lbl_remoteparameter7boundaries_value_lb, ha_name_remote_parameter_7_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    // --- OT ID 55 ---
    { 55, 0x00, ha_lbl_remoteparameter8boundaries_value_hb, ha_name_remote_parameter_8_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 55, 0x00, ha_lbl_remoteparameter8boundaries_value_lb, ha_name_remote_parameter_8_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 55, 0x07, ha_lbl_remoteparameter8boundaries_value_hb, ha_name_remote_parameter_8_boundary_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    { 55, 0x07, ha_lbl_remoteparameter8boundaries_value_lb, ha_name_remote_parameter_8_boundary_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::arrow_expand_horizontal, HaEntityCat::none, true},
    // --- OT ID 56 ---
    { 56, 0x00, ha_lbl_tdhwset, ha_name_dhw_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 56, 0x07, ha_lbl_tdhwset, ha_name_dhw_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 57 ---
    { 57, 0x00, ha_lbl_maxtset, ha_name_max_ch_water_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 57, 0x07, ha_lbl_maxtset, ha_name_max_ch_water_setpoint, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 58 ---
    { 58, 0x00, ha_lbl_hcratio, ha_name_otc_heat_curve_ratio, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    { 58, 0x07, ha_lbl_hcratio, ha_name_otc_heat_curve_ratio, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- OT ID 59 ---
    { 59, 0x00, ha_lbl_remoteparameter4, ha_name_remote_parameter_4, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    { 59, 0x07, ha_lbl_remoteparameter4, ha_name_remote_parameter_4, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    // --- OT ID 60 ---
    { 60, 0x00, ha_lbl_remoteparameter5, ha_name_remote_parameter_5, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    { 60, 0x07, ha_lbl_remoteparameter5, ha_name_remote_parameter_5, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    // --- OT ID 61 ---
    { 61, 0x00, ha_lbl_remoteparameter6, ha_name_remote_parameter_6, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    { 61, 0x07, ha_lbl_remoteparameter6, ha_name_remote_parameter_6, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    // --- OT ID 62 ---
    { 62, 0x00, ha_lbl_remoteparameter7, ha_name_remote_parameter_7, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    { 62, 0x07, ha_lbl_remoteparameter7, ha_name_remote_parameter_7, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    // --- OT ID 63 ---
    { 63, 0x00, ha_lbl_remoteparameter8, ha_name_remote_parameter_8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    { 63, 0x07, ha_lbl_remoteparameter8, ha_name_remote_parameter_8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, true},
    // --- OT ID 70 ---
    { 70, 0x00, ha_lbl_status_vh_master, ha_name_status_vh_master, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::list_status, HaEntityCat::none, false},
    { 70, 0x00, ha_lbl_status_vh_slave, ha_name_status_vh_slave, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::list_status, HaEntityCat::none, false},
    // --- OT ID 71 ---
    { 71, 0x00, ha_lbl_controlsetpointvh, ha_name_vh_relative_ventilation_position, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 71, 0x00, ha_lbl_controlsetpointvh_hb_u8, ha_name_controlsetpointvh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 71, 0x00, ha_lbl_controlsetpointvh_lb_u8, ha_name_controlsetpointvh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 71, 0x07, ha_lbl_controlsetpointvh, ha_name_vh_relative_ventilation_position, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 71, 0x07, ha_lbl_controlsetpointvh_hb_u8, ha_name_controlsetpointvh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 71, 0x07, ha_lbl_controlsetpointvh_lb_u8, ha_name_controlsetpointvh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 72 ---
    { 72, 0x00, ha_lbl_asffaultcodevh_code, ha_name_asffaultcodevh_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, false},
    { 72, 0x00, ha_lbl_asffaultcodevh_flag8, ha_name_asffaultcodevh_flag8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, false},
    // --- OT ID 73 ---
    { 73, 0x00, ha_lbl_diagnosticcodevh, ha_name_diagnosticcodevh, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 73, 0x07, ha_lbl_diagnosticcodevh, ha_name_diagnosticcodevh, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 74 ---
    { 74, 0x00, ha_lbl_vh_configuration, ha_name_vh_configuration, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::cog, HaEntityCat::diagnostic, false},
    { 74, 0x00, ha_lbl_vh_memberid_code, ha_name_vh_memberid_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::card_account_details, HaEntityCat::none, false},
    // --- OT ID 75 ---
    { 75, 0x00, ha_lbl_openthermversionvh, ha_name_openthermversionvh, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, false},
    { 75, 0x07, ha_lbl_openthermversionvh, ha_name_openthermversionvh, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, false},
    // --- OT ID 76 ---
    { 76, 0x00, ha_lbl_versiontypevh_hb_u8, ha_name_versiontypevh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, false},
    { 76, 0x00, ha_lbl_versiontypevh_lb_u8, ha_name_versiontypevh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, false},
    // --- OT ID 77 ---
    { 77, 0x00, ha_lbl_relativeventilation, ha_name_relative_ventilation, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 77, 0x00, ha_lbl_relativeventilation_hb_u8, ha_name_relativeventilation_hb_u8, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 77, 0x00, ha_lbl_relativeventilation_lb_u8, ha_name_relativeventilation_lb_u8, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 77, 0x07, ha_lbl_relativeventilation, ha_name_relativeventilation, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 77, 0x07, ha_lbl_relativeventilation_hb_u8, ha_name_relativeventilation_hb_u8, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 77, 0x07, ha_lbl_relativeventilation_lb_u8, ha_name_relativeventilation_lb_u8, HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 78 ---
    { 78, 0x00, ha_lbl_relativehumidityexhaustair, ha_name_relative_humidity_exhaust_air, HaDeviceClass::humidity, HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, false},
    { 78, 0x00, ha_lbl_relativehumidityexhaustair_hb_u8, ha_name_relativehumidityexhaustair_hb_u8, HaDeviceClass::humidity, HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, false},
    { 78, 0x00, ha_lbl_relativehumidityexhaustair_lb_u8, ha_name_relativehumidityexhaustair_lb_u8, HaDeviceClass::humidity, HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, false},
    { 78, 0x07, ha_lbl_relativehumidityexhaustair, ha_name_relative_humidity_exhaust_air, HaDeviceClass::humidity, HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, false},
    { 78, 0x07, ha_lbl_relativehumidityexhaustair_hb_u8, ha_name_relativehumidityexhaustair_hb_u8, HaDeviceClass::humidity, HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, false},
    { 78, 0x07, ha_lbl_relativehumidityexhaustair_lb_u8, ha_name_relativehumidityexhaustair_lb_u8, HaDeviceClass::humidity, HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, false},
    // --- OT ID 79 ---
    { 79, 0x00, ha_lbl_co2levelexhaustair, ha_name_co2_level_exhaust_air, HaDeviceClass::carbon_dioxide, HaUnit::ppm, HaStateClass::measurement, HaIcon::molecule_co2, HaEntityCat::none, false},
    { 79, 0x07, ha_lbl_co2levelexhaustair, ha_name_co2_level_exhaust_air, HaDeviceClass::carbon_dioxide, HaUnit::ppm, HaStateClass::measurement, HaIcon::molecule_co2, HaEntityCat::none, false},
    // --- OT ID 80 ---
    { 80, 0x00, ha_lbl_supplyinlettemperature, ha_name_supply_inlet_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 80, 0x07, ha_lbl_supplyinlettemperature, ha_name_supplyinlettemperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 81 ---
    { 81, 0x00, ha_lbl_supplyoutlettemperature, ha_name_supply_outlet_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 81, 0x07, ha_lbl_supplyoutlettemperature, ha_name_supplyoutlettemperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 82 ---
    { 82, 0x00, ha_lbl_exhaustinlettemperature, ha_name_exhaust_inlet_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 82, 0x07, ha_lbl_exhaustinlettemperature, ha_name_exhaustinlettemperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 83 ---
    { 83, 0x00, ha_lbl_exhaustoutlettemperature, ha_name_exhaust_outlet_temperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    { 83, 0x07, ha_lbl_exhaustoutlettemperature, ha_name_exhaustoutlettemperature, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, false},
    // --- OT ID 84 ---
    { 84, 0x00, ha_lbl_actualexhaustfanspeed, ha_name_exhaust_fan_speed, HaDeviceClass::none, HaUnit::rpm, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 84, 0x07, ha_lbl_actualexhaustfanspeed, ha_name_exhaust_fan_speed, HaDeviceClass::none, HaUnit::rpm, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 85 ---
    { 85, 0x00, ha_lbl_actualsupplyfanspeed, ha_name_supply_fan_speed, HaDeviceClass::none, HaUnit::rpm, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    { 85, 0x07, ha_lbl_actualsupplyfanspeed, ha_name_supply_fan_speed, HaDeviceClass::none, HaUnit::rpm, HaStateClass::measurement, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 86 ---
    { 86, 0x00, ha_lbl_remoteparametersettingvh_hb_flag8, ha_name_remoteparametersettingvh_hb_flag8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, false},
    { 86, 0x00, ha_lbl_remoteparametersettingvh_lb_flag8, ha_name_remoteparametersettingvh_lb_flag8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tune_variant, HaEntityCat::none, false},
    { 86, 0x00, ha_lbl_vh_rw_nominal_ventilation_value, ha_name_vh_rw_nominal_ventilation_value, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 86, 0x00, ha_lbl_vh_transfer_enable_nominal_ventilation_value, ha_name_vh_transfer_enable_nominal_ventilation_value, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 87 ---
    { 87, 0x00, ha_lbl_nominalventilationvalue, ha_name_nominalventilationvalue, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 87, 0x00, ha_lbl_nominalventilationvalue_hb_u8, ha_name_nominalventilationvalue_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 87, 0x00, ha_lbl_nominalventilationvalue_lb_u8, ha_name_nominalventilationvalue_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 87, 0x07, ha_lbl_nominalventilationvalue, ha_name_nominalventilationvalue, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 87, 0x07, ha_lbl_nominalventilationvalue_hb_u8, ha_name_nominalventilationvalue_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    { 87, 0x07, ha_lbl_nominalventilationvalue_lb_u8, ha_name_nominalventilationvalue_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::air_filter, HaEntityCat::none, false},
    // --- OT ID 88 ---
    { 88, 0x00, ha_lbl_tspnumbervh_hb_u8, ha_name_tspnumbervh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    { 88, 0x00, ha_lbl_tspnumbervh_lb_u8, ha_name_tspnumbervh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    // --- OT ID 89 ---
    { 89, 0x00, ha_lbl_tspentryvh_hb_u8, ha_name_tspentryvh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    { 89, 0x00, ha_lbl_tspentryvh_lb_u8, ha_name_tspentryvh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    // --- OT ID 90 ---
    { 90, 0x00, ha_lbl_faultbuffersizevh_hb_u8, ha_name_faultbuffersizevh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::none, false},
    { 90, 0x00, ha_lbl_faultbuffersizevh_lb_u8, ha_name_faultbuffersizevh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::none, false},
    // --- OT ID 91 ---
    { 91, 0x00, ha_lbl_faultbufferentryvh_hb_u8, ha_name_faultbufferentryvh_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::none, false},
    { 91, 0x00, ha_lbl_faultbufferentryvh_lb_u8, ha_name_faultbufferentryvh_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::none, false},
    // --- OT ID 93 ---
    { 93, 0x00, ha_lbl_brand_hb_u8, ha_name_brand_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    { 93, 0x00, ha_lbl_brand_lb_u8, ha_name_brand_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 94 ---
    { 94, 0x00, ha_lbl_brandversion_hb_u8, ha_name_brandversion_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    { 94, 0x00, ha_lbl_brandversion_lb_u8, ha_name_brandversion_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 95 ---
    { 95, 0x00, ha_lbl_brandserialnumber_hb_u8, ha_name_brandserialnumber_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    { 95, 0x00, ha_lbl_brandserialnumber_lb_u8, ha_name_brandserialnumber_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 96 ---
    { 96, 0x00, ha_lbl_coolingoperationhours, ha_name_coolingoperationhours, HaDeviceClass::none, HaUnit::h, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    { 96, 0x07, ha_lbl_coolingoperationhours, ha_name_coolingoperationhours, HaDeviceClass::none, HaUnit::h, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    // --- OT ID 97 ---
    { 97, 0x00, ha_lbl_powercycles, ha_name_powercycles, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    { 97, 0x07, ha_lbl_powercycles, ha_name_powercycles, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 98 ---
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_battery_indication, ha_name_rf_sensor_battery, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_battery_indication_code, ha_name_rf_sensor_battery_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_sensor_index, ha_name_rf_sensor_index, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_sensor_type, ha_name_rf_sensor_type, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_sensor_type_code, ha_name_rf_sensor_type_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_signal_strength, ha_name_rf_signal_strength, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfsensorstatusinformation_signal_strength_code, ha_name_rf_signal_strength_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfstrengthbatterylevel_hb_u8, ha_name_rf_signal_battery_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x00, ha_lbl_rfstrengthbatterylevel_lb_u8, ha_name_rf_signal_battery_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x07, ha_lbl_rfstrengthbatterylevel_hb_u8, ha_name_rf_signal_battery_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    { 98, 0x07, ha_lbl_rfstrengthbatterylevel_lb_u8, ha_name_rf_signal_battery_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::antenna, HaEntityCat::none, true},
    // --- OT ID 99 ---
    { 99, 0x00, ha_lbl_operatingmode_hc1_hc2_dhw_hb_u8, ha_name_operatingmode_hc1_hc2_dhw_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::thermostat_icon, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_operatingmode_hc1_hc2_dhw_lb_u8, ha_name_operatingmode_hc1_hc2_dhw_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::thermostat_icon, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_dhw_mode, ha_name_remoteoverrideoperatingmode_dhw_mode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_dhw_mode_code, ha_name_remoteoverrideoperatingmode_dhw_mode_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_hc1_mode, ha_name_remoteoverrideoperatingmode_hc1_mode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_hc1_mode_code, ha_name_remoteoverrideoperatingmode_hc1_mode_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_hc2_mode, ha_name_remoteoverrideoperatingmode_hc2_mode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_hc2_mode_code, ha_name_remoteoverrideoperatingmode_hc2_mode_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x00, ha_lbl_remoteoverrideoperatingmode_manual_dhw_push, ha_name_remoteoverrideoperatingmode_manual_dhw_push, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    { 99, 0x07, ha_lbl_operatingmode_hc1_hc2_dhw_hb_u8, ha_name_operatingmode_hc1_hc2_dhw_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::thermostat_icon, HaEntityCat::none, true},
    { 99, 0x07, ha_lbl_operatingmode_hc1_hc2_dhw_lb_u8, ha_name_operatingmode_hc1_hc2_dhw_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::thermostat_icon, HaEntityCat::none, true},
    // --- OT ID 100 ---
    {100, 0x00, ha_lbl_roomremoteoverridefunction_flag8, ha_name_roomremoteoverridefunction_flag8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::remote, HaEntityCat::none, true},
    // --- OT ID 101 ---
    {101, 0x00, ha_lbl_solar_storage_master_mode, ha_name_solar_storage_master_mode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::solar_panel, HaEntityCat::none, false},
    {101, 0x00, ha_lbl_solar_storage_mode_status, ha_name_solar_storage_mode_status, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::solar_panel, HaEntityCat::none, false},
    {101, 0x00, ha_lbl_solar_storage_slave_status, ha_name_solar_storage_slave_status, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::list_status, HaEntityCat::none, false},
    // --- OT ID 102 ---
    {102, 0x00, ha_lbl_solarstorageasfflags_code, ha_name_solarstorageasfflags_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, false},
    {102, 0x00, ha_lbl_solarstorageasfflags_flag8, ha_name_solarstorageasfflags_flag8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, false},
    // --- OT ID 103 ---
    {103, 0x00, ha_lbl_solar_storage_slave_configuration, ha_name_solar_storage_slave_configuration, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::cog, HaEntityCat::diagnostic, false},
    {103, 0x00, ha_lbl_solar_storage_slave_memberid_code, ha_name_solar_storage_slave_memberid_code, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::card_account_details, HaEntityCat::none, false},
    // --- OT ID 104 ---
    {104, 0x00, ha_lbl_solarstorageversiontype_hb_u8, ha_name_solarstorageversiontype_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, false},
    {104, 0x00, ha_lbl_solarstorageversiontype_lb_u8, ha_name_solarstorageversiontype_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, false},
    // --- OT ID 105 ---
    {105, 0x00, ha_lbl_solarstoragetsp_hb_u8, ha_name_solarstoragetsp_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    {105, 0x00, ha_lbl_solarstoragetsp_lb_u8, ha_name_solarstoragetsp_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    // --- OT ID 106 ---
    {106, 0x00, ha_lbl_solarstoragetspindextspvalue_hb_u8, ha_name_solarstoragetspindextspvalue_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    {106, 0x00, ha_lbl_solarstoragetspindextspvalue_lb_u8, ha_name_solarstoragetspindextspvalue_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::format_list_numbered, HaEntityCat::diagnostic, false},
    // --- OT ID 107 ---
    {107, 0x00, ha_lbl_solarstoragefhbsize_hb_u8, ha_name_solarstoragefhbsize_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, false},
    {107, 0x00, ha_lbl_solarstoragefhbsize_lb_u8, ha_name_solarstoragefhbsize_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, false},
    // --- OT ID 108 ---
    {108, 0x00, ha_lbl_solarstoragefhbindexfhbvalue_hb_u8, ha_name_solarstoragefhbindexfhbvalue_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, false},
    {108, 0x00, ha_lbl_solarstoragefhbindexfhbvalue_lb_u8, ha_name_solarstoragefhbindexfhbvalue_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::history, HaEntityCat::diagnostic, false},
    // --- OT ID 109 ---
    {109, 0x00, ha_lbl_electricityproducerstarts, ha_name_electricityproducerstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    {109, 0x07, ha_lbl_electricityproducerstarts, ha_name_electricityproducerstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 110 ---
    {110, 0x00, ha_lbl_electricityproducerhours, ha_name_electricityproducerhours, HaDeviceClass::none, HaUnit::h, HaStateClass::total_increasing, HaIcon::lightning_bolt, HaEntityCat::none, true},
    {110, 0x07, ha_lbl_electricityproducerhours, ha_name_electricityproducerhours, HaDeviceClass::none, HaUnit::h, HaStateClass::total_increasing, HaIcon::lightning_bolt, HaEntityCat::none, true},
    // --- OT ID 111 ---
    {111, 0x00, ha_lbl_electricityproduction, ha_name_electricityproduction, HaDeviceClass::power, HaUnit::W, HaStateClass::measurement, HaIcon::flash, HaEntityCat::none, true},
    {111, 0x07, ha_lbl_electricityproduction, ha_name_electricityproduction, HaDeviceClass::power, HaUnit::W, HaStateClass::measurement, HaIcon::flash, HaEntityCat::none, true},
    // --- OT ID 112 ---
    {112, 0x00, ha_lbl_cumulativeelectricityproduction, ha_name_cumulativeelectricityproduction, HaDeviceClass::energy, HaUnit::kWh, HaStateClass::total_increasing, HaIcon::lightning_bolt, HaEntityCat::none, true},
    {112, 0x07, ha_lbl_cumulativeelectricityproduction, ha_name_cumulativeelectricityproduction, HaDeviceClass::energy, HaUnit::kWh, HaStateClass::total_increasing, HaIcon::lightning_bolt, HaEntityCat::none, true},
    // --- OT ID 113 ---
    {113, 0x00, ha_lbl_burnerunsuccessfulstarts, ha_name_burnerunsuccessfulstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    {113, 0x07, ha_lbl_burnerunsuccessfulstarts, ha_name_burnerunsuccessfulstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 114 ---
    {114, 0x00, ha_lbl_flamesignaltoolow, ha_name_flamesignaltoolow, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::alert_outline, HaEntityCat::none, true},
    {114, 0x07, ha_lbl_flamesignaltoolow, ha_name_flamesignaltoolow, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::alert_outline, HaEntityCat::none, true},
    // --- OT ID 115 ---
    {115, 0x00, ha_lbl_oemdiagnosticcode, ha_name_oemdiagnosticcode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, true},
    {115, 0x07, ha_lbl_oemdiagnosticcode, ha_name_oemdiagnosticcode, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::alert_outline, HaEntityCat::diagnostic, true},
    // --- OT ID 116 ---
    {116, 0x00, ha_lbl_burnerstarts, ha_name_burnerstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    {116, 0x07, ha_lbl_burnerstarts, ha_name_burnerstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 117 ---
    {117, 0x00, ha_lbl_chpumpstarts, ha_name_chpumpstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    {117, 0x07, ha_lbl_chpumpstarts, ha_name_chpumpstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 118 ---
    {118, 0x00, ha_lbl_dhwpumpvalvestarts, ha_name_dhwpumpvalvestarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    {118, 0x07, ha_lbl_dhwpumpvalvestarts, ha_name_dhwpumpvalvestarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 119 ---
    {119, 0x00, ha_lbl_dhwburnerstarts, ha_name_dhwburnerstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    {119, 0x07, ha_lbl_dhwburnerstarts, ha_name_dhwburnerstarts, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::none, true},
    // --- OT ID 120 ---
    {120, 0x00, ha_lbl_burneroperationhours, ha_name_burneroperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    {120, 0x07, ha_lbl_burneroperationhours, ha_name_burneroperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    // --- OT ID 121 ---
    {121, 0x00, ha_lbl_chpumpoperationhours, ha_name_chpumpoperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    {121, 0x07, ha_lbl_chpumpoperationhours, ha_name_chpumpoperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    // --- OT ID 122 ---
    {122, 0x00, ha_lbl_dhwpumpvalveoperationhours, ha_name_dhwpumpvalveoperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    {122, 0x07, ha_lbl_dhwpumpvalveoperationhours, ha_name_dhwpumpvalveoperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    // --- OT ID 123 ---
    {123, 0x00, ha_lbl_dhwburneroperationhours, ha_name_dhwburneroperationhours_dhw, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    {123, 0x07, ha_lbl_dhwburneroperationhours, ha_name_dhwburneroperationhours, HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::timer_outline, HaEntityCat::none, true},
    // --- OT ID 124 ---
    {124, 0x00, ha_lbl_openthermversionmaster, ha_name_master_ot_protocol_version, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    {124, 0x07, ha_lbl_openthermversionmaster, ha_name_master_ot_protocol_version, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 125 ---
    {125, 0x00, ha_lbl_openthermversionslave, ha_name_slave_ot_protocol_version, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    {125, 0x07, ha_lbl_openthermversionslave, ha_name_slave_ot_protocol_version, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 126 ---
    {126, 0x00, ha_lbl_masterversion_hb_u8, ha_name_masterversion_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    {126, 0x00, ha_lbl_masterversion_lb_u8, ha_name_masterversion_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 127 ---
    {127, 0x00, ha_lbl_slaveversion_hb_u8, ha_name_slaveversion_hb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    {127, 0x00, ha_lbl_slaveversion_lb_u8, ha_name_slaveversion_lb_u8, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::tag, HaEntityCat::diagnostic, true},
    // --- OT ID 131 ---
    {131, 0x00, ha_lbl_remehadfducodes_hb_u8, ha_name_remeha_fd_u_codes_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::none, false},
    {131, 0x00, ha_lbl_remehadfducodes_lb_u8, ha_name_remeha_fd_u_codes_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::none, false},
    // --- OT ID 132 ---
    {132, 0x00, ha_lbl_remehaservicemessage_hb_u8, ha_name_remeha_service_message_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::none, false},
    {132, 0x00, ha_lbl_remehaservicemessage_lb_u8, ha_name_remeha_service_message_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::none, false},
    // --- OT ID 133 ---
    {133, 0x00, ha_lbl_remehadetectionconnectedscu_hb_u8, ha_name_remeha_detection_connected_scu_hb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::none, false},
    {133, 0x00, ha_lbl_remehadetectionconnectedscu_lb_u8, ha_name_remeha_detection_connected_scu_lb, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::none, false},
    // --- OT ID 245 ---
    {245, 0x00, ha_lbl_s0powerkw, ha_name_s0_power_kw, HaDeviceClass::power, HaUnit::kW, HaStateClass::measurement, HaIcon::flash, HaEntityCat::none, true},
    {245, 0x00, ha_lbl_s0pulsecount, ha_name_s0_pulse_count, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::pulse, HaEntityCat::none, true},
    {245, 0x00, ha_lbl_s0pulsecounttot, ha_name_s0_pulse_count_total, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::pulse, HaEntityCat::none, true},
    {245, 0x00, ha_lbl_s0pulsetime, ha_name_s0_pulse_time, HaDeviceClass::none, HaUnit::mS, HaStateClass::none, HaIcon::clock_outline, HaEntityCat::none, true},
    // --- OT ID 246 ---
    {246, 0x00, ha_lbl_sensor_id, ha_name_sensor_id, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    // --- Pseudo-ID 247: heap & discovery statistics (TASK-346) ---
    // 17 retained otgw-firmware/stats/* topics published hourly by sendMQTTheapdiag().
    // entity_category=diagnostic keeps them out of the HA primary sensor view.
    // Counters use total_increasing; live samples and last-known values use measurement.
    {247, 0x00, ha_lbl_stats_ws_drops,                ha_name_stats_ws_drops,                HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_mqtt_drops,              ha_name_stats_mqtt_drops,              HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_enter_low,               ha_name_stats_enter_low,               HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_enter_warning,           ha_name_stats_enter_warning,           HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_enter_critical,          ha_name_stats_enter_critical,          HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_drip_burst_skip,         ha_name_stats_drip_burst_skip,         HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_drip_cooldown_skip,      ha_name_stats_drip_cooldown_skip,      HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_drip_slowmode,           ha_name_stats_drip_slowmode,           HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_free_heap,               ha_name_stats_free_heap,               HaDeviceClass::none, HaUnit::bytes,   HaStateClass::measurement,      HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_max_block,               ha_name_stats_max_block,               HaDeviceClass::none, HaUnit::bytes,   HaStateClass::measurement,      HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_frag_pct,                ha_name_stats_frag_pct,                HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement,      HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_disc_verify_runs,        ha_name_stats_disc_verify_runs,        HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_disc_republish_triggered, ha_name_stats_disc_republish_triggered, HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_disc_last_missing,       ha_name_stats_disc_last_missing,       HaDeviceClass::none, HaUnit::none,    HaStateClass::measurement,      HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_disc_last_orphan,        ha_name_stats_disc_last_orphan,        HaDeviceClass::none, HaUnit::none,    HaStateClass::measurement,      HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_disc_published_topics,   ha_name_stats_disc_published_topics,   HaDeviceClass::none, HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter, HaEntityCat::diagnostic, true},
    {247, 0x00, ha_lbl_stats_disc_last_verify_epoch,  ha_name_stats_disc_last_verify_epoch,  HaDeviceClass::none, HaUnit::none,    HaStateClass::measurement,      HaIcon::information_outline, HaEntityCat::diagnostic, true},
    // --- Pseudo-ID 248: firmware diagnostics (TASK-540 / TASK-541) ---
    // Plain otgw-firmware/* topics; entity_category=diagnostic.
    {248, 0x00, ha_lbl_fw_reboot_count,  ha_name_fw_reboot_count,  HaDeviceClass::none, HaUnit::none, HaStateClass::total_increasing, HaIcon::counter,             HaEntityCat::diagnostic, true},
    {248, 0x00, ha_lbl_fw_reboot_reason, ha_name_fw_reboot_reason, HaDeviceClass::none, HaUnit::none, HaStateClass::none,             HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {248, 0x00, ha_lbl_fw_version,       ha_name_fw_version,       HaDeviceClass::none, HaUnit::none, HaStateClass::none,             HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {248, 0x00, ha_lbl_fw_hostname,      ha_name_fw_hostname,      HaDeviceClass::none, HaUnit::none, HaStateClass::none,             HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {248, 0x00, ha_lbl_fw_hardware_type, ha_name_fw_hardware_type, HaDeviceClass::none, HaUnit::none, HaStateClass::none,             HaIcon::information_outline, HaEntityCat::diagnostic, true},  // ADR-113
    // --- Pseudo-ID 249: PIC info (TASK-540 / TASK-541) ---
    // 0x08 = MQTT_HA_FLAG_IS_PIC_ENTRY → "otgw-pic/" prefix added by streamSensorDiscovery
    // and entries are skipped at publish time when isPICEnabled() is false.
    {249, 0x08, ha_lbl_pic_version,      ha_name_pic_version,      HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {249, 0x08, ha_lbl_pic_deviceid,     ha_name_pic_deviceid,     HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {249, 0x08, ha_lbl_pic_firmwaretype, ha_name_pic_firmwaretype, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {249, 0x08, ha_lbl_pic_designer,     ha_name_pic_designer,     HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    // ADR-113 stage 2 (TASK-754): ha_lbl_pic_available row removed.
    // --- Pseudo-ID 250: PIC settings (TASK-540 / TASK-541) ---
    // 0x08 → "otgw-pic/" prefix; labels start with "settings/" so the final topic is
    // <pubNs>/otgw-pic/settings/<x>. Mirrors publishAllPICsettings() in OTGW-Core.ino.
    {250, 0x08, ha_lbl_pic_set_setpoint_override,   ha_name_pic_set_setpoint_override,   HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_setback,             ha_name_pic_set_setback,             HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_dhw_override,        ha_name_pic_set_dhw_override,        HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_gpio,                ha_name_pic_set_gpio,                HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_gpio_states,         ha_name_pic_set_gpio_states,         HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_led,                 ha_name_pic_set_led,                 HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_tweaks,              ha_name_pic_set_tweaks,              HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_temp_sensor,         ha_name_pic_set_temp_sensor,         HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_smart_power,         ha_name_pic_set_smart_power,         HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_thermostat_detect,   ha_name_pic_set_thermostat_detect,   HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_builddate,           ha_name_pic_set_builddate,           HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_clock_mhz,           ha_name_pic_set_clock_mhz,           HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_reset_cause,         ha_name_pic_set_reset_cause,         HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_standalone_interval, ha_name_pic_set_standalone_interval, HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {250, 0x08, ha_lbl_pic_set_voltage_ref,         ha_name_pic_set_voltage_ref,         HaDeviceClass::none, HaUnit::none, HaStateClass::none, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    // --- Pseudo-ID 251: 2.0.0-specific diagnostics (TASK-541) ---
    // OTDirect flame metrics + SAT BLE health/availability + SAT pressure status string.
    // SAT measurement primaries (ble_temp/humidity, ch_pressure) and the wider SAT
    // control/PID/cycle/weather/zone surface live in TASK-543 — a dedicated discovery
    // task because those are user-facing primaries, not diagnostics.
    {251, 0x00, ha_lbl_otdirect_flame_duty_pct,       ha_name_otdirect_flame_duty_pct,       HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::gauge,               HaEntityCat::diagnostic, true},
    {251, 0x00, ha_lbl_otdirect_flame_cycles_per_hr,  ha_name_otdirect_flame_cycles_per_hr,  HaDeviceClass::none, HaUnit::none,    HaStateClass::measurement, HaIcon::counter,             HaEntityCat::diagnostic, true},
    {251, 0x00, ha_lbl_sat_ble_sensor_rssi,           ha_name_sat_ble_sensor_rssi,           HaDeviceClass::none, HaUnit::none,    HaStateClass::measurement, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {251, 0x00, ha_lbl_sat_ble_battery,               ha_name_sat_ble_battery,               HaDeviceClass::none, HaUnit::percent, HaStateClass::measurement, HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {251, 0x00, ha_lbl_sat_ble_sensor_count,          ha_name_sat_ble_sensor_count,          HaDeviceClass::none, HaUnit::none,    HaStateClass::measurement, HaIcon::counter,             HaEntityCat::diagnostic, true},
    {251, 0x00, ha_lbl_sat_ble_temp_valid,            ha_name_sat_ble_temp_valid,            HaDeviceClass::none, HaUnit::none,    HaStateClass::none,        HaIcon::information_outline, HaEntityCat::diagnostic, true},
    {251, 0x00, ha_lbl_sat_ch_pressure_status,        ha_name_sat_ch_pressure_status,        HaDeviceClass::none, HaUnit::none,    HaStateClass::none,        HaIcon::information_outline, HaEntityCat::diagnostic, true},
    // --- Pseudo-ID 252: TASK-543 SAT control + PID + cycle + rolling statistics ---
    {252, 0x00, ha_lbl_sat_target,                 ha_name_sat_target,                 HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_mode,                   ha_name_sat_mode,                   HaDeviceClass::none,        HaUnit::none,    HaStateClass::none,             HaIcon::thermostat_icon, HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_setpoint,               ha_name_sat_setpoint,               HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_heating_curve,          ha_name_sat_heating_curve,          HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_pid_output,             ha_name_sat_pid_output,             HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 ha_json_sat_pid_attributes},
    {252, 0x00, ha_lbl_sat_error,                  ha_name_sat_error,                  HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_room_temp,              ha_name_sat_room_temp,              HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_outside_temp,           ha_name_sat_outside_temp,           HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_pid_p,                  ha_name_sat_pid_p,                  HaDeviceClass::none,        HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_pid_i,                  ha_name_sat_pid_i,                  HaDeviceClass::none,        HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_pid_d,                  ha_name_sat_pid_d,                  HaDeviceClass::none,        HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_raw_derivative,         ha_name_sat_raw_derivative,         HaDeviceClass::none,        HaUnit::none,    HaStateClass::measurement,      HaIcon::speedometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_kp,                     ha_name_sat_kp,                     HaDeviceClass::none,        HaUnit::none,    HaStateClass::measurement,      HaIcon::tune_variant,    HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_ki,                     ha_name_sat_ki,                     HaDeviceClass::none,        HaUnit::none,    HaStateClass::measurement,      HaIcon::tune_variant,    HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_kd,                     ha_name_sat_kd,                     HaDeviceClass::none,        HaUnit::none,    HaStateClass::measurement,      HaIcon::tune_variant,    HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_boiler_status,          ha_name_sat_boiler_status,          HaDeviceClass::none,        HaUnit::none,    HaStateClass::none,             HaIcon::water_boiler,    HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_cycle_class,            ha_name_sat_cycle_class,            HaDeviceClass::none,        HaUnit::none,    HaStateClass::none,             HaIcon::history,         HaEntityCat::none, true, nullptr,                 ha_json_sat_cycle_attributes},
    {252, 0x00, ha_lbl_sat_pwm_duty,               ha_name_sat_pwm_duty,               HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement,      HaIcon::pulse,           HaEntityCat::none, true, ha_tpl_sat_pwm_percent,  nullptr},
    {252, 0x00, ha_lbl_sat_duty_ratio,             ha_name_sat_duty_ratio,             HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement,      HaIcon::percent_outline, HaEntityCat::none, true, ha_tpl_sat_ratio_percent, nullptr},
    {252, 0x00, ha_lbl_sat_overshoot_fraction,     ha_name_sat_overshoot_fraction,     HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement,      HaIcon::percent_outline, HaEntityCat::none, true, ha_tpl_sat_ratio_percent, nullptr},
    {252, 0x00, ha_lbl_sat_cycle_phase,            ha_name_sat_cycle_phase,            HaDeviceClass::none,        HaUnit::none,    HaStateClass::none,             HaIcon::history,         HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_overshoot_margin,       ha_name_sat_overshoot_margin,       HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_cycles_this_hour,       ha_name_sat_cycles_this_hour,       HaDeviceClass::none,        HaUnit::none,    HaStateClass::total_increasing, HaIcon::counter,         HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_4h_cycles,              ha_name_sat_4h_cycles,              HaDeviceClass::none,        HaUnit::none,    HaStateClass::measurement,      HaIcon::counter,         HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_4h_avg_on_sec,          ha_name_sat_4h_avg_on_sec,          HaDeviceClass::none,        HaUnit::s,       HaStateClass::measurement,      HaIcon::timer_outline,   HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_4h_avg_off_sec,         ha_name_sat_4h_avg_off_sec,         HaDeviceClass::none,        HaUnit::s,       HaStateClass::measurement,      HaIcon::timer_outline,   HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_4h_avg_flow_temp,       ha_name_sat_4h_avg_flow_temp,       HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_4h_duty_ratio,          ha_name_sat_4h_duty_ratio,          HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement,      HaIcon::percent_outline, HaEntityCat::none, true, ha_tpl_sat_ratio_percent, nullptr},
    {252, 0x00, ha_lbl_sat_4h_overshoot_fraction,  ha_name_sat_4h_overshoot_fraction,  HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement,      HaIcon::percent_outline, HaEntityCat::none, true, ha_tpl_sat_ratio_percent, nullptr},
    {252, 0x00, ha_lbl_sat_4h_underheat_fraction,  ha_name_sat_4h_underheat_fraction,  HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement,      HaIcon::percent_outline, HaEntityCat::none, true, ha_tpl_sat_ratio_percent, nullptr},
    {252, 0x00, ha_lbl_sat_4h_flow_ret_delta_p50,  ha_name_sat_4h_flow_ret_delta_p50,  HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    {252, 0x00, ha_lbl_sat_4h_flow_ret_delta_p90,  ha_name_sat_4h_flow_ret_delta_p90,  HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement,      HaIcon::thermometer,     HaEntityCat::none, true, nullptr,                 nullptr},
    // --- Pseudo-ID 253: TASK-543 SAT BLE + pressure + weather primaries ---
    {253, 0x00, ha_lbl_sat_ble_temp,               ha_name_sat_ble_temp,               HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement, HaIcon::thermometer,   HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_ble_humidity,           ha_name_sat_ble_humidity,           HaDeviceClass::humidity,    HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_ch_pressure,            ha_name_sat_ch_pressure,            HaDeviceClass::pressure,    HaUnit::bar,     HaStateClass::measurement, HaIcon::gauge,         HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_temperature,    ha_name_sat_weather_temperature,    HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement, HaIcon::thermometer,   HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_apparent_temp,  ha_name_sat_weather_apparent_temp,  HaDeviceClass::temperature, HaUnit::degC,    HaStateClass::measurement, HaIcon::thermometer,   HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_humidity,       ha_name_sat_weather_humidity,       HaDeviceClass::humidity,    HaUnit::percent, HaStateClass::measurement, HaIcon::water_percent, HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_wind_speed,     ha_name_sat_weather_wind_speed,     HaDeviceClass::none,        HaUnit::m_s,     HaStateClass::measurement, HaIcon::fan,           HaEntityCat::none, true, ha_tpl_sat_kmh_to_ms, nullptr},
    {253, 0x00, ha_lbl_sat_weather_wind_direction, ha_name_sat_weather_wind_direction, HaDeviceClass::none,        HaUnit::deg,     HaStateClass::measurement, HaIcon::angle_acute,   HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_wind_gusts,     ha_name_sat_weather_wind_gusts,     HaDeviceClass::none,        HaUnit::m_s,     HaStateClass::measurement, HaIcon::fan,           HaEntityCat::none, true, ha_tpl_sat_kmh_to_ms, nullptr},
    {253, 0x00, ha_lbl_sat_weather_cloud_cover,    ha_name_sat_weather_cloud_cover,    HaDeviceClass::none,        HaUnit::percent, HaStateClass::measurement, HaIcon::solar_panel,   HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_pressure_msl,   ha_name_sat_weather_pressure_msl,   HaDeviceClass::pressure,    HaUnit::hPa,     HaStateClass::measurement, HaIcon::gauge,         HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_precipitation,  ha_name_sat_weather_precipitation,  HaDeviceClass::none,        HaUnit::mm,      HaStateClass::measurement, HaIcon::water,         HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_rain,           ha_name_sat_weather_rain,           HaDeviceClass::none,        HaUnit::mm,      HaStateClass::measurement, HaIcon::water,         HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_snowfall,       ha_name_sat_weather_snowfall,       HaDeviceClass::none,        HaUnit::mm,      HaStateClass::measurement, HaIcon::snowflake,     HaEntityCat::none, true, nullptr,              nullptr},
    {253, 0x00, ha_lbl_sat_weather_weather_code,   ha_name_sat_weather_weather_code,   HaDeviceClass::none,        HaUnit::none,    HaStateClass::measurement, HaIcon::information,   HaEntityCat::none, true, nullptr,              nullptr},
    // --- Pseudo-ID 254: TASK-543 SAT flame status string ---
    {254, 0x00, ha_lbl_sat_flame_status,           ha_name_sat_flame_status,           HaDeviceClass::none,        HaUnit::none,    HaStateClass::none,        HaIcon::fire,          HaEntityCat::none, true, nullptr,              nullptr},
};

// ========== Binary sensor array (58 entries, sorted by id) ==========
// ADR-105: first 58 rows are sorted by OT id and indexed via mqttHaBinSensorIndex[].
// Rows 58..94 are HA-core aliases, NOT contiguous by OT id and NOT covered by
// the index. The discovery dispatcher walks the alias tail separately when
// settings.mqtt.bPublishHaCoreAliases is on (see MQTTstuff.ino).
const uint16_t MQTT_HA_BINSENSOR_COUNT         = 95;
const uint16_t MQTT_HA_BINSENSOR_INDEXED_COUNT = 58;

const MqttHaBinSensorCfg PROGMEM mqttHaBinSensors[] = {
//  {id, flags, label, friendlyName, icon, entityCat, enabledByDefault}
#define SAT_BIN(id, key, icon, cat, enabled, devCls, payloadKind) \
    {id, 0x00, ha_lbl_##key, ha_name_##key, icon, cat, enabled, devCls, payloadKind}
    // --- OT ID 0 ---
    {  0, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_centralheating, ha_name_central_heating, HaIcon::radiator, HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_centralheating2, ha_name_central_heating_2, HaIcon::radiator, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_ch2_enable, ha_name_central_heating_2_enable, HaIcon::toggle_switch, HaEntityCat::none, false},
    {  0, 0x00, ha_lbl_ch_enable, ha_name_central_heating_enable, HaIcon::radiator, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_cooling, ha_name_cooling, HaIcon::snowflake, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_cooling_enable, ha_name_cooling_enable, HaIcon::snowflake, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_dhw_blocking, ha_name_dhw_blocking, HaIcon::water_boiler, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_dhw_enable, ha_name_domestic_hot_water_enable, HaIcon::water_boiler, HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_diagnostic_indicator, ha_name_diagnostic_indicator, HaIcon::information, HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_domestichotwater, ha_name_domestic_hot_water, HaIcon::water_boiler, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_electric_production, ha_name_electric_production, HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_fault, ha_name_fault, HaIcon::alert_circle, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_flame, ha_name_flame, HaIcon::fire, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_otc_active, ha_name_otc_enable, HaIcon::information, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_summerwintertime, ha_name_summerwintertime, HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    // ADR-084: OT-bus presence entries are generic (no otgw-pic/ prefix) since
    // 2.0.0 -- stat_t now points at OTGW/value/<uid>/{boiler,thermostat}_connected
    // and HA-discovery no longer requires isPICEnabled() to pass. Do not restore
    // MQTT_HA_FLAG_IS_PIC_ENTRY (0x08) on these rows.
    {  0, 0x00, ha_lbl_boiler_connected, ha_name_boiler_connected, HaIcon::lan_connect, HaEntityCat::none, true},
    {  0, 0x00, ha_lbl_thermostat_connected, ha_name_thermostat_connected, HaIcon::lan_connect, HaEntityCat::none, true},
    // --- OT ID 2 ---
    {  2, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_master_configuration_smart_power, ha_name_master_configuration_smart_power, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    // --- OT ID 3 ---
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_ch2_present, ha_name_ch2_present, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_control_type_modulation, ha_name_control_type_modulation, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_cooling_config, ha_name_cooling_configs, HaIcon::snowflake, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_dhw_config, ha_name_dhw_config, HaIcon::water_boiler, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_dhw_present, ha_name_dhw_present, HaIcon::water_boiler, HaEntityCat::diagnostic, true},
    {  3, 0x00, ha_lbl_heat_cool_mode_control, ha_name_heat_cool_mode_control, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_master_low_off_pump_control_function, ha_name_master_low_off_pump_control_function, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_remote_water_filling_function, ha_name_remote_water_filling_function, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    // --- OT ID 5 ---
    {  5, 0x00, ha_lbl_air_pressure_fault, ha_name_air_press_fault, HaIcon::alert_circle, HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_gas_flame_fault, ha_name_gas_flame_fault, HaIcon::alert_circle, HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_lockout_reset, ha_name_lockout_reset, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  5, 0x00, ha_lbl_low_water_pressure, ha_name_low_water_press, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_service_request, ha_name_service_request, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_water_over_temperature, ha_name_water_over_temp, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    // --- OT ID 6 ---
    {  6, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_rbp_dhw_setpoint, ha_name_rbp_dhw_setpoint, HaIcon::water_boiler, HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_rbp_max_ch_setpoint, ha_name_rbp_max_ch_setpoint, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_rbp_rw_dhw_setpoint, ha_name_rbp_rw_dhw_setpoint, HaIcon::water_boiler, HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_rbp_rw_max_ch_setpoint, ha_name_rbp_rw_max_ch_setpoint, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    // --- OT ID 70 ---
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_bypass_automatic_status, ha_name_vh_bypass_automatic_status, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_bypass_mode, ha_name_vh_bypass_mode, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_bypass_position, ha_name_vh_bypass_position, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_bypass_status, ha_name_vh_bypass_status, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_diagnostic_indicator, ha_name_vh_diagnostic_indicator, HaIcon::information, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_fault, ha_name_vh_fault, HaIcon::alert_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_free_ventilation_mode, ha_name_vh_free_ventilation_mode, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_free_ventliation_status, ha_name_vh_free_ventliation_status, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_ventilation_enabled, ha_name_vh_ventilation_enabled, HaIcon::toggle_switch, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_ventilation_mode, ha_name_vh_ventilation_mode, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    // --- OT ID 74 ---
    { 74, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_configuration_bypass, ha_name_vh_configuration_bypass, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    { 74, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_configuration_speed_control, ha_name_vh_configuration_speed_control, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    { 74, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_vh_configuration_system_type, ha_name_vh_configuration_system_type, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    // --- OT ID 100 ---
    {100, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_remote_override_manual_change_priority, ha_name_remote_override_manual_change_priority, HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    {100, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_remote_override_program_change_priority, ha_name_remote_override_program_change_priority, HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    // --- OT ID 101 ---
    {101, MQTT_HA_FLAG_LEGACY_REPLACED_BY_ALIAS, ha_lbl_solar_storage_slave_fault_indicator, ha_name_solar_storage_slave_fault_indicator, HaIcon::alert_circle, HaEntityCat::none, false},
    // --- OT ID 113 ---
    {113, 0x00, ha_lbl_solar_storage_system_type, ha_name_solar_storage_system_type, HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    // --- Pseudo-ID 253: TASK-543 weather binary ---
    SAT_BIN(253, sat_weather_is_day, HaIcon::solar_panel, HaEntityCat::none, true, nullptr, HaBinaryPayload::one_zero),
    // --- Pseudo-ID 254: TASK-543 SAT binary primaries ---
    SAT_BIN(254, sat_active,         HaIcon::thermostat_icon, HaEntityCat::none, true, nullptr,           HaBinaryPayload::true_false),
    SAT_BIN(254, sat_safety_tripped, HaIcon::alert_circle,    HaEntityCat::none, true, ha_bincls_problem, HaBinaryPayload::true_false),
    SAT_BIN(254, sat_flame_health,   HaIcon::fire,            HaEntityCat::none, true, ha_bincls_problem, HaBinaryPayload::on_off),
    SAT_BIN(254, sat_valves_open,    HaIcon::radiator,        HaEntityCat::none, true, nullptr,           HaBinaryPayload::true_false),

    // ---------------------------------------------------------------------------
    // ADR-105: HA-core aliases (37 rows) — gated by settings.mqtt.bPublishHaCoreAliases
    // via MQTT_HA_FLAG_IS_HA_CORE_ALIAS (0x10) in streamHaDiscoveryEntries().
    // Each alias mirrors its current-name sibling's icon/category/enabledByDefault.
    // Positional init stops at the 7th field; deviceClass + payload default to
    // nullptr + HaBinaryPayload::on_off (matches existing capability-flag rows).
    // --- ADR-105 Category A: supports_* capability aliases (12) ---
    {  2, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_master_smart_power,                ha_name_alias_supports_master_smart_power,                HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_hot_water,                         ha_name_alias_supports_hot_water,                         HaIcon::water_boiler,           HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_cooling,                           ha_name_alias_supports_cooling,                           HaIcon::snowflake,              HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_pump_control,                      ha_name_alias_supports_pump_control,                      HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_ch_2,                              ha_name_alias_supports_ch_2,                              HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_remote_reset,                      ha_name_alias_supports_remote_reset,                      HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_lockout_reset,                     ha_name_alias_supports_lockout_reset,                     HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_hot_water_setpoint_transfer,       ha_name_alias_supports_hot_water_setpoint_transfer,       HaIcon::water_boiler,           HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_central_heating_setpoint_transfer, ha_name_alias_supports_central_heating_setpoint_transfer, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_hot_water_setpoint_writing,        ha_name_alias_supports_hot_water_setpoint_writing,        HaIcon::water_boiler,           HaEntityCat::diagnostic, true},
    {  6, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_central_heating_setpoint_writing,  ha_name_alias_supports_central_heating_setpoint_writing,  HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    { 74, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_supports_ventilation_bypass,                ha_name_alias_supports_ventilation_bypass,                HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    // --- ADR-105 Category B: plain-name HA-core verbatim (12) ---
    {  0, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_fault_indication,             ha_name_alias_fault_indication,             HaIcon::alert_circle,           HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_central_heating,              ha_name_alias_central_heating,              HaIcon::radiator,               HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_hot_water,                    ha_name_alias_hot_water,                    HaIcon::water_boiler,           HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_central_heating_2,            ha_name_alias_central_heating_2,            HaIcon::radiator,               HaEntityCat::none, true},
    {  0, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_diagnostic_indication,        ha_name_alias_diagnostic_indication,        HaIcon::information,            HaEntityCat::none, true},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_control_type,                 ha_name_alias_control_type,                 HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  3, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_hot_water_config,             ha_name_alias_hot_water_config,             HaIcon::water_boiler,           HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_service_required,             ha_name_alias_service_required,             HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_gas_fault,                    ha_name_alias_gas_fault,                    HaIcon::alert_circle,           HaEntityCat::diagnostic, true},
    {  5, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_water_overtemperature,        ha_name_alias_water_overtemperature,        HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, true},
    {100, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_override_manual_change_prio,  ha_name_alias_override_manual_change_prio,  HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    {100, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_override_program_change_prio, ha_name_alias_override_program_change_prio, HaIcon::checkbox_marked_circle, HaEntityCat::none, true},
    // --- ADR-105 Category C: plain-name firmware-coined (13) ---
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_enabled,          ha_name_alias_ventilation_enabled,          HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_bypass_position,  ha_name_alias_ventilation_bypass_position,  HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_bypass_mode,      ha_name_alias_ventilation_bypass_mode,      HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_free_mode,        ha_name_alias_ventilation_free_mode,        HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_fault,            ha_name_alias_ventilation_fault,            HaIcon::alert_circle,           HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_active,           ha_name_alias_ventilation_active,           HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_bypass_status,    ha_name_alias_ventilation_bypass_status,    HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_bypass_automatic, ha_name_alias_ventilation_bypass_automatic, HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_free_status,      ha_name_alias_ventilation_free_status,      HaIcon::checkbox_marked_circle, HaEntityCat::none, false},
    { 70, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_diagnostic,       ha_name_alias_ventilation_diagnostic,       HaIcon::information,            HaEntityCat::none, false},
    { 74, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_system_type,      ha_name_alias_ventilation_system_type,      HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    { 74, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_ventilation_speed_control_type, ha_name_alias_ventilation_speed_control_type, HaIcon::checkbox_marked_circle, HaEntityCat::diagnostic, false},
    {101, MQTT_HA_FLAG_IS_HA_CORE_ALIAS, ha_lbl_alias_solar_storage_fault,          ha_name_alias_solar_storage_fault,          HaIcon::alert_circle,           HaEntityCat::none, false},
};
#undef SAT_BIN

// ========== Index arrays (OT ID -> first entry) ==========
const uint16_t PROGMEM mqttHaSensorIndex[256] = {
    0, // id 0, 2 entries
    2, // id 1, 2 entries
    4, // id 2, 2 entries
    6, // id 3, 2 entries
    8, // id 4, 3 entries
    11, // id 5, 2 entries
    13, // id 6, 2 entries
    15, // id 7, 2 entries
    17, // id 8, 2 entries
    19, // id 9, 2 entries
    21, // id 10, 2 entries
    23, // id 11, 2 entries
    25, // id 12, 2 entries
    27, // id 13, 2 entries
    29, // id 14, 2 entries
    31, // id 15, 2 entries
    33, // id 16, 2 entries
    35, // id 17, 2 entries
    37, // id 18, 2 entries
    39, // id 19, 2 entries
    41, // id 20, 3 entries
    44, // id 21, 2 entries
    46, // id 22, 2 entries
    48, // id 23, 2 entries
    50, // id 24, 2 entries
    52, // id 25, 2 entries
    54, // id 26, 2 entries
    56, // id 27, 2 entries
    58, // id 28, 2 entries
    60, // id 29, 2 entries
    62, // id 30, 2 entries
    64, // id 31, 2 entries
    66, // id 32, 2 entries
    68, // id 33, 2 entries
    70, // id 34, 2 entries
    72, // id 35, 2 entries
    74, // id 36, 2 entries
    76, // id 37, 2 entries
    78, // id 38, 2 entries
    80, // id 39, 2 entries
    0xFFFF, // id 40
    0xFFFF, // id 41
    0xFFFF, // id 42
    0xFFFF, // id 43
    0xFFFF, // id 44
    0xFFFF, // id 45
    0xFFFF, // id 46
    0xFFFF, // id 47
    82, // id 48, 4 entries
    86, // id 49, 4 entries
    90, // id 50, 2 entries
    92, // id 51, 4 entries
    96, // id 52, 4 entries
    100, // id 53, 4 entries
    104, // id 54, 4 entries
    108, // id 55, 4 entries
    112, // id 56, 2 entries
    114, // id 57, 2 entries
    116, // id 58, 2 entries
    118, // id 59, 2 entries
    120, // id 60, 2 entries
    122, // id 61, 2 entries
    124, // id 62, 2 entries
    126, // id 63, 2 entries
    0xFFFF, // id 64
    0xFFFF, // id 65
    0xFFFF, // id 66
    0xFFFF, // id 67
    0xFFFF, // id 68
    0xFFFF, // id 69
    128, // id 70, 2 entries
    130, // id 71, 6 entries
    136, // id 72, 2 entries
    138, // id 73, 2 entries
    140, // id 74, 2 entries
    142, // id 75, 2 entries
    144, // id 76, 2 entries
    146, // id 77, 6 entries
    152, // id 78, 6 entries
    158, // id 79, 2 entries
    160, // id 80, 2 entries
    162, // id 81, 2 entries
    164, // id 82, 2 entries
    166, // id 83, 2 entries
    168, // id 84, 2 entries
    170, // id 85, 2 entries
    172, // id 86, 4 entries
    176, // id 87, 6 entries
    182, // id 88, 2 entries
    184, // id 89, 2 entries
    186, // id 90, 2 entries
    188, // id 91, 2 entries
    0xFFFF, // id 92
    190, // id 93, 2 entries
    192, // id 94, 2 entries
    194, // id 95, 2 entries
    196, // id 96, 2 entries
    198, // id 97, 2 entries
    200, // id 98, 11 entries
    211, // id 99, 11 entries
    222, // id 100, 1 entry
    223, // id 101, 3 entries
    226, // id 102, 2 entries
    228, // id 103, 2 entries
    230, // id 104, 2 entries
    232, // id 105, 2 entries
    234, // id 106, 2 entries
    236, // id 107, 2 entries
    238, // id 108, 2 entries
    240, // id 109, 2 entries
    242, // id 110, 2 entries
    244, // id 111, 2 entries
    246, // id 112, 2 entries
    248, // id 113, 2 entries
    250, // id 114, 2 entries
    252, // id 115, 2 entries
    254, // id 116, 2 entries
    256, // id 117, 2 entries
    258, // id 118, 2 entries
    260, // id 119, 2 entries
    262, // id 120, 2 entries
    264, // id 121, 2 entries
    266, // id 122, 2 entries
    268, // id 123, 2 entries
    270, // id 124, 2 entries
    272, // id 125, 2 entries
    274, // id 126, 2 entries
    276, // id 127, 2 entries
    0xFFFF, // id 128
    0xFFFF, // id 129
    0xFFFF, // id 130
    278, // id 131, 2 entries
    280, // id 132, 2 entries
    282, // id 133, 2 entries
    0xFFFF, // id 134
    0xFFFF, // id 135
    0xFFFF, // id 136
    0xFFFF, // id 137
    0xFFFF, // id 138
    0xFFFF, // id 139
    0xFFFF, // id 140
    0xFFFF, // id 141
    0xFFFF, // id 142
    0xFFFF, // id 143
    0xFFFF, // id 144
    0xFFFF, // id 145
    0xFFFF, // id 146
    0xFFFF, // id 147
    0xFFFF, // id 148
    0xFFFF, // id 149
    0xFFFF, // id 150
    0xFFFF, // id 151
    0xFFFF, // id 152
    0xFFFF, // id 153
    0xFFFF, // id 154
    0xFFFF, // id 155
    0xFFFF, // id 156
    0xFFFF, // id 157
    0xFFFF, // id 158
    0xFFFF, // id 159
    0xFFFF, // id 160
    0xFFFF, // id 161
    0xFFFF, // id 162
    0xFFFF, // id 163
    0xFFFF, // id 164
    0xFFFF, // id 165
    0xFFFF, // id 166
    0xFFFF, // id 167
    0xFFFF, // id 168
    0xFFFF, // id 169
    0xFFFF, // id 170
    0xFFFF, // id 171
    0xFFFF, // id 172
    0xFFFF, // id 173
    0xFFFF, // id 174
    0xFFFF, // id 175
    0xFFFF, // id 176
    0xFFFF, // id 177
    0xFFFF, // id 178
    0xFFFF, // id 179
    0xFFFF, // id 180
    0xFFFF, // id 181
    0xFFFF, // id 182
    0xFFFF, // id 183
    0xFFFF, // id 184
    0xFFFF, // id 185
    0xFFFF, // id 186
    0xFFFF, // id 187
    0xFFFF, // id 188
    0xFFFF, // id 189
    0xFFFF, // id 190
    0xFFFF, // id 191
    0xFFFF, // id 192
    0xFFFF, // id 193
    0xFFFF, // id 194
    0xFFFF, // id 195
    0xFFFF, // id 196
    0xFFFF, // id 197
    0xFFFF, // id 198
    0xFFFF, // id 199
    0xFFFF, // id 200
    0xFFFF, // id 201
    0xFFFF, // id 202
    0xFFFF, // id 203
    0xFFFF, // id 204
    0xFFFF, // id 205
    0xFFFF, // id 206
    0xFFFF, // id 207
    0xFFFF, // id 208
    0xFFFF, // id 209
    0xFFFF, // id 210
    0xFFFF, // id 211
    0xFFFF, // id 212
    0xFFFF, // id 213
    0xFFFF, // id 214
    0xFFFF, // id 215
    0xFFFF, // id 216
    0xFFFF, // id 217
    0xFFFF, // id 218
    0xFFFF, // id 219
    0xFFFF, // id 220
    0xFFFF, // id 221
    0xFFFF, // id 222
    0xFFFF, // id 223
    0xFFFF, // id 224
    0xFFFF, // id 225
    0xFFFF, // id 226
    0xFFFF, // id 227
    0xFFFF, // id 228
    0xFFFF, // id 229
    0xFFFF, // id 230
    0xFFFF, // id 231
    0xFFFF, // id 232
    0xFFFF, // id 233
    0xFFFF, // id 234
    0xFFFF, // id 235
    0xFFFF, // id 236
    0xFFFF, // id 237
    0xFFFF, // id 238
    0xFFFF, // id 239
    0xFFFF, // id 240
    0xFFFF, // id 241
    0xFFFF, // id 242
    0xFFFF, // id 243
    0xFFFF, // id 244
    284, // id 245, 4 entries
    288, // id 246, 1 entry
    289, // id 247, 17 entries
    306, // id 248, 5 entries (TASK-541 firmware diagnostics + ADR-113 hardware_type)
    311, // id 249, 4 entries (TASK-541 PIC info; ADR-113 stage 2 removed picavailable)
    315, // id 250, 15 entries (TASK-541 PIC settings)
    330, // id 251, 7 entries (TASK-541 OTDirect/SAT diagnostics)
    337, // id 252, 32 entries (TASK-543 SAT control/PID/cycle/stats)
    369, // id 253, 15 entries (TASK-543 SAT BLE/pressure/weather)
    384, // id 254, 1 entry  (TASK-543 SAT flame status)
    0xFFFF // id 255
};

const uint16_t PROGMEM mqttHaBinSensorIndex[256] = {
    0, // id 0, 17 entries
    0xFFFF, // id 1
    17, // id 2, 1 entry
    18, // id 3, 8 entries
    0xFFFF, // id 4
    26, // id 5, 6 entries
    32, // id 6, 4 entries
    0xFFFF, // id 7
    0xFFFF, // id 8
    0xFFFF, // id 9
    0xFFFF, // id 10
    0xFFFF, // id 11
    0xFFFF, // id 12
    0xFFFF, // id 13
    0xFFFF, // id 14
    0xFFFF, // id 15
    0xFFFF, // id 16
    0xFFFF, // id 17
    0xFFFF, // id 18
    0xFFFF, // id 19
    0xFFFF, // id 20
    0xFFFF, // id 21
    0xFFFF, // id 22
    0xFFFF, // id 23
    0xFFFF, // id 24
    0xFFFF, // id 25
    0xFFFF, // id 26
    0xFFFF, // id 27
    0xFFFF, // id 28
    0xFFFF, // id 29
    0xFFFF, // id 30
    0xFFFF, // id 31
    0xFFFF, // id 32
    0xFFFF, // id 33
    0xFFFF, // id 34
    0xFFFF, // id 35
    0xFFFF, // id 36
    0xFFFF, // id 37
    0xFFFF, // id 38
    0xFFFF, // id 39
    0xFFFF, // id 40
    0xFFFF, // id 41
    0xFFFF, // id 42
    0xFFFF, // id 43
    0xFFFF, // id 44
    0xFFFF, // id 45
    0xFFFF, // id 46
    0xFFFF, // id 47
    0xFFFF, // id 48
    0xFFFF, // id 49
    0xFFFF, // id 50
    0xFFFF, // id 51
    0xFFFF, // id 52
    0xFFFF, // id 53
    0xFFFF, // id 54
    0xFFFF, // id 55
    0xFFFF, // id 56
    0xFFFF, // id 57
    0xFFFF, // id 58
    0xFFFF, // id 59
    0xFFFF, // id 60
    0xFFFF, // id 61
    0xFFFF, // id 62
    0xFFFF, // id 63
    0xFFFF, // id 64
    0xFFFF, // id 65
    0xFFFF, // id 66
    0xFFFF, // id 67
    0xFFFF, // id 68
    0xFFFF, // id 69
    36, // id 70, 10 entries
    0xFFFF, // id 71
    0xFFFF, // id 72
    0xFFFF, // id 73
    46, // id 74, 3 entries
    0xFFFF, // id 75
    0xFFFF, // id 76
    0xFFFF, // id 77
    0xFFFF, // id 78
    0xFFFF, // id 79
    0xFFFF, // id 80
    0xFFFF, // id 81
    0xFFFF, // id 82
    0xFFFF, // id 83
    0xFFFF, // id 84
    0xFFFF, // id 85
    0xFFFF, // id 86
    0xFFFF, // id 87
    0xFFFF, // id 88
    0xFFFF, // id 89
    0xFFFF, // id 90
    0xFFFF, // id 91
    0xFFFF, // id 92
    0xFFFF, // id 93
    0xFFFF, // id 94
    0xFFFF, // id 95
    0xFFFF, // id 96
    0xFFFF, // id 97
    0xFFFF, // id 98
    0xFFFF, // id 99
    49, // id 100, 2 entries
    51, // id 101, 1 entry
    0xFFFF, // id 102
    0xFFFF, // id 103
    0xFFFF, // id 104
    0xFFFF, // id 105
    0xFFFF, // id 106
    0xFFFF, // id 107
    0xFFFF, // id 108
    0xFFFF, // id 109
    0xFFFF, // id 110
    0xFFFF, // id 111
    0xFFFF, // id 112
    52, // id 113, 1 entry
    0xFFFF, // id 114
    0xFFFF, // id 115
    0xFFFF, // id 116
    0xFFFF, // id 117
    0xFFFF, // id 118
    0xFFFF, // id 119
    0xFFFF, // id 120
    0xFFFF, // id 121
    0xFFFF, // id 122
    0xFFFF, // id 123
    0xFFFF, // id 124
    0xFFFF, // id 125
    0xFFFF, // id 126
    0xFFFF, // id 127
    0xFFFF, // id 128
    0xFFFF, // id 129
    0xFFFF, // id 130
    0xFFFF, // id 131
    0xFFFF, // id 132
    0xFFFF, // id 133
    0xFFFF, // id 134
    0xFFFF, // id 135
    0xFFFF, // id 136
    0xFFFF, // id 137
    0xFFFF, // id 138
    0xFFFF, // id 139
    0xFFFF, // id 140
    0xFFFF, // id 141
    0xFFFF, // id 142
    0xFFFF, // id 143
    0xFFFF, // id 144
    0xFFFF, // id 145
    0xFFFF, // id 146
    0xFFFF, // id 147
    0xFFFF, // id 148
    0xFFFF, // id 149
    0xFFFF, // id 150
    0xFFFF, // id 151
    0xFFFF, // id 152
    0xFFFF, // id 153
    0xFFFF, // id 154
    0xFFFF, // id 155
    0xFFFF, // id 156
    0xFFFF, // id 157
    0xFFFF, // id 158
    0xFFFF, // id 159
    0xFFFF, // id 160
    0xFFFF, // id 161
    0xFFFF, // id 162
    0xFFFF, // id 163
    0xFFFF, // id 164
    0xFFFF, // id 165
    0xFFFF, // id 166
    0xFFFF, // id 167
    0xFFFF, // id 168
    0xFFFF, // id 169
    0xFFFF, // id 170
    0xFFFF, // id 171
    0xFFFF, // id 172
    0xFFFF, // id 173
    0xFFFF, // id 174
    0xFFFF, // id 175
    0xFFFF, // id 176
    0xFFFF, // id 177
    0xFFFF, // id 178
    0xFFFF, // id 179
    0xFFFF, // id 180
    0xFFFF, // id 181
    0xFFFF, // id 182
    0xFFFF, // id 183
    0xFFFF, // id 184
    0xFFFF, // id 185
    0xFFFF, // id 186
    0xFFFF, // id 187
    0xFFFF, // id 188
    0xFFFF, // id 189
    0xFFFF, // id 190
    0xFFFF, // id 191
    0xFFFF, // id 192
    0xFFFF, // id 193
    0xFFFF, // id 194
    0xFFFF, // id 195
    0xFFFF, // id 196
    0xFFFF, // id 197
    0xFFFF, // id 198
    0xFFFF, // id 199
    0xFFFF, // id 200
    0xFFFF, // id 201
    0xFFFF, // id 202
    0xFFFF, // id 203
    0xFFFF, // id 204
    0xFFFF, // id 205
    0xFFFF, // id 206
    0xFFFF, // id 207
    0xFFFF, // id 208
    0xFFFF, // id 209
    0xFFFF, // id 210
    0xFFFF, // id 211
    0xFFFF, // id 212
    0xFFFF, // id 213
    0xFFFF, // id 214
    0xFFFF, // id 215
    0xFFFF, // id 216
    0xFFFF, // id 217
    0xFFFF, // id 218
    0xFFFF, // id 219
    0xFFFF, // id 220
    0xFFFF, // id 221
    0xFFFF, // id 222
    0xFFFF, // id 223
    0xFFFF, // id 224
    0xFFFF, // id 225
    0xFFFF, // id 226
    0xFFFF, // id 227
    0xFFFF, // id 228
    0xFFFF, // id 229
    0xFFFF, // id 230
    0xFFFF, // id 231
    0xFFFF, // id 232
    0xFFFF, // id 233
    0xFFFF, // id 234
    0xFFFF, // id 235
    0xFFFF, // id 236
    0xFFFF, // id 237
    0xFFFF, // id 238
    0xFFFF, // id 239
    0xFFFF, // id 240
    0xFFFF, // id 241
    0xFFFF, // id 242
    0xFFFF, // id 243
    0xFFFF, // id 244
    0xFFFF, // id 245
    0xFFFF, // id 246
    0xFFFF, // id 247
    0xFFFF, // id 248
    0xFFFF, // id 249
    0xFFFF, // id 250
    0xFFFF, // id 251
    0xFFFF, // id 252
    53, // id 253, 1 entry  (TASK-543 weather is_day)
    54, // id 254, 4 entries (TASK-543 SAT binary primaries)
    0xFFFF // id 255
};

// Climate (2) and Number (1) entries are handled by streaming functions below.

// ========== Enum-to-string lookup functions ==========

PGM_P haDeviceClassStr(HaDeviceClass dc) {
    switch (dc) {
        case HaDeviceClass::none: return nullptr;
        case HaDeviceClass::temperature: { static const char s[] PROGMEM = "temperature"; return s; }
        case HaDeviceClass::pressure: { static const char s[] PROGMEM = "pressure"; return s; }
        case HaDeviceClass::humidity: { static const char s[] PROGMEM = "humidity"; return s; }
        case HaDeviceClass::power: { static const char s[] PROGMEM = "power"; return s; }
        case HaDeviceClass::power_factor: { static const char s[] PROGMEM = "power_factor"; return s; }
        case HaDeviceClass::energy: { static const char s[] PROGMEM = "energy"; return s; }
        case HaDeviceClass::carbon_dioxide: { static const char s[] PROGMEM = "carbon_dioxide"; return s; }
        default: return nullptr;
    }
}

PGM_P haUnitStr(HaUnit u) {
    switch (u) {
        case HaUnit::none: return nullptr;
        case HaUnit::degC: { static const char s[] PROGMEM = "°C"; return s; }
        case HaUnit::deg: { static const char s[] PROGMEM = "°"; return s; }
        case HaUnit::bar: { static const char s[] PROGMEM = "bar"; return s; }
        case HaUnit::hPa: { static const char s[] PROGMEM = "hPa"; return s; }
        case HaUnit::percent: { static const char s[] PROGMEM = "%"; return s; }
        case HaUnit::l_min: { static const char s[] PROGMEM = "l/min"; return s; }
        case HaUnit::kW: { static const char s[] PROGMEM = "kW"; return s; }
        case HaUnit::W: { static const char s[] PROGMEM = "W"; return s; }
        case HaUnit::kWh: { static const char s[] PROGMEM = "kWh"; return s; }
        case HaUnit::uA: { static const char s[] PROGMEM = "µA"; return s; }
        case HaUnit::Hz: { static const char s[] PROGMEM = "Hz"; return s; }
        case HaUnit::rpm: { static const char s[] PROGMEM = "rpm"; return s; }
        case HaUnit::ppm: { static const char s[] PROGMEM = "ppm"; return s; }
        case HaUnit::mS: { static const char s[] PROGMEM = "mS"; return s; }
        case HaUnit::m_s: { static const char s[] PROGMEM = "m/s"; return s; }
        case HaUnit::mm: { static const char s[] PROGMEM = "mm"; return s; }
        case HaUnit::s: { static const char s[] PROGMEM = "s"; return s; }
        case HaUnit::h: { static const char s[] PROGMEM = "h"; return s; }
        case HaUnit::bytes: { static const char s[] PROGMEM = "B"; return s; }
        default: return nullptr;
    }
}

PGM_P haStateClassStr(HaStateClass sc) {
    switch (sc) {
        case HaStateClass::none: return nullptr;
        case HaStateClass::measurement: { static const char s[] PROGMEM = "measurement"; return s; }
        case HaStateClass::total_increasing: { static const char s[] PROGMEM = "total_increasing"; return s; }
        default: return nullptr;
    }
}

PGM_P haIconStr(HaIcon ic) {
    switch (ic) {
        case HaIcon::none: return nullptr;
        case HaIcon::thermometer: { static const char s[] PROGMEM = "thermometer"; return s; }
        case HaIcon::gauge: { static const char s[] PROGMEM = "gauge"; return s; }
        case HaIcon::water_percent: { static const char s[] PROGMEM = "water-percent"; return s; }
        case HaIcon::flash: { static const char s[] PROGMEM = "flash"; return s; }
        case HaIcon::angle_acute: { static const char s[] PROGMEM = "angle-acute"; return s; }
        case HaIcon::lightning_bolt: { static const char s[] PROGMEM = "lightning-bolt"; return s; }
        case HaIcon::molecule_co2: { static const char s[] PROGMEM = "molecule-co2"; return s; }
        case HaIcon::percent_outline: { static const char s[] PROGMEM = "percent-outline"; return s; }
        case HaIcon::timer_outline: { static const char s[] PROGMEM = "timer-outline"; return s; }
        case HaIcon::counter: { static const char s[] PROGMEM = "counter"; return s; }
        case HaIcon::information_outline: { static const char s[] PROGMEM = "information-outline"; return s; }
        case HaIcon::fan: { static const char s[] PROGMEM = "fan"; return s; }
        case HaIcon::current_ac: { static const char s[] PROGMEM = "current-ac"; return s; }
        case HaIcon::clock_outline: { static const char s[] PROGMEM = "clock-outline"; return s; }
        case HaIcon::pulse: { static const char s[] PROGMEM = "pulse"; return s; }
        case HaIcon::alert_circle: { static const char s[] PROGMEM = "alert-circle"; return s; }
        case HaIcon::fire: { static const char s[] PROGMEM = "fire"; return s; }
        case HaIcon::radiator: { static const char s[] PROGMEM = "radiator"; return s; }
        case HaIcon::water_boiler: { static const char s[] PROGMEM = "water-boiler"; return s; }
        case HaIcon::snowflake: { static const char s[] PROGMEM = "snowflake"; return s; }
        case HaIcon::information: { static const char s[] PROGMEM = "information"; return s; }
        case HaIcon::toggle_switch: { static const char s[] PROGMEM = "toggle-switch"; return s; }
        case HaIcon::lan_connect: { static const char s[] PROGMEM = "lan-connect"; return s; }
        case HaIcon::checkbox_marked_circle: { static const char s[] PROGMEM = "checkbox-marked-circle"; return s; }
        case HaIcon::thermostat_icon: { static const char s[] PROGMEM = "thermostat"; return s; }
        case HaIcon::thermometer_lines: { static const char s[] PROGMEM = "thermometer-lines"; return s; }
        case HaIcon::air_filter: { static const char s[] PROGMEM = "air-filter"; return s; }
        case HaIcon::alert_outline: { static const char s[] PROGMEM = "alert-outline"; return s; }
        case HaIcon::antenna: { static const char s[] PROGMEM = "antenna"; return s; }
        case HaIcon::arrow_expand_horizontal: { static const char s[] PROGMEM = "arrow-expand-horizontal"; return s; }
        case HaIcon::calendar: { static const char s[] PROGMEM = "calendar"; return s; }
        case HaIcon::card_account_details: { static const char s[] PROGMEM = "card-account-details"; return s; }
        case HaIcon::cog: { static const char s[] PROGMEM = "cog"; return s; }
        case HaIcon::console: { static const char s[] PROGMEM = "console"; return s; }
        case HaIcon::format_list_numbered: { static const char s[] PROGMEM = "format-list-numbered"; return s; }
        case HaIcon::history: { static const char s[] PROGMEM = "history"; return s; }
        case HaIcon::list_status: { static const char s[] PROGMEM = "list-status"; return s; }
        case HaIcon::remote: { static const char s[] PROGMEM = "remote"; return s; }
        case HaIcon::solar_panel: { static const char s[] PROGMEM = "solar-panel"; return s; }
        case HaIcon::speedometer: { static const char s[] PROGMEM = "speedometer"; return s; }
        case HaIcon::tag: { static const char s[] PROGMEM = "tag"; return s; }
        case HaIcon::tune_variant: { static const char s[] PROGMEM = "tune-variant"; return s; }
        case HaIcon::water: { static const char s[] PROGMEM = "water"; return s; }
        default: return nullptr;
    }
}

PGM_P haEntityCatStr(HaEntityCat ec) {
    switch (ec) {
        case HaEntityCat::none: return nullptr;
        case HaEntityCat::diagnostic: { static const char s[] PROGMEM = "diagnostic"; return s; }
        default: return nullptr;
    }
}

// ========== END AUTO-GENERATED SECTION ==========






// Hand-written code below -- DO NOT remove this marker

// ---------------------------------------------------------------------------
// Streaming HA MQTT discovery JSON composer
//
// Builds HA discovery JSON payloads at runtime and writes them directly
// to MQTT via PubSubClient::beginPublish/write/endPublish. Uses a
// dual-mode writer (MqttJsonWriter) that first measures the payload
// length, then writes it -- keeping the two passes in perfect sync.
//
// Lives in a .cpp file (not .ino) to avoid Arduino's auto-prototype
// generator injecting broken forward declarations for functions with
// MqttJsonWriter& parameters.
//
// Copyright (c) 2021-2026 Robert van den Breemen
// TERMS OF USE: MIT License.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// strlcpy_P fallback for ESP8266 Arduino Core 3.1.2 lives centrally in MQTTstuff.h,
// which this TU already includes (line 29). No local redefinition needed.

// External functions from the .ino translation unit.
extern bool canPublishMQTT();
extern void feedWatchDog();
extern void incPublishedTopicCount();   // ADR-062 / TASK-349: called after every successful retained discovery publish

// ---------------------------------------------------------------------------
// JSON streaming helpers
// ---------------------------------------------------------------------------

static bool writeJsonKV(MqttJsonWriter &w, PGM_P key, const char *value) {
  return w.writeChar('"') && w.writeProgmem(key) && w.writeProgmem(PSTR("\":\""))
      && w.writeRam(value) && w.writeChar('"');
}

static bool writeJsonKV_P(MqttJsonWriter &w, PGM_P key, PGM_P value) {
  return w.writeChar('"') && w.writeProgmem(key) && w.writeProgmem(PSTR("\":\""))
      && w.writeProgmem(value) && w.writeChar('"');
}

static bool writeJsonKVBool(MqttJsonWriter &w, PGM_P key, bool value) {
  return w.writeChar('"') && w.writeProgmem(key) && w.writeProgmem(PSTR("\":"))
      && w.writeProgmem(value ? PSTR("true") : PSTR("false"));
}

static bool writeJsonOpen(MqttJsonWriter &w)  { return w.writeChar('{'); }
static bool writeJsonClose(MqttJsonWriter &w) { return w.writeChar('}'); }
static bool writeJsonComma(MqttJsonWriter &w) { return w.writeChar(','); }

// Write a string to the JSON writer for the human-facing "name" field in HA
// discovery configs. Two transforms applied char-by-char:
//   1. '_' becomes ' ' (sibling-of-hostname / inter-word separators)
//   2. The first letter of every word is upper-cased; existing capitals are
//      preserved. Word boundary = start-of-string or after a space.
//
// This produces consistent Title Case from the heterogeneously-cased PROGMEM
// friendlyName tables: "central_heating", "Solar_collector_temperature",
// "status_vh_master" all render as "Central Heating", "Solar Collector
// Temperature", "Status Vh Master". Mixed-case acronyms in the source like
// "CH2" or "DHW" are preserved unchanged because the transform never lowercases.
//
// Transformation is friendly-name-only — unique_id, stat_t topic, entity_id,
// and discovery topic path continue to use the underscore form (Andre, 2026-05-07).
static bool writeFriendlyName(MqttJsonWriter &w, const char *s) {
  if (!s) return true;
  bool atWordStart = true;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (c == '_') c = ' ';
    if (atWordStart && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    if (!w.writeChar(c)) return false;
    atWordStart = (c == ' ');
  }
  return true;
}

// ---------------------------------------------------------------------------
// PROGMEM string constants for discovery JSON keys
// ---------------------------------------------------------------------------
static const char kAvtyT[]    PROGMEM = "avty_t";
static const char kDev[]      PROGMEM = "dev";
static const char kIds[]      PROGMEM = "identifiers";
static const char kMfr[]      PROGMEM = "manufacturer";
static const char kModel[]    PROGMEM = "model";
static const char kSwVer[]    PROGMEM = "sw_version";
static const char kDevName[]  PROGMEM = "name";
static const char kUniqId[]   PROGMEM = "uniq_id";
static const char kName[]     PROGMEM = "name";
static const char kStatT[]    PROGMEM = "stat_t";
static const char kDevCls[]   PROGMEM = "device_class";
static const char kUom[]      PROGMEM = "unit_of_measurement";
static const char kStatCls[]  PROGMEM = "state_class";
static const char kIcon[]     PROGMEM = "icon";
static const char kEntCat[]   PROGMEM = "entity_category";
static const char kEnByDef[]  PROGMEM = "enabled_by_default";
static const char kValTpl[]   PROGMEM = "value_template";
static const char kJsonAttrTopic[] PROGMEM = "json_attributes_topic";
static const char kOrigin[]   PROGMEM = "origin";
static const char kUrl[]      PROGMEM = "url";

// Fixed PROGMEM values
static const char kValTplVal[]  PROGMEM = "{{ value }}";
static const char kOriginName[] PROGMEM = "OTGW-firmware";
static const char kOriginUrl[]  PROGMEM = "https://github.com/rvdbreemen/OTGW-firmware";

// ---------------------------------------------------------------------------
// TASK-648: modern device-identifier suffix per HaDevice; legacy uses bare nodeId.
// ---------------------------------------------------------------------------
static PGM_P haDeviceSuffix(HaDevice d) {
  switch (d) {
    case HaDevice::Boiler:     return PSTR("-boiler");
    case HaDevice::Thermostat: return PSTR("-thermostat");
    case HaDevice::Gateway:    return PSTR("-gateway");
    case HaDevice::Esp:        return PSTR("-esp");
    case HaDevice::Sat:        return PSTR("-sat");
  }
  return PSTR("-esp");
}

// ---------------------------------------------------------------------------
// Device block: full (first-per-device) or minimal (subsequent).
// In legacy mode: bare nodeId identifier, gated by ctx.isFirstEntity (unchanged).
// In modern mode: nodeId+suffix identifier, gated by ctx.deviceIntroduced[device].
// ctx is non-const so deviceIntroduced can be set after emitting the full block.
// ---------------------------------------------------------------------------
static bool writeDeviceBlock(MqttJsonWriter &w, HaDiscoveryContext &ctx) {
  const bool useLegacy = ctx.legacyMode;

  // Determine whether to emit the full block for this entity.
  const uint8_t devIdx = static_cast<uint8_t>(ctx.device);
  // TASK-648 Job B: deviceIntroduced[devIdx] false = not yet introduced; emit full block.
  const bool emitFull = useLegacy ? ctx.isFirstEntity
                                  : (ctx.deviceIntroduced ? !ctx.deviceIntroduced[devIdx] : false);

  // Open device object and emit identifier string.
  // Legacy: bare nodeId.  Modern: nodeId + device suffix.
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kDev)) return false;
  if (!w.writeProgmem(PSTR("\":{"))) return false;
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kIds)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!w.writeRam(ctx.nodeId)) return false;
  if (!useLegacy) {
    if (!w.writeProgmem(haDeviceSuffix(ctx.device))) return false;
  }
  if (!w.writeChar('"')) return false;

  if (emitFull) {
    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV(w, kMfr, ctx.manufacturer)) return false;
    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV(w, kModel, ctx.model)) return false;
    if (!writeJsonComma(w)) return false;
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kDevName)) return false;
    if (!w.writeProgmem(PSTR("\":\"OpenTherm Gateway ("))) return false;
    if (!w.writeRam(ctx.hostname)) return false;
    if (!w.writeProgmem(PSTR(")\""))) return false;
    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV(w, kSwVer, ctx.version)) return false;
    // Mark device as introduced so subsequent entities get the minimal block.
    if (!useLegacy && ctx.deviceIntroduced) ctx.deviceIntroduced[devIdx] = true;
  }

  return w.writeChar('}');
}

// ---------------------------------------------------------------------------
// Origin block
// ---------------------------------------------------------------------------
static bool writeOriginBlock(MqttJsonWriter &w, const HaDiscoveryContext &ctx) {
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kOrigin)) return false;
  if (!w.writeProgmem(PSTR("\":{"))) return false;
  if (!writeJsonKV_P(w, kDevName, kOriginName)) return false;
  if (!writeJsonComma(w)) return false;
  if (!writeJsonKV(w, PSTR("sw"), ctx.version)) return false;
  if (!writeJsonComma(w)) return false;
  if (!writeJsonKV_P(w, kUrl, kOriginUrl)) return false;
  return w.writeChar('}');
}

// Make a label safe for use as an HA discovery object_id / uniq_id component.
// HA requires [a-zA-Z0-9_-] only; any other byte (e.g. '/') is replaced with '_'.
// The MQTT stat_t value is built from the original label and is NOT affected.
static void sanitizeHaObjectId(char *s) {
  if (!s) return;
  for (; *s; ++s) {
    char c = *s;
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) *s = '_';
  }
}

// ---------------------------------------------------------------------------
// Sensor payload composer
// ---------------------------------------------------------------------------
static bool composeSensorPayload(MqttJsonWriter &w,
                                 const MqttHaSensorCfg &cfg,
                                 HaDiscoveryContext &ctx)
{
  char label[48];
  char idLabel[48];
  char friendlyName[80];
  strlcpy_P(label, cfg.label, sizeof(label));
  strlcpy(idLabel, label, sizeof(idLabel));
  sanitizeHaObjectId(idLabel);
  strlcpy_P(friendlyName, cfg.friendlyName, sizeof(friendlyName));

  bool hasSrc = (ctx.sourceSuffix && ctx.sourceSuffix[0] != '\0');

  if (!writeJsonOpen(w)) return false;

  // "avty_t":"<mqttPubTopic>"
  if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
  if (!writeJsonComma(w)) return false;

  if (!writeDeviceBlock(w, ctx)) return false;
  if (!writeJsonComma(w)) return false;

  // "uniq_id":"<nodeId>[-<device>]-<label>[<sourceSuffix>]"
  // Modern mode: nodeId + device suffix + label. Legacy: nodeId + label (byte-identical to pre-648).
  // Uses idLabel (sanitized) so HA-forbidden characters like '/' become '_'.
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kUniqId)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!w.writeRam(ctx.nodeId)) return false;
  if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
  if (!w.writeChar('-')) return false;
  if (!w.writeRam(idLabel)) return false;
  if (hasSrc) { if (!w.writeRam(ctx.sourceSuffix)) return false; }
  if (!w.writeChar('"')) return false;
  if (!writeJsonComma(w)) return false;

  // "name":"<friendlyName>[ <sourceName>]"
  // Hostname dropped (device card title shows "OpenTherm Gateway (<hostname>)"
  // already, so prepending hostname to entity name is redundant). writeFriendlyName
  // applies '_' -> ' ' + Title Case for consistent rendering. (Andre 2026-05-07.)
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kName)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!writeFriendlyName(w, friendlyName)) return false;
  if (hasSrc && ctx.sourceName && ctx.sourceName[0]) {
    if (!w.writeChar(' ')) return false;
    if (!w.writeRam(ctx.sourceName)) return false;
  }
  if (!w.writeChar('"')) return false;
  if (!writeJsonComma(w)) return false;

  // "stat_t":"<mqttPubTopic>/[otgw-pic/]<label>[_<sourceTopicSegment>]"
  // otgw-pic/ prefix applied when MQTT_HA_FLAG_IS_PIC_ENTRY is set -- see ADR-065.
  // Source segment uses underscore (sibling-suffix shape) per ADR-097, replacing
  // the slash (nested-children shape) from 2.0.0-alpha / ADR-096.
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kStatT)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!w.writeRam(ctx.mqttPubTopic)) return false;
  if (!w.writeChar('/')) return false;
  if (cfg.flags & MQTT_HA_FLAG_IS_PIC_ENTRY) {
    if (!w.writeProgmem(kPicSubtreePrefix)) return false;
  }
  if (!w.writeRam(label)) return false;
  if (hasSrc && ctx.sourceTopicSegment && ctx.sourceTopicSegment[0]) {
    if (!w.writeChar('_')) return false;  // ADR-097: sibling-suffix shape
    if (!w.writeRam(ctx.sourceTopicSegment)) return false;
  }
  if (!w.writeChar('"')) return false;

  // Optional fields
  PGM_P dcStr = haDeviceClassStr(cfg.deviceClass);
  if (dcStr) { if (!writeJsonComma(w)) return false; if (!writeJsonKV_P(w, kDevCls, dcStr)) return false; }

  PGM_P unitStr = haUnitStr(cfg.unit);
  if (unitStr) { if (!writeJsonComma(w)) return false; if (!writeJsonKV_P(w, kUom, unitStr)) return false; }

  PGM_P scStr = haStateClassStr(cfg.stateClass);
  if (scStr) { if (!writeJsonComma(w)) return false; if (!writeJsonKV_P(w, kStatCls, scStr)) return false; }

  PGM_P iconStr = haIconStr(cfg.icon);
  if (iconStr) {
    if (!writeJsonComma(w)) return false;
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kIcon)) return false;
    if (!w.writeProgmem(PSTR("\":\"mdi:"))) return false;
    if (!w.writeProgmem(iconStr)) return false;
    if (!w.writeChar('"')) return false;
  }

  PGM_P catStr = haEntityCatStr(cfg.entityCat);
  if (catStr) { if (!writeJsonComma(w)) return false; if (!writeJsonKV_P(w, kEntCat, catStr)) return false; }

  if (!cfg.enabledByDefault) { if (!writeJsonComma(w)) return false; if (!writeJsonKVBool(w, kEnByDef, false)) return false; }

  if (cfg.jsonAttributesTopic) {
    if (!writeJsonComma(w)) return false;
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kJsonAttrTopic)) return false;
    if (!w.writeProgmem(PSTR("\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeChar('/')) return false;
    if (!w.writeProgmem(cfg.jsonAttributesTopic)) return false;
    if (!w.writeChar('"')) return false;
  }

  // "value_template":"{{ value }}" (or a per-entity override for JSON/unit conversions)
  if (!writeJsonComma(w)) return false;
  if (!writeJsonKV_P(w, kValTpl, cfg.valueTemplate ? cfg.valueTemplate : kValTplVal)) return false;

  // origin block
  if (!writeJsonComma(w)) return false;
  if (!writeOriginBlock(w, ctx)) return false;

  return writeJsonClose(w);
}

// ---------------------------------------------------------------------------
// Binary sensor payload composer
// ---------------------------------------------------------------------------
static bool composeBinSensorPayload(MqttJsonWriter &w,
                                    const MqttHaBinSensorCfg &cfg,
                                    HaDiscoveryContext &ctx)
{
  char label[48];
  char friendlyName[80];
  strlcpy_P(label, cfg.label, sizeof(label));
  strlcpy_P(friendlyName, cfg.friendlyName, sizeof(friendlyName));

  if (!writeJsonOpen(w)) return false;

  if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
  if (!writeJsonComma(w)) return false;

  if (!writeDeviceBlock(w, ctx)) return false;
  if (!writeJsonComma(w)) return false;

  // "uniq_id":"<nodeId>[-<device>]-<label>"
  // Modern: nodeId + device suffix + label. Legacy: nodeId + label (byte-identical to pre-648).
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kUniqId)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!w.writeRam(ctx.nodeId)) return false;
  if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
  if (!w.writeChar('-')) return false;
  if (!w.writeRam(label)) return false;
  if (!w.writeChar('"')) return false;
  if (!writeJsonComma(w)) return false;

  // "name":"<friendlyName>" (hostname dropped, '_' -> ' ' + Title Case)
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kName)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!writeFriendlyName(w, friendlyName)) return false;
  if (!w.writeChar('"')) return false;
  if (!writeJsonComma(w)) return false;

  // "stat_t":"<mqttPubTopic>/[otgw-pic/]<label>"
  // otgw-pic/ prefix applied when MQTT_HA_FLAG_IS_PIC_ENTRY is set -- see ADR-065.
  if (!w.writeChar('"')) return false;
  if (!w.writeProgmem(kStatT)) return false;
  if (!w.writeProgmem(PSTR("\":\""))) return false;
  if (!w.writeRam(ctx.mqttPubTopic)) return false;
  if (!w.writeChar('/')) return false;
  if (cfg.flags & MQTT_HA_FLAG_IS_PIC_ENTRY) {
    if (!w.writeProgmem(kPicSubtreePrefix)) return false;
  }
  if (!w.writeRam(label)) return false;
  if (!w.writeChar('"')) return false;

  // Optional fields
  PGM_P iconStr = haIconStr(cfg.icon);
  if (iconStr) {
    if (!writeJsonComma(w)) return false;
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kIcon)) return false;
    if (!w.writeProgmem(PSTR("\":\"mdi:"))) return false;
    if (!w.writeProgmem(iconStr)) return false;
    if (!w.writeChar('"')) return false;
  }

  PGM_P catStr = haEntityCatStr(cfg.entityCat);
  if (catStr) { if (!writeJsonComma(w)) return false; if (!writeJsonKV_P(w, kEntCat, catStr)) return false; }

  if (!cfg.enabledByDefault) { if (!writeJsonComma(w)) return false; if (!writeJsonKVBool(w, kEnByDef, false)) return false; }

  if (cfg.deviceClass) {
    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV_P(w, kDevCls, cfg.deviceClass)) return false;
  }

  switch (cfg.payload) {
    case HaBinaryPayload::true_false:
      if (!writeJsonComma(w)) return false;
      if (!w.writeProgmem(PSTR("\"pl_on\":\"true\",\"pl_off\":\"false\",\"stat_on\":\"true\",\"stat_off\":\"false\""))) return false;
      break;
    case HaBinaryPayload::one_zero:
      if (!writeJsonComma(w)) return false;
      if (!w.writeProgmem(PSTR("\"payload_on\":1,\"payload_off\":0"))) return false;
      break;
    case HaBinaryPayload::on_off:
    default:
      break;
  }

  if (!writeJsonComma(w)) return false;
  if (!writeOriginBlock(w, ctx)) return false;

  return writeJsonClose(w);
}

// ---------------------------------------------------------------------------
// Topic builders
// ---------------------------------------------------------------------------
// Source-variant discovery topics use sibling-suffix shape (`<label>_<src>`)
// per ADR-098, which supersedes the nested-children carve-out from ADR-097.
// HA's discovery dispatcher (homeassistant/components/mqtt/discovery.py
// TOPIC_MATCHER) restricts object_id to [a-zA-Z0-9_-]+, so the previous
// nested form (`<label>/<src>/config`) was rejected with "illegal discovery
// topic" and silently discarded — affecting both ESP8266 and ESP32-S3 builds.
// Canonical (no source) keeps the bare label as the object_id, which has
// always matched the regex. Mirrors dev ADR-071 / TASK-556.
static bool buildSensorDiscoveryTopic(char *dest, size_t destSize,
                                      const char *haPrefix, const char *nodeId,
                                      PGM_P label, const char *sourceTopicSegment)
{
  char labelBuf[48];
  strlcpy_P(labelBuf, label, sizeof(labelBuf));
  sanitizeHaObjectId(labelBuf);
  int n;
  if (sourceTopicSegment && sourceTopicSegment[0]) {
    n = snprintf_P(dest, destSize, PSTR("%s/sensor/%s/%s_%s/config"),
                   haPrefix, nodeId, labelBuf, sourceTopicSegment);
  } else {
    n = snprintf_P(dest, destSize, PSTR("%s/sensor/%s/%s/config"),
                   haPrefix, nodeId, labelBuf);
  }
  return (n > 0 && static_cast<size_t>(n) < destSize);
}

static bool buildBinSensorDiscoveryTopic(char *dest, size_t destSize,
                                         const char *haPrefix, const char *nodeId,
                                         PGM_P label)
{
  char labelBuf[48];
  strlcpy_P(labelBuf, label, sizeof(labelBuf));
  int n = snprintf_P(dest, destSize, PSTR("%s/binary_sensor/%s/%s/config"),
                     haPrefix, nodeId, labelBuf);
  return (n > 0 && static_cast<size_t>(n) < destSize);
}

// ---------------------------------------------------------------------------
// Minimum free heap for discovery publish (same as MQTTstuff.ino)
// ---------------------------------------------------------------------------
static constexpr uint32_t STREAM_HEAP_MIN = 4000;  // Streaming needs ~200 bytes, not 1200+
static constexpr size_t   STREAM_TOPIC_MAX = 200;

// ---------------------------------------------------------------------------
// Public API: streamSensorDiscovery
// ---------------------------------------------------------------------------
bool streamSensorDiscovery(PubSubClient &client,
                           const MqttHaSensorCfg &cfg,
                           HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  bool hasSrc = (ctx.sourceSuffix && ctx.sourceSuffix[0] != '\0');

  char topic[STREAM_TOPIC_MAX];
  if (!buildSensorDiscoveryTopic(topic, sizeof(topic), ctx.haPrefix, ctx.nodeId,
                                 cfg.label, hasSrc ? ctx.sourceTopicSegment : nullptr))
    return false;

  // Measure pass
  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!composeSensorPayload(measure, cfg, ctx)) return false;

  // Begin publish with exact payload length
  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  // Write pass
  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!composeSensorPayload(writer, cfg, ctx) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;

  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// Public API: streamBinarySensorDiscovery
// ---------------------------------------------------------------------------
bool streamBinarySensorDiscovery(PubSubClient &client,
                                 const MqttHaBinSensorCfg &cfg,
                                 HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  char topic[STREAM_TOPIC_MAX];
  if (!buildBinSensorDiscoveryTopic(topic, sizeof(topic), ctx.haPrefix, ctx.nodeId,
                                    cfg.label))
    return false;

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!composeBinSensorPayload(measure, cfg, ctx)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!composeBinSensorPayload(writer, cfg, ctx) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;

  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// Public API: streamDallasSensorDiscovery
// Dallas temperature sensors have dynamic addresses known only at runtime.
// This function builds a sensor discovery payload using the address as label.
// ---------------------------------------------------------------------------
bool streamDallasSensorDiscovery(PubSubClient &client,
                                 const char *sensorAddress,
                                 HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;
  if (!sensorAddress || sensorAddress[0] == '\0') return false;

  // Build topic: <haPrefix>/sensor/<nodeId>/<sensorAddress>/config
  char topic[STREAM_TOPIC_MAX];
  int n = snprintf_P(topic, sizeof(topic), PSTR("%s/sensor/%s/%s/config"),
                     ctx.haPrefix, ctx.nodeId, sensorAddress);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(topic)) return false;

  // Build a temporary config struct with the runtime address
  static const char dallasFriendlyName[] PROGMEM = "Temperature";
  MqttHaSensorCfg cfg;
  cfg.id = 246;  // OTGWdallasdataid
  cfg.flags = 0;
  cfg.label = nullptr;  // not used directly -- we override stat_t below
  cfg.friendlyName = dallasFriendlyName;
  cfg.deviceClass = HaDeviceClass::temperature;
  cfg.unit = HaUnit::degC;
  cfg.stateClass = HaStateClass::measurement;
  cfg.icon = HaIcon::thermometer;
  cfg.entityCat = HaEntityCat::none;
  cfg.enabledByDefault = true;

  // We need a custom compose since the label is a runtime RAM string, not PROGMEM.
  // Use the standard composer but with a patched context that has the address as sensorId.
  // Actually, we compose inline here for clarity.

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    // "avty_t"
    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    // device block
    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // "uniq_id":"<nodeId>[-<device>]-<sensorAddress>"
    // Modern: nodeId + device suffix + address. Legacy: nodeId + address.
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kUniqId)) return false;
    if (!w.writeProgmem(PSTR("\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeChar('-')) return false;
    if (!w.writeRam(sensorAddress)) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    // "name":"Temperature <sensorAddress>" (hostname dropped per Andre 2026-05-07)
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kName)) return false;
    if (!w.writeProgmem(PSTR("\":\"Temperature "))) return false;
    if (!w.writeRam(sensorAddress)) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    // "stat_t":"<mqttPubTopic>/<sensorAddress>"
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kStatT)) return false;
    if (!w.writeProgmem(PSTR("\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeChar('/')) return false;
    if (!w.writeRam(sensorAddress)) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    // "device_class":"temperature"
    if (!writeJsonKV_P(w, kDevCls, haDeviceClassStr(HaDeviceClass::temperature))) return false;
    if (!writeJsonComma(w)) return false;

    // "unit_of_measurement":"°C"
    if (!writeJsonKV_P(w, kUom, haUnitStr(HaUnit::degC))) return false;
    if (!writeJsonComma(w)) return false;

    // "state_class":"measurement"
    if (!writeJsonKV_P(w, kStatCls, haStateClassStr(HaStateClass::measurement))) return false;
    if (!writeJsonComma(w)) return false;

    // "icon":"mdi:thermometer"
    if (!w.writeChar('"')) return false;
    if (!w.writeProgmem(kIcon)) return false;
    if (!w.writeProgmem(PSTR("\":\"mdi:"))) return false;
    if (!w.writeProgmem(haIconStr(HaIcon::thermometer))) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    // "value_template":"{{ value }}"
    if (!writeJsonKV_P(w, kValTpl, kValTplVal)) return false;
    if (!writeJsonComma(w)) return false;

    // origin block
    if (!writeOriginBlock(w, ctx)) return false;

    return writeJsonClose(w);
  };

  // Measure pass
  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  // Write pass
  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;

  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// expandAndStreamSensorSources()
// Expands a source-template sensor into per-source variants and streams each
// via streamSensorDiscovery(). For 0x07-flagged sensors, two variants are
// emitted: _thermostat and _boiler.
//
// Per ADR-096 (worldview semantics) and ADR-097 (sibling-suffix shape) the
// two entities map as follows:
//   _thermostat — what the thermostat sees: the value it sent (T) or
//                 received (A under answer-override, B under pass-through).
//   _boiler     — what the boiler sees: the value it received (R under
//                 write-override, T under pass-through) or sent (B).
//
// The canonical entity (boiler-side worldview, identical to _boiler when both
// are published) is emitted by the SEPARATE base entry in mqttHaSensors[],
// not by this function. ADR-097 dropped ADR-095's mutual-exclusion rule, so
// the base entity is always advertised; this expansion adds two source
// variants on top when bSeparateSources=true.
//
// There is no _gateway variant; override visibility comes from divergence
// between _thermostat and _boiler. Routing is decided at publish time inside
// publishToSourceTopic() in MQTTstuff.ino.
// Returns true if at least one variant was successfully published.
// Lives here (not in MQTTstuff.ino) to avoid Arduino auto-prototyper
// mangling custom-type parameters.
// ---------------------------------------------------------------------------
bool expandAndStreamSensorSources(PubSubClient &client,
                                  const MqttHaSensorCfg &cfg,
                                  HaDiscoveryContext &ctx)
{
  static const char src_suffix_thermostat[] PROGMEM = "_thermostat";
  static const char src_suffix_boiler[]     PROGMEM = "_boiler";
  static const char src_name_thermostat[]   PROGMEM = "Thermostat";
  static const char src_name_boiler[]       PROGMEM = "Boiler";
  static const char src_seg_thermostat[]    PROGMEM = "thermostat";
  static const char src_seg_boiler[]        PROGMEM = "boiler";

  struct { PGM_P suffix; PGM_P name; PGM_P segment; } sources[] = {
    {src_suffix_thermostat, src_name_thermostat, src_seg_thermostat},
    {src_suffix_boiler,     src_name_boiler,     src_seg_boiler},
  };
  constexpr uint8_t kSourceVariantCount = sizeof(sources) / sizeof(sources[0]);

  bool published = false;
  char suffixBuf[16], nameBuf[16], segBuf[16];

  // Save original context values
  const char *origSuffix = ctx.sourceSuffix;
  const char *origName = ctx.sourceName;
  const char *origSeg = ctx.sourceTopicSegment;

  for (uint8_t i = 0; i < kSourceVariantCount; i++) {
    strncpy_P(suffixBuf, sources[i].suffix, sizeof(suffixBuf) - 1); suffixBuf[sizeof(suffixBuf)-1] = '\0';
    strncpy_P(nameBuf, sources[i].name, sizeof(nameBuf) - 1); nameBuf[sizeof(nameBuf)-1] = '\0';
    strncpy_P(segBuf, sources[i].segment, sizeof(segBuf) - 1); segBuf[sizeof(segBuf)-1] = '\0';

    ctx.sourceSuffix = suffixBuf;
    ctx.sourceName = nameBuf;
    ctx.sourceTopicSegment = segBuf;

    if (streamSensorDiscovery(client, cfg, ctx)) published = true;
    feedWatchDog();
  }

  // Restore original context
  ctx.sourceSuffix = origSuffix;
  ctx.sourceName = origName;
  ctx.sourceTopicSegment = origSeg;

  return published;
}

// ---------------------------------------------------------------------------
// Stubs for climate and number discovery (remain template-based for now)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Climate: Thermostat (climateIdx=0) and DHW Control (climateIdx=1)
// ---------------------------------------------------------------------------
bool streamClimateDiscovery(PubSubClient &client,
                            uint8_t climateIdx,
                            HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;
  if (climateIdx > 1) return false;

  // Topic
  char topic[STREAM_TOPIC_MAX];
  if (climateIdx == 0) {
    snprintf_P(topic, sizeof(topic), PSTR("%s/climate/%s/climate/config"), ctx.haPrefix, ctx.nodeId);
  } else {
    snprintf_P(topic, sizeof(topic), PSTR("%s/climate/%s/dhw_control/config"), ctx.haPrefix, ctx.nodeId);
  }

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    if (climateIdx == 0) {
      // === Thermostat ===
      if (!writeJsonKV_P(w, PSTR("action_template"), PSTR("{% if value == 'ON' %}heating{% else %}idle{% endif %}"))) return false;
      if (!writeJsonComma(w)) return false;

      // "action_topic":"<pub>/ch_enable"
      if (!w.writeProgmem(PSTR("\"action_topic\":\""))) return false;
      if (!w.writeRam(ctx.mqttPubTopic)) return false;
      if (!w.writeProgmem(PSTR("/ch_enable\""))) return false;
      if (!writeJsonComma(w)) return false;
    } else {
      // === DHW Control ===
      if (!writeJsonKV_P(w, PSTR("action_template"), PSTR("{% if value == 'ON' %}heating{% else %}idle{% endif %}"))) return false;
      if (!writeJsonComma(w)) return false;

      if (!w.writeProgmem(PSTR("\"action_topic\":\""))) return false;
      if (!w.writeRam(ctx.mqttPubTopic)) return false;
      if (!w.writeProgmem(PSTR("/domestichotwater\""))) return false;
      if (!writeJsonComma(w)) return false;
    }

    // avty_t
    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    // device block
    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // name + uniq_id (hostname dropped from name per Andre 2026-05-07; uniq_id keeps it)
    // TASK-648: modern mode inserts device suffix after nodeId in uniq_id.
    if (climateIdx == 0) {
      if (!w.writeProgmem(PSTR("\"name\":\"Thermostat\""))) return false;
      if (!writeJsonComma(w)) return false;
      if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
      if (!w.writeRam(ctx.nodeId)) return false;
      if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
      if (!w.writeProgmem(PSTR("-thermostat\""))) return false;
    } else {
      if (!w.writeProgmem(PSTR("\"name\":\"DHW Control\""))) return false;
      if (!writeJsonComma(w)) return false;
      if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
      if (!w.writeRam(ctx.nodeId)) return false;
      if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
      if (!w.writeProgmem(PSTR("-dhw_control\""))) return false;
    }
    if (!writeJsonComma(w)) return false;

    if (climateIdx == 1) {
      if (!w.writeProgmem(PSTR("\"optimistic\":true"))) return false;
      if (!writeJsonComma(w)) return false;
    }

    // modes
    if (climateIdx == 0) {
      if (!w.writeProgmem(PSTR("\"modes\":[\"off\",\"heat\"]"))) return false;
    } else {
      if (!w.writeProgmem(PSTR("\"modes\":[\"off\",\"auto\"]"))) return false;
    }
    if (!writeJsonComma(w)) return false;

    // mode_stat_t + mode_stat_tpl
    if (!w.writeProgmem(PSTR("\"mode_stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (climateIdx == 0) {
      // ADR-084: thermostat_connected lives under the generic value namespace
      // since 2.0.0; the otgw-pic/ prefix was retired together with the matching
      // bin-sensor flag flip in composeBinSensorPayload. Match that path here so
      // the Thermostat climate mode tracks the live publish.
      if (!w.writeProgmem(PSTR("/thermostat_connected\""))) return false;
      if (!writeJsonComma(w)) return false;
      if (!writeJsonKV_P(w, PSTR("mode_stat_tpl"), PSTR("{% if value == 'ON' %}heat{% else %}off{% endif %}"))) return false;
    } else {
      if (!w.writeProgmem(PSTR("/dhw_enable\""))) return false;
      if (!writeJsonComma(w)) return false;
      if (!writeJsonKV_P(w, PSTR("mode_stat_tpl"), PSTR("{% if value == 'ON' %}auto{% else %}off{% endif %}"))) return false;
    }
    if (!writeJsonComma(w)) return false;

    // curr_temp_t
    if (!w.writeProgmem(PSTR("\"curr_temp_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (climateIdx == 0) {
      if (!w.writeProgmem(PSTR("/Tr\""))) return false;
    } else {
      if (!w.writeProgmem(PSTR("/Tdhw\""))) return false;
    }
    if (!writeJsonComma(w)) return false;

    // temp_stat_t
    if (!w.writeProgmem(PSTR("\"temp_stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (climateIdx == 0) {
      if (!w.writeProgmem(PSTR("/TrSet\""))) return false;
    } else {
      if (!w.writeProgmem(PSTR("/TdhwSet\""))) return false;
    }
    if (!writeJsonComma(w)) return false;

    // temp_cmd_t + temp_cmd_tpl
    if (!w.writeProgmem(PSTR("\"temp_cmd_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttSubTopic)) return false;
    if (!w.writeProgmem(PSTR("/command\""))) return false;
    if (!writeJsonComma(w)) return false;
    if (climateIdx == 0) {
      if (!writeJsonKV_P(w, PSTR("temp_cmd_tpl"), PSTR("TT={{ value }}"))) return false;
    } else {
      if (!writeJsonKV_P(w, PSTR("temp_cmd_tpl"), PSTR("SW={{ value }}"))) return false;
    }
    if (!writeJsonComma(w)) return false;

    // temp bounds + settings
    if (climateIdx == 0) {
      if (!w.writeProgmem(PSTR("\"initial\":\"20\",\"min_temp\":\"12\",\"max_temp\":\"28\",\"temp_step\":\"0.5\",\"precision\":0.1"))) return false;
    } else {
      if (!w.writeProgmem(PSTR("\"min_temp\":\"40\",\"max_temp\":\"60\",\"temp_step\":\"1\",\"precision\":1"))) return false;
    }
    if (!writeJsonComma(w)) return false;

    if (!writeJsonKV_P(w, PSTR("temp_unit"), PSTR("C"))) return false;

    if (climateIdx == 0) {
      if (!writeJsonComma(w)) return false;
      if (!w.writeProgmem(PSTR("\"payload_off\":0,\"payload_on\":1"))) return false;
    }

    // icon
    if (!writeJsonComma(w)) return false;
    if (climateIdx == 0) {
      if (!writeJsonKV_P(w, kIcon, PSTR("mdi:radiator"))) return false;
    } else {
      if (!writeJsonKV_P(w, kIcon, PSTR("mdi:water-boiler"))) return false;
    }

    // json_attributes_topic: SAT PID/curve attributes on the thermostat entity (TASK-594)
    if (climateIdx == 0) {
      if (!writeJsonComma(w)) return false;
      if (!w.writeChar('"')) return false;
      if (!w.writeProgmem(kJsonAttrTopic)) return false;
      if (!w.writeProgmem(PSTR("\":\""))) return false;
      if (!w.writeRam(ctx.mqttPubTopic)) return false;
      if (!w.writeProgmem(PSTR("/sat/climate_attributes\""))) return false;
    }

    // origin
    if (!writeJsonComma(w)) return false;
    if (!writeOriginBlock(w, ctx)) return false;

    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;
  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// Number: Outside Temperature Override (OT ID 27)
// ---------------------------------------------------------------------------
bool streamNumberDiscovery(PubSubClient &client,
                           HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  char topic[STREAM_TOPIC_MAX];
  snprintf_P(topic, sizeof(topic), PSTR("%s/number/%s/Toutside_override/config"),
             ctx.haPrefix, ctx.nodeId);

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    // avty_t
    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    // device block
    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // uniq_id: modern inserts device suffix, legacy is bare nodeId
    if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeProgmem(PSTR("-Toutside_override\""))) return false;
    if (!writeJsonComma(w)) return false;

    // device_class + name
    if (!writeJsonKV_P(w, kDevCls, PSTR("temperature"))) return false;
    if (!writeJsonComma(w)) return false;
    // hostname dropped from name (Andre 2026-05-07)
    if (!w.writeProgmem(PSTR("\"name\":\"Outside Temperature Override\""))) return false;
    if (!writeJsonComma(w)) return false;

    // cmd_t
    if (!w.writeProgmem(PSTR("\"cmd_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttSubTopic)) return false;
    if (!w.writeProgmem(PSTR("/outside\""))) return false;
    if (!writeJsonComma(w)) return false;

    // stat_t — ADR-118: retargeted from canonical /Toutside to the override state
    // topic so the number entity reflects the user-injected value (the canonical
    // /Toutside stays Data-Invalid/0 when the boiler ignores the override).
    if (!w.writeProgmem(PSTR("\"stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeProgmem(PSTR("/Toutside/override\""))) return false;
    if (!writeJsonComma(w)) return false;

    // unit, min, max, step, mode
    if (!w.writeProgmem(PSTR("\"unit_of_measurement\":\"\xC2\xB0""C\",\"min\":-40,\"max\":50,\"step\":0.5,\"mode\":\"box\""))) return false;

    // icon
    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV_P(w, kIcon, PSTR("mdi:thermometer"))) return false;

    // origin
    if (!writeJsonComma(w)) return false;
    if (!writeOriginBlock(w, ctx)) return false;

    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;
  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// Sensor: active gateway override (ADR-118).
// Emits a read-only HA sensor whose stat_t points at <pub>/<label>/override for
// any override-capable msg id EXCEPT 27 (Toutside is already covered by the
// retargeted Toutside_override NUMBER entity above; emitting a sensor for 27 too
// would create two HA entities on the same topic). The override store is the data
// source; this is additive and does not touch any canonical entity.
// ---------------------------------------------------------------------------
bool streamOverrideSensorDiscovery(PubSubClient &client,
                                   HaDiscoveryContext &ctx,
                                   uint8_t otid,
                                   const char* label)
{
  if (otid == 27) return false;  // covered by the Toutside_override number entity
  if (!label || !*label) return false;
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  char topic[STREAM_TOPIC_MAX];
  snprintf_P(topic, sizeof(topic), PSTR("%s/sensor/%s/%s_override/config"),
             ctx.haPrefix, ctx.nodeId, label);

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    // avty_t
    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    // device block
    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // uniq_id "<nodeId>[-<device>]-<label>_override"
    // Modern: device suffix inserted after nodeId. Legacy: bare nodeId + label.
    if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeProgmem(PSTR("-"))) return false;
    if (!w.writeRam(label)) return false;
    if (!w.writeProgmem(PSTR("_override\""))) return false;
    if (!writeJsonComma(w)) return false;

    // device_class temperature + name "<label> Override"
    if (!writeJsonKV_P(w, kDevCls, PSTR("temperature"))) return false;
    if (!writeJsonComma(w)) return false;
    if (!w.writeProgmem(PSTR("\"name\":\""))) return false;
    if (!w.writeRam(label)) return false;
    if (!w.writeProgmem(PSTR(" Override\""))) return false;
    if (!writeJsonComma(w)) return false;

    // stat_t "<pub>/<label>/override"
    if (!w.writeProgmem(PSTR("\"stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeProgmem(PSTR("/"))) return false;
    if (!w.writeRam(label)) return false;
    if (!w.writeProgmem(PSTR("/override\""))) return false;
    if (!writeJsonComma(w)) return false;

    // unit + icon
    if (!w.writeProgmem(PSTR("\"unit_of_measurement\":\"\xC2\xB0""C\""))) return false;
    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV_P(w, kIcon, PSTR("mdi:thermometer-alert"))) return false;

    // origin
    if (!writeJsonComma(w)) return false;
    if (!writeOriginBlock(w, ctx)) return false;

    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();
    return false;
  }

  if (!client.endPublish()) return false;
  incPublishedTopicCount();
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// SAT switch (boolean) and select discovery (TASK-284).
// Matches the switch/select entries at the bottom of the archived 2.0.0
// mqttha.cfg. Hardcoded per index (like climate/number) to avoid PROGMEM
// arrays for a small fixed set. The 13 switches share the same JSON shape,
// so they route through a single streamSatBoolSwitch() helper.
// ---------------------------------------------------------------------------

static bool streamSatBoolSwitch(PubSubClient &client,
                                HaDiscoveryContext &ctx,
                                PGM_P uniqSuffix,
                                PGM_P nameSuffix,
                                PGM_P cmdSub,
                                PGM_P statSub,
                                PGM_P icon)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  // Derive topic object-id from uniqSuffix: strip leading dash, swap '-' to '_'.
  // e.g. "-sat-solar-gain-enable" -> "sat_solar_gain_enable".
  char objectId[48];
  {
    PGM_P p = uniqSuffix;
    if (pgm_read_byte(p) == '-') p++;
    size_t i = 0;
    while (i < sizeof(objectId) - 1) {
      char c = (char)pgm_read_byte(p + i);
      if (c == '\0') break;
      objectId[i] = (c == '-') ? '_' : c;
      i++;
    }
    objectId[i] = '\0';
  }

  char topic[STREAM_TOPIC_MAX];
  snprintf_P(topic, sizeof(topic), PSTR("%s/switch/%s/%s/config"),
             ctx.haPrefix, ctx.nodeId, objectId);

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // uniq_id: modern inserts device suffix between nodeId and the entity suffix.
    if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeProgmem(uniqSuffix)) return false;
    if (!w.writeProgmem(PSTR("\""))) return false;
    if (!writeJsonComma(w)) return false;

    // hostname dropped from name (Andre 2026-05-07). nameSuffix is PROGMEM and
    // typically starts with '_'; copy to a small RAM buffer with leading '_' stripped
    // so writeFriendlyName can apply '_' -> ' ' + Title Case uniformly.
    if (!w.writeProgmem(PSTR("\"name\":\""))) return false;
    {
      char nameBuf[48];
      strlcpy_P(nameBuf, nameSuffix, sizeof(nameBuf));
      const char *src = nameBuf[0] == '_' ? &nameBuf[1] : nameBuf;
      if (!writeFriendlyName(w, src)) return false;
    }
    if (!w.writeProgmem(PSTR("\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"cmd_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttSubTopic)) return false;
    if (!w.writeProgmem(cmdSub)) return false;
    if (!w.writeProgmem(PSTR("\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeProgmem(statSub)) return false;
    if (!w.writeProgmem(PSTR("\""))) return false;
    if (!writeJsonComma(w)) return false;

    // Boolean payload conventions from mqttha.cfg 2.0.0 archive
    if (!w.writeProgmem(PSTR("\"pl_on\":\"true\",\"pl_off\":\"false\",\"stat_on\":\"true\",\"stat_off\":\"false\""))) return false;

    if (!writeJsonComma(w)) return false;
    if (!writeJsonKV_P(w, kIcon, icon)) return false;

    if (!writeJsonComma(w)) return false;
    if (!writeOriginBlock(w, ctx)) return false;

    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;
  feedWatchDog();
  return true;
}

bool streamSatSwitchDiscovery(PubSubClient &client,
                              uint8_t switchIdx,
                              HaDiscoveryContext &ctx)
{
  // uniqSuffix, nameSuffix, cmdSub, statSub, icon
  bool published = false;
  switch (switchIdx) {
    case 0:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-solar-gain-enable"),       PSTR("_SAT_Solar_Gain"),
        PSTR("/sat/solar_gain"),              PSTR("/sat/solar_gain_enable"),
        PSTR("mdi:white-balance-sunny"));
      break;
    case 1:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-summer-simmer-enable"),    PSTR("_SAT_Summer_Simmer"),
        PSTR("/sat/summer_simmer"),           PSTR("/sat/summer_simmer_enable"),
        PSTR("mdi:weather-sunny-alert"));
      break;
    case 2:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-comfort-adjust-enable"),   PSTR("_SAT_Comfort_Adjust"),
        PSTR("/sat/comfort_adjust"),          PSTR("/sat/comfort_adjust_enable"),
        PSTR("mdi:water-thermometer"));
      break;
    case 3:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-multi-area-enable"),       PSTR("_SAT_Multi_Area"),
        PSTR("/sat/multi_area"),              PSTR("/sat/multi_area_enable"),
        PSTR("mdi:home-group"));
      break;
    case 4:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-auto-tune-enable"),        PSTR("_SAT_Auto_Tune"),
        PSTR("/sat/auto_tune"),               PSTR("/sat/auto_tune_enable"),
        PSTR("mdi:auto-fix"));
      break;
    case 5:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-simulation-enable"),       PSTR("_SAT_Simulation"),
        PSTR("/sat/simulation"),              PSTR("/sat/simulation_enable"),
        PSTR("mdi:flask"));
      break;
    case 6:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-window-detection-enable"), PSTR("_SAT_Window_Detection"),
        PSTR("/sat/window_detection"),        PSTR("/sat/window_detection_enable"),
        PSTR("mdi:window-open-variant"));
      break;
    case 7:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-force-pwm-enable"),        PSTR("_SAT_Force_PWM"),
        PSTR("/sat/force_pwm"),               PSTR("/sat/force_pwm_enable"),
        PSTR("mdi:pulse"));
      break;
    case 8:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-push-setpoint-enable"),    PSTR("_SAT_Push_Setpoint"),
        PSTR("/sat/push_setpoint"),           PSTR("/sat/push_setpoint_enable"),
        PSTR("mdi:upload"));
      break;
    case 9:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-ovp-enabled"),             PSTR("_SAT_OVP_Enabled"),
        PSTR("/sat/ovp_enabled"),             PSTR("/sat/ovp_enabled"),
        PSTR("mdi:shield-check"));
      break;
    case 10:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-preset-sync-enable"),      PSTR("_SAT_Preset_Sync"),
        PSTR("/sat/preset_sync"),             PSTR("/sat/preset_sync_enable"),
        PSTR("mdi:sync"));
      break;
    case 11:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-dhw-enabled"),             PSTR("_SAT_DHW_Enabled"),
        PSTR("/sat/dhw_enabled"),             PSTR("/sat/dhw_enabled"),
        PSTR("mdi:water-boiler"));
      break;
    case 12:
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-pwm-auto-switch-enable"),  PSTR("_SAT_PWM_Auto_Switch"),
        PSTR("/sat/pwm_auto_switch"),         PSTR("/sat/pwm_auto_switch_enable"),
        PSTR("mdi:swap-horizontal"));
      break;
    case 13:
      // TASK-516: master DHW enable (HW= command). Only emitted by the caller
      // when MsgID 3 HB3=1 (storage tank); combi boilers get no inert entity.
      published = streamSatBoolSwitch(client, ctx,
        PSTR("-sat-dhw-enable"),              PSTR("_SAT_DHW_Enable"),
        PSTR("/sat/dhw_enable"),              PSTR("/sat/dhw_enable"),
        PSTR("mdi:water-boiler"));
      break;
    default:
      return false;
  }
  if (published) incPublishedTopicCount();   // ADR-062 / TASK-349
  return published;
}

bool streamSatSelectDiscovery(PubSubClient &client,
                              uint8_t selectIdx,
                              HaDiscoveryContext &ctx)
{
  if (selectIdx != 0) return false;
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  char topic[STREAM_TOPIC_MAX];
  snprintf_P(topic, sizeof(topic), PSTR("%s/select/%s/sat_heating_system/config"),
             ctx.haPrefix, ctx.nodeId);

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // uniq_id: modern inserts device suffix after nodeId.
    if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeProgmem(PSTR("-sat-heating-system\""))) return false;
    if (!writeJsonComma(w)) return false;

    // hostname dropped from name (Andre 2026-05-07)
    if (!w.writeProgmem(PSTR("\"name\":\"SAT Heating System\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"cmd_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttSubTopic)) return false;
    if (!w.writeProgmem(PSTR("/sat/heating_system\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeProgmem(PSTR("/sat/heating_system\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"options\":[\"0\",\"1\",\"2\",\"3\"]"))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeJsonKV_P(w, kIcon, PSTR("mdi:radiator"))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeOriginBlock(w, ctx)) return false;
    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;
  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// ---------------------------------------------------------------------------
// PIC control entities (pseudo-ID OTGWpiccontrolsid = 244) — port of dev PR#576.
//
// Published unconditionally, like the other PIC pseudo-IDs (249/250) on this
// dual-target branch (TASK-543 gating decision in OTGW-firmware.h): the entity
// is always discovered; the set-commands and the otgw-pic/ state topics are
// PIC-gated at their source. Gating discovery on isPICEnabled() would make
// doAutoConfigureMsgid() return false on PIC-less devices and spin the
// discovery drip forever (the bug fixed on dev in PR #596).
// ---------------------------------------------------------------------------

// resetgateway -> HA button. No stat_t; cmd_t + payload_press only.
bool streamButtonDiscovery(PubSubClient &client, HaDiscoveryContext &ctx)
{
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  char topic[STREAM_TOPIC_MAX];
  snprintf_P(topic, sizeof(topic), PSTR("%s/button/%s/resetgateway/config"),
             ctx.haPrefix, ctx.nodeId);

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // uniq_id: modern inserts device suffix after nodeId.
    if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeProgmem(PSTR("-resetgateway\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"name\":\"Reset Gateway\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"cmd_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttSubTopic)) return false;
    if (!w.writeProgmem(PSTR("/resetgateway\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"payload_press\":\"1\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeJsonKV_P(w, kEntCat, PSTR("config"))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeJsonKV_P(w, kIcon, PSTR("mdi:restart"))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeOriginBlock(w, ctx)) return false;
    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;
  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}

// GPIO/LED function selects.
// selectIdx: 0=gpioa 1=gpiob 2=leda 3=ledb 4=ledc 5=ledd 6=lede 7=ledf
// State topics published by the existing PIC PR= polling:
//   GPIO: <mqttPubTopic>/otgw-pic/settings/gpio (2-char string, e.g. "05")
//   LED:  <mqttPubTopic>/otgw-pic/settings/led  (6-char string, e.g. "RFFTTT")
static const char kSelLabel0[] PROGMEM = "gpioa";
static const char kSelLabel1[] PROGMEM = "gpiob";
static const char kSelLabel2[] PROGMEM = "leda";
static const char kSelLabel3[] PROGMEM = "ledb";
static const char kSelLabel4[] PROGMEM = "ledc";
static const char kSelLabel5[] PROGMEM = "ledd";
static const char kSelLabel6[] PROGMEM = "lede";
static const char kSelLabel7[] PROGMEM = "ledf";
static const char* const kSelLabels[] PROGMEM = {
    kSelLabel0, kSelLabel1, kSelLabel2, kSelLabel3,
    kSelLabel4, kSelLabel5, kSelLabel6, kSelLabel7
};
static const char kSelName0[] PROGMEM = "GPIO A Function";
static const char kSelName1[] PROGMEM = "GPIO B Function";
static const char kSelName2[] PROGMEM = "LED A Function";
static const char kSelName3[] PROGMEM = "LED B Function";
static const char kSelName4[] PROGMEM = "LED C Function";
static const char kSelName5[] PROGMEM = "LED D Function";
static const char kSelName6[] PROGMEM = "LED E Function";
static const char kSelName7[] PROGMEM = "LED F Function";
static const char* const kSelNames[] PROGMEM = {
    kSelName0, kSelName1, kSelName2, kSelName3,
    kSelName4, kSelName5, kSelName6, kSelName7
};
static const char kSelGpioOptions[] PROGMEM = "[\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\"]";
static const char kSelLedOptions[]  PROGMEM = "[\"B\",\"C\",\"E\",\"F\",\"H\",\"M\",\"O\",\"P\",\"R\",\"T\",\"W\",\"X\"]";

bool streamSelectDiscovery(PubSubClient &client, uint8_t selectIdx, HaDiscoveryContext &ctx)
{
  if (selectIdx > 7) return false;
  if (!client.connected()) return false;
  if (!canPublishMQTT()) return false;
  if (platformFreeHeap() < STREAM_HEAP_MIN) return false;

  char label[12];
  char name[20];
  strlcpy_P(label, (PGM_P)pgm_read_ptr(&kSelLabels[selectIdx]), sizeof(label));
  strlcpy_P(name,  (PGM_P)pgm_read_ptr(&kSelNames[selectIdx]),  sizeof(name));

  bool    isGpio  = (selectIdx < 2);
  uint8_t charPos = isGpio ? selectIdx : (uint8_t)(selectIdx - 2);

  char topic[STREAM_TOPIC_MAX];
  snprintf_P(topic, sizeof(topic), PSTR("%s/select/%s/%s/config"),
             ctx.haPrefix, ctx.nodeId, label);

  auto compose = [&](MqttJsonWriter &w) -> bool {
    if (!writeJsonOpen(w)) return false;

    if (!writeJsonKV(w, kAvtyT, ctx.mqttPubTopic)) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeDeviceBlock(w, ctx)) return false;
    if (!writeJsonComma(w)) return false;

    // uniq_id: modern inserts device suffix after nodeId.
    if (!w.writeProgmem(PSTR("\"uniq_id\":\""))) return false;
    if (!w.writeRam(ctx.nodeId)) return false;
    if (!ctx.legacyMode && !w.writeProgmem(haDeviceSuffix(ctx.device))) return false;  // TASK-648: -<device>
    if (!w.writeChar('-')) return false;
    if (!w.writeRam(label)) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"name\":\""))) return false;
    if (!w.writeRam(name)) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    // stat_t: <mqttPubTopic>/otgw-pic/settings/{gpio,led}
    if (!w.writeProgmem(PSTR("\"stat_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttPubTopic)) return false;
    if (!w.writeChar('/')) return false;
    if (!w.writeProgmem(kPicSubtreePrefix)) return false;
    if (!w.writeProgmem(isGpio ? PSTR("settings/gpio\"") : PSTR("settings/led\""))) return false;
    if (!writeJsonComma(w)) return false;

    // value_template: {{ value[N] }}
    if (!w.writeProgmem(PSTR("\"value_template\":\"{{ value["))) return false;
    if (!w.writeChar((char)('0' + charPos))) return false;
    if (!w.writeProgmem(PSTR("] }}\""))) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"cmd_t\":\""))) return false;
    if (!w.writeRam(ctx.mqttSubTopic)) return false;
    if (!w.writeChar('/')) return false;
    if (!w.writeRam(label)) return false;
    if (!w.writeChar('"')) return false;
    if (!writeJsonComma(w)) return false;

    if (!w.writeProgmem(PSTR("\"options\":"))) return false;
    if (!w.writeProgmem(isGpio ? kSelGpioOptions : kSelLedOptions)) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeJsonKV_P(w, kEntCat, PSTR("config"))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeJsonKV_P(w, kIcon, isGpio ? PSTR("mdi:pin") : PSTR("mdi:led-outline"))) return false;
    if (!writeJsonComma(w)) return false;

    if (!writeOriginBlock(w, ctx)) return false;
    return writeJsonClose(w);
  };

  MqttJsonWriter measure(MqttJsonWriter::MEASURE);
  if (!compose(measure)) return false;

  if (!client.beginPublish(topic, measure.byteCount, true)) return false;

  MqttJsonWriter writer(MqttJsonWriter::WRITE);
  if (!compose(writer) || !writer.ok) {
    client.disconnect();  // desync: drop TCP instead of finalising a truncated payload (TASK-770)
    return false;
  }

  if (!client.endPublish()) return false;
  incPublishedTopicCount();   // ADR-062 / TASK-349
  feedWatchDog();
  return true;
}
