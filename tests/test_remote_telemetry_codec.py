#!/usr/bin/env python3
"""TASK-2901: the firmware's remote telemetry recorder encoders (src/OTGW-firmware/RemoteTelemetryCodec.h) against the reference
decoders and encoders in tools/remote-telemetry/otrec.py.

The real header is compiled with the host C++ compiler into tests/remote_telemetry_harness.cpp; nothing is
re-implemented here. Skipped when no C++ compiler is found.

  1. One record: the C sections are byte-identical to otrec's encoders (OT model v2, SAT, debug).
  2. A 30-minute run with realistic rates: the recorder closes records when a section is full; decoding all
     records gives back every OT frame with its ms time, every SATTICK line and the exact debug text.
  3. Lines that cannot go as tokens (a string over 127 bytes, a double that is not a float32, an unknown
     conversion) come back exactly through the line raw fallback.
"""
import datetime, os, random, shutil, struct, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools", "remote-telemetry"))
import otrec  # noqa: E402
import dbg_size_study  # noqa: E402

UTC = datetime.timezone.utc
T0 = 1763103600000


class FakeImage:
    """Stands in for esp_image.EspImage: strings at made-up DROM addresses."""

    def __init__(self):
        self.addr, self.text, self.next = {}, {}, otrec.DROM_BASE + 0x100000

    def add(self, s):
        if s not in self.addr:
            self.addr[s] = self.next
            self.text[self.next] = s
            self.next += len(s.encode()) + 1
        return self.addr[s]

    def read_cstr(self, a):
        return self.text[a]


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def typed_args(fmt, args):
    out = []
    for conv, v in zip(otrec.conversions(fmt), args):
        if conv in "di":
            out.append(f"d:{int(v)}")
        elif conv in "uxX":
            out.append(f"u:{int(v) & 0xFFFFFFFF}")
        elif conv == "f":
            out.append(f"f:{v!r}")
        elif conv == "c":
            out.append(f"c:{int(v)}")
        else:
            out.append("s:" + str(v).encode().hex())
    return out


