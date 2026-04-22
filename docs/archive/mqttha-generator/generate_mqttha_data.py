#!/usr/bin/env python3
"""
Generate mqtt_configuratie.cpp from mqttha.cfg.
Run from the repo root: python tools/generate_mqttha_data.py

Parses the mqttha.cfg discovery config file and produces a structured .cpp
file with separate PROGMEM arrays for sensors, binary sensors, climate and
number entities. Variable fields (device_class, unit, state_class, icon,
entity_category) are encoded as enum values to save flash and RAM.

Generated file:
  src/OTGW-firmware/mqtt_configuratie.cpp

This replaces the older generate_mqttha_progmem.py and generate_mqttha_readable.py.
"""

import json
import os
import re
import sys
from collections import OrderedDict
from datetime import datetime, timezone

# ---- Path setup ------------------------------------------------------------ #

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
SKETCH_DIR = os.path.join(REPO_ROOT, "src", "OTGW-firmware")

INPUT_FILE = os.path.join(SKETCH_DIR, "data", "mqttha.cfg")
ICONS_CFG_FILE = os.path.join(SKETCH_DIR, "data", "mqttha_icons.cfg")
CPP_FILE = os.path.join(SKETCH_DIR, "mqtt_configuratie.cpp")

# Flag bit definitions
FLAG_HAS_SOURCE_SUFFIX        = 0x01
FLAG_HAS_SOURCE_NAME          = 0x02
FLAG_HAS_SOURCE_TOPIC_SEGMENT = 0x04
FLAG_IS_PIC_ENTRY             = 0x08

# Diagnostic OT IDs
DIAGNOSTIC_IDS = {2, 3, 5, 6, 10, 11, 12, 13, 15}

# Disabled-by-default OT ID ranges
VH_IDS = set(range(70, 92))        # 70-91
SOLAR_IDS = set(range(101, 109))   # 101-108
REMEHA_IDS = {131, 132, 133}

# Diagnostic label patterns (case-insensitive check)
DIAGNOSTIC_LABEL_PATTERNS = ['asf', 'oem', 'config', 'version', 'tsp', 'fhb', 'rbp', 'brand']

# Disabled-by-default label patterns
DISABLED_LABEL_PATTERNS = ['ch2']


# ---- Enums ----------------------------------------------------------------- #

DEVICE_CLASSES = [
    'none', 'temperature', 'pressure', 'humidity', 'power', 'power_factor',
    'energy', 'carbon_dioxide',
]

UNITS = [
    'none', 'degC', 'bar', 'percent', 'l_min', 'kW', 'W', 'kWh',
    'uA', 'Hz', 'rpm', 'ppm', 'mS', 'h',
]

STATE_CLASSES = [
    'none', 'measurement', 'total_increasing',
]

ICONS = [
    # Must match HaIcon enum in MQTTstuff.h -- do NOT reorder existing entries
    'none', 'thermometer', 'gauge', 'water_percent', 'flash',
    'angle_acute', 'lightning_bolt', 'molecule_co2', 'percent_outline',
    'timer_outline', 'counter', 'information_outline', 'fan',
    'current_ac', 'clock_outline', 'pulse',
    'alert_circle', 'fire', 'radiator', 'water_boiler', 'snowflake',
    'information', 'toggle_switch', 'lan_connect', 'checkbox_marked_circle',
    # Climate / number (existing in enum)
    'thermostat_icon', 'thermometer_lines',
    # New icons from mqttha_icons.cfg
    'air_filter', 'alert_outline', 'antenna', 'arrow_expand_horizontal',
    'calendar', 'card_account_details', 'cog', 'console',
    'format_list_numbered', 'history', 'list_status', 'remote',
    'solar_panel', 'speedometer', 'tag', 'tune_variant', 'water',
]

ENTITY_CATS = [
    'none', 'diagnostic',
]

# Maps from source data strings to enum names
DEVICE_CLASS_MAP = {
    '': 'none', 'temperature': 'temperature', 'pressure': 'pressure',
    'humidity': 'humidity', 'power': 'power', 'power_factor': 'power_factor',
    'energy': 'energy', 'carbon_dioxide': 'carbon_dioxide',
}

UNIT_MAP = {
    '': 'none', '\u00b0C': 'degC', 'bar': 'bar', '%': 'percent',
    'l/min': 'l_min', 'kW': 'kW', 'W': 'W', 'kWh': 'kWh',
    '\u00b5A': 'uA', 'Hz': 'Hz', 'rpm': 'rpm', 'ppm': 'ppm',
    'mS': 'mS', 'h': 'h',
}

