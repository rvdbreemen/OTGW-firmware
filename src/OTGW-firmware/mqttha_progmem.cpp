// AUTO-GENERATED - DO NOT EDIT.
// Run tools/generate_mqttha_progmem.py to regenerate.
// Source: src/OTGW-firmware/data/mqttha.cfg
// Generated: 2026-04-16T20:12:58Z
//
// Total entries : 345
// Unique OT IDs : 118
// Longest topic : 98 chars
// Longest msg   : 843 chars
//
// Compiled by Arduino as a separate translation unit.

#include <pgmspace.h>
#include <stdint.h>
#include "mqttha_progmem.h"

// Topic pool — 23,758 bytes
const char PROGMEM mqttHaTopicPool[] =
  "%homeassistant%/climate/%node_id%/climate/config\0%homeassistant%/climate/%node_id%/dhw_control/config\0%homeassistant%/"
  "binary_sensor/%node_id%/fault/config\0%homeassistant%/binary_sensor/%node_id%/centralheating/config\0%homeassistant%/bin"
  "ary_sensor/%node_id%/domestichotwater/config\0%homeassistant%/binary_sensor/%node_id%/flame/config\0%homeassistant%/bina"
  "ry_sensor/%node_id%/cooling/config\0%homeassistant%/binary_sensor/%node_id%/centralheating2/config\0%homeassistant%/bina"
  "ry_sensor/%node_id%/diagnostic_indicator/config\0%homeassistant%/binary_sensor/%node_id%/electric_production/config\0%ho"
  "meassistant%/binary_sensor/%node_id%/ch_enable/config\0%homeassistant%/binary_sensor/%node_id%/dhw_enable/config\0%homea"
  "ssistant%/binary_sensor/%node_id%/cooling_enable/config\0%homeassistant%/binary_sensor/%node_id%/otc_active/config\0%hom"
  "eassistant%/binary_sensor/%node_id%/ch2_enable/config\0%homeassistant%/binary_sensor/%node_id%/thermostat_connected/conf"
  "ig\0%homeassistant%/binary_sensor/%node_id%/boiler_connected/config\0%homeassistant%/sensor/%node_id%/status_master/conf"
  "ig\0%homeassistant%/sensor/%node_id%/status_slave/config\0%homeassistant%/binary_sensor/%node_id%/dhw_blocking/config\0%"
  "homeassistant%/binary_sensor/%node_id%/summerwintertime/config\0%homeassistant%/sensor/%node_id%/TSet/config\0%homeassis"
  "tant%/sensor/%node_id%/TSet/%source_topic_segment%/config\0%homeassistant%/binary_sensor/%node_id%/master_configuration_"
  "smart_power/config\0%homeassistant%/sensor/%node_id%/master_configuration/config\0%homeassistant%/sensor/%node_id%/maste"
  "r_memberid_code/config\0%homeassistant%/binary_sensor/%node_id%/dhw_present/config\0%homeassistant%/binary_sensor/%node_"
  "id%/control_type_modulation/config\0%homeassistant%/binary_sensor/%node_id%/cooling_config/config\0%homeassistant%/binar"
  "y_sensor/%node_id%/dhw_config/config\0%homeassistant%/binary_sensor/%node_id%/master_low_off_pump_control_function/confi"
  "g\0%homeassistant%/binary_sensor/%node_id%/ch2_present/config\0%homeassistant%/binary_sensor/%node_id%/remote_water_fill"
  "ing_function/config\0%homeassistant%/binary_sensor/%node_id%/heat_cool_mode_control/config\0%homeassistant%/sensor/%node"
  "_id%/slave_configuration/config\0%homeassistant%/sensor/%node_id%/slave_memberid_code/config\0%homeassistant%/sensor/%no"
  "de_id%/Command_hb_u8/config\0%homeassistant%/sensor/%node_id%/Command_lb_u8/config\0%homeassistant%/sensor/%node_id%/Com"
  "mand_remote_command/config\0%homeassistant%/binary_sensor/%node_id%/service_request/config\0%homeassistant%/binary_senso"
  "r/%node_id%/lockout_reset/config\0%homeassistant%/binary_sensor/%node_id%/low_water_pressure/config\0%homeassistant%/bin"
  "ary_sensor/%node_id%/gas_flame_fault/config\0%homeassistant%/binary_sensor/%node_id%/air_pressure_fault/config\0%homeass"
  "istant%/binary_sensor/%node_id%/water_over_temperature/config\0%homeassistant%/sensor/%node_id%/ASF_flags/config\0%homea"
  "ssistant%/sensor/%node_id%/OEMFaultCode/config\0%homeassistant%/binary_sensor/%node_id%/rbp_dhw_setpoint/config\0%homeas"
  "sistant%/binary_sensor/%node_id%/rbp_max_ch_setpoint/config\0%homeassistant%/binary_sensor/%node_id%/rbp_rw_dhw_setpoint"
  "/config\0%homeassistant%/binary_sensor/%node_id%/rbp_rw_max_ch_setpoint/config\0%homeassistant%/sensor/%node_id%/RBP_fla"
  "gs_read_write/config\0%homeassistant%/sensor/%node_id%/RBP_flags_transfer_enable/config\0%homeassistant%/sensor/%node_id"
  "%/CoolingControl/config\0%homeassistant%/sensor/%node_id%/CoolingControl/%source_topic_segment%/config\0%homeassistant%/"
  "sensor/%node_id%/TsetCH2/config\0%homeassistant%/sensor/%node_id%/TsetCH2/%source_topic_segment%/config\0%homeassistant%"
  "/sensor/%node_id%/TrOverride/config\0%homeassistant%/sensor/%node_id%/TrOverride/%source_topic_segment%/config\0%homeass"
  "istant%/sensor/%node_id%/TSP_hb_u8/config\0%homeassistant%/sensor/%node_id%/TSP_lb_u8/config\0%homeassistant%/sensor/%no"
  "de_id%/TSPindexTSPvalue_hb_u8/config\0%homeassistant%/sensor/%node_id%/TSPindexTSPvalue_lb_u8/config\0%homeassistant%/se"
  "nsor/%node_id%/FHBsize_hb_u8/config\0%homeassistant%/sensor/%node_id%/FHBsize_lb_u8/config\0%homeassistant%/sensor/%node"
  "_id%/FHBindexFHBvalue_hb_u8/config\0%homeassistant%/sensor/%node_id%/FHBindexFHBvalue_lb_u8/config\0%homeassistant%/sens"
  "or/%node_id%/MaxRelModLevelSetting/config\0%homeassistant%/sensor/%node_id%/MaxRelModLevelSetting/%source_topic_segment%"
  "/config\0%homeassistant%/sensor/%node_id%/MaxCapacityMinModLevel_lb_u8/config\0%homeassistant%/sensor/%node_id%/MaxCapac"
  "ityMinModLevel_hb_u8/config\0%homeassistant%/sensor/%node_id%/TrSet/config\0%homeassistant%/sensor/%node_id%/TrSet/%sour"
  "ce_topic_segment%/config\0%homeassistant%/sensor/%node_id%/RelModLevel/config\0%homeassistant%/sensor/%node_id%/RelModLe"
  "vel/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/CHPressure/config\0%homeassistant%/sensor/%node_id%/"
  "CHPressure/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/DHWFlowRate/config\0%homeassistant%/sensor/%n"
  "ode_id%/DHWFlowRate/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/DayTime_dayofweek/config\0%homeassis"
  "tant%/sensor/%node_id%/DayTime_hour/config\0%homeassistant%/sensor/%node_id%/DayTime_minutes/config\0%homeassistant%/sen"
  "sor/%node_id%/Date_day_of_month/config\0%homeassistant%/sensor/%node_id%/Date_month/config\0%homeassistant%/sensor/%node"
  "_id%/Year/config\0%homeassistant%/sensor/%node_id%/Year/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/"
  "TrSetCH2/config\0%homeassistant%/sensor/%node_id%/TrSetCH2/%source_topic_segment%/config\0%homeassistant%/sensor/%node_i"
  "d%/Troom/config\0%homeassistant%/sensor/%node_id%/Tr/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Tbo"
  "iler/config\0%homeassistant%/sensor/%node_id%/Tboiler/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Td"
  "hw/config\0%homeassistant%/sensor/%node_id%/Tdhw/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Toutsid"
  "e/config\0%homeassistant%/number/%node_id%/Toutside_override/config\0%homeassistant%/sensor/%node_id%/Toutside/%source_t"
  "opic_segment%/config\0%homeassistant%/sensor/%node_id%/Tret/config\0%homeassistant%/sensor/%node_id%/Tret/%source_topic_"
  "segment%/config\0%homeassistant%/sensor/%node_id%/Tsolarstorage/config\0%homeassistant%/sensor/%node_id%/Tsolarstorage/%"
  "source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Tsolarcollector/config\0%homeassistant%/sensor/%node_id%/"
  "Tsolarcollector/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/TflowCH2/config\0%homeassistant%/sensor/"
  "%node_id%/TflowCH2/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Tdhw2/config\0%homeassistant%/sensor/"
  "%node_id%/Tdhw2/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Texhaust/config\0%homeassistant%/sensor/"
  "%node_id%/Texhaust/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Theatexchanger/config\0%homeassistant"
  "%/sensor/%node_id%/Theatexchanger/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/FanSpeed_setpoint_hz/c"
  "onfig\0%homeassistant%/sensor/%node_id%/FanSpeed_actual_hz/config\0%homeassistant%/sensor/%node_id%/ElectricalCurrentBur"
  "nerFlame/config\0%homeassistant%/sensor/%node_id%/ElectricalCurrentBurnerFlame/%source_topic_segment%/config\0%homeassis"
  "tant%/sensor/%node_id%/TRoomCH2/config\0%homeassistant%/sensor/%node_id%/TRoomCH2/%source_topic_segment%/config\0%homeas"
  "sistant%/sensor/%node_id%/RelativeHumidity/config\0%homeassistant%/sensor/%node_id%/RelativeHumidity/%source_topic_segme"
  "nt%/config\0%homeassistant%/sensor/%node_id%/TrOverride2/config\0%homeassistant%/sensor/%node_id%/TrOverride2/%source_to"
  "pic_segment%/config\0%homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_value_lb/config\0%homeassistant%/sensor/%node_i"
  "d%/TdhwSetUBTdhwSetLB_value_hb/config\0%homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_value_hb/%source_topic_segmen"
  "t%/config\0%homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_value_lb/%source_topic_segment%/config\0%homeassistant%/s"
  "ensor/%node_id%/MaxTSetUBMaxTSetLB_value_lb/config\0%homeassistant%/sensor/%node_id%/MaxTSetUBMaxTSetLB_value_hb/config"
  "\0%homeassistant%/sensor/%node_id%/MaxTSetUBMaxTSetLB_value_hb/%source_topic_segment%/config\0%homeassistant%/sensor/%no"
  "de_id%/MaxTSetUBMaxTSetLB_value_lb/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/HcratioUBHcratioLB_va"
  "lue_lb/config\0%homeassistant%/sensor/%node_id%/HcratioUBHcratioLB_value_hb/config\0%homeassistant%/sensor/%node_id%/Rem"
  "oteparameter4boundaries_value_hb/config\0%homeassistant%/sensor/%node_id%/Remoteparameter4boundaries_value_hb/%source_to"
  "pic_segment%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter4boundaries_value_lb/config\0%homeassistant%/sensor"
  "/%node_id%/Remoteparameter4boundaries_value_lb/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remotepar"
  "ameter5boundaries_value_hb/config\0%homeassistant%/sensor/%node_id%/Remoteparameter5boundaries_value_hb/%source_topic_se"
  "gment%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter5boundaries_value_lb/config\0%homeassistant%/sensor/%node"
  "_id%/Remoteparameter5boundaries_value_lb/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter"
  "6boundaries_value_hb/config\0%homeassistant%/sensor/%node_id%/Remoteparameter6boundaries_value_hb/%source_topic_segment%"
  "/config\0%homeassistant%/sensor/%node_id%/Remoteparameter6boundaries_value_lb/config\0%homeassistant%/sensor/%node_id%/R"
  "emoteparameter6boundaries_value_lb/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter7bound"
  "aries_value_hb/config\0%homeassistant%/sensor/%node_id%/Remoteparameter7boundaries_value_hb/%source_topic_segment%/confi"
  "g\0%homeassistant%/sensor/%node_id%/Remoteparameter7boundaries_value_lb/config\0%homeassistant%/sensor/%node_id%/Remotep"
  "arameter7boundaries_value_lb/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter8boundaries_"
  "value_hb/config\0%homeassistant%/sensor/%node_id%/Remoteparameter8boundaries_value_hb/%source_topic_segment%/config\0%ho"
  "meassistant%/sensor/%node_id%/Remoteparameter8boundaries_value_lb/config\0%homeassistant%/sensor/%node_id%/Remoteparamet"
  "er8boundaries_value_lb/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/TdhwSet/config\0%homeassistant%/s"
  "ensor/%node_id%/TdhwSet/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/MaxTSet/config\0%homeassistant%/"
  "sensor/%node_id%/MaxTSet/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Hcratio/config\0%homeassistant%"
  "/sensor/%node_id%/Hcratio/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter4/config\0%home"
  "assistant%/sensor/%node_id%/Remoteparameter4/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remoteparam"
  "eter5/config\0%homeassistant%/sensor/%node_id%/Remoteparameter5/%source_topic_segment%/config\0%homeassistant%/sensor/%n"
  "ode_id%/Remoteparameter6/config\0%homeassistant%/sensor/%node_id%/Remoteparameter6/%source_topic_segment%/config\0%homea"
  "ssistant%/sensor/%node_id%/Remoteparameter7/config\0%homeassistant%/sensor/%node_id%/Remoteparameter7/%source_topic_segm"
  "ent%/config\0%homeassistant%/sensor/%node_id%/Remoteparameter8/config\0%homeassistant%/sensor/%node_id%/Remoteparameter8"
  "/%source_topic_segment%/config\0%homeassistant%/binary_sensor/%node_id%/vh_ventilation_enabled/config\0%homeassistant%/b"
  "inary_sensor/%node_id%/vh_bypass_position/config\0%homeassistant%/binary_sensor/%node_id%/vh_bypass_mode/config\0%homeas"
  "sistant%/binary_sensor/%node_id%/vh_free_ventilation_mode/config\0%homeassistant%/binary_sensor/%node_id%/vh_fault/confi"
  "g\0%homeassistant%/binary_sensor/%node_id%/vh_ventilation_mode/config\0%homeassistant%/binary_sensor/%node_id%/vh_bypass"
  "_status/config\0%homeassistant%/binary_sensor/%node_id%/vh_bypass_automatic_status/config\0%homeassistant%/binary_sensor"
  "/%node_id%/vh_free_ventliation_status/config\0%homeassistant%/binary_sensor/%node_id%/vh_diagnostic_indicator/config\0%h"
  "omeassistant%/sensor/%node_id%/status_vh_master/config\0%homeassistant%/sensor/%node_id%/status_vh_slave/config\0%homeas"
  "sistant%/sensor/%node_id%/ControlSetpointVH/config\0%homeassistant%/sensor/%node_id%/ControlSetpointVH/%source_topic_seg"
  "ment%/config\0%homeassistant%/sensor/%node_id%/ControlSetpointVH_hb_u8/config\0%homeassistant%/sensor/%node_id%/ControlS"
  "etpointVH_lb_u8/config\0%homeassistant%/sensor/%node_id%/ControlSetpointVH_hb_u8/%source_topic_segment%/config\0%homeass"
  "istant%/sensor/%node_id%/ControlSetpointVH_lb_u8/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/ASFFaul"
  "tCodeVH_code/config\0%homeassistant%/sensor/%node_id%/ASFFaultCodeVH_flag8/config\0%homeassistant%/sensor/%node_id%/Diag"
  "nosticCodeVH/config\0%homeassistant%/sensor/%node_id%/DiagnosticCodeVH/%source_topic_segment%/config\0%homeassistant%/bi"
  "nary_sensor/%node_id%/vh_configuration_system_type/config\0%homeassistant%/binary_sensor/%node_id%/vh_configuration_bypa"
  "ss/config\0%homeassistant%/binary_sensor/%node_id%/vh_configuration_speed_control/config\0%homeassistant%/sensor/%node_i"
  "d%/vh_configuration/config\0%homeassistant%/sensor/%node_id%/vh_memberid_code/config\0%homeassistant%/sensor/%node_id%/O"
  "penthermVersionVH/config\0%homeassistant%/sensor/%node_id%/OpenthermVersionVH/%source_topic_segment%/config\0%homeassist"
  "ant%/sensor/%node_id%/VersionTypeVH_hb_u8/config\0%homeassistant%/sensor/%node_id%/VersionTypeVH_lb_u8/config\0%homeassi"
  "stant%/sensor/%node_id%/RelativeVentilation/config\0%homeassistant%/sensor/%node_id%/RelativeVentilation_hb_u8/config\0%"
  "homeassistant%/sensor/%node_id%/RelativeVentilation_lb_u8/config\0%homeassistant%/sensor/%node_id%/RelativeVentilation/%"
  "source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/RelativeVentilation_hb_u8/%source_topic_segment%/config\0"
  "%homeassistant%/sensor/%node_id%/RelativeVentilation_lb_u8/%source_topic_segment%/config\0%homeassistant%/sensor/%node_i"
  "d%/RelativeHumidityExhaustAir/config\0%homeassistant%/sensor/%node_id%/RelativeHumidityExhaustAir_hb_u8/config\0%homeass"
  "istant%/sensor/%node_id%/RelativeHumidityExhaustAir_lb_u8/config\0%homeassistant%/sensor/%node_id%/RelativeHumidityExhau"
  "stAir/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/RelativeHumidityExhaustAir_hb_u8/%source_topic_seg"
  "ment%/config\0%homeassistant%/sensor/%node_id%/RelativeHumidityExhaustAir_lb_u8/%source_topic_segment%/config\0%homeassi"
  "stant%/sensor/%node_id%/CO2LevelExhaustAir/config\0%homeassistant%/sensor/%node_id%/CO2LevelExhaustAir/%source_topic_seg"
  "ment%/config\0%homeassistant%/sensor/%node_id%/SupplyInletTemperature/config\0%homeassistant%/sensor/%node_id%/SupplyInl"
  "etTemperature/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/SupplyOutletTemperature/config\0%homeassis"
  "tant%/sensor/%node_id%/SupplyOutletTemperature/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/ExhaustIn"
  "letTemperature/config\0%homeassistant%/sensor/%node_id%/ExhaustInletTemperature/%source_topic_segment%/config\0%homeassi"
  "stant%/sensor/%node_id%/ExhaustOutletTemperature/config\0%homeassistant%/sensor/%node_id%/ExhaustOutletTemperature/%sour"
  "ce_topic_segment%/config\0%homeassistant%/sensor/%node_id%/ActualExhaustFanSpeed/config\0%homeassistant%/sensor/%node_id"
  "%/ActualExhaustFanSpeed/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/ActualSupplyFanSpeed/config\0%ho"
  "meassistant%/sensor/%node_id%/ActualSupplyFanSpeed/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/Remot"
  "eParameterSettingVH_hb_flag8/config\0%homeassistant%/sensor/%node_id%/RemoteParameterSettingVH_lb_flag8/config\0%homeass"
  "istant%/sensor/%node_id%/vh_rw_nominal_ventilation_value/config\0%homeassistant%/sensor/%node_id%/vh_transfer_enable_nom"
  "inal_ventilation_value/config\0%homeassistant%/sensor/%node_id%/NominalVentilationValue/config\0%homeassistant%/sensor/%"
  "node_id%/NominalVentilationValue_hb_u8/config\0%homeassistant%/sensor/%node_id%/NominalVentilationValue_lb_u8/config\0%h"
  "omeassistant%/sensor/%node_id%/NominalVentilationValue/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/N"
  "ominalVentilationValue_hb_u8/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/NominalVentilationValue_lb_"
  "u8/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/TSPNumberVH_hb_u8/config\0%homeassistant%/sensor/%nod"
  "e_id%/TSPNumberVH_lb_u8/config\0%homeassistant%/sensor/%node_id%/TSPEntryVH_hb_u8/config\0%homeassistant%/sensor/%node_i"
  "d%/TSPEntryVH_lb_u8/config\0%homeassistant%/sensor/%node_id%/FaultBufferSizeVH_hb_u8/config\0%homeassistant%/sensor/%nod"
  "e_id%/FaultBufferSizeVH_lb_u8/config\0%homeassistant%/sensor/%node_id%/FaultBufferEntryVH_hb_u8/config\0%homeassistant%/"
  "sensor/%node_id%/FaultBufferEntryVH_lb_u8/config\0%homeassistant%/sensor/%node_id%/Brand_hb_u8/config\0%homeassistant%/s"
  "ensor/%node_id%/Brand_lb_u8/config\0%homeassistant%/sensor/%node_id%/BrandVersion_hb_u8/config\0%homeassistant%/sensor/%"
  "node_id%/BrandVersion_lb_u8/config\0%homeassistant%/sensor/%node_id%/BrandSerialNumber_hb_u8/config\0%homeassistant%/sen"
  "sor/%node_id%/BrandSerialNumber_lb_u8/config\0%homeassistant%/sensor/%node_id%/CoolingOperationHours/config\0%homeassist"
  "ant%/sensor/%node_id%/CoolingOperationHours/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/PowerCycles/"
  "config\0%homeassistant%/sensor/%node_id%/PowerCycles/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/RFS"
  "ensorStatusInformation_battery_indication/config\0%homeassistant%/sensor/%node_id%/RFSensorStatusInformation_battery_ind"
  "ication_code/config\0%homeassistant%/sensor/%node_id%/RFSensorStatusInformation_sensor_index/config\0%homeassistant%/sen"
  "sor/%node_id%/RFSensorStatusInformation_sensor_type/config\0%homeassistant%/sensor/%node_id%/RFSensorStatusInformation_s"
  "ensor_type_code/config\0%homeassistant%/sensor/%node_id%/RFSensorStatusInformation_signal_strength/config\0%homeassistan"
  "t%/sensor/%node_id%/RFSensorStatusInformation_signal_strength_code/config\0%homeassistant%/sensor/%node_id%/RFstrengthba"
  "tterylevel_hb_u8/config\0%homeassistant%/sensor/%node_id%/RFstrengthbatterylevel_lb_u8/config\0%homeassistant%/sensor/%n"
  "ode_id%/RFstrengthbatterylevel_hb_u8/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/RFstrengthbatteryle"
  "vel_lb_u8/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_DHW_hb_u8/config\0%homea"
  "ssistant%/sensor/%node_id%/OperatingMode_HC1_HC2_DHW_lb_u8/config\0%homeassistant%/sensor/%node_id%/RemoteOverrideOperat"
  "ingMode_dhw_mode/config\0%homeassistant%/sensor/%node_id%/RemoteOverrideOperatingMode_dhw_mode_code/config\0%homeassista"
  "nt%/sensor/%node_id%/RemoteOverrideOperatingMode_hc1_mode/config\0%homeassistant%/sensor/%node_id%/RemoteOverrideOperati"
  "ngMode_hc1_mode_code/config\0%homeassistant%/sensor/%node_id%/RemoteOverrideOperatingMode_hc2_mode/config\0%homeassistan"
  "t%/sensor/%node_id%/RemoteOverrideOperatingMode_hc2_mode_code/config\0%homeassistant%/sensor/%node_id%/RemoteOverrideOpe"
  "ratingMode_manual_dhw_push/config\0%homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_DHW_hb_u8/%source_topic_segmen"
  "t%/config\0%homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_DHW_lb_u8/%source_topic_segment%/config\0%homeassistan"
  "t%/binary_sensor/%node_id%/remote_override_manual_change_priority/config\0%homeassistant%/binary_sensor/%node_id%/remote"
  "_override_program_change_priority/config\0%homeassistant%/sensor/%node_id%/RoomRemoteOverrideFunction_flag8/config\0%hom"
  "eassistant%/binary_sensor/%node_id%/solar_storage_slave_fault_indicator/config\0%homeassistant%/sensor/%node_id%/solar_s"
  "torage_master_mode/config\0%homeassistant%/sensor/%node_id%/solar_storage_mode_status/config\0%homeassistant%/sensor/%no"
  "de_id%/solar_storage_slave_status/config\0%homeassistant%/sensor/%node_id%/SolarStorageASFflags_code/config\0%homeassist"
  "ant%/sensor/%node_id%/SolarStorageASFflags_flag8/config\0%homeassistant%/sensor/%node_id%/solar_storage_slave_configurat"
  "ion/config\0%homeassistant%/sensor/%node_id%/solar_storage_slave_memberid_code/config\0%homeassistant%/sensor/%node_id%/"
  "SolarStorageVersionType_hb_u8/config\0%homeassistant%/sensor/%node_id%/SolarStorageVersionType_lb_u8/config\0%homeassist"
  "ant%/sensor/%node_id%/SolarStorageTSP_hb_u8/config\0%homeassistant%/sensor/%node_id%/SolarStorageTSP_lb_u8/config\0%home"
  "assistant%/sensor/%node_id%/SolarStorageTSPindexTSPvalue_hb_u8/config\0%homeassistant%/sensor/%node_id%/SolarStorageTSPi"
  "ndexTSPvalue_lb_u8/config\0%homeassistant%/sensor/%node_id%/SolarStorageFHBsize_hb_u8/config\0%homeassistant%/sensor/%no"
  "de_id%/SolarStorageFHBsize_lb_u8/config\0%homeassistant%/sensor/%node_id%/SolarStorageFHBindexFHBvalue_hb_u8/config\0%ho"
  "meassistant%/sensor/%node_id%/SolarStorageFHBindexFHBvalue_lb_u8/config\0%homeassistant%/sensor/%node_id%/ElectricityPro"
  "ducerStarts/config\0%homeassistant%/sensor/%node_id%/ElectricityProducerStarts/%source_topic_segment%/config\0%homeassis"
  "tant%/sensor/%node_id%/ElectricityProducerHours/config\0%homeassistant%/sensor/%node_id%/ElectricityProducerHours/%sourc"
  "e_topic_segment%/config\0%homeassistant%/sensor/%node_id%/ElectricityProduction/config\0%homeassistant%/sensor/%node_id%"
  "/ElectricityProduction/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/CumulativeElectricityProduction/c"
  "onfig\0%homeassistant%/sensor/%node_id%/CumulativeElectricityProduction/%source_topic_segment%/config\0%homeassistant%/b"
  "inary_sensor/%node_id%/solar_storage_system_type/config\0%homeassistant%/sensor/%node_id%/BurnerUnsuccessfulStarts/confi"
  "g\0%homeassistant%/sensor/%node_id%/BurnerUnsuccessfulStarts/%source_topic_segment%/config\0%homeassistant%/sensor/%node"
  "_id%/FlameSignalTooLow/config\0%homeassistant%/sensor/%node_id%/FlameSignalTooLow/%source_topic_segment%/config\0%homeas"
  "sistant%/sensor/%node_id%/OEMDiagnosticCode/config\0%homeassistant%/sensor/%node_id%/OEMDiagnosticCode/%source_topic_seg"
  "ment%/config\0%homeassistant%/sensor/%node_id%/BurnerStarts/config\0%homeassistant%/sensor/%node_id%/BurnerStarts/%sourc"
  "e_topic_segment%/config\0%homeassistant%/sensor/%node_id%/CHPumpStarts/config\0%homeassistant%/sensor/%node_id%/CHPumpSt"
  "arts/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/DHWPumpValveStarts/config\0%homeassistant%/sensor/%"
  "node_id%/DHWPumpValveStarts/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/DHWBurnerStarts/config\0%hom"
  "eassistant%/sensor/%node_id%/DHWBurnerStarts/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/BurnerOpera"
  "tionHours/config\0%homeassistant%/sensor/%node_id%/BurnerOperationHours/%source_topic_segment%/config\0%homeassistant%/s"
  "ensor/%node_id%/CHPumpOperationHours/config\0%homeassistant%/sensor/%node_id%/CHPumpOperationHours/%source_topic_segment"
  "%/config\0%homeassistant%/sensor/%node_id%/DHWPumpValveOperationHours/config\0%homeassistant%/sensor/%node_id%/DHWPumpVa"
  "lveOperationHours/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/DHWBurnerOperationHours/config\0%homea"
  "ssistant%/sensor/%node_id%/DHWBurnerOperationHours/%source_topic_segment%/config\0%homeassistant%/sensor/%node_id%/OpenT"
  "hermVersionMaster/config\0%homeassistant%/sensor/%node_id%/OpenThermVersionMaster/%source_topic_segment%/config\0%homeas"
  "sistant%/sensor/%node_id%/OpenThermVersionSlave/config\0%homeassistant%/sensor/%node_id%/OpenThermVersionSlave/%source_t"
  "opic_segment%/config\0%homeassistant%/sensor/%node_id%/MasterVersion_hb_u8/config\0%homeassistant%/sensor/%node_id%/Mast"
  "erVersion_lb_u8/config\0%homeassistant%/sensor/%node_id%/SlaveVersion_hb_u8/config\0%homeassistant%/sensor/%node_id%/Sla"
  "veVersion_lb_u8/config\0%homeassistant%/sensor/%node_id%/RemehadFdUcodes_hb_u8/config\0%homeassistant%/sensor/%node_id%/"
  "RemehadFdUcodes_lb_u8/config\0%homeassistant%/sensor/%node_id%/RemehaServicemessage_hb_u8/config\0%homeassistant%/sensor"
  "/%node_id%/RemehaServicemessage_lb_u8/config\0%homeassistant%/sensor/%node_id%/RemehaDetectionConnectedSCU_hb_u8/config"
  "\0%homeassistant%/sensor/%node_id%/RemehaDetectionConnectedSCU_lb_u8/config\0%homeassistant%/sensor/%node_id%/s0pulsecou"
  "nt/config\0%homeassistant%/sensor/%node_id%/s0pulsecounttot/config\0%homeassistant%/sensor/%node_id%/s0pulsetime/config"
  "\0%homeassistant%/sensor/%node_id%/s0powerkw/config\0%homeassistant%/sensor/%node_id%/%sensor_id%/config\0"
