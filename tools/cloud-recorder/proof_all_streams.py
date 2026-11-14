#!/usr/bin/env python3
"""End-to-end proof for all three streams (TASK-1938).

    python3 proof_all_streams.py --image firmware.bin --work /tmp/proof [--minutes 30] [--profile no-psram|psram]

1. Builds what a gateway would print over --minutes: OT frames (codec study stream), SATTICK lines in both
   firmware formats (with nan and -0.00 cases) and debug lines from the firmware's own call sites whose
   format string and function name are found in --image. The reference text of every SATTICK and debug line
   is rendered by ref_printf.c, i.e. by the C library's snprintf per conversion, as the device does.
2. Encodes it the way the gateway would: one mix record per 60 s, sent early when it would exceed the
   profile's buffer (no-psram: 4 KB, debug about 0.3 KB/s, no compression; psram: 48 KiB, full debug, zlib).
   Debug lines carry the real flash addresses from --image.
3. Uploads the records to a fresh local PocketBase (Docker, batch_size_study.start) as one device.
4. Downloads and decodes them with decode_all.py and compares otframes.log, sat.log and debug.log with the
   references, byte for byte.
"""
import argparse, base64, datetime, math, os, random, struct, subprocess, sys, zlib
from zoneinfo import ZoneInfo

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import otrec  # noqa: E402
import esp_image  # noqa: E402
import dbg_size_study  # noqa: E402
import batch_size_study as bss  # noqa: E402
import decode_all  # noqa: E402
import export_batches as eb  # noqa: E402

T_START = 1763103600000            # 2026-11-14 08:00:00 +01:00
TZ = ZoneInfo("Europe/Amsterdam")
PREFIX = "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): "


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


class Ref:
    """Batches render jobs for ref_printf (the C library's snprintf)."""

    def __init__(self, exe):
        self.exe, self.jobs = exe, []

    def add(self, fmt, typed):
        self.jobs.append(fmt.encode().hex() + "".join(
            f"\t{t}:{(v.encode().hex() if t == 's' else repr(v) if t == 'f' else v)}" for t, v in typed))
        return len(self.jobs) - 1

    def run(self):
        r = subprocess.run([self.exe], input="\n".join(self.jobs) + "\n", capture_output=True, text=True, check=True)
        return [bytes.fromhex(x).decode() for x in r.stdout.split()]


def typed(fmt, args):
    out = []
    for conv, v in zip(otrec.conversions(fmt), args):
        if conv in "di":
            out.append(("d", int(v)))
        elif conv in "uxX":
            out.append(("u", int(v) & 0xFFFFFFFF))
        elif conv == "f":
            out.append(("f", v))
        elif conv == "c":
            out.append(("c", int(v)))
        else:
            out.append(("s", str(v)))
    return out


def gen_args(rnd, fmt):
    args = []
    for m in otrec.SPEC.finditer(fmt):
        conv, prec = m.group(5), m.group(3)
        if conv == "%":
            continue
        v = dbg_size_study.gen_arg(rnd, conv, prec)
        if conv == "f":
            v = f32(v if rnd.random() > 0.02 else rnd.choice([float("nan"), -0.0004, 0.0]))
        elif conv == "c":
            v = ord(v)
        args.append(v)
    return args


def sat_args(rnd, fi, k):
    room = f32(20.0 + 0.4 * math.sin(k / 20) + rnd.gauss(0, 0.02))
    flow = f32(35 + 10 * math.sin(k / 15))
    ret = f32(flow - 6)
    if fi == 1:
        return [f32(20.5), 1, 1, flow, ret, ord("V"), rnd.randint(0, 3000), ord("V"), rnd.randint(0, 3000), "OT"]
    p = f32(rnd.choice([-0.004, 0.31, 1.25, -2.5]))
    return [room if k % 37 else float("nan"), room, 1, f32(20.5), f32(8.0 + k / 100), ord("E"), f32(45.0), p,
            f32(0.8), f32(0.0), f32(46.2), f32(46.0), f32(46.0), "-", 1, 1, 0, f32(0.0), 0, 0, k % 5, 1,
            flow, ret, ord("V"), rnd.randint(0, 3000), ord("V"), rnd.randint(0, 3000), "OT"]


