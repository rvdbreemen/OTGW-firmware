#!/usr/bin/env python3
"""
Generate readable mqttha_progmem.h + mqttha_progmem.cpp from mqttha.cfg.
Run from the repo root: python tools/generate_mqttha_readable.py

Design: each config entry gets its own named PROGMEM string variables,
with the JSON message formatted across multiple lines for readability.
The entry table uses direct PGM_P pointers (like OTlookup_t / OTmap[]).

Generated files:
  src/OTGW-firmware/mqttha_progmem.h   -- struct + helper accessor + extern declarations
  src/OTGW-firmware/mqttha_progmem.cpp -- named PROGMEM strings + pointer table + index

Struct layout (12 bytes on ESP8266, natural alignment):
  uint8_t  id        : offset 0  (1 byte)
  uint8_t  flags     : offset 1  (1 byte)  -- pre-computed source token / PIC flags
  [padding]          : offset 2  (2 bytes)
  PGM_P    topic     : offset 4  (4 bytes) -- PROGMEM pointer to topic template
  PGM_P    msg       : offset 8  (4 bytes) -- PROGMEM pointer to JSON message template
"""

import json
import os
import re
import sys
from collections import defaultdict
from datetime import datetime, timezone

# ---- Path setup ------------------------------------------------------------ #

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT   = os.path.dirname(SCRIPT_DIR)
SKETCH_DIR  = os.path.join(REPO_ROOT, "src", "OTGW-firmware")

INPUT_FILE  = os.path.join(SKETCH_DIR, "data", "mqttha.cfg")
HEADER_FILE = os.path.join(SKETCH_DIR, "mqttha_progmem.h")
CPP_FILE    = os.path.join(SKETCH_DIR, "mqttha_progmem.cpp")

# Flag bit definitions (must match MQTT_HA_FLAG_* in header)
FLAG_HAS_SOURCE_SUFFIX        = 0x01
FLAG_HAS_SOURCE_NAME          = 0x02
FLAG_HAS_SOURCE_TOPIC_SEGMENT = 0x04
FLAG_IS_PIC_ENTRY             = 0x08

# Max line width for C string literals
LINE_WIDTH = 100

# ---- Helpers --------------------------------------------------------------- #

def c_escape(s: str) -> str:
    """Escape a string for use inside C double quotes."""
    result = []
    for ch in s:
        if ch == "\\":   result.append("\\\\")
        elif ch == '"':  result.append('\\"')
        elif ch == "\r": result.append("\\r")
        elif ch == "\n": result.append("\\n")
        else:            result.append(ch)
    return "".join(result)


def compute_flags(topic: str, msg: str) -> int:
    """Compute pre-determined flags from topic and message content."""
    flags = 0
    combined = topic + msg
    if "%source_suffix%" in combined:        flags |= FLAG_HAS_SOURCE_SUFFIX
    if "%source_name%" in combined:          flags |= FLAG_HAS_SOURCE_NAME
    if "%source_topic_segment%" in combined: flags |= FLAG_HAS_SOURCE_TOPIC_SEGMENT
    if "otgw-pic/" in topic or "otgw-pic/" in msg:  flags |= FLAG_IS_PIC_ENTRY
    return flags


def derive_short_name(topic: str) -> str:
    """Derive a short C-identifier-safe name from a topic template path.

    Examples:
      %homeassistant%/climate/%node_id%/climate/config         -> climate
      %homeassistant%/sensor/%node_id%/TSet/config             -> tset
      %homeassistant%/sensor/%node_id%/TSet/%source_.../config -> tset_src
    """
    # Strip placeholders and split
    clean = topic.replace("%homeassistant%", "").replace("%node_id%", "")
    clean = clean.replace("%sensor_id%", "")
    parts = [p for p in clean.split("/") if p and p != "config"]

    has_source = "%source_topic_segment%" in topic or "%source_suffix%" in topic

    # Find the significant segment (skip HA entity type like sensor/binary_sensor/climate)
    ha_types = {"sensor", "binary_sensor", "climate", "number", "switch", "select", "button"}
    significant = [p for p in parts if p.lower() not in ha_types
                   and not p.startswith("%")]

    if significant:
        name = significant[-1].lower()
    elif parts:
        name = parts[-1].lower()
    else:
        name = "unknown"

    # Clean to valid C identifier
    name = re.sub(r'[^a-z0-9_]', '_', name)
    name = re.sub(r'_+', '_', name).strip('_')

    if has_source:
        name += "_src"

    return name or "entry"


