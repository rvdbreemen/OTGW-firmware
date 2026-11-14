#!/usr/bin/env python3
"""Compile the real webSendFile path and file gate behind bounded platform fakes.

Run: python3 tests/test_web_file_scheduling.py
--old-rev <revision> builds the same fixture against an older header/gate. Revisions
before the queue fail to compile against it (the fixture uses the queue counters), so
that run shows the queue is missing, not a behavioural failure of the old code.
Only Arduino, LittleFS and ESPAsyncWebServer's transport boundary are faked.

TASK-1640 (ADR-215): the same fixture also compiles the real gzipped-page serving code,
sliced by text anchors out of FSexplorer.ino (Helper, serveGzipPage, sendFSexplorerPage,
sendIndex, handleFile) and webServerCompat.h (webSendGzipPage). It checks the ETag
"<githash>-<path>", 304 on a matching If-None-Match, Content-Encoding gzip exactly once, the
plain-page fallback on a filesystem from before ADR-215 and the handleFile upload-prompt gate
(TASK-1926: existing files are served without a recovery page; the prompt only for a missing path).
--mutant <name> applies one defect to that production text and passes only when the fixture
FAILS on it (proof the checks bite): no-plain-fallback, double-encoding-file,
double-encoding-progmem, etag-without-path, handlefile-gz-only-gate.
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path, rev, encoding=None):
    if rev:
        return subprocess.check_output(["git", "show", f"{rev}:{path}"], cwd=ROOT, text=True, encoding=encoding)
    return (ROOT / path).read_text(encoding=encoding)


# One defect each, applied to the production text before it is compiled: (file, old, new).
MUTANTS = {
    "no-plain-fallback": ("FSexplorer.ino",
                          "if (!plainPath || !LittleFS.exists(plainPath)) return false;", "return false;"),
    "double-encoding-file": ("FSexplorer.ino",
                             # TASK-1689: serveGzipPage takes the mime as a parameter
                             'webSendFile(path, type, gzip);',
                             'webPushHeader(F("Content-Encoding"), F("gzip"));\n  '
                             'webSendFile(path, type, gzip);'),
    "double-encoding-progmem": ("webServerCompat.h",
                                # TASK-1723: anchor follows the TASK-1696B form (header only when gzip)
                                '  if (gzip) webPushHeader(F("Content-Encoding"), F("gzip"));\n  AsyncWebServerResponse* resp',
                                '  if (gzip) webPushHeader(F("Content-Encoding"), F("gzip"));\n'
                                '  webPushHeader(F("Content-Encoding"), F("gzip"));\n  AsyncWebServerResponse* resp'),
    "etag-without-path": ("FSexplorer.ino", r'PSTR("\"%s-%s\""), fsHash, path);', r'PSTR("\"%s%s\""), fsHash, "");'),
    "handlefile-gz-only-gate": ("FSexplorer.ino", ' && !LittleFS.exists("/FSexplorer.html")) {', ') {'),
}


def mutate(text, name, which):
    if not name or MUTANTS[name][0] != which:
        return text
    _, old, new = MUTANTS[name]
    if text.count(old) != 1:
        raise SystemExit(f"mutant {name}: anchor not found exactly once in {which}")
    return text.replace(old, new)


def slice_between(text, start, end, include_end=False):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b + (len(end) if include_end else 0)]


def gzip_pages_inc(fs, mutant):
    """The production gzipped-page code from FSexplorer.ino, in source order, ASCII only."""
    fs = mutate(fs, mutant, "FSexplorer.ino")
    parts = [
        slice_between(fs, "const char Helper[] PROGMEM =", ";\n", True),
        slice_between(fs, "static bool serveGzipPage(", "// TASK-1294: static text"),
        slice_between(fs, "static void sendIndex(", "\n}\n", True),
        slice_between(fs, "static void sendFSexplorerRedirect() {", "} // handleFile()", True),
    ]
    return re.sub(r"[^\x00-\x7f]", "?", "\n".join(parts) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old-rev")
    parser.add_argument("--mutant", choices=sorted(MUTANTS),
                        help="apply one defect to the production gzip-page code; pass only if the fixture fails")
    args = parser.parse_args()
    compiler = shutil.which("clang++") or shutil.which("g++")
    vcvars = None
    if not compiler:
        # TASK-1723: on Windows without g++/clang++, use MSVC through the shared vcvars lookup.
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from test_sat_pwm_override import _vcvars  # noqa: E402
        vcvars = _vcvars()
        if not vcvars:
            raise SystemExit("A host C++ compiler is required (clang++, g++ or MSVC)")
    with tempfile.TemporaryDirectory(prefix="otgw-file-scheduling-") as tmp:
        directory = Path(tmp)
        gate = source("src/OTGW-firmware/restAPI.ino", args.old_rev)
        # Compile the complete production gate block, not a copied implementation.
        start = gate.index("#ifndef WEB_FILE_MAX_INFLIGHT")
        end = gate.index("// Zero-allocation HTTP method", start)
        header = source("src/OTGW-firmware/webServerCompat.h", args.old_rev)
        header = mutate(header, args.mutant, "webServerCompat.h")
        fs = source("src/OTGW-firmware/FSexplorer.ino", args.old_rev, "utf-8")
        if "static bool serveGzipPage(" in fs:  # revisions before ADR-215 have no gzipped pages
            (directory / "gzip_pages.inc").write_text(gzip_pages_inc(fs, args.mutant), encoding="ascii")
        elif args.mutant:
            raise SystemExit("--mutant needs a revision with serveGzipPage")
        legacy = "bool g_fileSlotHeld = false;\n" if "g_fileSlotHeld" in header else ""
        (directory / "file_gate.inc").write_text(legacy + gate[start:end])
        (directory / "webServerCompat.h").write_text(header)
        platform = source("src/libraries/Platform/src/platform_esp32.h", args.old_rev)
        start = platform.index("struct PlatformTcpIdleConnection {") if "struct PlatformTcpIdleConnection {" in platform else platform.index("inline uint16_t platformTcpTxHoldingOnPort(")
        # End at the next block after the TX gate: later shims (TASK-1605 resolver, TASK-1450
        # gateway probe) need IPAddress/lwIP types the fixture does not fake.
        end = min(platform.index(m, start) for m in ("// TASK-1605: resolve", "// Exception cause")
                  if m in platform[start:])
        (directory / "tcp_gate.inc").write_text(platform[start:end])
        model = source("src/OTGW-firmware/OTGW-firmware.h", args.old_rev)
        start = model.index("struct HeapDiagSection {")
        end = model.index("\n};", start) + 3
        (directory / "heap_diag.inc").write_text(model[start:end])
        helpers = source("src/OTGW-firmware/helperStuff.ino", args.old_rev)
        start = helpers.index("void resetHeapWatermark() {")
        end = helpers.index("\n}", start) + 2
        (directory / "reset_heap.inc").write_text(helpers[start:end])
        (directory / "Arduino.h").write_text("")
        (directory / "ESPAsyncWebServer.h").write_text("")
        # webServerCompat.h includes <platform.h> for platformMaxFreeBlock() since TASK-1736
        # (44d6ef686, restBeginStreamSized). The fixture defines platformMaxFreeBlock() itself
        # (web_file_scheduling.cpp), so the header only needs the include to resolve (TASK-1723).
        (directory / "platform.h").write_text("#pragma once\n")
        executable = directory / ("file-scheduling.exe" if vcvars else "file-scheduling")
        if vcvars:
            cmd = (f'call "{vcvars}" >nul && cl /nologo /std:c++17 /EHsc /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS '
                   f'/I"{directory}" /I"{ROOT / "src/OTGW-firmware"}" /Fo:"{str(directory).replace(chr(92), "/")}/" /Fe:"{executable}" '
                   f'"{ROOT / "tests/fixtures/web_file_scheduling.cpp"}"')
            built = subprocess.run(cmd, shell=True, cwd=directory, capture_output=True, text=True, errors="replace")
            if built.returncode:
                raise SystemExit("MSVC compile failed:\n" + built.stdout[-3000:] + built.stderr[-1000:])
        else:
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                            "-I", str(directory), "-I", str(ROOT / "src/OTGW-firmware"),
                            str(ROOT / "tests/fixtures/web_file_scheduling.cpp"), "-o", str(executable)], check=True)
        code = subprocess.run([str(executable)]).returncode
        if args.mutant:
            print(f"mutant {args.mutant}: " + ("KILLED (fixture failed as it must)" if code else "SURVIVED (the fixture missed it)"))
            return 0 if code else 1
        return code


if __name__ == "__main__":
    raise SystemExit(main())
