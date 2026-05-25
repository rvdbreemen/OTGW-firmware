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
- Design-system CSS class drift analysis

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
from typing import Dict, List, Tuple, Any, Set, Optional

# Ensure Unicode output works on Windows consoles (cp1252 etc.)
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') != 'utf8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
if sys.stderr.encoding and sys.stderr.encoding.lower().replace('-', '') != 'utf8':
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

import config

# ===== Module-level pure helpers (testable; used by WorkspaceEvaluator) =====

# Hot-path files per ADR-004: no String class allowed.
# Matched as filename-prefix tuples (e.g. 'SAT' matches SATble.ino, SATcontrol.ino, ...).
HOT_PATH_PREFIXES: Tuple[str, ...] = (
    'SAT', 'MQTTstuff', 'restAPI', 'OTGW-Core', 'OTDirect',
)

# String-class regex: declarations of the form "String name;", "String name = ...",
# "String name(arg)". Reference forms like "String&" and "const String&" are skipped
# because they do not allocate.
_STRING_DECL_RE = re.compile(r'\bString\s+\w+\s*[;=(]')

# Binary-safe compare: strncmp_P / strstr_P on binary buffers causes Exception(2) on
# ESP8266. Reported as INFO so each call site can be hand-verified to operate on
# null-terminated text. See MEMORY.md "Exception (2) foot-gun" note.
_BINARY_COMPARE_RE = re.compile(r'\b(?:strncmp_P|strstr_P)\s*\(')

# Design-system class drift helpers. These are intentionally regex-based: the
# firmware Web UI has no Node/AST toolchain, and ADR-091 only needs to catch
# literal class names emitted by HTML and JavaScript.
_DS_CLASS_TOKEN = r"[A-Za-z_][A-Za-z0-9_-]*"
_DS_CLASS_ATTR_RE = re.compile(r"""class\s*=\s*["']([^"']+)["']""", re.IGNORECASE)
_DS_CLASSLIST_API_RE = re.compile(
    r"""classList\s*\.\s*(?:add|remove|toggle|replace|contains)\s*\(([^)]*)\)""",
    re.IGNORECASE,
)
_DS_CLASSNAME_ASSIGN_RE = re.compile(r"""\.\s*className\s*=\s*["']([^"']+)["']""")
_DS_QUOTED_STRING_RE = re.compile(r"""["']([^"']+)["']""")
_DS_CSS_CLASS_RE = re.compile(rf"\.({_DS_CLASS_TOKEN})")
_DS_CSS_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
_DS_JS_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
_DS_JS_LINE_COMMENT_RE = re.compile(r"//[^\n]*")

# Classes below are intentionally referenced without a CSS selector. Keep this
# grouped by reason so future additions are reviewable instead of becoming a
# silent dumping ground.
DESIGN_SYSTEM_CLASS_ALLOWLIST_BY_REASON: Dict[str, Tuple[str, ...]] = {
    "behavior/state hooks": (
        "FSexplorer",
        "adminSettings",
        "basicSettings",
        "btnSaveSettings",
        "home",
        "ot-command-capable",
        "otdirect-only",
        "page-nav-shell",
        "pic-only",
        "tabButton",
        "tabDeviceInfo",
        "tabPICflash",
        "tabSAT",
        "tabWebhook",
    ),
    "design.html reference-only demo helpers": (
        "dark-pane",
        "demo",
        "demo-label",
        "design-grid",
        "design-section",
        "design-shell",
        "design-swatch",
        "design-toc",
        "ds-tag",
        "label",
        "lede",
        "light-pane",
        "specbox",
        "swatch-meta",
        "swatch-tile",
        "theme-pair",
        "type-row",
    ),
    "browser/native utility classes with deliberate no-op styling": (
        "button",
    ),
}

DESIGN_SYSTEM_CLASS_ALLOWLIST: Set[str] = {
    cls
    for classes in DESIGN_SYSTEM_CLASS_ALLOWLIST_BY_REASON.values()
    for cls in classes
}


def strip_css_comments(text: str) -> str:
    """Strip CSS block comments before selector scanning."""
    return _DS_CSS_COMMENT_RE.sub("", text)


def strip_js_comments(text: str) -> str:
    """Strip JavaScript block and line comments before class extraction."""
    no_block = _DS_JS_BLOCK_COMMENT_RE.sub("", text)
    return _DS_JS_LINE_COMMENT_RE.sub("", no_block)


def _looks_like_template_placeholder(token: str) -> bool:
    """Return True for dynamic template fragments that are not literal classes."""
    return token.startswith("$") or token.startswith("{") or token.endswith("}") or "{" in token


def extract_classes_from_html(content: str) -> List[Tuple[str, int]]:
    """Return (class_name, line_number) tuples from literal HTML class attributes."""
    hits: List[Tuple[str, int]] = []
    for i, line in enumerate(content.split("\n"), 1):
        for m in _DS_CLASS_ATTR_RE.finditer(line):
            for cls in m.group(1).split():
                if cls:
                    hits.append((cls, i))
    return hits


def extract_classes_from_js(content: str) -> List[Tuple[str, int]]:
    """Return (class_name, line_number) tuples from literal JavaScript class refs."""
    cleaned = strip_js_comments(content)
    hits: List[Tuple[str, int]] = []
    for i, line in enumerate(cleaned.split("\n"), 1):
        # class="..." inside strings and template literals.
        for m in _DS_CLASS_ATTR_RE.finditer(line):
            for cls in m.group(1).split():
                if cls and not _looks_like_template_placeholder(cls):
                    hits.append((cls, i))
        # classList.{add,remove,toggle,replace,contains}('x' [, ...])
        for m in _DS_CLASSLIST_API_RE.finditer(line):
            for arg in _DS_QUOTED_STRING_RE.findall(m.group(1)):
                for cls in arg.split():
                    if cls and not _looks_like_template_placeholder(cls):
                        hits.append((cls, i))
        # el.className = 'x y' literal assignments only.
        for m in _DS_CLASSNAME_ASSIGN_RE.finditer(line):
            for cls in m.group(1).split():
                if cls and not _looks_like_template_placeholder(cls):
                    hits.append((cls, i))
    return hits


def extract_class_definitions_from_css(content: str) -> Set[str]:
    """Return class names defined as CSS selectors, including compound selectors."""
    cleaned = strip_css_comments(content)
    return set(_DS_CSS_CLASS_RE.findall(cleaned))


def compute_drift(
    used: Dict[str, List[str]],
    defined: Set[str],
    allowlist: Optional[Set[str]] = None,
) -> Tuple[Dict[str, List[str]], Set[str], Dict[str, List[str]]]:
    """Return (missing, dead, allowlisted_missing) for design-system classes."""
    allowed = allowlist or set()
    used_set = set(used.keys())
    raw_missing = {cls: locs for cls, locs in used.items() if cls not in defined}
    allowlisted_missing = {cls: locs for cls, locs in raw_missing.items() if cls in allowed}
    missing = {cls: locs for cls, locs in raw_missing.items() if cls not in allowed}
    dead = defined - used_set
    return missing, dead, allowlisted_missing


