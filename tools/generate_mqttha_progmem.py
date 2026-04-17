#!/usr/bin/env python3
"""
Generate mqttha_progmem.h + mqttha_progmem.cpp from mqttha.cfg.
Run from the repo root: python tools/generate_mqttha_progmem.py

Design: two flat PROGMEM string pools (all topics / all msgs concatenated) in a
SEPARATE .cpp translation unit.  The header contains only declarations.

Arduino compiles .cpp files in the sketch directory as separate TUs, which avoids
the Xtensa single-TU section/relocation explosion that occurs when large PROGMEM
data is placed in the main sketch.cpp.

Generated files:
  src/OTGW-firmware/mqttha_progmem.h   -- struct + extern declarations (include in .ino)
  src/OTGW-firmware/mqttha_progmem.cpp -- actual PROGMEM data definitions

Struct layout (natural alignment, sizeof = 8):
  uint8_t  id        : offset 0  (1 byte)
  uint8_t  flags     : offset 1  (1 byte)  — pre-computed source token flags
  uint16_t topicOff  : offset 2  (2 bytes)  — byte offset into mqttHaTopicPool
  uint32_t msgOff    : offset 4  (4 bytes)  — byte offset into mqttHaMsgPool

Flags byte layout:
  bit 0: hasSourceSuffix       — topic or msg contains %source_suffix%
  bit 1: hasSourceName         — topic or msg contains %source_name%
  bit 2: hasSourceTopicSegment — topic or msg contains %source_topic_segment%
  bit 3: isPicEntry            — topic contains "otgw-pic/"
"""

import os
import sys
import struct as pystructmod
from datetime import datetime, timezone

# ---- Path setup ------------------------------------------------------------ #

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT   = os.path.dirname(SCRIPT_DIR)
SKETCH_DIR  = os.path.join(REPO_ROOT, "src", "OTGW-firmware")

INPUT_FILE  = os.path.join(SKETCH_DIR, "data", "mqttha.cfg")
HEADER_FILE = os.path.join(SKETCH_DIR, "mqttha_progmem.h")
CPP_FILE    = os.path.join(SKETCH_DIR, "mqttha_progmem.cpp")

# Python struct format for MqttHaCfgEntry (8 bytes, little-endian):
# '<' = little-endian, 'B' = uint8, 'B' = uint8 flags, 'H' = uint16, 'I' = uint32
ENTRY_FMT = '<BBHI'
assert pystructmod.calcsize(ENTRY_FMT) == 8, "entry struct must be 8 bytes"

# Flag bit definitions (must match MQTT_HA_FLAG_* in mqttha_progmem.h)
FLAG_HAS_SOURCE_SUFFIX        = 0x01
FLAG_HAS_SOURCE_NAME          = 0x02
FLAG_HAS_SOURCE_TOPIC_SEGMENT = 0x04
FLAG_IS_PIC_ENTRY             = 0x08

# ---- Helpers --------------------------------------------------------------- #

def c_escape(s: str) -> str:
    result = []
    for ch in s:
        if ch == "\\":   result.append("\\\\")
        elif ch == '"':  result.append('\\"')
        elif ch == "\r": result.append("\\r")
        elif ch == "\n": result.append("\\n")
        else:            result.append(ch)
    return "".join(result)


# ---- Parse input ----------------------------------------------------------- #