STATE_CLASS_MAP = {
    '': 'none', 'measurement': 'measurement', 'total_increasing': 'total_increasing',
}

# device_class -> icon for sensors
DC_TO_ICON_SENSOR = {
    'temperature': 'thermometer',
    'pressure': 'gauge',
    'humidity': 'water_percent',
    'power': 'flash',
    'power_factor': 'angle_acute',
    'energy': 'lightning_bolt',
    'carbon_dioxide': 'molecule_co2',
}

# Icon strings for the enum-to-string function
# Returns the mdi icon name WITHOUT the "mdi:" prefix -- the streaming code
# in the hand-written section adds the "mdi:" prefix when writing the JSON.
ICON_STRINGS = {
    'none': None,
    'thermometer': 'thermometer',
    'gauge': 'gauge',
    'water_percent': 'water-percent',
    'flash': 'flash',
    'angle_acute': 'angle-acute',
    'lightning_bolt': 'lightning-bolt',
    'molecule_co2': 'molecule-co2',
    'percent_outline': 'percent-outline',
    'timer_outline': 'timer-outline',
    'counter': 'counter',
    'information_outline': 'information-outline',
    'fan': 'fan',
    'current_ac': 'current-ac',
    'clock_outline': 'clock-outline',
    'pulse': 'pulse',
    'alert_circle': 'alert-circle',
    'fire': 'fire',
    'radiator': 'radiator',
    'water_boiler': 'water-boiler',
    'snowflake': 'snowflake',
    'information': 'information',
    'toggle_switch': 'toggle-switch',
    'lan_connect': 'lan-connect',
    'checkbox_marked_circle': 'checkbox-marked-circle',
    'thermostat_icon': 'thermostat',
    'thermometer_lines': 'thermometer-lines',
    # New icons from mqttha_icons.cfg
    'air_filter': 'air-filter',
    'alert_outline': 'alert-outline',
    'antenna': 'antenna',
    'arrow_expand_horizontal': 'arrow-expand-horizontal',
    'calendar': 'calendar',
    'card_account_details': 'card-account-details',
    'cog': 'cog',
    'console': 'console',
    'format_list_numbered': 'format-list-numbered',
    'history': 'history',
    'list_status': 'list-status',
    'remote': 'remote',
    'solar_panel': 'solar-panel',
    'speedometer': 'speedometer',
    'tag': 'tag',
    'tune_variant': 'tune-variant',
    'water': 'water',
}

# Unit strings for the enum-to-string function
UNIT_STRINGS = {
    'none': None,
    'degC': '\u00b0C',
    'bar': 'bar',
    'percent': '%',
    'l_min': 'l/min',
    'kW': 'kW',
    'W': 'W',
    'kWh': 'kWh',
    'uA': '\u00b5A',
    'Hz': 'Hz',
    'rpm': 'rpm',
    'ppm': 'ppm',
    'mS': 'mS',
    'h': 'h',
}


# ---- Icon config alias (mdi hyphenated -> enum underscore) ----------------- #

# Maps mdi icon names (hyphenated) to enum names (underscored).
# Special cases where the cfg name differs from the enum name.
ICON_CFG_ALIASES = {
    'thermostat': 'thermostat_icon',  # cfg uses "thermostat", enum uses "thermostat_icon"
}


def _mdi_to_enum(mdi_name: str) -> str:
    """Convert an mdi icon name (hyphenated) to the Python/C++ enum name (underscored).
    Applies aliases for special cases."""
    enum_name = mdi_name.replace('-', '_')
    return ICON_CFG_ALIASES.get(enum_name, enum_name)