def format_json_multiline(json_str: str, indent: str = "    ") -> list[str]:
    """Break a JSON string into readable multi-line C string literals.

    Attempts to split on top-level JSON keys for readability.
    Returns list of C string literal lines (without outer quotes).
    """
    lines = []
    escaped = c_escape(json_str)

    # Try to parse as JSON for pretty formatting
    try:
        obj = json.loads(json_str)
        formatted = json.dumps(obj, indent=2, ensure_ascii=False)
        # Convert pretty-printed JSON back to C string lines
        for line in formatted.split("\n"):
            c_line = c_escape(line)
            lines.append(c_line)
        return lines
    except (json.JSONDecodeError, ValueError):
        pass

    # Fallback: split on '", "' boundaries for readability
    # Split after each top-level key-value pair
    chunks = []
    current = ""
    depth = 0
    i = 0
    while i < len(escaped):
        ch = escaped[i]
        if ch == '\\' and i + 1 < len(escaped):
            current += ch + escaped[i + 1]
            i += 2
            continue
        if ch == '{' or ch == '[':
            depth += 1
        elif ch == '}' or ch == ']':
            depth -= 1
        current += ch
        # Split after ", " at depth 1 (top-level keys)
        if depth == 1 and current.endswith(', ') and len(current) > 40:
            chunks.append(current)
            current = ""
        i += 1
    if current:
        chunks.append(current)

    # If single chunk and short enough, return as one line
    if len(chunks) == 1 and len(chunks[0]) <= LINE_WIDTH:
        return [chunks[0]]

    return chunks if chunks else [escaped]


# ---- Parse input ----------------------------------------------------------- #