def scan_design_system_workspace(data_dir: Path) -> Tuple[Dict[str, List[str]], Set[str], Dict[str, int]]:
    """Scan data/*.html, data/*.js, and data/*.css for class usage/definitions."""
    used: Dict[str, List[str]] = defaultdict(list)
    used_counts: Dict[str, int] = defaultdict(int)
    defined: Set[str] = set()

    for f in sorted(data_dir.glob("*.html")):
        text = f.read_text(encoding="utf-8", errors="replace")
        for cls, line in extract_classes_from_html(text):
            used[cls].append(f"{f.name}:{line}")
            used_counts[cls] += 1

    for f in sorted(data_dir.glob("*.js")):
        text = f.read_text(encoding="utf-8", errors="replace")
        for cls, line in extract_classes_from_js(text):
            used[cls].append(f"{f.name}:{line}")
            used_counts[cls] += 1

    for f in sorted(data_dir.glob("*.css")):
        text = f.read_text(encoding="utf-8", errors="replace")
        defined |= extract_class_definitions_from_css(text)

    return used, defined, used_counts


def is_hot_path_file(filename: str) -> bool:
    """Return True when the file is a known ADR-004 hot-path source file."""
    return any(filename.startswith(p) for p in HOT_PATH_PREFIXES)


def collect_firmware_source_files(
    src_dir: Path,
    *,
    include_ino: bool = True,
    include_cpp: bool = True,
    include_h: bool = True,
) -> List[Path]:
    """Collect firmware source files for static-analysis gates.

    Excludes auto-generated PlatformIO Arduino preprocessor output
    (``*.ino.cpp``), which is a single concatenation of every ``.ino`` and
    would otherwise inflate hit counts and make them depend on local build
    state. See TASK-482 / ADR notes.
    """
    files: List[Path] = []
    if include_ino:
        files.extend(src_dir.glob("*.ino"))
    if include_cpp:
        files.extend(f for f in src_dir.glob("*.cpp") if not f.name.endswith(".ino.cpp"))
    if include_h:
        files.extend(src_dir.glob("*.h"))
    return files


def _strip_line_comments(line: str) -> str:
    """Return the portion of the line before a // line-comment (if any)."""
    return line.split('//', 1)[0]


def _progmem_call_violates(line: str, open_paren_idx: int, arg_index) -> bool:
    """Decide whether a call (snprintf/strcmp/strcasecmp/DebugTln/DebugTf) on `line`
    has an unwrapped string literal as a DIRECT argument.

    `line[open_paren_idx]` is the '(' that opens the call's argument list. We walk
    forward, tracking paren depth. Only literals that appear at depth 1 (the call's
    own arg list, not a nested sub-call) and are not immediately preceded by `F(`
    or `PSTR(` count.

    `arg_index`:
      - None: any unwrapped literal at depth 1 is a violation. Used for DebugTln,
        DebugTf, strcmp, strcasecmp.
      - int N: only the Nth positional arg (0-based, separated by depth-1 commas)
        being an unwrapped literal counts. Used for snprintf where the format
        string is positional arg 1 (after the buffer at index 0).

    Limitation: scans a single line. Multi-line calls would need a multi-line
    join; for the firmware codebase, format strings and compare literals are
    on the same line as the call, so this is sufficient (matches the prior
    regex-only scan's reach).
    """
    if open_paren_idx < 0 or open_paren_idx >= len(line) or line[open_paren_idx] != '(':
        return False
    depth = 0
    pos = open_paren_idx
    current_arg = 0  # index of the positional arg we're currently inside (depth 1)
    n = len(line)
    while pos < n:
        ch = line[pos]
        if ch == '(':
            depth += 1
            pos += 1
            continue
        if ch == ')':
            depth -= 1
            if depth == 0:
                return False  # end of the call's argument list
            pos += 1
            continue
        if ch == ',' and depth == 1:
            current_arg += 1
            pos += 1
            continue
        if ch == '"' and depth == 1:
            # Found a string literal at depth 1. Is it wrapped in F(...) or PSTR(...)?
            # The wrapper would manifest as the immediately-preceding token being
            # `F(` or `PSTR(`, i.e. depth would have just incremented from 1 to 2
            # via that wrapper's '('. Since we're at depth 1 here, the literal is
            # NOT inside such a wrapper.
            if arg_index is None or current_arg == arg_index:
                # Skip the literal contents to keep parser sane (handles \" escapes).
                pos += 1
                while pos < n and line[pos] != '"':
                    if line[pos] == '\\' and pos + 1 < n:
                        pos += 2
                        continue
                    pos += 1
                return True
            # literal at depth 1 but not the arg index we care about -> ignore,
            # skip past it.
            pos += 1
            while pos < n and line[pos] != '"':
                if line[pos] == '\\' and pos + 1 < n:
                    pos += 2
                    continue
                pos += 1
            pos += 1
            continue
        if ch == '"':
            # Literal inside a sub-call (depth > 1) — not this call's concern.
            pos += 1
            while pos < n and line[pos] != '"':
                if line[pos] == '\\' and pos + 1 < n:
                    pos += 2
                    continue
                pos += 1
            pos += 1
            continue
        if ch == "'":
            # Char literal — skip past closing quote.
            pos += 1
            while pos < n and line[pos] != "'":
                if line[pos] == '\\' and pos + 1 < n:
                    pos += 2
                    continue
                pos += 1
            pos += 1
            continue
        pos += 1
    # Reached end of line without closing paren — be conservative, no violation.
    return False


def scan_string_usages_detailed(content: str, filename: str) -> List[str]:
    """Return "filename:lineno" entries for String-class declarations in content."""
    hits: List[str] = []
    for i, line in enumerate(content.split('\n'), 1):
        before_comment = _strip_line_comments(line)
        stripped = before_comment.lstrip()
        if stripped.startswith('/*') or stripped.startswith('*'):
            continue
        if _STRING_DECL_RE.search(before_comment):
            hits.append(f"{filename}:{i}")
    return hits


def scan_binary_compare_calls(content: str, filename: str) -> List[str]:
    """Return "filename:lineno" entries for strncmp_P / strstr_P call sites."""
    hits: List[str] = []
    for i, line in enumerate(content.split('\n'), 1):
        before_comment = _strip_line_comments(line)
        stripped = before_comment.lstrip()
        if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('#define'):
            continue
        if _BINARY_COMPARE_RE.search(before_comment):
            hits.append(f"{filename}:{i}")
    return hits


# ADR-080 gate helpers. An Accepted ADR either references an automated gate
# (evaluate.py / tests/) or carries a classification label marking it as non-
# pattern-level. Otherwise it is a binding rule without enforcement -- the
# exact drift mode ADR-080 was written to prevent.
_ADR_CLASSIFICATION_LABELS: Tuple[str, ...] = (
    'guideline-level', 'structural', 'historical', 'policy', 'meta-level',
    'meta-rule', 'system-level',
)
_ADR_GATE_MARKERS: Tuple[str, ...] = (
    'evaluate.py', 'tests/test_', 'check_progmem', 'check_binary_safe',
    'check_coding_standards', 'check_no_arduinojson', 'check_adr_gates',
    'check_backlog_hygiene', 'check_stack_safety',
)


