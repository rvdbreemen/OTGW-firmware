#!/usr/bin/env python3
"""ADR-185 check: do the NimBLE host overrides in include/sdkconfig.h reach the NimBLE sources?

    python scripts/check_nimble_overrides.py --env esp32-combo

It needs compile_commands.json for that env in the project root. Generate it with
PlatformIO's own interpreter (pio rejects Python 3.14; build.bat bootstraps one):

    .build-python\\python.exe -m platformio run -e <env> -t compiledb
    (or .build-venv\\Scripts\\python.exe, whichever build.bat created)

The database holds one env at a time; the script refuses a database made for
another env. Delete src/OTGW-firmware/OTGW-firmware.ino.cpp if pio leaves it behind.

For each checked translation unit (default: NimBLE-Arduino's src/NimBLEDevice.cpp
and src/nimble/nimble/host/src/ble_hs.c) the script reruns the recorded
command with -E -dM (preprocess only, dump the macros) and checks:
1. every CONFIG_BT_NIMBLE_* value that include/sdkconfig.h defines has that value;
2. the MYNEWT_VAL_* settings that size the host follow those values. For example,
   MYNEWT_VAL_MSYS_1_BLOCK_COUNT reaches the code only through the framework alias
   CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT (nimconfig.h); without that alias it falls
   back to 12 whatever the wrapper says;
3. the values ADR-185 must not change still equal the framework sdkconfig.h:
   CONFIG_BT_CTRL_*, CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT and the EVT pools;
4. the include/ dir is an -I dir that comes before the framework sdkconfig dir.
   Points 1-3 are end-of-TU values; only this order proves that every include of
   sdkconfig.h, quoted or angled, reaches the wrapper first.

It also checks the link map (.pio/build/<env>/firmware.map, or --map): the link
must take nothing from libbt.a but bt.c.obj, the controller glue. Any other member
is a piece of the prebuilt IDF NimBLE host mixed into NimBLE-Arduino's own host,
which happens when a role setting leaves NimBLE-Arduino without code that its other
files still call (ADR-185). A missing map is a failure; --no-map skips this check
for a preprocessor-only run.

Exit status: 0 when everything matches, 1 on a mismatch or a failed preprocessor
run, 2 on a usage or setup error.
"""
import argparse
import ast
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TUS = ["src/NimBLEDevice.cpp", "src/nimble/nimble/host/src/ble_hs.c"]

# MYNEWT_VAL_* setting that sizes the host -> the CONFIG_ value it must follow.
DERIVED = {
    "MYNEWT_VAL_BLE_ROLE_CENTRAL": "CONFIG_BT_NIMBLE_ROLE_CENTRAL",
    "MYNEWT_VAL_BLE_ROLE_PERIPHERAL": "CONFIG_BT_NIMBLE_ROLE_PERIPHERAL",
    "MYNEWT_VAL_BLE_ROLE_BROADCASTER": "CONFIG_BT_NIMBLE_ROLE_BROADCASTER",
    "MYNEWT_VAL_BLE_ROLE_OBSERVER": "CONFIG_BT_NIMBLE_ROLE_OBSERVER",
    "MYNEWT_VAL_BLE_MAX_CONNECTIONS": "CONFIG_BT_NIMBLE_MAX_CONNECTIONS",
    "MYNEWT_VAL_BLE_STORE_MAX_BONDS": "CONFIG_BT_NIMBLE_MAX_BONDS",
    "MYNEWT_VAL_BLE_STORE_MAX_CCCDS": "CONFIG_BT_NIMBLE_MAX_CCCDS",
    "MYNEWT_VAL_BLE_ATT_PREFERRED_MTU": "CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU",
    "MYNEWT_VAL_BLE_TRANSPORT_ACL_FROM_LL_COUNT": "CONFIG_BT_NIMBLE_TRANSPORT_ACL_FROM_LL_COUNT",
    "MYNEWT_VAL_MSYS_1_BLOCK_COUNT": "CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT",
    "MYNEWT_VAL_MSYS_2_BLOCK_COUNT": "CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT",
    "MYNEWT_VAL_BLE_TRANSPORT_EVT_COUNT": "CONFIG_BT_NIMBLE_TRANSPORT_EVT_COUNT",
    "MYNEWT_VAL_BLE_TRANSPORT_EVT_DISCARDABLE_COUNT": "CONFIG_BT_NIMBLE_TRANSPORT_EVT_DISCARD_COUNT",
}

# ADR-185 "Must Not": these keep the framework value.
GUARDED_EXACT = {
    "CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT",
    "CONFIG_BT_NIMBLE_TRANSPORT_EVT_COUNT",
    "CONFIG_BT_NIMBLE_TRANSPORT_EVT_DISCARD_COUNT",
    "CONFIG_BT_NIMBLE_TRANSPORT_EVT_SIZE",
}
GUARDED_PREFIX = "CONFIG_BT_CTRL_"

DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?\s*(.*?)\s*$")
TOKEN_RE = re.compile(r"0[xX][0-9a-fA-F]+[uUlL]*|\d+[uUlL]*|[A-Za-z_]\w*")


def fail_setup(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(2)


def win_argv(s):
    """Split a Windows command line with the MSVCRT (CommandLineToArgvW) rules."""
    bs, qt = "\\", '"'
    args, cur, inq, have, i, n = [], [], False, False, 0, len(s)
    while i < n:
        c = s[i]
        if c == bs:
            j = i
            while j < n and s[j] == bs:
                j += 1
            if j < n and s[j] == qt:
                cur.append(bs * ((j - i) // 2))
                if (j - i) % 2:
                    cur.append(qt)
                else:
                    inq = not inq
                i = j + 1
            else:
                cur.append(bs * (j - i))
                i = j
            have = True
        elif c == qt:
            inq, have, i = not inq, True, i + 1
        elif c in " \t" and not inq:
            if have:
                args.append("".join(cur))
                cur, have = [], False
            i += 1
        else:
            cur.append(c)
            have, i = True, i + 1
    if have:
        args.append("".join(cur))
    return args


def entry_argv(entry):
    if "arguments" in entry:
        return list(entry["arguments"])
    cmd = entry["command"]
    return win_argv(cmd) if os.name == "nt" else shlex.split(cmd)


def norm(p):
    return str(p).replace("\\", "/")


def parse_defines(text, strip_comments=False):
    """Object-like macros only: name -> body. Function-like macros are skipped."""
    if strip_comments:
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        text = re.sub(r"//[^\n]*", "", text)
    out = {}
    for line in text.splitlines():
        m = DEFINE_RE.match(line)
        if m and not m.group(2):
            out[m.group(1)] = m.group(3)
    return out


def c_int(tok):
    tok = tok.rstrip("uUlL")
    if tok[:2] in ("0x", "0X"):
        return int(tok, 16)
    if len(tok) > 1 and tok[0] == "0":
        return int(tok, 8)
    return int(tok)


def evaluate(name, macros, depth=0):
    """Expand an object-like macro to an int; raise ValueError when that is not possible."""
    if name not in macros:
        raise ValueError(f"{name} is not defined")
    if depth > 32:
        raise ValueError(f"{name}: expansion too deep")

    def sub(m):
        tok = m.group(0)
        if tok[0].isdigit():
            return str(c_int(tok))
        if tok in macros:
            return f"({evaluate(tok, macros, depth + 1)})"
        raise ValueError(f"{name}: unresolved identifier {tok}")

    expr = TOKEN_RE.sub(sub, macros[name])
    if not expr.strip():
        raise ValueError(f"{name} is defined empty")
    return safe_int(expr, name)


def safe_int(expr, name):
    """Evaluate an integer expression built from + - * and parentheses only."""
    try:
        tree = ast.parse(expr.strip(), mode="eval")
    except SyntaxError as e:
        raise ValueError(f"{name}: cannot parse '{expr}'") from e
    ops = {ast.Add: lambda a, b: a + b, ast.Sub: lambda a, b: a - b, ast.Mult: lambda a, b: a * b}

    def ev(node):
        if isinstance(node, ast.Expression):
            return ev(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd)):
            v = ev(node.operand)
            return -v if isinstance(node.op, ast.USub) else v
        if isinstance(node, ast.BinOp) and type(node.op) in ops:
            return ops[type(node.op)](ev(node.left), ev(node.right))
        raise ValueError(f"{name}: unsupported expression '{expr}'")

    return ev(tree)


def value_of(name, macros):
    """The macro as an int when it evaluates to one, else its raw body or a marker."""
    if name not in macros:
        return "undefined"
    try:
        return evaluate(name, macros)
    except ValueError:
        return macros[name].strip()


def find_compiler(argv0):
    if os.path.isfile(argv0):
        return argv0
    found = shutil.which(argv0)
    if found:
        return found
    core = os.environ.get("PLATFORMIO_CORE_DIR") or os.path.join(os.path.expanduser("~"), ".platformio")
    for exe in (argv0, argv0 + ".exe"):
        cand = os.path.join(core, "packages", "toolchain-xtensa-esp-elf", "bin", os.path.basename(exe))
        if os.path.isfile(cand):
            return cand
    fail_setup(f"compiler {argv0} not found (set PLATFORMIO_CORE_DIR or put the toolchain on PATH)")
    return None


def preprocess(entry, out_file):
    """Rerun the recorded command with -E -dM, writing the macros to out_file."""
    argv = entry_argv(entry)
    argv[0] = find_compiler(argv[0])
    cleaned, skip = [], False
    for a in argv:
        if skip:
            skip = False
            continue
        if a in ("-c", "-MMD", "-MD", "-MP"):
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a.startswith("-o") and len(a) > 2:
            continue
        cleaned.append(a)
    cleaned[1:1] = ["-E", "-dM", "-o", str(out_file)]
    r = subprocess.run(cleaned, cwd=entry["directory"], capture_output=True, text=True)
    return r.returncode == 0, r.stderr


def include_dirs(entry):
    dirs, argv = [], entry_argv(entry)
    for i, a in enumerate(argv):
        if a == "-I" and i + 1 < len(argv):
            dirs.append(argv[i + 1])
        elif a.startswith("-I") and len(a) > 2:
            dirs.append(a[2:])
    base = Path(entry["directory"])
    return [(base / d).resolve() for d in dirs]


def libbt_members(map_path):
    """libbt.a members the linker pulled in, with the reference that pulled each."""
    text = map_path.read_text(encoding="utf-8", errors="replace")
    start = text.find("Archive member included to satisfy reference by file (symbol)")
    if start < 0:
        return None
    ends = [i for i in (text.find(k, start) for k in ("Discarded input sections", "Allocating common symbols",
                                                      "Memory Configuration")) if i > 0]
    lines = text[start:min(ends) if ends else len(text)].splitlines()
    found = []
    for i, line in enumerate(lines):
        m = re.search(r"libbt\.a\(([^)]+)\)", line)
        if m and not line.startswith(" "):
            ref = lines[i + 1].strip() if i + 1 < len(lines) else ""
            found.append((m.group(1), re.sub(r".*[\\/]", "", ref)))
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--env", required=True, help="PlatformIO env the database was made for, e.g. esp32-combo")
    ap.add_argument("--db", default=str(ROOT / "compile_commands.json"), help="compile_commands.json path")
    ap.add_argument("--overrides", default=str(ROOT / "include" / "sdkconfig.h"),
                    help="header whose CONFIG_BT_NIMBLE_* defines are the intended values")
    ap.add_argument("--tu", action="append", default=None,
                    help="NimBLE-Arduino source to check, relative to the library root (repeatable; "
                         "default: src/NimBLEDevice.cpp and src/nimble/nimble/host/src/ble_hs.c)")
    ap.add_argument("--dump-dir", default=None, help="keep the -dM output files in this directory")
    ap.add_argument("--map", default=None,
                    help="linker map to check for libbt.a members (default: .pio/build/<env>/firmware.map if it exists)")
    ap.add_argument("--no-map", action="store_true",
                    help="skip the libbt.a link-map check (preprocessor-only run; the link stays unproven)")
    ap.add_argument("-v", "--verbose", action="store_true", help="print every check, not only failures")
    args = ap.parse_args()

    db_path = Path(args.db)
    if not db_path.is_file():
        fail_setup(f"{db_path} not found. Generate it first:\n"
                   f"  <pio python> -m platformio run -e {args.env} -t compiledb\n"
                   "  (see the header of this script)")
    db = json.loads(db_path.read_text(encoding="utf-8"))

    lib_root = f"/.pio/libdeps/{args.env}/NimBLE-Arduino/"
    if not any(lib_root in "/" + norm(e["file"]) for e in db):
        seen = sorted({m.group(1) for e in db for m in [re.search(r"/libdeps/([^/]+)/", norm(e["file"]))] if m})
        fail_setup(f"{db_path} has no NimBLE-Arduino sources for env '{args.env}' "
                   f"(it was made for: {', '.join(seen) or 'unknown'}). Regenerate it for this env.")

    ovr_path = Path(args.overrides).resolve()
    if not ovr_path.is_file():
        fail_setup(f"{ovr_path} not found")
    overrides = {}
    for name, body in parse_defines(ovr_path.read_text(encoding="utf-8"), strip_comments=True).items():
        try:
            overrides[name] = safe_int(body, name)
        except ValueError:
            fail_setup(f"{ovr_path}: {name} has a non-integer value '{body}'")
    if not overrides:
        fail_setup(f"{ovr_path} defines no values; nothing to check")

    problems = []
    print(f"env {args.env}, database {db_path}, overrides {ovr_path}")
    for name in overrides:
        if not name.startswith("CONFIG_BT_NIMBLE_"):
            problems.append(f"wrapper defines {name}; ADR-185 allows only CONFIG_BT_NIMBLE_* host values")
        if name in GUARDED_EXACT or name.startswith(GUARDED_PREFIX):
            problems.append(f"wrapper overrides {name}, which ADR-185 says must keep the framework value")
    for p in problems:
        print(f"  FAIL {p}")

    tmp = None
    if args.dump_dir:
        dump_dir = Path(args.dump_dir)
        dump_dir.mkdir(parents=True, exist_ok=True)
    else:
        tmp = tempfile.TemporaryDirectory()
        dump_dir = Path(tmp.name)

    for tu in args.tu or DEFAULT_TUS:
        suffix = lib_root + tu.lstrip("/")
        matches = [e for e in db if ("/" + norm(e["file"])).endswith(suffix)]
        print(f"\n[{tu}]")
        if len(matches) != 1:
            problems.append(f"{tu}: {len(matches)} entries in the database (need exactly 1)")
            print(f"  FAIL {problems[-1]}")
            continue
        entry = matches[0]
        out_file = dump_dir / (re.sub(r"[^\w.]+", "_", tu) + ".dM.txt")
        ok, err = preprocess(entry, out_file)
        if not ok:
            problems.append(f"{tu}: preprocessor run failed")
            print(f"  FAIL {problems[-1]}\n{err[-2000:]}")
            continue
        macros = parse_defines(out_file.read_text(encoding="utf-8", errors="replace"))

        incs = include_dirs(entry)
        fw_file = next((d / "sdkconfig.h" for d in incs
                        if (d / "sdkconfig.h").resolve() != ovr_path and (d / "sdkconfig.h").is_file()), None)
        if fw_file is None:
            problems.append(f"{tu}: framework sdkconfig.h not found in the -I dirs")
            print(f"  FAIL {problems[-1]}")
            continue
        pos = {d: i for i, d in enumerate(incs)}
        print(f"  -I order: wrapper dir at {pos.get(ovr_path.parent, 'absent')}, "
              f"framework sdkconfig dir at {pos.get(fw_file.parent)} (of {len(incs)} -I dirs)")
        # The values below are end-of-TU values. Only the -I order proves that every
        # include of sdkconfig.h, quoted or angled, reaches the wrapper first.
        if ovr_path.parent not in pos:
            problems.append(f"{tu}: the wrapper dir {ovr_path.parent} is not an -I dir")
            print(f"  FAIL {problems[-1]}")
        elif pos[ovr_path.parent] > pos[fw_file.parent]:
            problems.append(f"{tu}: the wrapper dir comes after the framework sdkconfig dir in the -I order")
            print(f"  FAIL {problems[-1]}")
        fw = parse_defines(fw_file.read_text(encoding="utf-8", errors="replace"))

        rows = [(name, fw.get(name, "-"), want, value_of(name, macros)) for name, want in overrides.items()]
        n_derived = 0
        for myn, cfg in DERIVED.items():
            want = overrides[cfg] if cfg in overrides else value_of(cfg, fw)
            rows.append((myn, f"via {cfg}", want, value_of(myn, macros)))
            n_derived += 1
        guarded = sorted(n for n in fw if n in GUARDED_EXACT or n.startswith(GUARDED_PREFIX))
        rows += [(name, fw[name], value_of(name, fw), value_of(name, macros)) for name in guarded]

        bad = 0
        for name, fwv, want, got in rows:
            good = got == want
            if not good:
                bad += 1
                problems.append(f"{tu}: {name} is {got}, expected {want}")
            if args.verbose or not good:
                print(f"  {'ok  ' if good else 'FAIL'} {name:46} framework={fwv!s:22} expected={want!s:5} actual={got}")
        print(f"  {len(rows) - bad}/{len(rows)} ok: {len(overrides)} overrides, {n_derived} derived "
              f"MYNEWT_VAL values, {len(guarded)} values that must stay at the framework setting")

    if tmp:
        tmp.cleanup()

    map_path = Path(args.map) if args.map else ROOT / ".pio" / "build" / args.env / "firmware.map"
    print(f"\n[link map {map_path}]")
    if args.no_map:
        print("  skipped (--no-map): this run proves the preprocessor values only, not the link")
    elif not map_path.is_file():
        problems.append(f"{map_path} not found; build the env first, or pass --no-map for a "
                        "preprocessor-only run")
        print(f"  FAIL {problems[-1]}")
    else:
        if map_path.stat().st_mtime < ovr_path.stat().st_mtime:
            print("  note: the map is older than the overrides header; rebuild for a current answer")
        members = libbt_members(map_path)
        if members is None:
            problems.append("link map has no archive-member section")
            print(f"  FAIL {problems[-1]}")
        else:
            for member, ref in members:
                good = member == "bt.c.obj"
                if not good:
                    problems.append(f"link takes libbt.a({member}), a piece of the prebuilt NimBLE host")
                print(f"  {'ok  ' if good else 'FAIL'} libbt.a({member})  pulled by {ref}")

    print("\nRESULT:", "PASS" if not problems else f"FAIL ({len(problems)} problem(s))")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