def load_icon_overrides(path: str) -> dict:
    """Load icon overrides from mqttha_icons.cfg.

    Returns a dict mapping label -> icon_enum_name.
    Empty labels (for climate/number entries) are stored as empty string key.
    Handles comments, blank lines, and the `label ; icon-name // comment` format.
    """
    overrides = {}
    if not os.path.isfile(path):
        return overrides  # No override file = use heuristics only

    with open(path, encoding='utf-8') as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip('\n').rstrip('\r')
            stripped = line.strip()
            if not stripped or stripped.startswith('//'):
                continue
            # Strip trailing // comment
            comment_pos = line.find('//')
            if comment_pos >= 0:
                line = line[:comment_pos]
            parts = line.split(';', 1)
            if len(parts) != 2:
                continue
            label = parts[0].strip()
            icon_mdi = parts[1].strip()
            if not icon_mdi:
                continue
            enum_name = _mdi_to_enum(icon_mdi)
            if enum_name not in ICONS:
                print(f'  WARNING: icon "{icon_mdi}" (enum: {enum_name}) at line {lineno} '
                      f'not in ICONS list -- skipping', file=sys.stderr)
                continue
            overrides[label] = enum_name
            # Also store with otgw-pic/ prefix stripped, since extract_label
            # strips it when processing mqttha.cfg entries
            stripped = re.sub(r'^otgw-pic/', '', label)
            if stripped != label:
                overrides[stripped] = enum_name

    print(f'  Loaded {len(overrides)} icon overrides from {os.path.basename(path)}.')
    return overrides


# Global icon overrides dict, loaded in main()
_icon_overrides: dict = {}


# ---- Helpers --------------------------------------------------------------- #

def c_escape(s: str) -> str:
    """Escape a string for use inside C double quotes."""
    result = []
    for ch in s:
        if ch == '\\':   result.append('\\\\')
        elif ch == '"':  result.append('\\"')
        elif ch == '\r': result.append('\\r')
        elif ch == '\n': result.append('\\n')
        else:            result.append(ch)
    return ''.join(result)


def to_c_ident(s: str) -> str:
    """Convert a string to a valid C identifier fragment (lowercase)."""
    s = s.lower()
    s = re.sub(r'[^a-z0-9_]', '_', s)
    s = re.sub(r'_+', '_', s).strip('_')
    return s or 'unknown'


def compute_flags(topic: str, msg: str) -> int:
    """Compute pre-determined flags from topic and message content."""
    flags = 0
    combined = topic + msg
    if '%source_suffix%' in combined:        flags |= FLAG_HAS_SOURCE_SUFFIX
    if '%source_name%' in combined:          flags |= FLAG_HAS_SOURCE_NAME
    if '%source_topic_segment%' in combined: flags |= FLAG_HAS_SOURCE_TOPIC_SEGMENT
    if 'otgw-pic/' in topic or 'otgw-pic/' in msg: flags |= FLAG_IS_PIC_ENTRY
    return flags


# ---- Parse input ----------------------------------------------------------- #

def parse_config(path: str):
    """Parse mqttha.cfg into list of (id, topic, msg) tuples."""
    entries = []
    with open(path, encoding='utf-8') as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip('\n').rstrip('\r')
            stripped = line.strip()
            if not stripped or stripped.startswith('//'):
                continue
            parts = line.split(';', 2)
            if len(parts) != 3:
                print(f'WARNING line {lineno}: expected 3 fields -- skipping', file=sys.stderr)
                continue
            raw_id, raw_topic, raw_msg = parts
            try:
                ot_id = int(raw_id.strip())
            except ValueError:
                print(f'WARNING line {lineno}: non-integer ID -- skipping', file=sys.stderr)
                continue
            if not (0 <= ot_id <= 255):
                print(f'WARNING line {lineno}: ID {ot_id} out of range -- skipping', file=sys.stderr)
                continue
            entries.append((ot_id, raw_topic.strip(), raw_msg.strip()))
    return entries


# ---- Classify entries ------------------------------------------------------ #

def get_entity_type(topic: str) -> str:
    """Extract HA entity type from topic path."""
    # After %homeassistant%/ the next segment is the entity type
    m = re.search(r'%homeassistant%/(\w+)/', topic)
    return m.group(1) if m else 'unknown'


def extract_label(msg_json: dict, topic: str) -> str:
    """Extract label from stat_t field, stripping prefix."""
    stat_t = msg_json.get('stat_t', '')
    # Strip %mqtt_pub_topic%/ prefix
    label = re.sub(r'^%mqtt_pub_topic%/', '', stat_t)
    # Strip /%source_topic_segment% suffix for source variants
    label = re.sub(r'/%source_topic_segment%$', '', label)
    # Strip otgw-pic/ prefix for PIC entries
    label = re.sub(r'^otgw-pic/', '', label)
    return label


def extract_friendly_name(msg_json: dict) -> str:
    """Extract friendly name, stripping %hostname%_ prefix."""
    name = msg_json.get('name', '')
    name = re.sub(r'^%hostname%_?', '', name)
    # Strip trailing source placeholder
    name = re.sub(r'\s*%source_name%$', '', name)
    return name