def prefix(t_us, heap, fn, line):
    ts = datetime.datetime.fromtimestamp(t_us // 1000000, UTC)
    return "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): " % (
        ts.hour, ts.minute, ts.second, t_us % 1000000, heap[0], heap[1], fn, line)


class Harness:
    exe = None

    @classmethod
    def build(cls):
        cxx = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
        if not cxx:
            return None
        d = tempfile.mkdtemp(prefix="telemetry-")
        exe = os.path.join(d, "harness")
        subprocess.run([cxx, "-std=c++17", "-O1", "-Wall", "-Wextra", "-o", exe,
                        os.path.join(ROOT, "tests", "remote_telemetry_harness.cpp")], check=True)
        return exe

    @classmethod
    def run(cls, lines):
        r = subprocess.run([cls.exe], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        if r.stderr:
            raise AssertionError(r.stderr)
        recs = []
        for l in r.stdout.splitlines():
            _, t0, hx = l.split(" ")
            recs.append((int(t0), bytes.fromhex(hx)))
        return recs


def build_events(minutes, dbg_rate, seed=3):
    """Returns (harness lines, OT frames, SAT ticks, expected debug text, image)."""
    rnd = random.Random(seed)
    img = FakeImage()
    end = T0 + int(minutes * 60000)
    ev = []                                            # (t_us, order, harness line, expected kind/data)
    frames = [(t + T0, L, f) for t, L, f in otrec.codec.synthetic(minutes / 60)]
    for t, L, f in frames:
        ev.append((t * 1000, 0, f"O {t} {L} {f:08X}"))
    sat = []
    k, t = 0, T0 + 1000
    while t < end:
        fi = 1 if k % 7 == 6 else 0
        fmt = otrec.SAT_FORMATS[fi]
        args = []
        for conv in otrec.conversions(fmt):
            if conv == "f":
                args.append(rnd.choice([f32(20.0 + rnd.random()), float("nan"), -0.0004, f32(45.5)]))
            elif conv in "diu":
                args.append(rnd.randint(0, 3000))
            elif conv == "c":
                args.append(ord(rnd.choice("VEN")))
            else:
                args.append(rnd.choice(["OT", "BRD", "-"]))
        text = otrec.py_printf(fmt, args)
        sat.append((t, fi, text))
        ev.append((t * 1000, 1, f"S {t} {fi} {text.encode().hex()}"))
        k += 1
        t += 38000
    sites = [s for s in dbg_size_study.call_sites() if "DebugT" in s[0]]
    dbg_lines, t_us, heap = [], T0 * 1000 + 700, None
    while t_us < end * 1000:
        macro, fmt, fn, line = rnd.choice(sites)
        if t_us // 1000000 != (heap[2] if heap else -1):
            heap = (rnd.randint(60000, 75000), rnd.randint(28000, 32000), t_us // 1000000)
        ev.append((t_us, 2, f"H {heap[0]} {heap[1]}"))
        fa, na = img.add(fmt), img.add(fn)
        pre = prefix(t_us, heap, fn, line)
        if "ln" in macro.lower():
            ev.append((t_us, 3, f"L {t_us} {fa} {na} {line}"))
            dbg_lines.append((t_us, pre + fmt + "\r\n"))
        else:
            args = []
            for m in otrec.SPEC.finditer(fmt):
                c = m.group(5)
                if c == "%":
                    continue
                v = dbg_size_study.gen_arg(rnd, c, m.group(3))
                args.append(f32(v) if c == "f" else (ord(v) if c == "c" else v))
            body = otrec.py_printf(fmt, args)
            ev.append((t_us, 3, f"P {t_us} {fa} {na} {line} {fmt.encode().hex()} {body.encode().hex()} "
                                + " ".join(typed_args(fmt, args))))
            dbg_lines.append((t_us, pre + body))
        t_us += int(rnd.expovariate(dbg_rate / 100.0) * 1e6) + 1
    ev.sort(key=lambda e: (e[0], e[1]))
    lines = [f"B {T0}"] + [e[2] for e in ev] + ["E"]
    return lines, frames, sat, "".join(t for _, t in dbg_lines).encode(), img


def decode(recs, img):
    ot, sat, dbg = [], [], b""
    for t0, payload in recs:
        sec = otrec.unpack_mix(t0, payload)
        ot += sec.get(otrec.SEC_OT, [])
        sat += otrec.sat_decode(t0, sec.get(otrec.SEC_SAT, b""))
        dbg += otrec.dbg_decode(t0, sec.get(otrec.SEC_DBG, b""), img, UTC)
    return ot, sat, dbg


@unittest.skipIf(Harness.build is None, "no build")
class RemoteTelemetryCodecTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        Harness.exe = Harness.build()
        if not Harness.exe:
            raise unittest.SkipTest("no C++ compiler")

    def test_one_record_is_byte_identical_to_otrec(self):
        lines, frames, sat, dbg_text, img = build_events(1, 50)
        frames = [f for f in frames if f[0] < T0 + 30000]
        sat = sat[:1]
        keep = [l for l in lines if not l.startswith(("O ", "S ", "H ", "P ", "L ", "X "))]
        ot_lines = [f"O {t} {L} {f:08X}" for t, L, f in frames]
        sat_lines = [f"S {t} {fi} {text.encode().hex()}" for t, fi, text in sat]
        recs = Harness.run([keep[0]] + ot_lines + sat_lines + ["E"])
        self.assertEqual(len(recs), 1)
        t0, payload = recs[0]
        _, ot_data = otrec.pack_ot2(frames)
        want = bytearray()
        sec = otrec._leb(frames[0][0] - t0) + ot_data
        want += bytes([otrec.SEC_OT_V2]) + otrec._leb(len(sec)) + sec
        s = otrec.sat_encode(t0, sat)
        want += bytes([otrec.SEC_SAT]) + otrec._leb(len(s)) + s
        self.assertEqual(payload.hex(), bytes(want).hex())

    def test_long_run_decodes_back_exactly(self):
        lines, frames, sat, dbg_text, img = build_events(30, 300)
        recs = Harness.run(lines)
        self.assertGreater(len(recs), 30)              # 30 minutes, and some records closed early
        self.assertLessEqual(max(len(p) for _, p in recs), 4096 + 32)
        ot, sat_out, dbg = decode(recs, img)
        self.assertEqual(ot, frames)
        self.assertEqual([(t, fi, x) for t, fi, x in sat_out], sat)
        self.assertEqual(dbg, dbg_text)

    def test_untokenizable_lines_fall_back_to_line_raw(self):
        img = FakeImage()
        fn, line, heap = "probe", 42, (70000, 31000)
        cases = [("long=%s\r\n", ["x" * 200]), ("dbl=%.3f\r\n", [0.1]), ("ptr=%p\r\n", [0])]
        lines, want, t_us = [f"B {T0}", f"H {heap[0]} {heap[1]}"], b"", T0 * 1000 + 5
        for fmt, args in cases:
            fa, na = img.add(fmt), img.add(fn)
            body = ("ptr=0\r\n" if "%p" in fmt else otrec.py_printf(fmt, args))
            typed = [f"s:{args[0].encode().hex()}"] if "%s" in fmt else ([f"f:{args[0]!r}"] if "%.3f" in fmt else ["u:0"])
            lines.append(f"P {t_us} {fa} {na} {line} {fmt.encode().hex()} {body.encode().hex()} " + " ".join(typed))
            want += (prefix(t_us, heap, fn, line) + body).encode()
            t_us += 1000
        recs = Harness.run(lines + ["E"])
        _, _, dbg = decode(recs, img)
        self.assertEqual(dbg, want)
        items = otrec.unpack_mix(recs[0][0], recs[0][1])[otrec.SEC_DBG]
        self.assertEqual(sum(1 for b in [items[0]] if b == otrec.DBG_HEAP), 1)


    def test_lost_items_are_reported_in_the_next_record(self):
        lines, frames, _, _, _ = build_events(2, 1)
        ot = [f"O {t} {L} {f:08X}" for t, L, f in frames]
        mid = len(ot) // 2
        recs = Harness.run([lines[0]] + ot[:mid] + ["G 3 1"] + ot[mid:] + ["E"])
        counts = [otrec.lost_counts(otrec.unpack_mix(t0, p)) for t0, p in recs]
        self.assertEqual(sum(c[0] for c in counts), 3)
        self.assertEqual(sum(c[1] for c in counts), 1)
        self.assertEqual(sum(1 for c in counts if c != (0, 0)), 1)       # reported once, then reset
        got = []
        for t0, p in recs:
            got += otrec.unpack_mix(t0, p).get(otrec.SEC_OT, [])
        self.assertEqual(got, [f for f in frames])                       # and the OT data is untouched


    def test_events_round_trip(self):
        """TASK-1473: every event type through the real recorder; records close when the section is full."""
        rnd = random.Random(11)
        samples = {11: ["CS=45.0"], 12: ["TT=20.5"], 13: ["CS: 45.00"], 14: ["boiler_temp", "45.50"],
                   16: ["set/otgw/command", "TT=21"], 17: [3, 1, '{"value":"*****"}'], 18: [f32(7.25), f32(81.5), f32(3.5)],
                   19: [2, f32(20.125), f32(55.0), 87, -71], 20: ["MQTTbroker", "10.0.0.2"],
                   21: [42, "Software reset", ""], 23: [1763200000], 30: [1, "28ff641e8016043c", 2731],
                   31: [3, 1200, 850], 32: [0, -63, 0x0A00000C], 33: [1, 0], 34: [5, 1, 400],
                   35: [[(2, 30), (7, 6)]], 36: [ord("5")], 37: [1, 200], 38: [0, 0, "gateway.hex"],
                   39: [61234, 30212, 11588, -63, 86400], 40: [2, 3], 41: [1, "CS=60"]}
        lines, events, t = [f"B {T0}"], [], T0
        for k in range(900):
            typ = rnd.choice(sorted(samples))
            t += rnd.randint(0, 900)
            item = otrec.evt_encode_item(t, t, typ, samples[typ])          # dt 0: only the payload is used
            payload = item[2:]
            lines.append(f"V {t} {typ} {payload.hex()}")
            events.append((t, typ))
        recs = Harness.run(lines + ["E"])
        got = []
        for t0, p in recs:
            sec = otrec.unpack_mix(t0, p)
            got += [(tt, ty, f) for tt, ty, f in otrec.evt_decode(t0, sec.get(otrec.SEC_EVT, b""))]
        self.assertGreater(len(recs), 3)                                   # the 1,408 B section filled up
        self.assertEqual([(tt, ty) for tt, ty, _ in got], events)          # every event, in order, exact ms
        names = dict(zip(otrec.EV_FIELDS[30], samples[30]))
        self.assertTrue(any(f == names for _, ty, f in got if ty == 30))
        ble = [f for _, ty, f in got if ty == 19][0]
        self.assertEqual((ble["temperature"], ble["rssi"]), (20.125, -71))


if __name__ == "__main__":
    unittest.main(verbosity=2)