def build(image, ref_exe, minutes, profile, seed=7):
    rnd = random.Random(seed)
    end_ms = T_START + int(minutes * 60000)
    frames = [(t + T_START, L, f) for t, L, f in otrec.codec.synthetic(minutes / 60)]
    ref = Ref(ref_exe)

    sat_events, k, t = [], 0, T_START + 1000
    while t < end_ms:
        fi = 1 if k % 10 == 9 else 0
        sat_events.append((t, fi, ref.add(otrec.SAT_FORMATS[fi], typed(otrec.SAT_FORMATS[fi], sat_args(rnd, fi, k)))))
        k += 1
        t += 38000

    sites = []
    for macro, fmt, fn, line in dbg_size_study.call_sites():
        if "DebugT" not in macro:
            continue
        fa, na = image.find_cstr(fmt), image.find_cstr(fn)
        if fa and na:
            sites.append(("println" if "ln" in macro.lower() else "printf", fmt, fn, line, fa, na))
    rate = 300 if profile == "no-psram" else 3000
    dbg_events, t_us, heap, heap_sec = [], T_START * 1000 + 500, None, -1
    while t_us < end_ms * 1000:
        kind, fmt, fn, line, fa, na = rnd.choice(sites)
        sec = t_us // 1000000
        if sec != heap_sec:
            heap_sec = sec
            heap = (rnd.randint(60000, 75000), rnd.randint(28000, 32000))
            dbg_events.append((t_us, ("heap",) + heap, None))
        ts = datetime.datetime.fromtimestamp(sec, TZ)
        pj = ref.add(PREFIX, [("d", ts.hour), ("d", ts.minute), ("d", ts.second), ("d", t_us % 1000000),
                              ("u", heap[0]), ("u", heap[1]), ("s", fn), ("d", line)])
        if kind == "println":
            dbg_events.append((t_us, ("println", t_us, fa, na, line), (pj, None, fmt)))
        else:
            args = gen_args(rnd, fmt)
            dbg_events.append((t_us, ("printf", t_us, fa, na, line, args, fmt), (pj, ref.add(fmt, typed(fmt, args)), None)))
        t_us += int(rnd.expovariate(rate / 100.0) * 1e6)

    rendered = ref.run()
    sat_ref = [(t, fi, rendered[j]) for t, fi, j in sat_events]
    dbg_ref = bytearray()
    for _, item, r in dbg_events:
        if r is None:
            continue
        pj, bj, literal = r
        dbg_ref += (rendered[pj] + (rendered[bj] if bj is not None else literal + "\r\n")).encode()
    return frames, sat_ref, dbg_events, bytes(dbg_ref), len(sites)


