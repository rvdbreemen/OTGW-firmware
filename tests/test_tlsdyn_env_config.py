"""TASK-1963: the esp32-combo-tlsdyn env keeps mbedTLS on fixed record buffers allocated through malloc().

poc.10 used dynamic mbedTLS buffers; on .63 freeing a dynamic RX buffer after the HTTP response hit a heap
poisoning assert (core dump 2026-11-19). The env must not turn them back on, must route mbedTLS through
malloc() (so blocks above 4 KB land in PSRAM) instead of internal-only or external-only RAM, and the link must
not carry IDF's dynamic-buffer --wrap flags. The upload itself only starts on a board with PSRAM.
"""
import configparser
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def tlsdyn_sdkconfig(text):
    cp = configparser.ConfigParser(interpolation=None, strict=False, inline_comment_prefixes=(";",))
    cp.read_string(text)
    lines = cp["env:esp32-combo-tlsdyn"]["custom_sdkconfig"].split("\n")
    return {k: v for k, v in (l.strip().split("=", 1) for l in lines if "=" in l)}


def check(ini, script, device):
    errors = []
    cfg = tlsdyn_sdkconfig(ini)
    if cfg.get("CONFIG_MBEDTLS_DYNAMIC_BUFFER") == "y":
        errors.append("CONFIG_MBEDTLS_DYNAMIC_BUFFER is on")
    if cfg.get("CONFIG_MBEDTLS_DEFAULT_MEM_ALLOC") != "y":
        errors.append("CONFIG_MBEDTLS_DEFAULT_MEM_ALLOC is not y")
    if cfg.get("CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC") == "y":
        errors.append("CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC breaks WiFi crypto on a board without PSRAM")
    if cfg.get("CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS") != "y":
        errors.append("session tickets dropped")
    if re.search(r"--wrap=", script):
        errors.append("the tlsdyn script still adds --wrap link flags")
    setup = device[device.index("void cloudRecSetup()"):]
    setup = setup[:setup.index("\n}\n")]
    if setup.find("platformHasPsram()") < 0 or setup.find("platformHasPsram()") > setup.find("cloudRecInitCore()"):
        errors.append("cloudRecSetup() starts the upload without checking platformHasPsram() first")
    return errors


def read(p):
    return (ROOT / p).read_text(encoding="utf-8")


def test_tlsdyn_env_config():
    assert check(read("platformio.ini"), read("scripts/tlsdyn_mbedtls_libs.py"),
                 read("src/OTGW-firmware/CloudRecDevice.h")) == []


if __name__ == "__main__":
    # python tests/test_tlsdyn_env_config.py <ini> <script> <CloudRecDevice.h>: check other copies (red run).
    files = sys.argv[1:] or ["platformio.ini", "scripts/tlsdyn_mbedtls_libs.py", "src/OTGW-firmware/CloudRecDevice.h"]
    errs = check(*(Path(f).read_text(encoding="utf-8") if Path(f).is_absolute() or Path(f).exists() else read(f)
                   for f in files))
    print("\n".join(errs) or "OK")
    sys.exit(1 if errs else 0)