def adr_has_gate_or_label(text: str) -> bool:
    """True when the ADR text references a gate or carries a classification label."""
    lowered = text.lower()
    if any(label in lowered for label in _ADR_CLASSIFICATION_LABELS):
        return True
    if any(marker in text for marker in _ADR_GATE_MARKERS):
        return True
    return False


def adr_is_accepted(text: str) -> bool:
    """True when the ADR's Status section contains 'Accepted' (case-insensitive)."""
    # Only look at the first ~40 lines; Status sits near the top.
    head = '\n'.join(text.splitlines()[:40])
    return 'accepted' in head.lower()


# Backlog hygiene helpers (enforces the AC Deviation Protocol at task level).
_TASK_DEVIATION_MARKERS: Tuple[str, ...] = (
    'deviation', 'deferred', 'defer ', 'follow-up', 'followup',
    'hardware test', 'manual test', "won't fix", 'wont-fix', 'wont fix',
    'out of scope', 'out-of-scope', 'scope-out',
    'superseded', 'retrofit', 'retrofitted',
)


def task_has_deviation_note(text: str) -> bool:
    """True when the task notes/final-summary reference a recognized deviation marker."""
    # Only scan sections that live after the AC block (Implementation Notes / Final Summary).
    # Using the whole body is fine for our purposes; a marker in the description
    # usually implies the author was aware of the shape of the deviation too.
    lowered = text.lower()
    return any(marker in lowered for marker in _TASK_DEVIATION_MARKERS)


_TASK_AC_UNCHECKED_RE = re.compile(r'^\s*-\s*\[\s\]\s+#?\d+', re.MULTILINE)
_TASK_STATUS_RE = re.compile(r'^status:\s*(\S+)', re.MULTILINE)


def task_status(text: str) -> str:
    m = _TASK_STATUS_RE.search(text)
    return m.group(1).strip().strip("'\"") if m else ''


def task_unchecked_ac_count(text: str) -> int:
    """Count unchecked ACs (- [ ] #N ...) in task body."""
    return len(_TASK_AC_UNCHECKED_RE.findall(text))


