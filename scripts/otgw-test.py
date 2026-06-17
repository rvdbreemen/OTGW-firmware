#!/usr/bin/env python3
"""OTGW32 build -> flash -> capture -> test -> analyse workflow (autonomous loops).

Orchestrates a full hardware test cycle so it can run unattended:

  1. BUILD     optional `python build.py --target esp32`
  2. FLASH     app + filesystem at their offsets WITHOUT erasing NVS, so the
               device keeps its WiFi credentials (no captive portal). esptool over
               the USB-Serial-JTAG port.
  3. MONITOR   start the capture scripts in parallel. They ONLY monitor (passive):
                 - capture-mqtt-debug.bat : telnet (the firmware debug channel) +
                   MQTT + browser + curl probes -> transcript-<...>.txt
                 - capture-serial.py      : USB serial (best-effort; the IDF console
                   is on UART0 for this firmware, so it is usually empty) -> serial-<...>.txt
  4. TEST      run the ACTIVE test scripts (scripts/tests/*.py). These drive the
               device via the REST API / MQTT and assert behaviour. Kept SEPARATE
               from the capture scripts on purpose.
  5. ANALYSE   collect the transcript paths + test results for review.

Secrets (MQTT password, etc.) come from the out-of-repo capture-settings.json via
scripts/_secrets.py (run `capture-mqtt-debug.bat -SaveSecrets` once to store the
password). Nothing secret is written into the repo.

Usage:
  python scripts/otgw-test.py --minutes 5 --build
  python scripts/otgw-test.py --no-flash --minutes 2 --tests webserver
"""
import argparse
import glob
import os
import subprocess
import sys
import time
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import _secrets  # noqa: E402

PIO = os.path.join(ROOT, ".pio", "build", "esp32")
BOOT_APP0 = os.path.join(
    os.path.expanduser("~"), ".platformio", "packages",
    "framework-arduinoespressif32", "tools", "partitions", "boot_app0.bin")
# offsets confirmed from the build merge command (otgw32 env)
FLASH_MAP = [
    ("0x0", "bootloader.bin"),
    ("0x8000", "partitions.bin"),
    ("0xe000", None),            # boot_app0 (packages path)
    ("0x10000", "firmware.bin"),
    ("0x1f0000", "littlefs.bin"),
]


def log(msg):
    print(f"[otgw-test] {msg}", flush=True)


def run(cmd, **kw):
    log("$ " + (cmd if isinstance(cmd, str) else " ".join(cmd)))
    return subprocess.run(cmd, **kw)


def do_build():
    log("BUILD: python build.py --target esp32")
    r = run([sys.executable, "build.py", "--target", "esp32"], cwd=ROOT)
    if r.returncode != 0:
        raise SystemExit("build failed")
    # build.py wrapper can mask per-env failure; require the app binary exists+fresh
    if not os.path.isfile(os.path.join(PIO, "firmware.bin")):
        raise SystemExit("build: firmware.bin missing")


def do_flash(port):
    """Flash app+fs+bootloader WITHOUT erase, preserving NVS (WiFi creds)."""
    args = [sys.executable, "-m", "esptool", "--port", port, "--chip", "esp32s3",
            "--before", "default_reset", "--after", "hard_reset", "write_flash"]
    for off, fname in FLASH_MAP:
        path = BOOT_APP0 if fname is None else os.path.join(PIO, fname)
        if not os.path.isfile(path):
            raise SystemExit(f"flash: missing {path}")
        args += [off, path]
    log("FLASH: no-erase app+fs+bootloader (NVS/creds preserved)")
    r = run(args, cwd=ROOT)
    if r.returncode != 0:
        raise SystemExit("flash failed")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=5.0, help="capture/test window (max 15)")
    ap.add_argument("--build", action="store_true", help="run python build.py first")
    ap.add_argument("--no-flash", action="store_true", help="skip flashing (test current firmware)")
    ap.add_argument("--port", default=_secrets.get("com_port", "COM4"))
    ap.add_argument("--skip-serial", action="store_true", help="skip the (usually empty) USB serial monitor")
    ap.add_argument("--tests", default="webserver", help="comma list of scripts/tests/test_<name>.py to run")
    args = ap.parse_args()
    seconds = int(min(max(args.minutes, 0.5), 15) * 60)

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = os.path.join(ROOT, "logs", "otgw-test", stamp)
    os.makedirs(run_dir, exist_ok=True)
    log(f"run dir: {run_dir}  window: {seconds}s")

    if args.build:
        do_build()
    if not args.no_flash:
        do_flash(args.port)
        log("waiting 8s for the device to boot + rejoin WiFi...")
        time.sleep(8)

    # --- start passive monitors in parallel (non-blocking) ---
    host = _secrets.get("device_host", "OTGW.local")
    broker = _secrets.get("broker_host", "homeassistant.local")
    bport = str(_secrets.get("broker_port", "1883"))
    user = _secrets.get("mqtt_user", "")
    monitors = []
    bat = os.path.join(HERE, "capture-mqtt-debug.bat")
    net_cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", bat,
               "-DeviceHost", host, "-BrokerHost", broker, "-BrokerPort", bport,
               "-OutputRoot", run_dir, "-DurationSeconds", str(seconds)]
    if user:
        net_cmd += ["-Username", user]
    log("MONITOR: network (telnet/mqtt/http/curl)")
    monitors.append(subprocess.Popen(net_cmd, cwd=ROOT, stdin=subprocess.DEVNULL))
    if not args.skip_serial:
        ser_cmd = [sys.executable, os.path.join(HERE, "capture-serial.py"),
                   "--port", args.port, "--seconds", str(seconds),
                   "--hardware", "otgw32", "--outdir", os.path.join(run_dir, "serial")]
        log("MONITOR: serial (best-effort; usually empty on this firmware)")
        monitors.append(subprocess.Popen(ser_cmd, cwd=ROOT, stdin=subprocess.DEVNULL))

    time.sleep(3)  # let the monitors attach before the active tests poke the device

    # --- run ACTIVE tests (separate scripts; drive via API/MQTT) ---
    results = {}
    for name in [t.strip() for t in args.tests.split(",") if t.strip()]:
        script = os.path.join(HERE, "tests", f"test_{name}.py")
        if not os.path.isfile(script):
            log(f"TEST {name}: SKIP (no {script})")
            continue
        log(f"TEST {name}: running")
        out = os.path.join(run_dir, f"test-{name}.log")
        with open(out, "w", encoding="utf-8") as fh:
            r = subprocess.run([sys.executable, script, "--minutes", str(args.minutes)],
                               cwd=ROOT, stdout=fh, stderr=subprocess.STDOUT)
        results[name] = r.returncode
        log(f"TEST {name}: exit {r.returncode} -> {out}")

    # --- wait for the monitors to finish their window ---
    log("waiting for monitors to finish their window...")
    for p in monitors:
        try:
            p.wait(timeout=seconds + 120)
        except Exception:
            p.terminate()

    # --- analyse: list the transcripts for review ---
    log("ARTIFACTS:")
    for f in sorted(glob.glob(os.path.join(run_dir, "**", "*.txt"), recursive=True)
                    + glob.glob(os.path.join(run_dir, "**", "transcript-*.txt"), recursive=True)):
        log(f"  {os.path.relpath(f, ROOT)}")
    log(f"test results: {results}")
    log("done")


if __name__ == "__main__":
    main()
