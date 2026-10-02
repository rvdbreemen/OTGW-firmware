#!/usr/bin/env python3
"""TASK-1130 host harness: the port-80 listener comes back after a refused bind.

    python test/host/test_webserver_listener_retry.py                       # FIX and mutants
    python test/host/test_webserver_listener_retry.py --old-rev a26b19134^  # plus OLD, before the retry

On the provisioning boot the WiFiManager config portal leaves its port-80 connections
in TIME_WAIT for 2*TCP_MSL (120 s). AsyncTCP binds without SOF_REUSEADDR, so the bind
in startWebserver() fails, and AsyncWebServer::begin() is void, so nothing reported it:
port 80 stayed refused until the next reboot. The fix logs the failed bind in
startWebserver() and retries it from doBackgroundTasks() every 5 s until the server
listens (handleWebserverListener()).

test_webserver_listener_retry.cpp is compiled from code extracted out of the revision
under test, never copied:
- from startWebserver() (FSexplorer.ino): the statements between its server.begin()
  and the "Set up first message" comment;
- handleWebserverListener() (FSexplorer.ino), when the revision has it;
- from doBackgroundTasks() (OTGW-firmware.ino): the lines that declare and fire the
  listener timer;
- safeTimers.h, as is.
Before the fix all three extractions are empty: there was no check and no retry, and
that absence is the defect the OLD run shows. Only AsyncWebServer and the lwIP bind are
modelled, in the driver.

Runtime cases, each in its own process so the function-local statics (the timer, the
retry counter) start fresh:
  L1  provisioning boot: the listener comes back after TIME_WAIT, the recovery is logged
      once with the true retry count, and begin() stops once the server listens
  L2  steady state: a server that listens from the start is left alone
  L3  a second outage is retried again and its log counts its own retries from zero
  L4  a refused first bind in startWebserver() is logged, not silent
  L5  the listener is back within one 5 s retry period after the bind becomes possible
Static cases, on the source text:
  S1  doBackgroundTasks() declares the listener timer and fires handleWebserverListener()
      from it, once
  S2  in startWebserver() the first statement after server.begin() checks the listener
      state and logs when it is not LISTEN

Each mutant of the FIX sources must fail exactly its listed cases. Every substitution
must match exactly once and change the text.

Exit code: 0 PASS, 1 FAIL, 2 harness error. A slice or compile failure is a harness
error, never a reproduction.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

FW = "src/OTGW-firmware/"
FSX, FW_INO, TIMERS = FW + "FSexplorer.ino", FW + "OTGW-firmware.ino", FW + "safeTimers.h"
RUNTIME = ["L1", "L2", "L3", "L4", "L5"]
STATIC = ["S1", "S2"]
OLD_DEFECT = ["L1", "L3", "L4", "L5", "S1", "S2"]

# (id, what, file, [(old text, new text), ...], cases that must fail)
MUTANTS = [
    ("MW1", "the loop no longer fires the retry", FW_INO,
     [("        if (DUE(timerWebListener)) handleWebserverListener();  // TASK-1130\n",
       "        // MW1: no retry call\n")],
     ["L1", "L3", "L5", "S1"]),
    ("MW2", "no early return while listening", FSX,
     [("  if (server.state() == LISTEN) return;\n", "")],
     ["L1", "L2", "L3"]),
    ("MW3", "the retry counter is not reset after a recovery", FSX,
     [("    retries = 0;\n", "")],
     ["L3"]),
    ("MW4", "startWebserver() no longer reports a failed bind", FSX,
     [("  if (server.state() != LISTEN) {\n"
       "    DebugTln(F(\"HTTP Server: bind on port 80 failed, retrying every 5s\"));\n"
       "  }\n", "")],
     ["L4", "S2"]),
    ("MW5", "the retry period is 600 s instead of 5 s", FW_INO,
     [("DECLARE_TIMER_SEC(timerWebListener, 5, SKIP_MISSED_TICKS)",
       "DECLARE_TIMER_SEC(timerWebListener, 600, SKIP_MISSED_TICKS)")],
     ["L1", "L3", "L5"]),
]


def read(path, rev):
    if rev is None:
        text = (REPO / path).read_text(encoding="utf-8", errors="replace")
    else:
        text = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                              text=True, encoding="utf-8", errors="replace", check=True).stdout
    return text.replace("\r\n", "\n")


def sources(rev):
    return {p: read(p, rev) for p in (FSX, FW_INO, TIMERS)}


def function_body(lines, rx):
    """The function whose signature line is the one line matching rx, through its
    closing brace at column 0, as a list of lines."""
    hits = [i for i, l in enumerate(lines) if re.search(rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match exactly once, got {len(hits)}: {rx}")
    end = next((i for i in range(hits[0] + 1, len(lines)) if re.match(r"^\}", lines[i])), None)
    if end is None:
        raise SystemExit(f"no closing brace at column 0 after: {rx}")
    return lines[hits[0]:end + 1]


def startwebserver_tail(fsx):
    body = function_body(fsx.split("\n"), r"^void startWebserver\(\)\s*\{")
    b = [i for i, l in enumerate(body) if re.match(r"^\s*server\.begin\(\);\s*$", l)]
    e = [i for i, l in enumerate(body) if "// Set up first message as the IP address" in l]
    if len(b) != 1 or len(e) != 1 or e[0] < b[0]:
        raise SystemExit(f"startWebserver(): need one server.begin() before one 'Set up first message' comment, got {b} / {e}")
    return body[b[0] + 1:e[0]]


def listener_function(fsx):
    lines = fsx.split("\n")
    if not any(re.match(r"^void handleWebserverListener\(\)\s*\{", l) for l in lines):
        return []
    return function_body(lines, r"^void handleWebserverListener\(\)\s*\{")


def background_body(fw):
    return function_body(fw.split("\n"), r"^void doBackgroundTasks\(\)\s*(\{\s*)?$")


def tick_lines(fw):
    return [l for l in background_body(fw) if re.search(r"timerWebListener|handleWebserverListener", l)]


def audit(name, lines, source):
    text = "\n".join(lines)
    if text and (text not in source or text.count("{") != text.count("}")):
        raise SystemExit(f"slice audit failed for {name}: not verbatim in its source or braces unbalanced")
    return text


def static_cases(src):
    body = background_body(src[FW_INO])
    decl = [i for i, l in enumerate(body) if re.search(r"DECLARE_TIMER_SEC\(\s*timerWebListener\s*,", l)]
    call = [i for i, l in enumerate(body) if "handleWebserverListener()" in l]
    s1 = (len(decl) == 1 and len(call) == 1 and decl[0] < call[0]
          and re.search(r"if\s*\(\s*DUE\(\s*timerWebListener\s*\)\s*\)\s*handleWebserverListener\(\);", body[call[0]]))
    s1_got = f"timer declarations {len(decl)}, handleWebserverListener() calls {len(call)} in doBackgroundTasks()"
    code = [l for l in startwebserver_tail(src[FSX]) if l.strip() and not l.strip().startswith("//")]
    s2 = (len(code) >= 2 and re.match(r"^\s*if \(server\.state\(\) != LISTEN\) \{\s*$", code[0])
          and re.search(r"Debug(Tln|Tf)\(", code[1]))
    s2_got = f"first statement after server.begin(): {code[0].strip() if code else '(none before the IP message)'}"
    return {"S1": ("pass" if s1 else "FAIL", "doBackgroundTasks() declares the listener timer and fires handleWebserverListener() from it, once", s1_got),
            "S2": ("pass" if s2 else "FAIL", "the first statement after server.begin() in startWebserver() checks the listener state and logs", s2_got)}


def export(src, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    head = "// GENERATED by test_webserver_listener_retry.py. Do not edit.\n"
    (out_dir / "tail.inc").write_text(head + audit("tail", startwebserver_tail(src[FSX]), src[FSX]) + "\n", encoding="utf-8")
    (out_dir / "listener.inc").write_text(head + audit("listener", listener_function(src[FSX]), src[FSX]) + "\n", encoding="utf-8")
    (out_dir / "tick.inc").write_text(head + audit("tick", tick_lines(src[FW_INO]), src[FW_INO]) + "\n", encoding="utf-8")
    (out_dir / "safeTimers.h").write_text(src[TIMERS], encoding="utf-8")


def build(label, out_dir):
    exe = out_dir / "test_webserver_listener_retry.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_webserver_listener_retry.cpp'}\" /I\"{out_dir}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    return exe


def run_variant(label, out_dir, src, verbose):
    export(src, out_dir)
    exe = build(label, out_dir)
    results, text = {}, []
    for case in RUNTIME:
        r = subprocess.run([str(exe), case], capture_output=True, text=True)
        m = re.search(rf"^CASE {case} (pass|FAIL)\b", r.stdout, re.M)
        if not m or r.returncode not in (0, 1):
            print(r.stdout, r.stderr)
            print(f"HARNESS ERROR: case {case} of {label} did not report (exit code {r.returncode})")
            sys.exit(2)
        results[case] = m.group(1)
        text.append(r.stdout.rstrip("\n"))
    for case, (verdict, title, got) in static_cases(src).items():
        results[case] = verdict
        text.append(f"CASE {case} {verdict}\n     {title}\n     got : {got}")
    failed = sorted(k for k, v in results.items() if v == "FAIL")
    if verbose:
        print(f"== {label} ==")
        print("\n".join(text))
    return failed, dict(zip(RUNTIME + STATIC, text))


def mutate(text, subs, fname):
    for old, new in subs:
        n = text.count(old)
        if n != 1:
            raise SystemExit(f"mutant anchor must match exactly once in {fname}, got {n}: {old!r}")
        mutated = text.replace(old, new)
        if mutated == text:
            raise SystemExit(f"mutant left {fname} unchanged")
        text = mutated
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--old-rev", help="also run the cases on the sources at this git revision (e.g. a26b19134^)")
    args = ap.parse_args()
    root = GEN / "webserver_listener_retry"
    checks = []

    fix = sources(None)
    failed, _ = run_variant("FIX (working tree)", root / "fix", fix, verbose=True)
    checks.append(("FIX passes every case", failed == []))

    if args.old_rev:
        old = sources(args.old_rev)
        failed, _ = run_variant(f"OLD (git {args.old_rev})", root / "old", old, verbose=True)
        print(f"   OLD failed: {', '.join(failed) or 'none'}")
        checks.append((f"OLD fails exactly {', '.join(OLD_DEFECT)} and passes L2", failed == sorted(OLD_DEFECT)))

    for mid, what, fname, subs, expect in MUTANTS:
        src = dict(fix)
        src[fname] = mutate(fix[fname], subs, fname)
        failed, text = run_variant(f"{mid} {what}", root / f"mutant_{mid}", src, verbose=False)
        print(f"== {mid}: {what} -> failed cases: {', '.join(failed) or 'none'} (expected {', '.join(expect)})")
        for case in expect:
            print("   " + text[case].replace("\n", "\n   "))
        checks.append((f"{mid} fails exactly {', '.join(expect)}", failed == sorted(expect)))

    print("== verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
