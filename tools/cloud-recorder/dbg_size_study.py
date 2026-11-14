#!/usr/bin/env python3
"""Sizes the debug stream in three forms (TASK-1938, research for TASK-1862).

    python3 dbg_size_study.py [--rate 3000] [--seconds 3600] [--batch 60]

Lines are built from the firmware's real format strings (every Debug* call with a literal format in
src/OTGW-firmware, except the help menu in handleDebug.ino) and the real _debugBOL prefix
"HH:MM:SS.uuuuuu (heap|block) function(line): ". Argument values are generated: the call-site mix and the
values are a guess, so the absolute volume is an estimate; the bytes-per-line ratios are the result.

Forms per line:
  text      the telnet bytes
  token     address form: varint time delta (ms), the function name's and the format string's flash
            offsets (3 bytes each, identical strings can share an address under LTO), the line number
            (varint), then the arguments in binary: ints zigzag varint, unsigned/hex varint, float 4 B,
            char 1 B, string length byte + bytes (max 127); heap/block once per second as a 7 B record
  +zlib     each form compressed per batch (zlib level 6, one stream per batch)
"""
import argparse, glob, os, random, re, struct, zlib

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "OTGW-firmware")
CALL = re.compile(r'\b(\w*Debug\w*?)\s*\(\s*(?:PSTR|F)?\(?\s*"((?:[^"\\]|\\.)*)"')
SPEC = re.compile(r'%([-+ #0]*)(\d*)(?:\.(\d+))?(hh|h|ll|l|z)?([diuxXfscS%])')
WORDS = ["boiler_temp", "return_temp", "control_setpoint", "flame", "ch_enable", "dhw_enable", "ON", "OFF", "true",
         "false", "idle", "heating", "otgw-firmware/value/otgw/boiler_temp", "homeassistant/sensor/otgw/flame/config",
         "192.168.1.42", "SAT", "PWM", "modulating", "OpenTherm", "Write-Ack", "Read-Data", "MsgID", "wifi", "mqtt",
         "connected", "reconnect", "PIC", "OTDirect", "settings.ini", "Europe/Amsterdam", "low_load", "room"]


def call_sites():
    sites = []
    for path in sorted(glob.glob(os.path.join(SRC, "*.ino")) + glob.glob(os.path.join(SRC, "*.h"))):
        if os.path.basename(path) == "handleDebug.ino":
            continue
        text = open(path, errors="replace").read()
        for m in CALL.finditer(text):
            macro, fmt = m.group(1), bytes(m.group(2), "utf-8").decode("unicode_escape", errors="replace")
            line = text.count("\n", 0, m.start()) + 1
            fn = re.findall(r"\n\w[\w\s\*&:<>,]*?\b(\w+)\s*\([^;{]*\)\s*\{", text[:m.start()])
            sites.append((macro, fmt, fn[-1] if fn else "setup", line))
    return sites


def gen_arg(rnd, conv, prec):
    if conv in "di":
        return rnd.choice([rnd.randint(-5, 120), rnd.randint(-2000, 5000), rnd.randint(0, 3)])
    if conv == "u":
        return rnd.choice([rnd.randint(0, 255), rnd.randint(20000, 90000), rnd.randint(0, 2_000_000)])
    if conv in "xX":
        return rnd.choice([rnd.getrandbits(32), rnd.randint(0, 255), rnd.getrandbits(16)])
    if conv == "f":
        return round(rnd.uniform(-5, 75), 3)
    if conv == "c":
        return rnd.choice("TBRAE")
    return rnd.choice(WORDS)


def zz(n):
    return (n << 1) ^ (n >> 63) if n < 0 else n << 1


