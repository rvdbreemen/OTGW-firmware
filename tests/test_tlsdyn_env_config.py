"""TASK-2901: the esp32-combo-tlsdyn env builds on the stock Arduino libraries; mbedTLS gets a PSRAM allocator.

poc.10 rebuilt the IDF libraries with dynamic mbedTLS buffers; on .63 freeing a dynamic RX buffer after the HTTP
response hit a heap poisoning assert (core dump 2026-11-19). From poc.12 the env has no custom_sdkconfig (so no
hybrid compile and no dynamic buffers), the link carries no --wrap flags, setup() installs the mbedTLS allocator
right after the RTC captures, the allocator puts blocks in PSRAM with an internal fallback, and the
remote telemetry upload starts only on a board with PSRAM.
poc.21 (09, .63, BLE on, 2026-11-19 20:18-20:29: poc.20 starved the heap to ~22 KB free / 10.7 KB block): every
mbedTLS block goes to PSRAM first (REMOTE_TELEMETRY_TLS_PSRAM_MIN = 0, poc.20 had 4096) and remoteTelemetryConnect()
skips the attempt below REMOTE_TELEMETRY_MIN_FREE / _MIN_BLOCK. Red on 2896c89e0 (2 errors), green on the fix.
"""
import configparser
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = ["platformio.ini", "src/OTGW-firmware/OTGW-firmware.ino", "src/OTGW-firmware/RemoteTelemetryDevice.h"]


def env_section(text):
    cp = configparser.ConfigParser(interpolation=None, strict=False, inline_comment_prefixes=(";",))
    cp.read_string(text)
    return cp["env:esp32-combo-tlsdyn"]


def body(src, signature):
    start = src.index(signature)
    return src[start:src.index("\n}\n", start)]


def check(ini, main_ino, device):
    errors = []
    env = env_section(ini)
    for key in ("custom_sdkconfig", "custom_component_remove"):
        if key in env:
            errors.append(f"{key} in the env starts the hybrid IDF rebuild")
    if "--wrap" in "\n".join(env.values()) or re.search(r"tlsdyn_mbedtls_libs", env.get("extra_scripts", "")):
        errors.append("the env still adds dynamic-buffer link wraps / the tlsdyn libs script")
    setup = body(main_ino, "void setup() {")
    # After the three RTC captures (they must stay first, tests/test_wifi_deadlink.py), before anything that can
    # touch mbedTLS: the GPIO/ISR setup, WiFi, OT, settings.
    first = [l.strip() for l in setup.splitlines()[1:] if l.strip() and not l.strip().startswith("//")]
    want = ["captureRebootReason();", "captureWifiDeadRestarts();", "captureWifiSkipPortal();", "#if HAS_REMOTE_TELEMETRY",
            "remoteTelemetryInstallTlsAllocator();"]
    if len(first) < 5 or any(not f.startswith(w) for f, w in zip(first, want)):
        errors.append("setup() does not install the mbedTLS allocator right after the RTC captures")
    if "mbedtls_platform_set_calloc_free(remoteTelemetryTlsCalloc, remoteTelemetryTlsFree)" not in device:
        errors.append("remoteTelemetryInstallTlsAllocator() does not install remoteTelemetryTlsCalloc/remoteTelemetryTlsFree")
    else:
        alloc = body(device, "static void *remoteTelemetryTlsCalloc(")
        if not re.search(r"total > REMOTE_TELEMETRY_TLS_PSRAM_MIN\)[\s\S]*MALLOC_CAP_SPIRAM[\s\S]*MALLOC_CAP_INTERNAL", alloc):
            errors.append("the allocator lacks the PSRAM branch with an internal fallback")
        # poc.21: every block to PSRAM first (poc.20 kept blocks up to 4 KB internal and starved .63 with BLE on)
        if not re.search(r"REMOTE_TELEMETRY_TLS_PSRAM_MIN\s*=\s*0\s*;", device):
            errors.append("the allocator keeps small mbedTLS blocks in internal RAM (REMOTE_TELEMETRY_TLS_PSRAM_MIN != 0)")
        if "__builtin_mul_overflow" not in alloc:
            errors.append("the allocator does not check n * size for overflow")
    # poc.21: no connection attempt on a low internal heap, checked before esp-tls is touched
    conn = body(device, "static bool remoteTelemetryConnect(")
    guard = re.search(r"REMOTE_TELEMETRY_MIN_FREE[\s\S]*?REMOTE_TELEMETRY_MIN_BLOCK[\s\S]*?return false;", conn)
    if not guard or conn.find("c.connect(") < guard.end():
        errors.append("remoteTelemetryConnect() connects without the low-heap check first")
    setup_rec = body(device, "void remoteTelemetrySetup()")
    if setup_rec.find("platformHasPsram()") < 0 or setup_rec.find("platformHasPsram()") > setup_rec.find("remoteTelemetryInitCore()"):
        errors.append("remoteTelemetrySetup() starts the upload without checking platformHasPsram() first")
    return errors


def read(p):
    return (ROOT / p).read_text(encoding="utf-8")


def test_tlsdyn_env_config():
    assert check(*(read(f) for f in FILES)) == []


if __name__ == "__main__":
    # python tests/test_tlsdyn_env_config.py <platformio.ini> <OTGW-firmware.ino> <RemoteTelemetryDevice.h>: red runs.
    args = sys.argv[1:] or FILES
    errs = check(*(Path(f).read_text(encoding="utf-8") if Path(f).exists() else read(f) for f in args))
    print("\n".join(errs) or "OK")
    sys.exit(1 if errs else 0)