def parse_config(path: str):
    entries = []
    with open(path, encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\n").rstrip("\r")
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            parts = line.split(";", 2)
            if len(parts) != 3:
                print(f"WARNING line {lineno}: expected 3 fields — skipping", file=sys.stderr)
                continue
            raw_id, raw_topic, raw_msg = parts
            try:
                ot_id = int(raw_id.strip())
            except ValueError:
                print(f"WARNING line {lineno}: non-integer ID — skipping", file=sys.stderr)
                continue
            if not (0 <= ot_id <= 255):
                print(f"WARNING line {lineno}: ID {ot_id} out of range — skipping", file=sys.stderr)
                continue
            entries.append((ot_id, raw_topic.strip(), raw_msg.strip()))
    return entries


# ---- Build index ----------------------------------------------------------- #

def build_index(entries):
    index = [0xFFFF] * 256
    for pos, (ot_id, _, _) in enumerate(entries):
        if index[ot_id] == 0xFFFF:
            index[ot_id] = pos
    return index


# ---- Build string pools ---------------------------------------------------- #

def compute_flags(topic: str, msg: str) -> int:
    flags = 0
    combined = topic + msg
    if "%source_suffix%" in combined:        flags |= FLAG_HAS_SOURCE_SUFFIX
    if "%source_name%" in combined:          flags |= FLAG_HAS_SOURCE_NAME
    if "%source_topic_segment%" in combined: flags |= FLAG_HAS_SOURCE_TOPIC_SEGMENT
    if "otgw-pic/" in topic or "otgw-pic/" in msg:  flags |= FLAG_IS_PIC_ENTRY
    return flags


def build_pools(sorted_entries):
    topic_pool = bytearray()
    msg_pool   = bytearray()
    t_offsets  = []
    m_offsets  = []
    flags_list = []
    for _, topic, msg in sorted_entries:
        t_offsets.append(len(topic_pool))
        topic_pool.extend(topic.encode("utf-8") + b"\x00")
        m_offsets.append(len(msg_pool))
        msg_pool.extend(msg.encode("utf-8") + b"\x00")
        flags_list.append(compute_flags(topic, msg))
    return bytes(topic_pool), bytes(msg_pool), t_offsets, m_offsets, flags_list


# ---- Pool → C string literal ----------------------------------------------- #

LINE_WIDTH = 120

def pool_to_c_string(pool: bytes) -> str:
    lines = []
    current = []
    current_len = 0
    for b in pool:
        if b == 0:         c = "\\0"
        elif b == ord("\\"): c = "\\\\"
        elif b == ord('"'): c = '\\"'
        elif b == ord("\r"): c = "\\r"
        elif b == ord("\n"): c = "\\n"
        elif 0x20 <= b <= 0x7E: c = chr(b)
        else:              c = f"\\x{b:02x}"
        if current_len + len(c) > LINE_WIDTH:
            lines.append(f'  "{("".join(current))}"')
            current = []; current_len = 0
        current.append(c)
        current_len += len(c)
    if current:
        lines.append(f'  "{("".join(current))}"')
    return "\n".join(lines)


# ---- Generate header ------------------------------------------------------- #

def generate_header(count, topic_pool_size, msg_pool_size, output_path, timestamp):
    lines = []
    lines.append(f"""\
// AUTO-GENERATED - DO NOT EDIT.
// Run tools/generate_mqttha_progmem.py to regenerate.
// Source: src/OTGW-firmware/data/mqttha.cfg
// Generated: {timestamp}
//
// Declarations only — actual PROGMEM data lives in mqttha_progmem.cpp
// which Arduino compiles as a separate translation unit.  This avoids
// the Xtensa single-TU relocation explosion from embedding large PROGMEM
// data in the main sketch.cpp.

#pragma once
#include <pgmspace.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Entry descriptor — 8 bytes with natural alignment
// Fields after memcpy_P to RAM:
//   id        : uint8_t  — OT message ID
//   flags     : uint8_t  — pre-computed flags (avoids strstr on PROGMEM at runtime)
//   topicOff  : uint16_t — byte offset into mqttHaTopicPool ({topic_pool_size} bytes)
//   msgOff    : uint32_t — byte offset into mqttHaMsgPool   ({msg_pool_size} bytes)
// ---------------------------------------------------------------------------
struct MqttHaCfgEntry {{
  uint8_t  id;
  uint8_t  flags;
  uint16_t topicOff;
  uint32_t msgOff;
}};
static_assert(sizeof(MqttHaCfgEntry) == 8, "MqttHaCfgEntry must be 8 bytes");

// Flag bit definitions
constexpr uint8_t MQTT_HA_FLAG_SOURCE_SUFFIX        = 0x01;
constexpr uint8_t MQTT_HA_FLAG_SOURCE_NAME          = 0x02;
constexpr uint8_t MQTT_HA_FLAG_SOURCE_TOPIC_SEGMENT = 0x04;
constexpr uint8_t MQTT_HA_FLAG_IS_PIC_ENTRY         = 0x08;
constexpr uint8_t MQTT_HA_FLAG_ANY_SOURCE           = 0x07;  // mask for any source token

// Total number of entries in mqttHaCfgTable
constexpr uint16_t MQTT_HA_CFG_COUNT = {count};

// PROGMEM data — defined in mqttha_progmem.cpp
extern const char   mqttHaTopicPool[];
extern const char   mqttHaMsgPool[];
extern const MqttHaCfgEntry mqttHaCfgTable[{count}];
extern const uint16_t mqttHaCfgIndex[256];
""")
    with open(output_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))
    print(f"Generated {output_path}  ({os.path.getsize(output_path):,} bytes)")