def make_records(frames, sat_ref, dbg_events, profile):
    cap = otrec.MIX_BUFFER_NO_PSRAM if profile == "no-psram" else otrec.MAX_PAYLOAD_BYTES
    compress = profile == "psram"

    def encode(lo, hi):
        ot = [f for f in frames if lo <= f[0] < hi]
        sat = [s for s in sat_ref if lo <= s[0] < hi]
        dbg = [d[1] for d in dbg_events if lo * 1000 <= d[0] < hi * 1000]
        if not (ot or sat or any(d[0] != "heap" for d in dbg)):
            return []
        t0 = lo
        sat_b = otrec.sat_encode(t0, sat)
        dbg_b = otrec.dbg_encode(t0, dbg)
        payload = otrec.pack_mix(t0, ot, sat_b, zlib.compress(dbg_b, 6) if compress else dbg_b, compress)
        if len(payload) > cap and hi - lo > 1000:
            mid = lo + (hi - lo) // 2
            return encode(lo, mid) + encode(mid, hi)
        return [(t0, payload)]

    out, end = [], max(frames[-1][0], sat_ref[-1][0], dbg_events[-1][0] // 1000) + 1
    for lo in range(T_START, end, otrec.MIX_SECONDS * 1000):
        out += encode(lo, min(lo + otrec.MIX_SECONDS * 1000, end))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--minutes", type=float, default=30)
    ap.add_argument("--profile", choices=["no-psram", "psram"], default="no-psram")
    ap.add_argument("--docker-image", default="otgw-pb-test")
    ap.add_argument("--port", type=int, default=8092)
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    exe = os.path.join(a.work, "ref_printf")
    subprocess.run(["cc", "-O2", "-o", exe, os.path.join(HERE, "ref_printf.c")], check=True)
    image = esp_image.EspImage(a.image)

    frames, sat_ref, dbg_events, dbg_ref, n_sites = build(image, exe, a.minutes, a.profile)
    records = make_records(frames, sat_ref, dbg_events, a.profile)
    sizes = [len(p) for _, p in records]
    print(f"profile {a.profile}: {len(frames)} OT frames, {len(sat_ref)} SAT ticks, "
          f"{sum(1 for e in dbg_events if e[2])} debug lines from {n_sites} call sites found in the image, "
          f"{len(dbg_ref)} B of debug text")
    print(f"{len(records)} mix records in {a.minutes:g} min, largest {max(sizes)} B, total {sum(sizes)} B "
          f"(text it replaces: {len(dbg_ref) + sum(len(s[2]) for s in sat_ref)} B debug + SAT)")

    data_dir = os.path.join(a.work, "pb_data")
    subprocess.run(["rm", "-rf", data_dir])
    os.makedirs(data_dir)
    dev, dev_id = bss.start(a.docker_image, data_dir, a.port)
    for seq, (t0, payload) in enumerate(records):
        st, r = dev.call("POST", otrec.CREATE_PATH, {"device": dev_id, "stream": "mix", "seq": seq, "t0": t0,
                                                     "fmt": otrec.FMT_MIX_V1,
                                                     "payload": base64.b64encode(payload).decode()})
        if st != 200:
            sys.exit(f"record {seq}: HTTP {st} {r}")
    base = f"http://127.0.0.1:{a.port}"
    token = eb.api(base, "/api/collections/_superusers/auth-with-password", None,
                   {"identity": bss.ADMIN, "password": bss.ADMIN_PW})["token"]
    got = eb.fetch_batches(base, token, dev_id, "mix")
    bss.docker("rm", "-f", "pbss", check=False)
    out = os.path.join(a.work, "decoded")
    c = decode_all.decode_records(got, image, TZ, out)
    print(f"decoded: {c}")

    ot_out = [(round(datetime.datetime.fromisoformat(l.split("\t")[0]).timestamp() * 1000), l.split("\t")[2],
               int(l.rstrip("\n").split("\t")[3], 16)) for l in open(os.path.join(out, "otframes.log")) if not l.startswith("#")]
    sat_out = [l.rstrip("\n") for l in open(os.path.join(out, "sat.log")) if not l.startswith("#")]
    sat_want = [f"{datetime.datetime.fromtimestamp(t / 1000, TZ).isoformat(timespec='milliseconds')}\t"
                f"{text.rstrip(chr(13) + chr(10))}" for t, _, text in sat_ref]
    dbg_out = open(os.path.join(out, "debug.log"), "rb").read()
    ok = {"OT frames identical": ot_out == frames, "SATTICK lines identical": sat_out == sat_want,
          "debug telnet bytes identical": dbg_out == dbg_ref}
    for k, v in ok.items():
        print(f"{'PASS' if v else 'FAIL'}  {k}")
    if not ok["debug telnet bytes identical"]:
        i = next((j for j in range(min(len(dbg_out), len(dbg_ref))) if dbg_out[j] != dbg_ref[j]), None)
        print("first difference at byte", i, repr(dbg_ref[max(0, (i or 0) - 60):(i or 0) + 40]), "vs",
              repr(dbg_out[max(0, (i or 0) - 60):(i or 0) + 40]))
    if not ok["SATTICK lines identical"]:
        i = next((j for j in range(min(len(sat_out), len(sat_want))) if sat_out[j] != sat_want[j]), None)
        print("first SAT difference", i, repr(sat_want[i] if i is not None else None), repr(sat_out[i] if i is not None else None))
    sys.exit(0 if all(ok.values()) else 1)


if __name__ == "__main__":
    main()