def determine_sensor_icon(device_class: str, label: str, unit: str) -> str:
    """Determine icon for a sensor entry.
    First checks icon overrides from mqttha_icons.cfg, then falls back to heuristics."""
    # Check icon override first (from mqttha_icons.cfg if it exists)
    if label in _icon_overrides:
        return _icon_overrides[label]
    # By device_class
    if device_class and device_class in DC_TO_ICON_SENSOR:
        return DC_TO_ICON_SENSOR[device_class]
    # Comprehensive label-based heuristics
    ll = label.lower()
    # Status/flags
    if 'status' in ll and ('master' in ll or 'slave' in ll or 'vh' in ll):
        return 'list_status'
    # Config/version/brand
    if 'config' in ll or 'configuration' in ll:
        return 'cog'
    if 'memberid' in ll or 'member_id' in ll:
        return 'card_account_details'
    if 'version' in ll:
        return 'tag'
    if 'brand' in ll and 'serial' not in ll:
        return 'tag'
    if 'serial' in ll:
        return 'tag'
    # Faults/diagnostics
    if 'fault' in ll or 'asf' in ll:
        return 'alert_outline'
    if 'oem' in ll and ('fault' in ll or 'diagnostic' in ll):
        return 'alert_outline'
    if 'oemdiagnostic' in ll:
        return 'alert_outline'
    # Commands
    if 'command' in ll:
        return 'console'
    # Remote parameters/override
    if 'rbp' in ll or 'remoteoverride' in ll:
        return 'remote'
    if 'remoteparameter' in ll and 'boundaries' in ll:
        return 'arrow_expand_horizontal'
    if 'remoteparameter' in ll:
        return 'tune_variant'
    # TSP/FHB
    if 'tsp' in ll:
        return 'format_list_numbered'
    if 'fhb' in ll:
        return 'history'
    # Capacity/modulation
    if 'maxcapacity' in ll or 'maxrelmod' in ll:
        return 'speedometer'
    # Counters/hours
    if 'operationhours' in ll:
        return 'timer_outline'
    if 'starts' in ll or 'cycles' in ll:
        return 'counter'
    if 'unsuccessful' in ll or 'toolow' in ll:
        return 'alert_outline'
    # Physical quantities
    if 'pressure' in ll:
        return 'gauge'
    if 'flow' in ll:
        return 'water'
    # Time/date
    if 'date' in ll or 'day' in ll:
        return 'calendar'
    if 'year' in ll:
        return 'calendar'
    if 'time' in ll or 'daytime' in ll:
        return 'clock_outline'
    # Solar
    if 'solar' in ll:
        return 'solar_panel'
    # Ventilation/heat recovery
    if 'vh' in ll or 'ventilation' in ll or 'exhaust' in ll or 'supply' in ll:
        return 'air_filter'
    # Fan
    if 'fan' in ll:
        return 'fan'
    # RF/wireless
    if 'rf' in ll and ('sensor' in ll or 'strength' in ll):
        return 'antenna'
    # S0 pulse
    if 's0pulse' in ll:
        return 'pulse'
    # Electricity
    if 'electricity' in ll or 'electric' in ll:
        return 'lightning_bolt'
    # Heating ratio/mode
    if 'hcratio' in ll:
        return 'thermostat_icon'
    if 'mode' in ll and 'operating' in ll:
        return 'thermostat_icon'
    # Current
    if 'current' in ll and 'burner' in ll:
        return 'current_ac'
    # Flame
    if 'flame' in ll:
        return 'fire'
    # Unit-based fallbacks
    if unit == '%':
        return 'percent_outline'
    if unit in ('h', 'hours'):
        return 'timer_outline'
    return 'information_outline'


def determine_binsensor_icon(label: str) -> str:
    """Determine icon for a binary sensor entry.
    First checks icon overrides from mqttha_icons.cfg, then falls back to heuristics."""
    # Check icon override first
    if label in _icon_overrides:
        return _icon_overrides[label]
    ll = label.lower()
    if 'fault' in ll:
        return 'alert_circle'
    if 'flame' in ll:
        return 'fire'
    if 'centralheating' in ll or 'ch_enable' in ll:
        return 'radiator'
    if 'domestichotwater' in ll or 'dhw' in ll:
        return 'water_boiler'
    if 'cooling' in ll:
        return 'snowflake'
    if 'diagnostic' in ll or 'otc' in ll:
        return 'information'
    if 'connected' in ll:
        return 'lan_connect'
    if 'enable' in ll or 'active' in ll:
        return 'toggle_switch'
    return 'checkbox_marked_circle'


