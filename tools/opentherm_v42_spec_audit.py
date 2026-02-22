#!/usr/bin/env python3
"""
OpenTherm v4.2 spec-driven audit for OTGW firmware and HA discovery.

Ground truth: specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md

What this checks:
- OTmap coverage and direction/type alignment against the v4.2 Markdown spec
- Main decode switch coverage and selected spec-sensitive decode routes
- Home Assistant discovery template consistency for known v4.2-sensitive entries
- Legacy pre-v4.2 IDs 50-63 are only tolerated when reserved-ID gating exists

Outputs:
- CSV matrix (one row per v4.2 spec ID)
- JSON report (counts + findings)

Exit code:
- 0: no failing findings
- 1: one or more failing findings (when --check is used)
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple


DEFAULT_SPEC = Path("specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md")
DEFAULT_HEADER = Path("src/OTGW-firmware/OTGW-Core.h")
DEFAULT_SOURCE = Path("src/OTGW-firmware/OTGW-Core.ino")
DEFAULT_MQTTHA = Path("src/OTGW-firmware/data/mqttha.cfg")
DEFAULT_MATRIX_OUT = Path(".tmp/ot_v42_matrix_spec_audit.csv")
DEFAULT_REPORT_OUT = Path(".tmp/ot_v42_audit_report_spec_audit.json")


SPEC_MSG_TO_OT = {"R/-": "OT_READ", "-/W": "OT_WRITE", "R/W": "OT_RW"}
OTTYPE_NORM = {
    "ot_flag8flag8": "flag8flag8",
    "ot_flag8u8": "flag8u8",
    "ot_f88": "f88",
    "ot_u8u8": "u8u8",
    "ot_u16": "u16",
    "ot_s16": "s16",
    "ot_s8s8": "s8s8",
    "ot_special": "special",
    "ot_u8": "u8",
    "ot_flag8": "flag8",
}

# Spec-sensitive decode routes where generic type matching is not sufficient
EXPECTED_DECODE_BY_ID = {
    38: "print_f88",
    71: "print_u8_lb",
    77: "print_u8_lb",
    78: "print_u8_lb",
    87: "print_u8_hb",
    98: "print_rf_sensor_status_information",
    99: "print_remote_override_operating_mode",
}

# Findings that should fail CI
FAILING_FINDING_KEYS = (
    "missing_spec_ids_in_otmap",
    "direction_mismatches",
    "type_mismatches",
    "main_decode_missing_spec_ids",
    "decode_mismatches",
    "legacy_reserved_guard_missing",
    "mqttha_direct_topic_id_mismatch",
    "mqttha_fanspeed_discovery_issues",
    "mqttha_hcratio_discovery_issues",
    "mqttha_vh_config_trigger_issues",
    "mqttha_direct_unit_semantic_issues",
)


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _norm_cell(cell: str) -> str:
    cell = cell.replace("`", "").replace("—", "-").strip()
    if cell.startswith("_") and cell.endswith("_"):
        cell = cell[1:-1]
    return cell.replace("f8.8", "f88")


def _spec_type_key(spec_row: Dict[str, Any]) -> str:
    hb = _norm_cell(spec_row["hb_type"])
    lb = _norm_cell(spec_row["lb_type"])
    cols = [str(c).lower() for c in spec_row["raw_cols"]]

    if hb == "special" or lb == "special":
        return "special"
    if hb == "flag8" and lb == "flag8":
        return "flag8flag8"
    if hb == "flag8" and lb == "u8":
        return "flag8u8"
    if hb == "flag8" and lb == "-":
        return "flag8"
    if hb == "u8" and lb == "u8":
        return "u8u8"
    if hb == "u8" and lb == "-":
        return "u8_hb"
    if hb == "-" and lb == "u8":
        return "u8_lb"
    if hb == "s8" and lb == "s8":
        return "s8s8"
    if hb == "-" and lb == "-":
        for t in ("f88", "u16", "s16"):
            if any(t in c for c in cols):
                return t
        return "unknown_combined"
    if hb in {"f88", "u16", "s16"}:
        return hb
    if lb in {"f88", "u16", "s16"}:
        return lb
    return f"{hb}/{lb}"


def parse_spec_ids(spec_text: str) -> Dict[int, Dict[str, Any]]:
    spec_ids: Dict[int, Dict[str, Any]] = {}
    for lineno, line in enumerate(spec_text.splitlines(), start=1):
        if not line.startswith("|"):
            continue
        parts = [p.strip() for p in line.strip().split("|")[1:-1]]
        if len(parts) < 9:
            continue
        if not re.fullmatch(r"\d+", parts[0]):
            continue
        if parts[1] not in SPEC_MSG_TO_OT:
            continue
        msg_id = int(parts[0])
        if not (0 <= msg_id <= 127):
            continue
        spec_ids[msg_id] = {
            "id": msg_id,
            "line": lineno,
            "msg": parts[1],
            "object": parts[2],
            "hb_type": parts[3],
            "lb_type": parts[4],
            "unit": parts[8],
            "raw_cols": parts,
        }
    return spec_ids


def parse_enum_map(header_text: str) -> Dict[str, Dict[str, Any]]:
    enum_map: Dict[str, Dict[str, Any]] = {}
    inside_enum = False
    current_value = -1

    for lineno, line in enumerate(header_text.splitlines(), start=1):
        if "enum OpenThermMessageID" in line:
            inside_enum = True
            continue
        if inside_enum and "};" in line:
            inside_enum = False
            continue
        if not inside_enum:
            continue

        match = re.match(r"\s*(OT_[A-Za-z0-9_]+)\s*(=\s*(\d+))?\s*,\s*(?://\s*(.*))?$", line)
        if not match:
            continue

        name = match.group(1)
        if match.group(3) is not None:
            current_value = int(match.group(3))
        else:
            current_value += 1

        enum_map[name] = {
            "id": current_value,
            "line": lineno,
            "comment": (match.group(4) or "").strip(),
        }
    return enum_map


def parse_otmap(header_text: str) -> Dict[int, Dict[str, Any]]:
    otmap: Dict[int, Dict[str, Any]] = {}
    pattern = re.compile(
        r"\{\s*(\d+)\s*,\s*(OT_[A-Z_]+)\s*,\s*(ot_[a-zA-Z0-9]+)\s*,\s*\"([^\"]*)\"\s*,\s*\"([^\"]*)\"\s*,\s*\"([^\"]*)\"\s*\}"
    )
    for lineno, line in enumerate(header_text.splitlines(), start=1):
        m = pattern.search(line)
        if not m:
            continue
        msg_id = int(m.group(1))
        otmap[msg_id] = {
            "line": lineno,
            "msgcmd": m.group(2),
            "type": m.group(3),
            "label": m.group(4),
            "desc": m.group(5),
            "unit": m.group(6),
        }
    return otmap


def parse_decode_cases(source_text: str, enum_map: Dict[str, Dict[str, Any]]) -> Dict[int, Dict[str, Any]]:
    decode_cases: Dict[int, Dict[str, Any]] = {}
    case_re = re.compile(r"case\s+(OT_[A-Za-z0-9_]+)\s*:\s*([A-Za-z0-9_]+)\s*\(")

    for lineno, line in enumerate(source_text.splitlines(), start=1):
        m = case_re.search(line)
        if not m:
            continue

        enum_name, fn_name = m.group(1), m.group(2)
        if enum_name not in enum_map:
            continue
        msg_id = enum_map[enum_name]["id"]
        rec = {"line": lineno, "enum_name": enum_name, "fn": fn_name}
        prev = decode_cases.get(msg_id)
        if prev is None:
            decode_cases[msg_id] = rec
            continue

        prev_is_print = str(prev["fn"]).startswith("print_")
        now_is_print = fn_name.startswith("print_")
        if now_is_print and not prev_is_print:
            decode_cases[msg_id] = rec
        elif now_is_print and prev_is_print and lineno < int(prev["line"]):
            decode_cases[msg_id] = rec

    return decode_cases


def parse_mqttha_entries(mqttha_text: str) -> List[Dict[str, Any]]:
    entries: List[Dict[str, Any]] = []
    line_re = re.compile(r"^(\d+)\s*;\s*([^;]+?)\s*;\s*(\{.*\})\s*$")
    for lineno, raw in enumerate(mqttha_text.splitlines(), start=1):
        stripped = raw.strip()
        if not stripped or stripped.startswith("//"):
            continue
        m = line_re.match(raw)
        if not m:
            continue
        payload = m.group(3)
        try:
            parsed_json = json.loads(payload)
        except json.JSONDecodeError:
            parsed_json = {"_parse_error": True, "_raw": payload}
        entries.append(
            {
                "line": lineno,
                "id": int(m.group(1)),
                "disc_topic": m.group(2).strip(),
                "json": parsed_json,
            }
        )
    return entries


def detect_legacy_reserved_guard(source_text: str) -> Dict[str, bool]:
    checks = {
        "has_reserved_helper": "isMsgIdReservedInActiveProfile(" in source_text,
        "has_validity_gate": "if (isMsgIdReservedInActiveProfile(OT.id)) return false;" in source_text,
        "has_processot_reserved_branch": "Reserved in %s profile (legacy pre-v4.2 ID %u ignored)" in source_text,
    }
    checks["guard_present"] = all(checks.values())
    return checks


def build_audit(
    spec_ids: Dict[int, Dict[str, Any]],
    enum_map: Dict[str, Dict[str, Any]],
    otmap: Dict[int, Dict[str, Any]],
    decode_cases: Dict[int, Dict[str, Any]],
    mqttha_entries: List[Dict[str, Any]],
    source_text: str,
) -> Tuple[List[Dict[str, Any]], Dict[str, List[Dict[str, Any]]], Dict[str, Any]]:
    findings: Dict[str, List[Dict[str, Any]]] = {
        "missing_spec_ids_in_otmap": [],
        "direction_mismatches": [],
        "type_mismatches": [],
        "main_decode_missing_spec_ids": [],
        "decode_mismatches": [],
        "reserved_legacy_ids_present": [],
        "legacy_reserved_guard_missing": [],
        "mqttha_direct_topic_id_mismatch": [],
        "mqttha_fanspeed_discovery_issues": [],
        "mqttha_hcratio_discovery_issues": [],
        "mqttha_vh_config_trigger_issues": [],
        "mqttha_direct_unit_semantic_issues": [],
    }

    matrix_rows: List[Dict[str, Any]] = []

    for msg_id, spec_row in sorted(spec_ids.items()):
        ot = otmap.get(msg_id)
        dec = decode_cases.get(msg_id)
        spec_type = _spec_type_key(spec_row)

        row = {
            "id": msg_id,
            "spec_line": spec_row["line"],
            "spec_msg": spec_row["msg"],
            "spec_object": spec_row["object"],
            "spec_type": spec_type,
            "otmap_line": ot["line"] if ot else None,
            "otmap_msg": ot["msgcmd"] if ot else None,
            "otmap_type": ot["type"] if ot else None,
            "otmap_label": ot["label"] if ot else None,
            "decode_line": dec["line"] if dec else None,
            "decode_print_fn": dec["fn"] if dec else None,
            "mqttha_count": sum(1 for e in mqttha_entries if int(e["id"]) == msg_id),
        }
        matrix_rows.append(row)

        if ot is None:
            findings["missing_spec_ids_in_otmap"].append(row)
            continue

        expected_msg = SPEC_MSG_TO_OT[spec_row["msg"]]
        if ot["msgcmd"] != expected_msg:
            findings["direction_mismatches"].append(row)

        ot_type_norm = OTTYPE_NORM.get(ot["type"])
        if spec_type in {"u8_hb", "u8_lb"}:
            if ot_type_norm != "u8":
                row_copy = dict(row)
                row_copy["expected_otmap_type"] = "ot_u8"
                findings["type_mismatches"].append(row_copy)
        elif spec_type != "unknown_combined":
            if ot_type_norm != spec_type:
                findings["type_mismatches"].append(row)

        if dec is None or not str(dec["fn"]).startswith("print_"):
            findings["main_decode_missing_spec_ids"].append(row)
        elif msg_id in EXPECTED_DECODE_BY_ID and dec["fn"] != EXPECTED_DECODE_BY_ID[msg_id]:
            row_copy = dict(row)
            row_copy["expected_decode_fn"] = EXPECTED_DECODE_BY_ID[msg_id]
            findings["decode_mismatches"].append(row_copy)

    # Legacy IDs present for compatibility (informational, unless guard missing)
    for legacy_id in range(50, 64):
        if legacy_id in otmap and legacy_id not in spec_ids:
            findings["reserved_legacy_ids_present"].append(
                {
                    "id": legacy_id,
                    "otmap_line": otmap[legacy_id]["line"],
                    "label": otmap[legacy_id]["label"],
                    "msgcmd": otmap[legacy_id]["msgcmd"],
                    "type": otmap[legacy_id]["type"],
                }
            )

    legacy_guard = detect_legacy_reserved_guard(source_text)
    if findings["reserved_legacy_ids_present"] and not legacy_guard["guard_present"]:
        findings["legacy_reserved_guard_missing"].append(legacy_guard)

    # mqttha checks (targeted + simple direct-topic consistency)
    label_to_id = {v["label"]: k for k, v in otmap.items() if v.get("label")}
    simple_leaf_re = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")

    for entry in mqttha_entries:
        payload = entry["json"]
        if not isinstance(payload, dict):
            continue
        stat_t = payload.get("stat_t")
        uom = payload.get("unit_of_measurement")

        disc_m = re.search(r"/([^/]+)/config$", str(entry["disc_topic"]))
        disc_leaf = disc_m.group(1) if disc_m else None
        stat_leaf = None
        if isinstance(stat_t, str):
            stat_leaf = stat_t.split("/")[-1]

        if (
            isinstance(disc_leaf, str)
            and isinstance(stat_leaf, str)
            and simple_leaf_re.fullmatch(disc_leaf)
            and simple_leaf_re.fullmatch(stat_leaf)
            and disc_leaf in label_to_id
            and stat_leaf in label_to_id
        ):
            if entry["id"] != label_to_id[disc_leaf] or label_to_id[disc_leaf] != label_to_id[stat_leaf]:
                findings["mqttha_direct_topic_id_mismatch"].append(
                    {
                        "line": entry["line"],
                        "id": entry["id"],
                        "disc_leaf": disc_leaf,
                        "stat_leaf": stat_leaf,
                        "disc_topic": entry["disc_topic"],
                        "stat_t": stat_t,
                    }
                )

        if "vh_configuration_" in str(entry["disc_topic"]) and int(entry["id"]) != 74:
            findings["mqttha_vh_config_trigger_issues"].append(
                {"line": entry["line"], "id": entry["id"], "disc_topic": entry["disc_topic"]}
            )

        if int(entry["id"]) == 58 and disc_leaf == "Hcratio":
            if not (isinstance(stat_t, str) and stat_t.endswith("/Hcratio")):
                findings["mqttha_hcratio_discovery_issues"].append(
                    {"line": entry["line"], "disc_topic": entry["disc_topic"], "stat_t": stat_t}
                )

        if int(entry["id"]) == 35:
            if disc_leaf == "FanSpeed":
                findings["mqttha_fanspeed_discovery_issues"].append(
                    {"line": entry["line"], "issue": "legacy single FanSpeed discovery still present"}
                )
            if disc_leaf == "FanSpeed_setpoint_hz":
                if stat_t != "%mqtt_pub_topic%/FanSpeed_hb_u8" or uom != "Hz":
                    findings["mqttha_fanspeed_discovery_issues"].append(
                        {
                            "line": entry["line"],
                            "issue": "FanSpeed setpoint discovery mismatch",
                            "stat_t": stat_t,
                            "unit_of_measurement": uom,
                        }
                    )
            if disc_leaf == "FanSpeed_actual_hz":
                if stat_t != "%mqtt_pub_topic%/FanSpeed_lb_u8" or uom != "Hz":
                    findings["mqttha_fanspeed_discovery_issues"].append(
                        {
                            "line": entry["line"],
                            "issue": "FanSpeed actual discovery mismatch",
                            "stat_t": stat_t,
                            "unit_of_measurement": uom,
                        }
                    )

        # Direct exact topic entries only: compare units to spec (avoid split/derived/template semantics)
        if (
            isinstance(disc_leaf, str)
            and isinstance(stat_leaf, str)
            and disc_leaf == stat_leaf
            and disc_leaf == otmap.get(int(entry["id"]), {}).get("label")
            and int(entry["id"]) in spec_ids
        ):
            spec_unit = str(spec_ids[int(entry["id"])]["unit"]).replace("—", "").strip()
            cfg_unit = str(uom or "").strip()
            if spec_unit and cfg_unit and spec_unit != cfg_unit:
                findings["mqttha_direct_unit_semantic_issues"].append(
                    {
                        "line": entry["line"],
                        "id": entry["id"],
                        "label": disc_leaf,
                        "spec_unit": spec_unit,
                        "cfg_unit": cfg_unit,
                    }
                )

    metadata = {
        "legacy_reserved_guard": legacy_guard,
    }
    return matrix_rows, findings, metadata


def write_csv(path: Path, rows: List[Dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def write_json(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def summarize_failures(findings: Dict[str, List[Dict[str, Any]]]) -> Tuple[int, Dict[str, int]]:
    fail_counts = {k: len(findings.get(k, [])) for k in FAILING_FINDING_KEYS}
    total_fail = sum(fail_counts.values())
    return total_fail, fail_counts


def _print_counts(counts: Dict[str, int], title: str) -> None:
    print(title)
    for key, value in counts.items():
        print(f"  {key}: {value}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Spec-driven OpenTherm v4.2 audit for firmware + mqttha.cfg")
    parser.add_argument("--root", type=Path, default=Path("."), help="Repository root (default: current directory)")
    parser.add_argument("--spec", type=Path, default=DEFAULT_SPEC, help="Path to v4.2 Markdown spec")
    parser.add_argument("--header", type=Path, default=DEFAULT_HEADER, help="Path to OTGW-Core.h")
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="Path to OTGW-Core.ino")
    parser.add_argument("--mqttha", type=Path, default=DEFAULT_MQTTHA, help="Path to mqttha.cfg")
    parser.add_argument("--matrix-out", type=Path, default=DEFAULT_MATRIX_OUT, help="CSV matrix output path")
    parser.add_argument("--report-out", type=Path, default=DEFAULT_REPORT_OUT, help="JSON report output path")
    parser.add_argument("--check", action="store_true", help="Return non-zero if failing findings are present")
    parser.add_argument("--quiet", action="store_true", help="Suppress detailed summary output")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()

    spec_path = (root / args.spec).resolve()
    header_path = (root / args.header).resolve()
    source_path = (root / args.source).resolve()
    mqttha_path = (root / args.mqttha).resolve()
    matrix_out = (root / args.matrix_out).resolve()
    report_out = (root / args.report_out).resolve()

    for path in (spec_path, header_path, source_path, mqttha_path):
        if not path.exists():
            print(f"ERROR: missing required file: {path}", file=sys.stderr)
            return 2

    spec_text = _read_text(spec_path)
    header_text = _read_text(header_path)
    source_text = _read_text(source_path)
    mqttha_text = _read_text(mqttha_path)

    spec_ids = parse_spec_ids(spec_text)
    enum_map = parse_enum_map(header_text)
    otmap = parse_otmap(header_text)
    decode_cases = parse_decode_cases(source_text, enum_map)
    mqttha_entries = parse_mqttha_entries(mqttha_text)

    matrix_rows, findings, metadata = build_audit(spec_ids, enum_map, otmap, decode_cases, mqttha_entries, source_text)

    report = {
        "counts": {k: len(v) for k, v in findings.items()},
        "matrix_summary": {
            "spec_ids": len(spec_ids),
            "implemented_spec_ids": sum(1 for r in matrix_rows if r["otmap_line"] is not None),
            "main_decoded_spec_ids": sum(1 for r in matrix_rows if str(r.get("decode_print_fn") or "").startswith("print_")),
        },
        "metadata": {
            "inputs": {
                "spec": str(spec_path.relative_to(root)),
                "header": str(header_path.relative_to(root)),
                "source": str(source_path.relative_to(root)),
                "mqttha": str(mqttha_path.relative_to(root)),
            },
            **metadata,
        },
        "findings": findings,
    }

    write_csv(matrix_out, matrix_rows)
    write_json(report_out, report)

    total_fail, fail_counts = summarize_failures(findings)
    if not args.quiet:
        print(f"Spec-driven OpenTherm v4.2 audit complete")
        print(f"  Matrix:  {matrix_out}")
        print(f"  Report:  {report_out}")
        _print_counts(report["matrix_summary"], "Matrix summary:")
        _print_counts(report["counts"], "Finding counts:")
        _print_counts(fail_counts, "Failing categories:")
        print(f"  failing_total: {total_fail}")

    if args.check and total_fail > 0:
        print("FAIL: spec-driven audit found regressions", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