# ---- Generate cpp ---------------------------------------------------------- #

def generate_cpp(sorted_entries, topic_pool, msg_pool, t_offsets, m_offsets, flags_list,
                 output_path, timestamp):
    index = build_index(sorted_entries)
    count = len(sorted_entries)
    unique_ids = len({e[0] for e in sorted_entries})
    max_topic  = max(len(e[1]) for e in sorted_entries)
    max_msg    = max(len(e[2]) for e in sorted_entries)

    lines = []
    lines.append(f"""\
// AUTO-GENERATED - DO NOT EDIT.
// Run tools/generate_mqttha_progmem.py to regenerate.
// Source: src/OTGW-firmware/data/mqttha.cfg
// Generated: {timestamp}
//
// Total entries : {count}
// Unique OT IDs : {unique_ids}
// Longest topic : {max_topic} chars
// Longest msg   : {max_msg} chars
//
// Compiled by Arduino as a separate translation unit.

#include <pgmspace.h>
#include <stdint.h>
#include "mqttha_progmem.h"
""")

    # Topic pool
    lines.append(f"// Topic pool — {len(topic_pool):,} bytes")
    lines.append("const char PROGMEM mqttHaTopicPool[] =")
    lines.append(pool_to_c_string(topic_pool))
    lines.append(";")
    lines.append("")

    # Msg pool
    lines.append(f"// Message pool — {len(msg_pool):,} bytes")
    lines.append("const char PROGMEM mqttHaMsgPool[] =")
    lines.append(pool_to_c_string(msg_pool))
    lines.append(";")
    lines.append("")

    # Entry table
    lines.append("// Entry table — sorted by id")
    lines.append(f"const MqttHaCfgEntry PROGMEM mqttHaCfgTable[{count}] = {{")
    for n, (ot_id, topic, _) in enumerate(sorted_entries):
        t_off = t_offsets[n]
        m_off = m_offsets[n]
        flg = flags_list[n]
        short = topic[:55] + ("..." if len(topic) > 55 else "")
        flag_str = f"0x{flg:02X}" if flg else "0"
        lines.append(f"  {{{ot_id}, {flag_str}, {t_off}, {m_off}}},  // [{n}] {short}")
    lines.append("};")
    lines.append("")

    # Index
    lines.append("// ID -> first table index (0xFFFF = not present)")
    lines.append("const uint16_t PROGMEM mqttHaCfgIndex[256] = {")
    for i, first_pos in enumerate(index):
        if first_pos == 0xFFFF:
            comment = f"// id {i} - not present"
        else:
            cnt = sum(1 for e in sorted_entries if e[0] == i)
            comment = f"// id {i}, {cnt} entr{'y' if cnt==1 else 'ies'} at {first_pos}"
        hex_val = "0xFFFF" if first_pos == 0xFFFF else str(first_pos)
        comma = "," if i < 255 else ""
        lines.append(f"  {hex_val}{comma} {comment}")
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
    topic_pool, msg_pool, t_offsets, m_offsets, flags_list = build_pools(sorted_entries)

    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    generate_header(len(sorted_entries), len(topic_pool), len(msg_pool), HEADER_FILE, timestamp)
    generate_cpp(sorted_entries, topic_pool, msg_pool, t_offsets, m_offsets, flags_list, CPP_FILE, timestamp)

    print(f"  Entries       : {len(sorted_entries)}")
    print(f"  Topic pool    : {len(topic_pool):,} bytes (offsets fit uint16)")
    print(f"  Msg pool      : {len(msg_pool):,} bytes (offsets need uint32)")
    print(f"  Entry table   : {len(sorted_entries)*8:,} bytes ({len(sorted_entries)} x 8)")
    print("Done.")


if __name__ == "__main__":
    main()