def varint(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def render(rnd, fmt):
    """Returns (text, binary args) for one call of fmt."""
    text, binary, pos = [], bytearray(), 0
    for m in SPEC.finditer(fmt):
        text.append(fmt[pos:m.start()])
        pos = m.end()
        flags, width, prec, length, conv = m.groups()
        if conv == "%":
            text.append("%")
            continue
        v = gen_arg(rnd, conv, prec)
        pyconv = "s" if conv == "S" else conv
        text.append(("%" + flags + width + (("." + prec) if prec else "") + pyconv) % v)
        if conv in "di":
            binary += varint(zz(v))
        elif conv in "uxX":
            binary += varint(v & 0xFFFFFFFF)
        elif conv == "f":
            binary += struct.pack("<f", v)
        elif conv == "c":
            binary += v.encode()
        else:
            s = str(v).encode()[:127]
            binary += bytes([len(s)]) + s
    text.append(fmt[pos:])
    return "".join(text), bytes(binary)


def generate(rate=3000, seconds=3600, batch=60, seed=1):
    """Returns (batches, lines, call-site counts). A batch is (t0_ms, text bytes, token bytes)."""
    rnd = random.Random(seed)
    sites = call_sites()
    bol = [s for s in sites if re.search(r"DebugT", s[0])]
    t_ms, last_heap_s, lines = 0, -1, 0
    tb, kb = bytearray(), bytearray()
    batches = []
    batch_start = 0
    while t_ms < seconds * 1000:
        macro, fmt, fn, line = rnd.choice(bol)
        body, args = render(rnd, fmt)
        sec = t_ms // 1000
        prefix = "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): " % (
            8 + sec // 3600 % 24, sec // 60 % 60, sec % 60, (t_ms % 1000) * 1000 + rnd.randint(0, 999),
            rnd.randint(60000, 75000), rnd.randint(28000, 32000), fn, line)
        text = (prefix + body + ("\r\n" if "ln" in macro.lower() else "")).encode()
        if sec != last_heap_s:
            kb += b"\x00" + varint(rnd.randint(60000, 75000)) + varint(rnd.randint(28000, 32000))
            last_heap_s = sec
        dt = rnd.randint(0, 40) if lines else 0
        kb += (varint(dt) + (rnd.getrandbits(20) | 0x100000).to_bytes(3, "little")
               + (rnd.getrandbits(20) | 0x100000).to_bytes(3, "little") + varint(line) + args)
        tb += text
        lines += 1
        t_ms += max(1, int(len(text) / rate * 1000 * rnd.uniform(0.2, 1.8)))
        if t_ms - batch_start >= batch * 1000:
            batches.append((batch_start, bytes(tb), bytes(kb)))
            tb, kb, batch_start = bytearray(), bytearray(), t_ms
    if tb:
        batches.append((batch_start, bytes(tb), bytes(kb)))
    return batches, lines, (len(sites), len(bol))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rate", type=float, default=3000, help="text bytes per second to generate")
    ap.add_argument("--seconds", type=int, default=3600)
    ap.add_argument("--batch", type=int, default=60)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    batches, lines, (n_sites, n_bol) = generate(a.rate, a.seconds, a.batch, a.seed)
    print(f"call sites with a literal format: {n_sites} (with the BOL prefix: {n_bol})")
    sums = {"text": 0, "token": 0, "text_z": 0, "token_z": 0}
    for _, tbytes, kbytes in batches:
        sums["text"] += len(tbytes)
        sums["token"] += len(kbytes)
        sums["text_z"] += len(zlib.compress(tbytes, 6))
        sums["token_z"] += len(zlib.compress(kbytes, 6))
    secs = a.seconds
    print(f"{lines} lines in {secs} s ({lines / secs:.1f} lines/s), batches of {a.batch} s: {len(batches)}")
    per_batch_max = max(len(zlib.compress(k, 6)) for _, _, k in batches)
    for k, label in (("text", "text (telnet bytes)"), ("text_z", "text + zlib"),
                     ("token", "token (address form)"), ("token_z", "token + zlib")):
        v = sums[k]
        print(f"  {label:22s} {v / lines:6.1f} B/line {v / secs:8.1f} B/s {v / secs * 86400 / 1e6:7.1f} MB/day "
              f"{v / secs * 86400 * 30 / 1e9:6.2f} GB/month  ({100 * v / sums['text']:.0f} % of text)")
    print(f"  largest token+zlib batch: {per_batch_max} B binary, {4 * ((per_batch_max + 2) // 3)} chars base64")


if __name__ == "__main__":
    main()