# OTGWstream / OTDirect 25238 bridge regression helpers.
# These are intentionally text-based: the failure mode here is wiring drift,
# not semantic behavior that a compiler can see.
def otdirect_25238_bridge_regressions(
    handle_debug_text: str,
    firmware_text: str,
    otdirect_text: str,
) -> Dict[str, bool]:
    """Return the key ownership/fanout checks for the ESP32 25238 bridge.

    The result stays small and explicit so unit tests can pin the intended split:
    - `handleOTGWstream()` remains serviced from core/network code.
    - `handleOTDirectBridgeStream()` owns the no-PIC inbound TCP bridge.
    - `bridgeFrameToParser()` / `synthesizeResponse()` still fan out to TCP.
    - short PIC-style rejection statuses also fan out before `processOT()`.
    - synthesized PR= responses also fan out before `processOT()`.
    """
    direct_short_status_re = re.compile(r'processOT\("(?:NG|BV|OR|SE|NS|NF)",\s*2\)')
    direct_pr_response_re = re.compile(r'processOT\s*\(\s*prBuf\s*,\s*strlen\s*\(\s*prBuf\s*\)\s*\)')

    return {
        "service_loop": (
            "handleOTGWstream();" in firmware_text
            and "OTGWstream.loop();" in handle_debug_text
        ),
        "inbound_bridge": (
            "void handleOTDirectBridgeStream()" in otdirect_text
            and "OTGWstream.available()" in otdirect_text
            and "OTGWstream.read()" in otdirect_text
            and "sendPICSerial(" in otdirect_text
        ),
        "outbound_fanout": (
            "otDirectBridgeWriteLine(buf, 9);" in otdirect_text
            and "otDirectBridgeWriteLine(buf, respLen);" in otdirect_text
            and "OTGWstream.write(" in otdirect_text
        ),
        "short_error_fanout": (
            "static void otDirectBridgeProcessStatus(const char* status)" in otdirect_text
            and "otDirectBridgeWriteLine(status, 2);" in otdirect_text
            and "processOT(status, 2);" in otdirect_text
            and not direct_short_status_re.search(otdirect_text)
        ),
        "pr_response_fanout": (
            "static void otDirectBridgeProcessPRResponse(const char* prLine)" in otdirect_text
            and "otDirectBridgeWriteLine(prLine, prLen);" in otdirect_text
            and "processOT(prLine, prLen);" in otdirect_text
            and not direct_pr_response_re.search(otdirect_text)
        ),
        "ownership_split": (
            "handleOTDirectBridgeStream();" in firmware_text
            and "void handleOTDirectBridgeStream()" in otdirect_text
            and "handlePICSerial()" not in otdirect_text
        ),
    }


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

        # Check for proper header guards in .h files. Accept both the classic
        # #ifndef/#define idiom and the modern #pragma once (supported by every
        # compiler this project targets).
        h_files = list(src_dir.glob("*.h"))
        for h_file in h_files:
            with open(h_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                if '#pragma once' in content or ('#ifndef' in content and '#define' in content):
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
        """ADR-086 binding rule (originally ADR-064): each consume-on-read
        time-boundary helper (minuteChanged / hourChanged / dayChanged /
        yearChanged) must have exactly ONE call site firmware-wide. A second
        caller silently steals the event.

        Enforcement per ADR-080 meta-rule. Fails on >1 call site.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR-086 Time-Boundary Single-Caller ==={Colors.ENDC}")

        helpers = ["minuteChanged", "hourChanged", "dayChanged", "yearChanged"]
        src_dir = config.FIRMWARE_ROOT
        # Scan all C/C++ source files under firmware root (not library subtree).
        source_files = collect_firmware_source_files(src_dir)

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
                    "ADR-086", f"{helper}() single caller", "PASS",
                    f"Exactly 1 call site at {loc}"
                ))
            elif len(call_sites) == 0:
                self.add_result(EvaluationResult(
                    "ADR-086", f"{helper}() single caller", "WARN",
                    "No call sites found (dead code or helper removed)"
                ))
            else:
                detail = "; ".join(f"{n}:{ln}" for n, ln, _ in call_sites)
                self.add_result(EvaluationResult(
                    "ADR-086", f"{helper}() single caller", "FAIL",
                    f"Found {len(call_sites)} call sites — ADR-086 requires exactly 1",
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
        source_files = collect_firmware_source_files(src_dir)

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

        # The arrays live in mqtt_configuratie.cpp on dev (1.4.x line) and in
        # MQTTHaDiscovery.cpp on feature-dev-2.0.0 (renamed per ADR-077). Try
        # both so the gate runs on either branch without divergence.
        cpp = config.FIRMWARE_ROOT / "mqtt_configuratie.cpp"
        if not cpp.exists():
            cpp = config.FIRMWARE_ROOT / "MQTTHaDiscovery.cpp"
        if not cpp.exists():
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "WARN",
                "Neither mqtt_configuratie.cpp nor MQTTHaDiscovery.cpp found — cannot verify"
            ))
            return

        try:
            source = cpp.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "HA-DISC", "Sensor index consistency", "FAIL",
                f"Could not read {cpp.name}: {e}"
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
            # The function was refactored to per-stat publishStatU32() calls;
            # with no fixed char[N] buffer there is no overflow surface, so the
            # arithmetic concern is moot rather than unverified.
            self.add_result(EvaluationResult(
                "Buffer", "sendMQTTheapdiag arithmetic", "PASS",
                "No fixed char[N] buffer in body — per-stat publish, "
                "buffer-arithmetic check not applicable"
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
                "ADR-088", "Status publishers wrap burst", "WARN",
                "OTGW-Core.ino not found"
            ))
            return

        try:
            source = core_ino.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-088", "Status publishers wrap burst", "FAIL",
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
                "ADR-088", "Status publishers wrap burst", "WARN",
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
                "ADR-088", "Status publishers wrap burst", "FAIL",
                f"{len(missing)} of {len(matches)} status publishers miss burst wrap",
                detail
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-088", "Status publishers wrap burst", "PASS",
                f"All {len(matches)} status publishers wrap begin/endStatusBurst()",
                detail
            ))

    # ===== ADR-088 SUB-RULE 3 GATE (TASK-426) =====

    def check_drip_consults_deferred(self):
        """TASK-426 / ADR-088 sub-rule 3: ``loopMQTTDiscovery()`` must consult
        ``isDripDeferred()`` before issuing a discovery drip publish so that
        Status-burst windowing can quiesce the drip during a fanout and during
        the post-burst cooldown. Without this consult, sub-rules 1 and 2 lose
        their effect because the drip would still fire on top of bursts.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Drip Consults isDripDeferred ==={Colors.ENDC}")

        mqtt_ino = config.FIRMWARE_ROOT / "MQTTstuff.ino"
        if not mqtt_ino.exists():
            self.add_result(EvaluationResult(
                "ADR-088", "Drip consults isDripDeferred", "WARN",
                "MQTTstuff.ino not found"
            ))
            return

        try:
            source = mqtt_ino.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-088", "Drip consults isDripDeferred", "FAIL",
                f"Could not read MQTTstuff.ino: {e}"
            ))
            return

        # Match the definition only (signature followed by '{', possibly across
        # lines). Loosely matching only the signature would also hit the forward
        # declaration in MQTTstuff.h-style prologue ("void loopMQTTDiscovery();")
        # and _extract_function_body would then walk past the ';' into an
        # unrelated function body.
        sig_re = re.compile(
            r"^\s*(?:static\s+)?void\s+(loopMQTTDiscovery)\s*\(\s*\)\s*\{",
            re.MULTILINE
        )
        m = sig_re.search(source)
        if not m:
            self.add_result(EvaluationResult(
                "ADR-088", "Drip consults isDripDeferred", "WARN",
                "loopMQTTDiscovery() definition not found in MQTTstuff.ino"
            ))
            return

        body, _ = self._extract_function_body(source, m.start())
        if not body:
            self.add_result(EvaluationResult(
                "ADR-088", "Drip consults isDripDeferred", "FAIL",
                "loopMQTTDiscovery() body not parseable"
            ))
            return

        if re.search(r"\bisDripDeferred\s*\(", body):
            self.add_result(EvaluationResult(
                "ADR-088", "Drip consults isDripDeferred", "PASS",
                "loopMQTTDiscovery() references isDripDeferred()"
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-088", "Drip consults isDripDeferred", "FAIL",
                "loopMQTTDiscovery() does not reference isDripDeferred()",
                "Add an isDripDeferred() check before the drip publish; see ADR-088 sub-rule 3"
            ))

    # ===== ADR-089 GATES (TASK-428) =====

    def check_heap_tier_thresholds_ordered(self):
        """ADR-089 sub-rule 1: HEAP_CRITICAL_THRESHOLD < HEAP_WARNING_THRESHOLD <
        HEAP_LOW_THRESHOLD, with HEAP_CRITICAL_THRESHOLD >= 1024 (sanity floor:
        ESP8266 WiFi stack baseline ~1-2 KB). Re-baselined under TASK-344.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Heap Tier Thresholds Ordered ==={Colors.ENDC}")

        helper = config.FIRMWARE_ROOT / "helperStuff.ino"
        if not helper.exists():
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier thresholds ordered", "WARN",
                "helperStuff.ino not found"
            ))
            return

        try:
            source = helper.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier thresholds ordered", "FAIL",
                f"Could not read helperStuff.ino: {e}"
            ))
            return

        decl_re = re.compile(
            r"#define\s+(HEAP_CRITICAL_THRESHOLD|HEAP_WARNING_THRESHOLD|HEAP_LOW_THRESHOLD)\s+(\d+)"
        )
        found = {m.group(1): int(m.group(2)) for m in decl_re.finditer(source)}
        missing = [k for k in ("HEAP_CRITICAL_THRESHOLD", "HEAP_WARNING_THRESHOLD", "HEAP_LOW_THRESHOLD") if k not in found]
        if missing:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier thresholds ordered", "FAIL",
                f"Missing #define for: {', '.join(missing)}"
            ))
            return

        crit = found["HEAP_CRITICAL_THRESHOLD"]
        warn = found["HEAP_WARNING_THRESHOLD"]
        low  = found["HEAP_LOW_THRESHOLD"]

        if crit < 1024:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier thresholds ordered", "FAIL",
                f"HEAP_CRITICAL_THRESHOLD = {crit} below sanity floor (1024); ESP8266 WiFi stack needs ~1-2 KB"
            ))
            return

        if not (crit < warn < low):
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier thresholds ordered", "FAIL",
                f"Tier ordering violated: CRITICAL={crit}, WARNING={warn}, LOW={low}"
            ))
            return

        self.add_result(EvaluationResult(
            "ADR-089", "Heap tier thresholds ordered", "PASS",
            f"CRITICAL={crit} < WARNING={warn} < LOW={low}, all >= 1024"
        ))

    def check_heap_fragmentation_promotion(self):
        """ADR-089 sub-rule 2: getHeapHealth() must reference HEAP_FRAG_PROMOTE_MAXBLOCK
        and call platformMaxFreeBlock() so a fragmented heap promotes LOW to WARNING
        before the next allocation fails silently (umm_malloc has no compaction).
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Heap Fragmentation Promotion ==={Colors.ENDC}")

        helper = config.FIRMWARE_ROOT / "helperStuff.ino"
        if not helper.exists():
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "WARN",
                "helperStuff.ino not found"
            ))
            return

        try:
            source = helper.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "FAIL",
                f"Could not read helperStuff.ino: {e}"
            ))
            return

        if not re.search(r"\bHEAP_FRAG_PROMOTE_MAXBLOCK\b", source):
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "FAIL",
                "HEAP_FRAG_PROMOTE_MAXBLOCK not defined in helperStuff.ino"
            ))
            return

        sig_re = re.compile(
            r"^\s*HeapHealthLevel\s+(getHeapHealth)\s*\(\s*\)\s*\{",
            re.MULTILINE
        )
        m = sig_re.search(source)
        if not m:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "WARN",
                "getHeapHealth() definition not found in helperStuff.ino"
            ))
            return

        body, _ = self._extract_function_body(source, m.start())
        if not body:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "FAIL",
                "getHeapHealth() body not parseable"
            ))
            return

        has_constant = bool(re.search(r"\bHEAP_FRAG_PROMOTE_MAXBLOCK\b", body))
        has_maxblock = bool(re.search(r"\bplatformMaxFreeBlock\s*\(", body))

        if has_constant and has_maxblock:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "PASS",
                "getHeapHealth() consults platformMaxFreeBlock() with HEAP_FRAG_PROMOTE_MAXBLOCK"
            ))
        else:
            parts = []
            if not has_constant: parts.append("missing HEAP_FRAG_PROMOTE_MAXBLOCK reference")
            if not has_maxblock: parts.append("missing platformMaxFreeBlock() call")
            self.add_result(EvaluationResult(
                "ADR-089", "Heap fragmentation promotion", "FAIL",
                f"getHeapHealth() incomplete: {', '.join(parts)}",
                "See ADR-089 sub-rule 2: promote LOW to WARNING when maxBlock < HEAP_FRAG_PROMOTE_MAXBLOCK"
            ))

    def check_heap_tier_entry_counters(self):
        """ADR-089 sub-rule 3: getHeapHealth() must increment iEnteredLowCount,
        iEnteredWarningCount, and iEnteredCriticalCount on transitions into stricter
        tiers (TASK-346). The counters publish hourly via sendMQTTheapdiag() to
        retained otgw-firmware/stats/enter_* topics for field telemetry.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Heap Tier-Entry Counters ==={Colors.ENDC}")

        helper = config.FIRMWARE_ROOT / "helperStuff.ino"
        if not helper.exists():
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier-entry counters", "WARN",
                "helperStuff.ino not found"
            ))
            return

        try:
            source = helper.read_text(encoding='utf-8', errors='ignore')
        except OSError as e:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier-entry counters", "FAIL",
                f"Could not read helperStuff.ino: {e}"
            ))
            return

        sig_re = re.compile(
            r"^\s*HeapHealthLevel\s+(getHeapHealth)\s*\(\s*\)\s*\{",
            re.MULTILINE
        )
        m = sig_re.search(source)
        if not m:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier-entry counters", "WARN",
                "getHeapHealth() definition not found in helperStuff.ino"
            ))
            return

        body, _ = self._extract_function_body(source, m.start())
        if not body:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier-entry counters", "FAIL",
                "getHeapHealth() body not parseable"
            ))
            return

        expected = ("iEnteredLowCount", "iEnteredWarningCount", "iEnteredCriticalCount")
        missing = [c for c in expected if not re.search(rf"\b{c}\s*\+\+", body)]

        if not missing:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier-entry counters", "PASS",
                "getHeapHealth() increments all three tier-entry counters"
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-089", "Heap tier-entry counters", "FAIL",
                f"getHeapHealth() missing increments for: {', '.join(missing)}",
                "See ADR-089 sub-rule 3: TASK-346 counters track tier transitions for field telemetry"
            ))

    # ===== DESIGN SYSTEM CHECKS =====

    def check_design_system_drift(self):
        """ADR-091 binding rule: literal classes emitted by HTML/JS must either
        have a CSS selector in data/*.css or be listed in the documented
        allowlist above. Grace release complete (TASK-480): actionable drift is
        now FAIL so CI blocks new design-system class drift.
        """
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Design-System Class Drift ==={Colors.ENDC}")

        data_dir = config.DATA_DIR
        if not data_dir.exists():
            self.add_result(EvaluationResult(
                "Design System", "Class drift", "WARN",
                f"data directory not found: {data_dir}"
            ))
            return

        try:
            used, defined, used_counts = scan_design_system_workspace(data_dir)
        except OSError as e:
            self.add_result(EvaluationResult(
                "Design System", "Class drift", "FAIL",
                f"Could not scan Web UI assets: {e}"
            ))
            return

        missing, _, allowlisted_missing = compute_drift(
            used,
            defined,
            DESIGN_SYSTEM_CLASS_ALLOWLIST,
        )

        if missing:
            details = []
            for cls in sorted(missing.keys(), key=lambda c: (-used_counts[c], c)):
                locs = missing[cls]
                details.append(f"{cls} first {locs[0]} ({used_counts[cls]} refs)")
            self.add_result(EvaluationResult(
                "Design System",
                "Class drift",
                "FAIL",
                f"{len(missing)} referenced CSS class(es) have no selector",
                "; ".join(details[:20])
            ))
            return

        self.add_result(EvaluationResult(
            "Design System",
            "Class drift",
            "PASS",
            f"No actionable drift ({len(allowlisted_missing)} intentional no-style class(es) allowlisted)"
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
        
        serial_debug_count = 0
        string_hot_hits: List[str] = []
        string_other_hits: List[str] = []

        src_dir = config.FIRMWARE_ROOT
        ino_cpp_files = collect_firmware_source_files(src_dir, include_h=False)

        for file in ino_cpp_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            # Serial.print detection (unchanged logic)
            for i, line in enumerate(content.split('\n'), 1):
                if 'Serial.print' in line and 'SetupDebug' not in line and '//' not in line.split('Serial.print')[0]:
                    serial_debug_count += 1
                    if self.verbose:
                        print(f"  {file.name}:{i}: Serial.print usage")

            # String class detection (widened regex: catches decl, init, direct-init;
            # skips reference forms which do not allocate). Split by ADR-004 hot path.
            hits = scan_string_usages_detailed(content, file.name)
            if is_hot_path_file(file.name):
                string_hot_hits.extend(hits)
            else:
                string_other_hits.extend(hits)

        if serial_debug_count > 0:
            self.add_result(EvaluationResult(
                "Coding", "Serial Debug Output", "WARN",
                f"Found {serial_debug_count} Serial.print() usage (should use Debug macros)"
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "Serial Debug Output", "PASS",
                "No improper Serial.print() usage found"
            ))

        # ADR-004 hot-path String usage: reported as WARN with concrete call sites
        # until the existing debt is burned down. Follow-up task will flip to FAIL.
        if string_hot_hits:
            self.add_result(EvaluationResult(
                "Coding", "String Class in Hot Path (ADR-004)", "WARN",
                f"Found {len(string_hot_hits)} String usages in hot-path files (SAT*, MQTTstuff, restAPI, OTGW-Core, OTDirect)",
                "; ".join(string_hot_hits[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "String Class in Hot Path (ADR-004)", "PASS",
                "No String usages in hot-path files"
            ))

        if len(string_other_hits) > 5:
            self.add_result(EvaluationResult(
                "Coding", "String Class Usage", "WARN",
                f"Found {len(string_other_hits)} String class usages in non-hot-path files"
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "String Class Usage", "PASS",
                f"Limited String usage ({len(string_other_hits)} non-hot-path instances)"
            ))

    def check_memory_usage(self):
        """Analyze memory usage patterns"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Memory Analysis ==={Colors.ENDC}")
        
        # Check for large buffers
        large_buffers: List[Tuple[str, int]] = []
        src_dir = config.FIRMWARE_ROOT
        ino_cpp_files = collect_firmware_source_files(src_dir)
        
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

    # ===== PROGMEM COMPLIANCE CHECKS =====

    def check_progmem_compliance(self):
        """Check that string literals use PROGMEM wrappers (F(), PSTR(), _P suffixes)"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== PROGMEM Compliance ==={Colors.ENDC}")

        violations: List[str] = []
        src_dir = config.FIRMWARE_ROOT
        code_files = collect_firmware_source_files(src_dir)

        # Simple regex pre-filters — kept cheap. Real verification is done by
        # _progmem_call_violates(), which parses the arg list at paren-depth 1
        # so literals nested inside F(), PSTR(), or sub-function-calls (e.g.
        # httpServer.arg("v") inside strcmp) do NOT count as violations.
        debugln_re   = re.compile(r'\bDebugTln\s*\(')
        debugf_re    = re.compile(r'\bDebugTf\s*\(')
        snprintf_re  = re.compile(r'(?<!_P)\bsnprintf\s*\(')      # excludes snprintf_P
        strcmp_re    = re.compile(r'(?<!_P)\bstrcmp\s*\(')         # excludes strcmp_P
        strcasecmp_re = re.compile(r'(?<!_P)\bstrcasecmp\s*\(')   # excludes strcasecmp_P

        # ('match-name', regex, description, literal-arg-index)
        # literal-arg-index meaning:
        #   None  -> any unwrapped literal at depth 1 is a violation (strcmp, strcasecmp)
        #   N     -> only the Nth positional argument (0-based) being an unwrapped literal counts
        #            (DebugTln/DebugTf format is arg 0; snprintf format is arg 1, after the
        #            buffer at index 0). Other depth-1 literals are runtime data values, not
        #            flash-candidate strings, and are correctly ignored.
        call_specs = [
            ('DebugTln',   debugln_re,    'DebugTln without F() wrapper',                    0),
            ('DebugTf',    debugf_re,     'DebugTf without PSTR() wrapper',                  0),
            ('snprintf',   snprintf_re,   'snprintf without _P suffix',                      1),
            ('strcmp',     strcmp_re,     'strcmp with string literal (use strcmp_P + PSTR)', None),
            ('strcasecmp', strcasecmp_re, 'strcasecmp with string literal (use strcasecmp_P + PSTR)', None),
        ]

        for file in code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.read().split('\n')

            # Track multi-line #define macro continuations. Lines that follow a
            # backslash-terminated line are part of the macro body, even though
            # they don't start with "#define" themselves. The macro expands at
            # the call site, so PROGMEM compliance is judged there, not here.
            prev_was_continuation = False
            for i, line in enumerate(lines, 1):
                stripped = line.strip()

                # If the previous line ended with a continuation, this line is
                # a macro-body line — skip it but keep the chain alive if it
                # also ends with a backslash.
                if prev_was_continuation:
                    prev_was_continuation = stripped.endswith('\\')
                    continue

                # Skip comment lines and #define macro openers; remember whether
                # the opener itself continues onto the next line.
                if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('#define'):
                    prev_was_continuation = stripped.endswith('\\')
                    continue

                prev_was_continuation = False

                for name, pat, desc, arg_idx in call_specs:
                    for m in pat.finditer(line):
                        # m.end() points just past the opening '(' of the call
                        if _progmem_call_violates(line, m.end() - 1, arg_idx):
                            violations.append(f"{file.name}:{i}: {desc}")

        if violations:
            self.add_result(EvaluationResult(
                "PROGMEM", "Flash string compliance", "FAIL",
                f"Found {len(violations)} PROGMEM violations (wastes RAM)",
                "; ".join(violations[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "PROGMEM", "Flash string compliance", "PASS",
                "All string literals use proper PROGMEM wrappers"
            ))

    # ===== ARDUINOJSON BAN CHECK =====

    def check_no_arduinojson(self):
        """Ensure ArduinoJson library is never used (hard project rule)"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ArduinoJson Ban ==={Colors.ENDC}")

        violations: List[str] = []
        src_dir = config.FIRMWARE_ROOT
        code_files = collect_firmware_source_files(src_dir)

        include_re = re.compile(r'#include\s*[<"]ArduinoJson')
        types_re = re.compile(
            r'StaticJsonDocument|DynamicJsonDocument|JsonDocument|deserializeJson|serializeJson'
        )

        for file in code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.read().split('\n')

            for i, line in enumerate(lines, 1):
                if include_re.search(line):
                    violations.append(f"{file.name}:{i}: #include ArduinoJson")
                if types_re.search(line):
                    violations.append(f"{file.name}:{i}: ArduinoJson type usage")

        if violations:
            self.add_result(EvaluationResult(
                "ArduinoJson", "Library ban", "FAIL",
                f"Found {len(violations)} ArduinoJson usages (banned by project rules)",
                "; ".join(violations[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "ArduinoJson", "Library ban", "PASS",
                "No ArduinoJson usage found"
            ))

    # ===== STACK SAFETY CHECK =====

    def check_stack_safety(self):
        """Find non-static local char arrays > 256 bytes that risk stack overflow"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Stack Safety ==={Colors.ENDC}")

        warnings: List[str] = []
        src_dir = config.FIRMWARE_ROOT
        code_files = collect_firmware_source_files(src_dir)

        # Match lines starting with whitespace, containing char name[SIZE], not static
        buf_re = re.compile(r'char\s+(\w+)\[(\d+)\]')

        for file in code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.read().split('\n')

            for i, line in enumerate(lines, 1):
                stripped = line.lstrip()
                # Only look at indented lines (local variables), skip globals and statics
                if not line or line[0] not in (' ', '\t'):
                    continue
                if 'static' in line:
                    continue
                m = buf_re.search(line)
                if m:
                    name = m.group(1)
                    size = int(m.group(2))
                    if size > 256:
                        warnings.append(f"{file.name}:{i}: {name}[{size}]")

        if warnings:
            self.add_result(EvaluationResult(
                "Stack", "Large local buffers", "WARN",
                f"Found {len(warnings)} non-static local char buffers > 256 bytes (ESP8266 has 4KB stack)",
                "; ".join(warnings[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "Stack", "Large local buffers", "PASS",
                "No oversized local char buffers found"
            ))

    # ===== BUILD SYSTEM CHECKS =====

    def check_build_system(self):
        """Validate build system configuration"""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Build System ==={Colors.ENDC}")

        # Check platformio.ini (primary build system)
        pio_ini = self.project_dir / "platformio.ini"
        if pio_ini.exists():
            self.add_result(EvaluationResult(
                "Build", "platformio.ini", "PASS",
                "Found PlatformIO configuration (primary build system)"
            ))

            with open(pio_ini, 'r', encoding='utf-8', errors='ignore') as f:
                pio_content = f.read()

            # Parse [env:*] sections
            env_sections = re.findall(r'\[env:(\w+)\]', pio_content)
            if env_sections:
                self.add_result(EvaluationResult(
                    "Build", "PlatformIO environments", "INFO",
                    f"Found {len(env_sections)} build environments",
                    ", ".join(env_sections)
                ))

            # Check lib_deps version pinning
            lib_deps = re.findall(r'^\s+(\S.*\S)\s*$', pio_content, re.MULTILINE)
            # Filter to lines that look like library dependencies (after a lib_deps = line)
            in_lib_deps = False
            all_libs: List[str] = []
            unpinned: List[str] = []
            for line in pio_content.split('\n'):
                stripped = line.strip()
                if stripped.startswith('lib_deps'):
                    in_lib_deps = True
                    continue
                if in_lib_deps:
                    if stripped == '' or (not stripped.startswith(';') and '=' in stripped and not stripped.startswith('$')):
                        in_lib_deps = False
                        continue
                    if stripped.startswith(';') or stripped.startswith('$') or stripped == '':
                        continue
                    # This is a library line
                    lib_line = stripped.rstrip(';').strip()
                    if not lib_line or lib_line.startswith('$'):
                        continue
                    # Remove inline comments
                    if ';' in lib_line:
                        lib_line = lib_line[:lib_line.index(';')].strip()
                    all_libs.append(lib_line)
                    # Check if version-pinned (contains @ or is a URL)
                    if '@' not in lib_line and '://' not in lib_line:
                        unpinned.append(lib_line)

            if all_libs:
                if unpinned:
                    self.add_result(EvaluationResult(
                        "Build", "lib_deps version pinning", "WARN",
                        f"{len(unpinned)}/{len(all_libs)} libraries not version-pinned",
                        "; ".join(unpinned[:5])
                    ))
                else:
                    self.add_result(EvaluationResult(
                        "Build", "lib_deps version pinning", "PASS",
                        f"All {len(all_libs)} libraries are version-pinned or URL-based"
                    ))
        else:
            self.add_result(EvaluationResult(
                "Build", "platformio.ini", "FAIL",
                "platformio.ini not found (primary build system missing)"
            ))

        # Check Makefile (legacy)
        makefile = self.project_dir / "Makefile"
        if makefile.exists():
            self.add_result(EvaluationResult(
                "Build", "Makefile (legacy)", "INFO",
                "Found legacy Makefile"
            ))
        else:
            self.add_result(EvaluationResult(
                "Build", "Makefile (legacy)", "INFO",
                "No legacy Makefile (expected for PlatformIO-based builds)"
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
            # Operational guides live under docs/guides/ per the project layout;
            # also accept repo root and docs/ for backward compatibility.
            candidates = [
                (self.project_dir / doc, doc),
                (self.project_dir / "docs" / doc, f"docs/{doc}"),
                (self.project_dir / "docs" / "guides" / doc, f"docs/guides/{doc}"),
            ]
            found = next(((p, label) for p, label in candidates if p.exists()), None)
            if found:
                p, label = found
                self.add_result(EvaluationResult(
                    "Documentation", doc, "PASS",
                    f"Found ({label}, {p.stat().st_size} bytes)"
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
            'buffer_overflow_risk': [],
            'unsafe_sprintf': []
        }
        
        src_dir = config.FIRMWARE_ROOT
        all_code_files = collect_firmware_source_files(src_dir)
        
        for file in all_code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
                for i, line in enumerate(lines, 1):
                    # Check for potential hardcoded credentials
                    if re.search(r'(password|passwd|pwd|secret|key)\s*=\s*["\'](?!xxx|changeme|\*+)[^"\']{3,}["\']', line, re.I):
                        security_issues['hardcoded_creds'].append(f"{file.name}:{i}")
                    
                    # Check for unsafe string operations
                    if re.search(r'\b(strcpy|strcat|gets)\s*\(', line):
                        security_issues['unsafe_string_ops'].append(f"{file.name}:{i}")

                    # Check for bare sprintf (should use snprintf_P with size limit)
                    if re.search(r'\bsprintf\s*\(', line) and not re.search(r'\bsnprintf', line):
                        security_issues['unsafe_sprintf'].append(f"{file.name}:{i}")

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

        if security_issues['unsafe_sprintf']:
            self.add_result(EvaluationResult(
                "Security", "Unsafe sprintf", "WARN",
                f"Found {len(security_issues['unsafe_sprintf'])} bare sprintf() calls (use snprintf_P with size limit)",
                "; ".join(security_issues['unsafe_sprintf'][:5])
            ))
        else:
            self.add_result(EvaluationResult(
                "Security", "Unsafe sprintf", "PASS",
                "No bare sprintf() calls found"
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

    # ===== ADR-102: OT-BUS LIVENESS TOPIC BAN =====

    def check_adr102_otbus_liveness_topic(self):
        """ADR-102 CI gate: sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(...)) must never appear.
        That call writes OT-bus liveness to the HA availability topic, conflating MQTT-link
        state with OT-bus state and causing all HA entities to flap."""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR-102: OT-Bus Liveness Topic Ban ==={Colors.ENDC}")

        forbidden_re = re.compile(
            r'sendMQTT\s*\(\s*MQTTPubNamespace\s*,\s*CONLINEOFFLINE'
        )
        src_dir = config.FIRMWARE_ROOT
        code_files = collect_firmware_source_files(src_dir)

        violations: List[str] = []
        for file in code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            for i, line in enumerate(lines, 1):
                if forbidden_re.search(line):
                    violations.append(f"{file.name}:{i}: {line.strip()}")

        if violations:
            self.add_result(EvaluationResult(
                "ADR-102", "OT-bus liveness topic ban", "FAIL",
                f"Found {len(violations)} forbidden sendMQTT(MQTTPubNamespace, CONLINEOFFLINE) call(s) — "
                "ADR-102: HA avty_t must reflect only the MQTT link (birth/LWT); use otgw_connected for OT-bus liveness",
                "; ".join(violations)
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR-102", "OT-bus liveness topic ban", "PASS",
                "No sendMQTT(MQTTPubNamespace, CONLINEOFFLINE) call sites found"
            ))

    # ===== BINARY-SAFE COMPARE CHECK =====

    def check_binary_safe_compare(self):
        """INFO: flag every strncmp_P / strstr_P call site. These MUST operate on
        null-terminated text only; running them against binary buffers triggers
        Exception(2) on ESP8266. Use memcmp_P for binary data. See MEMORY.md."""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Binary-safe Compare Audit ==={Colors.ENDC}")

        src_dir = config.FIRMWARE_ROOT
        code_files = collect_firmware_source_files(src_dir)

        hits: List[str] = []
        for file in code_files:
            with open(file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            hits.extend(scan_binary_compare_calls(content, file.name))

        if hits:
            self.add_result(EvaluationResult(
                "Coding", "Binary-safe compare audit", "INFO",
                f"Found {len(hits)} strncmp_P/strstr_P call sites; verify each operates on null-terminated text (use memcmp_P for binary)",
                "; ".join(hits[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "Binary-safe compare audit", "PASS",
                "No strncmp_P / strstr_P call sites found"
            ))

    def check_otdirect_25238_bridge(self):
        """Regression gate for the ESP32/OTDirect 25238 bridge wiring.

        The bug class here is structural: the socket loop can exist, but if the
        cooperative service call or the no-PIC bridge ownership moves, the code
        still compiles while the bridge silently stops working. This check keeps
        the ownership split explicit and anchored to the current code layout."""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== OTDirect 25238 Bridge Audit ==={Colors.ENDC}")

        handle_debug = config.FIRMWARE_ROOT / "handleDebug.ino"
        firmware = config.FIRMWARE_ROOT / "OTGW-firmware.ino"
        otdirect = config.FIRMWARE_ROOT / "OTDirect.ino"

        try:
            handle_debug_text = handle_debug.read_text(encoding='utf-8', errors='ignore')
            firmware_text = firmware.read_text(encoding='utf-8', errors='ignore')
            otdirect_text = otdirect.read_text(encoding='utf-8', errors='ignore')
        except OSError as exc:
            self.add_result(EvaluationResult(
                "Coding", "OTDirect 25238 bridge audit", "WARN",
                f"Could not read bridge source files: {exc}"
            ))
            return

        checks = otdirect_25238_bridge_regressions(handle_debug_text, firmware_text, otdirect_text)
        missing = [name for name, ok in checks.items() if not ok]

        if missing:
            details = []
            if "service_loop" in missing:
                details.append("OTGWstream must keep a cooperative service call from firmware/background code.")
            if "inbound_bridge" in missing:
                details.append("OTDirect.ino must own the no-PIC inbound TCP bridge and keep the OTGWstream read->sendPICSerial path.")
            if "outbound_fanout" in missing:
                details.append("OTDirect.ino must keep fanout from bridgeFrameToParser()/synthesizeResponse() back to OTGWstream.")
            if "short_error_fanout" in missing:
                details.append("OTDirect short PIC-style rejection statuses must fan out to OTGWstream before processOT().")
            if "pr_response_fanout" in missing:
                details.append("OTDirect synthesized PR= responses must fan out to OTGWstream before processOT().")
            if "ownership_split" in missing:
                details.append("The split must remain: core/network services the socket, OTDirect owns the no-PIC bridge behavior.")

            self.add_result(EvaluationResult(
                "Coding", "OTDirect 25238 bridge audit", "INFO",
                f"{len(missing)}/6 bridge regressions detected",
                " ".join(details)
            ))
        else:
            self.add_result(EvaluationResult(
                "Coding", "OTDirect 25238 bridge audit", "PASS",
                "OTGWstream service, OTDirect inbound bridge, outbound fanout, short status fanout, and PR response fanout are wired to the current ownership split"
            ))

    # ===== ADR-080 GATES =====

    def check_adr_gates(self):
        """ADR-080: Accepted ADRs must reference an automated gate OR carry a
        classification label (structural / historical / policy / guideline-level /
        meta-level). Without either, a binding-on-paper rule drifts unchecked --
        exactly the pattern that ADR-004 demonstrated before this check existed."""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== ADR Gate Audit (ADR-080) ==={Colors.ENDC}")

        adr_dir = self.project_dir / "docs" / "adr"
        if not adr_dir.exists():
            self.add_result(EvaluationResult(
                "ADR", "ADR directory", "INFO",
                "docs/adr/ not found -- skipping ADR gate audit"
            ))
            return

        gaps: List[str] = []
        audited = 0
        for adr in sorted(adr_dir.glob("ADR-*.md")):
            try:
                text = adr.read_text(encoding='utf-8', errors='ignore')
            except OSError:
                continue
            if not adr_is_accepted(text):
                continue
            audited += 1
            if not adr_has_gate_or_label(text):
                gaps.append(adr.name)

        if gaps:
            self.add_result(EvaluationResult(
                "ADR", "ADR gate audit", "INFO",
                f"{len(gaps)}/{audited} Accepted ADRs lack both a gate reference "
                f"and a classification label (ADR-080)",
                "; ".join(gaps[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "ADR", "ADR gate audit", "PASS",
                f"All {audited} Accepted ADRs reference a gate or carry a classification label"
            ))

    def check_backlog_hygiene(self):
        """AC Deviation Protocol: Done tasks with unchecked ACs must reference a
        deviation, deferral, follow-up, or out-of-scope note. Otherwise the task
        is a silent partial Done -- the drift the protocol was written to prevent."""
        print(f"\n{Colors.BOLD}{Colors.OKBLUE}=== Backlog Hygiene (AC Deviation Protocol) ==={Colors.ENDC}")

        tasks_dir = self.project_dir / "backlog" / "tasks"
        if not tasks_dir.exists():
            self.add_result(EvaluationResult(
                "Backlog", "Backlog directory", "INFO",
                "backlog/tasks/ not found -- skipping hygiene check"
            ))
            return

        silent_partials: List[str] = []
        audited = 0
        for task in sorted(tasks_dir.glob("task-*.md")):
            try:
                text = task.read_text(encoding='utf-8', errors='ignore')
            except OSError:
                continue
            if task_status(text) != 'Done':
                continue
            audited += 1
            if task_unchecked_ac_count(text) == 0:
                continue
            if task_has_deviation_note(text):
                continue
            # Extract task id for a compact report
            short = task.name.split(' - ', 1)[0]
            silent_partials.append(short)

        if silent_partials:
            self.add_result(EvaluationResult(
                "Backlog", "AC Deviation Protocol", "INFO",
                f"{len(silent_partials)}/{audited} Done tasks have unchecked ACs without a deviation note",
                "; ".join(silent_partials[:10])
            ))
        else:
            self.add_result(EvaluationResult(
                "Backlog", "AC Deviation Protocol", "PASS",
                f"All {audited} Done tasks either have every AC checked or document the deviation"
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
        self.check_progmem_compliance()
        self.check_no_arduinojson()
        self.check_binary_safe_compare()
        self.check_otdirect_25238_bridge()
        self.check_adr102_otbus_liveness_topic()     # ADR-102 CI gate (TASK-623)
        self.check_adr_gates()
        self.check_backlog_hygiene()
        self.check_time_boundary_single_caller()      # ADR-086 CI gate (originally ADR-064, TASK-350)
        self.check_discovery_counter_instrumented()   # ADR-062 CI gate (TASK-364)
        self.check_publishedtopic_counter_reset()     # ADR-062 CI gate (TASK-364)
        self.check_ha_sensor_index_consistency()      # HA discovery gate (TASK-392)
        self.check_json_buffer_arithmetic()           # TASK-352/368
        self.check_status_burst_cooldown_bound()      # TASK-353/368
        self.check_status_publishers_wrap_burst()     # TASK-347/354/368, ADR-088 sub-rule 1
        self.check_drip_consults_deferred()           # TASK-426, ADR-088 sub-rule 3
        self.check_heap_tier_thresholds_ordered()     # TASK-428, ADR-089 sub-rule 1
        self.check_heap_fragmentation_promotion()     # TASK-428, ADR-089 sub-rule 2
        self.check_heap_tier_entry_counters()         # TASK-428, ADR-089 sub-rule 3
        self.check_design_system_drift()              # TASK-470, ADR-091 FAIL gate (TASK-480 grace complete)
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
            self.check_stack_safety()

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
