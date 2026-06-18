#!/usr/bin/env python
"""One-shot helper (TASK-881): generate type-accurate OpenAPI properties for the
flat /v2/sat/status object from a live device dump. Not wired into the build."""
import json, sys

plain = json.load(open(sys.argv[1]))

DESC = {
 "enabled": "SAT control loop enabled",
 "active": "SAT actively driving the boiler setpoint",
 "control_mode": "Control mode (0=off, 1=continuous, 2=PWM)",
 "boiler_status": "Boiler status label (off/idle/preheating/at_setpoint/modulating/etc.)",
 "target_temp": "Target room temperature (C)",
 "room_temp": "Room temperature used by the PID (C); null when no sensor",
 "outside_temp": "Outside temperature used by the heating curve (C)",
 "heating_curve": "Heating-curve flow temperature (C)",
 "pid_output": "PID output = curve + P + I + D (C)",
 "final_setpoint": "Final flow setpoint sent to the boiler (C)",
 "error": "PID error = target - room (C)",
 "pid_p": "Proportional PID term (C)", "pid_i": "Integral PID term (C)", "pid_d": "Derivative PID term (C)",
 "kp": "Active Kp gain", "ki": "Active Ki gain", "kd": "Active Kd gain",
 "raw_derivative": "Raw (unfiltered) derivative of the error",
 "coefficient": "Heating-curve coefficient (steepness)",
 "deadband": "PID deadband (C) around the target",
 "overshoot_margin": "Allowed overshoot above target before a cycle is classed as overshoot (C)",
 "cycle_count": "Total boiler cycles counted",
 "cycles_this_hour": "Boiler cycles in the trailing hour",
 "last_cycle_class": "Last cycle classification code (0=none,1=good,2=overshoot,3=underheat,4=short,5=uncertain)",
 "cycle_max_flow": "Peak flow temperature in the current/last cycle (C)",
 "cycle_overshoot_sec": "Seconds spent above target in the current/last cycle",
 "duty_ratio": "Flame-on duty ratio (0.0-1.0)",
 "overshoot_fraction": "Fraction of cycle time spent in overshoot (0.0-1.0)",
 "underheat_fraction": "Fraction of cycle time spent underheating (0.0-1.0)",
 "cycle_phase": "Current cycle phase label (idle/heating/cooling/etc.)",
 "phase_duration_sec": "Seconds elapsed in the current cycle phase",
 "pwm_duty": "PWM duty cycle (0.0-1.0)",
 "pwm_flame_req": "PWM currently requesting flame on",
 "active_preset": "Active preset index",
 "mod_suppressed": "Modulation suppression active",
 "dhw_active": "Domestic hot water draw active",
 "dhw_setpoint": "DHW setpoint (C)",
 "dhw_config_tank": "DHW configured as a storage tank",
 "dhw_enable": "DHW enabled",
 "control_interval_sec": "Control loop interval (s)",
 "fallback_active": "Fallback control active (sensor/comms loss)",
 "fallback_reason": "Fallback reason code",
 "max_rel_modulation": "Maximum relative modulation allowed (%)",
 "current_modulation": "Current relative modulation (%)",
 "ovp_value": "Overshoot-protection learned value (C)",
 "ovp_enabled": "Overshoot protection enabled",
 "ovp_calib_phase": "OVP calibration phase",
 "ovp_calib_max_temp": "Max temperature seen during OVP calibration (C)",
 "ovp_calib_samples": "OVP calibration sample count",
 "heating_system": "Configured heating-system type code (0=auto)",
 "heating_system_detected": "Detected heating-system type code",
 "manufacturer": "Boiler manufacturer label",
 "manufacturer_setting": "Configured manufacturer code (0=auto)",
 "manufacturer_detected": "Detected manufacturer (slave member) ID",
 "slave_memberid": "OpenTherm slave member ID",
 "max_setpoint_system": "System maximum flow setpoint (C)",
 "external_temp_valid": "External room temperature input valid",
 "external_outdoor_valid": "External outdoor temperature input valid",
 "pv_surplus_w": "Reported PV surplus (W)",
 "pv_surplus_valid": "PV surplus reading valid",
 "pv_boost_active": "PV boost currently applied",
 "pv_boost_applied_c": "PV boost added to setpoint (C)",
 "pv_boost_enabled": "PV boost feature enabled",
 "safety_tripped": "Safety shutdown active",
 "valves_open": "Zone valves reported open",
 "window_open": "Open-window detected",
 "window_detection": "Open-window detection enabled",
 "push_setpoint": "Setpoint pushed to boiler this cycle",
 "flame_off_offset": "Flame-off setpoint offset (C)",
 "force_pwm": "Force PWM mode regardless of conditions",
 "flow_offset": "Flow-temperature offset applied (C)",
 "pressure": "System water pressure (bar)",
 "pressure_drop_rate": "Pressure drop rate (bar/min)",
 "pressure_alarm": "Low/high pressure alarm active",
 "modulation_reliable": "Modulation feedback considered reliable",
 "setpoint_mismatch": "Boiler flow setpoint differs from commanded",
 "curve_recommendation": "Heating-curve tuning recommendation (insufficient/raise/lower/ok)",
 "heating_curve_recommendation": "Heating-curve recommendation (duplicate of curve_recommendation)",
 "mean_error": "Mean PID error over the sample window (C)",
 "error_stddev": "PID error standard deviation (C)",
 "target_temp_step": "Target temperature adjustment step (C)",
 "power_kw": "Estimated boiler power output (kW)",
 "energy_kwh": "Estimated cumulative energy (kWh)",
 "boiler_capacity": "Configured boiler capacity (kW)",
 "boiler_rated_kw": "Rated boiler power (kW)",
 "boiler_efficiency": "Assumed boiler efficiency (0.0-1.0)",
 "energy_estimated_kwh": "Model-estimated energy (kWh)",
 "preset_sync": "Preset synchronised with thermostat",
 "thermal_coeff": "Thermal model coefficient",
 "thermal_drop_rate": "Modelled room temperature drop rate (C/min)",
 "thermal_model_valid": "Thermal model has enough data to be valid",
 "estimated_room": "Model-estimated room temperature (C)",
 "last_known_room": "Last known room temperature (C)",
 "solar_gain_active": "Solar-gain compensation active",
 "indoor_rise_rate": "Observed indoor temperature rise rate (C/min)",
 "summer_simmer": "Summer simmer mode active",
 "summer_active": "Summer mode active",
 "summer_hours_above": "Hours outdoor temp stayed above the summer threshold",
 "summer_threshold": "Outdoor temperature threshold for summer mode (C)",
 "summer_min_hours": "Minimum hours above threshold to enter summer mode",
 "comfort_adjust": "Humidity comfort adjustment active",
 "humidity": "Relative humidity (%)",
 "humidity_valid": "Humidity reading valid",
 "comfort_offset": "Applied comfort setpoint offset (C)",
 "comfort_ref_humidity": "Reference humidity for comfort adjustment (%)",
 "comfort_max_offset": "Maximum comfort offset (C)",
 "simulation": "Simulation mode active",
 "sim_available": "Simulation mode available on this build",
 "auto_tune": "Auto-tune enabled",
 "auto_tune_active": "Auto-tune currently running",
 "auto_tune_cycles": "Auto-tune cycles completed",
 "auto_tune_score": "Auto-tune fit score",
 "auto_tune_rate": "Auto-tune learning rate",
 "sensor_max_age": "Maximum sensor age before stale (s)",
 "error_monitoring": "Error monitoring enabled",
 "auto_gains_value": "Auto-gains aggressiveness value",
 "auto_gains": "Automatic PID gain scheduling enabled",
 "kp_manual": "Manual Kp gain (used when auto_gains off)",
 "ki_manual": "Manual Ki gain", "kd_manual": "Manual Kd gain",
 "thermal_comfort": "Thermal-comfort mode active",
 "heating_mode": "Heating mode label (comfort/eco/etc.)",
 "cycles_per_hour": "Target maximum boiler cycles per hour",
 "valve_offset": "Valve compensation offset (C)",
 "solar_freeze_integral": "Freeze the PID integral during solar gain",
 "multi_area": "Multi-area control enabled",
 "multi_area_count": "Number of configured areas",
 "ble_enable": "BLE thermostat input enabled",
 "ble_failover": "BLE failover enabled",
 "ble_failover_active": "BLE failover currently active",
 "ble_temp_valid": "BLE temperature reading valid",
 "ble_sensor_count": "Number of BLE sensors seen",
}

def yaml_type(v):
    if isinstance(v, bool): return "boolean"
    if isinstance(v, int): return "integer"
    if isinstance(v, float): return "number"
    if isinstance(v, str): return "string"
    return None

lines = []
for k, v in plain.items():
    t = yaml_type(v)
    d = DESC.get(k, k.replace('_', ' ').capitalize())
    lines.append("        %s:" % k)
    if t is None:
        lines.append("          type: number")
        lines.append("          nullable: true")
    else:
        lines.append("          type: %s" % t)
    lines.append('          description: "%s"' % d.replace('"', '\\"'))

open(sys.argv[2], 'w').write("\n".join(lines) + "\n")
sys.stderr.write("wrote %d props\n" % len(plain))
