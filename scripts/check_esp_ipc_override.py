#!/usr/bin/env python3
"""TASK-1870 / ADR-225: prove on a built ELF that our esp_ipc_stack.c replaced the prebuilt IPC code.

src/OTGW-firmware/esp_ipc_stack.c is a copy of IDF esp_ipc.c with a 1280 B IPC task stack. The
linker should take it instead of libesp_system.a(esp_ipc.c.obj). This script checks the result in
the final ELF (and in the linker map when one is given), so the build is proven before any flash:

  1. esp_ipc_init creates the IPC tasks with 1280 B: the stack argument (a12) loaded right before
     the call to xTaskCreatePinnedToCore is 0x500, not the prebuilt 0x400.
  2. esp_ipc_init is registered as a constructor: its address is in __init_array.
  3. ipc_task and every literal it loads (l32r) are in IRAM: the IPC task runs while the flash
     cache is off during a flash write, so code or literals in flash would crash there.
  4. The IPC data (s_ipc_task_handle, s_ipc_mutex_buffer, s_ipc_ack_buffer) is in internal DRAM.
  5. Each IPC function is defined once.
  6. With --map: the map does not pull esp_ipc.c.obj out of libesp_system.a.

Exit 0 when every check passes, 1 otherwise. A beta.217 ELF (prebuilt IPC) fails check 1.

Run: python scripts/check_esp_ipc_override.py --elf <firmware.elf> [--map <firmware.map>]
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

IRAM = (0x40370000, 0x403E0000)   # ESP32-S3 internal SRAM, instruction bus
DRAM = (0x3FC88000, 0x3FD00000)   # ESP32-S3 internal SRAM, data bus
WANT_STACK = 0x500                # 1280 B, ADR-225
PREBUILT_STACK = 0x400


def toolchain_bin():
    roots = []
    if os.environ.get("PLATFORMIO_CORE_DIR"):
        roots.append(Path(os.environ["PLATFORMIO_CORE_DIR"]))
    roots += [Path("D:/DevData/platformio"), Path.home() / ".platformio"]
    for r in roots:
        b = r / "packages" / "toolchain-xtensa-esp-elf" / "bin"
        if b.is_dir():
            return b
    sys.exit("toolchain-xtensa-esp-elf not found (set PLATFORMIO_CORE_DIR)")


def tool(b, name):
    exe = b / ("xtensa-esp32s3-elf-" + name + (".exe" if os.name == "nt" else ""))
    return str(exe)


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def symbols(b, elf):
    out = {}
    for line in run([tool(b, "nm"), elf]).splitlines():
        parts = line.split()
        if len(parts) == 3:
            addr, kind, name = parts
            base = name.split(".")[0]   # LTO may rename a static to name.lto_priv.0
            out.setdefault(base, []).append((int(addr, 16), kind, name))
    return out


def in_range(a, rng):
    return rng[0] <= a < rng[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", required=True)
    ap.add_argument("--map")
    a = ap.parse_args()
    b = toolchain_bin()
    syms = symbols(b, a.elf)
    fails, notes = [], []

    def one(name):
        v = syms.get(name, [])
        if len(v) != 1:
            fails.append(f"{name}: expected one definition, found {len(v)}")
            return None
        return v[0]

    # --gc-sections drops an unreferenced API function (beta.217 has no esp_ipc_call): at most one.
    for fn in ("esp_ipc_call", "esp_ipc_call_blocking", "esp_ipc_call_nonblocking"):
        if len(syms.get(fn, [])) > 1:
            fails.append(f"{fn}: defined {len(syms[fn])} times")

    init = one("esp_ipc_init")
    if init:
        dis = run([tool(b, "objdump"), "-d", "--disassemble=" + init[2], a.elf])
        stack = None
        for line in dis.splitlines():
            m = re.search(r"\bmovi(?:\.n)?\s+a12,\s*(-?0x[0-9a-f]+|-?\d+)", line)
            if m:
                stack = int(m.group(1), 0)
            if "xTaskCreatePinnedToCore" in line and stack is not None:
                break
        notes.append(f"esp_ipc_init stack argument: {stack!r} ({hex(stack) if stack is not None else '-'})")
        if stack != WANT_STACK:
            fails.append(f"IPC stack is {stack} B, want {WANT_STACK} B"
                         + (" (prebuilt esp_ipc.c.obj linked)" if stack == PREBUILT_STACK else ""))
        lo, hi = syms.get("__init_array_start"), syms.get("__init_array_end")
        if lo and hi:
            dump = run([tool(b, "objdump"), "-s", f"--start-address={lo[0][0]:#x}",
                        f"--stop-address={hi[0][0]:#x}", a.elf])
            words = []
            for line in dump.splitlines():
                m = re.match(r"\s*[0-9a-f]{8}\s+((?:[0-9a-f]{8}\s?){1,4})", line)
                if m:
                    for w in m.group(1).split():
                        words.append(int.from_bytes(bytes.fromhex(w), "little"))
            if init[0] not in words:
                fails.append("esp_ipc_init is not in __init_array: the IPC tasks would never start")
            else:
                notes.append(f"esp_ipc_init {init[0]:#x} found in __init_array ({len(words)} entries)")
        else:
            fails.append("__init_array_start/end not found")

    task = one("ipc_task")
    if task:
        if not in_range(task[0], IRAM):
            fails.append(f"ipc_task at {task[0]:#x} is not in IRAM")
        dis = run([tool(b, "objdump"), "-d", "--disassemble=" + task[2], a.elf])
        lits = [int(x, 16) for x in re.findall(r"\bl32r\s+a\d+,\s*([0-9a-f]+)", dis)]
        bad = [hex(x) for x in lits if not in_range(x, IRAM)]
        notes.append(f"ipc_task {task[0]:#x}, {len(lits)} literal loads, outside IRAM: {bad or 'none'}")
        if bad or not lits:
            fails.append(f"ipc_task literals outside IRAM: {bad}" if bad else "ipc_task: no literal loads found")

    for d in ("s_ipc_task_handle", "s_ipc_mutex_buffer", "s_ipc_ack_buffer"):
        v = one(d)
        if v and not in_range(v[0], DRAM):
            fails.append(f"{d} at {v[0]:#x} is not in internal DRAM")

    if a.map:
        text = Path(a.map).read_text(encoding="utf-8", errors="replace")
        if re.search(r"libesp_system\.a\(esp_ipc\.c\.obj\)", text):
            fails.append("map: esp_ipc.c.obj is still pulled from libesp_system.a")
        else:
            notes.append("map: no libesp_system.a(esp_ipc.c.obj)")
        if "esp_ipc_stack.c" not in text:
            fails.append("map: esp_ipc_stack.c object not listed")

    for n in notes:
        print("  " + n)
    if fails:
        for f in fails:
            print("FAIL " + f)
        return 1
    print("PASS: IPC tasks come from esp_ipc_stack.c with a 1280 B stack, code and literals in IRAM")
    return 0


if __name__ == "__main__":
    sys.exit(main())