def determine_entity_cat(ot_id: int, label: str) -> str:
    """Determine entity category."""
    if ot_id in DIAGNOSTIC_IDS:
        return 'diagnostic'
    ll = label.lower()
    for pat in DIAGNOSTIC_LABEL_PATTERNS:
        if pat in ll:
            return 'diagnostic'
    return 'none'


def determine_enabled_by_default(ot_id: int, label: str) -> bool:
    """Determine if entity should be enabled by default."""
    if ot_id in VH_IDS or ot_id in SOLAR_IDS or ot_id in REMEHA_IDS:
        return False
    ll = label.lower()
    for pat in DISABLED_LABEL_PATTERNS:
        if pat in ll:
            return False
    return True


# ---- Process entries into typed lists -------------------------------------- #

class SensorEntry:
    def __init__(self, ot_id, flags, label, friendly_name, device_class, unit,
                 state_class, icon, entity_cat, enabled_by_default):
        self.ot_id = ot_id
        self.flags = flags
        self.label = label
        self.friendly_name = friendly_name
        self.device_class = device_class
        self.unit = unit
        self.state_class = state_class
        self.icon = icon
        self.entity_cat = entity_cat
        self.enabled_by_default = enabled_by_default


class BinSensorEntry:
    def __init__(self, ot_id, flags, label, friendly_name, icon, entity_cat, enabled_by_default):
        self.ot_id = ot_id
        self.flags = flags
        self.label = label
        self.friendly_name = friendly_name
        self.icon = icon
        self.entity_cat = entity_cat
        self.enabled_by_default = enabled_by_default


class SpecialEntry:
    """Climate or number entry -- kept as full JSON PROGMEM string."""
    def __init__(self, ot_id, entity_type, topic, msg, flags):
        self.ot_id = ot_id
        self.entity_type = entity_type
        self.topic = topic
        self.msg = msg
        self.flags = flags


def process_entries(raw_entries):
    """Classify and extract fields from all raw entries."""
    sensors = []
    bin_sensors = []
    specials = []

    for ot_id, topic, msg_str in raw_entries:
        entity_type = get_entity_type(topic)
        flags = compute_flags(topic, msg_str)

        if entity_type in ('climate', 'number'):
            specials.append(SpecialEntry(ot_id, entity_type, topic, msg_str, flags))
            continue

        try:
            msg_json = json.loads(msg_str)
        except (json.JSONDecodeError, ValueError):
            print(f'WARNING: could not parse JSON for id {ot_id} -- skipping', file=sys.stderr)
            continue

        label = extract_label(msg_json, topic)
        friendly_name = extract_friendly_name(msg_json)

        if entity_type == 'binary_sensor':
            icon = determine_binsensor_icon(label)
            entity_cat = determine_entity_cat(ot_id, label)
            enabled = determine_enabled_by_default(ot_id, label)
            bin_sensors.append(BinSensorEntry(ot_id, flags, label, friendly_name,
                                              icon, entity_cat, enabled))
        elif entity_type == 'sensor':
            dc = msg_json.get('device_class', '')
            unit_raw = msg_json.get('unit_of_measurement', '')
            sc = msg_json.get('state_class', '')
            icon = determine_sensor_icon(dc, label, unit_raw)
            entity_cat = determine_entity_cat(ot_id, label)
            enabled = determine_enabled_by_default(ot_id, label)

            dc_enum = DEVICE_CLASS_MAP.get(dc, 'none')
            unit_enum = UNIT_MAP.get(unit_raw, 'none')
            sc_enum = STATE_CLASS_MAP.get(sc, 'none')

            if dc_enum == 'none' and dc:
                print(f'WARNING: unknown device_class "{dc}" for label {label}', file=sys.stderr)
            if unit_enum == 'none' and unit_raw:
                print(f'WARNING: unknown unit "{unit_raw}" for label {label}', file=sys.stderr)

            sensors.append(SensorEntry(ot_id, flags, label, friendly_name,
                                       dc_enum, unit_enum, sc_enum, icon,
                                       entity_cat, enabled))
        else:
            print(f'WARNING: unknown entity type "{entity_type}" for id {ot_id}', file=sys.stderr)

    # Sort by id, then by flags (non-source first)
    sensors.sort(key=lambda e: (e.ot_id, e.flags, e.label))
    bin_sensors.sort(key=lambda e: (e.ot_id, e.flags, e.label))

    return sensors, bin_sensors, specials