;

// Message pool — 140,767 bytes
const char PROGMEM mqttHaMsgPool[] =
  "{\"action_template\": \"{% if value == 'ON' %}heating{% else %}idle{% endif %}\", \"action_topic\": \"%mqtt_pub_topic%/c"
  "h_enable\", \"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"name\": \""
  "%hostname%_Thermostat\", \"uniq_id\": \"%node_id%-thermostat\", \"curr_temp_t\": \"%mqtt_pub_topic%/Tr\", \"initial\": "
  "\"20\", \"max_temp\": \"28\", \"min_temp\": \"12\", \"mode_stat_tpl\": \"{% if value == 'ON' %}heat{% else %}off{% endif"
  " %}\", \"mode_stat_t\": \"%mqtt_pub_topic%/otgw-pic/thermostat_connected\", \"modes\": [\"off\", \"heat\"], \"precision"
  "\": 0.1, \"temp_cmd_t\": \"%mqtt_sub_topic%/command\", \"temp_cmd_tpl\": \"TT={{ value }}\", \"temp_stat_t\": \"%mqtt_pu"
  "b_topic%/TrSet\", \"temp_unit\": \"C\", \"temp_step\": \"0.5\", \"payload_off\": 0, \"payload_on\": 1 }\0{\"action_templ"
  "ate\": \"{% if value == 'ON' %}heating{% else %}idle{% endif %}\", \"action_topic\": \"%mqtt_pub_topic%/domestichotwater"
  "\", \"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mo"
  "del\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"name\": \"%hostnam"
  "e%_DHW_Control\", \"uniq_id\": \"%node_id%-dhw_control\", \"optimistic\": true, \"modes\": [\"off\", \"auto\"], \"mode_s"
  "tat_t\": \"%mqtt_pub_topic%/dhw_enable\", \"mode_stat_tpl\": \"{% if value == 'ON' %}auto{% else %}off{% endif %}\", \"c"
  "urr_temp_t\": \"%mqtt_pub_topic%/Tdhw\", \"temp_stat_t\": \"%mqtt_pub_topic%/TdhwSet\", \"temp_cmd_t\": \"%mqtt_sub_topi"
  "c%/command\", \"temp_cmd_tpl\": \"SW={{ value }}\", \"initial\": \"43\", \"min_temp\": \"40\", \"max_temp\": \"60\", \"t"
  "emp_step\": \"1\", \"precision\": 1, \"temp_unit\": \"C\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\""
  ": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%"
  ")\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-fault\", \"name\": \"%hostname%_Fault\", \"stat_t\": \"%mq"
  "tt_pub_topic%/fault\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": "
  "\"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}"
  ", \"uniq_id\": \"%node_id%-centralheating\", \"name\": \"%hostname%_Central_Heating\", \"stat_t\": \"%mqtt_pub_topic%/ce"
  "ntralheating\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelt"
  "e Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq"
  "_id\": \"%node_id%-domestichotwater\", \"name\": \"%hostname%_Domestic_Hot_Water\", \"stat_t\": \"%mqtt_pub_topic%/domes"
  "tichotwater\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte"
  " Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_"
  "id\": \"%node_id%-flame\", \"name\": \"%hostname%_Flame\", \"stat_t\": \"%mqtt_pub_topic%/flame\"}\0{\"avty_t\": \"%mqtt"
  "_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", "
  "\"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-cooling\", \"name"
  "\": \"%hostname%_Cooling\", \"stat_t\": \"%mqtt_pub_topic%/cooling\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"id"
  "entifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway "
  "(%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-centralheating2\", \"name\": \"%hostname%_Centr"
  "al_Heating_2\", \"stat_t\": \"%mqtt_pub_topic%/centralheating2\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identi"
  "fiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%ho"
  "stname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-diagnostic_indicator\", \"name\": \"%hostname%_Diag"
  "nostic_Indicator\", \"stat_t\": \"%mqtt_pub_topic%/diagnostic_indicator\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": "
  "{\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gat"
  "eway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-electric_production\", \"name\": \"%hostna"
  "me%_Electric_Production\", \"stat_t\": \"%mqtt_pub_topic%/electric_production\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"d"
  "ev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenThe"
  "rm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ch_enable\", \"name\": \"%hostname%_"
  "Central_Heating_enable\", \"stat_t\": \"%mqtt_pub_topic%/ch_enable\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"id"
  "entifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway "
  "(%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-dhw_enable\", \"name\": \"%hostname%_Domestic_H"
  "ot_Water_enable\", \"stat_t\": \"%mqtt_pub_topic%/dhw_enable\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-cooling_enable\", \"name\": \"%hostname%_Cooling_enab"
  "le\", \"stat_t\": \"%mqtt_pub_topic%/cooling_enable\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \""
  "%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\","
  " \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-otc_active\", \"name\": \"%hostname%_OTC_enable\", \"stat_t\":"
  " \"%mqtt_pub_topic%/otc_active\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufa"
  "cturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%v"
  "ersion%\"}, \"uniq_id\": \"%node_id%-ch2_enable\", \"name\": \"%hostname%_central_heating_2_enable\", \"stat_t\": \"%mqt"
  "t_pub_topic%/ch2_enable\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-thermostat_connected\", \"name\": \"%hostname%_Thermostat_Connected\", \"stat_t\": \"%mqt"
  "t_pub_topic%/otgw-pic/thermostat_connected\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id"
  "%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ve"
  "rsion\": \"%version%\"}, \"uniq_id\": \"%node_id%-boiler_connected\", \"name\": \"%hostname%_Boiler_Connected\", \"stat_"
  "t\": \"%mqtt_pub_topic%/otgw-pic/boiler_connected\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%"
  "node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-status_master\", \"name\": \"%hostname%_Status_Master\", \"stat"
  "_t\": \"%mqtt_pub_topic%/status_master\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", "
  "\"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version"
  "\": \"%version%\"}, \"uniq_id\": \"%node_id%-status_slave\", \"name\": \"%hostname%_Status_Slave\", \"stat_t\": \"%mqtt_"
  "pub_topic%/status_slave\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-dhw_blocking\", \"name\": \"%hostname%_dhw_blocking\", \"stat_t\": \"%mqtt_pub_topic%/dhw"
  "_blocking\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte B"
  "ron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id"
  "\": \"%node_id%-summerwintertime\", \"name\": \"%hostname%_summerwintertime\", \"stat_t\": \"%mqtt_pub_topic%/summerwint"
  "ertime\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-TSet\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Control_setpoint\", \"stat_t\": \"%mqtt_p"
  "ub_topic%/TSet\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measu"
  "rement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-TSet%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Control_setpoint %source_"
  "name%\", \"stat_t\": \"%mqtt_pub_topic%/TSet/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_t"
  "emplate\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifier"
  "s\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostna"
  "me%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-master_configuration_smart_power\", \"name\": \"%hostna"
  "me%_master_configuration_smart_power\", \"stat_t\": \"%mqtt_pub_topic%/master_configuration_smart_power\"}\0{\"avty_t\":"
  " \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-n"
  "odo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-master_con"
  "figuration\", \"name\": \"%hostname%_Status_Master_Configuration\", \"stat_t\": \"%mqtt_pub_topic%/master_configuration"
  "\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \""
  "model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%no"
  "de_id%-master_memberid_code\", \"name\": \"%hostname%_Status_Master_Memberid_Code\", \"stat_t\": \"%mqtt_pub_topic%/mast"
  "er_memberid_code\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sc"
  "helte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \""
  "uniq_id\": \"%node_id%-dhw_present\", \"name\": \"%hostname%_dhw_present\", \"stat_t\": \"%mqtt_pub_topic%/dhw_present\""
  "}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mo"
  "del\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node"
  "_id%-control_type_modulation\", \"name\": \"%hostname%_control_type_modulation\", \"stat_t\": \"%mqtt_pub_topic%/control"
  "_type_modulation\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sc"
  "helte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \""
  "uniq_id\": \"%node_id%-cooling_config\", \"name\": \"%hostname%_Cooling_configs\", \"stat_t\": \"%mqtt_pub_topic%/coolin"
  "g_config\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Br"
  "on\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id"
  "\": \"%node_id%-dhw_config\", \"name\": \"%hostname%_DHW_config\", \"stat_t\": \"%mqtt_pub_topic%/dhw_config\"}\0{\"avty"
  "_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"o"
  "tgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-maste"
  "r_low_off_pump_control_function\", \"name\": \"%hostname%_Master_low_off_pump_control_function\", \"stat_t\": \"%mqtt_pu"
  "b_topic%/master_low_off_pump_control_function\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_"
  "id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_"
  "version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ch2_present\", \"name\": \"%hostname%_ch2_present\", \"stat_t\": \"%"
  "mqtt_pub_topic%/ch2_present\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactu"
  "rer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%vers"
  "ion%\"}, \"uniq_id\": \"%node_id%-remote_water_filling_function\", \"name\": \"%hostname%_remote_water_filling_function"
  "\", \"stat_t\": \"%mqtt_pub_topic%/remote_water_filling_function\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"iden"
  "tifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%"
  "hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-heat_cool_mode_control\", \"name\": \"%hostname%_"
  "heat_cool_mode_control\", \"stat_t\": \"%mqtt_pub_topic%/heat_cool_mode_control\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-slave_configuration\", \"name\": "
  "\"%hostname%_Status_Slave_Configuration\", \"stat_t\": \"%mqtt_pub_topic%/slave_configuration\"}\0{\"avty_t\": \"%mqtt_p"
  "ub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"n"
  "ame\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-slave_memberid_code"
  "\", \"name\": \"%hostname%_Status_Slave_Memberid_Code\", \"stat_t\": \"%mqtt_pub_topic%/slave_memberid_code\"}\0{\"avty_"
  "t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"ot"
  "gw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Comman"
  "d_hb_u8\", \"name\": \"%hostname%_Remote_Command_Code\", \"stat_t\": \"%mqtt_pub_topic%/Command_hb_u8\", \"value_templat"
  "e\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \""
  "Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, "
  "\"uniq_id\": \"%node_id%-Command_lb_u8\", \"name\": \"%hostname%_Remote_Command_Response\", \"stat_t\": \"%mqtt_pub_topi"
  "c%/Command_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": "
  "\"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)"
  "\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Command_remote_command\", \"name\": \"%hostname%_Remote_Com"
  "mand\", \"stat_t\": \"%mqtt_pub_topic%/Command_remote_command\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%m"
  "qtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\""
  ", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-service_request"
  "\", \"name\": \"%hostname%_Service_request\", \"stat_t\": \"%mqtt_pub_topic%/service_request\"}\0{\"avty_t\": \"%mqtt_pu"
  "b_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"na"
  "me\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-lockout_reset\", \"na"
  "me\": \"%hostname%_Lockout_reset\", \"stat_t\": \"%mqtt_pub_topic%/lockout_reset\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-low_water_pressure\", \"name\": "
  "\"%hostname%_Low_water_press\", \"stat_t\": \"%mqtt_pub_topic%/low_water_pressure\"}\0{\"avty_t\": \"%mqtt_pub_topic%\","
  " \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Ope"
  "nTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-gas_flame_fault\", \"name\": \"%"
  "hostname%_Gas_flame_fault\", \"stat_t\": \"%mqtt_pub_topic%/gas_flame_fault\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-air_pressure_fault\", \"name\": \"%hos"
  "tname%_Air_press_fault\", \"stat_t\": \"%mqtt_pub_topic%/air_pressure_fault\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-water_over_temperature\", \"name\": \""
  "%hostname%_Water_over_temp\", \"stat_t\": \"%mqtt_pub_topic%/water_over_temperature\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ASF_flags\", \"name\": \"%hos"
  "tname%_Application_Specific_Fault\", \"stat_t\": \"%mqtt_pub_topic%/ASF_flags\", \"unit_of_measurement\": \"\", \"value_"
  "template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactur"
  "er\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versi"
  "on%\"}, \"uniq_id\": \"%node_id%-OEMFaultCode\", \"name\": \"%hostname%_OEMFaultCode\", \"stat_t\": \"%mqtt_pub_topic%/O"
  "EMFaultCode\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-rbp_dhw_setpoint\", \"name\": \"%"
  "hostname%_rbp_dhw_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/rbp_dhw_setpoint\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"d"
  "ev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenThe"
  "rm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-rbp_max_ch_setpoint\", \"name\": \"%"
  "hostname%_rbp_max_ch_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/rbp_max_ch_setpoint\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-rbp_rw_dhw_setpoint\", \"name"
  "\": \"%hostname%_rbp_rw_dhw_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/rbp_rw_dhw_setpoint\"}\0{\"avty_t\": \"%mqtt_pub_"
  "topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name"
  "\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-rbp_rw_max_ch_setpointr"
  "\", \"name\": \"%hostname%_rbp_rw_max_ch_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/rbp_rw_max_ch_setpoint\"}\0{\"avty_t"
  "\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otg"
  "w-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RBP_fla"
  "gs_read_write\", \"name\": \"%hostname%_RBP_flags_read_write\", \"stat_t\": \"%mqtt_pub_topic%/RBP_flags_read_write\", "
  "\"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"man"
  "ufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-RBP_flags_transfer_enable\", \"name\": \"%hostname%_RBP_flags_transfer_enable\""
  ", \"stat_t\": \"%mqtt_pub_topic%/RBP_flags_transfer_enable\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt"
  "_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", "
  "\"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-CoolingControl\","
  " \"name\": \"%hostname%_Cooling_control_signal\", \"stat_t\": \"%mqtt_pub_topic%/CoolingControl\", \"unit_of_measurement"
  "\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-CoolingControl%source_suffix%\", "
  "\"name\": \"%hostname%_Cooling_control_signal %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/CoolingControl/%source_top"
  "ic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{"
  "\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-TsetCH2\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Control_setpoint_2\", \"stat_t\": \"%mqtt_pub_top"
  "ic%/TsetCH2\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurem"
  "ent\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\""
  ", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": "
  "\"%node_id%-TsetCH2%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Control_setpoint_2 %sour"
  "ce_name%\", \"stat_t\": \"%mqtt_pub_topic%/TsetCH2/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"v"
  "alue_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"iden"
  "tifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%"
  "hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrOverride\", \"device_class\": \"temperature\", "
  "\"name\": \"%hostname%_Remote_override_room_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/TrOverride\", \"unit_of_measureme"
  "nt\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_"
  "topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name"
  "\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrOverride%source_suffi"
  "x%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Remote_override_room_setpoint %source_name%\", \"stat_t"
  "\": \"%mqtt_pub_topic%/TrOverride/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": "
  "\"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%nod"
  "e_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"s"
  "w_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TSP_hb_u8\", \"name\": \"%hostname%_TSP_Count\", \"stat_t\": \"%mq"
  "tt_pub_topic%/TSP_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TSP_lb_u8\", \"name\": \"%hostname%_TSP_Index\", \"st"
  "at_t\": \"%mqtt_pub_topic%/TSP_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TSPindexTSPvalue_hb_u8\", \"name\": \"%h"
  "ostname%_TSP_Entry_Index\", \"stat_t\": \"%mqtt_pub_topic%/TSPindexTSPvalue_hb_u8\", \"value_template\": \"{{ value }}\""
  "}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mo"
  "del\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node"
  "_id%-TSPindexTSPvalue_lb_u8\", \"name\": \"%hostname%_TSP_Entry_Value\", \"stat_t\": \"%mqtt_pub_topic%/TSPindexTSPvalue"
  "_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id"
  "%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ve"
  "rsion\": \"%version%\"}, \"uniq_id\": \"%node_id%-FHBsize_hb_u8\", \"name\": \"%hostname%_Fault_History_Buffer_Size\", "
  "\"stat_t\": \"%mqtt_pub_topic%/FHBsize_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\","
  " \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Ope"
  "nTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-FHBsize_lb_u8\", \"name\": \"%ho"
  "stname%_Fault_History_Buffer_Max\", \"stat_t\": \"%mqtt_pub_topic%/FHBsize_lb_u8\", \"value_template\": \"{{ value }}\"}"
  "\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mod"
  "el\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_"
  "id%-FHBindexFHBvalue_hb_u8\", \"name\": \"%hostname%_Fault_History_Index\", \"stat_t\": \"%mqtt_pub_topic%/FHBindexFHBva"
  "lue_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node"
  "_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw"
  "_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-FHBindexFHBvalue_lb_u8\", \"name\": \"%hostname%_Fault_History_Valu"
  "e\", \"stat_t\": \"%mqtt_pub_topic%/FHBindexFHBvalue_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt"
  "_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", "
  "\"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MaxRelModLevelSet"
  "ting\", \"name\": \"%hostname%_Max_Rel_Modulation_level_setting\", \"stat_t\": \"%mqtt_pub_topic%/MaxRelModLevelSetting"
  "\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t"
  "\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otg"
  "w-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MaxRelM"
  "odLevelSetting%source_suffix%\", \"name\": \"%hostname%_Max_Rel_Modulation_level_setting %source_name%\", \"stat_t\": \""
  "%mqtt_pub_topic%/MaxRelModLevelSetting/%source_topic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{"
  "{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_i"
  "d%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_v"
  "ersion\": \"%version%\"}, \"uniq_id\": \"%node_id%-MaxCapacityMinModLevel_lb_u8\", \"device_class\": \"power_factor\", "
  "\"name\": \"%hostname%_MaxCapacityMinModLevel_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/MaxCapacityMinModLevel_lb_u8\", \""
  "unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ident"
  "ifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%h"
  "ostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MaxCapacityMinModLevel_hb_u8\", \"device_class\": "
  "\"power\", \"name\": \"%hostname%_MaxCapacityMinModLevel_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/MaxCapacityMinModLevel_"
  "hb_u8\", \"unit_of_measurement\": \"kW\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrSet\", \"device_class\": \"temperatu"
  "re\", \"name\": \"%hostname%_Room_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/TrSet\", \"unit_of_measurement\": \"\xc2"
  "\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \""
  "dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTh"
  "erm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrSet%source_suffix%\", \"device_cl"
  "ass\": \"temperature\", \"name\": \"%hostname%_Room_setpoint %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TrSet/%sour"
  "ce_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"mea"
  "surement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Br"
  "on\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id"
  "\": \"%node_id%-RelModLevel\", \"name\": \"%hostname%_Relative_Modulation_Level\", \"stat_t\": \"%mqtt_pub_topic%/RelMod"
  "Level\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"av"
  "ty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": "
  "\"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Re"
  "lModLevel%source_suffix%\", \"name\": \"%hostname%_Relative_Modulation_Level %source_name%\", \"stat_t\": \"%mqtt_pub_to"
  "pic%/RelModLevel/%source_topic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_"
  "class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\""
  ": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%"
  "\"}, \"uniq_id\": \"%node_id%-CHPressure\", \"name\": \"%hostname%_Water_pressure_in_CH_circuit\", \"stat_t\": \"%mqtt_p"
  "ub_topic%/CHPressure\", \"unit_of_measurement\": \"bar\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measu"
  "rement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-CHPressure%source_suffix%\", \"device_class\": \"pressure\", \"name\": \"%hostname%_Water_pressure_in_CH_c"
  "ircuit %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/CHPressure/%source_topic_segment%\", \"unit_of_measurement\": \"b"
  "ar\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-DHWFlowRate\", \"name\": \"%hostname%_Wa"
  "ter_flow_rate_in_DHW circuit\", \"stat_t\": \"%mqtt_pub_topic%/DHWFlowRate\", \"unit_of_measurement\": \"l/min\", \"valu"
  "e_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ident"
  "ifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%h"
  "ostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-DHWFlowRate%source_suffix%\", \"name\": \"%hostnam"
  "e%_Water_flow_rate_in_DHW_circuit %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/DHWFlowRate/%source_topic_segment%\", "
  "\"unit_of_measurement\": \"l/min\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\":"
  " \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-n"
  "odo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-DayTime_da"
  "yofweek\", \"name\": \"%hostname%_DayTime_dayofweek\", \"stat_t\": \"%mqtt_pub_topic%/DayTime_dayofweek\", \"value_templ"
  "ate\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": "
  "\"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}"
  ", \"uniq_id\": \"%node_id%-DayTime_hour\", \"name\": \"%hostname%_DayTime_hour\", \"stat_t\": \"%mqtt_pub_topic%/DayTime"
  "_hour\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-DayTime_minutes\", \"name\": \"%hostname%_DayTime_minutes\", \"stat_t\""
  ": \"%mqtt_pub_topic%/DayTime_minutes\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Date_day_of_month\", \"name\": \"%hostna"
  "me%_Date_day_of_month\", \"stat_t\": \"%mqtt_pub_topic%/Date_day_of_month\", \"value_template\": \"{{ value }}\"}\0{\"av"
  "ty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": "
  "\"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Da"
  "te_month\", \"name\": \"%hostname%_Date_month\", \"stat_t\": \"%mqtt_pub_topic%/Date_month\", \"value_template\": \"{{ v"
  "alue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-Year\", \"name\": \"%hostname%_Year\", \"stat_t\": \"%mqtt_pub_topic%/Year\", \"value_template\": \"{{ val"
  "ue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-Year%source_suffix%\", \"name\": \"%hostname%_Year %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Year/%so"
  "urce_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers"
  "\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostnam"
  "e%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrSetCH2\", \"name\": \"%hostname%_Room_Setpoint_CH2\", "
  "\"stat_t\": \"%mqtt_pub_topic%/TrSetCH2\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \""
  "state_class\": \"measurement\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ide"
  "ntifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway ("
  "%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrSetCH2%source_suffix%\", \"name\": \"%hostname"
  "%_Room_Setpoint_CH2 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TrSetCH2/%source_topic_segment%\", \"device_class\":"
  " \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"state_class\": \"measurement\", \"value_template\": \"{{ val"
  "ue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-Troom\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Room_Temperature\", \"stat_t\": \"%mqtt_"
  "pub_topic%/Tr\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measur"
  "ement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-Tr%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Room_Temperature %source_nam"
  "e%\", \"stat_t\": \"%mqtt_pub_topic%/Tr/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_templa"
  "te\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": "
  "\"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)"
  "\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tboiler\", \"device_class\": \"temperature\", \"name\": \"%"
  "hostname%_Boiler_flow_water_temperature\", \"stat_t\": \"%mqtt_pub_topic%/Tboiler\", \"unit_of_measurement\": \"\xc2\xb0"
  "C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tboiler%source_suffix%\", \"device_cla"
  "ss\": \"temperature\", \"name\": \"%hostname%_Boiler_flow_water_temperature %source_name%\", \"stat_t\": \"%mqtt_pub_top"
  "ic%/Tboiler/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"sta"
  "te_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacture"
  "r\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versio"
  "n%\"}, \"uniq_id\": \"%node_id%-Tdhw\", \"device_class\": \"temperature\", \"name\": \"%hostname%_DHW_temperature\", \"s"
  "tat_t\": \"%mqtt_pub_topic%/Tdhw\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state"
  "_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacture"
  "r\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versio"
  "n%\"}, \"uniq_id\": \"%node_id%-Tdhw%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_DHW_tem"
  "perature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Tdhw/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2"
  "\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"de"
  "v\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTher"
  "m Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Toutside\", \"device_class\": \"tempe"
  "rature\", \"name\": \"%hostname%_Outside_Temperature\", \"stat_t\": \"%mqtt_pub_topic%/Toutside\", \"unit_of_measurement"
  "\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_to"
  "pic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\""
  ": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Toutside_override\", \"de"
  "vice_class\": \"temperature\", \"name\": \"%hostname%_Outside_Temperature_Override\", \"cmd_t\": \"%mqtt_sub_topic%/outs"
  "ide\", \"stat_t\": \"%mqtt_pub_topic%/Toutside\", \"unit_of_measurement\": \"\xc2\xb0C\", \"min\": -40, \"max\": 50, \"s"
  "tep\": 0.5, \"mode\": \"box\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufact"
  "urer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%ver"
  "sion%\"}, \"uniq_id\": \"%node_id%-Toutside%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_"
  "Outside_Temperature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Toutside/%source_topic_segment%\", \"unit_of_measure"
  "ment\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_"
  "topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name"
  "\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tret\", \"device_class"
  "\": \"temperature\", \"name\": \"%hostname%_Return_water_temperature\", \"stat_t\": \"%mqtt_pub_topic%/Tret\", \"unit_of"
  "_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \""
  "%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo"
  "\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tret%source_s"
  "uffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Return_water_temperature %source_name%\", \"stat_t\""
  ": \"%mqtt_pub_topic%/Tret/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ val"
  "ue }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\","
  " \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_versio"
  "n\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tsolarstorage\", \"device_class\": \"temperature\", \"name\": \"%hostname%"
  "_Solar_storage_temperature\", \"stat_t\": \"%mqtt_pub_topic%/Tsolarstorage\", \"unit_of_measurement\": \"\xc2\xb0C\", \""
  "value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"i"
  "dentifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway"
  " (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tsolarstorage%source_suffix%\", \"device_class"
  "\": \"temperature\", \"name\": \"%hostname%_Solar_storage_temperature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Ts"
  "olarstorage/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"sta"
  "te_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacture"
  "r\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versio"
  "n%\"}, \"uniq_id\": \"%node_id%-Tsolarcollector\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Solar_colle"
  "ctor_temperature\", \"stat_t\": \"%mqtt_pub_topic%/Tsolarcollector\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_te"
  "mplate\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifie"
  "rs\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostn"
  "ame%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Tsolarcollector%source_suffix%\", \"device_class\": \""
  "temperature\", \"name\": \"%hostname%_Solar_collector_temperature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Tsolar"
  "collector/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state"
  "_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-TflowCH2\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Flow_water_temperat"
  "ure_CH2 cir.\", \"stat_t\": \"%mqtt_pub_topic%/TflowCH2\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": "
  "\"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%n"
  "ode_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TflowCH2%source_suffix%\", \"device_class\": \"temperature\", "
  "\"name\": \"%hostname%_Flow_water_temperature_CH2 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TflowCH2/%source_topic"
  "_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement"
  "\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \""
  "model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%no"
  "de_id%-Tdhw2\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Domestic_hot_water_temperature_2\", \"stat_t\""
  ": \"%mqtt_pub_topic%/Tdhw2\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class"
  "\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": "
  "\"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}"
  ", \"uniq_id\": \"%node_id%-Tdhw2%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Domestic_ho"
  "t_water_temperature_2 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Tdhw2/%source_topic_segment%\", \"unit_of_measurem"
  "ent\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_t"
  "opic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name"
  "\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Texhaust\", \"device_cl"
  "ass\": \"temperature\", \"name\": \"%hostname%_Boiler_exhaust_temperature\", \"stat_t\": \"%mqtt_pub_topic%/Texhaust\", "
  "\"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avt"
  "y_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \""
  "otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Texh"
  "aust%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Boiler_exhaust_temperature %source_name"
  "%\", \"stat_t\": \"%mqtt_pub_topic%/Texhaust/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_t"
  "emplate\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifier"
  "s\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostna"
  "me%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Theatexchanger\", \"name\": \"%hostname%_Heat_Exchanger"
  "_Temperature\", \"stat_t\": \"%mqtt_pub_topic%/Theatexchanger\", \"device_class\": \"temperature\", \"unit_of_measuremen"
  "t\": \"\xc2\xb0C\", \"state_class\": \"measurement\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_top"
  "ic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\":"
  " \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Theatexchanger%source_suff"
  "ix%\", \"name\": \"%hostname%_Heat_Exchanger_Temperature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Theatexchanger/"
  "%source_topic_segment%\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"state_class\": \""
  "measurement\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%no"
  "de_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \""
  "sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-FanSpeed_setpoint_hz\", \"name\": \"%hostname%_Boiler_fan_speed_s"
  "etpoint\", \"stat_t\": \"%mqtt_pub_topic%/FanSpeed_hb_u8\", \"unit_of_measurement\": \"Hz\", \"value_template\": \"{{ va"
  "lue }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-FanSpeed_actual_hz\", \"name\": \"%hostname%_Boiler_fan_speed_actual\","
  " \"stat_t\": \"%mqtt_pub_topic%/FanSpeed_lb_u8\", \"unit_of_measurement\": \"Hz\", \"value_template\": \"{{ value }}\", "
  "\"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manu"
  "facturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \""
  "%version%\"}, \"uniq_id\": \"%node_id%-ElectricalCurrentBurnerFlame\", \"name\": \"%hostname%_ElectricalCurrentBurnerFla"
  "me\", \"stat_t\": \"%mqtt_pub_topic%/ElectricalCurrentBurnerFlame\", \"unit_of_measurement\": \"\xc2\xb5A\", \"value_tem"
  "plate\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-ElectricalCurrentBurnerFlame%source_suffix%\", \"name\": \"%hostname%_ElectricalCurrentBu"
  "rnerFlame %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/ElectricalCurrentBurnerFlame/%source_topic_segment%\", \"unit_"
  "of_measurement\": \"\xc2\xb5A\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"id"
  "entifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway "
  "(%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TRoomCH2\", \"name\": \"%hostname%_Room_Tempera"
  "ture_CH2\", \"stat_t\": \"%mqtt_pub_topic%/TRoomCH2\", \"device_class\": \"temperature\", \"unit_of_measurement\": \""
  "\xc2\xb0C\", \"state_class\": \"measurement\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TRoomCH2%source_suffix%\", \"name"
  "\": \"%hostname%_Room_Temperature_CH2 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TRoomCH2/%source_topic_segment%\","
  " \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"state_class\": \"measurement\", \"value_te"
  "mplate\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-RelativeHumidity\", \"name\": \"%hostname%_Relative_Humidity\", \"stat_t\": \"%mqtt_pub_t"
  "opic%/RelativeHumidity\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measu"
  "rement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-RelativeHumidity%source_suffix%\", \"name\": \"%hostname%_Relative_Humidity %source_name%\", \"stat_t\": "
  "\"%mqtt_pub_topic%/RelativeHumidity/%source_topic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ v"
  "alue }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrOverride2\", \"name\": \"%hostname%_Remote_Override_Setpoint_CH2\", "
  "\"stat_t\": \"%mqtt_pub_topic%/TrOverride2\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\","
  " \"state_class\": \"measurement\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\""
  "identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gatewa"
  "y (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TrOverride2%source_suffix%\", \"name\": \"%ho"
  "stname%_Remote_Override_Setpoint_CH2 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TrOverride2/%source_topic_segment%"
  "\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"state_class\": \"measurement\", \"value"
  "_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactur"
  "er\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versi"
  "on%\"}, \"uniq_id\": \"%node_id%-TdhwSetUBTdhwSetLB_value_lb\", \"device_class\": \"temperature\", \"name\": \"%hostname"
  "%_TdhwSetUBTdhwSetLB_value_lb\", \"stat_t\": \"%mqtt_pub_topic%/TdhwSetUBTdhwSetLB_value_lb\", \"unit_of_measurement\": "
  "\"\xc2\xb0C\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%n"
  "ode_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TdhwSetUBTdhwSetLB_value_hb\", \"device_class\": \"temperature"
  "\", \"name\": \"%hostname%_TdhwSetUBTdhwSetLB_value_hb\", \"stat_t\": \"%mqtt_pub_topic%/TdhwSetUBTdhwSetLB_value_hb\", "
  "\"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TdhwSetUBTdhwSetLB_value_hb%source_suf"
  "fix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_TdhwSetUBTdhwSetLB_value_hb %source_name%\", \"stat_t"
  "\": \"%mqtt_pub_topic%/TdhwSetUBTdhwSetLB_value_hb/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"v"
  "alue_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufa"
  "cturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%v"
  "ersion%\"}, \"uniq_id\": \"%node_id%-TdhwSetUBTdhwSetLB_value_lb%source_suffix%\", \"device_class\": \"temperature\", \""
  "name\": \"%hostname%_TdhwSetUBTdhwSetLB_value_lb %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TdhwSetUBTdhwSetLB_valu"
  "e_lb/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\""
  ": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-"
  "nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MaxTSetUB"
  "MaxTSetLB_value_lb\", \"device_class\": \"temperature\", \"name\": \"%hostname%_MaxTSetUBMaxTSetLB_value_lb\", \"stat_t"
  "\": \"%mqtt_pub_topic%/MaxTSetUBMaxTSetLB_value_lb\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ v"
  "alue }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Br"
  "on\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id"
  "\": \"%node_id%-MaxTSetUBMaxTSetLB_value_hb\", \"device_class\": \"temperature\", \"name\": \"%hostname%_MaxTSetUBMaxTSe"
  "tLB_value_hb\", \"stat_t\": \"%mqtt_pub_topic%/MaxTSetUBMaxTSetLB_value_hb\", \"unit_of_measurement\": \"\xc2\xb0C\", \""
  "value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manu"
  "facturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \""
  "%version%\"}, \"uniq_id\": \"%node_id%-MaxTSetUBMaxTSetLB_value_hb%source_suffix%\", \"device_class\": \"temperature\", "
  "\"name\": \"%hostname%_MaxTSetUBMaxTSetLB_value_hb %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/MaxTSetUBMaxTSetLB_va"
  "lue_hb/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\"}\0{\"avty_t"
  "\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otg"
  "w-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MaxTSet"
  "UBMaxTSetLB_value_lb%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_MaxTSetUBMaxTSetLB_valu"
  "e_lb %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/MaxTSetUBMaxTSetLB_value_lb/%source_topic_segment%\", \"unit_of_mea"
  "surement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-HcratioUBHcratioLB_value_lb\", \"device_class\": \"te"
  "mperature\", \"name\": \"%hostname%_HcratioUBHcratioLB_value_lb\", \"stat_t\": \"%mqtt_pub_topic%/HcratioUBHcratioLB_val"
  "ue_lb\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-HcratioUBHcratioLB_value_hb\""
  ", \"device_class\": \"temperature\", \"name\": \"%hostname%_HcratioUBHcratioLB_value_hb\", \"stat_t\": \"%mqtt_pub_topic"
  "%/HcratioUBHcratioLB_value_hb\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\" }\0{\"avty_"
  "t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"ot"
  "gw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remote"
  "parameter4boundaries_value_hb\", \"name\": \"%hostname%_Remote_Parameter_4_Boundary_HB\", \"stat_t\": \"%mqtt_pub_topic%"
  "/Remoteparameter4boundaries_value_hb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter4boundaries_value_hb%sour"
  "ce_suffix%\", \"name\": \"%hostname%_Remote_Parameter_4_Boundary_HB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remo"
  "teparameter4boundaries_value_hb/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub"
  "_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"nam"
  "e\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter4bounda"
  "ries_value_lb\", \"name\": \"%hostname%_Remote_Parameter_4_Boundary_LB\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter"
  "4boundaries_value_lb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers"
  "\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostnam"
  "e%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter4boundaries_value_lb%source_suffix%\", \""
  "name\": \"%hostname%_Remote_Parameter_4_Boundary_LB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter4boun"
  "daries_value_lb/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter5boundaries_value_hb\","
  " \"name\": \"%hostname%_Remote_Parameter_5_Boundary_HB\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter5boundaries_valu"
  "e_hb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter5boundaries_value_hb%source_suffix%\", \"name\": \"%host"
  "name%_Remote_Parameter_5_Boundary_HB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter5boundaries_value_hb"
  "/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter5boundaries_value_lb\", \"name\": \"%h"
  "ostname%_Remote_Parameter_5_Boundary_LB\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter5boundaries_value_lb\", \"value"
  "_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactur"
  "er\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versi"
  "on%\"}, \"uniq_id\": \"%node_id%-Remoteparameter5boundaries_value_lb%source_suffix%\", \"name\": \"%hostname%_Remote_Par"
  "ameter_5_Boundary_LB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter5boundaries_value_lb/%source_topic_s"
  "egment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_i"
  "d%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_v"
  "ersion\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter6boundaries_value_hb\", \"name\": \"%hostname%_Remote_"
  "Parameter_6_Boundary_HB\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter6boundaries_value_hb\", \"value_template\": \"{"
  "{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte "
  "Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_i"
  "d\": \"%node_id%-Remoteparameter6boundaries_value_hb%source_suffix%\", \"name\": \"%hostname%_Remote_Parameter_6_Boundar"
  "y_HB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter6boundaries_value_hb/%source_topic_segment%\", \"val"
  "ue_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufact"
  "urer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%ver"
  "sion%\"}, \"uniq_id\": \"%node_id%-Remoteparameter6boundaries_value_lb\", \"name\": \"%hostname%_Remote_Parameter_6_Boun"
  "dary_LB\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter6boundaries_value_lb\", \"value_template\": \"{{ value }}\"}\0{"
  "\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-Remoteparameter6boundaries_value_lb%source_suffix%\", \"name\": \"%hostname%_Remote_Parameter_6_Boundary_LB %source_na"
  "me%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter6boundaries_value_lb/%source_topic_segment%\", \"value_template\": "
  "\"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schel"
  "te Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uni"
  "q_id\": \"%node_id%-Remoteparameter7boundaries_value_hb\", \"name\": \"%hostname%_Remote_Parameter_7_Boundary_HB\", \"st"
  "at_t\": \"%mqtt_pub_topic%/Remoteparameter7boundaries_value_hb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%"
  "mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo"
  "\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparamet"
  "er7boundaries_value_hb%source_suffix%\", \"name\": \"%hostname%_Remote_Parameter_7_Boundary_HB %source_name%\", \"stat_t"
  "\": \"%mqtt_pub_topic%/Remoteparameter7boundaries_value_hb/%source_topic_segment%\", \"value_template\": \"{{ value }}\""
  "}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mo"
  "del\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node"
  "_id%-Remoteparameter7boundaries_value_lb\", \"name\": \"%hostname%_Remote_Parameter_7_Boundary_LB\", \"stat_t\": \"%mqtt"
  "_pub_topic%/Remoteparameter7boundaries_value_lb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter7boundaries_va"
  "lue_lb%source_suffix%\", \"name\": \"%hostname%_Remote_Parameter_7_Boundary_LB %source_name%\", \"stat_t\": \"%mqtt_pub_"
  "topic%/Remoteparameter7boundaries_value_lb/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": "
  "\"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-no"
  "do\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparam"
  "eter8boundaries_value_hb\", \"name\": \"%hostname%_Remote_Parameter_8_Boundary_HB\", \"stat_t\": \"%mqtt_pub_topic%/Remo"
  "teparameter8boundaries_value_hb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\""
  "identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gatewa"
  "y (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter8boundaries_value_hb%source_su"
  "ffix%\", \"name\": \"%hostname%_Remote_Parameter_8_Boundary_HB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remotepar"
  "ameter8boundaries_value_hb/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topi"
  "c%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": "
  "\"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter8boundaries_"
  "value_lb\", \"name\": \"%hostname%_Remote_Parameter_8_Boundary_LB\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter8boun"
  "daries_value_lb\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": "
  "\"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)"
  "\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter8boundaries_value_lb%source_suffix%\", \"nam"
  "e\": \"%hostname%_Remote_Parameter_8_Boundary_LB %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter8boundar"
  "ies_value_lb/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\":"
  " {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Ga"
  "teway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TdhwSet\", \"device_class\": \"temperatur"
  "e\", \"name\": \"%hostname%_DHW_setpoint\", \"stat_t\": \"%mqtt_pub_topic%/TdhwSet\", \"unit_of_measurement\": \"\xc2"
  "\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \""
  "dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTh"
  "erm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TdhwSet%source_suffix%\", \"device_"
  "class\": \"temperature\", \"name\": \"%hostname%_DHW_setpoint %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/TdhwSet/%s"
  "ource_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \""
  "measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte"
  " Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_"
  "id\": \"%node_id%-MaxTSet\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Max_CH_water_setpoint\", \"stat_t"
  "\": \"%mqtt_pub_topic%/MaxTSet\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_c"
  "lass\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-MaxTSet%source_suffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_Max_C"
  "H_water_setpoint %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/MaxTSet/%source_topic_segment%\", \"unit_of_measurement"
  "\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topi"
  "c%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": "
  "\"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Hcratio\", \"device_class\""
  ": \"temperature\", \"name\": \"%hostname%_OTC_heat_curve_ratio\", \"stat_t\": \"%mqtt_pub_topic%/Hcratio\", \"unit_of_me"
  "asurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mq"
  "tt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\","
  " \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Hcratio%source_s"
  "uffix%\", \"device_class\": \"temperature\", \"name\": \"%hostname%_OTC_heat_curve_ratio %source_name%\", \"stat_t\": \""
  "%mqtt_pub_topic%/Hcratio/%source_topic_segment%\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ valu"
  "e }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", "
  "\"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version"
  "\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter4\", \"name\": \"%hostname%_Remote_Parameter_4\", \"stat_t\""
  ": \"%mqtt_pub_topic%/Remoteparameter4\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter4%source_suffix%\", \"n"
  "ame\": \"%hostname%_Remote_Parameter_4 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter4/%source_topic_se"
  "gment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id"
  "%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ve"
  "rsion\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter5\", \"name\": \"%hostname%_Remote_Parameter_5\", \"sta"
  "t_t\": \"%mqtt_pub_topic%/Remoteparameter5\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter5%source_suffix%\""
  ", \"name\": \"%hostname%_Remote_Parameter_5 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter5/%source_top"
  "ic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%no"
  "de_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \""
  "sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter6\", \"name\": \"%hostname%_Remote_Parameter_6\", "
  "\"stat_t\": \"%mqtt_pub_topic%/Remoteparameter6\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter6%source_suffi"
  "x%\", \"name\": \"%hostname%_Remote_Parameter_6 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter6/%source"
  "_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": "
  "\"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)"
  "\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter7\", \"name\": \"%hostname%_Remote_Parameter"
  "_7\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter7\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_"
  "topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name"
  "\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter7%source"
  "_suffix%\", \"name\": \"%hostname%_Remote_Parameter_7 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter7/%"
  "source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifier"
  "s\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostna"
  "me%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter8\", \"name\": \"%hostname%_Remote_Param"
  "eter_8\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparameter8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_"
  "pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \""
  "name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remoteparameter8%so"
  "urce_suffix%\", \"name\": \"%hostname%_Remote_Parameter_8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Remoteparamete"
  "r8/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identi"
  "fiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%ho"
  "stname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_ventilation_enabled\", \"name\": \"%hostname%_vh"
  "_ventilation_enabled\", \"stat_t\": \"%mqtt_pub_topic%/vh_ventilation_enabled\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"d"
  "ev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenThe"
  "rm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_bypass_position\", \"name\": \"%h"
  "ostname%_vh_bypass_position\", \"stat_t\": \"%mqtt_pub_topic%/vh_bypass_position\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_bypass_mode\", \"name\": \"%ho"
  "stname%_vh_bypass_mode\", \"stat_t\": \"%mqtt_pub_topic%/vh_bypass_mode\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": "
  "{\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gat"
  "eway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_free_ventilation_mode\", \"name\": \"%h"
  "ostname%_vh_free_ventilation_mode\", \"stat_t\": \"%mqtt_pub_topic%/vh_free_ventilation_mode\"}\0{\"avty_t\": \"%mqtt_pu"
  "b_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"na"
  "me\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_fault\", \"name\":"
  " \"%hostname%_vh_fault\", \"stat_t\": \"%mqtt_pub_topic%/vh_fault\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ide"
  "ntifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway ("
  "%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_ventilation_mode\", \"name\": \"%hostname%_vh"
  "_ventilation_mode\", \"stat_t\": \"%mqtt_pub_topic%/vh_ventilation_mode\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": "
  "{\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gat"
  "eway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_bypass_status\", \"name\": \"%hostname%"
  "_vh_bypass_status\", \"stat_t\": \"%mqtt_pub_topic%/vh_bypass_status\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\""
  "identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gatewa"
  "y (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_bypass_automatic_status\", \"name\": \"%ho"
  "stname%_vh_bypass_automatic_status\", \"stat_t\": \"%mqtt_pub_topic%/vh_bypass_automatic_status\"}\0{\"avty_t\": \"%mqtt"
  "_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", "
  "\"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_free_ventliati"
  "on_status\", \"name\": \"%hostname%_vh_free_ventliation_status\", \"stat_t\": \"%mqtt_pub_topic%/vh_free_ventliation_sta"
  "tus\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\","
  " \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \""
  "%node_id%-vh_diagnostic_indicator\", \"name\": \"%hostname%_vh_diagnostic_indicator\", \"stat_t\": \"%mqtt_pub_topic%/vh"
  "_diagnostic_indicator\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\":"
  " \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\""
  "}, \"uniq_id\": \"%node_id%-status_vh_master\", \"name\": \"%hostname%_status_vh_master\", \"stat_t\": \"%mqtt_pub_topic"
  "%/status_vh_master\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\""
  ": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%"
  ")\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-status_vh_slave\", \"name\": \"%hostname%_status_vh_slave"
  "\", \"stat_t\": \"%mqtt_pub_topic%/status_vh_slave\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_top"
  "ic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\":"
  " \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ControlSetpointVH\", \"nam"
  "e\": \"%hostname%_VH_relative_ventilation_position\", \"stat_t\": \"%mqtt_pub_topic%/ControlSetpointVH\", \"unit_of_meas"
  "urement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\" : \"measurement\" }\0{\"avty_t\": \"%mqtt_pub_top"
  "ic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\":"
  " \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ControlSetpointVH%source_s"
  "uffix%\", \"name\": \"%hostname%_VH_relative_ventilation_position %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Contro"
  "lSetpointVH/%source_topic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class"
  "\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"S"
  "chelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, "
  "\"uniq_id\": \"%node_id%-ControlSetpointVH_hb_u8\", \"name\": \"%hostname%_ControlSetpointVH_hb_u8\", \"stat_t\": \"%mqt"
  "t_pub_topic%/ControlSetpointVH_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ControlSetpointVH_lb_u8\", \"name\": \"%"
  "hostname%_ControlSetpointVH_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/ControlSetpointVH_lb_u8\", \"value_template\": \"{{ "
  "value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Br"
  "on\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id"
  "\": \"%node_id%-ControlSetpointVH_hb_u8%source_suffix%\", \"name\": \"%hostname%_ControlSetpointVH_hb_u8 %source_name%\""
  ", \"stat_t\": \"%mqtt_pub_topic%/ControlSetpointVH_hb_u8/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}"
  "\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mod"
  "el\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_"
  "id%-ControlSetpointVH_lb_u8%source_suffix%\", \"name\": \"%hostname%_ControlSetpointVH_lb_u8 %source_name%\", \"stat_t\""
  ": \"%mqtt_pub_topic%/ControlSetpointVH_lb_u8/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\""
  ": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-"
  "nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ASFFaultC"
  "odeVH_code\", \"name\": \"%hostname%_ASFFaultCodeVH_code\", \"stat_t\": \"%mqtt_pub_topic%/ASFFaultCodeVH_code\", \"valu"
  "e_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactu"
  "rer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%vers"
  "ion%\"}, \"uniq_id\": \"%node_id%-ASFFaultCodeVH_flag8\", \"name\": \"%hostname%_ASFFaultCodeVH_flag8\", \"stat_t\": \"%"
  "mqtt_pub_topic%/ASFFaultCodeVH_flag8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-DiagnosticCodeVH\", \"name\": \"%hostnam"
  "e%_DiagnosticCodeVH\", \"stat_t\": \"%mqtt_pub_topic%/DiagnosticCodeVH\", \"value_template\": \"{{ value }}\"}\0{\"avty_"
  "t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"ot"
  "gw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Diagno"
  "sticCodeVH%source_suffix%\", \"name\": \"%hostname%_DiagnosticCodeVH %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Dia"
  "gnosticCodeVH/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\""
  ": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm G"
  "ateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_configuration_system_type\", \"name\""
  ": \"%hostname%_vh_configuration_system_type\", \"stat_t\": \"%mqtt_pub_topic%/vh_configuration_system_type\"}\0{\"avty_t"
  "\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otg"
  "w-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_conf"
  "iguration_bypass\", \"name\": \"%hostname%_vh_configuration_bypass\", \"stat_t\": \"%mqtt_pub_topic%/vh_configuration_by"
  "pass\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\""
  ", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": "
  "\"%node_id%-vh_configuration_speed_control\", \"name\": \"%hostname%_vh_configuration_speed_control\", \"stat_t\": \"%mq"
  "tt_pub_topic%/vh_configuration_speed_control\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_i"
  "d%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_v"
  "ersion\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_configuration\", \"name\": \"%hostname%_vh_configuration\", \"stat"
  "_t\": \"%mqtt_pub_topic%/vh_configuration\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \""
  "dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTh"
  "erm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_memberid_code\", \"name\": \"%ho"
  "stname%_vh_memberid_code\", \"stat_t\": \"%mqtt_pub_topic%/vh_memberid_code\", \"value_template\": \"{{ value }}\"}\0{\""
  "avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\":"
  " \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-O"
  "penthermVersionVH\", \"name\": \"%hostname%_OpenthermVersionVH\", \"stat_t\": \"%mqtt_pub_topic%/OpenthermVersionVH\", "
  "\"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"man"
  "ufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-OpenthermVersionVH%source_suffix%\", \"name\": \"%hostname%_OpenthermVersionVH "
  "%source_name%\", \"stat_t\": \"%mqtt_pub_topic%/OpenthermVersionVH/%source_topic_segment%\", \"value_template\": \"{{ va"
  "lue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-VersionTypeVH_hb_u8\", \"name\": \"%hostname%_VersionTypeVH_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/Version"
  "TypeVH_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%n"
  "ode_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-VersionTypeVH_lb_u8\", \"name\": \"%hostname%_VersionTypeVH_lb_"
  "u8\", \"stat_t\": \"%mqtt_pub_topic%/VersionTypeVH_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_p"
  "ub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"n"
  "ame\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeVentilation"
  "\", \"name\": \"%hostname%_Relative_Ventilation\", \"stat_t\": \"%mqtt_pub_topic%/RelativeVentilation\", \"unit_of_measu"
  "rement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeVentilation_hb_u8\", "
  "\"name\": \"%hostname%_RelativeVentilation_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/RelativeVentilation_hb_u8\", \"unit_o"
  "f_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub"
  "_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"nam"
  "e\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeVentilation_lb"
  "_u8\", \"name\": \"%hostname%_RelativeVentilation_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/RelativeVentilation_lb_u8\", "
  "\"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%"
  "mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo"
  "\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeVenti"
  "lation%source_suffix%\", \"name\": \"%hostname%_RelativeVentilation %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Rela"
  "tiveVentilation/%source_topic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_c"
  "lass\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\":"
  " \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\""
  "}, \"uniq_id\": \"%node_id%-RelativeVentilation_hb_u8%source_suffix%\", \"name\": \"%hostname%_RelativeVentilation_hb_u8"
  " %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/RelativeVentilation_hb_u8/%source_topic_segment%\", \"unit_of_measureme"
  "nt\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeVentilation_lb_u8%source_"
  "suffix%\", \"name\": \"%hostname%_RelativeVentilation_lb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/RelativeVent"
  "ilation_lb_u8/%source_topic_segment%\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_cla"
  "ss\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": "
  "\"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}"
  ", \"uniq_id\": \"%node_id%-RelativeHumidityExhaustAir\", \"name\": \"%hostname%_Relative_Humidity_Exhaust_Air\", \"stat_"
  "t\": \"%mqtt_pub_topic%/RelativeHumidityExhaustAir\", \"device_class\": \"humidity\", \"unit_of_measurement\": \"%\", \""
  "value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ide"
  "ntifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway ("
  "%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeHumidityExhaustAir_hb_u8\", \"name\": \""
  "%hostname%_RelativeHumidityExhaustAir_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/RelativeHumidityExhaustAir_hb_u8\", \"devi"
  "ce_class\": \"humidity\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", \"state_class\": \"measur"
  "ement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-RelativeHumidityExhaustAir_lb_u8\", \"name\": \"%hostname%_RelativeHumidityExhaustAir_lb_u8\", \"stat_t\": "
  "\"%mqtt_pub_topic%/RelativeHumidityExhaustAir_lb_u8\", \"device_class\": \"humidity\", \"unit_of_measurement\": \"%\", "
  "\"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"i"
  "dentifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway"
  " (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeHumidityExhaustAir%source_suffix%\", "
  "\"name\": \"%hostname%_Relative_Humidity_Exhaust_Air %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/RelativeHumidityExh"
  "austAir/%source_topic_segment%\", \"device_class\": \"humidity\", \"unit_of_measurement\": \"%\", \"value_template\": \""
  "{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_"
  "id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_"
  "version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeHumidityExhaustAir_hb_u8%source_suffix%\", \"name\": \"%host"
  "name%_RelativeHumidityExhaustAir_hb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/RelativeHumidityExhaustAir_hb_u8/"
  "%source_topic_segment%\", \"device_class\": \"humidity\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value"
  " }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", "
  "\"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version"
  "\": \"%version%\"}, \"uniq_id\": \"%node_id%-RelativeHumidityExhaustAir_lb_u8%source_suffix%\", \"name\": \"%hostname%_R"
  "elativeHumidityExhaustAir_lb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/RelativeHumidityExhaustAir_lb_u8/%source"
  "_topic_segment%\", \"device_class\": \"humidity\", \"unit_of_measurement\": \"%\", \"value_template\": \"{{ value }}\", "
  "\"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufa"
  "cturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%v"
  "ersion%\"}, \"uniq_id\": \"%node_id%-CO2LevelExhaustAir\", \"name\": \"%hostname%_CO2_Level_Exhaust_Air\", \"stat_t\": "
  "\"%mqtt_pub_topic%/CO2LevelExhaustAir\", \"device_class\": \"carbon_dioxide\", \"unit_of_measurement\": \"ppm\", \"value"
  "_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-CO2LevelExhaustAir%source_suffix%\", \"name\": \"%hos"
  "tname%_CO2_Level_Exhaust_Air %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/CO2LevelExhaustAir/%source_topic_segment%\""
  ", \"device_class\": \"carbon_dioxide\", \"unit_of_measurement\": \"ppm\", \"value_template\": \"{{ value }}\", \"state_c"
  "lass\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\":"
  " \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\""
  "}, \"uniq_id\": \"%node_id%-SupplyInletTemperature\", \"name\": \"%hostname%_Supply_Inlet_Temperature\", \"stat_t\": \"%"
  "mqtt_pub_topic%/SupplyInletTemperature\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"v"
  "alue_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"iden"
  "tifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%"
  "hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SupplyInletTemperature%source_suffix%\", \"name\""
  ": \"%hostname%_SupplyInletTemperature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/SupplyInletTemperature/%source_top"
  "ic_segment%\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value"
  " }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", "
  "\"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version"
  "\": \"%version%\"}, \"uniq_id\": \"%node_id%-SupplyOutletTemperature\", \"name\": \"%hostname%_Supply_Outlet_Temperature"
  "\", \"stat_t\": \"%mqtt_pub_topic%/SupplyOutletTemperature\", \"device_class\": \"temperature\", \"unit_of_measurement\""
  ": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SupplyOutletTemperature%sourc"
  "e_suffix%\", \"name\": \"%hostname%_SupplyOutletTemperature %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/SupplyOutlet"
  "Temperature/%source_topic_segment%\", \"device_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value"
  "_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ExhaustInletTemperature\", \"name\": \"%hostname%_Exh"
  "aust_Inlet_Temperature\", \"stat_t\": \"%mqtt_pub_topic%/ExhaustInletTemperature\", \"device_class\": \"temperature\", "
  "\"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_"
  "t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"ot"
  "gw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Exhaus"
  "tInletTemperature%source_suffix%\", \"name\": \"%hostname%_ExhaustInletTemperature %source_name%\", \"stat_t\": \"%mqtt_"
  "pub_topic%/ExhaustInletTemperature/%source_topic_segment%\", \"device_class\": \"temperature\", \"unit_of_measurement\":"
  " \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ExhaustOutletTemperature\", "
  "\"name\": \"%hostname%_Exhaust_Outlet_Temperature\", \"stat_t\": \"%mqtt_pub_topic%/ExhaustOutletTemperature\", \"device"
  "_class\": \"temperature\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\":"
  " \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sche"
  "lte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"un"
  "iq_id\": \"%node_id%-ExhaustOutletTemperature%source_suffix%\", \"name\": \"%hostname%_ExhaustOutletTemperature %source_"
  "name%\", \"stat_t\": \"%mqtt_pub_topic%/ExhaustOutletTemperature/%source_topic_segment%\", \"device_class\": \"temperatu"
  "re\", \"unit_of_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{"
  "\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-ActualExhaustFanSpeed\", \"name\": \"%hostname%_Exhaust_Fan_Speed\", \"stat_t\": \"%mqtt_pub_topic%/ActualExhaustFanSp"
  "eed\", \"unit_of_measurement\": \"rpm\", \"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty"
  "_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"o"
  "tgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Actua"
  "lExhaustFanSpeed%source_suffix%\", \"name\": \"%hostname%_Exhaust_Fan_Speed %source_name%\", \"stat_t\": \"%mqtt_pub_top"
  "ic%/ActualExhaustFanSpeed/%source_topic_segment%\", \"unit_of_measurement\": \"rpm\", \"value_template\": \"{{ value }}"
  "\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"ma"
  "nufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-ActualSupplyFanSpeed\", \"name\": \"%hostname%_Supply_Fan_Speed\", \"stat_t\": "
  "\"%mqtt_pub_topic%/ActualSupplyFanSpeed\", \"unit_of_measurement\": \"rpm\", \"value_template\": \"{{ value }}\", \"stat"
  "e_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-ActualSupplyFanSpeed%source_suffix%\", \"name\": \"%hostname%_Supply_Fan_Speed %source_na"
  "me%\", \"stat_t\": \"%mqtt_pub_topic%/ActualSupplyFanSpeed/%source_topic_segment%\", \"unit_of_measurement\": \"rpm\", "
  "\"value_template\": \"{{ value }}\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"i"
  "dentifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway"
  " (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemoteParameterSettingVH_hb_flag8\", \"name\":"
  " \"%hostname%_RemoteParameterSettingVH_hb_flag8\", \"stat_t\": \"%mqtt_pub_topic%/RemoteParameterSettingVH_hb_flag8\", "
  "\"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"man"
  "ufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-RemoteParameterSettingVH_lb_flag8\", \"name\": \"%hostname%_RemoteParameterSett"
  "ingVH_lb_flag8\", \"stat_t\": \"%mqtt_pub_topic%/RemoteParameterSettingVH_lb_flag8\", \"value_template\": \"{{ value }}"
  "\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \""
  "model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%no"
  "de_id%-vh_rw_nominal_ventilation_value\", \"name\": \"%hostname%_vh_rw_nominal_ventilation_value\", \"stat_t\": \"%mqtt_"
  "pub_topic%/vh_rw_nominal_ventilation_value\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", "
  "\"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Open"
  "Therm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-vh_transfer_enable_nominal_ventil"
  "ation_value\", \"name\": \"%hostname%_vh_transfer_enable_nominal_ventilation_value\", \"stat_t\": \"%mqtt_pub_topic%/vh_"
  "transfer_enable_nominal_ventilation_value\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \""
  "dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTh"
  "erm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-NominalVentilationValue\", \"name\""
  ": \"%hostname%_NominalVentilationValue\", \"stat_t\": \"%mqtt_pub_topic%/NominalVentilationValue\", \"value_template\": "
  "\"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schel"
  "te Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uni"
  "q_id\": \"%node_id%-NominalVentilationValue_hb_u8\", \"name\": \"%hostname%_NominalVentilationValue_hb_u8\", \"stat_t\":"
  " \"%mqtt_pub_topic%/NominalVentilationValue_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topi"
  "c%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": "
  "\"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-NominalVentilationValue_lb_"
  "u8\", \"name\": \"%hostname%_NominalVentilationValue_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/NominalVentilationValue_lb_"
  "u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\","
  " \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_versio"
  "n\": \"%version%\"}, \"uniq_id\": \"%node_id%-NominalVentilationValue%source_suffix%\", \"name\": \"%hostname%_NominalVe"
  "ntilationValue %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/NominalVentilationValue/%source_topic_segment%\", \"value"
  "_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactur"
  "er\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versi"
  "on%\"}, \"uniq_id\": \"%node_id%-NominalVentilationValue_hb_u8%source_suffix%\", \"name\": \"%hostname%_NominalVentilati"
  "onValue_hb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/NominalVentilationValue_hb_u8/%source_topic_segment%\", \""
  "value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manuf"
  "acturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%"
  "version%\"}, \"uniq_id\": \"%node_id%-NominalVentilationValue_lb_u8%source_suffix%\", \"name\": \"%hostname%_NominalVent"
  "ilationValue_lb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/NominalVentilationValue_lb_u8/%source_topic_segment%"
  "\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", "
  "\"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version"
  "\": \"%version%\"}, \"uniq_id\": \"%node_id%-TSPNumberVH_hb_u8\", \"name\": \"%hostname%_TSPNumberVH_hb_u8\", \"stat_t\""
  ": \"%mqtt_pub_topic%/TSPNumberVH_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-TSPNumberVH_lb_u8\", \"name\": \"%host"
  "name%_TSPNumberVH_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/TSPNumberVH_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\""
  "avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\":"
  " \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-T"
  "SPEntryVH_hb_u8\", \"name\": \"%hostname%_TSPEntryVH_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/TSPEntryVH_hb_u8\", \"value"
  "_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufactur"
  "er\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versi"
  "on%\"}, \"uniq_id\": \"%node_id%-TSPEntryVH_lb_u8\", \"name\": \"%hostname%_TSPEntryVH_lb_u8\", \"stat_t\": \"%mqtt_pub_"
  "topic%/TSPEntryVH_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifi"
  "ers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%host"
  "name%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-FaultBufferSizeVH_hb_u8\", \"name\": \"%hostname%_Fau"
  "ltBufferSizeVH_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/FaultBufferSizeVH_hb_u8\", \"value_template\": \"{{ value }}\"}\0"
  "{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-FaultBufferSizeVH_lb_u8\", \"name\": \"%hostname%_FaultBufferSizeVH_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/FaultBuffe"
  "rSizeVH_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%"
  "node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-FaultBufferEntryVH_hb_u8\", \"name\": \"%hostname%_FaultBufferE"
  "ntryVH_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/FaultBufferEntryVH_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty"
  "_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"o"
  "tgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Fault"
  "BufferEntryVH_lb_u8\", \"name\": \"%hostname%_FaultBufferEntryVH_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/FaultBufferEntr"
  "yVH_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node"
  "_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw"
  "_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Brand_hb_u8\", \"name\": \"%hostname%_Brand_hb_u8\", \"stat_t\": \""
  "%mqtt_pub_topic%/Brand_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ide"
  "ntifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway ("
  "%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Brand_lb_u8\", \"name\": \"%hostname%_Brand_lb_u"
  "8\", \"stat_t\": \"%mqtt_pub_topic%/Brand_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BrandVersion_hb_u8\", \"name"
  "\": \"%hostname%_BrandVersion_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/BrandVersion_hb_u8\", \"value_template\": \"{{ val"
  "ue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-BrandVersion_lb_u8\", \"name\": \"%hostname%_BrandVersion_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/BrandVers"
  "ion_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node"
  "_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw"
  "_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BrandSerialNumber_hb_u8\", \"name\": \"%hostname%_BrandSerialNumber"
  "_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/BrandSerialNumber_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": "
  "\"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-no"
  "do\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BrandSerial"
  "Number_lb_u8\", \"name\": \"%hostname%_BrandSerialNumber_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/BrandSerialNumber_lb_u8"
  "\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", "
  "\"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version"
  "\": \"%version%\"}, \"uniq_id\": \"%node_id%-CoolingOperationHours\", \"name\": \"%hostname%_CoolingOperationHours\", \""
  "stat_t\": \"%mqtt_pub_topic%/CoolingOperationHours\", \"value_template\": \"{{ value }}\", \"unit_of_measurement\": \"h"
  "\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\","
  " \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_versio"
  "n\": \"%version%\"}, \"uniq_id\": \"%node_id%-CoolingOperationHours%source_suffix%\", \"name\": \"%hostname%_CoolingOper"
  "ationHours %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/CoolingOperationHours/%source_topic_segment%\", \"value_templ"
  "ate\": \"{{ value }}\", \"unit_of_measurement\": \"h\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub"
  "_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"nam"
  "e\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-PowerCycles\", \"name"
  "\": \"%hostname%_PowerCycles\", \"stat_t\": \"%mqtt_pub_topic%/PowerCycles\", \"value_template\": \"{{ value }}\", \"uni"
  "t_of_measurement\": \"\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identif"
  "iers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hos"
  "tname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-PowerCycles%source_suffix%\", \"name\": \"%hostname%"
  "_PowerCycles %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/PowerCycles/%source_topic_segment%\", \"value_template\": "
  "\"{{ value }}\", \"unit_of_measurement\": \"\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RFSensorStatusInformation_bat"
  "tery_indication\", \"name\": \"%hostname%_RF_Sensor_Battery\", \"stat_t\": \"%mqtt_pub_topic%/RFSensorStatusInformation_"
  "battery_indication\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\""
  ": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%"
  ")\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RFSensorStatusInformation_battery_indication_code\", \"nam"
  "e\": \"%hostname%_RF_Sensor_Battery_Code\", \"stat_t\": \"%mqtt_pub_topic%/RFSensorStatusInformation_battery_indication_"
  "code\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-RFSensorStatusInformation_sensor_index\", \"name\": \"%hostname%_RF_Sen"
  "sor_Index\", \"stat_t\": \"%mqtt_pub_topic%/RFSensorStatusInformation_sensor_index\", \"value_template\": \"{{ value }}"
  "\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \""
  "model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%no"
  "de_id%-RFSensorStatusInformation_sensor_type\", \"name\": \"%hostname%_RF_Sensor_Type\", \"stat_t\": \"%mqtt_pub_topic%/"
  "RFSensorStatusInformation_sensor_type\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RFSensorStatusInformation_sensor_type_"
  "code\", \"name\": \"%hostname%_RF_Sensor_Type_Code\", \"stat_t\": \"%mqtt_pub_topic%/RFSensorStatusInformation_sensor_ty"
  "pe_code\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_i"
  "d%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_v"
  "ersion\": \"%version%\"}, \"uniq_id\": \"%node_id%-RFSensorStatusInformation_signal_strength\", \"name\": \"%hostname%_R"
  "F_Signal_Strength\", \"stat_t\": \"%mqtt_pub_topic%/RFSensorStatusInformation_signal_strength\", \"value_template\": \"{"
  "{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte "
  "Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_i"
  "d\": \"%node_id%-RFSensorStatusInformation_signal_strength_code\", \"name\": \"%hostname%_RF_Signal_Strength_Code\", \"s"
  "tat_t\": \"%mqtt_pub_topic%/RFSensorStatusInformation_signal_strength_code\", \"value_template\": \"{{ value }}\"}\0{\"a"
  "vty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": "
  "\"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RF"
  "strengthbatterylevel_hb_u8\", \"name\": \"%hostname%_RF_Signal_Battery_HB\", \"stat_t\": \"%mqtt_pub_topic%/RFstrengthba"
  "tterylevel_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": "
  "\"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)"
  "\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RFstrengthbatterylevel_lb_u8\", \"name\": \"%hostname%_RF_S"
  "ignal_Battery_LB\", \"stat_t\": \"%mqtt_pub_topic%/RFstrengthbatterylevel_lb_u8\", \"value_template\": \"{{ value }}\"}"
  "\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mod"
  "el\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_"
  "id%-RFstrengthbatterylevel_hb_u8%source_suffix%\", \"name\": \"%hostname%_RF_Signal_Battery_HB %source_name%\", \"stat_t"
  "\": \"%mqtt_pub_topic%/RFstrengthbatterylevel_hb_u8/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"a"
  "vty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": "
  "\"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RF"
  "strengthbatterylevel_lb_u8%source_suffix%\", \"name\": \"%hostname%_RF_Signal_Battery_LB %source_name%\", \"stat_t\": \""
  "%mqtt_pub_topic%/RFstrengthbatterylevel_lb_u8/%source_topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t"
  "\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otg"
  "w-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Operati"
  "ngMode_HC1_HC2_DHW_hb_u8\", \"name\": \"%hostname%_OperatingMode_HC1_HC2_DHW_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/Ope"
  "ratingMode_HC1_HC2_DHW_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ide"
  "ntifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway ("
  "%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-OperatingMode_HC1_HC2_DHW_lb_u8\", \"name\": \"%"
  "hostname%_OperatingMode_HC1_HC2_DHW_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/OperatingMode_HC1_HC2_DHW_lb_u8\", \"value_t"
  "emplate\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-RemoteOverrideOperatingMode_dhw_mode\", \"name\": \"%hostname%_RemoteOverrideOperatingMod"
  "e_dhw_mode\", \"stat_t\": \"%mqtt_pub_topic%/RemoteOverrideOperatingMode_dhw_mode\", \"value_template\": \"{{ value }}\""
  "}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"mo"
  "del\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node"
  "_id%-RemoteOverrideOperatingMode_dhw_mode_code\", \"name\": \"%hostname%_RemoteOverrideOperatingMode_dhw_mode_code\", \""
  "stat_t\": \"%mqtt_pub_topic%/RemoteOverrideOperatingMode_dhw_mode_code\", \"value_template\": \"{{ value }}\"}\0{\"avty_"
  "t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"ot"
  "gw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-Remote"
  "OverrideOperatingMode_hc1_mode\", \"name\": \"%hostname%_RemoteOverrideOperatingMode_hc1_mode\", \"stat_t\": \"%mqtt_pub"
  "_topic%/RemoteOverrideOperatingMode_hc1_mode\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\","
  " \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"Ope"
  "nTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemoteOverrideOperatingMode_hc1_"
  "mode_code\", \"name\": \"%hostname%_RemoteOverrideOperatingMode_hc1_mode_code\", \"stat_t\": \"%mqtt_pub_topic%/RemoteOv"
  "errideOperatingMode_hc1_mode_code\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {"
  "\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gate"
  "way (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemoteOverrideOperatingMode_hc2_mode\", \"n"
  "ame\": \"%hostname%_RemoteOverrideOperatingMode_hc2_mode\", \"stat_t\": \"%mqtt_pub_topic%/RemoteOverrideOperatingMode_h"
  "c2_mode\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_i"
  "d%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_v"
  "ersion\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemoteOverrideOperatingMode_hc2_mode_code\", \"name\": \"%hostname%_R"
  "emoteOverrideOperatingMode_hc2_mode_code\", \"stat_t\": \"%mqtt_pub_topic%/RemoteOverrideOperatingMode_hc2_mode_code\", "
  "\"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"man"
  "ufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-RemoteOverrideOperatingMode_manual_dhw_push\", \"name\": \"%hostname%_RemoteOve"
  "rrideOperatingMode_manual_dhw_push\", \"stat_t\": \"%mqtt_pub_topic%/RemoteOverrideOperatingMode_manual_dhw_push\", \"va"
  "lue_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufac"
  "turer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%ve"
  "rsion%\"}, \"uniq_id\": \"%node_id%-OperatingMode_HC1_HC2_DHW_hb_u8%source_suffix%\", \"name\": \"%hostname%_OperatingMo"
  "de_HC1_HC2_DHW_hb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/OperatingMode_HC1_HC2_DHW_hb_u8/%source_topic_segme"
  "nt%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\""
  ", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_versi"
  "on\": \"%version%\"}, \"uniq_id\": \"%node_id%-OperatingMode_HC1_HC2_DHW_lb_u8%source_suffix%\", \"name\": \"%hostname%_"
  "OperatingMode_HC1_HC2_DHW_lb_u8 %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/OperatingMode_HC1_HC2_DHW_lb_u8/%source_"
  "topic_segment%\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \""
  "%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\","
  " \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-remote_override_manual_change_priority\", \"name\": \"%hostnam"
  "e%_remote_override_manual_change_priority\", \"stat_t\": \"%mqtt_pub_topic%/remote_override_manual_change_priority\"}\0{"
  "\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-remote_override_program_change_priority\", \"name\": \"%hostname%_remote_override_program_change_priority\", \"stat_t"
  "\": \"%mqtt_pub_topic%/remote_override_program_change_priority\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identi"
  "fiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%ho"
  "stname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RoomRemoteOverrideFunction_flag8\", \"name\": \"%ho"
  "stname%_RoomRemoteOverrideFunction_flag8\", \"stat_t\": \"%mqtt_pub_topic%/RoomRemoteOverrideFunction_flag8\", \"value_t"
  "emplate\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-solar_storage_slave_fault_indicator\", \"name\": \"%hostname%_solar_storage_slave_fault_i"
  "ndicator\", \"stat_t\": \"%mqtt_pub_topic%/solar_storage_slave_fault_indicator\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \""
  "dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTh"
  "erm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-solar_storage_master_mode\", \"name"
  "\": \"%hostname%_solar_storage_master_mode\", \"stat_t\": \"%mqtt_pub_topic%/solar_storage_master_mode\", \"unit_of_meas"
  "urement\": \"\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \""
  "%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\","
  " \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-solar_storage_mode_status\", \"name\": \"%hostname%_solar_stor"
  "age_mode_status\", \"stat_t\": \"%mqtt_pub_topic%/solar_storage_mode_status\", \"unit_of_measurement\": \"\", \"value_te"
  "mplate\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer"
  "\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version"
  "%\"}, \"uniq_id\": \"%node_id%-solar_storage_slave_status\", \"name\": \"%hostname%_solar_storage_slave_status\", \"stat"
  "_t\": \"%mqtt_pub_topic%/solar_storage_slave_status\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}"
  "\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", "
  "\"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%"
  "node_id%-SolarStorageASFflags_code\", \"name\": \"%hostname%_SolarStorageASFflags_code\", \"stat_t\": \"%mqtt_pub_topic%"
  "/SolarStorageASFflags_code\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ident"
  "ifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%h"
  "ostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageASFflags_flag8\", \"name\": \"%hostnam"
  "e%_SolarStorageASFflags_flag8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStorageASFflags_flag8\", \"value_template\": \"{{ v"
  "alue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-solar_storage_slave_configuration\", \"name\": \"%hostname%_solar_storage_slave_configuration\", \"stat_t"
  "\": \"%mqtt_pub_topic%/solar_storage_slave_configuration\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_p"
  "ub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"n"
  "ame\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-solar_storage_slave_"
  "memberid_code\", \"name\": \"%hostname%_solar_storage_slave_memberid_code\", \"stat_t\": \"%mqtt_pub_topic%/solar_storag"
  "e_slave_memberid_code\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifier"
  "s\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostna"
  "me%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageVersionType_hb_u8\", \"name\": \"%hostname%"
  "_SolarStorageVersionType_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStorageVersionType_hb_u8\", \"value_template\": \""
  "{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte"
  " Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_"
  "id\": \"%node_id%-SolarStorageVersionType_lb_u8\", \"name\": \"%hostname%_SolarStorageVersionType_lb_u8\", \"stat_t\": "
  "\"%mqtt_pub_topic%/SolarStorageVersionType_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic"
  "%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": "
  "\"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageTSP_hb_u8\", \""
  "name\": \"%hostname%_SolarStorageTSP_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStorageTSP_hb_u8\", \"value_template\""
  ": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sch"
  "elte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"u"
  "niq_id\": \"%node_id%-SolarStorageTSP_lb_u8\", \"name\": \"%hostname%_SolarStorageTSP_lb_u8\", \"stat_t\": \"%mqtt_pub_t"
  "opic%/SolarStorageTSP_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"iden"
  "tifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%"
  "hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageTSPindexTSPvalue_hb_u8\", \"name\": "
  "\"%hostname%_SolarStorageTSPindexTSPvalue_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStorageTSPindexTSPvalue_hb_u8\", "
  "\"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"man"
  "ufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageTSPindexTSPvalue_lb_u8\", \"name\": \"%hostname%_SolarStorageTSPind"
  "exTSPvalue_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStorageTSPindexTSPvalue_lb_u8\", \"value_template\": \"{{ value "
  "}}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", "
  "\"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%"
  "node_id%-SolarStorageFHBsize_hb_u8\", \"name\": \"%hostname%_SolarStorageFHBsize_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%"
  "/SolarStorageFHBsize_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ident"
  "ifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%h"
  "ostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageFHBsize_lb_u8\", \"name\": \"%hostname"
  "%_SolarStorageFHBsize_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStorageFHBsize_lb_u8\", \"value_template\": \"{{ valu"
  "e }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\""
  ", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": "
  "\"%node_id%-SolarStorageFHBindexFHBvalue_hb_u8\", \"name\": \"%hostname%_SolarStorageFHBindexFHBvalue_hb_u8\", \"stat_t"
  "\": \"%mqtt_pub_topic%/SolarStorageFHBindexFHBvalue_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_"
  "pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \""
  "name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SolarStorageFHBinde"
  "xFHBvalue_lb_u8\", \"name\": \"%hostname%_SolarStorageFHBindexFHBvalue_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/SolarStor"
  "ageFHBindexFHBvalue_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identi"
  "fiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%ho"
  "stname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ElectricityProducerStarts\", \"name\": \"%hostname%"
  "_ElectricityProducerStarts\", \"stat_t\": \"%mqtt_pub_topic%/ElectricityProducerStarts\", \"value_template\": \"{{ value"
  " }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-ElectricityProducerStarts%source_suffix%\", \"name\": \"%hostname%_Elec"
  "tricityProducerStarts %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/ElectricityProducerStarts/%source_topic_segment%\""
  ", \"value_template\": \"{{ value }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ElectricityProducerHours\", \"name\": "
  "\"%hostname%_ElectricityProducerHours\", \"stat_t\": \"%mqtt_pub_topic%/ElectricityProducerHours\", \"value_template\": "
  "\"{{ value }}\", \"unit_of_measurement\": \"h\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%"
  "\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \""
  "OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ElectricityProducerHours%sour"
  "ce_suffix%\", \"name\": \"%hostname%_ElectricityProducerHours %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Electricit"
  "yProducerHours/%source_topic_segment%\", \"value_template\": \"{{ value }}\", \"unit_of_measurement\": \"h\", \"state_cl"
  "ass\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacture"
  "r\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%versio"
  "n%\"}, \"uniq_id\": \"%node_id%-ElectricityProduction\", \"name\": \"%hostname%_ElectricityProduction\", \"stat_t\": \"%"
  "mqtt_pub_topic%/ElectricityProduction\", \"value_template\": \"{{ value }}\", \"device_class\": \"power\", \"unit_of_mea"
  "surement\": \"W\", \"state_class\": \"measurement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%n"
  "ode_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-ElectricityProduction%source_suffix%\", \"name\": \"%hostname%_"
  "ElectricityProduction %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/ElectricityProduction/%source_topic_segment%\", \""
  "value_template\": \"{{ value }}\", \"device_class\": \"power\", \"unit_of_measurement\": \"W\", \"state_class\": \"measu"
  "rement\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-CumulativeElectricityProduction\", \"name\": \"%hostname%_CumulativeElectricityProduction\", \"stat_t\": \""
  "%mqtt_pub_topic%/CumulativeElectricityProduction\", \"value_template\": \"{{ value }}\", \"device_class\": \"energy\", "
  "\"unit_of_measurement\": \"kWh\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {"
  "\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gate"
  "way (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-CumulativeElectricityProduction%source_suff"
  "ix%\", \"name\": \"%hostname%_CumulativeElectricityProduction %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Cumulative"
  "ElectricityProduction/%source_topic_segment%\", \"value_template\": \"{{ value }}\", \"device_class\": \"energy\", \"uni"
  "t_of_measurement\": \"kWh\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"iden"
  "tifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%"
  "hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-solar_storage_system_type\", \"name\": \"%hostnam"
  "e%_solar_storage_system_type\", \"stat_t\": \"%mqtt_pub_topic%/solar_storage_system_type\"}\0{\"avty_t\": \"%mqtt_pub_to"
  "pic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\""
  ": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BurnerUnsuccessfulStarts"
  "\", \"name\": \"%hostname%_BurnerUnsuccessfulStarts\", \"stat_t\": \"%mqtt_pub_topic%/BurnerUnsuccessfulStarts\", \"unit"
  "_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\" : \"total_increasing\" }\0{\"avty_t\": \"%"
  "mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo"
  "\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BurnerUnsucce"
  "ssfulStarts%source_suffix%\", \"name\": \"%hostname%_BurnerUnsuccessfulStarts %source_name%\", \"stat_t\": \"%mqtt_pub_t"
  "opic%/BurnerUnsuccessfulStarts/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }"
  "}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\""
  ", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_versi"
  "on\": \"%version%\"}, \"uniq_id\": \"%node_id%-FlameSignalTooLow\", \"name\": \"%hostname%_FlameSignalTooLow\", \"stat_t"
  "\": \"%mqtt_pub_topic%/FlameSignalTooLow\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_"
  "class\": \"total_increasing\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufact"
  "urer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%ver"
  "sion%\"}, \"uniq_id\": \"%node_id%-FlameSignalTooLow%source_suffix%\", \"name\": \"%hostname%_FlameSignalTooLow %source_"
  "name%\", \"stat_t\": \"%mqtt_pub_topic%/FlameSignalTooLow/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"val"
  "ue_template\": \"{{ value }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"i"
  "dentifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway"
  " (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-OEMDiagnosticCode\", \"name\": \"%hostname%_OE"
  "MDiagnosticCode\", \"stat_t\": \"%mqtt_pub_topic%/OEMDiagnosticCode\", \"unit_of_measurement\": \"\", \"value_template\""
  ": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sc"
  "helte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \""
  "uniq_id\": \"%node_id%-OEMDiagnosticCode%source_suffix%\", \"name\": \"%hostname%_OEMDiagnosticCode %source_name%\", \"s"
  "tat_t\": \"%mqtt_pub_topic%/OEMDiagnosticCode/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template"
  "\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"S"
  "chelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, "
  "\"uniq_id\": \"%node_id%-BurnerStarts\", \"name\": \"%hostname%_BurnerStarts\", \"stat_t\": \"%mqtt_pub_topic%/BurnerSta"
  "rts\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\" : \"total_increasing\" }\0{\""
  "avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\":"
  " \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-B"
  "urnerStarts%source_suffix%\", \"name\": \"%hostname%_BurnerStarts %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/Burner"
  "Starts/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\": \""
  "total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sc"
  "helte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \""
  "uniq_id\": \"%node_id%-CHPumpStarts\", \"name\": \"%hostname%_CHPumpStarts\", \"stat_t\": \"%mqtt_pub_topic%/CHPumpStart"
  "s\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\" : \"total_increasing\" }\0{\"av"
  "ty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": "
  "\"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-CH"
  "PumpStarts%source_suffix%\", \"name\": \"%hostname%_CHPumpStarts %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/CHPumpS"
  "tarts/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\": \"t"
  "otal_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Sch"
  "elte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"u"
  "niq_id\": \"%node_id%-DHWPumpValveStarts\", \"name\": \"%hostname%_DHWPumpValveStarts\", \"stat_t\": \"%mqtt_pub_topic%/"
  "DHWPumpValveStarts\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\" : \"total_incr"
  "easing\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-DHWPumpValveStarts%source_suffix%\", \"name\": \"%hostname%_DHWPumpValveStarts %source_name%\", \"stat_t\""
  ": \"%mqtt_pub_topic%/DHWPumpValveStarts/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{"
  "{ value }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%n"
  "ode_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", "
  "\"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-DHWBurnerStarts\", \"name\": \"%hostname%_DHWBurnerStarts\", \""
  "stat_t\": \"%mqtt_pub_topic%/DHWBurnerStarts\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"st"
  "ate_class\" : \"total_increasing\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"man"
  "ufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": "
  "\"%version%\"}, \"uniq_id\": \"%node_id%-DHWBurnerStarts%source_suffix%\", \"name\": \"%hostname%_DHWBurnerStarts %sourc"
  "e_name%\", \"stat_t\": \"%mqtt_pub_topic%/DHWBurnerStarts/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"val"
  "ue_template\": \"{{ value }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"i"
  "dentifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway"
  " (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BurnerOperationHours\", \"name\": \"%hostname%"
  "_BurnerOperationHours\", \"stat_t\": \"%mqtt_pub_topic%/BurnerOperationHours\", \"unit_of_measurement\": \"\", \"value_t"
  "emplate\": \"{{ value }}\", \"state_class\" : \"total_increasing\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ide"
  "ntifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway ("
  "%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-BurnerOperationHours%source_suffix%\", \"name\":"
  " \"%hostname%_BurnerOperationHours %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/BurnerOperationHours/%source_topic_se"
  "gment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\": \"total_increasing\"}\0{"
  "\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-CHPumpOperationHours\", \"name\": \"%hostname%_CHPumpOperationHoursg\", \"stat_t\": \"%mqtt_pub_topic%/CHPumpOperation"
  "Hours\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\" : \"total_increasing\" }\0{"
  "\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model"
  "\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id"
  "%-CHPumpOperationHours%source_suffix%\", \"name\": \"%hostname%_CHPumpOperationHours %source_name%\", \"stat_t\": \"%mqt"
  "t_pub_topic%/CHPumpOperationHours/%source_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ valu"
  "e }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id"
  "%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ve"
  "rsion\": \"%version%\"}, \"uniq_id\": \"%node_id%-DHWPumpValveOperationHours\", \"name\": \"%hostname%_DHWPumpValveOpera"
  "tionHours\", \"stat_t\": \"%mqtt_pub_topic%/DHWPumpValveOperationHours\", \"unit_of_measurement\": \"\", \"value_templat"
  "e\": \"{{ value }}\", \"state_class\" : \"total_increasing\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifie"
  "rs\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostn"
  "ame%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-DHWPumpValveOperationHours%source_suffix%\", \"name\":"
  " \"%hostname%_DHWPumpValveOperationHours %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/DHWPumpValveOperationHours/%sou"
  "rce_topic_segment%\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\": \"total_incre"
  "asing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron"
  "\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\":"
  " \"%node_id%-DHWBurnerOperationHours\", \"name\": \"%hostname%_DHWBurnerOperationHours DHW\", \"stat_t\": \"%mqtt_pub_to"
  "pic%/DHWBurnerOperationHours\", \"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\", \"state_class\" : \""
  "total_increasing\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"S"
  "chelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, "
  "\"uniq_id\": \"%node_id%-DHWBurnerOperationHours%source_suffix%\", \"name\": \"%hostname%_DHWBurnerOperationHours %sourc"
  "e_name%\", \"stat_t\": \"%mqtt_pub_topic%/DHWBurnerOperationHours/%source_topic_segment%\", \"unit_of_measurement\": \""
  "\", \"value_template\": \"{{ value }}\", \"state_class\": \"total_increasing\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"de"
  "v\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTher"
  "m Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-OpenThermVersionMaster\", \"name\": "
  "\"%hostname%_Master_OT_protocol_version\", \"stat_t\": \"%mqtt_pub_topic%/OpenThermVersionMaster\", \"unit_of_measuremen"
  "t\": \"\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_"
  "id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_"
  "version\": \"%version%\"}, \"uniq_id\": \"%node_id%-OpenThermVersionMaster%source_suffix%\", \"name\": \"%hostname%_Mast"
  "er_OT_protocol_version %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/OpenThermVersionMaster/%source_topic_segment%\", "
  "\"unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"iden"
  "tifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%"
  "hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-OpenThermVersionSlave\", \"name\": \"%hostname%_S"
  "lave_OT_protocol_version\", \"stat_t\": \"%mqtt_pub_topic%/OpenThermVersionSlave\", \"unit_of_measurement\": \"\", \"val"
  "ue_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufac"
  "turer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%ve"
  "rsion%\"}, \"uniq_id\": \"%node_id%-OpenThermVersionSlave%source_suffix%\", \"name\": \"%hostname%_Slave_OT_protocol_ver"
  "sion %source_name%\", \"stat_t\": \"%mqtt_pub_topic%/OpenThermVersionSlave/%source_topic_segment%\", \"unit_of_measureme"
  "nt\": \"\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_"
  "id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_"
  "version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MasterVersion_hb_u8\", \"name\": \"%hostname%_MasterVersion_hb_u8\","
  " \"stat_t\": \"%mqtt_pub_topic%/MasterVersion_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_to"
  "pic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\""
  ": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-MasterVersion_lb_u8\", \""
  "name\": \"%hostname%_MasterVersion_lb_u8\", \"stat_t\": \"%mqtt_pub_topic%/MasterVersion_lb_u8\", \"value_template\": \""
  "{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte"
  " Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_"
  "id\": \"%node_id%-SlaveVersion_hb_u8\", \"name\": \"%hostname%_SlaveVersion_hb_u8\", \"stat_t\": \"%mqtt_pub_topic%/Slav"
  "eVersion_hb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \""
  "%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\","
  " \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-SlaveVersion_lb_u8\", \"name\": \"%hostname%_SlaveVersion_lb_u"
  "8\", \"stat_t\": \"%mqtt_pub_topic%/SlaveVersion_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub"
  "_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"nam"
  "e\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemehadFdUcodes_hb_u8"
  "\", \"name\": \"%hostname%_Remeha_FD_U_Codes_HB\", \"stat_t\": \"%mqtt_pub_topic%/RemehadFdUcodes_hb_u8\", \"value_templ"
  "ate\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": "
  "\"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}"
  ", \"uniq_id\": \"%node_id%-RemehadFdUcodes_lb_u8\", \"name\": \"%hostname%_Remeha_FD_U_Codes_LB\", \"stat_t\": \"%mqtt_p"
  "ub_topic%/RemehadFdUcodes_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\""
  "identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gatewa"
  "y (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemehaServicemessage_hb_u8\", \"name\": \"%ho"
  "stname%_Remeha_Service_Message_HB\", \"stat_t\": \"%mqtt_pub_topic%/RemehaServicemessage_hb_u8\", \"value_template\": \""
  "{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte"
  " Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_"
  "id\": \"%node_id%-RemehaServicemessage_lb_u8\", \"name\": \"%hostname%_Remeha_Service_Message_LB\", \"stat_t\": \"%mqtt_"
  "pub_topic%/RemehaServicemessage_lb_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev"
  "\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm"
  " Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemehaDetectionConnectedSCU_hb_u8\", "
  "\"name\": \"%hostname%_Remeha_Detection_Connected_SCU_HB\", \"stat_t\": \"%mqtt_pub_topic%/RemehaDetectionConnectedSCU_h"
  "b_u8\", \"value_template\": \"{{ value }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%"
  "\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_ver"
  "sion\": \"%version%\"}, \"uniq_id\": \"%node_id%-RemehaDetectionConnectedSCU_lb_u8\", \"name\": \"%hostname%_Remeha_Dete"
  "ction_Connected_SCU_LB\", \"stat_t\": \"%mqtt_pub_topic%/RemehaDetectionConnectedSCU_lb_u8\", \"value_template\": \"{{ v"
  "alue }}\"}\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bro"
  "n\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\""
  ": \"%node_id%-s0pulsecount\", \"name\": \"%hostname%_S0_Pulse_Count\", \"stat_t\": \"%mqtt_pub_topic%/s0pulsecount\", \""
  "unit_of_measurement\": \"\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"ident"
  "ifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%h"
  "ostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-s0pulsecounttot\", \"name\": \"%hostname%_S0_Pulse"
  "_Count_Total\", \"stat_t\": \"%mqtt_pub_topic%/s0pulsecounttot\", \"unit_of_measurement\": \"\", \"value_template\": \"{"
  "{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers\": \"%node_id%\", \"manufacturer\": \"Schelte"
  " Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_"
  "id\": \"%node_id%-s0pulsetime\", \"name\": \"%hostname%_S0_Pulse_Time\", \"stat_t\": \"%mqtt_pub_topic%/s0pulsetime\", "
  "\"unit_of_measurement\": \"mS\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"i"
  "dentifiers\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway"
  " (%hostname%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-s0powerkw\", \"name\": \"%hostname%_S0_Power_k"
  "w\", \"stat_t\": \"%mqtt_pub_topic%/s0powerkw\", \"device_class\": \"power\",\"state_class\": \"measurement\",\"unit_of_"
  "measurement\": \"kW\", \"value_template\": \"{{ value }}\" }\0{\"avty_t\": \"%mqtt_pub_topic%\", \"dev\": {\"identifiers"
  "\": \"%node_id%\", \"manufacturer\": \"Schelte Bron\", \"model\": \"otgw-nodo\", \"name\": \"OpenTherm Gateway (%hostnam"
  "e%)\", \"sw_version\": \"%version%\"}, \"uniq_id\": \"%node_id%-%sensor_id%\", \"name\": \"%hostname%%sensor_id%\", \"st"
  "at_t\": \"%mqtt_pub_topic%/%sensor_id%\", \"device_class\": \"temperature\",\"state_class\" : \"measurement\", \"unit_of"
  "_measurement\": \"\xc2\xb0C\", \"value_template\": \"{{ value }}\" }\0"