def parse_config(path: str):
    """Parse mqttha.cfg into list of (id, topic, msg) tuples."""
    entries = []
    with open(path, encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\n").rstrip("\r")
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            parts = line.split(";", 2)
            if len(parts) != 3:
                print(f"WARNING line {lineno}: expected 3 fields -- skipping", file=sys.stderr)
                continue
            raw_id, raw_topic, raw_msg = parts
            try:
                ot_id = int(raw_id.strip())
            except ValueError:
                print(f"WARNING line {lineno}: non-integer ID -- skipping", file=sys.stderr)
                continue
            if not (0 <= ot_id <= 255):
                print(f"WARNING line {lineno}: ID {ot_id} out of range -- skipping", file=sys.stderr)
                continue
            entries.append((ot_id, raw_topic.strip(), raw_msg.strip()))
    return entries


# ---- Build index ----------------------------------------------------------- #

def build_index(sorted_entries):
    """Build ID -> first table index mapping."""
    index = [0xFFFF] * 256
    for pos, (ot_id, _, _) in enumerate(sorted_entries):
        if index[ot_id] == 0xFFFF:
            index[ot_id] = pos
    return index


# ---- Assign unique names --------------------------------------------------- #

def assign_names(sorted_entries):
    """Assign unique C identifier names to each entry."""
    name_counts = defaultdict(int)  # track duplicates within same ID
    names = []

    for ot_id, topic, msg in sorted_entries:
        base = derive_short_name(topic)
        key = f"{ot_id}_{base}"
        name_counts[key] += 1

        if name_counts[key] == 1:
            name = f"{ot_id}_{base}"
        else:
            name = f"{ot_id}_{base}_{name_counts[key]}"

        names.append(name)

    # Second pass: if a name was used only once, keep it; if used multiple times,
    # add _1 to the first occurrence for consistency
    final_counts = defaultdict(int)
    for i, (ot_id, topic, msg) in enumerate(sorted_entries):
        base = derive_short_name(topic)
        key = f"{ot_id}_{base}"
        total = sum(1 for n in names if n.startswith(key))
        if total > 1 and not names[i].endswith(f"_{total}") and "_" not in names[i][len(key):]:
            # This was the first occurrence of a duplicate
            pass  # Already handled above

    return names


# ---- Derive description from topic ---------------------------------------- #

def describe_entry(topic: str, msg: str) -> str:
    """Create a short human-readable description from the topic and message."""
    # Extract HA entity type
    parts = topic.replace("%homeassistant%/", "").split("/")
    entity_type = parts[0] if parts else "unknown"

    # Extract name from JSON if possible
    try:
        obj = json.loads(msg)
        name = obj.get("name", "").replace("%hostname%_", "").replace("_", " ")
        if name:
            return f"{entity_type}: {name}"
    except (json.JSONDecodeError, ValueError):
        pass

    # Fallback: use topic segments
    significant = [p for p in parts if p and p != "config" and not p.startswith("%")]
    if significant:
        return f"{entity_type}: {significant[-1]}"
    return entity_type


# ---- Generate header ------------------------------------------------------- #

def generate_header(count, output_path, timestamp):
    content = f"""\
// AUTO-GENERATED - DO NOT EDIT.
// Run tools/generate_mqttha_readable.py to regenerate from data/mqttha.cfg.
// Generated: {timestamp}
//
// Readable PROGMEM discovery config -- OTlookup_t style.
// Each entry has named PROGMEM strings and direct PGM_P pointers.

#pragma once
#include <pgmspace.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Entry descriptor -- 12 bytes on ESP8266 (with pointer alignment)
//
// After reading from PROGMEM with readMqttHaCfgEntry():
//   id    : OT message ID (0-255)
//   flags : pre-computed flags (source tokens, PIC entry)
//   topic : PGM_P pointer to topic template in flash
//   msg   : PGM_P pointer to JSON message template in flash
// ---------------------------------------------------------------------------
struct MqttHaCfgEntry {{
    uint8_t  id;
    uint8_t  flags;
    PGM_P    topic;
    PGM_P    msg;
}};

// Flag bit definitions
constexpr uint8_t MQTT_HA_FLAG_SOURCE_SUFFIX        = 0x01;
constexpr uint8_t MQTT_HA_FLAG_SOURCE_NAME          = 0x02;
constexpr uint8_t MQTT_HA_FLAG_SOURCE_TOPIC_SEGMENT = 0x04;
constexpr uint8_t MQTT_HA_FLAG_IS_PIC_ENTRY         = 0x08;
constexpr uint8_t MQTT_HA_FLAG_ANY_SOURCE           = 0x07;  // mask for any source token

// Total number of entries in mqttHaCfgTable
constexpr uint16_t MQTT_HA_CFG_COUNT = {count};

// PROGMEM data -- defined in mqttha_progmem.cpp
extern const MqttHaCfgEntry mqttHaCfgTable[];
extern const uint16_t mqttHaCfgIndex[256];

// Helper: read one entry from PROGMEM into RAM (like PROGMEM_readAnything)
// After the call, entry.topic and entry.msg are PGM_P pointers --
// use pgm_read_char(), strlen_P(), strncpy_P() to access the strings.
inline MqttHaCfgEntry readMqttHaCfgEntry(uint16_t idx) {{
    MqttHaCfgEntry entry;
    memcpy_P(&entry, &mqttHaCfgTable[idx], sizeof(entry));
    return entry;
}}
"""
    with open(output_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(content)
    print(f"Generated {output_path}  ({os.path.getsize(output_path):,} bytes)")


# ---- Generate cpp ---------------------------------------------------------- #

def generate_cpp(sorted_entries, names, output_path, timestamp):
    index = build_index(sorted_entries)
    count = len(sorted_entries)
    unique_ids = len({e[0] for e in sorted_entries})

    lines = []
    lines.append(f"""\
// AUTO-GENERATED - DO NOT EDIT.
// Run tools/generate_mqttha_readable.py to regenerate from data/mqttha.cfg.
// Generated: {timestamp}
//
// Total entries : {count}
// Unique OT IDs : {unique_ids}
//
// Each entry has named PROGMEM strings for readability.
// JSON messages are formatted across multiple lines.

#include <pgmspace.h>
#include <stdint.h>
#include "mqttha_progmem.h"
""")

    # Track current ID for section headers
    current_id = -1
    entry_comments = []  # (name, description) for the table

    for n, (ot_id, topic, msg) in enumerate(sorted_entries):
        name = names[n]
        flags = compute_flags(topic, msg)
        desc = describe_entry(topic, msg)
        entry_comments.append((name, desc, flags))

        # Section header for new ID
        if ot_id != current_id:
            current_id = ot_id
            lines.append(f"// {'=' * 70}")
            lines.append(f"// ID {ot_id}")
            lines.append(f"// {'=' * 70}")
            lines.append("")

        # Topic string
        lines.append(f"// {desc}")
        escaped_topic = c_escape(topic)
        lines.append(f'const char ha_topic_{name}[] PROGMEM =')
        lines.append(f'    "{escaped_topic}";')
        lines.append("")

        # Message string (formatted JSON)
        json_lines = format_json_multiline(msg)
        lines.append(f'const char ha_msg_{name}[] PROGMEM =')
        for i, jline in enumerate(json_lines):
            suffix = ";" if i == len(json_lines) - 1 else ""
            lines.append(f'    "{jline}"{suffix}')
        lines.append("")

    # Entry table
    lines.append(f"// {'=' * 70}")
    lines.append(f"// Discovery config table -- {count} entries, sorted by ID")
    lines.append(f"// {'=' * 70}")
    lines.append(f"const MqttHaCfgEntry PROGMEM mqttHaCfgTable[] = {{")

    current_id = -1
    for n, (ot_id, topic, msg) in enumerate(sorted_entries):
        name, desc, flags = entry_comments[n]
        flag_str = f"0x{flags:02X}" if flags else "0x00"

        if ot_id != current_id:
            current_id = ot_id
            lines.append(f"    // --- ID {ot_id} ---")

        # Align columns for readability
        topic_ref = f"ha_topic_{name},"
        msg_ref = f"ha_msg_{name}"
        lines.append(f"    {{{ot_id:3d}, {flag_str}, {topic_ref:40s} {msg_ref}}},  // {desc}")

    lines.append("};")
    lines.append("")

    # Index table
    lines.append(f"// {'=' * 70}")
    lines.append("// ID -> first table index (0xFFFF = not present)")
    lines.append(f"// {'=' * 70}")
    lines.append("const uint16_t PROGMEM mqttHaCfgIndex[256] = {")
    for i, first_pos in enumerate(index):
        if first_pos == 0xFFFF:
            hex_val = "0xFFFF"
            comment = f"// id {i}"
        else:
            cnt = sum(1 for e in sorted_entries if e[0] == i)
            hex_val = str(first_pos)
            comment = f"// id {i}, {cnt} entr{'y' if cnt == 1 else 'ies'}"
        comma = "," if i < 255 else ""
        lines.append(f"    {hex_val}{comma} {comment}")
    lines.append("};")
    lines.append("")

    with open(output_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))
    print(f"Generated {output_path}  ({os.path.getsize(output_path):,} bytes)")


# ---- Main ------------------------------------------------------------------ #

def main():
    if not os.path.isfile(INPUT_FILE):
        print(f"ERROR: input file not found: {INPUT_FILE}", file=sys.stderr)
        sys.exit(1)

    print(f"Parsing {INPUT_FILE} ...")
    entries = parse_config(INPUT_FILE)
    print(f"  Parsed {len(entries)} data entries.")

    sorted_entries = sorted(entries, key=lambda e: e[0])
    names = assign_names(sorted_entries)

    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    generate_header(len(sorted_entries), HEADER_FILE, timestamp)
    generate_cpp(sorted_entries, names, CPP_FILE, timestamp)

    print(f"  Entries       : {len(sorted_entries)}")
    print(f"  Unique IDs    : {len({e[0] for e in sorted_entries})}")
    print("Done.")


if __name__ == "__main__":
    main()
