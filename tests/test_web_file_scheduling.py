#!/usr/bin/env python3
"""Compile the real webSendFile path and file gate behind bounded platform fakes.

Run: python3 tests/test_web_file_scheduling.py
--old-rev <revision> builds the same fixture against an older header/gate. Revisions
before the queue fail to compile against it (the fixture uses the queue counters), so
that run shows the queue is missing, not a behavioural failure of the old code.
Only Arduino, LittleFS and ESPAsyncWebServer's transport boundary are faked.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path, rev):
    if rev:
        return subprocess.check_output(["git", "show", f"{rev}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old-rev")
    args = parser.parse_args()
    compiler = shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("A host C++ compiler is required")
    with tempfile.TemporaryDirectory(prefix="otgw-file-scheduling-") as tmp:
        directory = Path(tmp)
        gate = source("src/OTGW-firmware/restAPI.ino", args.old_rev)
        # Compile the complete production gate block, not a copied implementation.
        start = gate.index("#ifndef WEB_FILE_MAX_INFLIGHT")
        end = gate.index("// Zero-allocation HTTP method", start)
        header = source("src/OTGW-firmware/webServerCompat.h", args.old_rev)
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
        executable = directory / "file-scheduling"
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                        "-I", str(directory), "-I", str(ROOT / "src/OTGW-firmware"),
                        str(ROOT / "tests/fixtures/web_file_scheduling.cpp"), "-o", str(executable)], check=True)
        return subprocess.run([str(executable)]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