;

// Entry table — sorted by id
const MqttHaCfgEntry PROGMEM mqttHaCfgTable[345] = {
  {0, 0x08, 0, 0},  // [0] %homeassistant%/climate/%node_id%/climate/config
  {0, 0, 49, 844},  // [1] %homeassistant%/climate/%node_id%/dhw_control/config
  {0, 0, 102, 1662},  // [2] %homeassistant%/binary_sensor/%node_id%/fault/config
  {0, 0, 155, 1947},  // [3] %homeassistant%/binary_sensor/%node_id%/centralheating/...
  {0, 0, 217, 2260},  // [4] %homeassistant%/binary_sensor/%node_id%/domestichotwate...
  {0, 0, 281, 2580},  // [5] %homeassistant%/binary_sensor/%node_id%/flame/config
  {0, 0, 334, 2865},  // [6] %homeassistant%/binary_sensor/%node_id%/cooling/config
  {0, 0, 389, 3156},  // [7] %homeassistant%/binary_sensor/%node_id%/centralheating2...
  {0, 0, 452, 3473},  // [8] %homeassistant%/binary_sensor/%node_id%/diagnostic_indi...
  {0, 0, 520, 3803},  // [9] %homeassistant%/binary_sensor/%node_id%/electric_produc...
  {0, 0, 587, 4130},  // [10] %homeassistant%/binary_sensor/%node_id%/ch_enable/confi...
  {0, 0, 644, 4440},  // [11] %homeassistant%/binary_sensor/%node_id%/dhw_enable/conf...
  {0, 0, 702, 4755},  // [12] %homeassistant%/binary_sensor/%node_id%/cooling_enable/...
  {0, 0, 764, 5067},  // [13] %homeassistant%/binary_sensor/%node_id%/otc_active/conf...
  {0, 0, 822, 5367},  // [14] %homeassistant%/binary_sensor/%node_id%/ch2_enable/conf...
  {0, 0x08, 880, 5681},  // [15] %homeassistant%/binary_sensor/%node_id%/thermostat_conn...
  {0, 0x08, 948, 6021},  // [16] %homeassistant%/binary_sensor/%node_id%/boiler_connecte...
  {0, 0, 1012, 6349},  // [17] %homeassistant%/sensor/%node_id%/status_master/config
  {0, 0, 1066, 6658},  // [18] %homeassistant%/sensor/%node_id%/status_slave/config
  {0, 0, 1119, 6964},  // [19] %homeassistant%/binary_sensor/%node_id%/dhw_blocking/co...
  {0, 0, 1179, 7270},  // [20] %homeassistant%/binary_sensor/%node_id%/summerwintertim...
  {1, 0, 1243, 7588},  // [21] %homeassistant%/sensor/%node_id%/TSet/config
  {1, 0x07, 1288, 8008},  // [22] %homeassistant%/sensor/%node_id%/TSet/%source_topic_seg...
  {2, 0, 1356, 8478},  // [23] %homeassistant%/binary_sensor/%node_id%/master_configur...
  {2, 0, 1436, 8844},  // [24] %homeassistant%/sensor/%node_id%/master_configuration/c...
  {2, 0, 1497, 9181},  // [25] %homeassistant%/sensor/%node_id%/master_memberid_code/c...
  {3, 0, 1558, 9518},  // [26] %homeassistant%/binary_sensor/%node_id%/dhw_present/con...
  {3, 0, 1617, 9821},  // [27] %homeassistant%/binary_sensor/%node_id%/control_type_mo...
  {3, 0, 1688, 10160},  // [28] %homeassistant%/binary_sensor/%node_id%/cooling_config/...
  {3, 0, 1750, 10473},  // [29] %homeassistant%/binary_sensor/%node_id%/dhw_config/conf...
  {3, 0, 1808, 10773},  // [30] %homeassistant%/binary_sensor/%node_id%/master_low_off_...
  {3, 0, 1892, 11151},  // [31] %homeassistant%/binary_sensor/%node_id%/ch2_present/con...
  {3, 0, 1951, 11454},  // [32] %homeassistant%/binary_sensor/%node_id%/remote_water_fi...
  {3, 0, 2028, 11811},  // [33] %homeassistant%/binary_sensor/%node_id%/heat_cool_mode_...
  {3, 0, 2098, 12147},  // [34] %homeassistant%/sensor/%node_id%/slave_configuration/co...
  {3, 0, 2158, 12481},  // [35] %homeassistant%/sensor/%node_id%/slave_memberid_code/co...
  {4, 0, 2218, 12815},  // [36] %homeassistant%/sensor/%node_id%/Command_hb_u8/config
  {4, 0, 2272, 13163},  // [37] %homeassistant%/sensor/%node_id%/Command_lb_u8/config
  {4, 0, 2326, 13515},  // [38] %homeassistant%/sensor/%node_id%/Command_remote_command...
  {5, 0, 2389, 13876},  // [39] %homeassistant%/binary_sensor/%node_id%/service_request...
  {5, 0, 2452, 14191},  // [40] %homeassistant%/binary_sensor/%node_id%/lockout_reset/c...
  {5, 0, 2513, 14500},  // [41] %homeassistant%/binary_sensor/%node_id%/low_water_press...
  {5, 0, 2579, 14821},  // [42] %homeassistant%/binary_sensor/%node_id%/gas_flame_fault...
  {5, 0, 2642, 15136},  // [43] %homeassistant%/binary_sensor/%node_id%/air_pressure_fa...
  {5, 0, 2708, 15457},  // [44] %homeassistant%/binary_sensor/%node_id%/water_over_temp...
  {5, 0, 2778, 15786},  // [45] %homeassistant%/sensor/%node_id%/ASF_flags/config
  {5, 0, 2828, 16161},  // [46] %homeassistant%/sensor/%node_id%/OEMFaultCode/config
  {6, 0, 2881, 16528},  // [47] %homeassistant%/binary_sensor/%node_id%/rbp_dhw_setpoin...
  {6, 0, 2945, 16846},  // [48] %homeassistant%/binary_sensor/%node_id%/rbp_max_ch_setp...
  {6, 0, 3012, 17173},  // [49] %homeassistant%/binary_sensor/%node_id%/rbp_rw_dhw_setp...
  {6, 0, 3079, 17500},  // [50] %homeassistant%/binary_sensor/%node_id%/rbp_rw_max_ch_s...
  {6, 0, 3149, 17837},  // [51] %homeassistant%/sensor/%node_id%/RBP_flags_read_write/c...
  {6, 0, 3210, 18200},  // [52] %homeassistant%/sensor/%node_id%/RBP_flags_transfer_ena...
  {7, 0, 3276, 18578},  // [53] %homeassistant%/sensor/%node_id%/CoolingControl/config
  {7, 0x07, 3331, 18991},  // [54] %homeassistant%/sensor/%node_id%/CoolingControl/%source...
  {8, 0, 3409, 19454},  // [55] %homeassistant%/sensor/%node_id%/TsetCH2/config
  {8, 0x07, 3457, 19882},  // [56] %homeassistant%/sensor/%node_id%/TsetCH2/%source_topic_...
  {9, 0, 3528, 20360},  // [57] %homeassistant%/sensor/%node_id%/TrOverride/config
  {9, 0x07, 3579, 20805},  // [58] %homeassistant%/sensor/%node_id%/TrOverride/%source_top...
  {10, 0, 3653, 21300},  // [59] %homeassistant%/sensor/%node_id%/TSP_hb_u8/config
  {10, 0, 3703, 21630},  // [60] %homeassistant%/sensor/%node_id%/TSP_lb_u8/config
  {11, 0, 3753, 21960},  // [61] %homeassistant%/sensor/%node_id%/TSPindexTSPvalue_hb_u8...
  {11, 0, 3816, 22322},  // [62] %homeassistant%/sensor/%node_id%/TSPindexTSPvalue_lb_u8...
  {12, 0, 3879, 22684},  // [63] %homeassistant%/sensor/%node_id%/FHBsize_hb_u8/config
  {12, 0, 3933, 23038},  // [64] %homeassistant%/sensor/%node_id%/FHBsize_lb_u8/config
  {13, 0, 3987, 23391},  // [65] %homeassistant%/sensor/%node_id%/FHBindexFHBvalue_hb_u8...
  {13, 0, 4050, 23757},  // [66] %homeassistant%/sensor/%node_id%/FHBindexFHBvalue_lb_u8...
  {14, 0, 4113, 24123},  // [67] %homeassistant%/sensor/%node_id%/MaxRelModLevelSetting/...
  {14, 0x07, 4175, 24560},  // [68] %homeassistant%/sensor/%node_id%/MaxRelModLevelSetting/...
  {15, 0, 4260, 25047},  // [69] %homeassistant%/sensor/%node_id%/MaxCapacityMinModLevel...
  {15, 0, 4329, 25494},  // [70] %homeassistant%/sensor/%node_id%/MaxCapacityMinModLevel...
  {16, 0, 4398, 25935},  // [71] %homeassistant%/sensor/%node_id%/TrSet/config
  {16, 0x07, 4444, 26354},  // [72] %homeassistant%/sensor/%node_id%/TrSet/%source_topic_se...
  {17, 0, 4513, 26823},  // [73] %homeassistant%/sensor/%node_id%/RelModLevel/config
  {17, 0x07, 4565, 27233},  // [74] %homeassistant%/sensor/%node_id%/RelModLevel/%source_to...
  {18, 0, 4640, 27693},  // [75] %homeassistant%/sensor/%node_id%/CHPressure/config
  {18, 0x07, 4691, 28106},  // [76] %homeassistant%/sensor/%node_id%/CHPressure/%source_top...
  {19, 0, 4765, 28597},  // [77] %homeassistant%/sensor/%node_id%/DHWFlowRate/config
  {19, 0x07, 4817, 29016},  // [78] %homeassistant%/sensor/%node_id%/DHWFlowRate/%source_to...
  {20, 0, 4892, 29485},  // [79] %homeassistant%/sensor/%node_id%/DayTime_dayofweek/conf...
  {20, 0, 4950, 29839},  // [80] %homeassistant%/sensor/%node_id%/DayTime_hour/config
  {20, 0, 5003, 30178},  // [81] %homeassistant%/sensor/%node_id%/DayTime_minutes/config
  {21, 0, 5059, 30526},  // [82] %homeassistant%/sensor/%node_id%/Date_day_of_month/conf...
  {21, 0, 5117, 30880},  // [83] %homeassistant%/sensor/%node_id%/Date_month/config
  {22, 0, 5168, 31213},  // [84] %homeassistant%/sensor/%node_id%/Year/config
  {22, 0x07, 5213, 31528},  // [85] %homeassistant%/sensor/%node_id%/Year/%source_topic_seg...
  {23, 0, 5281, 31895},  // [86] %homeassistant%/sensor/%node_id%/TrSetCH2/config
  {23, 0x07, 5330, 32322},  // [87] %homeassistant%/sensor/%node_id%/TrSetCH2/%source_topic...
  {24, 0, 5402, 32801},  // [88] %homeassistant%/sensor/%node_id%/Troom/config
  {24, 0x07, 5448, 33220},  // [89] %homeassistant%/sensor/%node_id%/Tr/%source_topic_segme...
  {25, 0, 5514, 33686},  // [90] %homeassistant%/sensor/%node_id%/Tboiler/config
  {25, 0x07, 5562, 34125},  // [91] %homeassistant%/sensor/%node_id%/Tboiler/%source_topic_...
  {26, 0, 5633, 34614},  // [92] %homeassistant%/sensor/%node_id%/Tdhw/config
  {26, 0x07, 5678, 35033},  // [93] %homeassistant%/sensor/%node_id%/Tdhw/%source_topic_seg...
  {27, 0, 5746, 35502},  // [94] %homeassistant%/sensor/%node_id%/Toutside/config
  {27, 0, 5795, 35933},  // [95] %homeassistant%/number/%node_id%/Toutside_override/conf...
  {27, 0x07, 5853, 36406},  // [96] %homeassistant%/sensor/%node_id%/Toutside/%source_topic...
  {28, 0, 5925, 36887},  // [97] %homeassistant%/sensor/%node_id%/Tret/config
  {28, 0x07, 5970, 37315},  // [98] %homeassistant%/sensor/%node_id%/Tret/%source_topic_seg...
  {29, 0, 6038, 37793},  // [99] %homeassistant%/sensor/%node_id%/Tsolarstorage/config
  {29, 0x07, 6092, 38240},  // [100] %homeassistant%/sensor/%node_id%/Tsolarstorage/%source_...
  {30, 0, 6169, 38737},  // [101] %homeassistant%/sensor/%node_id%/Tsolarcollector/config
  {30, 0x07, 6225, 39190},  // [102] %homeassistant%/sensor/%node_id%/Tsolarcollector/%sourc...
  {31, 0, 6304, 39693},  // [103] %homeassistant%/sensor/%node_id%/TflowCH2/config
  {31, 0x07, 6353, 40136},  // [104] %homeassistant%/sensor/%node_id%/TflowCH2/%source_topic...
  {32, 0, 6425, 40624},  // [105] %homeassistant%/sensor/%node_id%/Tdhw2/config
  {32, 0x07, 6471, 41062},  // [106] %homeassistant%/sensor/%node_id%/Tdhw2/%source_topic_se...
  {33, 0, 6540, 41550},  // [107] %homeassistant%/sensor/%node_id%/Texhaust/config
  {33, 0x07, 6589, 41988},  // [108] %homeassistant%/sensor/%node_id%/Texhaust/%source_topic...
  {34, 0, 6661, 42476},  // [109] %homeassistant%/sensor/%node_id%/Theatexchanger/config
  {34, 0x07, 6716, 42924},  // [110] %homeassistant%/sensor/%node_id%/Theatexchanger/%source...
  {35, 0, 6794, 43424},  // [111] %homeassistant%/sensor/%node_id%/FanSpeed_setpoint_hz/c...
  {35, 0, 6855, 43847},  // [112] %homeassistant%/sensor/%node_id%/FanSpeed_actual_hz/con...
  {36, 0, 6914, 44266},  // [113] %homeassistant%/sensor/%node_id%/ElectricalCurrentBurne...
  {36, 0x07, 6983, 44684},  // [114] %homeassistant%/sensor/%node_id%/ElectricalCurrentBurne...
  {37, 0, 7075, 45153},  // [115] %homeassistant%/sensor/%node_id%/TRoomCH2/config
  {37, 0x07, 7124, 45583},  // [116] %homeassistant%/sensor/%node_id%/TRoomCH2/%source_topic...
  {38, 0, 7196, 46065},  // [117] %homeassistant%/sensor/%node_id%/RelativeHumidity/confi...
  {38, 0x07, 7253, 46477},  // [118] %homeassistant%/sensor/%node_id%/RelativeHumidity/%sour...
  {39, 0, 7333, 46939},  // [119] %homeassistant%/sensor/%node_id%/TrOverride2/config
  {39, 0x07, 7385, 47383},  // [120] %homeassistant%/sensor/%node_id%/TrOverride2/%source_to...
  {48, 0, 7460, 47879},  // [121] %homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_val...
  {48, 0, 7528, 48325},  // [122] %homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_val...
  {48, 0x07, 7596, 48771},  // [123] %homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_val...
  {48, 0x07, 7687, 49268},  // [124] %homeassistant%/sensor/%node_id%/TdhwSetUBTdhwSetLB_val...
  {49, 0, 7778, 49765},  // [125] %homeassistant%/sensor/%node_id%/MaxTSetUBMaxTSetLB_val...
  {49, 0, 7846, 50211},  // [126] %homeassistant%/sensor/%node_id%/MaxTSetUBMaxTSetLB_val...
  {49, 0x07, 7914, 50657},  // [127] %homeassistant%/sensor/%node_id%/MaxTSetUBMaxTSetLB_val...
  {49, 0x07, 8005, 51154},  // [128] %homeassistant%/sensor/%node_id%/MaxTSetUBMaxTSetLB_val...
  {50, 0, 8096, 51651},  // [129] %homeassistant%/sensor/%node_id%/HcratioUBHcratioLB_val...
  {50, 0, 8164, 52097},  // [130] %homeassistant%/sensor/%node_id%/HcratioUBHcratioLB_val...
  {51, 0, 8232, 52543},  // [131] %homeassistant%/sensor/%node_id%/Remoteparameter4bounda...
  {51, 0x07, 8308, 52946},  // [132] %homeassistant%/sensor/%node_id%/Remoteparameter4bounda...
  {51, 0, 8407, 53401},  // [133] %homeassistant%/sensor/%node_id%/Remoteparameter4bounda...
  {51, 0x07, 8483, 53804},  // [134] %homeassistant%/sensor/%node_id%/Remoteparameter4bounda...
  {52, 0, 8582, 54259},  // [135] %homeassistant%/sensor/%node_id%/Remoteparameter5bounda...
  {52, 0x07, 8658, 54662},  // [136] %homeassistant%/sensor/%node_id%/Remoteparameter5bounda...
  {52, 0, 8757, 55117},  // [137] %homeassistant%/sensor/%node_id%/Remoteparameter5bounda...
  {52, 0x07, 8833, 55520},  // [138] %homeassistant%/sensor/%node_id%/Remoteparameter5bounda...
  {53, 0, 8932, 55975},  // [139] %homeassistant%/sensor/%node_id%/Remoteparameter6bounda...
  {53, 0x07, 9008, 56378},  // [140] %homeassistant%/sensor/%node_id%/Remoteparameter6bounda...
  {53, 0, 9107, 56833},  // [141] %homeassistant%/sensor/%node_id%/Remoteparameter6bounda...
  {53, 0x07, 9183, 57236},  // [142] %homeassistant%/sensor/%node_id%/Remoteparameter6bounda...
  {54, 0, 9282, 57691},  // [143] %homeassistant%/sensor/%node_id%/Remoteparameter7bounda...
  {54, 0x07, 9358, 58094},  // [144] %homeassistant%/sensor/%node_id%/Remoteparameter7bounda...
  {54, 0, 9457, 58549},  // [145] %homeassistant%/sensor/%node_id%/Remoteparameter7bounda...
  {54, 0x07, 9533, 58952},  // [146] %homeassistant%/sensor/%node_id%/Remoteparameter7bounda...
  {55, 0, 9632, 59407},  // [147] %homeassistant%/sensor/%node_id%/Remoteparameter8bounda...
  {55, 0x07, 9708, 59810},  // [148] %homeassistant%/sensor/%node_id%/Remoteparameter8bounda...
  {55, 0, 9807, 60265},  // [149] %homeassistant%/sensor/%node_id%/Remoteparameter8bounda...
  {55, 0x07, 9883, 60668},  // [150] %homeassistant%/sensor/%node_id%/Remoteparameter8bounda...
  {56, 0, 9982, 61123},  // [151] %homeassistant%/sensor/%node_id%/TdhwSet/config
  {56, 0x07, 10030, 61545},  // [152] %homeassistant%/sensor/%node_id%/TdhwSet/%source_topic_...
  {57, 0, 10101, 62017},  // [153] %homeassistant%/sensor/%node_id%/MaxTSet/config
  {57, 0x07, 10149, 62448},  // [154] %homeassistant%/sensor/%node_id%/MaxTSet/%source_topic_...
  {58, 0, 10220, 62929},  // [155] %homeassistant%/sensor/%node_id%/Hcratio/config
  {58, 0x07, 10268, 63359},  // [156] %homeassistant%/sensor/%node_id%/Hcratio/%source_topic_...
  {59, 0, 10339, 63839},  // [157] %homeassistant%/sensor/%node_id%/Remoteparameter4/confi...
  {59, 0x07, 10396, 64192},  // [158] %homeassistant%/sensor/%node_id%/Remoteparameter4/%sour...
  {60, 0, 10476, 64597},  // [159] %homeassistant%/sensor/%node_id%/Remoteparameter5/confi...
  {60, 0x07, 10533, 64950},  // [160] %homeassistant%/sensor/%node_id%/Remoteparameter5/%sour...
  {61, 0, 10613, 65355},  // [161] %homeassistant%/sensor/%node_id%/Remoteparameter6/confi...
  {61, 0x07, 10670, 65708},  // [162] %homeassistant%/sensor/%node_id%/Remoteparameter6/%sour...
  {62, 0, 10750, 66113},  // [163] %homeassistant%/sensor/%node_id%/Remoteparameter7/confi...
  {62, 0x07, 10807, 66466},  // [164] %homeassistant%/sensor/%node_id%/Remoteparameter7/%sour...
  {63, 0, 10887, 66871},  // [165] %homeassistant%/sensor/%node_id%/Remoteparameter8/confi...
  {63, 0x07, 10944, 67224},  // [166] %homeassistant%/sensor/%node_id%/Remoteparameter8/%sour...
  {70, 0, 11024, 67629},  // [167] %homeassistant%/binary_sensor/%node_id%/vh_ventilation_...
  {70, 0, 11094, 67965},  // [168] %homeassistant%/binary_sensor/%node_id%/vh_bypass_posit...
  {70, 0, 11160, 68289},  // [169] %homeassistant%/binary_sensor/%node_id%/vh_bypass_mode/...
  {70, 0, 11222, 68601},  // [170] %homeassistant%/binary_sensor/%node_id%/vh_free_ventila...
  {70, 0, 11294, 68943},  // [171] %homeassistant%/binary_sensor/%node_id%/vh_fault/config
  {70, 0, 11350, 69237},  // [172] %homeassistant%/binary_sensor/%node_id%/vh_ventilation_...
  {70, 0, 11417, 69564},  // [173] %homeassistant%/binary_sensor/%node_id%/vh_bypass_statu...
  {70, 0, 11481, 69882},  // [174] %homeassistant%/binary_sensor/%node_id%/vh_bypass_autom...
  {70, 0, 11555, 70230},  // [175] %homeassistant%/binary_sensor/%node_id%/vh_free_ventlia...
  {70, 0, 11629, 70578},  // [176] %homeassistant%/binary_sensor/%node_id%/vh_diagnostic_i...
  {70, 0, 11700, 70917},  // [177] %homeassistant%/sensor/%node_id%/status_vh_master/confi...
  {70, 0, 11757, 71268},  // [178] %homeassistant%/sensor/%node_id%/status_vh_slave/config
  {71, 0, 11813, 71616},  // [179] %homeassistant%/sensor/%node_id%/ControlSetpointVH/conf...
  {71, 0x07, 11871, 72045},  // [180] %homeassistant%/sensor/%node_id%/ControlSetpointVH/%sou...
  {71, 0, 11952, 72524},  // [181] %homeassistant%/sensor/%node_id%/ControlSetpointVH_hb_u...
  {71, 0, 12016, 72896},  // [182] %homeassistant%/sensor/%node_id%/ControlSetpointVH_lb_u...
  {71, 0x07, 12080, 73268},  // [183] %homeassistant%/sensor/%node_id%/ControlSetpointVH_hb_u...
  {71, 0x07, 12167, 73692},  // [184] %homeassistant%/sensor/%node_id%/ControlSetpointVH_lb_u...
  {72, 0, 12254, 74116},  // [185] %homeassistant%/sensor/%node_id%/ASFFaultCodeVH_code/co...
  {72, 0, 12314, 74476},  // [186] %homeassistant%/sensor/%node_id%/ASFFaultCodeVH_flag8/c...
  {73, 0, 12375, 74839},  // [187] %homeassistant%/sensor/%node_id%/DiagnosticCodeVH/confi...
  {73, 0x07, 12432, 75190},  // [188] %homeassistant%/sensor/%node_id%/DiagnosticCodeVH/%sour...
  {74, 0, 12512, 75593},  // [189] %homeassistant%/binary_sensor/%node_id%/vh_configuratio...
  {74, 0, 12588, 75947},  // [190] %homeassistant%/binary_sensor/%node_id%/vh_configuratio...
  {74, 0, 12659, 76286},  // [191] %homeassistant%/binary_sensor/%node_id%/vh_configuratio...
  {74, 0, 12737, 76646},  // [192] %homeassistant%/sensor/%node_id%/vh_configuration/confi...
  {74, 0, 12794, 76997},  // [193] %homeassistant%/sensor/%node_id%/vh_memberid_code/confi...
  {75, 0, 12851, 77348},  // [194] %homeassistant%/sensor/%node_id%/OpenthermVersionVH/con...
  {75, 0x07, 12910, 77705},  // [195] %homeassistant%/sensor/%node_id%/OpenthermVersionVH/%so...
  {76, 0, 12992, 78114},  // [196] %homeassistant%/sensor/%node_id%/VersionTypeVH_hb_u8/co...
  {76, 0, 13052, 78474},  // [197] %homeassistant%/sensor/%node_id%/VersionTypeVH_lb_u8/co...
  {77, 0, 13112, 78834},  // [198] %homeassistant%/sensor/%node_id%/RelativeVentilation/co...
  {77, 0, 13172, 79253},  // [199] %homeassistant%/sensor/%node_id%/RelativeVentilation_hb...
  {77, 0, 13238, 79689},  // [200] %homeassistant%/sensor/%node_id%/RelativeVentilation_lb...
  {77, 0x07, 13304, 80125},  // [201] %homeassistant%/sensor/%node_id%/RelativeVentilation/%s...
  {77, 0x07, 13387, 80595},  // [202] %homeassistant%/sensor/%node_id%/RelativeVentilation_hb...
  {77, 0x07, 13476, 81083},  // [203] %homeassistant%/sensor/%node_id%/RelativeVentilation_lb...
  {78, 0, 13565, 81571},  // [204] %homeassistant%/sensor/%node_id%/RelativeHumidityExhaus...
  {78, 0, 13632, 82041},  // [205] %homeassistant%/sensor/%node_id%/RelativeHumidityExhaus...
  {78, 0, 13705, 82526},  // [206] %homeassistant%/sensor/%node_id%/RelativeHumidityExhaus...
  {78, 0x07, 13778, 83011},  // [207] %homeassistant%/sensor/%node_id%/RelativeHumidityExhaus...
  {78, 0x07, 13868, 83533},  // [208] %homeassistant%/sensor/%node_id%/RelativeHumidityExhaus...
  {78, 0x07, 13964, 84070},  // [209] %homeassistant%/sensor/%node_id%/RelativeHumidityExhaus...
  {79, 0, 14060, 84607},  // [210] %homeassistant%/sensor/%node_id%/CO2LevelExhaustAir/con...
  {79, 0x07, 14119, 85061},  // [211] %homeassistant%/sensor/%node_id%/CO2LevelExhaustAir/%so...
  {80, 0, 14201, 85567},  // [212] %homeassistant%/sensor/%node_id%/SupplyInletTemperature...
  {80, 0x07, 14264, 86029},  // [213] %homeassistant%/sensor/%node_id%/SupplyInletTemperature...
  {81, 0, 14350, 86541},  // [214] %homeassistant%/sensor/%node_id%/SupplyOutletTemperatur...
  {81, 0x07, 14414, 87006},  // [215] %homeassistant%/sensor/%node_id%/SupplyOutletTemperatur...
  {82, 0, 14501, 87521},  // [216] %homeassistant%/sensor/%node_id%/ExhaustInletTemperatur...
  {82, 0x07, 14565, 87986},  // [217] %homeassistant%/sensor/%node_id%/ExhaustInletTemperatur...
  {83, 0, 14652, 88501},  // [218] %homeassistant%/sensor/%node_id%/ExhaustOutletTemperatu...
  {83, 0x07, 14717, 88969},  // [219] %homeassistant%/sensor/%node_id%/ExhaustOutletTemperatu...
  {84, 0, 14805, 89487},  // [220] %homeassistant%/sensor/%node_id%/ActualExhaustFanSpeed/...
  {84, 0x07, 14867, 89909},  // [221] %homeassistant%/sensor/%node_id%/ActualExhaustFanSpeed/...
  {85, 0, 14952, 90383},  // [222] %homeassistant%/sensor/%node_id%/ActualSupplyFanSpeed/c...
  {85, 0x07, 15013, 90802},  // [223] %homeassistant%/sensor/%node_id%/ActualSupplyFanSpeed/%...
  {86, 0, 15097, 91273},  // [224] %homeassistant%/sensor/%node_id%/RemoteParameterSetting...
  {86, 0, 15171, 91675},  // [225] %homeassistant%/sensor/%node_id%/RemoteParameterSetting...
  {86, 0, 15245, 92077},  // [226] %homeassistant%/sensor/%node_id%/vh_rw_nominal_ventilat...
  {86, 0, 15317, 92473},  // [227] %homeassistant%/sensor/%node_id%/vh_transfer_enable_nom...
  {87, 0, 15402, 92908},  // [228] %homeassistant%/sensor/%node_id%/NominalVentilationValu...
  {87, 0, 15466, 93280},  // [229] %homeassistant%/sensor/%node_id%/NominalVentilationValu...
  {87, 0, 15536, 93670},  // [230] %homeassistant%/sensor/%node_id%/NominalVentilationValu...
  {87, 0x07, 15606, 94060},  // [231] %homeassistant%/sensor/%node_id%/NominalVentilationValu...
  {87, 0x07, 15693, 94484},  // [232] %homeassistant%/sensor/%node_id%/NominalVentilationValu...
  {87, 0x07, 15786, 94926},  // [233] %homeassistant%/sensor/%node_id%/NominalVentilationValu...
  {88, 0, 15879, 95368},  // [234] %homeassistant%/sensor/%node_id%/TSPNumberVH_hb_u8/conf...
  {88, 0, 15937, 95722},  // [235] %homeassistant%/sensor/%node_id%/TSPNumberVH_lb_u8/conf...
  {89, 0, 15995, 96076},  // [236] %homeassistant%/sensor/%node_id%/TSPEntryVH_hb_u8/confi...
  {89, 0, 16052, 96427},  // [237] %homeassistant%/sensor/%node_id%/TSPEntryVH_lb_u8/confi...
  {90, 0, 16109, 96778},  // [238] %homeassistant%/sensor/%node_id%/FaultBufferSizeVH_hb_u...
  {90, 0, 16173, 97150},  // [239] %homeassistant%/sensor/%node_id%/FaultBufferSizeVH_lb_u...
  {91, 0, 16237, 97522},  // [240] %homeassistant%/sensor/%node_id%/FaultBufferEntryVH_hb_...
  {91, 0, 16302, 97897},  // [241] %homeassistant%/sensor/%node_id%/FaultBufferEntryVH_lb_...
  {93, 0, 16367, 98272},  // [242] %homeassistant%/sensor/%node_id%/Brand_hb_u8/config
  {93, 0, 16419, 98608},  // [243] %homeassistant%/sensor/%node_id%/Brand_lb_u8/config
  {94, 0, 16471, 98944},  // [244] %homeassistant%/sensor/%node_id%/BrandVersion_hb_u8/con...
  {94, 0, 16530, 99301},  // [245] %homeassistant%/sensor/%node_id%/BrandVersion_lb_u8/con...
  {95, 0, 16589, 99658},  // [246] %homeassistant%/sensor/%node_id%/BrandSerialNumber_hb_u...
  {95, 0, 16653, 100030},  // [247] %homeassistant%/sensor/%node_id%/BrandSerialNumber_lb_u...
  {96, 0, 16717, 100402},  // [248] %homeassistant%/sensor/%node_id%/CoolingOperationHours/...
  {96, 0x07, 16779, 100831},  // [249] %homeassistant%/sensor/%node_id%/CoolingOperationHours/...
  {97, 0, 16864, 101312},  // [250] %homeassistant%/sensor/%node_id%/PowerCycles/config
  {97, 0x07, 16916, 101710},  // [251] %homeassistant%/sensor/%node_id%/PowerCycles/%source_to...
  {98, 0, 16991, 102160},  // [252] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17076, 102568},  // [253] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17166, 102991},  // [254] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17245, 103385},  // [255] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17323, 103776},  // [256] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17406, 104182},  // [257] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17488, 104585},  // [258] %homeassistant%/sensor/%node_id%/RFSensorStatusInformat...
  {98, 0, 17575, 105003},  // [259] %homeassistant%/sensor/%node_id%/RFstrengthbatterylevel...
  {98, 0, 17644, 105382},  // [260] %homeassistant%/sensor/%node_id%/RFstrengthbatterylevel...
  {98, 0x07, 17713, 105761},  // [261] %homeassistant%/sensor/%node_id%/RFstrengthbatterylevel...
  {98, 0x07, 17805, 106192},  // [262] %homeassistant%/sensor/%node_id%/RFstrengthbatterylevel...
  {99, 0, 17897, 106623},  // [263] %homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_...
  {99, 0, 17969, 107019},  // [264] %homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_...
  {99, 0, 18041, 107415},  // [265] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0, 18118, 107826},  // [266] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0, 18200, 108252},  // [267] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0, 18277, 108663},  // [268] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0, 18359, 109089},  // [269] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0, 18436, 109500},  // [270] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0, 18518, 109926},  // [271] %homeassistant%/sensor/%node_id%/RemoteOverrideOperatin...
  {99, 0x07, 18602, 110358},  // [272] %homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_...
  {99, 0x07, 18697, 110806},  // [273] %homeassistant%/sensor/%node_id%/OperatingMode_HC1_HC2_...
  {100, 0, 18792, 111254},  // [274] %homeassistant%/binary_sensor/%node_id%/remote_override...
  {100, 0, 18878, 111638},  // [275] %homeassistant%/binary_sensor/%node_id%/remote_override...
  {100, 0, 18965, 112025},  // [276] %homeassistant%/sensor/%node_id%/RoomRemoteOverrideFunc...
  {101, 0, 19038, 112424},  // [277] %homeassistant%/binary_sensor/%node_id%/solar_storage_s...
  {101, 0, 19121, 112799},  // [278] %homeassistant%/sensor/%node_id%/solar_storage_master_m...
  {101, 0, 19187, 113205},  // [279] %homeassistant%/sensor/%node_id%/solar_storage_mode_sta...
  {101, 0, 19253, 113611},  // [280] %homeassistant%/sensor/%node_id%/solar_storage_slave_st...
  {102, 0, 19320, 114020},  // [281] %homeassistant%/sensor/%node_id%/SolarStorageASFflags_c...
  {102, 0, 19386, 114398},  // [282] %homeassistant%/sensor/%node_id%/SolarStorageASFflags_f...
  {103, 0, 19453, 114779},  // [283] %homeassistant%/sensor/%node_id%/solar_storage_slave_co...
  {103, 0, 19527, 115181},  // [284] %homeassistant%/sensor/%node_id%/solar_storage_slave_me...
  {104, 0, 19601, 115583},  // [285] %homeassistant%/sensor/%node_id%/SolarStorageVersionTyp...
  {104, 0, 19671, 115973},  // [286] %homeassistant%/sensor/%node_id%/SolarStorageVersionTyp...
  {105, 0, 19741, 116363},  // [287] %homeassistant%/sensor/%node_id%/SolarStorageTSP_hb_u8/...
  {105, 0, 19803, 116729},  // [288] %homeassistant%/sensor/%node_id%/SolarStorageTSP_lb_u8/...
  {106, 0, 19865, 117095},  // [289] %homeassistant%/sensor/%node_id%/SolarStorageTSPindexTS...
  {106, 0, 19940, 117500},  // [290] %homeassistant%/sensor/%node_id%/SolarStorageTSPindexTS...
  {107, 0, 20015, 117905},  // [291] %homeassistant%/sensor/%node_id%/SolarStorageFHBsize_hb...
  {107, 0, 20081, 118283},  // [292] %homeassistant%/sensor/%node_id%/SolarStorageFHBsize_lb...
  {108, 0, 20147, 118661},  // [293] %homeassistant%/sensor/%node_id%/SolarStorageFHBindexFH...
  {108, 0, 20222, 119066},  // [294] %homeassistant%/sensor/%node_id%/SolarStorageFHBindexFH...
  {109, 0, 20297, 119471},  // [295] %homeassistant%/sensor/%node_id%/ElectricityProducerSta...
  {109, 0x07, 20363, 119884},  // [296] %homeassistant%/sensor/%node_id%/ElectricityProducerSta...
  {110, 0, 20452, 120349},  // [297] %homeassistant%/sensor/%node_id%/ElectricityProducerHou...
  {110, 0x07, 20517, 120787},  // [298] %homeassistant%/sensor/%node_id%/ElectricityProducerHou...
  {111, 0, 20605, 121277},  // [299] %homeassistant%/sensor/%node_id%/ElectricityProduction/...
  {111, 0x07, 20667, 121726},  // [300] %homeassistant%/sensor/%node_id%/ElectricityProduction/...
  {112, 0, 20752, 122227},  // [301] %homeassistant%/sensor/%node_id%/CumulativeElectricityP...
  {112, 0x07, 20824, 122714},  // [302] %homeassistant%/sensor/%node_id%/CumulativeElectricityP...
  {113, 0, 20919, 123253},  // [303] %homeassistant%/binary_sensor/%node_id%/solar_storage_s...
  {113, 0, 20992, 123598},  // [304] %homeassistant%/sensor/%node_id%/BurnerUnsuccessfulStar...
  {113, 0x07, 21057, 124037},  // [305] %homeassistant%/sensor/%node_id%/BurnerUnsuccessfulStar...
  {114, 0, 21145, 124526},  // [306] %homeassistant%/sensor/%node_id%/FlameSignalTooLow/conf...
  {114, 0x07, 21203, 124943},  // [307] %homeassistant%/sensor/%node_id%/FlameSignalTooLow/%sou...
  {115, 0, 21284, 125411},  // [308] %homeassistant%/sensor/%node_id%/OEMDiagnosticCode/conf...
  {115, 0x07, 21342, 125793},  // [309] %homeassistant%/sensor/%node_id%/OEMDiagnosticCode/%sou...
  {116, 0, 21423, 126226},  // [310] %homeassistant%/sensor/%node_id%/BurnerStarts/config
  {116, 0x07, 21476, 126629},  // [311] %homeassistant%/sensor/%node_id%/BurnerStarts/%source_t...
  {117, 0, 21552, 127082},  // [312] %homeassistant%/sensor/%node_id%/CHPumpStarts/config
  {117, 0x07, 21605, 127485},  // [313] %homeassistant%/sensor/%node_id%/CHPumpStarts/%source_t...
  {118, 0, 21681, 127938},  // [314] %homeassistant%/sensor/%node_id%/DHWPumpValveStarts/con...
  {118, 0x07, 21740, 128359},  // [315] %homeassistant%/sensor/%node_id%/DHWPumpValveStarts/%so...
  {119, 0, 21822, 128830},  // [316] %homeassistant%/sensor/%node_id%/DHWBurnerStarts/config
  {119, 0x07, 21878, 129242},  // [317] %homeassistant%/sensor/%node_id%/DHWBurnerStarts/%sourc...
  {120, 0, 21957, 129704},  // [318] %homeassistant%/sensor/%node_id%/BurnerOperationHours/c...
  {120, 0x07, 22018, 130131},  // [319] %homeassistant%/sensor/%node_id%/BurnerOperationHours/%...
  {121, 0, 22102, 130608},  // [320] %homeassistant%/sensor/%node_id%/CHPumpOperationHours/c...
  {121, 0x07, 22163, 131036},  // [321] %homeassistant%/sensor/%node_id%/CHPumpOperationHours/%...
  {122, 0, 22247, 131513},  // [322] %homeassistant%/sensor/%node_id%/DHWPumpValveOperationH...
  {122, 0x07, 22314, 131958},  // [323] %homeassistant%/sensor/%node_id%/DHWPumpValveOperationH...
  {123, 0, 22404, 132453},  // [324] %homeassistant%/sensor/%node_id%/DHWBurnerOperationHour...
  {123, 0x07, 22468, 132893},  // [325] %homeassistant%/sensor/%node_id%/DHWBurnerOperationHour...
  {124, 0, 22555, 133379},  // [326] %homeassistant%/sensor/%node_id%/OpenThermVersionMaster...
  {124, 0x07, 22618, 133780},  // [327] %homeassistant%/sensor/%node_id%/OpenThermVersionMaster...
  {125, 0, 22704, 134232},  // [328] %homeassistant%/sensor/%node_id%/OpenThermVersionSlave/...
  {125, 0x07, 22766, 134630},  // [329] %homeassistant%/sensor/%node_id%/OpenThermVersionSlave/...
  {126, 0, 22851, 135079},  // [330] %homeassistant%/sensor/%node_id%/MasterVersion_hb_u8/co...
  {126, 0, 22911, 135439},  // [331] %homeassistant%/sensor/%node_id%/MasterVersion_lb_u8/co...
  {127, 0, 22971, 135799},  // [332] %homeassistant%/sensor/%node_id%/SlaveVersion_hb_u8/con...
  {127, 0, 23030, 136156},  // [333] %homeassistant%/sensor/%node_id%/SlaveVersion_lb_u8/con...
  {131, 0, 23089, 136513},  // [334] %homeassistant%/sensor/%node_id%/RemehadFdUcodes_hb_u8/...
  {131, 0, 23151, 136878},  // [335] %homeassistant%/sensor/%node_id%/RemehadFdUcodes_lb_u8/...
  {132, 0, 23213, 137243},  // [336] %homeassistant%/sensor/%node_id%/RemehaServicemessage_h...
  {132, 0, 23280, 137623},  // [337] %homeassistant%/sensor/%node_id%/RemehaServicemessage_l...
  {133, 0, 23347, 138003},  // [338] %homeassistant%/sensor/%node_id%/RemehaDetectionConnect...
  {133, 0, 23421, 138405},  // [339] %homeassistant%/sensor/%node_id%/RemehaDetectionConnect...
  {245, 0, 23495, 138807},  // [340] %homeassistant%/sensor/%node_id%/s0pulsecount/config
  {245, 0, 23548, 139176},  // [341] %homeassistant%/sensor/%node_id%/s0pulsecounttot/config
  {245, 0, 23604, 139557},  // [342] %homeassistant%/sensor/%node_id%/s0pulsetime/config
  {245, 0, 23656, 139925},  // [343] %homeassistant%/sensor/%node_id%/s0powerkw/config
  {246, 0, 23706, 140340},  // [344] %homeassistant%/sensor/%node_id%/%sensor_id%/config
};

