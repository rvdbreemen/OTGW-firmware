#!/usr/bin/env python3
"""Parallel host test runner (stdlib only).

Runs every tests/test_*.py as its own subprocess (each file runs standalone),
N at a time, with a per-file timeout. See docs/guides/test-runner.md.

    python scripts/run_tests.py                 # full suite, 4 parallel
    python scripts/run_tests.py -j 8 --timeout 900
    python scripts/run_tests.py --changed       # only tests affected vs origin/dev
    python scripts/run_tests.py --changed HEAD~3 --list
    python scripts/run_tests.py --webui         # plus node --test tests/webui/*.test.mjs

Exit code: 0 when every selected file passed or was skipped, 1 otherwise.
"""
import argparse
import concurrent.futures as cf
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "tests"
WEBUI = TESTS / "webui"

# Files that must not run inside the parallel pool; they run serially after it.
# Fill in with a reason when a file proves to collide in parallel (see the doc).
SERIAL_ONLY = {
    # "test_example.py": "reason",
}

# Words too generic to link a changed file to a test by name.
GENERIC_WORDS = {"test", "tests", "src", "firmware", "otgw", "main", "index",
                 "data", "docs", "script", "scripts", "build", "readme"}
# Changed paths that never influence host tests.
IGNORED_PREFIXES = ("backlog/", ".wolf/", ".claude/", "docs/", "graphify-out/")
IGNORED_SUFFIXES = (".md",)


def kill_tree(proc):
    """Kill a process and its children (compilers started by a test)."""
    try:
        if os.name == "nt":
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            proc.kill()
    except Exception:
        pass


