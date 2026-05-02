#!/usr/bin/env python3
"""
OTGW-firmware Workspace Evaluation Framework

This script performs comprehensive evaluation of the workspace including:
- Code quality metrics
- Build system validation
- Dependency health checks
- Documentation coverage
- Security analysis
- Memory and resource analysis
- Test coverage analysis

Usage:
    python evaluate.py              # Full evaluation
    python evaluate.py --quick      # Quick check (essentials only)
    python evaluate.py --report     # Generate detailed report
    python evaluate.py --fix        # Auto-fix issues where possible
"""

import argparse
import io
import json
import re
import subprocess
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Any

# Ensure Unicode output works on Windows consoles (cp1252 etc.)
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') != 'utf8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
if sys.stderr.encoding and sys.stderr.encoding.lower().replace('-', '') != 'utf8':
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

import config

class Colors:
    """ANSI color codes"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

    @staticmethod
    def disable():
        # Suppress type checker warnings for intentional constant reassignment
        Colors.HEADER = Colors.OKBLUE = Colors.OKCYAN = ''  # type: ignore
        Colors.OKGREEN = Colors.WARNING = Colors.FAIL = Colors.ENDC = Colors.BOLD = ''  # type: ignore


class EvaluationResult:
    """Store evaluation results"""
    def __init__(self, category: str, name: str, status: str, message: str, details: str = ""):
        self.category = category
        self.name = name
        self.status = status  # PASS, WARN, FAIL, INFO
        self.message = message
        self.details = details
        self.timestamp = datetime.now()

    def __repr__(self):
        icon = {"PASS": "✓", "WARN": "⚠", "FAIL": "✗", "INFO": "ℹ"}.get(self.status, "?")
        color = {"PASS": Colors.OKGREEN, "WARN": Colors.WARNING, "FAIL": Colors.FAIL, "INFO": Colors.OKCYAN}.get(self.status, "")
        return f"{color}{icon} [{self.category}] {self.name}: {self.message}{Colors.ENDC}"


class WorkspaceEvaluator:
    """Main evaluation framework"""
    
    def __init__(self, project_dir: Path, verbose: bool = False):
        self.project_dir = project_dir
        self.verbose = verbose
        self.results: List[EvaluationResult] = []
        self.stats: Dict[str, int] = defaultdict(int)
        
    def add_result(self, result: EvaluationResult):
        """Add evaluation result and update stats"""
        self.results.append(result)
        self.stats[result.status] += 1
        if self.verbose or result.status in ["FAIL", "WARN"]:
            print(result)

    def run_command(self, cmd: List[str], capture: bool = True) -> Tuple[int, str, str]:
        """Run command and return (returncode, stdout, stderr)"""
        try:
            result = subprocess.run(
                cmd,
                cwd=self.project_dir,
                capture_output=capture,
                text=True,
                timeout=60
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return -1, "", "Command timeout"
        except Exception as e:
            return -1, "", str(e)

    # ===== CODE QUALITY CHECKS =====
    
    def check_code_structure(self):
        """Analyze code structure and organization"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Code Structure Analysis ==={Colors.ENDC}")
        
        # Check for required files
        required_files: List[Path] = [
            config.FIRMWARE_ROOT / "OTGW-firmware.ino",
            config.FIRMWARE_ROOT / "OTGW-firmware.h",
            config.FIRMWARE_ROOT / "version.h",
            Path("README.md"),
            Path("LICENSE")
        ]
        
        for file in required_files:
            file_path = self.project_dir / file
            if file_path.exists():
                self.add_result(EvaluationResult(
                    "Structure", f"Required file: {file}", "PASS",
                    f"Found ({file_path.stat().st_size} bytes)"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Structure", f"Required file: {file}", "FAIL",
                    "Missing required file"
                ))

        # Check .ino file organization
        src_dir = config.FIRMWARE_ROOT
        ino_files = list(src_dir.glob("*.ino"))
        self.add_result(EvaluationResult(
            "Structure", "INO modules", "INFO",
            f"Found {len(ino_files)} Arduino modules",
            ", ".join([f.name for f in ino_files])
        ))

        # Check for proper header guards in .h files
        h_files = list(src_dir.glob("*.h"))
        for h_file in h_files:
            with open(h_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                if '#ifndef' in content and '#define' in content:
                    self.add_result(EvaluationResult(
                        "Structure", f"Header guard: {h_file.name}", "PASS",
                        "Has header guards"
                    ))
                else:
                    self.add_result(EvaluationResult(
                        "Structure", f"Header guard: {h_file.name}", "WARN",
                        "Missing or incomplete header guards"
                    ))

    def check_time_boundary_single_caller(self):
        """ADR-064 binding rule: each consume-on-read time-boundary helper
        (minuteChanged / hourChanged / dayChanged / yearChanged) must have
        exactly ONE call site firmware-wide. A second caller silently steals
        the event.

        Enforcement per ADR-080 meta-rule. Fails on >1 call site.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR-064 Time-Boundary Single-Caller ==={Colors.ENDC}")

        helpers = ["minuteChanged", "hourChanged", "dayChanged", "yearChanged"]
        src_dir = config.FIRMWARE_ROOT
        # Scan all C/C++ source files under firmware root (not library subtree).
        source_files: List[Path] = []
        for pattern in ("*.ino", "*.cpp", "*.h"):
            source_files.extend(src_dir.glob(pattern))

        for helper in helpers:
            # Pattern: helper name followed by '(' — catches calls and declarations.
            # We subtract definitions (helperStuff.ino: 'bool <name>(){') from call count.
            call_pattern = re.compile(rf"\b{helper}\s*\(")
            definition_pattern = re.compile(rf"^\s*bool\s+{helper}\s*\(")

            call_sites: List[Tuple[str, int, str]] = []

            for src in source_files:
                try:
                    with open(src, 'r', encoding='utf-8', errors='ignore') as f:
                        for lineno, line in enumerate(f, 1):
                            stripped = line.lstrip()
                            # Skip pure comment lines so comments mentioning
                            # xChanged() aren't counted as calls.
                            if stripped.startswith(("//", "*", "/*")):
                                continue
                            # Skip the definition line itself so 'bool hourChanged(){'
                            # isn't counted as a call.
                            if definition_pattern.match(line):
                                continue
                            if call_pattern.search(line):
                                call_sites.append((src.name, lineno, line.rstrip()))
                except (OSError, UnicodeDecodeError):
                    continue

            if len(call_sites) == 1:
                loc = f"{call_sites[0][0]}:{call_sites[0][1]}"
                self.add_result(EvaluationResult(
                    "ADR-064", f"{helper}() single caller", "PASS",
                    f"Exactly 1 call site at {loc}"
                ))
            elif len(call_sites) == 0:
                self.add_result(EvaluationResult(
                    "ADR-064", f"{helper}() single caller", "WARN",
                    "No call sites found (dead code or helper removed)"
                ))
            else:
                detail = "; ".join(f"{n}:{ln}" for n, ln, _ in call_sites)
                self.add_result(EvaluationResult(
                    "ADR-064", f"{helper}() single caller", "FAIL",
                    f"Found {len(call_sites)} call sites — ADR-064 requires exactly 1",
                    detail
                ))

    # ---- Helpers for body extraction (shared by ADR-062 / TASK-354 gates) ----

    @staticmethod
    def _extract_function_body(source: str, signature_start: int) -> Tuple[str, int]:
        """Starting at ``signature_start`` (index of the signature's first char),
        walk forward to the first '{' and return (body, end_index) where body
        is the text enclosed in the outermost matching braces and end_index is
        the position just after the closing '}'. Returns ('', -1) if the body
        can't be delimited. Handles single-line // and /* */ comments and
        simple "..." / '...' string literals so that braces inside them are
        not counted.
        """
        i = source.find('{', signature_start)
        if i == -1:
            return '', -1
        depth = 0
        n = len(source)
        body_start = i
        while i < n:
            c = source[i]
            # // line comment
            if c == '/' and i + 1 < n and source[i + 1] == '/':
                nl = source.find('\n', i)
                if nl == -1:
                    return '', -1
                i = nl + 1
                continue
            # /* block comment */
            if c == '/' and i + 1 < n and source[i + 1] == '*':
                end = source.find('*/', i + 2)
                if end == -1:
                    return '', -1
                i = end + 2
                continue
            # string literal "..."
            if c == '"':
                i += 1
                while i < n and source[i] != '"':
                    if source[i] == '\\' and i + 1 < n:
                        i += 2
                        continue
                    i += 1
                i += 1
                continue
            # char literal '...'
            if c == "'":
                i += 1
                while i < n and source[i] != "'":
                    if source[i] == '\\' and i + 1 < n:
                        i += 2
                        continue
                    i += 1
                i += 1
                continue
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return source[body_start:i + 1], i + 1
            i += 1
        return '', -1

    # ===== ADR-062 DISCOVERY COUNTER GATES (TASK-349/364) =====

    def check_discovery_counter_instrumented(self):
        """ADR-062 binding rule: every ``bool stream*Discovery(`` helper in
        ``mqtt_configuratie.cpp`` must contain at least one ``incPublishedTopicCount()``
        call. Without this, a newly-added helper would silently under-count its
        retained-discovery publishes, causing the daily verify pass to see a
        false-missing state and republish the entire discovery set.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR-062 Discovery Counter Instrumented ==={Colors.ENDC}")

        cpp = config.FIRMWARE_ROOT / "mqtt_configuratie.cpp"
        if not cpp.exists():
            self.add_result(EvaluationResult(
                "ADR-062", "Discovery counter instrumented", "WARN",
                "mqtt_configuratie.cpp not found — cannot verify"
            ))
            return

        try:
            source = cpp.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-062", "Discovery counter instrumented", "FAIL",
                f"Could not read mqtt_configuratie.cpp: {e}"
            ))
            return

        # Find every "bool streamXxxDiscovery(" signature. Anchor to start of
        # line so forward declarations in .h headers (none today, but safe)
        # aren't mis-matched.
        sig_re = re.compile(r"^bool\s+(stream\w*Discovery)\s*\(", re.MULTILINE)
        matches = list(sig_re.finditer(source))

        if not matches:
            self.add_result(EvaluationResult(
                "ADR-062", "Discovery counter instrumented", "WARN",
                "No bool stream*Discovery( helpers found — file restructured?"
            ))
            return

        missing: List[str] = []
        covered: List[str] = []
        for m in matches:
            name = m.group(1)
            body, _ = self._extract_function_body(source, m.start())
            if not body:
                missing.append(f"{name} (body not parseable)")
                continue
            # Count inc calls inside body (tolerate comment stripping being
            # approximate — _extract_function_body already skipped comments
            # inside the body walk for brace matching, but the body text
            # itself may still contain // comments; a simple string search
            # is adequate for "at least one call").
            if re.search(r"\bincPublishedTopicCount\s*\(", body):
                covered.append(name)
            else:
                missing.append(name)

        detail = (
            f"covered={len(covered)}: " + ", ".join(covered) +
            (f"; missing: {', '.join(missing)}" if missing else "")
        )
        if missing:
            self.add_result(EvaluationResult(
                "ADR-062", "Discovery counter instrumented", "FAIL",
                f"{len(missing)} of {len(matches)} stream*Discovery helpers miss incPublishedTopicCount()",
                detail
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-062", "Discovery counter instrumented", "PASS",
                f"All {len(matches)} stream*Discovery helpers call incPublishedTopicCount()",
                detail
            ))

    def check_publishedtopic_counter_reset(self):
        """ADR-062 binding rule: ``iPublishedTopicCount`` must be reset to 0 at
        least once in the firmware, typically inside ``clearMQTTConfigDone``
        (or equivalent reset path). Without this, the counter would drift and
        make the verify-vs-publish comparison meaningless after a discovery
        bitmap clear.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR-062 PublishedTopic Counter Reset ==={Colors.ENDC}")

        src_dir = config.FIRMWARE_ROOT
        source_files: List[Path] = []
        for pattern in ("*.ino", "*.cpp", "*.h"):
            source_files.extend(src_dir.glob(pattern))

        reset_re = re.compile(r"iPublishedTopicCount\s*=\s*0")
        hits: List[Tuple[str, int, str]] = []
        for src in source_files:
            try:
                with open(src, 'r', encoding='utf-8', errors='ignore') as f:
                    for lineno, line in enumerate(f, 1):
                        stripped = line.lstrip()
                        # Skip declarations of the form "uint32_t iPublishedTopicCount = 0;"
                        # inside the struct definition — those are default
                        # initialisers, not reset paths. Heuristic: skip lines
                        # that contain a type keyword before the identifier.
                        if re.search(r"\b(uint\d+_t|int|unsigned|long|size_t)\b[^;]*iPublishedTopicCount\s*=\s*0", line):
                            continue
                        if reset_re.search(stripped):
                            hits.append((src.name, lineno, stripped.rstrip()))
            except (OSError, UnicodeDecodeError):
                continue

        if not hits:
            self.add_result(EvaluationResult(
                "ADR-062", "PublishedTopic counter reset", "FAIL",
                "No reset path found — iPublishedTopicCount will drift after clearMQTTConfigDone()"
            ))
            return

        # Prefer a reset inside a clearMQTTConfigDone / markAllMQTTConfigPending
        # style function; flag WARN if the only reset lives somewhere unexpected.
        blessed: List[str] = []
        for name, lineno, _ in hits:
            if name.endswith((".ino", ".cpp")):
                try:
                    src_path = src_dir / name
                    text = src_path.read_text(encoding='utf-8', errors='ignore')
                    # Look backwards 40 lines for a function header.
                    lines = text.split('\n')
                    window = "\n".join(lines[max(0, lineno - 40):lineno])
                    if re.search(r"\b(clearMQTTConfigDone|markAllMQTTConfigPending|resetDiscovery\w*)\s*\(", window):
                        blessed.append(f"{name}:{lineno}")
                except OSError:
                    pass

        detail = "; ".join(f"{n}:{ln}" for n, ln, _ in hits)
        if blessed:
            self.add_result(EvaluationResult(
                "ADR-062", "PublishedTopic counter reset", "PASS",
                f"Reset found in blessed function(s): {', '.join(blessed)}",
                detail
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-062", "PublishedTopic counter reset", "WARN",
                f"Reset present ({len(hits)} site(s)) but not inside clearMQTTConfigDone / markAllMQTTConfigPending",
                detail
            ))

    # ===== HA DISCOVERY INDEX CONSISTENCY (TASK-392) =====

    def check_ha_sensor_index_consistency(self):
        """Verify that mqttHaSensorIndex[] points to the correct first-entry row
        for every OT ID that has entries in mqttHaSensors[], and 0xFFFF for IDs
        with no entries. A mismatch means discovery for that ID will either
        never publish (index=0xFFFF for an ID that has entries) or will publish
        the wrong rows (index points to a different ID's rows).

        TASK-392: catches the ID-247 class of bug where a pseudo-ID is added to
        mqttHaSensors[] but the sibling index table is not updated.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== HA Sensor Index Consistency ==={Colors.ENDC}")

        cpp = config.FIRMWARE_ROOT / "mqtt_configuratie.cpp"
        if not cpp.exists():
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "WARN",
                "mqtt_configuratie.cpp not found — cannot verify"
            ))
            return

        try:
            source = cpp.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "FAIL",
                f"Could not read mqtt_configuratie.cpp: {e}"
            ))
            return

        # --- Parse mqttHaSensors[] body ---
        sensors_block_re = re.compile(
            r"const\s+MqttHaSensorCfg\s+PROGMEM\s+mqttHaSensors\s*\[\s*\]\s*=\s*\{(.*?)\n\}\s*;",
            re.DOTALL,
        )
        m = sensors_block_re.search(source)
        if not m:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "FAIL",
                "Could not locate mqttHaSensors[] array — file restructured?"
            ))
            return

        sensors_body = m.group(1)
        row_re = re.compile(r"\{\s*(\d+)\s*,")
        ids_in_order: List[int] = []
        for line in sensors_body.split('\n'):
            code = line.split('//', 1)[0]  # strip // comments to avoid matching digits in prose
            for rm in row_re.finditer(code):
                ids_in_order.append(int(rm.group(1)))

        if not ids_in_order:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "WARN",
                "mqttHaSensors[] body parsed but no {id,...} rows matched"
            ))
            return

        # Build id -> (first_row_index, count)
        first_index: dict = {}
        count_per_id: dict = {}
        for row_idx, id_val in enumerate(ids_in_order):
            if id_val not in first_index:
                first_index[id_val] = row_idx
            count_per_id[id_val] = count_per_id.get(id_val, 0) + 1

        # --- Parse mqttHaSensorIndex[256] body ---
        index_block_re = re.compile(
            r"const\s+uint16_t\s+PROGMEM\s+mqttHaSensorIndex\s*\[\s*256\s*\]\s*=\s*\{(.*?)\n\}\s*;",
            re.DOTALL,
        )
        m2 = index_block_re.search(source)
        if not m2:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "FAIL",
                "Could not locate mqttHaSensorIndex[256] array — file restructured?"
            ))
            return

        index_body = m2.group(1)
        # Comments are already stripped per line; match just the leading value.
        # Last array entry in C may omit the trailing comma.
        val_re = re.compile(r"^\s*(?:(0x[0-9A-Fa-f]+)|(\d+))\b")
        index_vals: List[int] = []
        for line in index_body.split('\n'):
            code = line.split('//', 1)[0]
            vm = val_re.match(code)
            if not vm:
                continue
            if vm.group(1):
                index_vals.append(int(vm.group(1), 16))
            else:
                index_vals.append(int(vm.group(2)))

        if len(index_vals) != 256:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "FAIL",
                f"mqttHaSensorIndex[256] parsed {len(index_vals)} values; expected 256"
            ))
            return

        # --- Validate consistency ---
        MQTT_HA_INDEX_NONE = 0xFFFF
        mismatches: List[str] = []
        for otid in range(256):
            idx_val = index_vals[otid]
            expected = first_index.get(otid, MQTT_HA_INDEX_NONE)
            if idx_val != expected:
                cnt = count_per_id.get(otid, 0)
                mismatches.append(
                    f"id {otid}: index={hex(idx_val) if idx_val == MQTT_HA_INDEX_NONE else idx_val} "
                    f"expected={hex(expected) if expected == MQTT_HA_INDEX_NONE else expected} "
                    f"(rows in mqttHaSensors[]={cnt})"
                )

        total_ids_with_entries = len(first_index)
        total_entries = sum(count_per_id.values())
        if mismatches:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "FAIL",
                f"{len(mismatches)} mismatch(es) across {total_ids_with_entries} IDs with entries",
                "; ".join(mismatches[:10]) + (" ..." if len(mismatches) > 10 else "")
            ))
        else:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "PASS",
                f"All {total_ids_with_entries} IDs indexed correctly ({total_entries} sensor entries total)"
            ))

    # ===== JSON BUFFER ARITHMETIC (TASK-368) =====

    def check_json_buffer_arithmetic(self):
        """Compute worst-case output length of ``snprintf_P(buf, sizeof(buf), PSTR(fmt), ...)``
        calls inside ``sendMQTTheapdiag`` and fail if it exceeds the buffer.

        Scope note: first iteration is narrowly scoped to the
        ``sendMQTTheapdiag`` function in ``MQTTstuff.ino`` because it was the
        Phase 2B perf review's concrete concern (TASK-352 raised 384->512 after
        measuring a 465-byte worst case). Expand scoping in a follow-up task
        once the parser handles non-numeric conversions robustly.

        Conversion budgeting:
          %lu -> 10 (uint32 max = 4294967295, 10 digits)
          %ld -> 11 (signed int32, sign + 10 digits)
          %u  -> 10 (treated as uint32 worst-case — safer than uint16=5)
          %d  -> 11 (signed, sign + 10 digits)
          %s  -> 0 with a warning (not inferrable without type info)
          %%  -> 1 literal percent
          literal bytes in the format string -> counted as-is
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== JSON Buffer Arithmetic (sendMQTTheapdiag) ==={Colors.ENDC}")

        mqtt_ino = config.FIRMWARE_ROOT / "MQTTstuff.ino"
        if not mqtt_ino.exists():
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "WARN",
                "MQTTstuff.ino not found — cannot verify"
            ))
            return

        try:
            source = mqtt_ino.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "FAIL",
                f"Could not read MQTTstuff.ino: {e}"
            ))
            return

        sig_re = re.compile(r"^void\s+sendMQTTheapdiag\s*\(", re.MULTILINE)
        m = sig_re.search(source)
        if not m:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "WARN",
                "sendMQTTheapdiag() not found"
            ))
            return

        body, _ = self._extract_function_body(source, m.start())
        if not body:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "FAIL",
                "Could not parse sendMQTTheapdiag() body"
            ))
            return

        # Extract buffer size: "char json[512];" or similar.
        buf_decl = re.search(r"\bchar\s+(\w+)\s*\[\s*(\d+)\s*\]", body)
        if not buf_decl:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "WARN",
                "No 'char X[N]' buffer declaration found in body"
            ))
            return
        buf_name = buf_decl.group(1)
        buf_size = int(buf_decl.group(2))

        # Pull the first snprintf_P(buf_name, sizeof(buf_name), PSTR("..."...), ...).
        # Capture the quoted format string; handle the PSTR("a" "b" "c") adjacent-
        # string concatenation idiom by grabbing everything between PSTR( and the
        # matching ) and then extracting every "..." inside.
        snp_re = re.compile(
            rf"snprintf_P\s*\(\s*{re.escape(buf_name)}\s*,\s*sizeof\(\s*{re.escape(buf_name)}\s*\)\s*,\s*PSTR\s*\(",
            re.DOTALL
        )
        sm = snp_re.search(body)
        if not sm:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "WARN",
                f"No snprintf_P({buf_name}, sizeof({buf_name}), PSTR(...), ...) pattern found"
            ))
            return

        # Walk from end of PSTR( marker, balance parens to find matching ')'.
        i = sm.end()
        depth = 1
        n = len(body)
        pstr_start = i
        pstr_end = -1
        while i < n and depth > 0:
            c = body[i]
            if c == '"':
                # skip string literal
                i += 1
                while i < n and body[i] != '"':
                    if body[i] == '\\' and i + 1 < n:
                        i += 2
                        continue
                    i += 1
                i += 1
                continue
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    pstr_end = i
                    break
            i += 1

        if pstr_end == -1:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "FAIL",
                "Could not balance parens around PSTR(...)"
            ))
            return

        pstr_segment = body[pstr_start:pstr_end]
        # Extract every "..." chunk and concatenate.
        chunk_re = re.compile(r'"((?:[^"\\]|\\.)*)"')
        fmt_chars = ''.join(
            # Turn common escape sequences back into a single char for counting;
            # this is worst-case byte count of runtime output.
            chunk.encode('latin-1', 'backslashreplace').decode('unicode_escape')
            for chunk in chunk_re.findall(pstr_segment)
        )

        # Walk format string, budgeting each conversion.
        worst = 0
        unknown: List[str] = []
        i = 0
        while i < len(fmt_chars):
            c = fmt_chars[i]
            if c != '%':
                worst += 1
                i += 1
                continue
            # Consume optional flags/width/precision/length modifier.
            j = i + 1
            # flags
            while j < len(fmt_chars) and fmt_chars[j] in "-+ #0":
                j += 1
            # width (digits)
            while j < len(fmt_chars) and fmt_chars[j].isdigit():
                j += 1
            # precision
            if j < len(fmt_chars) and fmt_chars[j] == '.':
                j += 1
                while j < len(fmt_chars) and fmt_chars[j].isdigit():
                    j += 1
            # length modifiers (l, ll, h, hh, z, j, t, L)
            length = ''
            while j < len(fmt_chars) and fmt_chars[j] in "lhzjtL":
                length += fmt_chars[j]
                j += 1
            if j >= len(fmt_chars):
                # dangling %
                worst += 1
                i = j
                continue
            conv = fmt_chars[j]
            if conv == '%':
                worst += 1
            elif conv in ('u', 'd', 'i'):
                # worst-case 32-bit signed or unsigned: 10 or 11 chars.
                worst += 11 if conv in ('d', 'i') else 10
            elif conv in ('x', 'X', 'o'):
                worst += 8  # 32-bit hex/octal upper bound
            elif conv == 'c':
                worst += 1
            elif conv == 's':
                unknown.append("%s (skipped, length unknown)")
            elif conv in ('f', 'F', 'e', 'E', 'g', 'G'):
                worst += 24  # conservative double
            else:
                unknown.append(f"%{conv} (unknown conv)")
            i = j + 1

        # Plus NUL terminator.
        required = worst + 1
        headroom = buf_size - required
        detail_parts = [f"buf={buf_size}", f"worst={worst}", f"required(+NUL)={required}", f"headroom={headroom}"]
        if unknown:
            detail_parts.append("skipped: " + ", ".join(unknown))
        detail = ", ".join(detail_parts)

        if required > buf_size:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "FAIL",
                f"Buffer too small: needs {required} bytes, have {buf_size}",
                detail
            ))
        elif headroom < 16:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "WARN",
                f"Buffer has only {headroom} bytes of headroom (< 16 recommended)",
                detail
            ))
        else:
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "PASS",
                f"Buffer adequate: {headroom} bytes headroom",
                detail
            ))

    # ===== STATUS BURST TUNING (TASK-353/368) =====

    def check_status_burst_cooldown_bound(self):
        """Guard against regressing ``STATUS_BURST_COOLDOWN_MS`` back to a
        large value. TASK-353 tuned it from 10000 -> 2000 to fit Crashevans'
        status cadence; any value >= 3000 should carry a ``// verified tuning``
        marker on one of the preceding 5 lines to prove it was re-validated.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== STATUS_BURST_COOLDOWN_MS Tuning Bound ==={Colors.ENDC}")

        mqtt_ino = config.FIRMWARE_ROOT / "MQTTstuff.ino"
        if not mqtt_ino.exists():
            self.add_result(EvaluationResult(
                "Tuning", "STATUS_BURST_COOLDOWN_MS bound", "WARN",
                "MQTTstuff.ino not found"
            ))
            return

        try:
            lines = mqtt_ino.read_text(encoding='utf-8', errors='ignore').split('\n')
        except OSError as e:
            self.add_result(EvaluationResult(
                "Tuning", "STATUS_BURST_COOLDOWN_MS bound", "FAIL",
                f"Could not read MQTTstuff.ino: {e}"
            ))
            return

        decl_re = re.compile(r"\bSTATUS_BURST_COOLDOWN_MS\s*=\s*(\d+)")
        found = False
        for idx, line in enumerate(lines):
            # Skip comments that just mention the constant in prose.
            if line.lstrip().startswith("//"):
                continue
            m = decl_re.search(line)
            if not m:
                continue
            found = True
            value = int(m.group(1))
            lineno = idx + 1
            if value < 3000:
                self.add_result(EvaluationResult(
                    "Tuning", "STATUS_BURST_COOLDOWN_MS bound", "PASS",
                    f"STATUS_BURST_COOLDOWN_MS = {value} ms (< 3000)",
                    f"MQTTstuff.ino:{lineno}"
                ))
            else:
                window = "\n".join(lines[max(0, idx - 5):idx])
                if "verified tuning" in window:
                    self.add_result(EvaluationResult(
                        "Tuning", "STATUS_BURST_COOLDOWN_MS bound", "PASS",
                        f"STATUS_BURST_COOLDOWN_MS = {value} ms carries 'verified tuning' marker",
                        f"MQTTstuff.ino:{lineno}"
                    ))
                else:
                    self.add_result(EvaluationResult(
                        "Tuning", "STATUS_BURST_COOLDOWN_MS bound", "FAIL",
                        f"STATUS_BURST_COOLDOWN_MS = {value} ms (>= 3000) without '// verified tuning' marker",
                        f"MQTTstuff.ino:{lineno}"
                    ))
            break
        if not found:
            self.add_result(EvaluationResult(
                "Tuning", "STATUS_BURST_COOLDOWN_MS bound", "WARN",
                "STATUS_BURST_COOLDOWN_MS declaration not found"
            ))

    def check_status_publishers_wrap_burst(self):
        """TASK-347/354: every status-publisher function whose name matches
        ``publish(Master|Slave)Status.*State`` in ``OTGW-Core.ino`` must wrap
        its sub-topic fanout with ``beginStatusBurst(`` and ``endStatusBurst(``
        so the drip loop and heap-health gate can quiesce during the burst.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Status Publishers Wrap Burst ==={Colors.ENDC}")

        core_ino = config.FIRMWARE_ROOT / "OTGW-Core.ino"
        if not core_ino.exists():
            self.add_result(EvaluationResult(
                "ADR-062", "Status publishers wrap burst", "WARN",
                "OTGW-Core.ino not found"
            ))
            return

        try:
            source = core_ino.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-062", "Status publishers wrap burst", "FAIL",
                f"Could not read OTGW-Core.ino: {e}"
            ))
            return

        # Signatures like "static void publishMasterStatusState(..."
        sig_re = re.compile(
            r"^\s*(?:static\s+)?void\s+(publish(?:Master|Slave)Status\w*State)\s*\(",
            re.MULTILINE
        )
        matches = list(sig_re.finditer(source))
        if not matches:
            self.add_result(EvaluationResult(
                "ADR-062", "Status publishers wrap burst", "WARN",
                "No publish(Master|Slave)Status*State functions found"
            ))
            return

        missing: List[str] = []
        covered: List[str] = []
        for m in matches:
            name = m.group(1)
            body, _ = self._extract_function_body(source, m.start())
            if not body:
                missing.append(f"{name} (body not parseable)")
                continue
            has_begin = bool(re.search(r"\bbeginStatusBurst\s*\(", body))
            has_end = bool(re.search(r"\bendStatusBurst\s*\(", body))
            if has_begin and has_end:
                covered.append(name)
            else:
                parts = []
                if not has_begin: parts.append("no beginStatusBurst(")
                if not has_end:   parts.append("no endStatusBurst(")
                missing.append(f"{name} [{', '.join(parts)}]")

        detail = f"covered={len(covered)}: " + ", ".join(covered)
        if missing:
            detail += f"; missing: {', '.join(missing)}"
            self.add_result(EvaluationResult(
                "ADR-062", "Status publishers wrap burst", "FAIL",
                f"{len(missing)} of {len(matches)} status publishers miss burst wrap",
                detail
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-062", "Status publishers wrap burst", "PASS",
                f"All {len(matches)} status publishers wrap begin/endStatusBurst()",
                detail
            ))

    def check_ps_summary_master_topic_gate(self):
        """ADR-066 amendment 2026-05-02 (TASK-483 ACs #8-#13):
        ``publishPSSummaryFieldValue`` in ``OTGW-Core.ino`` must compute
        ``validForMaster = is_msgid_valid_for_master_topic_in_ps_summary(...)``
        and gate every ``sendMQTTData(...)`` / ``publishPSSummarySplitBytes(...)``
        call plus every ``OTcurrentSystemState.X = ...`` assignment on it.
        Without the gate, the PS=1 summary path bypasses the master-topic
        invariant for non-echo MsgIDs (Tr / TrSet / TrSetCH2 / TRoomCH2 /
        MaxRelModLevelSetting / RFsensorStatus) and reintroduces the v1.4.x
        flapping regression.

        The ``ot_flag8flag8`` case is excluded: it has its own per-MsgID
        switch (status-flag semantics, parallel to the OT_Statusflags
        exception in ``is_value_valid_for_master_topic``).
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== PS=1 Summary Master-Topic Gate ==={Colors.ENDC}")

        core_ino = config.FIRMWARE_ROOT / "OTGW-Core.ino"
        if not core_ino.exists():
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "WARN",
                "OTGW-Core.ino not found"
            ))
            return

        try:
            source = core_ino.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "FAIL",
                f"Could not read OTGW-Core.ino: {e}"
            ))
            return

        sig_re = re.compile(r"^\s*static\s+bool\s+publishPSSummaryFieldValue\s*\(", re.MULTILINE)
        m = sig_re.search(source)
        if not m:
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "FAIL",
                "publishPSSummaryFieldValue function not found in OTGW-Core.ino"
            ))
            return

        body, _ = self._extract_function_body(source, m.start())
        if not body:
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "FAIL",
                "publishPSSummaryFieldValue body not parseable"
            ))
            return

        guard_re = re.compile(
            r"\bvalidForMaster\s*=\s*is_msgid_valid_for_master_topic_in_ps_summary\s*\("
        )
        if not guard_re.search(body):
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "FAIL",
                "publishPSSummaryFieldValue does not compute validForMaster from "
                "is_msgid_valid_for_master_topic_in_ps_summary()"
            ))
            return

        value_cases = ['ot_f88', 'ot_s16', 'ot_u16', 'ot_s8s8', 'ot_u8u8', 'ot_u8']
        case_split = re.split(r"^\s*case\s+(ot_\w+)\s*:", body, flags=re.MULTILINE)
        cases = {}
        for i in range(1, len(case_split), 2):
            cases[case_split[i]] = case_split[i + 1] if i + 1 < len(case_split) else ""

        issues: List[str] = []
        for case_name in value_cases:
            case_body = cases.get(case_name)
            if case_body is None:
                issues.append(f"{case_name}: case missing")
                continue

            lines = case_body.splitlines()
            for line_no, line in enumerate(lines):
                stripped = line.strip()
                is_publish = (
                    'sendMQTTData(' in stripped
                    or 'publishPSSummarySplitBytes(' in stripped
                )
                is_state_write = re.match(r"OTcurrentSystemState\.\w+\s*=", stripped) is not None
                if not (is_publish or is_state_write):
                    continue
                # Same-line guard or guard-block opened within last few lines.
                window = '\n'.join(lines[max(0, line_no - 4):line_no + 1])
                if 'if (validForMaster' not in window:
                    issues.append(f"{case_name}: line '{stripped[:60]}' not gated on validForMaster")

        if issues:
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "FAIL",
                f"{len(issues)} ungated publish/state-write site(s) in publishPSSummaryFieldValue",
                "; ".join(issues)
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-066", "PS=1 master-topic gate", "PASS",
                f"publishPSSummaryFieldValue gates all {len(value_cases)} "
                f"value-bearing cases on validForMaster"
            ))

    # ===== ADR REFERENCE RESOLUTION (TASK-368) =====

    def check_adr_references_resolve(self):
        """Every ``ADR-NNN`` citation in ``docs/adr/*.md`` and firmware source
        must resolve to ``docs/adr/ADR-NNN-*.md``. Catches Phase 1B ghost-ADR
        class of finding (e.g. ADR-077/078/080 before TASK-355).

        Forward-citation escape hatch: if the ADR number appears on a line
        (or within a 40-char window around the match) that contains one of
        the markers ``future``, ``proposed``, or ``TBD`` (case-insensitive),
        the reference is treated as a known forward citation and not failed.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR References Resolve ==={Colors.ENDC}")

        # Inventory of existing ADR numbers.
        adr_dir = self.project_dir / "docs" / "adr"
        if not adr_dir.exists():
            self.add_result(EvaluationResult(
                "ADR", "References resolve", "WARN",
                "docs/adr/ not found"
            ))
            return

        existing_nums: set = set()
        file_name_re = re.compile(r"^ADR-(\d{3})-.*\.md$")
        for path in adr_dir.glob("ADR-*.md"):
            m = file_name_re.match(path.name)
            if m:
                existing_nums.add(m.group(1))

        # Scan targets: ADR docs + firmware source.
        scan_targets: List[Path] = list(adr_dir.glob("*.md"))
        fw = config.FIRMWARE_ROOT
        for pattern in ("*.ino", "*.cpp", "*.h"):
            scan_targets.extend(fw.glob(pattern))

        ref_re = re.compile(r"ADR-(\d{3})")
        forward_markers = re.compile(r"\b(future|proposed|TBD)\b", re.IGNORECASE)

        unresolved: List[Tuple[str, int, str]] = []
        total_refs = 0
        for target in scan_targets:
            try:
                with open(target, 'r', encoding='utf-8', errors='ignore') as f:
                    for lineno, line in enumerate(f, 1):
                        for m in ref_re.finditer(line):
                            total_refs += 1
                            num = m.group(1)
                            if num in existing_nums:
                                continue
                            # Forward-citation escape.
                            if forward_markers.search(line):
                                continue
                            rel = target.relative_to(self.project_dir) if target.is_absolute() else target
                            unresolved.append((str(rel), lineno, f"ADR-{num}"))
            except (OSError, UnicodeDecodeError):
                continue

        if not unresolved:
            self.add_result(EvaluationResult(
                "ADR", "References resolve", "PASS",
                f"All {total_refs} ADR-NNN references resolve to existing ADR files"
            ))
        else:
            # Show up to first 5 offenders in the message.
            sample = "; ".join(f"{n}:{ln}->{ref}" for n, ln, ref in unresolved[:5])
            self.add_result(EvaluationResult(
                "ADR", "References resolve", "FAIL",
                f"{len(unresolved)} unresolved ADR reference(s) out of {total_refs}",
                sample
            ))

    def check_coding_standards(self):
        """Check coding standards and best practices"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Coding Standards ==={Colors.ENDC}")
        
        issues = {
            'serial_debug': 0,
            'string_usage': 0,
            'global_vars': 0,
            'magic_numbers': 0
        }
        
        src_dir = config.FIRMWARE_ROOT
        ino_cpp_files = list(src_dir.glob("*.ino")) + list(src_dir.glob("*.cpp"))
        
        for file in ino_cpp_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
                for i, line in enumerate(lines, 1):
                    # Check for Serial.print usage (should use Debug macros)
                    if 'Serial.print' in line and 'SetupDebug' not in line and '//' not in line.split('Serial.print')[0]:
                        issues['serial_debug'] += 1
                        if self.verbose:
                            print(f"  {file.name}:{i}: Serial.print usage")
                    
                    # Check for String class usage in critical paths
                    if re.search(r'\bString\s+\w+\s*=', line) and '//' not in line.split('String')[0]:
                        issues['string_usage'] += 1

        if issues['serial_debug'] > 0:
            self.add_result(EvaluationResult(
                "Coding", "Serial Debug Output", "WARN",
                f"Found {issues['serial_debug']} Serial.print() usage (should use Debug macros)"
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "Serial Debug Output", "PASS",
                "No improper Serial.print() usage found"
            ))

        if issues['string_usage'] > 5:
            self.add_result(EvaluationResult(
                "Coding", "String Class Usage", "WARN",
                f"Found {issues['string_usage']} String class usages (may cause heap fragmentation)"
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "String Class Usage", "PASS",
                f"Limited String usage ({issues['string_usage']} instances)"
            ))

    def check_memory_usage(self):
        """Analyze memory usage patterns"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Memory Analysis ==={Colors.ENDC}")
        
        # Check for large buffers
        large_buffers: List[Tuple[str, int]] = []
        src_dir = config.FIRMWARE_ROOT
        ino_cpp_files = list(src_dir.glob("*.ino")) + list(src_dir.glob("*.cpp")) + list(src_dir.glob("*.h"))
        
        for file in ino_cpp_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                # Look for large array declarations
                matches = re.findall(r'char\s+\w+\[(\d+)\]', content)
                for size_str in matches:
                    size = int(size_str)
                    if size > 1024:
                        large_buffers.append((file.name, size))

        if large_buffers:
            total_size = sum([s for _, s in large_buffers])
            self.add_result(EvaluationResult(
                "Memory", "Large Buffers", "INFO",
                f"Found {len(large_buffers)} buffers > 1KB (total: {total_size} bytes)",
                "; ".join([f"{f}: {s}B" for f, s in large_buffers[:5]])
            ))

    # ===== BUILD SYSTEM CHECKS =====
    
    def check_build_system(self):
        """Validate build system configuration"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Build System ==={Colors.ENDC}")
        
        # Check Makefile
        makefile = self.project_dir / "Makefile"
        if makefile.exists():
            self.add_result(EvaluationResult(
                "Build", "Makefile", "PASS",
                "Found Makefile"
            ))
            
            with open(makefile, 'r') as f:
                content = f.read()
                # Check for essential targets
                targets = ['binaries', 'clean', 'upload', 'filesystem']
                for target in targets:
                    if f"{target}:" in content:
                        self.add_result(EvaluationResult(
                            "Build", f"Make target: {target}", "PASS",
                            "Target defined"
                        ))
                    else:
                        self.add_result(EvaluationResult(
                            "Build", f"Make target: {target}", "WARN",
                            "Target not found"
                        ))
        else:
            self.add_result(EvaluationResult(
                "Build", "Makefile", "FAIL",
                "Makefile not found"
            ))

        # Check build.py
        build_script = self.project_dir / "build.py"
        if build_script.exists():
            self.add_result(EvaluationResult(
                "Build", "build.py", "PASS",
                "Found build script"
            ))
        else:
            self.add_result(EvaluationResult(
                "Build", "build.py", "WARN",
                "Build script not found"
            ))

    def check_dependencies(self):
        """Check library dependencies"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Dependencies ==={Colors.ENDC}")
        
        # Parse dependencies from Makefile
        lib_matches: List[str] = []
        makefile = self.project_dir / "Makefile"
        if makefile.exists():
            with open(makefile, 'r') as f:
                content = f.read()
                # Extract library installations
                lib_matches = re.findall(r'lib install (\S+)', content)
                if lib_matches:
                    self.add_result(EvaluationResult(
                        "Dependencies", "Library Count", "INFO",
                        f"Found {len(lib_matches)} library dependencies",
                        ", ".join(lib_matches)
                    ))

        # Check for library version pinning
        if lib_matches:
            pinned = [lib for lib in lib_matches if '@' in lib]
            if len(pinned) == len(lib_matches):
                self.add_result(EvaluationResult(
                    "Dependencies", "Version Pinning", "PASS",
                    "All dependencies are version-pinned"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Dependencies", "Version Pinning", "WARN",
                    f"Only {len(pinned)}/{len(lib_matches)} dependencies are version-pinned"
                ))

    # ===== DOCUMENTATION CHECKS =====
    
    def check_documentation(self):
        """Check documentation coverage"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Documentation ==={Colors.ENDC}")
        
        # Check README
        readme = self.project_dir / "README.md"
        if readme.exists():
            with open(readme, 'r', encoding='utf-8') as f:
                content = f.read()
                size = len(content)
                
                sections = ['Installation', 'Build', 'Features', 'License']
                found_sections: List[str] = []
                for section in sections:
                    if section.lower() in content.lower():
                        found_sections.append(section)
                
                self.add_result(EvaluationResult(
                    "Documentation", "README.md", "PASS",
                    f"{size} bytes, {len(found_sections)}/{len(sections)} key sections",
                    ", ".join(found_sections)
                ))
        else:
            self.add_result(EvaluationResult(
                "Documentation", "README.md", "FAIL",
                "README.md not found"
            ))

        # Check for build documentation
        build_docs = ["BUILD.md", "FLASH_GUIDE.md"]
        for doc in build_docs:
            doc_path = self.project_dir / doc
            docs_path = self.project_dir / "docs" / doc
            if doc_path.exists():
                self.add_result(EvaluationResult(
                    "Documentation", doc, "PASS",
                    f"Found ({doc_path.stat().st_size} bytes)"
                ))
            elif docs_path.exists():
                self.add_result(EvaluationResult(
                    "Documentation", doc, "PASS",
                    f"Found (docs/{doc}, {docs_path.stat().st_size} bytes)"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Documentation", doc, "WARN",
                    f"{doc} not found"
                ))

        # Check inline documentation (comments ratio)
        total_lines = 0
        comment_lines = 0
        src_dir = config.FIRMWARE_ROOT
        ino_files = list(src_dir.glob("*.ino"))
        
        for file in ino_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    total_lines += 1
                    stripped = line.strip()
                    if stripped.startswith('//') or stripped.startswith('/*'):
                        comment_lines += 1

        if total_lines > 0:
            comment_ratio = (comment_lines / total_lines) * 100
            status = "PASS" if comment_ratio > 10 else "WARN"
            self.add_result(EvaluationResult(
                "Documentation", "Code Comments", status,
                f"{comment_ratio:.1f}% comment ratio ({comment_lines}/{total_lines} lines)"
            ))

    # ===== SECURITY CHECKS =====
    
    def check_security(self):
        """Check for common security issues"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Security Analysis ==={Colors.ENDC}")
        
        security_issues: Dict[str, List[str]] = {
            'hardcoded_creds': [],
            'unsafe_string_ops': [],
            'buffer_overflow_risk': []
        }
        
        src_dir = config.FIRMWARE_ROOT
        all_code_files = (list(src_dir.glob("*.ino")) + 
                         list(src_dir.glob("*.cpp")) + 
                         list(src_dir.glob("*.h")))
        
        for file in all_code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
                for i, line in enumerate(lines, 1):
                    # Check for potential hardcoded credentials
                    if re.search(r'(password|passwd|pwd|secret|key)\s*=\s*["\'](?!xxx|changeme|\*+)[^"\']{3,}["\']', line, re.I):
                        security_issues['hardcoded_creds'].append(f"{file.name}:{i}")
                    
                    # Check for unsafe string operations
                    if re.search(r'\b(strcpy|strcat|sprintf|gets)\s*\(', line):
                        security_issues['unsafe_string_ops'].append(f"{file.name}:{i}")

        if security_issues['hardcoded_creds']:
            self.add_result(EvaluationResult(
                "Security", "Hardcoded Credentials", "WARN",
                f"Found {len(security_issues['hardcoded_creds'])} potential hardcoded credentials",
                "; ".join(security_issues['hardcoded_creds'][:3])
            ))
        else:
            self.add_result(EvaluationResult(
                "Security", "Hardcoded Credentials", "PASS",
                "No obvious hardcoded credentials found"
            ))

        if security_issues['unsafe_string_ops']:
            self.add_result(EvaluationResult(
                "Security", "Unsafe String Ops", "WARN",
                f"Found {len(security_issues['unsafe_string_ops'])} unsafe string operations",
                "; ".join(security_issues['unsafe_string_ops'][:3])
            ))
        else:
            self.add_result(EvaluationResult(
                "Security", "Unsafe String Ops", "PASS",
                "No unsafe string operations found"
            ))

    # ===== GIT REPOSITORY CHECKS =====
    
    def check_git_repository(self):
        """Check Git repository health"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Git Repository ==={Colors.ENDC}")
        
        git_dir = self.project_dir / ".git"
        if not git_dir.exists():
            self.add_result(EvaluationResult(
                "Git", "Repository", "WARN",
                "Not a Git repository"
            ))
            return

        # Check current branch
        rc, stdout, _ = self.run_command(["git", "branch", "--show-current"])
        if rc == 0:
            branch = stdout.strip()
            self.add_result(EvaluationResult(
                "Git", "Current Branch", "INFO",
                f"On branch: {branch}"
            ))

        # Check for uncommitted changes
        rc, stdout, _ = self.run_command(["git", "status", "--porcelain"])
        if rc == 0:
            if stdout.strip():
                changed_files = len(stdout.strip().split('\n'))
                self.add_result(EvaluationResult(
                    "Git", "Working Tree", "WARN",
                    f"{changed_files} uncommitted changes"
                ))
            else:
                self.add_result(EvaluationResult(
                    "Git", "Working Tree", "PASS",
                    "Clean working tree"
                ))

        # Check .gitignore
        gitignore = self.project_dir / ".gitignore"
        if gitignore.exists():
            with open(gitignore, 'r') as f:
                rules = [line.strip() for line in f if line.strip() and not line.startswith('#')]
                self.add_result(EvaluationResult(
                    "Git", ".gitignore", "PASS",
                    f"Found with {len(rules)} rules"
                ))
        else:
            self.add_result(EvaluationResult(
                "Git", ".gitignore", "WARN",
                ".gitignore not found"
            ))

    # ===== FILE SYSTEM CHECKS =====
    
    def check_filesystem_data(self):
        """Check data directory for LittleFS"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Filesystem Data ==={Colors.ENDC}")
        
        data_dir = config.DATA_DIR
        if not data_dir.exists():
            self.add_result(EvaluationResult(
                "Filesystem", "data/ directory", "FAIL",
                "data/ directory not found"
            ))
            return

        # Count files in data directory
        files = list(data_dir.rglob("*"))
        file_count = len([f for f in files if f.is_file()])
        total_size = sum([f.stat().st_size for f in files if f.is_file()])
        
        self.add_result(EvaluationResult(
            "Filesystem", "data/ content", "INFO",
            f"{file_count} files, {total_size} bytes total"
        ))

        # Check for web interface files
        web_files = ['.html', '.css', '.js', '.json']
        web_count = len([f for f in files if f.suffix in web_files])
        
        if web_count > 0:
            self.add_result(EvaluationResult(
                "Filesystem", "Web UI files", "PASS",
                f"Found {web_count} web interface files"
            ))

    # ===== VERSION CONTROL =====
    
    def check_version_info(self):
        """Check version information"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Version Information ==={Colors.ENDC}")
        
        version_file = config.FIRMWARE_ROOT / "version.h"
        if version_file.exists():
            with open(version_file, 'r') as f:
                content = f.read()
                
                # Extract version info
                semver_match = re.search(r'#define\s+_SEMVER_FULL\s+"([^"]+)"', content)
                build_match = re.search(r'#define\s+_BUILD\s+(\d+)', content)
                
                if semver_match:
                    version = semver_match.group(1)
                    build = build_match.group(1) if build_match else "unknown"
                    self.add_result(EvaluationResult(
                        "Version", "Version Info", "INFO",
                        f"Version: {version}, Build: {build}"
                    ))
                else:
                    self.add_result(EvaluationResult(
                        "Version", "Version Info", "WARN",
                        "Could not parse version information"
                    ))
        else:
            self.add_result(EvaluationResult(
                "Version", "version.h", "WARN",
                "version.h not found"
            ))

    # ===== MAIN EVALUATION =====
    
    def evaluate_all(self, quick: bool = False):
        """Run all evaluations"""
        print(f"\n{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}OTGW-firmware Workspace Evaluation{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}")
        print(f"{Colors.OKCYAN}Project: {self.project_dir}{Colors.ENDC}")
        print(f"{Colors.OKCYAN}Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}{Colors.ENDC}\n")
        
        # Essential checks (always run)
        self.check_code_structure()
        self.check_build_system()
        self.check_version_info()
        self.check_time_boundary_single_caller()      # ADR-064 CI gate (TASK-350)
        self.check_discovery_counter_instrumented()   # ADR-062 CI gate (TASK-364)
        self.check_publishedtopic_counter_reset()     # ADR-062 CI gate (TASK-364)
        self.check_ha_sensor_index_consistency()      # HA discovery gate (TASK-392)
        self.check_json_buffer_arithmetic()           # TASK-352/368
        self.check_status_burst_cooldown_bound()      # TASK-353/368
        self.check_status_publishers_wrap_burst()     # TASK-347/354/368
        self.check_ps_summary_master_topic_gate()     # ADR-066 amendment / TASK-483
        self.check_adr_references_resolve()           # TASK-355/368

        if not quick:
            # Detailed checks
            self.check_coding_standards()
            self.check_memory_usage()
            self.check_dependencies()
            self.check_documentation()
            self.check_security()
            self.check_git_repository()
            self.check_filesystem_data()

    def print_summary(self):
        """Print evaluation summary"""
        print(f"\n{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}Evaluation Summary{Colors.ENDC}")
        print(f"{Colors.BOLD}{Colors.HEADER}{'='*60}{Colors.ENDC}\n")
        
        total = len(self.results)
        pass_count = self.stats.get("PASS", 0)
        warn_count = self.stats.get("WARN", 0)
        fail_count = self.stats.get("FAIL", 0)
        info_count = self.stats.get("INFO", 0)
        
        print(f"Total Checks: {total}")
        print(f"{Colors.OKGREEN}✓ Passed: {pass_count}{Colors.ENDC}")
        print(f"{Colors.WARNING}⚠ Warnings: {warn_count}{Colors.ENDC}")
        print(f"{Colors.FAIL}✗ Failed: {fail_count}{Colors.ENDC}")
        print(f"{Colors.OKCYAN}ℹ Info: {info_count}{Colors.ENDC}")
        
        # Calculate health score
        if total > 0:
            health_score = ((pass_count + info_count) / total) * 100
            health_color = Colors.OKGREEN if health_score >= 80 else Colors.WARNING if health_score >= 60 else Colors.FAIL
            print(f"\n{Colors.BOLD}Health Score: {health_color}{health_score:.1f}%{Colors.ENDC}")
        
        # Exit code based on failures
        if fail_count > 0:
            return 1
        elif warn_count > 5:
            return 2
        return 0

    def generate_report(self, output_file: Path):
        """Generate detailed JSON report"""
        report: Dict[str, Any] = {
            "timestamp": datetime.now().isoformat(),
            "project_dir": str(self.project_dir),
            "summary": {
                "total": len(self.results),
                "passed": self.stats.get("PASS", 0),
                "warnings": self.stats.get("WARN", 0),
                "failed": self.stats.get("FAIL", 0),
                "info": self.stats.get("INFO", 0)
            },
            "results": []
        }
        
        for result in self.results:
            report["results"].append({
                "category": result.category,
                "name": result.name,
                "status": result.status,
                "message": result.message,
                "details": result.details,
                "timestamp": result.timestamp.isoformat()
            })
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"\n{Colors.OKGREEN}Report saved to: {output_file}{Colors.ENDC}")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="OTGW-firmware Workspace Evaluation Framework",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Quick evaluation (essential checks only)"
    )
    parser.add_argument(
        "--report",
        action="store_true",
        help="Generate detailed JSON report"
    )
    parser.add_argument(
        "--output",
        type=str,
        default="evaluation-report.json",
        help="Output file for report (default: evaluation-report.json)"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output (show all checks)"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable colored output"
    )
    
    args = parser.parse_args()
    
    if args.no_color:
        Colors.disable()
    
    # Get project directory
    project_dir = Path(__file__).parent.resolve()
    
    # Run evaluation
    evaluator = WorkspaceEvaluator(project_dir, verbose=args.verbose)
    evaluator.evaluate_all(quick=args.quick)
    
    # Print summary
    exit_code = evaluator.print_summary()
    
    # Generate report if requested
    if args.report:
        output_path = project_dir / args.output
        evaluator.generate_report(output_path)
    
    return exit_code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(f"\n{Colors.WARNING}Evaluation interrupted by user{Colors.ENDC}")
        sys.exit(130)
    except Exception as e:
        print(f"\n{Colors.FAIL}Error: {e}{Colors.ENDC}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