// ID -> first table index (0xFFFF = not present)
const uint16_t PROGMEM mqttHaCfgIndex[256] = {
  0, // id 0, 21 entries at 0
  21, // id 1, 2 entries at 21
  23, // id 2, 3 entries at 23
  26, // id 3, 10 entries at 26
  36, // id 4, 3 entries at 36
  39, // id 5, 8 entries at 39
  47, // id 6, 6 entries at 47
  53, // id 7, 2 entries at 53
  55, // id 8, 2 entries at 55
  57, // id 9, 2 entries at 57
  59, // id 10, 2 entries at 59
  61, // id 11, 2 entries at 61
  63, // id 12, 2 entries at 63
  65, // id 13, 2 entries at 65
  67, // id 14, 2 entries at 67
  69, // id 15, 2 entries at 69
  71, // id 16, 2 entries at 71
  73, // id 17, 2 entries at 73
  75, // id 18, 2 entries at 75
  77, // id 19, 2 entries at 77
  79, // id 20, 3 entries at 79
  82, // id 21, 2 entries at 82
  84, // id 22, 2 entries at 84
  86, // id 23, 2 entries at 86
  88, // id 24, 2 entries at 88
  90, // id 25, 2 entries at 90
  92, // id 26, 2 entries at 92
  94, // id 27, 3 entries at 94
  97, // id 28, 2 entries at 97
  99, // id 29, 2 entries at 99
  101, // id 30, 2 entries at 101
  103, // id 31, 2 entries at 103
  105, // id 32, 2 entries at 105
  107, // id 33, 2 entries at 107
  109, // id 34, 2 entries at 109
  111, // id 35, 2 entries at 111
  113, // id 36, 2 entries at 113
  115, // id 37, 2 entries at 115
  117, // id 38, 2 entries at 117
  119, // id 39, 2 entries at 119
  0xFFFF, // id 40 - not present
  0xFFFF, // id 41 - not present
  0xFFFF, // id 42 - not present
  0xFFFF, // id 43 - not present
  0xFFFF, // id 44 - not present
  0xFFFF, // id 45 - not present
  0xFFFF, // id 46 - not present
  0xFFFF, // id 47 - not present
  121, // id 48, 4 entries at 121
  125, // id 49, 4 entries at 125
  129, // id 50, 2 entries at 129
  131, // id 51, 4 entries at 131
  135, // id 52, 4 entries at 135
  139, // id 53, 4 entries at 139
  143, // id 54, 4 entries at 143
  147, // id 55, 4 entries at 147
  151, // id 56, 2 entries at 151
  153, // id 57, 2 entries at 153
  155, // id 58, 2 entries at 155
  157, // id 59, 2 entries at 157
  159, // id 60, 2 entries at 159
  161, // id 61, 2 entries at 161
  163, // id 62, 2 entries at 163
  165, // id 63, 2 entries at 165
  0xFFFF, // id 64 - not present
  0xFFFF, // id 65 - not present
  0xFFFF, // id 66 - not present
  0xFFFF, // id 67 - not present
  0xFFFF, // id 68 - not present
  0xFFFF, // id 69 - not present
  167, // id 70, 12 entries at 167
  179, // id 71, 6 entries at 179
  185, // id 72, 2 entries at 185
  187, // id 73, 2 entries at 187
  189, // id 74, 5 entries at 189
  194, // id 75, 2 entries at 194
  196, // id 76, 2 entries at 196
  198, // id 77, 6 entries at 198
  204, // id 78, 6 entries at 204
  210, // id 79, 2 entries at 210
  212, // id 80, 2 entries at 212
  214, // id 81, 2 entries at 214
  216, // id 82, 2 entries at 216
  218, // id 83, 2 entries at 218
  220, // id 84, 2 entries at 220
  222, // id 85, 2 entries at 222
  224, // id 86, 4 entries at 224
  228, // id 87, 6 entries at 228
  234, // id 88, 2 entries at 234
  236, // id 89, 2 entries at 236
  238, // id 90, 2 entries at 238
  240, // id 91, 2 entries at 240
  0xFFFF, // id 92 - not present
  242, // id 93, 2 entries at 242
  244, // id 94, 2 entries at 244
  246, // id 95, 2 entries at 246
  248, // id 96, 2 entries at 248
  250, // id 97, 2 entries at 250
  252, // id 98, 11 entries at 252
  263, // id 99, 11 entries at 263
  274, // id 100, 3 entries at 274
  277, // id 101, 4 entries at 277
  281, // id 102, 2 entries at 281
  283, // id 103, 2 entries at 283
  285, // id 104, 2 entries at 285
  287, // id 105, 2 entries at 287
  289, // id 106, 2 entries at 289
  291, // id 107, 2 entries at 291
  293, // id 108, 2 entries at 293
  295, // id 109, 2 entries at 295
  297, // id 110, 2 entries at 297
  299, // id 111, 2 entries at 299
  301, // id 112, 2 entries at 301
  303, // id 113, 3 entries at 303
  306, // id 114, 2 entries at 306
  308, // id 115, 2 entries at 308
  310, // id 116, 2 entries at 310
  312, // id 117, 2 entries at 312
  314, // id 118, 2 entries at 314
  316, // id 119, 2 entries at 316
  318, // id 120, 2 entries at 318
  320, // id 121, 2 entries at 320
  322, // id 122, 2 entries at 322
  324, // id 123, 2 entries at 324
  326, // id 124, 2 entries at 326
  328, // id 125, 2 entries at 328
  330, // id 126, 2 entries at 330
  332, // id 127, 2 entries at 332
  0xFFFF, // id 128 - not present
  0xFFFF, // id 129 - not present
  0xFFFF, // id 130 - not present
  334, // id 131, 2 entries at 334
  336, // id 132, 2 entries at 336
  338, // id 133, 2 entries at 338
  0xFFFF, // id 134 - not present
  0xFFFF, // id 135 - not present
  0xFFFF, // id 136 - not present
  0xFFFF, // id 137 - not present
  0xFFFF, // id 138 - not present
  0xFFFF, // id 139 - not present
  0xFFFF, // id 140 - not present
  0xFFFF, // id 141 - not present
  0xFFFF, // id 142 - not present
  0xFFFF, // id 143 - not present
  0xFFFF, // id 144 - not present
  0xFFFF, // id 145 - not present
  0xFFFF, // id 146 - not present
  0xFFFF, // id 147 - not present
  0xFFFF, // id 148 - not present
  0xFFFF, // id 149 - not present
  0xFFFF, // id 150 - not present
  0xFFFF, // id 151 - not present
  0xFFFF, // id 152 - not present
  0xFFFF, // id 153 - not present
  0xFFFF, // id 154 - not present
  0xFFFF, // id 155 - not present
  0xFFFF, // id 156 - not present
  0xFFFF, // id 157 - not present
  0xFFFF, // id 158 - not present
  0xFFFF, // id 159 - not present
  0xFFFF, // id 160 - not present
  0xFFFF, // id 161 - not present
  0xFFFF, // id 162 - not present
  0xFFFF, // id 163 - not present
  0xFFFF, // id 164 - not present
  0xFFFF, // id 165 - not present
  0xFFFF, // id 166 - not present
  0xFFFF, // id 167 - not present
  0xFFFF, // id 168 - not present
  0xFFFF, // id 169 - not present
  0xFFFF, // id 170 - not present
  0xFFFF, // id 171 - not present
  0xFFFF, // id 172 - not present
  0xFFFF, // id 173 - not present
  0xFFFF, // id 174 - not present
  0xFFFF, // id 175 - not present
  0xFFFF, // id 176 - not present
  0xFFFF, // id 177 - not present
  0xFFFF, // id 178 - not present
  0xFFFF, // id 179 - not present
  0xFFFF, // id 180 - not present
  0xFFFF, // id 181 - not present
  0xFFFF, // id 182 - not present
  0xFFFF, // id 183 - not present
  0xFFFF, // id 184 - not present
  0xFFFF, // id 185 - not present
  0xFFFF, // id 186 - not present
  0xFFFF, // id 187 - not present
  0xFFFF, // id 188 - not present
  0xFFFF, // id 189 - not present
  0xFFFF, // id 190 - not present
  0xFFFF, // id 191 - not present
  0xFFFF, // id 192 - not present
  0xFFFF, // id 193 - not present
  0xFFFF, // id 194 - not present
  0xFFFF, // id 195 - not present
  0xFFFF, // id 196 - not present
  0xFFFF, // id 197 - not present
  0xFFFF, // id 198 - not present
  0xFFFF, // id 199 - not present
  0xFFFF, // id 200 - not present
  0xFFFF, // id 201 - not present
  0xFFFF, // id 202 - not present
  0xFFFF, // id 203 - not present
  0xFFFF, // id 204 - not present
  0xFFFF, // id 205 - not present
  0xFFFF, // id 206 - not present
  0xFFFF, // id 207 - not present
  0xFFFF, // id 208 - not present
  0xFFFF, // id 209 - not present
  0xFFFF, // id 210 - not present
  0xFFFF, // id 211 - not present
  0xFFFF, // id 212 - not present
  0xFFFF, // id 213 - not present
  0xFFFF, // id 214 - not present
  0xFFFF, // id 215 - not present
  0xFFFF, // id 216 - not present
  0xFFFF, // id 217 - not present
  0xFFFF, // id 218 - not present
  0xFFFF, // id 219 - not present
  0xFFFF, // id 220 - not present
  0xFFFF, // id 221 - not present
  0xFFFF, // id 222 - not present
  0xFFFF, // id 223 - not present
  0xFFFF, // id 224 - not present
  0xFFFF, // id 225 - not present
  0xFFFF, // id 226 - not present
  0xFFFF, // id 227 - not present
  0xFFFF, // id 228 - not present
  0xFFFF, // id 229 - not present
  0xFFFF, // id 230 - not present
  0xFFFF, // id 231 - not present
  0xFFFF, // id 232 - not present
  0xFFFF, // id 233 - not present
  0xFFFF, // id 234 - not present
  0xFFFF, // id 235 - not present
  0xFFFF, // id 236 - not present
  0xFFFF, // id 237 - not present
  0xFFFF, // id 238 - not present
  0xFFFF, // id 239 - not present
  0xFFFF, // id 240 - not present
  0xFFFF, // id 241 - not present
  0xFFFF, // id 242 - not present
  0xFFFF, // id 243 - not present
  0xFFFF, // id 244 - not present
  340, // id 245, 4 entries at 340
  344, // id 246, 1 entry at 344
  0xFFFF, // id 247 - not present
  0xFFFF, // id 248 - not present
  0xFFFF, // id 249 - not present
  0xFFFF, // id 250 - not present
  0xFFFF, // id 251 - not present
  0xFFFF, // id 252 - not present
  0xFFFF, // id 253 - not present
  0xFFFF, // id 254 - not present
  0xFFFF // id 255 - not present
};