# ---- Deduplicate PROGMEM strings ------------------------------------------ #

def collect_progmem_strings(sensors, bin_sensors):
    """Collect and deduplicate all label and name strings.
    Returns (labels_dict, names_dict) mapping string -> C variable name."""
    labels = OrderedDict()  # label_str -> c_var_name
    names = OrderedDict()   # name_str -> c_var_name

    all_entries = list(sensors) + list(bin_sensors)

    for e in all_entries:
        if e.label not in labels:
            labels[e.label] = f'ha_lbl_{to_c_ident(e.label)}'
        if e.friendly_name not in names:
            names[e.friendly_name] = f'ha_name_{to_c_ident(e.friendly_name)}'

    # Resolve any name collisions by appending a suffix
    seen_vars = {}
    for k, v in labels.items():
        if v in seen_vars:
            # Collision -- append a number
            i = 2
            while f'{v}_{i}' in seen_vars.values():
                i += 1
            labels[k] = f'{v}_{i}'
        seen_vars[labels[k]] = k

    seen_vars = {}
    for k, v in names.items():
        if v in seen_vars:
            i = 2
            while f'{v}_{i}' in seen_vars.values():
                i += 1
            names[k] = f'{v}_{i}'
        seen_vars[names[k]] = k

    return labels, names


# ---- Build index arrays ---------------------------------------------------- #

def build_index(sorted_entries, count):
    """Build OT ID -> first entry index array."""
    index = [0xFFFF] * 256
    for pos, e in enumerate(sorted_entries):
        if index[e.ot_id] == 0xFFFF:
            index[e.ot_id] = pos
    return index


# ---- Generate .cpp --------------------------------------------------------- #