def run_one(cmd, timeout):
    """Run cmd; return (rc or None on timeout, output, seconds)."""
    env = dict(os.environ, PYTHONUTF8="1", PYTHONIOENCODING="utf-8")
    t0 = time.time()
    proc = subprocess.Popen(cmd, cwd=str(ROOT), env=env, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        out, _ = proc.communicate(timeout=timeout)
        rc = proc.returncode
    except subprocess.TimeoutExpired:
        kill_tree(proc)
        try:
            out, _ = proc.communicate(timeout=10)
        except Exception:
            out = b""
        rc = None
    return rc, out.decode("utf-8", "replace"), time.time() - t0


def classify_py(rc, out):
    if rc is None:
        return "TIMEOUT"
    if rc != 0:
        return "FAILED"
    m = re.search(r"^Ran (\d+) tests?", out, re.M)
    if m and int(m.group(1)) == 0:
        return "SKIP"
    if re.search(r"^(SKIP|SKIPPED)\b", out.strip(), re.M) and not m:
        return "SKIP"
    if re.search(r"^FAILED", out, re.M):
        return "FAILED"
    return "PASS"


def classify_node(rc, out):
    if rc is None:
        return "TIMEOUT"
    if rc == 0 and re.search(r"(ℹ|#)\s*fail 0", out):
        return "PASS"
    return "FAILED"


def detail_py(out):
    m = re.search(r"^Ran (\d+) tests?", out, re.M)
    s = f"{m.group(1)} tests" if m else ""
    k = re.search(r"skipped=(\d+)", out)
    return s + (f", skipped={k.group(1)}" if k else "")


def git_lines(*args):
    r = subprocess.run(["git", *args], cwd=str(ROOT), capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} failed: {r.stderr.strip()}")
    return [l.strip().replace("\\", "/") for l in r.stdout.splitlines() if l.strip()]


def words(name):
    return {w for w in re.split(r"[^a-z0-9]+", name.lower())
            if len(w) >= 4 and w not in GENERIC_WORDS and not w.isdigit()}


def select_changed(ref, py_tests, mjs_tests):
    changed = set(git_lines("diff", "--name-only", ref))
    changed |= set(git_lines("ls-files", "--others", "--exclude-standard"))
    changed = {c for c in changed
               if not c.startswith(IGNORED_PREFIXES) and not c.endswith(IGNORED_SUFFIXES)}
    why = {}
    texts = {}
    for t in list(py_tests) + list(mjs_tests):
        try:
            texts[t] = t.read_text(encoding="utf-8", errors="replace")
        except OSError:
            texts[t] = ""
    for t in list(py_tests) + list(mjs_tests):
        rel = t.relative_to(ROOT).as_posix()
        reasons = []
        if rel in changed:
            reasons.append("test itself changed")
        t_words = words(t.stem)
        for c in sorted(changed):
            base = os.path.basename(c)
            if rel == c:
                continue
            if base in texts[t]:
                reasons.append(f"mentions {base}")
            else:
                shared = t_words & words(Path(c).stem)
                if shared:
                    reasons.append(f"name shares '{sorted(shared)[0]}' with {base}")
        if reasons:
            why[t] = reasons
    return changed, why


def main():
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-j", "--jobs", type=int, default=4, help="parallel files (default 4)")
    ap.add_argument("--timeout", type=int, default=600, help="seconds per file (default 600)")
    ap.add_argument("--changed", nargs="?", const="origin/dev", metavar="REF",
                    help="only tests affected by git diff REF (default origin/dev) + untracked")
    ap.add_argument("--webui", action="store_true", help="also run node --test tests/webui/*.test.mjs")
    ap.add_argument("--list", action="store_true", help="print the selection, do not run")
    ap.add_argument("pattern", nargs="*", help="optional substrings to restrict test file names")
    a = ap.parse_args()

    py_tests = sorted(TESTS.glob("test_*.py"))
    mjs_tests = sorted(WEBUI.glob("*.test.mjs")) if a.webui else []
    if a.pattern:
        py_tests = [t for t in py_tests if any(p in t.name for p in a.pattern)]
        mjs_tests = [t for t in mjs_tests if any(p in t.name for p in a.pattern)]

    if a.changed:
        changed, why = select_changed(a.changed, py_tests, mjs_tests)
        print(f"changed files vs {a.changed} (+untracked, docs/backlog ignored): {len(changed)}")
        if why:
            for t, rs in why.items():
                print(f"  select {t.name}: {'; '.join(rs[:3])}")
            py_tests = [t for t in py_tests if t in why]
            mjs_tests = [t for t in mjs_tests if t in why]
        else:
            print("  nothing maps to a test: running the full suite")

    jobs = [("py", t) for t in py_tests] + [("mjs", t) for t in mjs_tests]
    if a.list:
        for _, t in jobs:
            print(t.relative_to(ROOT).as_posix())
        print(f"{len(jobs)} files selected")
        return 0
    if not jobs:
        print("no tests selected")
        return 0
    if any(k == "mjs" for k, _ in jobs) and not shutil_which("node"):
        print("node not found on PATH; cannot run --webui tests", file=sys.stderr)
        return 1

    pool_jobs = [j for j in jobs if j[1].name not in SERIAL_ONLY]
    serial_jobs = [j for j in jobs if j[1].name in SERIAL_ONLY]
    results = []

    def work(job):
        kind, t = job
        cmd = ([sys.executable, str(t)] if kind == "py"
               else ["node", "--test", str(t)])
        rc, out, secs = run_one(cmd, a.timeout)
        status = classify_py(rc, out) if kind == "py" else classify_node(rc, out)
        return t, status, secs, out, (detail_py(out) if kind == "py" else "")

    def report(r):
        t, status, secs, _, det = r
        print(f"{status:<8} {secs:7.1f}s  {t.relative_to(ROOT).as_posix()}"
              + (f"  ({det})" if det else ""), flush=True)
        results.append(r)

    t0 = time.time()
    with cf.ThreadPoolExecutor(max_workers=max(1, a.jobs)) as ex:
        for fut in cf.as_completed([ex.submit(work, j) for j in pool_jobs]):
            report(fut.result())
    for j in serial_jobs:
        report(work(j))
    total = time.time() - t0

    bad = [r for r in results if r[1] in ("FAILED", "TIMEOUT")]
    for t, status, secs, out, _ in sorted(bad, key=lambda r: r[0].name):
        print(f"\n===== {status}: {t.relative_to(ROOT).as_posix()} (last 40 lines) =====")
        print("\n".join(out.splitlines()[-40:]))
    n = lambda s: sum(1 for r in results if r[1] == s)
    print(f"\nfiles: {len(results)}  passed: {n('PASS')}  failed: {n('FAILED')}  "
          f"timeout: {n('TIMEOUT')}  skipped: {n('SKIP')}  total time: {total:.1f}s  (-j {a.jobs})")
    return 1 if bad else 0


def shutil_which(name):
    import shutil
    return shutil.which(name)


if __name__ == "__main__":
    sys.exit(main())