def generate_cpp(sensors, bin_sensors, specials, labels, names, output_path, timestamp):
    lines = []
    sensor_count = len(sensors)
    binsensor_count = len(bin_sensors)
    unique_sensor_ids = len({e.ot_id for e in sensors})
    unique_binsensor_ids = len({e.ot_id for e in bin_sensors})

    lines.append(f"""\
// AUTO-GENERATED from mqttha.cfg by tools/generate_mqttha_data.py
// DO NOT EDIT -- regenerate with: python tools/generate_mqttha_data.py
// Generated: {timestamp}
//
// Sensors        : {sensor_count} entries ({unique_sensor_ids} unique OT IDs)
// Binary sensors : {binsensor_count} entries ({unique_binsensor_ids} unique OT IDs)
// Climate        : {sum(1 for s in specials if s.entity_type == 'climate')} entries
// Number         : {sum(1 for s in specials if s.entity_type == 'number')} entries
//
// Total PROGMEM strings: {len(labels)} labels + {len(names)} friendly names

#include "MQTTstuff.h"
""")

    # ========== Named PROGMEM strings: Labels ==========
    lines.append('// ========== Named PROGMEM strings: Labels ==========')
    for label_str, var_name in labels.items():
        lines.append(f'const char {var_name}[] PROGMEM = "{c_escape(label_str)}";')
    lines.append('')

    # ========== Named PROGMEM strings: Friendly names ==========
    lines.append('// ========== Named PROGMEM strings: Friendly names ==========')
    for name_str, var_name in names.items():
        lines.append(f'const char {var_name}[] PROGMEM = "{c_escape(name_str)}";')
    lines.append('')

    # ========== Sensor array ==========
    lines.append(f'// ========== Sensor array ({sensor_count} entries, sorted by id) ==========')
    lines.append(f'const uint16_t MQTT_HA_SENSOR_COUNT = {sensor_count};')
    lines.append('')
    lines.append('const MqttHaSensorCfg PROGMEM mqttHaSensors[] = {')
    lines.append('//  {id, flags, label, friendlyName, deviceClass, unit, stateClass, icon, entityCat, enabledByDefault}')

    current_id = -1
    for e in sensors:
        if e.ot_id != current_id:
            current_id = e.ot_id
            lines.append(f'    // --- OT ID {e.ot_id} ---')
        lbl_var = labels[e.label]
        name_var = names[e.friendly_name]
        flag_str = f'0x{e.flags:02X}' if e.flags else '0x00'
        enabled_str = 'true' if e.enabled_by_default else 'false'
        lines.append(
            f'    {{{e.ot_id:3d}, {flag_str}, {lbl_var}, {name_var}, '
            f'HaDeviceClass::{e.device_class}, HaUnit::{e.unit}, '
            f'HaStateClass::{e.state_class}, HaIcon::{e.icon}, '
            f'HaEntityCat::{e.entity_cat}, {enabled_str}}},'
        )

    lines.append('};')
    lines.append('')

    # ========== Binary sensor array ==========
    lines.append(f'// ========== Binary sensor array ({binsensor_count} entries, sorted by id) ==========')
    lines.append(f'const uint16_t MQTT_HA_BINSENSOR_COUNT = {binsensor_count};')
    lines.append('')
    lines.append('const MqttHaBinSensorCfg PROGMEM mqttHaBinSensors[] = {')
    lines.append('//  {id, flags, label, friendlyName, icon, entityCat, enabledByDefault}')

    current_id = -1
    for e in bin_sensors:
        if e.ot_id != current_id:
            current_id = e.ot_id
            lines.append(f'    // --- OT ID {e.ot_id} ---')
        lbl_var = labels[e.label]
        name_var = names[e.friendly_name]
        flag_str = f'0x{e.flags:02X}' if e.flags else '0x00'
        enabled_str = 'true' if e.enabled_by_default else 'false'
        lines.append(
            f'    {{{e.ot_id:3d}, {flag_str}, {lbl_var}, {name_var}, '
            f'HaIcon::{e.icon}, HaEntityCat::{e.entity_cat}, {enabled_str}}},'
        )

    lines.append('};')
    lines.append('')

    # ========== Index arrays ==========
    sensor_index = build_index(sensors, sensor_count)
    binsensor_index = build_index(bin_sensors, binsensor_count)

    lines.append('// ========== Index arrays (OT ID -> first entry) ==========')
    lines.append('const uint16_t PROGMEM mqttHaSensorIndex[256] = {')
    for i in range(256):
        val = sensor_index[i]
        hex_val = '0xFFFF' if val == 0xFFFF else str(val)
        comma = ',' if i < 255 else ''
        if val != 0xFFFF:
            cnt = sum(1 for e in sensors if e.ot_id == i)
            lines.append(f'    {hex_val}{comma} // id {i}, {cnt} entr{"y" if cnt == 1 else "ies"}')
        else:
            lines.append(f'    {hex_val}{comma} // id {i}')
    lines.append('};')
    lines.append('')

    lines.append('const uint16_t PROGMEM mqttHaBinSensorIndex[256] = {')
    for i in range(256):
        val = binsensor_index[i]
        hex_val = '0xFFFF' if val == 0xFFFF else str(val)
        comma = ',' if i < 255 else ''
        if val != 0xFFFF:
            cnt = sum(1 for e in bin_sensors if e.ot_id == i)
            lines.append(f'    {hex_val}{comma} // id {i}, {cnt} entr{"y" if cnt == 1 else "ies"}')
        else:
            lines.append(f'    {hex_val}{comma} // id {i}')
    lines.append('};')
    lines.append('')

    # Climate and Number entries are handled by streaming functions
    # (streamClimateDiscovery, streamNumberDiscovery) in the hand-written section.
    # No static PROGMEM templates generated for these.
    if specials:
        lines.append(f'// Climate ({sum(1 for s in specials if s.entity_type == "climate")}) and '
                     f'Number ({sum(1 for s in specials if s.entity_type == "number")}) entries '
                     f'are handled by streaming functions below.')
        lines.append('')

    # ========== Enum-to-string lookup functions ==========
    lines.append('// ========== Enum-to-string lookup functions ==========')
    lines.append('')

    # haDeviceClassStr
    lines.append('PGM_P haDeviceClassStr(HaDeviceClass dc) {')
    lines.append('    switch (dc) {')
    for dc in DEVICE_CLASSES:
        if dc == 'none':
            lines.append(f'        case HaDeviceClass::{dc}: return nullptr;')
        else:
            lines.append(f'        case HaDeviceClass::{dc}: {{ static const char s[] PROGMEM = "{dc}"; return s; }}')
    lines.append('        default: return nullptr;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # haUnitStr
    lines.append('PGM_P haUnitStr(HaUnit u) {')
    lines.append('    switch (u) {')
    for unit in UNITS:
        s = UNIT_STRINGS.get(unit)
        if s is None:
            lines.append(f'        case HaUnit::{unit}: return nullptr;')
        else:
            lines.append(f'        case HaUnit::{unit}: {{ static const char s[] PROGMEM = "{c_escape(s)}"; return s; }}')
    lines.append('        default: return nullptr;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # haStateClassStr
    lines.append('PGM_P haStateClassStr(HaStateClass sc) {')
    lines.append('    switch (sc) {')
    for sc in STATE_CLASSES:
        if sc == 'none':
            lines.append(f'        case HaStateClass::{sc}: return nullptr;')
        else:
            lines.append(f'        case HaStateClass::{sc}: {{ static const char s[] PROGMEM = "{sc}"; return s; }}')
    lines.append('        default: return nullptr;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # haIconStr
    lines.append('PGM_P haIconStr(HaIcon ic) {')
    lines.append('    switch (ic) {')
    for icon in ICONS:
        s = ICON_STRINGS.get(icon)
        if s is None:
            lines.append(f'        case HaIcon::{icon}: return nullptr;')
        else:
            lines.append(f'        case HaIcon::{icon}: {{ static const char s[] PROGMEM = "{c_escape(s)}"; return s; }}')
    lines.append('        default: return nullptr;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # haEntityCatStr
    lines.append('PGM_P haEntityCatStr(HaEntityCat ec) {')
    lines.append('    switch (ec) {')
    for ec in ENTITY_CATS:
        if ec == 'none':
            lines.append(f'        case HaEntityCat::{ec}: return nullptr;')
        else:
            lines.append(f'        case HaEntityCat::{ec}: {{ static const char s[] PROGMEM = "{ec}"; return s; }}')
    lines.append('        default: return nullptr;')
    lines.append('    }')
    lines.append('}')
    lines.append('')

    # ---------------------------------------------------------------------------
    # Preserve hand-written code below the marker in the existing file.
    # The marker line is:
    #   // ========== END AUTO-GENERATED SECTION ==========
    #   // Hand-written code below -- DO NOT remove this marker
    # Everything from the marker onward is appended after the generated section.
    # ---------------------------------------------------------------------------
    MARKER = '// ========== END AUTO-GENERATED SECTION =========='
    hand_written = ''
    if os.path.isfile(output_path):
        with open(output_path, encoding='utf-8') as fh:
            existing = fh.read()
        marker_pos = existing.find(MARKER)
        if marker_pos >= 0:
            hand_written = existing[marker_pos:]
            print(f'  Preserved {len(hand_written):,} bytes of hand-written code below marker.')
        else:
            print(f'  WARNING: marker not found in existing {output_path} -- hand-written code may be lost!',
                  file=sys.stderr)

    # Append marker + hand-written code
    lines.append(MARKER)
    if hand_written:
        # hand_written already starts with the marker line, strip to avoid duplication
        after_marker = hand_written[len(MARKER):]
        # Split into lines, skip the first (it's the marker we already appended)
        lines.append(after_marker.rstrip('\n'))
    else:
        lines.append('// Hand-written code below -- DO NOT remove this marker')
        lines.append('')

    with open(output_path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(lines))
    print(f'Generated {output_path}  ({os.path.getsize(output_path):,} bytes)')


# ---- Main ------------------------------------------------------------------ #

def main():
    global _icon_overrides

    if not os.path.isfile(INPUT_FILE):
        print(f'ERROR: input file not found: {INPUT_FILE}', file=sys.stderr)
        sys.exit(1)

    print(f'Parsing {INPUT_FILE} ...')
    raw_entries = parse_config(INPUT_FILE)
    print(f'  Parsed {len(raw_entries)} data entries.')

    _icon_overrides = load_icon_overrides(ICONS_CFG_FILE)

    sensors, bin_sensors, specials = process_entries(raw_entries)

    print(f'  Sensors        : {len(sensors)}')
    print(f'  Binary sensors : {len(bin_sensors)}')
    print(f'  Climate/Number : {len(specials)}')

    labels, names = collect_progmem_strings(sensors, bin_sensors)
    print(f'  Unique labels  : {len(labels)}')
    print(f'  Unique names   : {len(names)}')

    timestamp = datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

    generate_cpp(sensors, bin_sensors, specials, labels, names, CPP_FILE, timestamp)

    print('Done.')


if __name__ == '__main__':
    main()
